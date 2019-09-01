/** @file r_util.cpp  Refresh Utility Routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "misc/r_util.h"

#include <cmath>
#include <de/binangle.h>
#include <de/vector1.h>

#ifdef __CLIENT__
#  include "world/p_players.h"

#  include "api_render.h"
#  include "render/viewports.h"
#endif

using namespace de;

float R_MovementYaw(float const mom[])
{
    return radianToDegree(atan2f(-mom[MY], mom[MX]));
}

float R_MovementXYYaw(float momx, float momy)
{
    float mom[2] = { momx, momy };
    return R_MovementYaw(mom);
}

float R_MovementPitch(float const mom[])
{
    return radianToDegree(atan2f(1.2f * mom[MZ], V2f_Length(mom)));
}

float R_MovementXYZPitch(float momx, float momy, float momz)
{
    float mom[3] = { momx, momy, momz };
    return R_MovementPitch(mom);
}

#ifdef __CLIENT__

angle_t R_ViewPointToAngle(Vec2d point)
{
    viewdata_t const *viewData = &viewPlayer->viewport();
    point -= Vec2d(viewData->current.origin);
    return M_PointXYToAngle(point.x, point.y);
}

coord_t R_ViewPointDistance(coord_t x, coord_t y)
{
    Vec3d const &viewOrigin = viewPlayer->viewport().current.origin;
    coord_t viewOriginv1[2] = { viewOrigin.x, viewOrigin.y };
    coord_t pointv1[2] = { x, y };
    return M_PointDistance(viewOriginv1, pointv1);
}

#endif // __CLIENT__

Vec3d R_ClosestPointOnPlane(Vec3f const &planeNormal_,
    Vec3d const &planePoint_, Vec3d const &origin_)
{
    vec3f_t planeNormal; V3f_Set(planeNormal, planeNormal_.x, planeNormal_.y, planeNormal_.z);
    vec3d_t planePoint; V3d_Set(planePoint, planePoint_.x, planePoint_.y, planePoint_.z);
    vec3d_t origin; V3d_Set(origin, origin_.x, origin_.y, origin_.z);
    vec3d_t point; V3d_ClosestPointOnPlanef(point, planeNormal, planePoint, origin);
    return point;
}

#ifdef __CLIENT__

void R_ProjectViewRelativeLine2D(coord_t const center[2], dd_bool alignToViewPlane,
    coord_t width, coord_t offset, coord_t start[2], coord_t end[2])
{
    viewdata_t const *viewData = &viewPlayer->viewport();
    float sinrv, cosrv;

    if(alignToViewPlane)
    {
        // Should be fully aligned to view plane.
        sinrv = -viewData->viewCos;
        cosrv =  viewData->viewSin;
    }
    else
    {
        // Transform the origin point.
        coord_t trX   = center[VX] - viewData->current.origin.x;
        coord_t trY   = center[VY] - viewData->current.origin.y;
        float thangle = BANG2RAD(bamsAtan2(trY * 10, trX * 10)) - float(de::PI) / 2;
        sinrv = sin(thangle);
        cosrv = cos(thangle);
    }

    start[VX] = center[VX];
    start[VY] = center[VY];

    start[VX] -= cosrv * ((width / 2) + offset);
    start[VY] -= sinrv * ((width / 2) + offset);
    end[VX] = start[VX] + cosrv * width;
    end[VY] = start[VY] + sinrv * width;
}

void R_ProjectViewRelativeLine2D(Vec2d const center, bool alignToViewPlane,
    coord_t width, coord_t offset, Vec2d &start, Vec2d &end)
{
    viewdata_t const *viewData = &viewPlayer->viewport();
    float sinrv, cosrv;

    if(alignToViewPlane)
    {
        // Should be fully aligned to view plane.
        sinrv = -viewData->viewCos;
        cosrv =  viewData->viewSin;
    }
    else
    {
        // Transform the origin point.
        coord_t trX   = center[VX] - viewData->current.origin.x;
        coord_t trY   = center[VY] - viewData->current.origin.y;
        float thangle = BANG2RAD(bamsAtan2(trY * 10, trX * 10)) - float(de::PI) / 2;
        sinrv = sin(thangle);
        cosrv = cos(thangle);
    }

    start = center - Vec2d(cosrv * ((width / 2) + offset),
                              sinrv * ((width / 2) + offset));
    end = start + Vec2d(cosrv * width, sinrv * width);
}

void R_AmplifyColor(de::Vec3f &rgb)
{
    float max = 0;

    for(int i = 0; i < 3; ++i)
    {
        if(rgb[i] > max)
            max = rgb[i];
    }
    if(!max || max == 1) return;

    for(int i = 0; i < 3; ++i)
    {
        rgb[i] = rgb[i] / max;
    }
}

#endif // __CLIENT__

void R_ScaleAmbientRGB(float *out, float const *in, float mul)
{
    mul = de::clamp(0.f, mul, 1.f);
    for(int i = 0; i < 3; ++i)
    {
        float val = in[i] * mul;
        if(out[i] < val)
            out[i] = val;
    }
}

bool R_GenerateTexCoords(Vec2f &s, Vec2f &t, Vec3d const &point,
    float xScale, float yScale, Vec3d const &v1, Vec3d const &v2,
    Mat3f const &tangentMatrix)
{
    Vec3d const v1ToPoint = v1 - point;
    s[0] = v1ToPoint.dot(tangentMatrix.column(0)/*tangent*/) * xScale + .5f;
    t[0] = v1ToPoint.dot(tangentMatrix.column(1)/*bitangent*/) * yScale + .5f;

    // Is the origin point visible?
    if(s[0] >= 1 || t[0] >= 1)
        return false; // Right on the X axis or below on the Y axis.

    Vec3d const v2ToPoint = v2 - point;
    s[1] = v2ToPoint.dot(tangentMatrix.column(0)) * xScale + .5f;
    t[1] = v2ToPoint.dot(tangentMatrix.column(1)) * yScale + .5f;

    // Is the end point visible?
    if(s[1] <= 0 || t[1] <= 0)
        return false; // Left on the X axis or above on the Y axis.

    return true;
}

