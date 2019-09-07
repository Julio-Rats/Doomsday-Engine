/** @file gameprofiles.cpp  Game profiles.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/GameProfiles"
#include "doomsday/Games"
#include "doomsday/DoomsdayApp"
#include "doomsday/GameStateFolder"

#include <de/App>
#include <de/Folder>
#include <de/NoneValue>
#include <de/PackageLoader>
#include <de/Record>

#include <sstream>

using namespace de;

DE_STATIC_STRING(VAR_GAME,                  "game");
DE_STATIC_STRING(VAR_PACKAGES,              "packages");
DE_STATIC_STRING(VAR_CUSTOM_DATA_FILE,      "customDataFile");
DE_STATIC_STRING(VAR_USER_CREATED,          "userCreated");
DE_STATIC_STRING(VAR_USE_GAME_REQUIREMENTS, "useGameRequirements");
DE_STATIC_STRING(VAR_AUTO_START_MAP,        "autoStartMap");
DE_STATIC_STRING(VAR_AUTO_START_SKILL,      "autoStartSkill");
static String const VAR_LAST_PLAYED     ("lastPlayed");
static String const VAR_SAVE_LOCATION_ID("saveLocationId");
static String const VAR_VALUES          ("values");

static constexpr int DEFAULT_SKILL = 3; // Normal skill level (1-5)

static GameProfile nullGameProfile;

DE_PIMPL(GameProfiles)
, DE_OBSERVES(Games, Addition)
{
    Impl(Public *i) : Base(i) {}

    void gameAdded(Game &game)
    {
        /*
         * Make sure there is a profile matching this game's title. The session
         * configuration for each game is persistently stored using these profiles.
         * (User-created profiles must use different names.)
         */
        if (!self().tryFind(game.title()))
        {
            auto *prof = new Profile(game.title());
            prof->setGame(game.id());
            self().add(prof);
        }
    }
};

GameProfiles::GameProfiles()
    : d(new Impl(this))
{
    setPersistentName("game");
}

void GameProfiles::setGames(Games &games)
{
    games.audienceForAddition() += d;
}

const GameProfiles::Profile &GameProfiles::null()
{
    return nullGameProfile;
}

const GameProfile &GameProfiles::builtInProfile(const String &gameId) const
{
    return find(DoomsdayApp::games()[gameId].title()).as<GameProfile>();
}

LoopResult GameProfiles::forAll(std::function<LoopResult (Profile &)> func)
{
    return Profiles::forAll([&func] (AbstractProfile &prof) -> LoopResult
    {
        if (auto result = func(prof.as<Profile>()))
        {
            return result;
        }
        return LoopContinue;
    });
}

LoopResult GameProfiles::forAll(std::function<LoopResult (const Profile &)> func) const
{
    return Profiles::forAll([&func] (const AbstractProfile &prof) -> LoopResult
    {
        if (auto result = func(prof.as<Profile>()))
        {
            return result;
        }
        return LoopContinue;
    });
}

List<GameProfile *> GameProfiles::profilesInFamily(const de::String &family)
{
    List<GameProfile *> profs;
    forAll([&profs, &family] (GameProfile &profile)
    {
        if (profile.game().family() == family)
        {
            profs << &profile;
        }
        return LoopContinue;
    });
    return profs;
}

List<GameProfile *> GameProfiles::profilesSortedByFamily()
{
    List<GameProfile *> profs;
    forAll([&profs] (GameProfile &profile)
    {
        profs << &profile;
        return LoopContinue;
    });
    std::sort(profs.begin(), profs.end(), [] (const GameProfile *a, const GameProfile *b)
    {
        String family1 = a->game().family();
        String family2 = b->game().family();
        if (family1.isEmpty()) family1 = "other";
        if (family2.isEmpty()) family2 = "other";
        if (family1 == family2)
        {
            return a->name().compareWithoutCase(b->name()) < 0;
        }
        return family1.compareWithoutCase(family2) < 0;
    });
    return profs;
}

List<const GameProfile *> GameProfiles::allPlayableProfiles() const
{
    List<const GameProfile *> playable;
    forAll([&playable] (const Profile &prof)
    {
        if (prof.isPlayable()) playable << &prof;
        return LoopContinue;
    });
    return playable;
}

