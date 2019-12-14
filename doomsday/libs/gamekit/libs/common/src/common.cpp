/** @file common.c  Top-level libcommon routines.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "g_common.h"
#include "g_defs.h"
#include "g_update.h"
#include "p_map.h"
#include "polyobjs.h"
#include "r_common.h"

#include <de/Binder>
#include <de/Hash>
#include <de/Context>
#include <de/NoneValue>
#include <de/ScriptSystem>
#include <doomsday/DoomsdayApp>
#include <doomsday/defs/definition.h>
#include <doomsday/players.h>
#include <doomsday/world/map.h>
#include <doomsday/world/entitydef.h>

int Common_GetInteger(int id)
{
    switch(id)
    {
    case DD_MOBJ_SIZE:
        return sizeof(mobj_t); // game plugin specific

    case DD_POLYOBJ_SIZE:
        return sizeof(Polyobj);

    case DD_GAME_RECOMMENDS_SAVING:
        // The engine will use this as a hint whether to remind the user to
        // manually save the game before, e.g., upgrading to a new version.
        return G_GameState() == GS_MAP;

    default:
        break;
    }
    return 0;
}

void *Common_GetGameAPI(const char *name)
{
    #define HASH_ENTRY(Name, Func) std::make_pair(Name, de::function_cast<void *>(Func))
    static de::Hash<de::String, void *> const funcs(
    {
        HASH_ENTRY("DrawViewPort",          G_DrawViewPort),
        HASH_ENTRY("FinaleResponder",       FI_PrivilegedResponder),
        HASH_ENTRY("FinalizeMapChange",     P_FinalizeMapChange),
        HASH_ENTRY("HandleMapDataPropertyValue", P_HandleMapDataPropertyValue),
        HASH_ENTRY("HandleMapObjectStatusReport", P_HandleMapObjectStatusReport),
        HASH_ENTRY("HandlePacket",          D_HandlePacket),
        HASH_ENTRY("MobjCheckPositionXYZ",  P_CheckPositionXYZ),
        HASH_ENTRY("MobjFriction",          Mobj_Friction),
        HASH_ENTRY("MobjRestoreState",      Mobj_RestoreObjectState),
        HASH_ENTRY("MobjStateAsInfo",       Mobj_StateAsInfo),
        HASH_ENTRY("MobjThinker",           P_MobjThinker),
        HASH_ENTRY("MobjTryMoveXYZ",        P_TryMoveXYZ),
        HASH_ENTRY("NetConnect",            D_NetConnect),
        HASH_ENTRY("NetDisconnect",         D_NetDisconnect),
        HASH_ENTRY("NetPlayerEvent",        D_NetPlayerEvent),
        HASH_ENTRY("NetServerStart",        D_NetServerStarted),
        HASH_ENTRY("NetServerStop",         D_NetServerClose),
        HASH_ENTRY("NetWorldEvent",         D_NetWorldEvent),
        HASH_ENTRY("PrivilegedResponder",   G_PrivilegedResponder),
        HASH_ENTRY("Responder",             G_Responder),
        HASH_ENTRY("SectorHeightChangeNotification", P_HandleSectorHeightChange),
        HASH_ENTRY("Ticker",                G_Ticker),
        HASH_ENTRY("UpdateState",           G_UpdateState),
    });
    #undef HASH_ENTRY

    auto found = funcs.find(name);
    if (found != funcs.end()) return found->second;
    return nullptr;
}

GameRules &gfw_DefaultGameRules()
{
    static GameRules defaultGameRules;
    return defaultGameRules;
}

void GameRules_UpdateDefaultsFromCVars()
{
#if !__JHEXEN__
    gfw_SetDefaultRule(fast, cfg.common.defaultRuleFastMonsters);
#endif
}

#ifdef __JDOOM__
void fastMonstersChanged()
{
    GameRules_UpdateDefaultsFromCVars();
}
#endif

void Common_Register()
{
    C_VAR_BYTE ("hud-title-author-nounknown",   &cfg.common.hideIWADAuthor,     0, 0, 1);

    // Movement
    C_VAR_FLOAT("player-move-speed",    &cfg.common.playerMoveSpeed,    0, 0, 1);
    C_VAR_INT  ("player-jump",          &cfg.common.jumpEnabled,        0, 0, 1);
    C_VAR_FLOAT("player-jump-power",    &cfg.common.jumpPower,          0, 0, 100);
    C_VAR_BYTE ("player-air-movement",  &cfg.common.airborneMovement,   0, 0, 32);

    // Gameplay
    C_VAR_BYTE ("sound-switch-origin",  &cfg.common.switchSoundOrigin,  0, 0, 1);
#ifdef __JDOOM__
    C_VAR_BYTE2("game-monsters-fast",   &cfg.common.defaultRuleFastMonsters, 0, 0, 1, fastMonstersChanged);
#endif
    C_VAR_BYTE ("game-objects-pushable-limit", &cfg.common.pushableMomentumLimitedToPusher, 0, 0, 1);
}

//-------------------------------------------------------------------------------------------------

static de::Binder *gameBindings;
static de::Record *gameModule;

static mobj_t &instanceMobj(const de::Context &ctx)
{
    using namespace de;

    const int id = ctx.selfInstance().geti(DE_STR("__id__"), 0);
    if (mobj_t *mo = Mobj_ById(id))
    {
        return *mo;
    }
    throw world::BaseMap::MissingObjectError("instanceMobj",
                                             String::format("Mobj %d does not exist", id));
}

static de::Value *Function_Thing_SpawnMissile(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    using namespace de;

    mobj_t *src = &instanceMobj(ctx);

    const mobjtype_t missileId = mobjtype_t(Defs().getMobjNum(args.at(0)->asText()));

    if (is<NoneValue>(args.at(1))) // Fire at target mobj.
    {
        if (src->target)
        {
#if defined(__JHERETIC__)
            if (mobj_t *mis = P_SpawnMissile(missileId, src, src->target, true))
            {
                if (missileId == MT_MUMMYFX1)
                {
                    // Tracer is used to keep track of where the missile is homing.
                    mis->tracer = src->target;
                }
                else if (missileId == MT_WHIRLWIND)
                {
                    P_InitWhirlwind(mis, src->target);
                }
            }
#else
            P_SpawnMissile(missileId, src, src->target);
#endif
        }
    }
    else
    {
#if defined(__JHERETIC__) || defined(__JHEXEN__)
        const double angle = args.at(1)->asNumber();
        const double momZ  = args.at(2)->asNumber();
        P_SpawnMissileAngle(missileId, src, angle_t(angle * ANGLE_MAX), momZ);
#endif
    }
    return nullptr;
}

#if defined(__JHERETIC__)
static de::Value *Function_Thing_Attack(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    using namespace de;

    mobj_t *src = &instanceMobj(ctx);

    const int meleeDamage = args.at(0)->asInt();
    const mobjtype_t missileId = mobjtype_t(Defs().getMobjNum(args.at(1)->asText()));

    return new NumberValue(P_Attack(src, meleeDamage, missileId));
}
#endif

static int playerNumberArgument(const de::Value &arg)
{
    if (de::is<de::NoneValue>(arg))
    {
        return CONSOLEPLAYER;
    }
    const int plrNum = arg.asInt();
    if (plrNum < 0 || plrNum >= MAXPLAYERS)
    {
        throw de::Error("playerNumberArgument", "Player index out of bounds");
    }
    return plrNum;
}

static de::Value *Function_SetMessage(de::Context &, const de::Function::ArgumentValues &args)
{
    const int plrNum = playerNumberArgument(*args.at(1));
    P_SetMessage(&players[plrNum], args.at(0)->asText());
    return nullptr;
}

#if defined (__JHEXEN__)
static de::Value *Function_SetYellowMessage(de::Context &, const de::Function::ArgumentValues &args)
{
    const int plrNum = playerNumberArgument(*args.at(1));
    P_SetYellowMessage(&players[plrNum], args.at(0)->asText());
    return nullptr;
}
#endif

void Common_Load()
{
    using namespace de;

    gameModule = new Record;

    Function::Defaults spawnMissileArgs;
    spawnMissileArgs["angle"] = new NoneValue;
    spawnMissileArgs["momz"] = new NumberValue(0.0);

    Function::Defaults attackArgs;
    attackArgs["damage"] = new NumberValue(0.0);
    attackArgs["missile"] = new NoneValue;

    Function::Defaults setMessageArgs;
    setMessageArgs["player"] = new NoneValue;

    DE_ASSERT(gameBindings == nullptr);
    gameBindings = new Binder(nullptr, Binder::FunctionsOwned); // must delete when plugin unloaded
    gameBindings->init(ScriptSystem::get().builtInClass("World", "Thing"))
#if defined(__JHERETIC__)
        << DE_FUNC_DEFS(Thing_Attack, "attack", "damage" << "missile", attackArgs)
#endif
        << DE_FUNC_DEFS(Thing_SpawnMissile, "spawnMissile", "id" << "angle" << "momz", spawnMissileArgs);

    gameBindings->init(*gameModule)
        << DE_FUNC_DEFS(SetMessage, "setMessage", "message" << "player", setMessageArgs);

    #if defined(__JHEXEN__)
    {
        Function::Defaults setYellowMessageArgs;
        setYellowMessageArgs["player"] = new NoneValue;
        *gameBindings
            << DE_FUNC_DEFS(SetYellowMessage, "setYellowMessage", "message" << "player", setYellowMessageArgs);
    }
#endif

    ScriptSystem::get().addNativeModule("Game", *gameModule);
}

void Common_Unload()
{
    DE_ASSERT(gameBindings != nullptr);
    de::ScriptSystem::get().removeNativeModule("Game");
    delete gameBindings;
    gameBindings = nullptr;
    delete gameModule;
    gameModule = nullptr;
}

void Common_RegisterMapObjs()
{
    P_RegisterMapObj(MO_THING, "Thing");
    P_RegisterMapObjProperty(MO_THING, MO_X, "X", DDVT_DOUBLE);
    P_RegisterMapObjProperty(MO_THING, MO_Y, "Y", DDVT_DOUBLE);
    P_RegisterMapObjProperty(MO_THING, MO_Z, "Z", DDVT_DOUBLE);
    P_RegisterMapObjProperty(MO_THING, MO_ANGLE, "Angle", DDVT_ANGLE);
    P_RegisterMapObjProperty(MO_THING, MO_DOOMEDNUM, "DoomEdNum", DDVT_INT);
    P_RegisterMapObjProperty(MO_THING, MO_SKILLMODES, "SkillModes", DDVT_INT);
    P_RegisterMapObjProperty(MO_THING, MO_FLAGS, "Flags", DDVT_INT);

    P_RegisterMapObj(MO_XLINEDEF, "XLinedef");
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_TAG, "Tag", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_TYPE, "Type", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_FLAGS, "Flags", DDVT_SHORT);

    P_RegisterMapObj(MO_XSECTOR, "XSector");
    P_RegisterMapObjProperty(MO_XSECTOR, MO_TAG, "Tag", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XSECTOR, MO_TYPE, "Type", DDVT_SHORT);
}
