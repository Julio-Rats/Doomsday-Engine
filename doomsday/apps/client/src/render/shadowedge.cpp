/** @file shadowedge.cpp  FakeRadio Shadow Edge Geometry
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

#include "render/shadowedge.h"

#include "Face"
#include "HEdge"

#include "ConvexSubspace"
#include "Plane"
#include "Sector"
#include "Surface"
#include "world/lineowner.h"
#include "client/clientsubsector.h"

#include "render/rend_main.h"
#include "MaterialAnimator"
#include "WallEdge"

using namespace world;

namespace de {

DE_PIMPL_NOREF(ShadowEdge)
{
    const HEdge *leftMostHEdge = nullptr;
    dint edge = 0;

    Vec3d inner;
    Vec3d outer;
    dfloat sectorOpenness = 0;
    dfloat openness = 0;
};

ShadowEdge::ShadowEdge() : d(new Impl)
{}

void ShadowEdge::init(const HEdge &leftMostHEdge, dint edge)
{
    d->leftMostHEdge = &leftMostHEdge;
    d->edge          = edge;

    d->inner = d->outer = Vec3d();
    d->sectorOpenness = d->openness = 0;
}

/**
 * Returns a value in the range of 0..2, representing how 'open' the edge is.
 *
 * @c =0 Completely closed, it is facing a wall or is relatively distant from the edge on
 *       the other side.
 * @c >0 && <1 How near the 'other' edge is.
 * @c =1 At the same height as "this" one.
 * @c >1 The 'other' edge is past our height (clearly 'open').
 */
static dfloat opennessFactor(dfloat fz, dfloat bz, dfloat bhz)
{
    if(fz <= bz - SHADOWEDGE_OPEN_THRESHOLD || fz >= bhz)
        return 0;  // Fully closed.

    if(fz >= bhz - SHADOWEDGE_OPEN_THRESHOLD)
        return (bhz - fz) / SHADOWEDGE_OPEN_THRESHOLD;

    if(fz <= bz)
        return 1 - (bz - fz) / SHADOWEDGE_OPEN_THRESHOLD;

    if(fz <= bz + SHADOWEDGE_OPEN_THRESHOLD)
        return 1 + (fz - bz) / SHADOWEDGE_OPEN_THRESHOLD;

    return 2;  // Fully open!
}

/// @todo fixme: Should use the visual plane heights of subsectors.
static bool middleMaterialCoversOpening(const LineSide &side)
{
    if (!side.hasSector()) return false;  // Never.

    if (!side.hasSections()) return false;
    //if (!side.middle().hasMaterial()) return false;

    MaterialAnimator *matAnimator = side.middle().materialAnimator();
    /* material().as<ClientMaterial>()
            .getAnimator(Rend_MapSurfaceMaterialSpec());*/

    if (!matAnimator) return false;

    // Ensure we have up to date info about the material.
    matAnimator->prepare();

    // Might the material cover the opening?
    if (matAnimator->isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        // Stretched middles always cover the opening.
        if (side.isFlagged(SDF_MIDDLE_STRETCH))
            return true;

        const Sector &frontSec = side.sector();
        const Sector *backSec  = side.back().sectorPtr();

        // Determine the opening between the visual sector planes at this edge.
        coord_t openBottom;
        if (backSec && backSec->floor().heightSmoothed() > frontSec.floor().heightSmoothed())
        {
            openBottom = backSec->floor().heightSmoothed();
        }
        else
        {
            openBottom = frontSec.floor().heightSmoothed();
        }

        coord_t openTop;
        if (backSec && backSec->ceiling().heightSmoothed() < frontSec.ceiling().heightSmoothed())
        {
            openTop = backSec->ceiling().heightSmoothed();
        }
        else
        {
            openTop = frontSec.ceiling().heightSmoothed();
        }

        if (matAnimator->dimensions().y >= openTop - openBottom)
        {
            // Possibly; check the placement.
            if(side.leftHEdge()) // possibility of degenerate BSP leaf
            {
                WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                              *side.leftHEdge(), Line::From);
                return (edge.isValid() && edge.top().z() > edge.bottom().z()
                        && edge.top().z() >= openTop && edge.bottom().z() <= openBottom);
            }
        }
    }

    return false;
}

