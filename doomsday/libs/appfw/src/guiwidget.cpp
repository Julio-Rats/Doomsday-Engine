/** @file guiwidget.cpp  Base class for graphical widgets.
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

#include "de/GuiWidget"
#include "de/GuiRootWidget"
#include "de/BlurWidget"
#include "de/Style"
#include "de/BaseGuiApp"
#include "de/IPersistent"

#include <de/Garbage>
#include <de/MouseEvent>
#include <de/Drawable>
#include <de/GLTexture>
#include <de/GLTextureFramebuffer>
#include <de/LogBuffer>
#include <de/FocusWidget>
#include <de/PopupWidget>

#include <QList>

namespace de {

DENG2_PIMPL(GuiWidget)
, DENG2_OBSERVES(Widget, ChildAddition)
, DENG2_OBSERVES(ui::Margins, Change)
, DENG2_OBSERVES(Style, Change)
#ifdef DENG2_DEBUG
, DENG2_OBSERVES(Widget, ParentChange)
#endif
{
    enum
    {
        Inited       = 0x1,
        NeedGeometry = 0x2,
        StyleChanged = 0x4,
        FirstUpdateAfterCreation
                     = 0x8,
        DefaultFlags = NeedGeometry | FirstUpdateAfterCreation,
    };

    RuleRectangle rule;     ///< Visual rule, used when drawing.
    std::unique_ptr<RuleRectangle> hitRule; ///< Used only for hit testing. By default matches the visual rule.
    ui::Margins margins;
    Rectanglei savedPos;
    duint32 flags;
    Attributes attribs;
    Background background;
    Animation opacity;
    Animation opacityWhenDisabled;
    Rectanglef oldClip; // when drawing children
    float saturation = 1.f;
    QList<IEventHandler *> eventHandlers;

    // Style.
    DotPath fontId;
    DotPath textColorId;

    // Background blurring.
    struct BlurState
    {
        Time updatedAt;
        Vector2ui size;
        QScopedPointer<GLTextureFramebuffer> fb[2];
        Drawable drawable;
        GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
        GLUniform uColor     { "uColor",     GLUniform::Vec4 };
        GLUniform uTex       { "uTex",       GLUniform::Sampler2D };
        GLUniform uBlurStep  { "uBlurStep",  GLUniform::Vec2 };
        GLUniform uWindow    { "uWindow",    GLUniform::Vec4 };
    };
    std::unique_ptr<BlurState> blur;

    Impl(Public *i)
        : Base(i)
        , margins("gap")
        , flags(DefaultFlags)
        , attribs(DefaultAttributes)
        , opacity(1.f, Animation::Linear)
        , opacityWhenDisabled(1.f, Animation::Linear)
        , fontId("default")
        , textColorId("text")
    {
        self().audienceForChildAddition() += this;
        margins.audienceForChange() += this;

        Style::get().audienceForChange() += this;

#ifdef DENG2_DEBUG
        self().audienceForParentChange() += this;
        rule.setDebugName(self().path());
#endif
    }

    ~Impl() override
    {
        qDeleteAll(eventHandlers);

        // The base class will delete all children, but we need to deinitialize
        // them first.
        self().notifyTree(&Widget::deinitialize);

        deinitBlur();

        /*
         * Deinitialization must occur before destruction so that GL resources
         * are not leaked. Derived classes are responsible for deinitializing
         * first before beginning destruction.
         */
#ifdef DENG2_DEBUG
        if (flags & Inited) qDebug() << "GuiWidget" << thisPublic << self().name() << "is still inited!";
        DENG2_ASSERT(!(flags & Inited));
#endif
    }

    void marginsChanged() override
    {
        flags |= StyleChanged;
    }

#ifdef DENG2_DEBUG
    void widgetParentChanged(Widget &, Widget *, Widget *) override
    {
        rule.setDebugName(self().path());
    }
