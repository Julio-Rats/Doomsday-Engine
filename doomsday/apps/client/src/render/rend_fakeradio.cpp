/** @file rend_fakeradio.cpp  Geometry generation for faked, radiosity lighting.
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "render/rend_fakeradio.h"

#include <de/Vector>
#include <doomsday/console/var.h>
#include "clientapp.h"

#include "gl/gl_texmanager.h"

#include "MaterialAnimator"
#include "MaterialVariantSpec"

#include "world/map.h"
#include "ConvexSubspace"
#include "Line"
#include "Surface"
#include "client/clientsubsector.h"

#include "Face"
#include "HEdge"
#include "WallEdge"

#include "render/rend_main.h"
#include "render/rendersystem.h"
#include "render/r_main.h"  // levelFullBright
#include "render/drawlist.h"
#include "render/shadowedge.h"
#include "render/viewports.h"  // R_FrameCount()
#include "render/store.h"

using namespace de;
using namespace world;

enum WallShadow
{
    TopShadow,
    BottomShadow,
    LeftShadow,
    RightShadow
};

static dfloat const MIN_OPEN            = 0.1f;
static ddouble const MINDIFF            = 8;       ///< Min plane height difference (world units).
static ddouble const INDIFF             = 8;       ///< Max plane height for indifference offset.
static dfloat const MIN_SHADOW_DARKNESS = .0001f;  ///< Minimum to qualify.
static ddouble const MIN_SHADOW_SIZE    = 1;       ///< In map units.

dint rendFakeRadio              = true;  ///< cvar
static dfloat fakeRadioDarkness = 1.2f;  ///< cvar
byte devFakeRadioUpdate         = true;  ///< cvar

/**
 * Returns the "shadow darkness" (factor) for the given @a ambientLight (level), derived
 * from values in Config.
 *
 * @param ambientLight  Ambient light level to process. It is assumed that adaptation has
 *                      @em NOT yet been applied (it will be).
 */
static inline dfloat calcShadowDarkness(dfloat ambientLight)
{
    ambientLight += Rend_LightAdaptationDelta(ambientLight);
    return (0.6f - ambientLight * 0.4f) * 0.65f * ::fakeRadioDarkness;
}

/**
 * Returns the "shadow size" in map units for the given @a ambientLight (level).
 *
 * @param ambientLight  Ambient light level to process. It is assumed that adaptation has
 *                      @em NOT yet been applied (it will be).
 */
static inline dfloat calcShadowSize(dfloat ambientLight)
{
    return 2 * (8 + 16 - ambientLight * 16);  /// @todo Make cvars out of constants.
}

/**
 * Returns the "wall height" (i.e., distance in map units) of the wall described by
 * @a leftEdge and @a rightEdge.
 *
 * @param leftEdge    WallEdge describing the logical left-edge of the wall section.
 * @param rightEdge   WallEdge describing the logical right-edge of the wall section.
 *
 * @see wallWidth()
 * @see wallDimensions()
 */
static inline ddouble wallHeight(WallEdge const &leftEdge, WallEdge const &rightEdge)
{
    return rightEdge.top().origin().z - leftEdge.bottom().origin().z;
}

/**
 * Returns the "wall width" (i.e., distance in map units) of the wall described by
 * @a leftEdge and @a rightEdge.
 *
 * @param leftEdge    WallEdge describing the logical left-edge of the wall section.
 * @param rightEdge   WallEdge describing the logical right-edge of the wall section.
 *
 * @see wallHeight()
 * @see wallDimensions()
 */
static inline ddouble wallWidth(WallEdge const &leftEdge, WallEdge const &rightEdge)
{
    return de::abs((rightEdge.origin() - leftEdge.origin()).length());
}

/**
 * Returns the "wall dimensions" (i.e., distance in map units) of the wall described
 * by @a leftEdge and @a rightEdge.
 *
 * @param leftEdge    WallEdge describing the logical left-edge of the wall section.
 * @param rightEdge   WallEdge describing the logical right-edge of the wall section.
 *
 * @see wallWidth()
 * @see wallHeight()
 */
#if 0
static Vec2d wallDimensions(WallEdge const &leftEdge, WallEdge const &rightEdge)
{
    return Vec2d(wallWidth(leftEdge, rightEdge), wallHeight(leftEdge, rightEdge));
}
#endif

/**
 * Returns the "wall offset" (i.e., distance in map units from the LineSide's vertex) of
 * the wall described by @a leftEdge and @a rightEdge.
 *
 * @param leftEdge    WallEdge describing the logical left-edge of the wall section.
 * @param rightEdge   WallEdge describing the logical right-edge of the wall section.
 */
static ddouble wallOffset(WallEdge const &leftEdge, WallEdge const &/*rightEdge*/)
{
    return leftEdge.lineSideOffset();
}

/**
 * Return the "wall side-openness" (factor) of specified side of the wall described by
 * @a leftEdge and @a rightEdge.
 *
 * @param leftEdge    WallEdge describing the logical left-edge of the wall section.
 * @param rightEdge   WallEdge describing the logical right-edge of the wall section.
 * @param rightSide   Use the right edge if @c true; otherwise the left edge.
 */
static dfloat wallSideOpenness(WallEdge const &leftEdge, WallEdge const &/*rightEdge*/, bool rightSide)
{
    return leftEdge.lineSide().radioCornerSide(rightSide).corner;
}