char const *R_NameForBlendMode(blendmode_t mode)
{
    static char const *names[1 + NUM_BLENDMODES] = {
        /* invalid */               "(invalid)",
        /* BM_ZEROALPHA */          "zero_alpha",
        /* BM_NORMAL */             "normal",
        /* BM_ADD */                "add",
        /* BM_DARK */               "dark",
        /* BM_SUBTRACT */           "subtract",
        /* BM_REVERSE_SUBTRACT */   "reverse_subtract",
        /* BM_MUL */                "mul",
        /* BM_INVERSE */            "inverse",
        /* BM_INVERSE_MUL */        "inverse_mul",
        /* BM_ALPHA_SUBTRACT */     "alpha_subtract"
    };
    if(!VALID_BLENDMODE(mode)) return names[0];
    return names[2 + int(mode)];
}

#undef R_ChooseAlignModeAndScaleFactor
DE_EXTERN_C dd_bool R_ChooseAlignModeAndScaleFactor(float *scale, int width, int height,
    int availWidth, int availHeight, scalemode_t scaleMode)
{
    if(scaleMode == SCALEMODE_STRETCH)
    {
        if(scale) *scale = 1;
        return true;
    }
    else
    {
        float heightAspectCorrected = height * 1.2f;

        // First try scaling horizontally to fit the available width.
        float factor = float(availWidth) / float(width);
        if(factor * heightAspectCorrected <= availHeight)
        {
            // Fits, use letterbox.
            if(scale) *scale = factor;
            return false;
        }

        // Fit vertically instead.
        if(scale) *scale = float(availHeight) / heightAspectCorrected;
        return true; // Pillarbox.
    }
}

#undef R_ChooseScaleMode2
DE_EXTERN_C scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight,
    scalemode_t overrideMode, float stretchEpsilon)
{
    float const availRatio = float(availWidth) / availHeight;
    float const origRatio  = float(width) / (height * 1.2f);

    // Considered identical?
    if(INRANGE_OF(availRatio, origRatio, .001f))
        return SCALEMODE_STRETCH;

    if(SCALEMODE_STRETCH == overrideMode || SCALEMODE_NO_STRETCH  == overrideMode)
        return overrideMode;

    // Within tolerable stretch range?
    return INRANGE_OF(availRatio, origRatio, stretchEpsilon)? SCALEMODE_STRETCH : SCALEMODE_NO_STRETCH;
}

#undef R_ChooseScaleMode
DE_EXTERN_C scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight,
    scalemode_t overrideMode)
{
    return R_ChooseScaleMode2(availWidth, availHeight, width, height, overrideMode,
                              DEFAULT_SCALEMODE_STRETCH_EPSILON);
}
