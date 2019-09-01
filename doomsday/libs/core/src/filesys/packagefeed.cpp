/** @file packagefeed.cpp  Links to loaded packages.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/PackageFeed"
#include "de/LinkFile"
#include "de/ArchiveFolder"
#include "de/PackageLoader"
#include "de/FS"

namespace de {

static String const VAR_LINK_PACKAGE_ID("link.package");

DENG2_PIMPL(PackageFeed)
{
    PackageLoader &loader;
    LinkMode linkMode;
    Filter filter;

    Impl(Public *i, PackageLoader &ldr, LinkMode lm)
        : Base(i), loader(ldr), linkMode(lm)
    {}

    File *linkToPackage(Package &pkg, String const &linkName, Folder const &folder)
    {
        /// @todo Resolve conflicts: replace, ignore, or fail. -jk

        if (folder.has(linkName)) return nullptr; // Already there, keep the existing link.

        // Packages can be optionally filtered from the feed.
        if (filter && !filter(pkg)) return nullptr;

        // Create a link to the loaded package's file.
        String name;
        if (linkMode == LinkIdentifier)
        {
            name = linkName;
        }
        else
        {
            name = Package::versionedIdentifierForFile(pkg.file());
        }
        LinkFile *link = LinkFile::newLinkToFile(pkg.file(), name);

        // We will decide on pruning this.
        link->setOriginFeed(thisPublic);

        // Identifier also in metadata.
        link->objectNamespace().addText(VAR_LINK_PACKAGE_ID, pkg.identifier());

        return link;
    }

    PopulatedFiles populate(Folder const &folder)
    {
        PopulatedFiles populated;
        DENG2_FOR_EACH_CONST(PackageLoader::LoadedPackages, i, loader.loadedPackages())
        {
            Package *pkg = i.value();
            populated << linkToPackage(*pkg, i.key(), folder);

            // Also link it under its possible alias identifier (for variants).
            if (pkg->objectNamespace().has(Package::VAR_PACKAGE_ALIAS))
            {
                populated << linkToPackage(*pkg, pkg->objectNamespace()
                                           .gets(Package::VAR_PACKAGE_ALIAS), folder);
            }

            // Link each contained asset, too.
            foreach (String ident, pkg->assets())
            {
                populated << linkToPackage(*pkg, "asset." + ident, folder);
            }
        }
        return populated;
    }
};

PackageFeed::PackageFeed(PackageLoader &loader, LinkMode linkMode)
    : d(new Impl(this, loader, linkMode))
{}

void PackageFeed::setFilter(Filter filter)
{
    d->filter = filter;
}

PackageLoader &PackageFeed::loader()
{
    return d->loader;
}

String PackageFeed::description() const
{
    return "loaded packages";
}

Feed::PopulatedFiles PackageFeed::populate(Folder const &folder)
{
    return d->populate(folder);
}

bool PackageFeed::prune(File &file) const
{
    if (LinkFile const *link = maybeAs<LinkFile>(file))
    {
        // Links to unloaded packages should be pruned.
        if (!d->loader.isLoaded(link->objectNamespace().gets(VAR_LINK_PACKAGE_ID)))
            return true;

        //if (Folder const *pkg = maybeAs<Folder>(link->target()))
        {
            // Links to unloaded packages should be pruned.
            //if (!d->loader.isLoaded(*pkg)) return true;

//            qDebug() << "Link:" << link->description() << link->status().modifiedAt.asText();
//            qDebug() << " tgt:" << link->target().description() << link->target().status().modifiedAt.asText();

            // Package has been modified, should be pruned.
            if (link->status() != link->target().status()) return true;
        }
    }
    return false; // Don't prune.
}

} // namespace de