/**
 * Returns @c true if the wall described by @a leftEdge and @a rightEdge should receive
 * the specified @a shadow.
 *
 * @param leftEdge    WallEdge describing the logical left-edge of the wall section.
 * @param rightEdge   WallEdge describing the logical right-edge of the wall section.
 * @param shadow      WallShadow identifier.
 * @param shadowSize  Shadow size in map units.
 */
static bool wallReceivesShadow(WallEdge const &leftEdge, WallEdge const &rightEdge,
    WallShadow shadow, dfloat shadowSize)
{
    if(shadowSize <= 0) return false;

    LineSide const &side = leftEdge.lineSide();
    DENG2_ASSERT(side.leftHEdge());

    auto const &subsec      = side.leftHEdge()->face().mapElementAs<ConvexSubspace>().subsector().as<world::ClientSubsector>();
    Plane const &visFloor   = subsec.visFloor  ();
    Plane const &visCeiling = subsec.visCeiling();

    switch(shadow)
    {
    case TopShadow:
        return visCeiling.castsShadow()
               && rightEdge.top   ().z() > visCeiling.heightSmoothed() - shadowSize
               && leftEdge .bottom().z() < visCeiling.heightSmoothed();

    case BottomShadow:
        return visFloor.castsShadow()
               && leftEdge .bottom().z() < visFloor  .heightSmoothed() + shadowSize
               && rightEdge.top   ().z() > visFloor  .heightSmoothed();

    case LeftShadow:
        return (visFloor.castsShadow() || visCeiling.castsShadow())
               && wallSideOpenness(leftEdge, rightEdge, false/*left side*/) > 0
               && leftEdge.lineSideOffset() < shadowSize;

    case RightShadow:
        return (visFloor.castsShadow() || visCeiling.castsShadow())
               && wallSideOpenness(leftEdge, rightEdge, true/*right side*/) > 0
               && leftEdge.lineSideOffset() + wallWidth(leftEdge, rightEdge) > side.line().length() - shadowSize;
    }
    DENG2_ASSERT_FAIL("Unknown WallShadow");
    return false;
}

/**
 * Determine the horizontal offset for a FakeRadio wall, shadow geometry.
 *
 * @param lineLength  If negative; implies that the texture is flipped horizontally.
 * @param segOffset   Offset to the start of the segment.
 */
static inline dfloat calcTexCoordX(dfloat lineLength, dfloat segOffset)
{
    return (lineLength > 0 ? segOffset : lineLength + segOffset);
}

/**
 * Determine the vertical offset for a FakeRadio wall, shadow geometry.
 *
 * @param z          Z height of the vertex.
 * @param bottom     Z height of the bottom of the wall section.
 * @param top        Z height of the top of the wall section.
 * @param texHeight  If negative; implies that the texture is flipped vertically.
 */
static inline dfloat calcTexCoordY(dfloat z, dfloat bottom, dfloat top, dfloat texHeight)
{
    return (texHeight > 0 ? top - z : bottom - z);
}

struct ProjectedShadowData
{
    lightingtexid_t texture;
    Vec2f texOrigin;
    Vec2f texDimensions;
    Vec2f texCoords[4];  ///< { bl, tl, br, tr }
};

