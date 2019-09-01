/** @file entityrender.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "entityrender.h"
#include "gloom/world/map.h"
#include "gloom/render/defs.h"
#include "gloom/render/icamera.h"
#include "gloom/render/lightrender.h"
#include "gloom/render/light.h"
#include "src/gloomapp.h"

#include <de/PackageLoader>
#include <de/ModelDrawable>
#include <de/GLProgram>

using namespace de;

namespace gloom {

struct InstanceData {
    Mat4f matrix;
    Vec4f color;
    LIBGUI_DECLARE_VERTEX_FORMAT(2)
};
internal::AttribSpec const InstanceData::_spec[2] = {
    { internal::AttribSpec::InstanceMatrix, 16, GL_FLOAT, false, sizeof(InstanceData), 0 },
    { internal::AttribSpec::InstanceColor,  4,  GL_FLOAT, false, sizeof(InstanceData), 16 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(InstanceData, 20 * sizeof(float))

DENG2_PIMPL(EntityRender)
{
    EntityMap     ents;
    ModelDrawable entityModels[3];
    GLProgram     program;
    GLProgram     dirShadowProgram;
    GLProgram     omniShadowProgram;

    Impl(Public *i)
        : Base(i)
    {}

    void init()
    {
        loadModels();
        for (auto &model : entityModels)
        {
            model.glInit();
        }
    }

    void deinit()
    {
        for (auto &model : entityModels)
        {
            model.glDeinit();
        }
    }

    void loadModels()
    {
        auto &context = self().context();
        auto const &pkg = PackageLoader::get().package("net.dengine.gloom");

        const char *filenames[] = {
            "models/tree1/t2.3ds",
            "models/tree2/t3.3ds",
            "models/tree3/t4.3ds",
        };

        int idx = 0;
        for (auto &model : entityModels)
        {
            model.load(pkg.root().locate<File>(filenames[idx]));
            model.setAtlas(ModelDrawable::Diffuse,  *context.atlas[Diffuse]);
            model.setAtlas(ModelDrawable::Emissive, *context.atlas[Emissive]);
            model.setAtlas(ModelDrawable::Normals,  *context.atlas[NormalDisplacement]);
            model.setAtlas(ModelDrawable::Specular, *context.atlas[SpecularGloss]);
            model.setProgram(&program);
            idx++;
        }

        GloomApp::shaders().build(program, "gloom.entity.material");
        context.bindCamera(program)
               .bindMaterials(program);

        GloomApp::shaders().build(dirShadowProgram, "gloom.entity.shadow.dir")
            << context.uLightMatrix
            << context.uDiffuseAtlas;

        GloomApp::shaders().build(omniShadowProgram, "gloom.entity.shadow.omni")
            << context.uLightOrigin
            << context.uLightFarPlane
            << context.uLightCubeMatrices
            << context.uDiffuseAtlas;
    }

    void create()
    {
        DENG2_ASSERT(self().context().map);

        const auto &map = *self().context().map;

        ents.clear();
        ents.setBounds(map.bounds());

        // Create entities for all objects defined in the map.
        for (auto i = map.entities().begin(), end = map.entities().end(); i != end; ++i)
        {
            ents.insert(*i.value());
        }
    }

    void setProgram(GLProgram &prog)
    {
        for (auto &model : entityModels)
        {
            model.setProgram(&prog);
        }
    }

    typedef GLBufferT<InstanceData> InstanceBuf;

    void render()
    {
        const ICamera &camera = *self().context().view.camera;

        float fullDist = 500;
        const auto entities = ents.listRegionBackToFront(camera.cameraPosition(), fullDist);

        InstanceBuf ibuf; // TODO: it's enough to create this once per frame

        // Draw all model types.
        int entType = Entity::Tree1;
        for (const auto &model : entityModels)
        {
            InstanceBuf::Builder data;

            // Set up the instance buffer.
            foreach (const Entity *e, entities)
            {
                if (e->type() != entType) continue;

                float dims = model.dimensions().z * e->scale().y;

                float maxDist = min(fullDist, dims * 10);
                float fadeItv = .333f * maxDist;
                float distance = float((e->position() - camera.cameraPosition()).length());

                if (distance < maxDist)
                {
                    InstanceBuf::Type inst{
                        Mat4f::translate(e->position()) *
                            Mat4f::rotate(e->angle(), Vec3f(0, -1, 0)) *
                            Mat4f::rotate(-90, Vec3f(1, 0, 0)) *
                            Mat4f::scale(e->scale() * 0.1f),
                        Vec4f(1, 1, 1,
                                 clamp(0.f, 1.f - (distance - maxDist + fadeItv) / fadeItv, 1.f))};
                    data << inst;
                }
            }

            if (!data.isEmpty())
            {
                ibuf.setVertices(data, gl::Stream);
                model.drawInstanced(ibuf);
            }

            entType++;
        }
    }
};

EntityRender::EntityRender()
    : d(new Impl(this))
{}

void EntityRender::glInit(Context &context)
{
    Render::glInit(context);
    d->init();
}

void EntityRender::glDeinit()
{
    d->deinit();
    Render::glDeinit();
}

void EntityRender::createEntities()
{
    d->create();
}

EntityMap &EntityRender::entityMap()
{
    return d->ents;
}

void EntityRender::render()
{
    d->render();
}

void EntityRender::renderShadows(const Light &light)
{
    GLState::push() = context().lights->shadowState();
    d->setProgram(light.type() == Light::Directional? d->dirShadowProgram
                                                    : d->omniShadowProgram);
    d->render();
    d->setProgram(d->program);
    GLState::pop();
}

} // namespace gloom