void ShadowEdge::prepare(dint planeIndex)
{
    dint const otherPlaneIndex = planeIndex == Sector::Floor? Sector::Ceiling : Sector::Floor;
    const HEdge &hedge = *d->leftMostHEdge;
    const auto &subsec = hedge.face().mapElementAs<ConvexSubspace>()
                            .subsector().as<world::ClientSubsector>();
    const Plane &plane = subsec.visPlane(planeIndex);

    const LineSide &lineSide = hedge.mapElementAs<LineSideSegment>().lineSide();

    d->sectorOpenness = d->openness = 0; // Default is fully closed.

    // Determine the 'openness' of the wall edge sector. If the sector is open,
    // there won't be a shadow at all. Open neighbor sectors cause some changes
    // in the polygon corner vertices (placement, opacity).

    if (hedge.twin().hasFace() &&
        hedge.twin().face().mapElementAs<ConvexSubspace>().hasSubsector())
    {
        const auto &backSubsec = hedge.twin().face().mapElementAs<ConvexSubspace>()
                                    .subsector().as<world::ClientSubsector>();

        const Plane &backPlane = backSubsec.visPlane(planeIndex);
        const Surface &wallEdgeSurface =
            lineSide.back().hasSector() ? lineSide.surface(planeIndex == Sector::Ceiling ? LineSide::Top : LineSide::Bottom)
                                        : lineSide.middle();

        // Figure out the relative plane heights.
        coord_t fz = plane.heightSmoothed();
        if (planeIndex == Sector::Ceiling)
            fz = -fz;

        coord_t bz = backPlane.heightSmoothed();
        if (planeIndex == Sector::Ceiling)
            bz = -bz;

        coord_t bhz = backSubsec.sector().plane(otherPlaneIndex).heightSmoothed();
        if (planeIndex == Sector::Ceiling)
            bhz = -bhz;

        // Determine openness.
        if (fz < bz && !wallEdgeSurface.hasMaterial())
        {
            d->sectorOpenness = 2; // Consider it fully open.
        }
        // Is the back sector a closed yet sky-masked surface?
        else if (subsec.visFloor().heightSmoothed() >= backSubsec.visCeiling().heightSmoothed() &&
                 subsec    .visPlane(otherPlaneIndex).surface().hasSkyMaskedMaterial() &&
                 backSubsec.visPlane(otherPlaneIndex).surface().hasSkyMaskedMaterial())
        {
            d->sectorOpenness = 2; // Consider it fully open.
        }
        else
        {
            // Does the middle material completely cover the open range (we do
            // not want to give away the location of any secret areas)?
            if (!middleMaterialCoversOpening(lineSide))
            {
                d->sectorOpenness = opennessFactor(fz, bz, bhz);
            }
        }
    }

    // Only calculate the remaining values when the edge is at least partially open.
    if (d->sectorOpenness >= 1)
        return;

    // Find the neighbor of this wall section and determine the relative
    // 'openness' of it's plane heights vs those of "this" wall section.
    /// @todo fixme: Should use the visual plane heights of subsectors.

    dint const edge = lineSide.sideId() ^ d->edge;
    const LineOwner *vo = lineSide.line().vertexOwner(edge)->navigate(ClockDirection(d->edge ^ 1));
    const Line &neighborLine = vo->line();

    if (&neighborLine == &lineSide.line())
    {
        d->openness = 1; // Fully open.
    }
    else if (neighborLine.isSelfReferencing()) /// @todo Skip over these? -ds
    {
        d->openness = 1;
    }
    else
    {
        // Choose the correct side of the neighbor (determined by which vertex is shared).
        const LineSide &neighborLineSide = neighborLine.side(&lineSide.line().vertex(edge) == &neighborLine.from()? d->edge ^ 1 : d->edge);

        if (!neighborLineSide.hasSections() && neighborLineSide.back().hasSector())
        {
            // A one-way window, open side.
            d->openness = 1;
        }
        else if (!neighborLineSide.hasSector() ||
                 (neighborLineSide.back().hasSector() && middleMaterialCoversOpening(neighborLineSide)))
        {
            d->openness = 0;
        }
        else if (neighborLineSide.back().hasSector())
        {
            // Its a normal neighbor.
            const Sector *backSec  = neighborLineSide.back().sectorPtr();
            if (backSec != &subsec.sector() &&
                !((plane.isSectorFloor  () && backSec->ceiling().heightSmoothed() <= plane.heightSmoothed()) ||
                  (plane.isSectorCeiling() && backSec->floor  ().heightSmoothed() >= plane.heightSmoothed())))
            {
                // Figure out the relative plane heights.
                coord_t fz = plane.heightSmoothed();
                if (planeIndex == Sector::Ceiling)
                    fz = -fz;

                coord_t bz = backSec->plane(planeIndex).heightSmoothed();
                if (planeIndex == Sector::Ceiling)
                    bz = -bz;

                coord_t bhz = backSec->plane(otherPlaneIndex).heightSmoothed();
                if (planeIndex == Sector::Ceiling)
                    bhz = -bhz;

                d->openness = opennessFactor(fz, bz, bhz);
            }
        }
    }

    if (d->openness < 1)
    {
        LineOwner *vo = lineSide.line().vertexOwner(lineSide.sideId() ^ d->edge);
        if (d->edge) vo = vo->prev();

        d->inner = Vec3d(lineSide.vertex(d->edge).origin() + vo->innerShadowOffset(),
                            plane.heightSmoothed());
    }
    else
    {
        d->inner = Vec3d(lineSide.vertex(d->edge).origin() + vo->extendedShadowOffset(),
                            plane.heightSmoothed());
    }

    d->outer = Vec3d(lineSide.vertex(d->edge).origin(), plane.heightSmoothed());
}

const Vec3d &ShadowEdge::inner() const
{
    return d->inner;
}

const Vec3d &ShadowEdge::outer() const
{
    return d->outer;
}

dfloat ShadowEdge::openness() const
{
    return d->openness;
}

dfloat ShadowEdge::sectorOpenness() const
{
    return d->sectorOpenness;
}

/// @todo Cache this result?
dfloat ShadowEdge::shadowStrength(dfloat darkness) const
{
    if(d->sectorOpenness < 1)
    {
        dfloat strength = de::min(darkness * (1 - d->sectorOpenness), 1.f);
        if(d->openness < 1) strength *= 1 - d->openness;
        return strength;
    }
    return 0;
}

}  // namespace de