#endif

    void widgetChildAdded(Widget &child) override
    {
        if (self().hasRoot())
        {
            // Make sure newly added children know the view size.
            child.viewResized();
            child.notifyTree(&Widget::viewResized);
        }
    }

    /**
     * Test if a clipped widget is contained within its ancestors' clip rectangles.
     */
    bool isClipCulled() const
    {
        bool wasClipped = false;
        Rectanglei visibleArea = self().root().viewRule().recti();

        for (GuiWidget const *w = self().parentGuiWidget(); w; w = w->parentGuiWidget())
        {
            // Does this ancestor use child clipping?
            if (w->behavior().testFlag(ChildVisibilityClipping))
            {
                wasClipped = true;
                visibleArea &= w->rule().recti();
            }
        }
        if (!wasClipped) return false;

        if (self().isClipped())
        {
            const int CULL_SAFETY_WIDTH = int(pointsToPixels(40)); // avoid pop-in when scrolling

            // Clipped widgets are guaranteed to be within their rectangle.
            return !visibleArea.overlaps(self().rule().recti().expanded(CULL_SAFETY_WIDTH));
        }
        // Otherwise widgets may draw anywhere in the view.
        return visibleArea.isNull();
    }

    void initBlur()
    {
        if (blur) return;

        blur.reset(new BlurState);

        // The blurred version of the view is downsampled.
        blur->size = (self().root().viewSize() / GuiWidget::pointsToPixels(4)).max(Vector2ui(1, 1));

        for (int i = 0; i < 2; ++i)
        {
            // Multisampling is disabled in the blurs for now.
            blur->fb[i].reset(new GLTextureFramebuffer(Image::RGB_888, blur->size, 1));
            blur->fb[i]->glInit();
            blur->fb[i]->colorTexture().setFilter(gl::Linear, gl::Linear, gl::MipNone);
        }

        // Set up the drawble.
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        blur->drawable.addBuffer(buf);
        buf->setVertices(gl::TriangleStrip,
                         DefaultVertexBuf::Builder().makeQuad(
                             Rectanglef(0, 0, 1, 1),
                             Vector4f(1, 1, 1, 1),
                             Rectanglef(0, 0, 1, 1)),
                         gl::Static);

        blur->uBlurStep = Vector2f(1.f / float(blur->size.x),
                                   1.f / float(blur->size.y));

        self().root().shaders().build(blur->drawable.program(), "fx.blur.horizontal")
                << blur->uMvpMatrix
                << blur->uTex
                << blur->uBlurStep
                << blur->uWindow;

        blur->drawable.addProgram("vert");
        self().root().shaders().build(blur->drawable.program("vert"), "fx.blur.vertical")
                << blur->uMvpMatrix
                << blur->uTex
                << blur->uColor
                << blur->uBlurStep
                << blur->uWindow;

        blur->updatedAt = Time::currentHighPerformanceTime();
    }

    void deinitBlur()
    {
        if (!blur) return;

        for (int i = 0; i < 2; ++i)
        {
            blur->fb[i].reset();
        }
        blur->drawable.clear();

        blur.reset();
    }

    void updateBlurredBackground()
    {
        if (blur)
        {
            auto const now = Time::currentHighPerformanceTime();
            if (blur->updatedAt == now)
            {
                return;
            }
            blur->updatedAt = now;
        }

        // Ensure normal drawing is complete.
        auto &painter = self().root().painter();
        painter.flush();

        // Make sure blurring is initialized.
        initBlur();

        auto const oldClip = painter.normalizedScissor();

        DENG2_ASSERT(blur->fb[0]->isReady());

        // Pass 1: render all the widgets behind this one onto the first blur
        // texture, downsampled.
        GLState::push()
            .setTarget(*blur->fb[0])
            .setViewport(Rectangleui::fromSize(blur->size));
        blur->fb[0]->clear(GLFramebuffer::Depth);
        self().root().drawUntil(self());
        GLState::pop();

        blur->fb[0]->resolveSamples();

        // Pass 2: apply the horizontal blur filter to draw the background
        // contents onto the second blur texture.
        GLState::push()
            .setTarget(*blur->fb[1])
            .setViewport(Rectangleui::fromSize(blur->size));
        blur->uTex = blur->fb[0]->colorTexture();
        blur->uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        blur->uWindow = Vector4f(0, 0, 1, 1);
        blur->drawable.setProgram(blur->drawable.program());
        blur->drawable.draw();
        GLState::pop();

        blur->fb[1]->resolveSamples();

        painter.setNormalizedScissor(oldClip);
    }

    void drawBlurredBackground()
    {
        if (background.type == Background::SharedBlur ||
            background.type == Background::SharedBlurWithBorderGlow)
        {
            // Use another widget's blur.
            DENG2_ASSERT(background.blur != 0);
            if (background.blur)
            {
                self().root().painter().flush();
                background.blur->d->updateBlurredBackground();
                background.blur->drawBlurredRect(self().rule().recti(), background.solidFill);
            }
            return;
        }

        if (background.type != Background::Blurred &&
            background.type != Background::BlurredWithBorderGlow &&
            background.type != Background::BlurredWithSolidFill)
        {
            deinitBlur();
            return;
        }

        // Pass 3: apply the vertical blur filter, drawing the final result
        // into the original target.
        Vector4f blurColor = background.solidFill;
        float blurOpacity  = self().visibleOpacity();
        if (background.type == Background::BlurredWithSolidFill)
        {
            blurColor.w = 1;
        }
        if (blurColor.w > 0 && blurOpacity > 0)
        {
            updateBlurredBackground();
            self().drawBlurredRect(self().rule().recti(), blurColor, blurOpacity);
        }
    }

    inline float currentOpacity() const
    {
        return min(opacity.value(), opacityWhenDisabled.value());
    }

    void updateOpacityForDisabledWidgets()
    {
        float const opac = (self().isDisabled()? .3f : 1.f);
        if (!fequal(opacityWhenDisabled.target(), opac))
        {
            opacityWhenDisabled.setValue(opac, 0.3);
        }
        if ((flags & FirstUpdateAfterCreation) ||
            !attribs.testFlag(AnimateOpacityWhenEnabledOrDisabled))
        {
            opacityWhenDisabled.finish();
        }
    }

    void restoreState()
    {
        try
        {
            if (IPersistent *po = maybeAs<IPersistent>(self()))
            {
                DENG2_BASE_GUI_APP->persistentUIState() >> *po;
            }
        }
        catch (Error const &er)
        {
            // Benign: widget will use default state.
            LOG_VERBOSE("Failed to restore state of widget '%s': %s")
                    << self().path() << er.asText();
        }
    }

    void saveState()
    {
        try
        {
            if (IPersistent *po = maybeAs<IPersistent>(self()))
            {
                DENG2_BASE_GUI_APP->persistentUIState() << *po;
            }
        }
        catch (Error const &er)
        {
            LOG_WARNING("Failed to save state of widget '%s': %s")
                    << self().path() << er.asText();
        }
    }

    GuiWidget *findNextWidgetToFocus(WalkDirection dir)
    {
        PopupWidget *parentPopup = self().findParentPopup();
        Rectanglei const viewRect = self().root().viewRule().recti();
        bool escaped = false;
        auto *widget = self().walkInOrder(dir, [this, &viewRect, parentPopup, &escaped] (Widget &widget)
        {
            if (parentPopup && !widget.hasAncestor(*parentPopup))
            {
                // Cannot get out of the popup.
                escaped = true;
                return LoopAbort;
            }
            if (widget.canBeFocused() && is<GuiWidget>(widget))
            {
                // The widget's center must be in view.
                if (viewRect.contains(widget.as<GuiWidget>().rule().recti().middle()))
                {
                    // This is good.
                    return LoopAbort;
                }
            }
            return LoopContinue;
        });
        if (widget && !escaped)
        {
            return widget->asPtr<GuiWidget>();
        }
        return nullptr;
    }

    float scoreForWidget(GuiWidget const &widget, ui::Direction dir) const
    {
        if (!widget.canBeFocused() || &widget == thisPublic)
        {
            return -1;
        }

        Rectanglef const viewRect  = self().root().viewRule().rect();
        Rectanglef const selfRect  = self().hitRule().rect();
        Rectanglef const otherRect = widget.hitRule().rect();
        Vector2f const otherMiddle =
                (dir == ui::Up?   otherRect.midBottom() :
                 dir == ui::Down? otherRect.midTop()    :
                 dir == ui::Left? otherRect.midRight()  :
                                  otherRect.midLeft()  );
                //otherRect.middle();

        if (!viewRect.contains(otherMiddle))
        {
            return -1;
        }

        bool const axisOverlap =
                (isHorizontal(dir) && !selfRect.vertical()  .intersection(otherRect.vertical())  .isEmpty()) ||
                (isVertical(dir)   && !selfRect.horizontal().intersection(otherRect.horizontal()).isEmpty());

        // Check for contacting edges.
        float edgeDistance = 0; // valid only if axisOverlap
        if (axisOverlap)
        {
            switch (dir)
            {
            case ui::Left:
                edgeDistance = selfRect.left() - otherRect.right();
                break;

            case ui::Up:
                edgeDistance = selfRect.top() - otherRect.bottom();
                break;

            case ui::Right:
                edgeDistance = otherRect.left() - selfRect.right();
                break;

            default:
                edgeDistance = otherRect.top() - selfRect.bottom();
                break;
            }
            // Very close edges are considered contacting.
            if (edgeDistance >= 0 && edgeDistance < pointsToPixels(5))
            {
                return edgeDistance;
            }
        }

        Vector2f const middle    = (dir == ui::Up?   selfRect.midTop()    :
                                    dir == ui::Down? selfRect.midBottom() :
                                    dir == ui::Left? selfRect.midLeft()   :
                                                     selfRect.midRight() );
        Vector2f const delta     = otherMiddle - middle;
        Vector2f const dirVector = directionVector(dir);
        auto dotProd = delta.normalize().dot(dirVector);
        if (dotProd <= 0)
        {
            // On the wrong side.
            return -1;
        }
        float distance = delta.length();
        if (axisOverlap)
        {
            dotProd = 1.0;
            if (edgeDistance > 0)
            {
                distance = de::min(distance, edgeDistance);
            }
        }

        float favorability = 1;
        if (widget.parentWidget() == self().parentWidget())
        {
            favorability = .1f; // Siblings are much preferred.
        }
        else if (self().hasAncestor(widget) || widget.hasAncestor(self()))
        {
            favorability = .2f; // Ancestry is also good.
        }

        // Prefer widgets that are nearby, particularly in the specified direction.
        return distance * (.5f + acos(dotProd)) * favorability;
    }

    GuiWidget *findAdjacentWidgetToFocus(ui::Direction dir) const
    {
        float bestScore = 0;
        GuiWidget *bestWidget = nullptr;

        // Focus navigation is always contained within the popup where the focus is
        // currently in.
        Widget *walkRoot = self().findParentPopup();
        if (!walkRoot) walkRoot = &self().root();

        walkRoot->walkChildren(Forward, [this, &dir, &bestScore, &bestWidget] (Widget &widget)
        {
            GuiWidget *gui = &widget.as<GuiWidget>();
            {
                float score = scoreForWidget(*gui, dir);
                if (score >= 0)
                {
                    /*qDebug() << "Scored:" << gui
                             << score
                             << "rect:" << gui->rule().recti().asText()
                             << (gui->is<LabelWidget>()? gui->as<LabelWidget>().text() : String());*/

                    if (!bestWidget || score < bestScore)
                    {
                        bestWidget = gui;
                        bestScore  = score;
                    }
                }
            }
            return LoopContinue;
        });
        // Consider all the widgets in the tree.
        /*if (bestWidget)
        {
            qDebug() << "Best:" << bestWidget
                     << "focusable:" << bestWidget->behavior().testFlag(Focusable)
                     << "rect:" << bestWidget->rule().recti().asText()
                     << "opacity:" << bestWidget->visibleOpacity()
                     << "visible:" << bestWidget->isVisible();
        }*/
        return bestWidget? bestWidget : thisPublic;
    }

    void styleChanged(Style &) override
    {
        deinitBlur();
        flags |= StyleChanged;
        // updateStyle() will be called during the next update().
    }

    static inline float pointsToPixels(double points)
    {
        return float(points) * DENG2_BASE_GUI_APP->pixelRatio().value();
    }

    static inline float pixelsToPoints(double pixels)
    {
        return float(pixels) / DENG2_BASE_GUI_APP->pixelRatio().value();
    }
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Impl(this))
{
    d->rule.setDebugName(name);
}