static void setTopShadowParams(WallEdge const &leftEdge, WallEdge const &rightEdge, ddouble shadowSize,
    ProjectedShadowData &projected)
{
    LineSide /*const*/ &side = leftEdge.lineSide();
    DENG2_ASSERT(side.leftHEdge());
    auto const &space       = side.leftHEdge()->face().mapElementAs<ConvexSubspace>();
    auto const &subsec      = space.subsector().as<world::ClientSubsector>();
    Plane const &visFloor   = subsec.visFloor  ();
    Plane const &visCeiling = subsec.visCeiling();

    projected = {};
    projected.texDimensions = Vec2f(0, shadowSize);
    projected.texOrigin     = Vec2f(0, calcTexCoordY(leftEdge.top().z(), subsec.visFloor().heightSmoothed()
                                                        , subsec.visCeiling().heightSmoothed(), shadowSize));
    projected.texture       = LST_RADIO_OO;

    edgespan_t const &edgeSpan = side.radioEdgeSpan(true/*top*/);

    // One or both neighbors without a back sector?
    if(side.radioCornerSide(0/*left*/ ).corner == -1 ||
       side.radioCornerSide(1/*right*/).corner == -1)
    {
        // At least one corner faces outwards.
        projected.texture         = LST_RADIO_OO;
        projected.texDimensions.x = edgeSpan.length;
        projected.texOrigin.x     = calcTexCoordX(edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));

        // Both corners face outwards?
        if(   (side.radioCornerSide(0/*left*/).corner == -1 && side.radioCornerSide(1/*right*/).corner == -1)
           || (side.radioCornerTop (0/*left*/).corner == -1 && side.radioCornerTop (1/*right*/).corner == -1))
        {
            projected.texture = LST_RADIO_OO;//CC;
        }
        // Right corner faces outwards?
        else if(side.radioCornerSide(1/*right*/).corner == -1)
        {
            if(-side.radioCornerTop(0/*left*/).pOffset < 0 && side.radioCornerBottom(0/*left*/).pHeight < visCeiling.heightSmoothed())
            {
                projected.texture         = LST_RADIO_OE;
                // Must flip horizontally.
                projected.texDimensions.x = -edgeSpan.length;
                projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));
            }
        }
        else  // Left corner faces outwards.
        {
            if(-side.radioCornerTop(1/*right*/).pOffset < 0 && side.radioCornerBottom(1/*right*/).pHeight < visCeiling.heightSmoothed())
            {
                projected.texture = LST_RADIO_OE;
            }
        }
    }
    else
    {
        // Corners WITH a neighbor back sector
        projected.texDimensions.x = edgeSpan.length;
        projected.texOrigin.x     = calcTexCoordX(edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));

        // Both corners face outwards?
        if(side.radioCornerTop(0/*left*/).corner == -1 && side.radioCornerTop(1/*right*/).corner == -1)
        {
            projected.texture = LST_RADIO_OO;//CC;
        }
        // Right corner faces outwards?
        else if(side.radioCornerTop(1/*right*/).corner == -1 && side.radioCornerTop(0/*left*/).corner > MIN_OPEN)
        {
            projected.texture = LST_RADIO_OO;
        }
        // Left corner faces outwards?
        else if(side.radioCornerTop(0/*left*/).corner == -1 && side.radioCornerTop(1/*right*/).corner > MIN_OPEN)
        {
            projected.texture = LST_RADIO_OO;
        }
        // Both edges open?
        else if(side.radioCornerTop(0/*left*/).corner <= MIN_OPEN && side.radioCornerTop(1/*right*/).corner <= MIN_OPEN)
        {
            projected.texture = LST_RADIO_OO;
            if(side.radioCornerTop(0/*left*/).proximity && side.radioCornerTop(1/*right*/).proximity)
            {
                if(-side.radioCornerTop(0/*left*/).pOffset >= 0 && -side.radioCornerTop(1/*right*/).pOffset < 0)
                {
                    projected.texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(shadowSize > -side.radioCornerTop(0/*left*/).pOffset)
                    {
                        if(-side.radioCornerTop(0/*left*/).pOffset < INDIFF)
                        {
                            projected.texture = LST_RADIO_OE;
                        }
                        else
                        {
                            projected.texDimensions.y = -side.radioCornerTop(0/*left*/).pOffset;
                            projected.texOrigin.y     = calcTexCoordY(leftEdge.top().z(), visFloor.heightSmoothed(), visCeiling.heightSmoothed(), projected.texDimensions.y);
                        }
                    }
                }
                else if(-side.radioCornerTop(0/*left*/).pOffset < 0 && -side.radioCornerTop(1/*right*/).pOffset >= 0)
                {
                    projected.texture         = LST_RADIO_CO;
                    // Must flip horizontally.
                    projected.texDimensions.x = -edgeSpan.length;
                    projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));

                    // The shadow can't go over the higher edge.
                    if(shadowSize > -side.radioCornerTop(1/*right*/).pOffset)
                    {
                        if(-side.radioCornerTop(1/*right*/).pOffset < INDIFF)
                        {
                            projected.texture = LST_RADIO_OE;
                        }
                        else
                        {
                            projected.texDimensions.y = -side.radioCornerTop(1/*right*/).pOffset;
                            projected.texOrigin.y     = calcTexCoordY(leftEdge.top().z(), visFloor.heightSmoothed(), visCeiling.heightSmoothed(), projected.texDimensions.y);
                        }
                    }
                }
            }
            else
            {
                if(-side.radioCornerTop(0/*left*/).pOffset < -MINDIFF)
                {
                    projected.texture         = LST_RADIO_OE;
                    // Must flip horizontally.
                    projected.texDimensions.x = -edgeSpan.length;
                    projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));
                }
                else if(-side.radioCornerTop(1/*right*/).pOffset < -MINDIFF)
                {
                    projected.texture = LST_RADIO_OE;
                }
            }
        }
        else if(side.radioCornerTop(0/*left*/).corner <= MIN_OPEN)
        {
            if(-side.radioCornerTop(0/*left*/).pOffset < 0)
                projected.texture = LST_RADIO_CO;
            else
                projected.texture = LST_RADIO_OO;

            // Must flip horizontally.
            projected.texDimensions.x = -edgeSpan.length;
            projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));
        }
        else if(side.radioCornerTop(1/*right*/).corner <= MIN_OPEN)
        {
            if(-side.radioCornerTop(1/*right*/).pOffset < 0)
                projected.texture = LST_RADIO_CO;
            else
                projected.texture = LST_RADIO_OO;
        }
        else  // C/C ???
        {
            projected.texture = LST_RADIO_OO;
        }
    }
}

