/** @file api_mapedit.cpp  Internal runtime map editing interface.
 *
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_MAP_EDIT

#include "de_platform.h"
#include "api_mapedit.h"

#include "world/map.h"
#include "world/polyobjdata.h"
#include "Plane"
#include "Sector"
#include "Surface"
#include "edit_map.h"
#include "dd_main.h"

#include <doomsday/world/entitydef.h>
#include <doomsday/world/Materials>
#include <doomsday/EntityDatabase>
#include <de/Error>
#include <de/Log>
#include <de/StringPool>

using namespace de;
using namespace world;

#define ERROR_IF_NOT_INITIALIZED() { \
    if (!editMapInited) \
        throw Error(__FUNCTION__, "Not active, did you forget to call MPE_Begin()?"); \
    }

static world::Map *editMap;
static bool        editMapInited;

/**
 * Material name references specified during map conversion are recorded in
 * this dictionary. A dictionary is used to avoid repeatedly resolving the same
 * URIs and to facilitate a log of missing materials encountered during the
 * process.
 *
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks the number of times a reference occurs.
 */
static StringPool *materialDict;

/**
 * Destroy the missing material dictionary.
 */
static void clearMaterialDict()
{
    // Initialized?
    if(!materialDict) return;

    materialDict->clear();
    delete materialDict; materialDict = 0;
}

/**
 * Print any "missing" materials in the dictionary to the log.
 */
static void printMissingMaterialsInDict()
{
    if(!::materialDict) return;

    ::materialDict->forAll([] (StringPool::Id id)
    {
        // A valid id?
        if(::materialDict->string(id))
        {
            // An unresolved reference?
            if(!::materialDict->userPointer(id))
            {
                LOG_RES_WARNING("Found %4i x unknown material \"%s\"")
                    << ::materialDict->userValue(id)
                    << ::materialDict->string(id);
            }
        }
        return LoopContinue;
    });
}

/**
 * Attempt to locate a material by its URI. A dictionary of previously
 * searched-for URIs is maintained to avoid repeated searching and to record
 * "missing" materials.
 *
 * @param materialUriStr  URI of the material to search for.
 *
 * @return  Pointer to the found material; otherwise @c 0.
 */
static Material *findMaterialInDict(const String &materialUriStr)
{
    if(materialUriStr.isEmpty()) return 0;

    // Time to create the dictionary?
    if(!materialDict)
    {
        materialDict = new StringPool;
    }

    res::Uri materialUri(materialUriStr, RC_NULL);

    // Intern this reference.
    StringPool::Id internId = materialDict->intern(materialUri.compose());
    Material *material = 0;

    // Have we previously encountered this?.
    uint refCount = materialDict->userValue(internId);
    if(refCount)
    {
        // Yes, if resolved the user pointer holds the found material.
        material = (Material *) materialDict->userPointer(internId);
    }
    else
    {
        // No, attempt to resolve this URI and update the dictionary.
        // First try the preferred scheme, then any.
        try
        {
            material = &world::Materials::get().material(materialUri);
        }
        catch(const Resources::MissingResourceManifestError &)
        {
            // Try any scheme.
            try
            {
                materialUri.setScheme("");
                material = &world::Materials::get().material(materialUri);
            }
            catch(const Resources::MissingResourceManifestError &)
            {}
        }

        // Insert the possibly resolved material into the dictionary.
        materialDict->setUserPointer(internId, material);
    }

    // There is now one more reference.
    refCount++;
    materialDict->setUserValue(internId, refCount);

    return material;
}

static inline Material *findMaterialInDict(const ddstring_t *materialUriStr)
{
    if(!materialUriStr) return 0;
    return findMaterialInDict(Str_Text(materialUriStr));
}

world::Map *MPE_Map()
{
    return editMapInited? editMap : 0;
}

world::Map *MPE_TakeMap()
{
    editMapInited = false;
    world::Map *retMap = editMap; editMap = 0;
    return retMap;
}