void GuiWidget::destroy(GuiWidget *widget)
{
    if (widget)
    {
        widget->deinitialize();
        delete widget;
    }
}

void GuiWidget::destroyLater(GuiWidget *widget)
{
    if (widget)
    {
        widget->deinitialize();
        widget->guiDeleteLater();
    }
}

GuiRootWidget &GuiWidget::root() const
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

GuiWidget::Children GuiWidget::childWidgets() const
{
    Children children;
    children.reserve(childCount());
    foreach (Widget *c, Widget::children())
    {
        DENG2_ASSERT(is<GuiWidget>(c));
        children.append(static_cast<GuiWidget *>(c));
    }
    return children;
}

GuiWidget *GuiWidget::parentGuiWidget() const
{
    Widget *p = parentWidget();
    if (!p) return nullptr;
    if (!p->parent())
    {
        if (is<RootWidget>(p)) return nullptr; // GuiRootWidget is not a GuiWidget
    }
    return static_cast<GuiWidget *>(p);
}

Style const &GuiWidget::style() const
{
    return Style::get();
}

Rule const &GuiWidget::rule(DotPath const &path) const
{
    return style().rules().rule(path);
}

Font const &GuiWidget::font() const
{
    return style().fonts().font(d->fontId);
}

DotPath const &GuiWidget::fontId() const
{
    return d->fontId;
}

