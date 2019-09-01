/** @file glframebuffer.h  GL render target.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLFRAMEBUFFER_H
#define LIBGUI_GLFRAMEBUFFER_H

#include <QImage>

#include <de/libcore.h>
#include <de/Asset>
#include <de/Error>
#include <de/Vector>
#include <de/Rectangle>
#include <QFlags>

#include "../gui/libgui.h"
#include "opengl.h"
#include "../GLTexture"

namespace de {

/**
 * GL render target.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLFramebuffer : public Asset
{
public:
    /// Something is incorrect in the configuration of the contained
    /// framebuffer object. @ingroup errors
    DENG2_ERROR(ConfigError);

    enum Flag {
        Color0   = 0x001, ///< Target has a color attachment.
        Color1   = 0x002,
        Color2   = 0x004,
        Color3   = 0x008,
        Depth    = 0x100, ///< Target has a depth attachment.
        Stencil  = 0x200, ///< Target has a stencil attachment.

        Changed = 0x1000, ///< Draw/clear has occurred on the target.

        ColorAny          = Color0 | Color1 | Color2 | Color3,
        ColorDepth        = Color0 | Depth,
        ColorDepthStencil = Color0 | Depth | Stencil,
        ColorStencil      = Color0 | Stencil,
        DepthStencil      = Depth  | Stencil,

        SeparateDepthAndStencil = 0x2000, ///< Depth and stencil should use separate buffers.

        NoAttachments = 0,
        DefaultFlags  = ColorDepth,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef Vec2ui Size;

public:
    static void setDefaultFramebuffer(GLuint defaultFBO);

    /**
     * Constructs a default render target.
     */
    GLFramebuffer();

    /**
     * Constructs a render target than renders onto a texture. The texture must
     * be initialized with the appropriate size beforehand.
     *
     * @param colorTarget       Target texture for Color attachment.
     * @param otherAttachments  Other supporting attachments (renderbuffers).
     */
    GLFramebuffer(GLTexture &colorTarget, Flags otherAttachments = NoAttachments);

    /**
     * Constructs a render target with a texture attachment and optionally
     * other renderbuffer attachments.
     *
     * @param attachment        Where to attach the texture (color, depth, stencil).
     * @param texture           Texture to render on.
     * @param otherAttachments  Other supporting attachments (renderbuffers).
     */
    GLFramebuffer(Flags attachment, GLTexture &texture,
                  Flags otherAttachments = NoAttachments);

    //GLFramebuffer(GLTexture *color, GLTexture *depth = 0, GLTexture *stencil = 0);

    /**
     * Constructs a render target with a specific size.
     *
     * @param size   Size of the render target.
     * @param flags  Attachments to set up.
     */
    GLFramebuffer(Vec2ui const &size, Flags flags = DefaultFlags);

    Flags flags() const;

    /**
     * Marks the rendering target modified. This is done automatically when the
     * target is cleared or when GLBuffer is used to draw something on the
     * target.
     */
    void markAsChanged();

    /**
     * Reconfigures the render target back to the default OpenGL framebuffer.
     * All attachments will be released.
     */
    void configure();

    /**
     * Configure the target with one or more renderbuffers. Multisampled buffers
     * can be configured by giving a @a sampleCount higher than 1.
     *
     * @param size         Size of the render target.
     * @param flags        Which attachments to set up.
     * @param sampleCount  Number of samples per pixel in each attachment.
     */
    void configure(Vec2ui const &size,
                   Flags flags = DefaultFlags,
                   int sampleCount = 1);

    /**
     * Reconfigures the render target with two textures, one for the color
     * values and one for depth/stencil values.
     *
     * If @a colorTex or @a depthStencilTex is omitted, a renderbuffer will be
     * created in its place.
     *
     * Any previous attachments are released.
     *
     * @param colorTex         Texture for color values.
     * @param depthStencilTex  Texture for depth/stencil values.
     */
    void configure(GLTexture *colorTex, GLTexture *depthStencilTex);

    /**
     * Reconfigures the framebuffer with multiple textures.
     *
     * @param colorTextures    Textures for color attachments.
     * @param depthStencilTex  Texture for depth/stencil values.
     */
    void configure(QList<GLTexture *> colorTextures, GLTexture *depthStencilTex);

    /**
     * Changes the configuration of the render target. Any previously allocated
     * renderbuffers are released.
     *
     * @param attachment  Where to attach @a texture.
     * @param texture     Texture to render on.
     * @param otherAttachments  Other supporting attachments (renderbuffers).
     */
    void configure(Flags attachment,
                   GLTexture &texture,
                   Flags otherAttachments = NoAttachments);

    /**
     * Release all GL resources for the framebuffer. This is the opposite to configure().
     * The object is in an unusable state after the call, ready to be destroyed.
     */
    void deinit();

    /**
     * Activates this render target as the one where GL drawing is being done.
     */
    void glBind() const;

    /**
     * Deactivates the render target.
     */
    void glRelease() const;

    GLuint glName() const;

    Size size() const;

    /**
     * Copies the contents of the render target's color attachment to an image.
     */
    QImage toImage() const;

    /**
     * Sets the color for clearing the target (see clear()).
     *
     * @param color  Color for clearing.
     */
    void setClearColor(Vec4f const &color);

    /**
     * Clears the contents of the render target's attached buffers.
     *
     * @param attachments  Which ones to clear.
     */
    void clear(Flags attachments);

    /**
     * Resizes the target's attached buffers and/or textures to a new size.
     * Nothing happens if the provided size is the same as the current size. If
     * resizing occurs, the contents of all buffers/textures become undefined.
     *
     * @param size  New size for buffers and/or textures.
     */
    void resize(Size const &size);

    /**
     * Returns the texture being used for a particular attachment in this target.
     *
     * @param attachment  Which attachment.
     * @return
     */
    virtual GLTexture *attachedTexture(Flags attachment) const;

    /**
     * Returns the render buffer attachment, if one exists.
     *
     * @param attachment  Which attachment.
     *
     * @return GL name of the render buffer.
     */
    GLuint attachedRenderBuffer(Flags attachment) const;

    /**
     * Replaces a currently attached texture with another.
     *
     * @param attachment  Which attachment.
     * @param texture     New texture to use as the attachment.
     */
    void replaceAttachment(Flags attachment, GLTexture &texture);

    /**
     * Replaces an attachment with a render buffer.
     *
     * @param attachment      Which attachment.
     * @param renderBufferId  GL name of a render buffer.
     */
    void replaceAttachment(Flags attachment, GLuint renderBufferId);

    void replaceWithNewRenderBuffer(Flags attachment);

    void releaseAttachment(Flags attachment);

    /**
     * Blits this target's contents to the @a copy target.
     *
     * @param dest         Target where to copy this target's contents.
     * @param attachments  Which attachments to blit.
     * @param filtering    Filtering to use if source and destinations sizes
     *                     aren't the same.
     */
    void blit(GLFramebuffer &dest,
              Flags          attachments = Color0,
              gl::Filter     filtering   = gl::Nearest) const;

    /**
     * Blits this target's contents to the default framebuffer.
     * @param filtering  Blit filtering.
     */
    void blit(gl::Filter filtering = gl::Nearest) const;

    /**
     * Sets the subregion inside the render target where scissor and viewport
     * will be scaled into. Scissor and viewport can still be defined as if the
     * entire window was in use; this only applies an offset and scaling to
     * both.
     *
     * @param rect   Target window rectangle. Set a null rectangle to
     *               use the entire window (like normally).
     * @param applyGLState  Immediately update current OpenGL state accordingly.
     */
    void setActiveRect(Rectangleui const &rect, bool applyGLState = false);

    void unsetActiveRect(bool applyGLState = false);

    Vec2f activeRectScale() const;
    Vec2f activeRectNormalizedOffset() const;
    Rectangleui scaleToActiveRect(Rectangleui const &rect) const;
    Rectangleui const &activeRect() const;
    bool hasActiveRect() const;

    /**
     * Returns the area of the target currently in use. This is the full area
     * of the target, if there is no active rectangle specified. Otherwise
     * some sub-area of the target is returned.
     */
    Rectangleui rectInUse() const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GLFramebuffer::Flags)

} // namespace de

#endif // LIBGUI_GLFRAMEBUFFER_H
