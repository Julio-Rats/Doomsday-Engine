/** @file packageiconbank.cpp  Bank for package icons.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/PackageIconBank"
#include <de/FileSystem>
#include <de/ImageFile>

namespace de {

DENG2_PIMPL_NOREF(PackageIconBank)
{
    class PackageImageSource : public TextureBank::ImageSource
    {
    public:
        PackageImageSource(Path const &packagePath,
                           Size const &displaySize)
            : ImageSource(packagePath)
            , _displaySize(displaySize)
        {
            DENG2_ASSERT(packagePath.lastSegment().toStringRef().endsWith(".pack"));
        }

        Image load() const override
        {
            const String iconPath = sourcePath() / QStringLiteral("icon");

            Image img;
            if (ImageFile const *file = FS::tryLocate<ImageFile const>(iconPath + ".jpg"))
            {
                img = file->image();
            }
            else if (ImageFile const *file = FS::tryLocate<ImageFile const>(iconPath + ".png"))
            {
                img = file->image();
            }
            if (!img.isNull())
            {
                // Cut to square aspect first.
                if (img.height() > img.width())
                {
                    duint const off = img.height() - img.width();
                    img = img.subImage(Rectanglei(0, off/2, img.width(), img.width()));
                }
                else if (img.width() > img.height())
                {
                    duint const off = img.width() - img.height();
                    img = img.subImage(Rectanglei(off/2, 0, img.height(), img.height()));
                }

                if (img.width() > _displaySize.x)
                {
                    // Resize to display size.
                    img.resize(_displaySize);
                }
            }
            img.setPointRatio(1.f);
            return img;
        }

    private:
        Size _displaySize;
    };

    Size displaySize;

    static bool doesPackageHaveIcon(const Path &packagePath)
    {
        return FS::exists(packagePath / "icon.jpg") ||
               FS::exists(packagePath / "icon.png");
    }
};

PackageIconBank::PackageIconBank()
    : TextureBank("PackageIconBank", BackgroundThread | DisableHotStorage)
    , d(new Impl)
{
    setSeparator('/'); // expect package paths as keys
}

void PackageIconBank::setDisplaySize(Size const &displaySize)
{
    d->displaySize = displaySize;
}

Id PackageIconBank::packageIcon(File const &packageFile)
{
    const Path packagePath = packageFile.path();
    if (!has(packagePath))
    {
        add(packagePath, new Impl::PackageImageSource(packagePath, d->displaySize));
    }
    if (isLoaded(packagePath))
    {
        // Already have it.
        return TextureBank::texture(packagePath);
    }
    LOG_AS("PackageIconBank");
    LOG_RES_VERBOSE("Loading icon from \"%s\"") << packagePath.toString();
    // Every new request goes to the front of the queue.
    load(packagePath, BeforeQueued);
    return Id::None;
}

bool PackageIconBank::packageContainsIcon(const de::File &packageFile) const
{
    return Impl::doesPackageHaveIcon(packageFile.path());
}

} // namespace de