DotPath const &GuiWidget::textColorId() const
{
    return d->textColorId;
}

void GuiWidget::setFont(DotPath const &id)
{
    d->fontId = id;
    d->flags |= Impl::StyleChanged;
}

ColorBank::Color GuiWidget::textColor() const
{
    return style().colors().color(d->textColorId);
}

ColorBank::Colorf GuiWidget::textColorf() const
{
    return style().colors().colorf(d->textColorId);
}

void GuiWidget::setTextColor(DotPath const &id)
{
    d->textColorId = id;
    d->flags |= Impl::StyleChanged;
}

RuleRectangle &GuiWidget::rule()
{
    return d->rule;
}

Rectanglei GuiWidget::contentRect() const
{
    Vector4i const pad = margins().toVector();
    return rule().recti().adjusted(pad.xy(), -pad.zw());
}

RuleRectangle const &GuiWidget::rule() const
{
    return d->rule;
}

float GuiWidget::estimatedHeight() const
{
    return rule().height().value();
}

ui::Margins &GuiWidget::margins()
{
    return d->margins;
}

ui::Margins const &GuiWidget::margins() const
{
    return d->margins;
}

Rectanglef GuiWidget::normalizedRect(de::Rectanglei const &rect,
                                     de::Rectanglei const &containerRect) // static
{
    Rectanglef const rectf = rect.moved(-containerRect.topLeft);
    Vector2f const contSize = containerRect.size();
    return Rectanglef(Vector2f(rectf.left()   / contSize.x,
                               rectf.top()    / contSize.y),
                      Vector2f(rectf.right()  / contSize.x,
                               rectf.bottom() / contSize.y));
}