static void setBottomShadowParams(WallEdge const &leftEdge, WallEdge const &rightEdge, ddouble shadowSize,
    ProjectedShadowData &projected)
{
    LineSide /*const*/ &side = leftEdge.lineSide();
    DENG2_ASSERT(side.leftHEdge());
    auto const &subsec      = side.leftHEdge()->face().mapElementAs<ConvexSubspace>().subsector().as<world::ClientSubsector>();
    Plane const &visFloor   = subsec.visFloor  ();
    Plane const &visCeiling = subsec.visCeiling();

    projected = {};
    projected.texDimensions.y = -shadowSize;
    projected.texOrigin.y     = calcTexCoordY(leftEdge.top().z(), visFloor.heightSmoothed(), visCeiling.heightSmoothed(), -shadowSize);
    projected.texture         = LST_RADIO_OO;

    edgespan_t const &edgeSpan = side.radioEdgeSpan(false/*bottom*/);

    // Corners without a neighbor back sector?
    if(side.radioCornerSide(0/*left*/).corner == -1 || side.radioCornerSide(1/*right*/).corner == -1)
    {
        // At least one corner faces outwards.
        projected.texture         = LST_RADIO_OO;
        projected.texDimensions.x = edgeSpan.length;
        projected.texOrigin.x     = calcTexCoordX(edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));

        // Both corners face outwards?
        if(   (side.radioCornerSide  (0/*left*/).corner == -1 && side.radioCornerSide  (1/*right*/).corner == -1)
           || (side.radioCornerBottom(0/*left*/).corner == -1 && side.radioCornerBottom(1/*right*/).corner == -1))
        {
            projected.texture = LST_RADIO_OO;//CC;
        }
        // Right corner faces outwards?
        else if(side.radioCornerSide(1/*right*/).corner == -1)
        {
            if(side.radioCornerBottom(0/*left*/).pOffset < 0 && side.radioCornerTop(0/*left*/).pHeight > visFloor.heightSmoothed())
            {
                projected.texture         = LST_RADIO_OE;
                // Must flip horizontally.
                projected.texDimensions.x = -edgeSpan.length;
                projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));
            }
        }
        else  // Left corner faces outwards.
        {
            if(side.radioCornerBottom(1/*right*/).pOffset < 0 && side.radioCornerTop(1/*right*/).pHeight > visFloor.heightSmoothed())
            {
                projected.texture = LST_RADIO_OE;
            }
        }
    }
    else  // Corners WITH a neighbor back sector.
    {
        projected.texDimensions.x = edgeSpan.length;
        projected.texOrigin.x     = calcTexCoordX(edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));

        // Both corners face outwards?
        if(side.radioCornerBottom(0/*left*/).corner == -1 && side.radioCornerBottom(1/*right*/).corner == -1)
        {
            projected.texture = LST_RADIO_OO;//CC;
        }
        // Right corner faces outwards?
        else if(side.radioCornerBottom(1/*right*/).corner == -1 && side.radioCornerBottom(0/*right*/).corner > MIN_OPEN)
        {
            projected.texture = LST_RADIO_OO;
        }
        // Left corner faces outwards?
        else if(side.radioCornerBottom(0/*left*/).corner == -1 && side.radioCornerBottom(1/*right*/).corner > MIN_OPEN)
        {
            projected.texture = LST_RADIO_OO;
        }
        // Both edges open?
        else if(side.radioCornerBottom(0/*left*/).corner <= MIN_OPEN && side.radioCornerBottom(1/*right*/).corner <= MIN_OPEN)
        {
            projected.texture = LST_RADIO_OO;

            if(side.radioCornerBottom(0/*left*/).proximity && side.radioCornerBottom(1/*right*/).proximity)
            {
                if(side.radioCornerBottom(0/*left*/).pOffset >= 0 && side.radioCornerBottom(1/*right*/).pOffset < 0)
                {
                    projected.texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(shadowSize > side.radioCornerBottom(0/*left*/).pOffset)
                    {
                        if(side.radioCornerBottom(0/*left*/).pOffset < INDIFF)
                        {
                            projected.texture = LST_RADIO_OE;
                        }
                        else
                        {
                            projected.texDimensions.y = -side.radioCornerBottom(0/*left*/).pOffset;
                            projected.texOrigin.y     = calcTexCoordY(leftEdge.top().z(), visFloor.heightSmoothed(), visCeiling.heightSmoothed(), projected.texDimensions.y);
                        }
                    }
                }
                else if(side.radioCornerBottom(0/*left*/).pOffset < 0 && side.radioCornerBottom(1/*right*/).pOffset >= 0)
                {
                    projected.texture         = LST_RADIO_CO;
                    // Must flip horizontally.
                    projected.texDimensions.x = -edgeSpan.length;
                    projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));

                    if(shadowSize > side.radioCornerBottom(1/*right*/).pOffset)
                    {
                        if(side.radioCornerBottom(1/*right*/).pOffset < INDIFF)
                        {
                            projected.texture = LST_RADIO_OE;
                        }
                        else
                        {
                            projected.texDimensions.y = -side.radioCornerBottom(1/*right*/).pOffset;
                            projected.texOrigin.y     = calcTexCoordY(leftEdge.top().z(), visFloor.heightSmoothed(), visCeiling.heightSmoothed(), projected.texDimensions.y);
                        }
                    }
                }
            }
            else
            {
                if(side.radioCornerBottom(0/*left*/).pOffset < -MINDIFF)
                {
                    projected.texture         = LST_RADIO_OE;
                    // Must flip horizontally.
                    projected.texDimensions.x = -edgeSpan.length;
                    projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));
                }
                else if(side.radioCornerBottom(1/*right*/).pOffset < -MINDIFF)
                {
                    projected.texture = LST_RADIO_OE;
                }
            }
        }
        // Right corner closed?
        else if(side.radioCornerBottom(0/*left*/).corner <= MIN_OPEN)
        {
            if(side.radioCornerBottom(0/*left*/).pOffset < 0)
                projected.texture = LST_RADIO_CO;
            else
                projected.texture = LST_RADIO_OO;

            // Must flip horizontally.
            projected.texDimensions.x = -edgeSpan.length;
            projected.texOrigin.x     = calcTexCoordX(-edgeSpan.length, edgeSpan.shift + wallOffset(leftEdge, rightEdge));
        }
        // Left Corner closed?
        else if(side.radioCornerBottom(1/*right*/).corner <= MIN_OPEN)
        {
            if(side.radioCornerBottom(1/*right*/).pOffset < 0)
                projected.texture = LST_RADIO_CO;
            else
                projected.texture = LST_RADIO_OO;
        }
        else  // C/C ???
        {
            projected.texture = LST_RADIO_OO;
        }
    }
}

