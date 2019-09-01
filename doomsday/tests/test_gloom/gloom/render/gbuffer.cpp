/** @file gbuffer.cpp
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

#include "gloom/render/gbuffer.h"
#include "gloom/render/screenquad.h"

#include <de/Drawable>
#include <de/GLTextureFramebuffer>
#include <de/GLProgram>
#include <de/GLState>
#include <de/GLUniform>

using namespace de;

namespace gloom {

DENG2_PIMPL(GBuffer)
{
    ScreenQuad quad;
    GLTextureFramebuffer frame{
        QList<Image::Format>({Image::RGBA_16f /* albedo */, Image::RGBA_8888 /* normals */})};
    GLUniform uMvpMatrix        {"uMvpMatrix",         GLUniform::Mat4};
    GLUniform uGBufferAlbedo    {"uGBufferAlbedo",     GLUniform::Sampler2D};
    GLUniform uGBufferNormal    {"uGBufferNormal",     GLUniform::Sampler2D};
    GLUniform uGBufferDepth     {"uGBufferDepth",      GLUniform::Sampler2D};
    GLUniform uDebugMode        {"uDebugMode",         GLUniform::Int};

    Impl(Public *i) : Base(i)
    {
        uDebugMode = 0;
    }

    void setSize(const Vector2ui &size)
    {
        frame.resize(size);
    }
};

GBuffer::GBuffer()
    : d(new Impl(this))
{}

void GBuffer::glInit(const Context &context)
{
    Render::glInit(context);
    d->quad.glInit(context);
    context.shaders->build(d->quad.program(), "gloom.finalize")
        << d->uMvpMatrix
        << context.view.uInverseProjMatrix
        << d->uGBufferAlbedo << d->uGBufferNormal << d->uGBufferDepth
        << d->uDebugMode;
    d->frame.glInit();
}

void GBuffer::glDeinit()
{
    d->quad.glDeinit();
    d->frame.glDeinit();
    Render::glDeinit();
}

void GBuffer::resize(const Vector2ui &size)
{
    d->setSize(size);
}

void GBuffer::clear()
{
    d->frame.clear(GLFramebuffer::ColorAny | GLFramebuffer::DepthStencil);
}

void GBuffer::render()
{
    d->uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);

    d->uGBufferAlbedo = d->frame.attachedTexture(GLFramebuffer::Color0);
    d->uGBufferNormal = d->frame.attachedTexture(GLFramebuffer::Color1);
    d->uGBufferDepth  = d->frame.attachedTexture(GLFramebuffer::DepthStencil);

    d->quad.render();
}

void gloom::GBuffer::setDebugMode(int debugMode)
{
    LOG_AS("GBuffer");
    LOG_MSG("Changing debug mode: %i") << debugMode;

    d->uDebugMode = debugMode;
}

GLFramebuffer &GBuffer::framebuf()
{
    return d->frame;
}

} // namespace gloom