float GuiWidget::pointsToPixels(float points)
{
    return Impl::pointsToPixels(points);
}

float GuiWidget::pixelsToPoints(float pixels)
{
    return Impl::pixelsToPoints(pixels);
}

Rectanglef GuiWidget::normalizedRect() const
{
    return GuiWidget::normalizedRect(rule().recti(),
                                     Rectanglei::fromSize(root().viewSize()));
}

Rectanglef GuiWidget::normalizedRect(Rectanglei const &viewSpaceRect) const
{
    return GuiWidget::normalizedRect(viewSpaceRect,
                                     Rectanglei::fromSize(root().viewSize()));
}

Rectanglef GuiWidget::normalizedContentRect() const
{
    Rectanglef const rect = rule().rect().adjusted( Vector2f(margins().left().value(),
                                                             margins().top().value()),
                                                   -Vector2f(margins().right().value(),
                                                             margins().bottom().value()));
    GuiRootWidget::Size const &viewSize = root().viewSize();
    return Rectanglef(Vector2f(float(rect.left())   / float(viewSize.x),
                               float(rect.top())    / float(viewSize.y)),
                      Vector2f(float(rect.right())  / float(viewSize.x),
                               float(rect.bottom()) / float(viewSize.y)));
}

static void deleteGuiWidget(void *ptr)
{
    GuiWidget::destroy(reinterpret_cast<GuiWidget *>(ptr));
}

void GuiWidget::guiDeleteLater()
{
    Garbage_TrashInstance(this, deleteGuiWidget);
}

GuiWidget::ColorTheme GuiWidget::invertColorTheme(ColorTheme theme)
{
    return theme == Inverted? Normal : Inverted;
}

void GuiWidget::recycleTrashedWidgets()
{
    Garbage_RecycleAllWithDestructor(deleteGuiWidget);
}

void GuiWidget::set(Background const &bg)
{
    d->background = bg;
    requestGeometry();
}

void GuiWidget::setSaturation(float saturation)
{
    d->saturation = saturation;
}

bool GuiWidget::isClipped() const
{
    return behavior().testFlag(ContentClipping);
}

GuiWidget::Background const &GuiWidget::background() const
{
    return d->background;
}

void GuiWidget::setOpacity(float opacity, TimeSpan span, TimeSpan startDelay)
{
    d->opacity.setValue(opacity, span, startDelay);
}

Animation GuiWidget::opacity() const
{
    return d->opacity;
}

float GuiWidget::visibleOpacity() const
{
    float opacity = d->currentOpacity();
    if (!d->attribs.testFlag(IndependentOpacity))
    {
        for (GuiWidget *i = parentGuiWidget(); i; i = i->parentGuiWidget())
        {
            opacity *= i->d->currentOpacity();
        }
    }
    return opacity;
}

void GuiWidget::addEventHandler(IEventHandler *handler)
{
    d->eventHandlers.append(handler);
}

void GuiWidget::removeEventHandler(IEventHandler *handler)
{
    d->eventHandlers.removeOne(handler);
}

void GuiWidget::setAttribute(Attributes const &attr, FlagOpArg op)
{
    applyFlagOperation(d->attribs, attr, op);
}

GuiWidget::Attributes GuiWidget::attributes() const
{
    return d->attribs;
}

GuiWidget::Attributes GuiWidget::familyAttributes() const
{
    Attributes attribs = d->attribs;
    for (GuiWidget const *p = parentGuiWidget(); p; p = p->parentGuiWidget())
    {
        attribs |= p->attributes() & FamilyAttributes;
    }
    return attribs;
}