static void setSideShadowParams(WallEdge const &leftEdge, WallEdge const &rightEdge, bool rightSide,
    ddouble shadowSize, ProjectedShadowData &projected)
{
    LineSide /*const*/ &side = leftEdge.lineSide();
    HEdge const *hedge = side.leftHEdge();
    DENG2_ASSERT(hedge);
    auto const &subsec      = hedge->face().mapElementAs<ConvexSubspace>().subsector().as<world::ClientSubsector>();
    Plane const &visFloor   = subsec.visFloor  ();
    Plane const &visCeiling = subsec.visCeiling();
    DENG2_ASSERT(visFloor.castsShadow() || visCeiling.castsShadow());  // sanity check.

    projected = {};
    projected.texOrigin     = Vec2f(0, leftEdge.bottom().z() - visFloor.heightSmoothed());
    projected.texDimensions = Vec2f(0, visCeiling.heightSmoothed() - visFloor.heightSmoothed());

    if(rightSide)
    {
        // Right shadow.
        projected.texOrigin.x = -side.line().length() + wallOffset(leftEdge, rightEdge);
        // Make sure the shadow isn't too big
        if(shadowSize > side.line().length())
        {
            projected.texDimensions.x = -side.line().length();
            if(side.radioCornerSide(0/*left*/).corner > MIN_OPEN)
                projected.texDimensions.x /= 2;
        }
        else
        {
            projected.texDimensions.x = -shadowSize;
        }
    }
    else
    {
        // Left shadow.
        projected.texOrigin.x = wallOffset(leftEdge, rightEdge);
        // Make sure the shadow isn't too big
        if(shadowSize > side.line().length())
        {
            projected.texDimensions.x = side.line().length();
            if(side.radioCornerSide(1/*right*/).corner > MIN_OPEN)
                projected.texDimensions.x /= 2;
        }
        else
        {
            projected.texDimensions.x = shadowSize;
        }
    }

    if(!hedge->twin().hasFace() || leftEdge.spec().section == LineSide::Middle)
    {
        if(!visFloor.castsShadow())
        {
            projected.texDimensions.y = -(visCeiling.heightSmoothed() - visFloor.heightSmoothed());
            projected.texOrigin.y     = calcTexCoordY(leftEdge.top().z(), visFloor.heightSmoothed(), visCeiling.heightSmoothed(), projected.texDimensions.y);
            projected.texture         = LST_RADIO_CO;
        }
        else if(!visCeiling.castsShadow())
        {
            projected.texture = LST_RADIO_CO;
        }
        else
        {
            projected.texture = LST_RADIO_CC;
        }
    }
    else
    {
        auto const &bSpace = hedge->twin().face().mapElementAs<ConvexSubspace>();
        if (bSpace.hasSubsector())
        {
            auto const &bSubsec  = bSpace.subsector().as<world::ClientSubsector>();
            ddouble const bFloor = bSubsec.visFloor  ().heightSmoothed();
            ddouble const bCeil  = bSubsec.visCeiling().heightSmoothed();
            if (bFloor > visFloor.heightSmoothed() && bCeil < visCeiling.heightSmoothed())
            {
                if (visFloor.castsShadow() && visCeiling.castsShadow())
                {
                    projected.texture = LST_RADIO_CC;
                }
                else if (!visFloor.castsShadow())
                {
                    projected.texOrigin.y     = leftEdge.bottom().z() - visCeiling.heightSmoothed();
                    projected.texDimensions.y = -(visCeiling.heightSmoothed() - visFloor.heightSmoothed());
                    projected.texture         = LST_RADIO_CO;
                }
                else
                {
                    projected.texture = LST_RADIO_CO;
                }
            }
            else if (bFloor > visFloor.heightSmoothed())
            {
                if (visFloor.castsShadow() && visCeiling.castsShadow())
                {
                    projected.texture = LST_RADIO_CC;
                }
                else if (!visFloor.castsShadow())
                {
                    projected.texOrigin.y     = leftEdge.bottom().z() - visCeiling.heightSmoothed();
                    projected.texDimensions.y = -(visCeiling.heightSmoothed() - visFloor.heightSmoothed());
                    projected.texture         = LST_RADIO_CO;
                }
                else
                {
                    projected.texture = LST_RADIO_CO;
                }
            }
            else if (bCeil < visCeiling.heightSmoothed())
            {
                if (visFloor.castsShadow() && visCeiling.castsShadow())
                {
                    projected.texture = LST_RADIO_CC;
                }
                else if (!visFloor.castsShadow())
                {
                    projected.texOrigin.y     = leftEdge.bottom().z() - visCeiling.heightSmoothed();
                    projected.texDimensions.y = -(visCeiling.heightSmoothed() - visFloor.heightSmoothed());
                    projected.texture         = LST_RADIO_CO;
                }
                else
                {
                    projected.texture = LST_RADIO_CO;
                }
            }
            }
    }
}

static void quadTexCoords(Vec2f *tc, WallEdge const &leftEdge, WallEdge const &rightEdge,
    Vec2f const &texOrigin, Vec2f const &texDimensions, bool horizontal)
{
    DENG2_ASSERT(tc);
    if(horizontal)
    {
        tc[0] = (texOrigin / texDimensions).yx();
        tc[2] = tc[0] + Vec2f(0                              , wallWidth(leftEdge, rightEdge)) / texDimensions.yx();
        tc[3] = tc[0] + Vec2f(wallHeight(leftEdge, rightEdge), wallWidth(leftEdge, rightEdge)) / texDimensions.yx();
        tc[1] = Vec2f(tc[3].x, tc[0].y);
    }
    else  // Vertical.
    {
        tc[1] = texOrigin / texDimensions;
        tc[0] = tc[1] + Vec2f(0                             , wallHeight(leftEdge, rightEdge)) / texDimensions;
        tc[2] = tc[1] + Vec2f(wallWidth(leftEdge, rightEdge), wallHeight(leftEdge, rightEdge)) / texDimensions;
        tc[3] = Vec2f(tc[2].x, tc[1].y);
    }
}