Profiles::AbstractProfile *GameProfiles::profileFromInfoBlock(const Info::BlockElement &block)
{
    std::unique_ptr<Profile> prof(new Profile);

    prof->setGame(block.keyValue(VAR_GAME()).text);

    if (const Info::ListElement *pkgs = block.findAs<Info::ListElement>(VAR_PACKAGES()))
    {
        StringList ids;
        for (const auto &val : pkgs->values())
        {
            ids << val.text;
        }
        prof->setPackages(ids);
    }

    prof->setUserCreated(!block.keyValue(VAR_USER_CREATED()).text.compareWithoutCase("True"));
    if (block.contains(VAR_CUSTOM_DATA_FILE()))
    {
        prof->setCustomDataFile(block.keyValue(VAR_CUSTOM_DATA_FILE()).text);
    }
    if (block.contains(VAR_USE_GAME_REQUIREMENTS()))
    {
        prof->setUseGameRequirements(!block.keyValue(VAR_USE_GAME_REQUIREMENTS())
                                     .text.compareWithoutCase("True"));
    }
    if (block.contains(VAR_AUTO_START_MAP()))
    {
        prof->setAutoStartMap(block.keyValue(VAR_AUTO_START_MAP()).text);
    }
    if (block.contains(VAR_AUTO_START_SKILL()))
    {
        prof->setAutoStartSkill(block.keyValue(VAR_AUTO_START_SKILL()).text.toInt());
    }
    if (block.contains(VAR_SAVE_LOCATION_ID))
    {
        prof->setSaveLocationId(block.keyValue(VAR_SAVE_LOCATION_ID).text.toUInt32(nullptr, 16));
    }
    if (block.contains(VAR_LAST_PLAYED))
    {
        prof->setLastPlayedAt(Time::fromText(block.keyValue(VAR_LAST_PLAYED).text));
    }
    if (const auto *values = block.findAs<Info::BlockElement>(VAR_VALUES))
    {
        prof->objectNamespace() = values->asRecord();
    }

    return prof.release();
}

//-------------------------------------------------------------------------------------------------

DE_PIMPL_NOREF(GameProfiles::Profile)
{
    String     gameId;
    String     customDataFile;
    StringList packages;
    bool       userCreated         = false;
    bool       useGameRequirements = true;
    String     autoStartMap;
    int        autoStartSkill = DEFAULT_SKILL;
    Time       lastPlayedAt   = Time::invalidTime();
    duint32    saveLocationId = 0;
    Record     values;

    Impl() {}

    Impl(const Impl &other)
        : gameId             (other.gameId)
        , customDataFile     (other.customDataFile)
        , packages           (other.packages)
        , userCreated        (other.userCreated)
        , useGameRequirements(other.useGameRequirements)
        , autoStartMap       (other.autoStartMap)
        , autoStartSkill     (other.autoStartSkill)
        , lastPlayedAt       (other.lastPlayedAt)
        , saveLocationId     (other.saveLocationId)
        , values             (other.values)
    {}
};

GameProfiles::Profile::Profile(const String &name)
    : d(new Impl)
{
    setName(name);
}

GameProfiles::Profile::Profile(const Profile &other)
    : AbstractProfile(other)
    , d(new Impl(*other.d))
{}

GameProfiles::Profile &GameProfiles::Profile::operator=(const Profile &other)
{
    AbstractProfile::operator=(other);
    d->gameId                = other.d->gameId;
    d->customDataFile        = other.d->customDataFile;
    d->packages              = other.d->packages;
    d->userCreated           = other.d->userCreated;
    d->useGameRequirements   = other.d->useGameRequirements;
    d->autoStartMap          = other.d->autoStartMap;
    d->autoStartSkill        = other.d->autoStartSkill;
    d->lastPlayedAt          = other.d->lastPlayedAt;
    d->saveLocationId        = other.d->saveLocationId;
    d->values                = other.d->values;
    return *this;
}

void GameProfiles::Profile::setGame(const String &id)
{
    if (d->gameId != id)
    {
        d->gameId = id;
        notifyChange();
    }
}

void GameProfiles::Profile::setCustomDataFile(const String &id)
{
    if (d->customDataFile != id)
    {
        d->customDataFile = id;
        notifyChange();
    }
}

void GameProfiles::Profile::setPackages(const StringList& packagesInOrder)
{
    if (d->packages != packagesInOrder)
    {
        d->packages = packagesInOrder;
        notifyChange();
    }
}

void GameProfiles::Profile::setUserCreated(bool userCreated)
{
    if (d->userCreated != userCreated)
    {
        d->userCreated = userCreated;
        notifyChange();
    }
}