void GuiWidget::saveState()
{
    d->saveState();

    foreach (GuiWidget *child, childWidgets())
    {
        child->saveState();
    }
}

void GuiWidget::restoreState()
{
    d->restoreState();

    foreach (GuiWidget *child, childWidgets())
    {
        child->restoreState();
    }
}

void GuiWidget::initialize()
{
    if (d->flags & Impl::Inited) return;

    try
    {
        // Each widget has a single root, and it never changes.
        setRoot(findRoot());

        d->flags |= Impl::Inited;
        glInit();

        if (d->attribs.testFlag(RetainStatePersistently))
        {
            d->restoreState();
        }
    }
    catch (Error const &er)
    {
        LOG_WARNING("Error when initializing widget '%s': %s")
                << name() << er.asText();
    }
}

void GuiWidget::deinitialize()
{
    if (!(d->flags & Impl::Inited)) return;

    try
    {
        GLWindow::main().glActivate(); // This may be called via deferred destructors.

        if (d->attribs.testFlag(RetainStatePersistently))
        {
            d->saveState();
        }

        applyFlagOperation(d->flags, Impl::Inited, false);
        d->deinitBlur();
        glDeinit();
        setRoot(nullptr);
    }
    catch (Error const &er)
    {
        LOG_WARNING("Error when deinitializing widget '%s': %s")
                << name() << er.asText();
    }
}

void GuiWidget::viewResized()
{
    d->deinitBlur();
}

void GuiWidget::update()
{
    if (!(d->flags & Impl::Inited))
    {
        initialize();
    }
    if (d->flags & Impl::StyleChanged)
    {
        applyFlagOperation(d->flags, Impl::StyleChanged, false);
        updateStyle();
    }
    auto const familyAttribs = familyAttributes();
    if ( familyAttribs.testFlag(AutomaticOpacity) ||
        !familyAttribs.testFlag(ManualOpacity))
    {
        d->updateOpacityForDisabledWidgets();
    }
    applyFlagOperation(d->flags, Impl::FirstUpdateAfterCreation, false);
}

void GuiWidget::draw()
{
    if ((d->flags & Impl::Inited) && !isHidden() && visibleOpacity() > 0 && !d->isClipCulled())
    {
#ifdef DENG2_DEBUG
        // Detect mistakes in GLState stack usage.
        dsize const depthBeforeDrawingWidget = GLState::stackDepth();
#endif
        if (!d->attribs.testFlag(DontDrawContent))
        {
            d->drawBlurredBackground();

            auto &painter = root().painter();
            painter.setSaturation(d->saturation);

            Rectanglef const oldClip = painter.normalizedScissor();
            if (isClipped())
            {
                painter.setNormalizedScissor(oldClip & normalizedRect());
            }

            drawContent();

            if (isClipped())
            {
                painter.setNormalizedScissor(oldClip);
            }
            painter.setSaturation(1.f);
        }

        DENG2_ASSERT(GLState::stackDepth() == depthBeforeDrawingWidget);
    }
}

bool GuiWidget::handleEvent(Event const &event)
{
    foreach (IEventHandler *handler, d->eventHandlers)
    {
        if (handler->handleEvent(*this, event))
        {
            return true;
        }
    }

    if (hasFocus() && event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if (!attributes().testFlag(FocusCyclingDisabled) && key.ddKey() == DDKEY_TAB)
        {
            if (auto *focus = d->findNextWidgetToFocus(
                        key.modifiers().testFlag(KeyEvent::Shift)? Backward : Forward))
            {
                root().focusIndicator().fadeIn();
                root().setFocus(focus);
                return true;
            }
        }
        if (!attributes().testFlag(FocusMoveWithArrowKeysDisabled) &&
            (key.ddKey() == DDKEY_LEFTARROW  ||
             key.ddKey() == DDKEY_RIGHTARROW ||
             key.ddKey() == DDKEY_UPARROW    ||
             key.ddKey() == DDKEY_DOWNARROW  ))
        {
            root().focusIndicator().fadeIn();
            root().setFocus(d->findAdjacentWidgetToFocus(
                                key.ddKey() == DDKEY_LEFTARROW ? ui::Left  :
                                key.ddKey() == DDKEY_RIGHTARROW? ui::Right :
                                key.ddKey() == DDKEY_UPARROW   ? ui::Up    :
                                                                 ui::Down));
            return true;
        }
    }

    if (Widget::handleEvent(event))
    {
        return true;
    }

    if (d->attribs.testFlag(EatAllMouseEvents))
    {
        if ((event.type() == Event::MouseButton ||
             event.type() == Event::MousePosition ||
             event.type() == Event::MouseWheel) && hitTest(event))
        {
            return true;
        }
    }
    return false;
}