static bool projectWallShadow(WallEdge const &leftEdge, WallEdge const &rightEdge,
    WallShadow shadow, ddouble shadowSize, ProjectedShadowData &projected)
{
    if(!wallReceivesShadow(leftEdge, rightEdge, shadow, shadowSize))
        return false;

    switch(shadow)
    {
    case TopShadow:
        setTopShadowParams(leftEdge, rightEdge, shadowSize, projected);
        quadTexCoords(projected.texCoords, leftEdge, rightEdge,
                      projected.texOrigin, projected.texDimensions, false/*vertical*/);
        return true;

    case BottomShadow:
        setBottomShadowParams(leftEdge, rightEdge, shadowSize, projected);
        quadTexCoords(projected.texCoords, leftEdge, rightEdge,
                      projected.texOrigin, projected.texDimensions, false/*vertical*/);
        return true;

    case LeftShadow:
        setSideShadowParams(leftEdge, rightEdge, false/*left side*/, shadowSize, projected);
        quadTexCoords(projected.texCoords, leftEdge, rightEdge,
                      projected.texOrigin, projected.texDimensions, true/*horizontal*/);
        return true;

    case RightShadow:
        setSideShadowParams(leftEdge, rightEdge, true/*right side*/, shadowSize, projected);
        quadTexCoords(projected.texCoords, leftEdge, rightEdge,
                      projected.texOrigin, projected.texDimensions, true/*horizontal*/);
        return true;
    }
    DENG2_ASSERT_FAIL("Unknown WallShadow");
    return false;
}

static void drawWallShadow(Vec3f const *posCoords, WallEdge const &leftEdge, WallEdge const &rightEdge,
    dfloat shadowDark, ProjectedShadowData const &tp)
{
    DENG2_ASSERT(posCoords);

    // Uniform color - shadows are black.
    Vec4ub const shadowColor(0, 0, 0, 255 * de::clamp(0.f, shadowDark, 1.0f));

    DrawListSpec listSpec;
    listSpec.group = ShadowGeom;
    listSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(tp.texture), gl::ClampToEdge, gl::ClampToEdge);
    DrawList &shadowList = ClientApp::renderSystem().drawLists().find(listSpec);

    static DrawList::Indices indices;
    if (indices.size() < 64) indices.resize(64);

    // Walls with edge divisions mean two trifans.
    if(leftEdge.divisionCount() || rightEdge.divisionCount())
    {
        Store &buffer = ClientApp::renderSystem().buffer();
        // Right fan.
        {
            duint const numVerts = 3 + rightEdge.divisionCount();
            duint const base     = buffer.allocateVertices(numVerts);
            if (indices.size() < int(numVerts)) indices.resize(numVerts);
            for(duint i = 0; i < numVerts; ++i)
            {
                indices[i] = base + i;
            }

            //
            // Build geometry.
            //
            buffer.posCoords   [indices[0           ]] = posCoords[0];
            buffer.colorCoords [indices[0           ]] = shadowColor;
            buffer.texCoords[0][indices[0           ]] = tp.texCoords[0];

            buffer.posCoords   [indices[1           ]] = posCoords[3];
            buffer.colorCoords [indices[1           ]] = shadowColor;
            buffer.texCoords[0][indices[1           ]] = tp.texCoords[3];

            buffer.posCoords   [indices[numVerts - 1]] = posCoords[2];
            buffer.colorCoords [indices[numVerts - 1]] = shadowColor;
            buffer.texCoords[0][indices[numVerts - 1]] = tp.texCoords[2];

            for(dint i = 0; i < rightEdge.divisionCount(); ++i)
            {
                WorldEdge::Event const &icpt = rightEdge.at(rightEdge.lastDivision() - i);

                buffer.posCoords   [indices[2 + i]] = icpt.origin();
                buffer.colorCoords [indices[2 + i]] = shadowColor;
                buffer.texCoords[0][indices[2 + i]] = Vec2f(tp.texCoords[3].x, tp.texCoords[2].y + (tp.texCoords[3].y - tp.texCoords[2].y) * icpt.distance());
            }

            // Write the geometry?
            if(::rendFakeRadio != 2)
            {
                shadowList.write(buffer, indices.constData(), numVerts, gl::TriangleFan);
            }
        }
        // Left fan.
        {
            duint const numVerts = 3 + leftEdge .divisionCount();
            duint const base     = buffer.allocateVertices(numVerts);
            if (indices.size() < int(numVerts)) indices.resize(numVerts);
            for(duint i = 0; i < numVerts; ++i)
            {
                indices[i] = base + i;
            }

            //
            // Build geometry.
            //
            buffer.posCoords   [indices[0           ]] = posCoords[3];
            buffer.colorCoords [indices[0           ]] = shadowColor;
            buffer.texCoords[0][indices[0           ]] = tp.texCoords[3];

            buffer.posCoords   [indices[1           ]] = posCoords[0];
            buffer.colorCoords [indices[1           ]] = shadowColor;
            buffer.texCoords[0][indices[1           ]] = tp.texCoords[0];

            buffer.posCoords   [indices[numVerts - 1]] = posCoords[1];
            buffer.colorCoords [indices[numVerts - 1]] = shadowColor;
            buffer.texCoords[0][indices[numVerts - 1]] = tp.texCoords[1];

            for(dint i = 0; i < leftEdge.divisionCount(); ++i)
            {
                WorldEdge::Event const &icpt = leftEdge.at(leftEdge.firstDivision() + i);

                buffer.posCoords   [indices[2 + i]] = icpt.origin();
                buffer.colorCoords [indices[2 + i]] = shadowColor;
                buffer.texCoords[0][indices[2 + i]] = Vec2f(tp.texCoords[0].x, tp.texCoords[0].y + (tp.texCoords[1].y - tp.texCoords[0].y) * icpt.distance());
            }

            // Write the geometry?
            if(::rendFakeRadio != 2)
            {
                shadowList.write(buffer, indices.constData(), numVerts, gl::TriangleFan);
            }
        }
    }
    else
    {
        Store &buffer = ClientApp::renderSystem().buffer();
        duint base = buffer.allocateVertices(4);
        for(duint i = 0; i < 4; ++i)
        {
            indices[i] = base + i;
        }

        //
        // Build geometry.
        //
        for(duint i = 0; i < 4; ++i)
        {
            buffer.posCoords   [indices[i]] = posCoords[i];
            buffer.colorCoords [indices[i]] = shadowColor;
            buffer.texCoords[0][indices[i]] = tp.texCoords[i];
        }

        // Write the geometry?
        if(::rendFakeRadio != 2)
        {
            shadowList.write(buffer, indices.constData(), 4, gl::TriangleStrip);
        }
    }
}