void GameProfiles::Profile::setUseGameRequirements(bool useGameRequirements)
{
    if (d->useGameRequirements != useGameRequirements)
    {
        d->useGameRequirements = useGameRequirements;
        notifyChange();
    }
}

void GameProfiles::Profile::setAutoStartMap(const String &map)
{
    if (d->autoStartMap != map)
    {
        d->autoStartMap = map;
        notifyChange();
    }
}

void GameProfiles::Profile::setAutoStartSkill(int level)
{
    if (level < 1 || level > 5) level = DEFAULT_SKILL;

    if (d->autoStartSkill != level)
    {
        d->autoStartSkill = level;
        notifyChange();
    }
}

void GameProfiles::Profile::setLastPlayedAt(const Time &at)
{
    if (d->lastPlayedAt != at)
    {
        d->lastPlayedAt = at;
        notifyChange();
    }
}

void GameProfiles::Profile::setSaveLocationId(const duint32 saveLocationId)
{
    if (d->saveLocationId != saveLocationId)
    {
        d->saveLocationId = saveLocationId;
        notifyChange();
    }
}

void GameProfiles::Profile::setOptionValue(const String &option, const Value &value)
{
    const String key = "option." + option;
    if (!d->values.has(key) || d->values[key].value().compare(value))
    {
        d->values.set(key, value.duplicate());
        notifyChange();
    }
}

bool GameProfiles::Profile::appendPackage(const String &id)
{
    if (!d->packages.contains(id))
    {
        d->packages << id;
        notifyChange();
        return true;
    }
    return false;
}

Game &GameProfiles::Profile::game() const
{
    auto &games = DoomsdayApp::games();
    if (games.contains(d->gameId))
    {
        return games[d->gameId];
    }
    return Games::nullGame();
}

String GameProfiles::Profile::customDataFile() const
{
    return d->customDataFile;
}

String GameProfiles::Profile::gameId() const
{
    return d->gameId;
}

StringList GameProfiles::Profile::packages() const
{
    return d->packages;
}

bool GameProfiles::Profile::isUserCreated() const
{
    return d->userCreated;
}

bool GameProfiles::Profile::isUsingGameRequirements() const
{
    return d->useGameRequirements;
}

String GameProfiles::Profile::autoStartMap() const
{
    return d->autoStartMap;
}

int GameProfiles::Profile::autoStartSkill() const
{
    return d->autoStartSkill;
}

Time GameProfiles::Profile::lastPlayedAt() const
{
    return d->lastPlayedAt;
}

duint32 GameProfiles::Profile::saveLocationId() const
{
    return d->saveLocationId;
}

static const String PATH_SAVEGAMES = "/home/savegames";

String GameProfiles::Profile::savePath() const
{
    /// If the profile has a custom save location, use that instead.
    if (d->saveLocationId)
    {
        return PATH_SAVEGAMES / String::format("profile-%08x", d->saveLocationId);
    }
    return PATH_SAVEGAMES / gameId();
}

const Value &GameProfiles::Profile::optionValue(const String &option) const
{
    if (const auto *var = d->values.tryFind("option." + option))
    {
        return var->value();
    }
    return game()[Game::DEF_OPTIONS.concatenateMember(option + ".default")].value();
}

bool GameProfiles::Profile::isSaveLocationEmpty() const
{
    FS::waitForIdle();
    if (const auto *loc = FS::tryLocate<const Folder>(savePath()))
    {
        return loc->contents().size() == 0;
    }
    return true;
}

void GameProfiles::Profile::createSaveLocation()
{
    FS::waitForIdle();
    do
    {
        d->saveLocationId = randui32();
    } while (FS::exists(savePath()));
    Folder &loc = FS::get().makeFolder(savePath());
    LOG_MSG("Created save location %s") << loc.description();
}

void GameProfiles::Profile::destroySaveLocation()
{
    if (d->saveLocationId)
    {
        FS::waitForIdle();
        if (auto *loc = FS::tryLocate<Folder>(savePath()))
        {
            LOG_NOTE("Destroying save location %s") << loc->description();
            loc->destroyAllFiles();
            loc->correspondingNativePath().destroy();
            loc->parent()->populate();
        }
        d->saveLocationId = 0;
    }
}

void GameProfiles::Profile::checkSaveLocation() const
{
    if (d->saveLocationId && !FS::exists(savePath()))
    {
        Folder &loc = FS::get().makeFolder(savePath());
        LOG_MSG("Created missing save location %s") << loc.description();
    }
}