bool GuiWidget::hitTest(Vector2i const &pos) const
{
    if (behavior().testFlag(Unhittable))
    {
        // Can never be hit by anything.
        return false;
    }

    Widget const *w = Widget::parent();
    while (w)
    {
        GuiWidget const *gui = dynamic_cast<GuiWidget const *>(w);
        if (gui)
        {
            if (gui->behavior().testFlag(ChildHitClipping) &&
               !gui->hitRule().recti().contains(pos))
            {
                // Must hit clipped parent widgets as well.
                return false;
            }
        }
        w = w->Widget::parent();
    }

    return hitRule().recti().contains(pos);
}

bool GuiWidget::hitTest(Event const &event) const
{
    return event.isMouse() && hitTest(event.as<MouseEvent>().pos());
}

GuiWidget const *GuiWidget::treeHitTest(Vector2i const &pos) const
{
    Children const childs = childWidgets();
    for (int i = childs.size() - 1; i >= 0; --i)
    {
        // Check children first.
        if (GuiWidget const *hit = childs.at(i)->treeHitTest(pos))
        {
            return hit;
        }
    }
    if (hitTest(pos))
    {
        return this;
    }
    return 0;
}

RuleRectangle &GuiWidget::hitRule()
{
    if (!d->hitRule)
    {
        d->hitRule.reset(new RuleRectangle);
        d->hitRule->setRect(d->rule);
    }
    return *d->hitRule;
}

RuleRectangle const &GuiWidget::hitRule() const
{
    if (d->hitRule) return *d->hitRule;
    return d->rule;
}

GuiWidget::MouseClickStatus GuiWidget::handleMouseClick(Event const &event, MouseEvent::Button button)
{
    if (isDisabled()) return MouseClickUnrelated;

    if (event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if (mouse.button() != button)
        {
            return MouseClickUnrelated;
        }

        if (mouse.state() == MouseEvent::Pressed && hitTest(mouse.pos()))
        {
            root().routeMouse(this);
            return MouseClickStarted;
        }

        if (mouse.state() == MouseEvent::Released && root().isEventRouted(event.type(), this))
        {
            root().routeMouse(nullptr);
            if (hitTest(mouse.pos()))
            {
                return MouseClickFinished;
            }
            return MouseClickAborted;
        }
    }
    return MouseClickUnrelated;
}

void GuiWidget::glInit()
{}

void GuiWidget::glDeinit()
{}

void GuiWidget::drawContent()
{}

void GuiWidget::drawBlurredRect(Rectanglei const &rect, Vector4f const &color, float opacity)
{
    auto *blur = d->blur.get();
    if (!blur) return;

    DENG2_ASSERT(blur->fb[1]->isReady());

    root().painter().flush();

    Vector2ui const viewSize = root().viewSize();

    blur->uTex = blur->fb[1]->colorTexture();
    blur->uColor = Vector4f((1 - color.w) + color.x * color.w,
                            (1 - color.w) + color.y * color.w,
                            (1 - color.w) + color.z * color.w,
                            opacity);
    blur->uWindow = Vector4f(rect.left()   / float(viewSize.x),
                             rect.top()    / float(viewSize.y),
                             rect.width()  / float(viewSize.x),
                             rect.height() / float(viewSize.y));
    blur->uMvpMatrix = root().projMatrix2D() *
                       Matrix4f::scaleThenTranslate(rect.size(), rect.topLeft);
    blur->drawable.setProgram(QStringLiteral("vert"));

    blur->drawable.draw();
}

void GuiWidget::requestGeometry(bool yes)
{
    applyFlagOperation(d->flags, Impl::NeedGeometry, yes);
}

bool GuiWidget::geometryRequested() const
{
    return (d->flags & Impl::NeedGeometry) != 0;
}

bool GuiWidget::isInitialized() const
{
    return (d->flags & Impl::Inited) != 0;
}

bool GuiWidget::canBeFocused() const
{
    if (!Widget::canBeFocused() ||
        fequal(visibleOpacity(), 0.f) ||
        rule().recti().size() == Vector2ui())
    {
        return false;
    }
    return true;
}

GuiWidget *GuiWidget::guiFind(String const &name)
{
    return maybeAs<GuiWidget>(find(name));
}

GuiWidget const *GuiWidget::guiFind(String const &name) const
{
    return maybeAs<GuiWidget>(find(name));
}