void Rend_DrawWallRadio(WallEdge const &leftEdge, WallEdge const &rightEdge, dfloat ambientLight)
{
    // Disabled?
    if(!::rendFakeRadio || ::levelFullBright || leftEdge.spec().flags.testFlag(WallSpec::NoFakeRadio))
        return;

    // Skip if the surface is not lit with ambient light.
    dfloat const shadowDark = calcShadowDarkness(ambientLight);
    if(shadowDark < MIN_SHADOW_DARKNESS)
        return;

    // Skip if the determined shadow size is too small.
    dfloat const shadowSize = calcShadowSize(ambientLight);
    if(shadowSize < MIN_SHADOW_SIZE)
        return;

    // Ensure we have up-to-date information for generating shadow geometry.
    leftEdge.lineSide().updateRadioForFrame(R_FrameCount());

    Vec3f const posCoords[] = {
        leftEdge .bottom().origin(),
        leftEdge .top   ().origin(),
        rightEdge.bottom().origin(),
        rightEdge.top   ().origin()
    };

    ProjectedShadowData projected;

    if(projectWallShadow(leftEdge, rightEdge, TopShadow, shadowSize, projected))
    {
        drawWallShadow(posCoords, leftEdge, rightEdge, shadowDark,
                       projected);
    }

    if(projectWallShadow(leftEdge, rightEdge, BottomShadow, shadowSize, projected))
    {
        drawWallShadow(posCoords, leftEdge, rightEdge, shadowDark,
                       projected);
    }

    if(projectWallShadow(leftEdge, rightEdge, LeftShadow, shadowSize, projected))
    {
        drawWallShadow(posCoords, leftEdge, rightEdge,
                       shadowDark * de::cubed(wallSideOpenness(leftEdge, rightEdge, false/*left edge*/) * .8f),
                       projected);
    }

    if(projectWallShadow(leftEdge, rightEdge, RightShadow, shadowSize, projected))
    {
        drawWallShadow(posCoords, leftEdge, rightEdge,
                       shadowDark * de::cubed(wallSideOpenness(leftEdge, rightEdge, true/*right edge*/) * .8f),
                       projected);
    }
}

/**
 * Determines whether FakeRadio flat, shadow geometry should be drawn between the vertices of
 * the given half-edges @a hEdges and prepares the ShadowEdges @a edges accordingly.
 *
 * @param edges             ShadowEdge descriptors for both edges { left, right }.
 * @param hEdges            Half-edge accessors for both edges { left, right }.
 * @param sectorPlaneIndex  Logical index of the sector plane to consider a shadow for.
 * @param shadowDark        Shadow darkness factor.
 *
 * @return  @c true if one or both edges are partially in shadow.
 */
static bool prepareFlatShadowEdges(ShadowEdge edges[2], HEdge const *hEdges[2], dint sectorPlaneIndex,
    dfloat shadowDark)
{
    DENG2_ASSERT(edges && hEdges && hEdges[0] && hEdges[1]);

    // If the sector containing the shadowing line section is fully closed (i.e., volume is
    // not positive) then skip shadow drawing entirely.
    /// @todo Encapsulate this logic in ShadowEdge -ds
    if(!hEdges[0]->hasFace() || !hEdges[0]->face().hasMapElement())
        return false;

    if(!hEdges[0]->face().mapElementAs<ConvexSubspace>().subsector().as<world::ClientSubsector>().hasWorldVolume())
        return false;

    for(dint i = 0; i < 2; ++i)
    {
        edges[i].init(*hEdges[i], i);
        edges[i].prepare(sectorPlaneIndex);
    }
    return (edges[0].shadowStrength(shadowDark) >= .0001 && edges[1].shadowStrength(shadowDark) >= .0001);
}