#undef MPE_Begin
dd_bool MPE_Begin(const uri_s * /*mapUri*/)
{
    if(!editMapInited)
    {
        delete editMap;
        editMap = new world::Map;
        editMapInited = true;
    }
    return true;
}

#undef MPE_End
dd_bool MPE_End()
{
    if (!editMapInited) return false;

    /*
     * Log warnings about any issues we encountered during conversion of
     * the basic map data elements.
     */
    printMissingMaterialsInDict();
    clearMaterialDict();

    // Note the map is left in an editable state in case the caller decides
    // they aren't finished after all...
    return true;
}

#undef MPE_VertexCreate
int MPE_VertexCreate(coord_t x, coord_t y, int archiveIndex)
{
    ERROR_IF_NOT_INITIALIZED();
    return editMap->createVertex(Vec2d(x, y), archiveIndex)->indexInMap();
}

#undef MPE_VertexCreatev
dd_bool MPE_VertexCreatev(int num, const coord_t *values, int *archiveIndices, int *retIndices)
{
    ERROR_IF_NOT_INITIALIZED();

    if(num <= 0 || !values)
        return false;

    // Create many vertexes.
    for(int n = 0; n < num; ++n)
    {
        Vertex *vertex = editMap->createVertex(Vec2d(values[n * 2], values[n * 2 + 1]),
                                               archiveIndices[n]);
        if(retIndices)
        {
            retIndices[n] = vertex->indexInMap();
        }
    }

    return true;
}

#undef MPE_LineCreate
int MPE_LineCreate(int v1, int v2, int frontSectorIdx, int backSectorIdx, int flags,
                   int archiveIndex)
{
    ERROR_IF_NOT_INITIALIZED();

    if(frontSectorIdx >= editMap->editableSectorCount()) return -1;
    if(backSectorIdx  >= editMap->editableSectorCount()) return -1;
    if(v1 < 0 || v1 >= editMap->vertexCount()) return -1;
    if(v2 < 0 || v2 >= editMap->vertexCount()) return -1;
    if(v1 == v2) return -1;

    // Next, check the length is not zero.
    /// @todo fixme: We need to allow these... -ds
    Vertex &vtx1 = editMap->vertex(v1);
    Vertex &vtx2 = editMap->vertex(v2);
    if(de::abs(Vec2d(vtx1.origin() - vtx2.origin()).length()) <= 0.0001) return -1;

    Sector *frontSector = (frontSectorIdx >= 0? editMap->editableSectors().at(frontSectorIdx) : 0);
    Sector *backSector  = (backSectorIdx  >= 0? editMap->editableSectors().at(backSectorIdx) : 0);

    return editMap->createLine(vtx1, vtx2, flags, frontSector, backSector, archiveIndex)
                        ->indexInMap();
}

#undef MPE_LineAddSide
void MPE_LineAddSide(int lineIdx, int sideId, short flags, const ddstring_t *topMaterialUri,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    const ddstring_t *middleMaterialUri, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleOpacity, const ddstring_t *bottomMaterialUri,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue, int archiveIndex)
{
    ERROR_IF_NOT_INITIALIZED();

    if(lineIdx < 0 || lineIdx >= editMap->editableLineCount()) return;

    Line *line = editMap->editableLines().at(lineIdx);
    LineSide &side = line->side(sideId);

    side.setFlags(flags);
    side.setIndexInArchive(archiveIndex);

    // Ensure sections are defined if they aren't already.
    side.addSections();

    // Assign the resolved material if found.
    side.top()
        .setMaterial(findMaterialInDict(topMaterialUri))
        .setOrigin(Vec2f(topOffsetX, topOffsetY))
        .setColor(Vec3f(topRed, topGreen, topBlue));

    side.middle()
        .setMaterial(findMaterialInDict(middleMaterialUri))
        .setOrigin(Vec2f(middleOffsetX, middleOffsetY))
        .setColor(Vec3f(middleRed, middleGreen, middleBlue))
        .setOpacity(middleOpacity);

    side.bottom()
        .setMaterial(findMaterialInDict(bottomMaterialUri))
        .setOrigin(Vec2f(bottomOffsetX, bottomOffsetY))
        .setColor(Vec3f(bottomRed, bottomGreen, bottomBlue));
}