PopupWidget *GuiWidget::findParentPopup() const
{
    for (GuiWidget *i = parentGuiWidget(); i; i = i->parentGuiWidget())
    {
        if (PopupWidget *popup = maybeAs<PopupWidget>(i))
        {
            return popup;
        }
    }
    return nullptr;
}

void GuiWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    auto &rootWgt = root();
    float const thick = d->pointsToPixels(d->background.thickness);

    // Is there a solid fill?
    if (d->background.solidFill.w > 0)
    {
        if (d->background.type == Background::GradientFrameWithRoundedFill)
        {
            Rectanglei const recti = rule().recti().shrunk(d->pointsToPixels(2));
            verts.makeQuad(recti.shrunk(thick), d->background.solidFill,
                           rootWgt.atlas().imageRectf(rootWgt.solidRoundCorners()).middle());
            verts.makeFlexibleFrame(recti, thick, d->background.solidFill,
                                    rootWgt.atlas().imageRectf(rootWgt.solidRoundCorners()));
        }
        else if (d->background.type != Background::Blurred &&
                 d->background.type != Background::BlurredWithBorderGlow &&
                 d->background.type != Background::SharedBlur &&
                 d->background.type != Background::SharedBlurWithBorderGlow)
        {
            verts.makeQuad(rule().recti(),
                           d->background.solidFill,
                           rootWgt.atlas().imageRectf(rootWgt.solidWhitePixel()).middle());
        }
    }

    switch (d->background.type)
    {
    case Background::GradientFrame:
    case Background::GradientFrameWithRoundedFill:
    case Background::GradientFrameWithThinBorder:
        if (d->background.type == Background::GradientFrameWithThinBorder)
        {
            verts.makeFlexibleFrame(rule().recti().shrunk(d->pointsToPixels(2)),
                                    thick,
                                    Vector4f(0, 0, 0, .5f),
                                    rootWgt.atlas().imageRectf(rootWgt.boldRoundCorners()));
        }
        verts.makeFlexibleFrame(rule().recti().shrunk(d->pointsToPixels(1)),
                                thick,
                                d->background.color,
                                rootWgt.atlas().imageRectf(rootWgt.boldRoundCorners()));
        break;

    case Background::Rounded:
        verts.makeFlexibleFrame(rule().recti().shrunk(d->pointsToPixels(d->background.thickness - 4)),
                                thick,
                                d->background.color,
                                rootWgt.atlas().imageRectf(rootWgt.roundCorners()));
        break;

    case Background::BorderGlow:
    case Background::BlurredWithBorderGlow:
    case Background::SharedBlurWithBorderGlow:
        verts.makeFlexibleFrame(rule().recti().expanded(thick),
                                thick,
                                d->background.color,
                                rootWgt.atlas().imageRectf(rootWgt.borderGlow()));
        break;

    case Background::Blurred: // blurs drawn separately in GuiWidget::draw()
    case Background::SharedBlur:
    case Background::BlurredWithSolidFill:
        break;

    case Background::None:
        break;
    }
}

bool GuiWidget::hasChangedPlace(Rectanglei &currentPlace)
{
    currentPlace = rule().recti();
    bool changed = (d->savedPos != currentPlace);
    d->savedPos = currentPlace;
    return changed;
}

bool GuiWidget::hasBeenUpdated() const
{
    return !(d->flags & Impl::FirstUpdateAfterCreation);
}

void GuiWidget::updateStyle()
{}

Animation &GuiWidget::opacityAnimation()
{
    return d->opacity;
}

void GuiWidget::preDrawChildren()
{
    if (behavior().testFlag(ChildVisibilityClipping))
    {
        auto &painter = root().painter();
        d->oldClip = painter.normalizedScissor();
        painter.setNormalizedScissor(d->oldClip & normalizedRect());
    }
}

void GuiWidget::postDrawChildren()
{
    if (behavior().testFlag(ChildVisibilityClipping))
    {
        root().painter().setNormalizedScissor(d->oldClip);
    }

    // Focus indicator is an overlay.
    auto &rootWidget = root();
    if (rootWidget.focus() && rootWidget.focus()->parentWidget() == this)
    {
        rootWidget.focusIndicator().draw();
    }
}

void GuiWidget::collectNotReadyAssets(AssetGroup &collected, CollectMode collectMode)
{
#if defined (DENG2_DEBUG)
    if (!rule().isFullyDefined())
    {
        qDebug() << path() << "rule rectangle not fully defined";
        qDebug("%s", rule().description().toLatin1().constData());
        qDebug("Widget layout will be undefined");
    }
#endif
    Widget::collectNotReadyAssets(collected, collectMode);
}

} // namespace de