static uint makeFlatShadowGeometry(DrawList::Indices &indices, Store &verts, gl::Primitive &primitive,
    ShadowEdge const edges[2], dfloat shadowDark, bool haveFloor)
{
    static duint const floorOrder[][4] = { { 0, 1, 2, 3 }, { 1, 2, 3, 0 } };
    static duint const ceilOrder [][4] = { { 0, 3, 2, 1 }, { 1, 0, 3, 2 } };

    static Vec4ub const white(255, 255, 255, 0);
    static Vec4ub const black(  0,   0,   0, 0);

    // What vertex winding order (0 = left, 1 = right)? (For best results, the cross edge
    // should always be the shortest.)
    duint const winding = (edges[1].length() > edges[0].length()? 1 : 0);
    duint const *order  = (haveFloor ? floorOrder[winding] : ceilOrder[winding]);

    // Assign indices.
    duint base = verts.allocateVertices(4);
    DrawList::reserveSpace(indices, 4);
    for(duint i = 0; i < 4; ++i)
    {
        indices[i] = base + i;
    }

    //
    // Build the geometry.
    //
    primitive = gl::TriangleFan;
    verts.posCoords[indices[order[0]]] = edges[0].outer();
    verts.posCoords[indices[order[1]]] = edges[1].outer();
    verts.posCoords[indices[order[2]]] = edges[1].inner();
    verts.posCoords[indices[order[3]]] = edges[0].inner();
    // Set uniform color.
#if defined (DENG_OPENGL)
    Vec4ub const &uniformColor = (::renderWireframe? white : black);  // White to assist visual debugging.
#else
    Vec4ub const &uniformColor = black;
#endif
    for(duint i = 0; i < 4; ++i)
    {
        verts.colorCoords[indices[i]] = uniformColor;
    }
    // Set outer edge opacity:
    for(duint i = 0; i < 2; ++i)
    {
        verts.colorCoords[indices[order[i]]].w = dbyte( edges[i].shadowStrength(shadowDark) * 255 );
    }

    return 4;
}

void Rend_DrawFlatRadio(ConvexSubspace const &subspace)
{
    // Disabled?
    if(!::rendFakeRadio || ::levelFullBright) return;

    // If no shadow-casting lines are linked we no work to do.
    if(!subspace.shadowLineCount()) return;

    auto const &subsec = subspace.subsector().as<world::ClientSubsector>();

    // Determine the shadow properties.
    dfloat const shadowDark = calcShadowDarkness(subsec.lightSourceIntensity());
    if(shadowDark < MIN_SHADOW_DARKNESS)
        return;

    static DrawList::Indices indices;
    static ShadowEdge shadowEdges[2/*left, right*/];  // Keep these around (needed often).

    // Can skip drawing for Planes that do not face the viewer - find the 2D vector to subspace center.
    auto const eyeToSubspace = Vec2f(Rend_EyeOrigin().xz() - subspace.poly().center());

    // All shadow geometry uses the same texture (i.e., none) - use the same list.
    DrawList &shadowList = ClientApp::renderSystem().drawLists().find(
#if defined (DENG_OPENGL)
        DrawListSpec(renderWireframe? UnlitGeom : ShadowGeom)
#else
        DrawListSpec(ShadowGeom)
#endif
    );

    // Process all LineSides linked to this subspace as potential shadow casters.
    subspace.forAllShadowLines([&subsec, &shadowDark, &eyeToSubspace, &shadowList] (LineSide &side)
    {
        DENG2_ASSERT(side.hasSections() && !side.line().definesPolyobj() && side.leftHEdge());

        // Process each only once per frame (we only want to draw a shadow set once).
        if(side.shadowVisCount() != R_FrameCount())
        {
            side.setShadowVisCount(R_FrameCount());  // Mark processed.

            for (dint pln = 0; pln < subsec.visPlaneCount(); ++pln)
            {
                Plane const &plane = subsec.visPlane(pln);

                // Skip Planes which should not receive FakeRadio shadowing.
                if (!plane.receivesShadow()) continue;

                // Skip Planes facing away from the viewer.
                if (Vec3f(eyeToSubspace, Rend_EyeOrigin().y - plane.heightSmoothed())
                         .dot(plane.surface().normal()) >= 0)
                {
                    HEdge const *hEdges[2/*left, right*/] = { side.leftHEdge(), side.leftHEdge() };

                    if (prepareFlatShadowEdges(shadowEdges, hEdges, pln, shadowDark))
                    {
                        bool const haveFloor = plane.surface().normal()[2] > 0;

                        // Build geometry.
                        Store &buffer = ClientApp::renderSystem().buffer();
                        gl::Primitive primitive;
                        uint vertCount = makeFlatShadowGeometry(indices, buffer, primitive, shadowEdges, shadowDark, haveFloor);

                        // Skip drawing entirely?
                        if (::rendFakeRadio == 2) continue;

                        // Write the geometry.
                        shadowList.write(buffer, indices.constData(), vertCount, primitive);
                    }
                }
            }
        }
        return LoopContinue;
    });
}

void Rend_RadioRegister()
{
    C_VAR_INT  ("rend-fakeradio",               &::rendFakeRadio,       0, 0, 2);
    C_VAR_FLOAT("rend-fakeradio-darkness",      &::fakeRadioDarkness,   0, 0, 2);

    C_VAR_BYTE ("rend-dev-fakeradio-update",    &::devFakeRadioUpdate,  CVF_NO_ARCHIVE, 0, 1);
}