#undef MPE_PlaneCreate
int MPE_PlaneCreate(int sectorIdx, coord_t height, const ddstring_t *materialUri,
    float matOffsetX, float matOffsetY, float tintRed, float tintGreen, float tintBlue, float opacity,
    float normalX, float normalY, float normalZ, int archiveIndex)
{
    ERROR_IF_NOT_INITIALIZED();

    if(sectorIdx < 0 || sectorIdx >= editMap->editableSectorCount()) return -1;

    Sector *sector = editMap->editableSectors().at(sectorIdx);
    Plane *plane = sector->addPlane(Vec3f(normalX, normalY, normalZ), height);

    plane->setIndexInArchive(archiveIndex);

    plane->surface()
        .setMaterial(findMaterialInDict(materialUri))
        .setColor(Vec3f(tintRed, tintGreen, tintBlue))
        .setOrigin(Vec2f(matOffsetX, matOffsetY));

    if(!plane->isSectorFloor() && !plane->isSectorCeiling())
    {
        plane->surface().setOpacity(opacity);
    }

    return plane->indexInSector();
}

#undef MPE_SectorCreate
int MPE_SectorCreate(float lightlevel, float red, float green, float blue,
                     int archiveIndex)
{
    ERROR_IF_NOT_INITIALIZED();
    return editMap->createSector(lightlevel, Vec3f(red, green, blue),
                                 archiveIndex)->indexInMap();
}

#undef MPE_PolyobjCreate
int MPE_PolyobjCreate(const int *lines, int lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY, int archiveIndex)
{
    DE_UNUSED(archiveIndex); /// @todo Use this!

    ERROR_IF_NOT_INITIALIZED();

    if(lineCount <= 0 || !lines) return -1;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(int i = 0; i < lineCount; ++i)
    {
        if(lines[i] < 0 || lines[i] >= editMap->editableLineCount()) return -1;

        Line *line = editMap->editableLines().at(lines[i]);
        if(line->definesPolyobj()) return -1;
    }

    Polyobj *po = editMap->createPolyobj(Vec2d(originX, originY));
    po->setSequenceType(sequenceType);
    po->setTag(tag);

    for(int i = 0; i < lineCount; ++i)
    {
        Line *line = editMap->editableLines().at(lines[i]);

        // This line belongs to a polyobj.
        line->setPolyobj(po);
        po->data().lines.append(line);
    }

    return po->indexInMap();
}

#undef MPE_GameObjProperty
dd_bool MPE_GameObjProperty(const char *entityName, int elementIndex,
    const char *propertyName, valuetype_t valueType, void *valueAdr)
{
    LOG_AS("MPE_GameObjProperty");

    ERROR_IF_NOT_INITIALIZED();

    if(!entityName || !propertyName || !valueAdr)
        return false; // Hmm...

    // Is this a known entity?
    MapEntityDef *entityDef = P_MapEntityDefByName(entityName);
    if(!entityDef)
    {
        LOG_WARNING("Unknown entity name:\"%s\", ignoring.") << entityName;
        return false;
    }

    // Is this a known property?
    MapEntityPropertyDef *propertyDef;
    if(MapEntityDef_PropertyByName(entityDef, propertyName, &propertyDef) < 0)
    {
        LOG_WARNING("Entity \"%s\" has no \"%s\" property, ignoring.")
                << entityName << propertyName;
        return false;
    }

    try
    {
        EntityDatabase &entities = editMap->entityDatabase();
        entities.setProperty(propertyDef, elementIndex, valueType, valueAdr);
        return true;
    }
    catch(const Error &er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

DE_DECLARE_API(MPE) =
{
    { DE_API_MAP_EDIT },

    MPE_Begin,
    MPE_End,
    MPE_VertexCreate,
    MPE_VertexCreatev,
    MPE_LineCreate,
    MPE_LineAddSide,
    MPE_SectorCreate,
    MPE_PlaneCreate,
    MPE_PolyobjCreate,
    MPE_GameObjProperty
};