StringList GameProfiles::Profile::allRequiredPackages() const
{
    StringList list;
    if (d->customDataFile)
    {
        list += d->customDataFile;
    }
    if (d->useGameRequirements)
    {
        StringList reqs = DoomsdayApp::games()[d->gameId].requiredPackages();
        if (d->customDataFile)
        {
            // Remove any normally required gamedata-tagged packages.
            reqs = filter(reqs, [](const String &id) {
                if (const auto *f = PackageLoader::get().select(id))
                {
                    if (Package::matchTags(*f, DE_STR("\\bgamedata\\b")))
                    {
                        return false;
                    }
                }
                return true;
            });
        }
        list += reqs;
    }
    return list + d->packages;
}

StringList GameProfiles::Profile::packagesAffectingGameplay() const
{
    StringList ids = PackageLoader::get().expandDependencies(allRequiredPackages());
    for (auto iter = ids.begin(); iter != ids.end(); )
    {
        if (!GameStateFolder::isPackageAffectingGameplay(*iter))
    {
            iter = ids.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    return ids;
}

StringList GameProfiles::Profile::unavailablePackages() const
{
    return de::filter(allRequiredPackages(), [] (const String &pkgId) {
        return !PackageLoader::get().isAvailable(pkgId);
    });
}

bool GameProfiles::Profile::isCompatibleWithPackages(const StringList &ids) const
{
    return GameProfiles::arePackageListsCompatible(packagesAffectingGameplay(), ids);
}

bool GameProfiles::Profile::isPlayable() const
{
    for (const String &pkg : allRequiredPackages())
    {
        if (!App::packageLoader().isAvailable(pkg))
            return false;
    }
    return true;
}

void GameProfiles::Profile::loadPackages() const
{
    for (const String &id : allRequiredPackages())
    {
        PackageLoader::get().load(id);
    }
}

void GameProfiles::Profile::unloadPackages() const
{
    StringList const allPackages = allRequiredPackages();
    for (int i = allPackages.sizei() - 1; i >= 0; --i)
    {
        PackageLoader::get().unload(allPackages.at(i));
    }
}

bool GameProfiles::Profile::resetToDefaults()
{
    if (isReadOnly()) return false;

    d->packages.clear();
    return true;
}

String GameProfiles::Profile::toInfoSource() const
{
    std::ostringstream os;

    os << VAR_GAME()                  << ": " << d->gameId << "\n"
       << VAR_PACKAGES()              << " <" << String::join(de::map(d->packages, Info::quoteString), ", ") << ">\n"
       << VAR_USER_CREATED()          << ": " << (d->userCreated? "True" : "False") << "\n"
       << VAR_CUSTOM_DATA_FILE()      << ": " << d->customDataFile << "\n"
       << VAR_USE_GAME_REQUIREMENTS() << ": " << (d->useGameRequirements? "True" : "False");
    if (d->autoStartMap)
    {
        os << "\n" << VAR_AUTO_START_MAP() << ": " << d->autoStartMap;
    }
    os << "\n" << VAR_AUTO_START_SKILL() << ": " << d->autoStartSkill;
    if (d->lastPlayedAt.isValid())
    {
        os << "\n" << VAR_LAST_PLAYED << ": " << d->lastPlayedAt.asText();
    }
    if (d->saveLocationId)
    {
        os << "\n" << VAR_SAVE_LOCATION_ID << ": " << String::format("%08x", d->saveLocationId);
    }
    // Additional configuration values (e.g., config for the game to use).
    if (!d->values.isEmpty())
    {
        String indented = d->values.asInfo();
        indented.replace("\n", "\n    ");
        os << "\n" << VAR_VALUES << " {\n    " << indented << "\n}";
    }
    return os.str();
}

Record &GameProfiles::Profile::objectNamespace()
{
    return d->values;
}

const Record &GameProfiles::Profile::objectNamespace() const
{
    return d->values;
}

bool GameProfiles::arePackageListsCompatible(const StringList &list1, const StringList &list2) // static
{
    if (list1.size() != list2.size()) return false;

    // The package lists must match order and IDs, but currently we ignore the
    // versions.
    for (int i = 0; i < list1.sizei(); ++i)
    {
        if (!Package::equals(list1.at(i), list2.at(i)))
        {
            return false;
        }
    }
    return true;
}
