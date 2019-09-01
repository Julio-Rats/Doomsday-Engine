/** @file render/shadervar.h
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

#ifndef DE_CLIENT_RENDER_SHADERVAR_H
#define DE_CLIENT_RENDER_SHADERVAR_H

#include <de/AnimationValue>
#include <de/GLUniform>
#include <de/Range>
#include <QList>

/**
 * Animatable variable bound to a GL uniform. The value can have 1...4 float
 * components.
 */
struct ShaderVar
{
    struct Value {
        de::AnimationValue *anim; // not owned
        de::Rangef wrap;

        Value(de::AnimationValue *a = nullptr) : anim(a) {} // not owned
    };
    QList<Value> values;
    de::GLUniform *uniform = nullptr; // owned

public:
    virtual ~ShaderVar();

    void init(float value);

    template <typename VecType>
    void init(VecType const &vec)
    {
        values.clear();
        for (int i = 0; i < vec.size(); ++i)
        {
            values.append(new de::AnimationValue(de::Animation(vec[i], de::Animation::Linear)));
        }
    }

    float currentValue(int index) const;

    /**
     * Copies the current values to the uniform.
     */
    void updateUniform();

    /**
     * Sets the pointers to the AnimationValue objects by looking them up from a Record.
     */
    void updateValuePointers(de::Record &names, de::String const &varName);
};

struct ShaderVars
{
    QHash<de::String, ShaderVar *> members;

    DE_ERROR(DefinitionError);

public:
    virtual ~ShaderVars();

    void initVariableFromDefinition(de::String const &variableName,
                                    de::Record const &valueDef,
                                    de::Record &bindingNames);

    void addBinding(de::Record &names, de::String const &varName, de::AnimationValue *anim);
};

#endif // DE_CLIENT_RENDER_SHADERVAR_H
