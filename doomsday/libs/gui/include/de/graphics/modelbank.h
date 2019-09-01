/** @file modelbank.h  Bank containing 3D models.
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

#ifndef LIBGUI_MODELBANK_H
#define LIBGUI_MODELBANK_H

#include <de/Bank>
#include <de/ModelDrawable>

#include <functional>

namespace de {

/**
 * Bank of ModelDrawable instances. @ingroup gl
 *
 * Loads model files using background tasks, as model files may contain large
 * amounts of geometry and preprocessing operations may be involved.
 *
 * @todo Consider refactoring so that the Data items are derived from
 * ModelDrawable.
 */
class LIBGUI_PUBLIC ModelBank : public Bank
{
public:
    /**
     * Interface for auxiliary data for a loaded model. @ingroup gl
     */
    class LIBGUI_PUBLIC IUserData
    {
    public:
        virtual ~IUserData() {}
        DENG2_CAST_METHODS()
    };

    typedef std::function<ModelDrawable * ()> Constructor;
    typedef std::pair<ModelDrawable *, IUserData *> ModelWithData;

public:
    ModelBank(Constructor modelConstructor = nullptr);

    void add(DotPath const &id, String const &sourcePath);

    ModelDrawable &model(DotPath const &id);

    template <typename Type>
    Type &model(DotPath const &id)
    {
        return static_cast<Type &>(model(id));
    }

    /**
     * Sets the user data of a loaded model.
     * @param id        Model identifier.
     * @param userData  User data object. Ownership taken.
     */
    void setUserData(DotPath const &id, IUserData *userData);

    IUserData const *userData(DotPath const &id) const;

    ModelWithData modelAndData(DotPath const &id) const;

    template <typename Type>
    std::pair<ModelDrawable *, Type *> modelAndData(DotPath const &id) const
    {
        ModelWithData entry = modelAndData(id);
        return std::pair<ModelDrawable *, Type *>
                (entry.first, static_cast<Type *>(entry.second));
    }

protected:
    IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_MODELBANK_H
