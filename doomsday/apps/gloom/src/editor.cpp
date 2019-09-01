/** @file editor.cpp
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

#include "editor.h"
#include "../src/gloomapp.h"
#include "gloom/geo/geomath.h"
#include "gloom/world/mapimport.h"

#include <de/FS>
#include <de/Matrix>
#include <doomsday/DataBundle>
#include <doomsday/LumpCatalog>

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>

using namespace de;
using namespace gloom;

static const int DRAG_MIN_DIST = 2;
static const int UNDO_MAX = 50;

struct EntityType {
    Entity::Type type;
    QString label;
};
static const QMap<Entity::Type, String> entityMetadata {
    std::make_pair(Entity::Light,        String("Light")),
    std::make_pair(Entity::Spotlight,    String("Spotlight")),
    std::make_pair(Entity::Tree1,        String("Tree1")),
    std::make_pair(Entity::Tree2,        String("Tree2")),
    std::make_pair(Entity::Tree3,        String("Tree3")),
    std::make_pair(Entity::TestSphere,   String("Test Sphere")),
    std::make_pair(Entity::Buggy,        String("Buggy"))
};

enum Direction {
    Horizontal = 0x1,
    Vertical   = 0x2,
    Both       = Horizontal | Vertical,
};
Q_DECLARE_FLAGS(Directions, Direction)
Q_DECLARE_OPERATORS_FOR_FLAGS(Directions)

DENG2_PIMPL(Editor)
{
    enum UserAction {
        None,
        TranslateView,
        SelectRegion,
        Move,
        Scale,
        Rotate,
        AddLines,
        AddSector,
    };

    Map        map;
    String     filePath;
    bool       isModified = false;
    QList<Map> undoStack;

    Mode       mode       = EditPoints;
    UserAction userAction = None;
    QPoint     actionPos;
    QPoint     pivotPos;
    QFont      metaFont;
    QRectF     selectRect;
    QSet<ID>   selection;
    ID         hoverPoint  = 0;
    ID         hoverLine   = 0;
    ID         hoverSector = 0;
    ID         hoverEntity = 0;
    ID         hoverPlane  = 0;

    float viewScale = 10;
    float viewYawAngle = 0;
    float viewPitchAngle = 0;
    Vec2f viewOrigin;
    Plane viewPlane;
    Vec3f worldFront;
    Mat4f viewTransform;
    Mat4f inverseViewTransform;

    QHash<ID, Vec3d> floorPoints;

    const QColor metaBg{255, 255, 255, 192};
    const QColor metaColor{0, 0, 0, 128};
    const QColor metaBg2{0, 0, 0, 128};
    const QColor metaColor2{255, 255, 255};

    Impl(Public *i) : Base(i)
    {
        // Load the last map.
        if (persistentMapPath())
        {
            loadMap(persistentMapPath());
        }

        // Check for previous state.
        {
            QSettings st;
            viewScale  = st.value("viewScale", 10).toFloat();
            viewOrigin = geo::toVec2d(st.value("viewOrigin").value<QVector2D>());
        }
    }

    ~Impl()
    {
        // Save the editor state.
        {
            QSettings st;
            st.setValue("filePath", filePath);
            st.setValue("viewScale", viewScale);
            st.setValue("viewOrigin", geo::toQVector2D(viewOrigin));
        }
    }

    String persistentMapPath() const
    {
        return QSettings().value("filePath", "").toString();
    }

    String modeText() const
    {
        const char *modeStr[ModeCount] = {
            "Points",
            "Lines",
            "Sectors",
            "Planes",
            "Volumes",
            "Entities",
        };
        return modeStr[mode];
    }

    String actionText() const
    {
        switch (userAction)
        {
        case TranslateView: return "Translate view";
        case SelectRegion: return "Select";
        case Move: return "Move";
        case Scale: return "Scale";
        case Rotate: return "Rotate";
        case AddLines: return "Add lines";
        case AddSector: return "Add sector";
        case None: break;
        }
        return "";
    }

    String statusText() const
    {
        String selText;
        if (selection.size())
        {
            selText = String(":%1").arg(selection.size());
        }
        String text = String("%1 (%2%3) %4")
                .arg(modeText())
                .arg(mode == EditPoints   ? map.points()  .size()
                   : mode == EditLines    ? map.lines()   .size()
                   : mode == EditSectors  ? map.sectors() .size()
                   : mode == EditEntities ? map.entities().size()
                   : mode == EditPlanes   ? map.planes()  .size()
                   : mode == EditVolumes  ? map.volumes() .size()
                   : 0)
                .arg(selText)
                .arg(actionText());
        if (hoverPoint)
        {
            text += String(" \u25aa%1").arg(hoverPoint, 0, 16);
        }
        if (hoverLine)
        {
            text += String(" \u2215%1").arg(hoverLine, 0, 16);
        }
        if (hoverEntity)
        {
            text += String(" \u25c9%1").arg(hoverEntity, 0, 16);
        }
        if (hoverSector)
        {
            text += String(" \u25b3%1").arg(hoverSector, 0, 16);
        }
        if (hoverPlane)
        {
            text += String(" \u25b1%1").arg(hoverPlane, 0, 16);
        }
        return text;
    }

    void setMode(Mode newMode)
    {
        finishAction();
        mode = newMode;
        emit self().modeChanged(mode);
        self().update();
    }

    bool isModifyingAction(UserAction action) const
    {
        switch (action)
        {
            default: return false;

            case Move:
            case Rotate:
            case Scale:
            case AddLines:
            case AddSector:
                return true;
        }
    }

    void beginAction(UserAction action)
    {
        finishAction();

        if (isModifyingAction(action))
        {
            pushUndo();
        }

        userAction = action;
        switch (action)
        {
        case Rotate:
        case Scale:
            pivotPos = actionPos = viewMousePos();
            self().setCursor(action == Rotate? Qt::SizeVerCursor : Qt::SizeFDiagCursor);
            break;

        default: break;
        }
    }

    bool finishAction()
    {
        switch (userAction)
        {
        case None:
            return false;

        case TranslateView:
            break;

        case SelectRegion:
            switch (mode)
            {
            case EditPoints:
                for (auto i = map.points().begin(); i != map.points().end(); ++i)
                {
                    const QPointF viewPos = viewPoint(i.key());
                    if (selectRect.contains(viewPos))
                    {
                        selection.insert(i.key());
                    }
                }
                break;

            case EditLines:
            case EditSectors:
                for (auto i = map.lines().begin(); i != map.lines().end(); ++i)
                {
                    const auto &line = i.value();
                    QPointF viewPos[2] = {worldToView(map.point(line.points[0])),
                                          worldToView(map.point(line.points[1]))};
                    if (selectRect.contains(viewPos[0]) && selectRect.contains(viewPos[1]))
                    {
                        selection.insert(i.key());
                    }
                }
                if (mode == EditLines) emit self().lineSelectionChanged();
                break;

            case EditEntities:
                break;
            }
            break;

        case Move:
        case Scale:
        case Rotate:
        case AddLines:
        case AddSector:
            break;
        }

        userAction = None;
        actionPos  = QPoint();
        selectRect = QRectF();

        self().setCursor(Qt::CrossCursor);
        self().update();
        return true;
    }

    QPointF worldToView(const Vec3d &worldPos) const
    {
        const auto p = viewTransform * worldPos;
        return QPointF(p.x, p.y);
    }

    QPointF worldToView(const Point &point, const Plane *plane = nullptr) const
    {
        if (!plane) plane = &viewPlane;
        return worldToView(plane->projectPoint(point));
    }

    Vec3d viewToWorldCoord(const QPointF &pos) const
    {
        return inverseViewTransform * Vec3f(float(pos.x()), float(pos.y()));
    }

    Point viewToWorldPoint(const QPointF &pos) const
    {
        Vec3d p = viewToWorldCoord(pos);
        p = viewPlane.toGeoPlane().intersectRay(p, worldFront);
        return Point{Vec2d(p.x, p.z)};
    }

    Mat4f viewOrientation() const
    {
        return Mat4f::rotate(viewPitchAngle, Vec3f(1, 0, 0)) *
               Mat4f::rotate(viewYawAngle,   Vec3f(0, 1, 0));
    }

    void updateView()
    {
        const QSize viewSize = self().rect().size();

        Mat4f mapRot = viewOrientation();

        worldFront = mapRot.inverse() * Vec3f{0, -1, 0};

        viewPlane     = Plane{{viewOrigin.x, 0, viewOrigin.y}, {0, 1, 0}, {"", ""}};
        viewTransform = Mat4f::translate(Vec3f(viewSize.width() / 2, viewSize.height() / 2)) *
                        Mat4f::rotate(-90, Vec3f(1, 0, 0)) *
                        mapRot *
                        Mat4f::scale(viewScale) *
                        Mat4f::translate(-viewPlane.point);
        inverseViewTransform = viewTransform.inverse();
    }

    QPoint viewMousePos() const { return self().mapFromGlobal(QCursor().pos()); }

    QPointF viewPoint(ID pointId, ID heightReferencePointId = 0) const
    {
        auto found = floorPoints.find(pointId);
        if (found != floorPoints.end())
        {
            Vec3d coord = found.value();
            // Check the reference.
            if (heightReferencePointId)
            {
                auto ref = floorPoints.find(heightReferencePointId);
                coord.y = std::max(coord.y, ref.value().y);
            }
            return worldToView(coord);
        }
        return worldToView(map.point(pointId));
    }

    QLineF viewLine(const Line &line) const
    {
        const QPointF start = viewPoint(line.points[0], line.points[1]);
        const QPointF end   = viewPoint(line.points[1], line.points[0]);
        return QLineF(start, end);
    }

    Vec3d worldMouseCoord() const { return viewToWorldCoord(viewMousePos()); }

    Point worldMousePoint() const { return viewToWorldPoint(viewMousePos()); }

    Vec3d worldActionCoord() const { return viewToWorldCoord(actionPos); }

    Point worldActionPoint() const { return viewToWorldPoint(actionPos); }

    void pushUndo()
    {
        isModified = true;
        undoStack.append(map);
        if (undoStack.size() > UNDO_MAX)
        {
            undoStack.removeFirst();
        }
    }

    void popUndo()
    {
        if (!undoStack.isEmpty())
        {
            map = undoStack.takeLast();
            self().update();
        }
    }

    void userSelectAll()
    {
        selection.clear();
        switch (mode)
        {
        case EditPoints:
            for (auto id : map.points().keys())
            {
                selection.insert(id);
            }
            break;

        case EditLines:
            for (auto id : map.lines().keys())
            {
                selection.insert(id);
            }
            emit self().lineSelectionChanged();
            break;

        case EditSectors:
            for (auto id : map.sectors().keys())
            {
                selection.insert(id);
            }
            break;

        case EditEntities:
            for (auto id : map.entities().keys())
            {
                selection.insert(id);
            }
            break;

        case EditPlanes:
            emit self().planeSelectionChanged();
            break;
        }
        self().update();
    }

    void userSelectNone()
    {
        selection.clear();
        emit self().lineSelectionChanged();
        emit self().planeSelectionChanged();
        self().update();
    }

    void userAdd()
    {
        switch (mode)
        {
        case EditPoints:
            pushUndo();
            map.append(map.points(), worldMousePoint());
            break;

        case EditLines:
            if (selection.size() == 1)
            {
                beginAction(AddLines);
            }
            break;

        case EditSectors:
                /*
            if (selection.size() >= 3)
            {
                pushUndo();
                Sector sector;
                for (ID id : selection)
                {
                    if (map.isLine(id)) sector.walls << id;
                }
                selection.clear();
                ID floor = map.append(map.planes(), Plane{{Vec3d()}, {Vec3f(0, 1, 0)}});
                ID ceil  = map.append(map.planes(), Plane{{Vec3d(0, 3, 0)}, {Vec3f(0, -1, 0)}});
                ID vol   = map.append(map.volumes(), Volume{{floor, ceil}});
                sector.volumes << vol;
                ID secId = map.append(map.sectors(), sector);
                if (!linkSectorLines(secId))
                {
                    popUndo();
                }
                selection.insert(secId);
            }*/
            break;

        case EditVolumes:
            if (hoverSector)
            {
                pushUndo();

                Sector &sec = map.sector(hoverSector);
                ID oldCeiling = map.ceilingPlaneId(hoverSector);
                Plane &ceil = map.plane(oldCeiling);
                Plane newCeil = ceil;

                // Make a new plane 2 meters above the old ceiling.
                newCeil.point.y += 2;
                ceil.normal = -ceil.normal;
                ceil.material[1] = ceil.material[0];
                ID newCeiling = map.append(map.planes(), newCeil);
                Volume vol{{oldCeiling, newCeiling}};

                // The new volume becomes to topmost volume in the sector.
                ID newVolume = map.append(map.volumes(), vol);
                sec.volumes.append(newVolume);
                self().update();
            }
            break;

        case EditEntities:
            pushUndo();
            std::shared_ptr<Entity> ent(new Entity);
            ent->setPosition(worldMouseCoord());
            ent->setId(map.append(map.entities(), ent));
            break;
        }
        self().update();
    }

    void userDelete()
    {
        switch (mode)
        {
        case EditPoints:
            if (selection.size())
            {
                pushUndo();
                for (auto id : selection)
                {
                    map.points().remove(id);
                }
            }
            break;

        case EditLines:
            if (hoverLine)
            {
                pushUndo();
                map.lines().remove(hoverLine);
                hoverLine = 0;
            }
            break;

        case EditSectors:
            if (hoverSector)
            {
                pushUndo();
                map.sectors().remove(hoverSector);
                hoverSector = 0;
            }
            break;

        case EditEntities:
            if (hoverEntity)
            {
                pushUndo();
                map.entities().remove(hoverEntity);
                hoverEntity = 0;
            }
            break;
        }
        selection.clear();
        map.removeInvalid();
        self().update();
    }

    void userClick(Qt::KeyboardModifiers modifiers)
    {
        if (userAction == AddLines)
        {
            ID prevPoint = 0;
            if (selection.size()) prevPoint = *selection.begin();

            selection.clear();
            selectClickedObject(modifiers);
            if (!selection.isEmpty())
            {
                // Connect to this point.
                Line newLine;
                newLine.points[0]          = prevPoint;
                newLine.points[1]          = *selection.begin();
                newLine.surfaces[0].sector = newLine.surfaces[1].sector = 0;
                if (newLine.points[0] != newLine.points[1])
                {
                    map.append(map.lines(), newLine);
                    self().update();
                    return;
                }
            }
        }

        if (userAction != None)
        {
            finishAction();
            return;
        }

        if (mode == EditSectors && !hoverSector && hoverLine)
        {
            if (modifiers & Qt::ShiftModifier)
            {
                selectOrUnselect(hoverLine);
                return;
            }

            // Select a group of lines that might form a new sector.
            const auto clickPos = worldMousePoint();
            Edge       startRef{hoverLine,
                          map.geoLine(hoverLine).isFrontSide(clickPos.coord) ? Line::Front
                                                                             : Line::Back};

            if (map.line(hoverLine).surfaces[startRef.side].sector == 0)
            {
                IDList      secPoints;
                IDList      secWalls;
                QList<Edge> secEdges;

                if (map.buildSector(startRef, secPoints, secWalls, secEdges))
                {
                    pushUndo();

                    const ID floor = map.append(map.planes(), Plane{{Vec3d()}, {Vec3f(0, 1, 0)}, {"", ""}});
                    const ID ceil  = map.append(map.planes(), Plane{{Vec3d(0, 3, 0)}, {Vec3f(0, -1, 0)}, {"", ""}});
                    const ID vol   = map.append(map.volumes(), Volume{{floor, ceil}});

                    Sector newSector{secPoints, secWalls, {vol}};
                    ID secId = map.append(map.sectors(), newSector);

                    for (Edge edge : secEdges)
                    {
                        map.line(edge.line).surfaces[edge.side].sector = secId;
                    }
                    selection.clear();
                    selection.insert(secId);
                }
            }
            return;
        }

        // Select clicked object.
        if (!(modifiers & Qt::ShiftModifier))
        {
            selection.clear();
        }
        selectClickedObject(modifiers);
    }

    void userScale()
    {
        if (userAction != None)
        {
            finishAction();
        }
        else if (!selection.isEmpty())
        {
            beginAction(Scale);
        }
        self().update();
    }

    void userRotate()
    {
        if (userAction != None)
        {
            finishAction();
        }
        else if (!selection.isEmpty())
        {
            beginAction(Rotate);
        }
        self().update();
    }

    void drawGridLine(QPainter &ptr,
                      const Vec2d &worldPos,
                      const QColor &color,
                      Directions dirs = Both)
    {
        const QRect   winRect = self().rect();
        const QPointF origin  = worldToView(worldPos);

        ptr.setPen(color);

        if (dirs & Vertical)   ptr.drawLine(QLineF(origin.x(), 0, origin.x(), winRect.height()));
        if (dirs & Horizontal) ptr.drawLine(QLineF(0, origin.y(), winRect.width(), origin.y()));
    }

    void drawArrow(QPainter &ptr, QPointF a, QPointF b)
    {
        ptr.drawLine(a, b);

        QVector2D span(float(b.x() - a.x()), float(b.y() - a.y()));
        const float len = 5;
        if (span.length() > 5*len)
        {
            QVector2D dir = span.normalized();
            QVector2D normal{dir.y(), -dir.x()};
            QVector2D offsets[2] = {len*normal - 2*len*dir, -len*normal - 2*len*dir};
            QPointF mid = (a + 3*b) / 4;

            //ptr.drawLine(mid, mid + offsets[0].toPointF());
            ptr.drawLine(mid, mid + offsets[1].toPointF());
        }
    }

    void drawMetaLabel(QPainter &ptr, QPointF pos, QString text, bool lightStyle = true)
    {
        ptr.save();

        ptr.setFont(metaFont);
        ptr.setBrush(lightStyle? metaBg : metaBg2);
        ptr.setPen(Qt::NoPen);

        QFontMetrics  metrics(metaFont);
        const QSize   dims(metrics.width(text), metrics.height());
        const QPointF off(-dims.width() / 2, dims.height() / 2);
        const QPointF gap(-3, 3);

        ptr.drawRect(QRectF(pos - off - gap, pos + off + gap));
        ptr.setPen(lightStyle? metaColor : metaColor2);
        ptr.drawText(pos + off + QPointF(0, -metrics.descent()), text);

        ptr.restore();
    }

    double defaultClickDistance() const
    {
        return 20 / viewScale;
    }

    ID findPointAt(const QPoint &viewPos, double maxDistance = -1) const
    {
        if (maxDistance < 0)
        {
            maxDistance = defaultClickDistance() * viewScale;
        }

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.points().begin(); i != map.points().end(); ++i)
        {
            double d = (QVector2D(viewPoint(i.key())) - QVector2D(viewPos)).length();
            if (d < dist)
            {
                id = i.key();
                dist = d;
            }
        }
        return id;
    }

    ID findLineAt(const QPoint &pos, double maxDistance = -1) const
    {
        if (maxDistance < 0) maxDistance = defaultClickDistance() * viewScale;

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.lines().begin(), end = map.lines().end(); i != end; ++i)
        {
            const auto        line = viewLine(i.value());
            const geo::Line2d gLine{Vec2d(line.x1(), line.y1()), Vec2d(line.x2(), line.y2())};
            double            d = gLine.distanceTo(Vec2d(pos.x(), pos.y()));
            if (d < dist)
            {
                id = i.key();
                dist = d;
            }
        }
        return id;
    }

    ID findSectorAt(const Point &pos) const
    {
        for (ID id : map.sectors().keys())
        {
            if (map.sectorPolygon(id).isPointInside(pos.coord))
            {
                return id;
            }
        }
        return 0;
    }

    ID findPlaneAtViewPos(const QPoint &pos) const
    {
        for (ID secId : map.sectors().keys())
        {
            const Sector &sector = map.sector(secId);
            const auto secPoly = map.sectorPolygon(secId);
            for (ID volId : sector.volumes)
            {
                for (ID plnId : map.volume(volId).planes)
                {
                    QPolygonF poly;
                    for (auto pp : secPoly.points)
                    {
                        poly << worldToView(Point{pp.pos}, &map.plane(plnId));
                    }
                    if (poly.containsPoint(pos, Qt::OddEvenFill))
                    {
                        return plnId;
                    }
                }
            }
        }
        return 0;
    }

    ID findEntityAt(const QPoint &viewPos, double maxDistance = -1) const
    {
        if (maxDistance < 0) maxDistance = defaultClickDistance() * viewScale;

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.entities().begin(), end = map.entities().end(); i != end; ++i)
        {
            const auto &ent = i.value();
            const QPoint delta = worldToView(ent->position()).toPoint() - viewPos;
            double d = Vec2f(delta.x(), delta.y()).length();
            if (d < dist)
            {
                id   = i.key();
                dist = d;
            }
        }
        return id;
    }

    String entityLabel(const Entity &ent) const
    {
        return entityMetadata[ent.type()];
    }

    void selectOrUnselect(ID id)
    {
        if (!selection.contains(id))
        {
            selection.insert(id);
        }
        else
        {
            selection.remove(id);
        }
    }

    void selectClickedObject(Qt::KeyboardModifiers modifiers)
    {
        const auto pos = worldActionPoint();
        switch (mode)
        {
        case EditPoints:
            if (auto id = findPointAt(actionPos))
            {
                selectOrUnselect(id);
            }
            break;

        case EditLines:
            if (modifiers & Qt::ShiftModifier)
            {
                if (hoverLine)
                {
                    selectOrUnselect(hoverLine);
                }
            }
            else
            {
                if (auto id = findPointAt(actionPos))
                {
                    selectOrUnselect(id);
                }
            }
            emit self().lineSelectionChanged();
            break;

        case EditSectors:
            if (hoverSector)
            {
                selectOrUnselect(hoverSector);
            }
            break;

        case EditPlanes:
            if (hoverPlane)
            {
                selectOrUnselect(hoverPlane);
            }
            emit self().planeSelectionChanged();
            break;

        case EditEntities:
            if (hoverEntity)
            {
                selectOrUnselect(hoverEntity);
            }
            break;
        }
    }

    void splitLine(ID line, const Vec2d &where)
    {
        pushUndo();
        map.splitLine(line, Point{map.geoLine(line).nearestPoint(where)});
        self().update();
    }

    void build()
    {
        emit self().buildMapRequested();
    }

    bool askSaveFile()
    {
        if (isModified)
        {
            const auto answer =
                QMessageBox::question(thisPublic,
                                      "Save file?",
                                      "The map has been modified. Do you want to save the changes?",
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (answer == QMessageBox::Cancel)
            {
                return false;
            }
            if (answer == QMessageBox::Yes)
            {
                saveFile();
            }
        }
        return true;
    }

    void newFile()
    {
        if (!askSaveFile()) return;

        map = Map();
        isModified = false;
        filePath.clear();
        undoStack.clear();
        self().setWindowTitle("(unnamed)");

        self().update();
    }

    void openFile()
    {
        if (!askSaveFile()) return;

        if (String openPath = QFileDialog::getOpenFileName(
                thisPublic, "Open File", filePath.fileNamePath(), "Gloom Map (*.gloommap)"))
        {
            loadMap(openPath);
            self().update();
        }
    }

    void loadMap(const String &path)
    {
        filePath = path;

        QFile f(path);
        DENG2_ASSERT(f.exists());
        f.open(QFile::ReadOnly);
        map.deserialize(f.readAll());
        undoStack.clear();
        isModified = false;
        self().setWindowTitle(filePath.fileName());
    }

    void saveAsFile()
    {
        if (String newPath = QFileDialog::getSaveFileName(
                thisPublic, "Save As", filePath.fileNamePath(), "Gloom Map (*.gloommap)"))
        {
            filePath = newPath;
            self().setWindowTitle(filePath.fileName());
            saveFile();
        }
    }

    void saveFile()
    {
        if (!filePath)
        {
            saveAsFile();
            return;
        }
        QFile f(filePath);
        f.open(QFile::WriteOnly);
        f.write(map.serialize());
        isModified = false;
    }

    void importWADLevel()
    {
        askSaveFile();

        if (String openPath = QFileDialog::getOpenFileName(
                thisPublic, "Import from WAD File", filePath.fileNamePath(), "WAD File (*.wad)"))
        {
            String path = FS::accessNativeLocation(openPath);
            if (const DataBundle *bundle = FS::tryLocate<const DataBundle>(path))
            {
                if (bundle->readLumpDirectory())
                {
                    const auto *lumpDir = bundle->lumpDirectory();
                    StringList maps = lumpDir->findMapLumpNames();
                    if (maps.isEmpty()) return; // No maps found.

                    res::LumpCatalog catalog;
                    catalog.setBundles({bundle});

                    MapImport importer(catalog);
                    importer.importMap(maps.front());
                }
            }
            //newFile();
        }
    }
};

Editor::Editor()
    : d(new Impl(this))
{
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    auto addKeyAction = [this] (QString shortcut, std::function<void()> func)
    {
        QAction *add = new QAction;
        add->setShortcut(QKeySequence(shortcut));
        connect(add, &QAction::triggered, func);
        addAction(add);
    };

    d->metaFont = font();
    d->metaFont.setPointSizeF(d->metaFont.pointSizeF() * .75);

    // Actions.
    addKeyAction("Ctrl+1",          [this]() { d->setMode(EditPoints); });
    addKeyAction("Ctrl+2",          [this]() { d->setMode(EditLines); });
    addKeyAction("Ctrl+3",          [this]() { d->setMode(EditSectors); });
    addKeyAction("Ctrl+4",          [this]() { d->setMode(EditPlanes); });
    addKeyAction("Ctrl+5",          [this]() { d->setMode(EditVolumes); });
    addKeyAction("Ctrl+6",          [this]() { d->setMode(EditEntities); });
    addKeyAction("Ctrl+A",          [this]() { d->userSelectAll(); });
    addKeyAction("Ctrl+Shift+A",    [this]() { d->userSelectNone(); });
    addKeyAction("Ctrl+D",          [this]() { d->userAdd(); });
    addKeyAction("Ctrl+Backspace",  [this]() { d->userDelete(); });
    addKeyAction("R",               [this]() { d->userRotate(); });
    addKeyAction("S",               [this]() { d->userScale(); });
    addKeyAction("Ctrl+Z",          [this]() { d->popUndo(); });
    addKeyAction("Return",          [this]() { d->build(); });

    // Menu items.
    QMenuBar *menuBar = new QMenuBar;
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));
    fileMenu->addAction("&New",        [this]() { d->newFile(); });
    fileMenu->addAction("&Open...",    [this]() { d->openFile(); }, QKeySequence("Ctrl+O"));
    fileMenu->addSeparator();
    fileMenu->addAction("Import from WAD...", [this]() { d->importWADLevel(); }, QKeySequence("Ctrl+Shift+I"));
    fileMenu->addSeparator();
    fileMenu->addAction("Save &as...", [this]() { d->saveAsFile(); });
    fileMenu->addAction("&Save",       [this]() { d->saveFile(); }, QKeySequence("Ctrl+S"));
}

Map &Editor::map()
{
    return d->map;
}

bool Editor::maybeClose()
{
    if (!d->askSaveFile())
    {
        return false;
    }
    d->isModified = false;
    return true;
}

QSet<ID> Editor::selection() const
{
    return d->selection;
}

void Editor::markAsChanged()
{
    d->isModified = true;
}

void Editor::paintEvent(QPaintEvent *)
{
    d->updateView();

    QPainter ptr(this);
    ptr.setRenderHint(QPainter::Antialiasing);

    QRect        winRect = rect();
    QFontMetrics fontMetrics(font());
    QFontMetrics metaMetrics(d->metaFont);

    const Points &  mapPoints  = d->map.points();
    const Planes &  mapPlanes  = d->map.planes();
    const Lines &   mapLines   = d->map.lines();
    const Sectors & mapSectors = d->map.sectors();
    const Volumes & mapVolumes = d->map.volumes();
    const Entities &mapEnts    = d->map.entities();

    const int lineHgt = fontMetrics.height();
    const int gap     = 6;

    static const QColor panelBgs[ModeCount] = {
        {  0,   0,   0, 128},   // Points
        {  0,  20,  90, 160},   // Lines
        {255, 160,   0, 192},   // Sectors
        {  0, 128, 255, 128},   // Planes
        {225,  50, 225, 128},   // Volumes
        {140,  10,   0, 160},   // Entities
    };

    const QColor panelBg = panelBgs[d->mode];
    const QColor selectColor(64, 92, 255);
    const QColor selectColorAlpha(selectColor.red(), selectColor.green(), selectColor.blue(), 150);
    const QColor gridMajor{180, 180, 180, 255};
    const QColor gridMinor{220, 220, 220, 255};
    const QColor textColor = (panelBg.lightnessF() > 0.45 ? Qt::black : Qt::white);
    const QColor pointColor(170, 0, 0, 255);
    const QColor lineColor(64, 64, 64);
    const QColor verticalLineColor(128, 128, 128);
    const QColor sectorColor(128, 92, 0, 64);

    // Grid.
    {
        d->drawGridLine(ptr, d->worldMousePoint().coord, gridMinor);
        d->drawGridLine(ptr, Vec2d(), gridMajor);
    }

    // Bottom-most position where points are being used in sectors.
    d->floorPoints.clear();

    // Sectors and planes.
    {
        for (auto i = mapSectors.begin(), end = mapSectors.end(); i != end; ++i)
        {
            const ID    secId  = i.key();
            const auto &sector = i.value();

            const auto geoPoly = d->map.sectorPolygon(secId);

            // Determine corners.
            {
                QVector<QLineF> cornerLines;
                const Plane &ceiling = d->map.ceilingPlane(secId);
                const Plane &floor   = d->map.floorPlane(secId);
                for (int i = 0; i < geoPoly.size(); ++i)
                {
                    // Check the bottom-most floor positions.
                    {
                        const Vec3d fpos  = floor.projectPoint(Point{geoPoly.at(i)});
                        const ID    pid   = geoPoly.points[i].id;

                        auto found = d->floorPoints.find(pid);
                        if (found == d->floorPoints.end())
                        {
                            d->floorPoints.insert(pid, fpos);
                        }
                        else
                        {
                            found.value().y = std::min(found.value().y, fpos.y);
                        }
                    }

                    cornerLines << QLineF(d->worldToView(Point{geoPoly.at(i)}, &floor),
                                          d->worldToView(Point{geoPoly.at(i)}, &ceiling));
                }
                ptr.setPen(QPen(verticalLineColor));
                ptr.drawLines(cornerLines.constData(), cornerLines.size());
            }

            if (d->selection.contains(secId))
            {
                ptr.setPen(QPen(selectColor, 4));
            }
            else
            {
                ptr.setPen(Qt::NoPen);
            }

            QPolygonF poly;
            for (int vol = 0; vol < sector.volumes.size(); ++vol)
            {
                for (int planeIndex = 0; planeIndex < 2; ++planeIndex)
                {
                    if (vol < sector.volumes.size() - 1 && planeIndex > 0) continue;

                    const ID     planeId = d->map.volume(sector.volumes.at(vol)).planes[planeIndex];
                    const Plane &secPlane = mapPlanes[planeId];

                    poly.clear();
                    for (const auto &pp : geoPoly.points)
                    {
                        poly.append(d->worldToView(Point{pp.pos}, &secPlane));
                    }

                    ptr.setBrush(d->hoverSector == secId ? panelBg : sectorColor);

                    if (d->mode == EditPlanes)
                    {
                        if (d->selection.contains(planeId))
                        {
                            ptr.setBrush(selectColor);
                        }
                        else if (d->hoverPlane == planeId)
                        {
                            ptr.setBrush(panelBg);
                        }
                        else
                        {
                            ptr.setBrush(sectorColor);
                        }
                    }

                    ptr.drawPolygon(poly);
                }
            }
            if (d->selection.contains(secId))
            {
                d->drawMetaLabel(ptr, poly.boundingRect().center(), String::format("%X", secId));
            }
        }
    }

    // Points.
    if (!mapPoints.isEmpty())
    {
        ptr.setPen(d->metaColor);
        ptr.setFont(d->metaFont);

        QVector<QPointF> points;
        QVector<QRectF>  selected;
        QVector<ID>      selectedIds;

        for (auto i = mapPoints.begin(), end = mapPoints.end(); i != end; ++i)
        {
            const ID      id  = i.key();
            const QPointF pos = d->viewPoint(id);

            points << pos;

            // Indicate selected points.
            if (d->selection.contains(id))
            {
                selected << QRectF(pos - QPointF(gap, gap), QSizeF(2*gap, 2*gap));
                selectedIds << id;
            }
        }
        ptr.setFont(font());

        ptr.setPen(QPen(pointColor, 4));
        ptr.drawPoints(&points[0], points.size());

        if (selected.size())
        {
            ptr.setPen(QPen(selectColorAlpha));
            ptr.setBrush(Qt::NoBrush);
            ptr.drawRects(&selected[0], selected.size());

            // Show ID numbers.
            for (int i = 0; i < selected.size(); ++i)
            {
                d->drawMetaLabel(ptr,
                                 selected[i].center() - QPointF(0, 2 * gap),
                                 String::format("%X", selectedIds[i]));
            }
        }
    }

    // Lines.
    if (!mapLines.isEmpty())
    {
        ptr.setPen(lineColor);

        QVector<QLineF> lines;
        QVector<QLineF> selected;
        QVector<ID>     selectedIds;

        for (auto i = mapLines.begin(); i != mapLines.end(); ++i)
        {
            auto vline = d->viewLine(i.value());
            lines << vline;

            if (d->selection.contains(i.key()))
            {
                selected << vline;
                selectedIds << i.key();
            }
        }
        ptr.drawLines(&lines[0], lines.size());

        if ((d->mode == EditLines || d->mode == EditSectors) && d->hoverLine)
        {
            const QLineF vl = d->viewLine(mapLines[d->hoverLine]);
            ptr.setPen(QPen(lineColor, 2));
            d->drawArrow(ptr, vl.p1(), vl.p2());
        }

        if (selected.size())
        {
            ptr.setPen(QPen(selectColor, 3));
            ptr.drawLines(&selected[0], selected.size());

            for (int i = 0; i < selected.size(); ++i)
            {
                const auto &line = mapLines[selectedIds[i]];
                const auto normal = selected[i].normalVector();
                QPointF delta{normal.dx(), normal.dy()};

                d->drawMetaLabel(ptr, selected[i].center(), String::format("%X", selectedIds[i]));

                if (normal.length() > 80)
                {
                    delta /= normal.length();

                    //if (line.sectors[0])
                        d->drawMetaLabel(ptr,
                                         selected[i].center() + delta * -20,
                                         String::format("%X", line.surfaces[0].sector), false);
                    if (line.surfaces[1].sector)
                        d->drawMetaLabel(ptr,
                                         selected[i].center() + delta * 20,
                                         String::format("%X", line.surfaces[1].sector), false);
                }
            }
        }
    }

    // Entities.
    {
        const QFontMetrics metrics(d->metaFont);
        ptr.setPen(Qt::black);
        ptr.setFont(d->metaFont);

        for (auto i = mapEnts.begin(), end = mapEnts.end(); i != end; ++i)
        {
            const auto &  ent = i.value();
            const QPointF pos = d->worldToView(ent->position());

            float radius = 0.5f * d->viewScale;
            ptr.setBrush(d->selection.contains(i.key())? selectColor : QColor(Qt::white));
            ptr.drawEllipse(pos, radius, radius);

            ptr.drawText(pos + QPointF(radius + 5, metrics.ascent() / 2), d->entityLabel(*ent));
        }

        ptr.setBrush(Qt::NoBrush);
        const Point mousePos = d->worldMousePoint();
        ptr.drawEllipse(d->worldToView(mousePos), 5, 5);

        ptr.setFont(font());
    }

    // Status bar.
    {
        const int statusHgt = lineHgt + 2*gap;
        const QRect rect{0, winRect.height() - statusHgt, winRect.width(), statusHgt};
        const QRect content = rect.adjusted(gap, gap, -gap, -gap);

        ptr.setBrush(QBrush(panelBg));
        ptr.setPen(Qt::NoPen);
        ptr.drawRect(rect);

        ptr.setBrush(Qt::NoBrush);
        ptr.setPen(textColor);
        const int y = content.center().y() + fontMetrics.ascent()/2;
        ptr.drawText(content.left(), y, d->statusText());

        const auto mouse = d->worldMousePoint();
        const String viewText = String("[%1 %2] (%3 %4) z:%5")
                .arg(mouse.coord.x,   0, 'f', 1)
                .arg(mouse.coord.y,   0, 'f', 1)
                .arg(d->viewOrigin.x, 0, 'f', 1)
                .arg(d->viewOrigin.y, 0, 'f', 1)
                .arg(d->viewScale,    0, 'f', 2);
        ptr.drawText(content.right() - fontMetrics.width(viewText), y, viewText);
    }

    // Current selection.
    if (d->userAction == Impl::SelectRegion)
    {
        ptr.setPen(selectColor);
        ptr.setBrush(Qt::NoBrush);
        ptr.drawRect(d->selectRect);
    }

    // Line connection indicator.
    if (d->userAction == Impl::AddLines)
    {
        const QColor invalidColor{200, 0, 0};
        const QColor validColor{0, 200, 0};

        if (d->selection.size())
        {
            const auto startPos = d->worldToView(d->map.point(*d->selection.begin()));
            const auto endPos   = d->viewMousePos();

            ptr.setPen(QPen(d->hoverPoint? validColor : invalidColor, 2));
            d->drawArrow(ptr, startPos, endPos);
        }
    }
}

void Editor::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    d->actionPos = event->pos();
}

void Editor::mouseMoveEvent(QMouseEvent *event)
{
    // Check what the mouse is hovering on.
    {
        const auto pos = d->viewToWorldPoint(event->pos());

        d->hoverPoint  = d->findPointAt(event->pos());
        d->hoverLine   = d->findLineAt(event->pos());
        d->hoverSector = (d->mode == EditSectors || d->mode == EditVolumes ? d->findSectorAt(pos) : 0);
        d->hoverPlane  = (d->mode == EditPlanes  ? d->findPlaneAtViewPos(event->pos()) : 0);
        d->hoverEntity = d->findEntityAt(event->pos());
    }

    // Begin a drag action.
    if (event->buttons() && d->userAction == Impl::None &&
        (event->pos() - d->actionPos).manhattanLength() >= DRAG_MIN_DIST)
    {
        if (event->buttons() & Qt::LeftButton)
        {
            if (event->modifiers() & Qt::ShiftModifier)
            {
                d->beginAction(Impl::SelectRegion);
                update();
            }
            else
            {
                if (d->selection.size() <= 1)
                {
                    d->selection.clear();
                    d->selectClickedObject(event->modifiers());
                }
                if (!d->selection.isEmpty())
                {
                    d->beginAction(Impl::Move);
                    update();
                }
            }
        }
        if ((event->modifiers() & Qt::ShiftModifier) && (event->buttons() & Qt::RightButton))
        {
            // Translate the view.
            d->beginAction(Impl::TranslateView);
            update();
        }
    }

    switch (d->userAction)
    {
    case Impl::TranslateView: {
        QPoint delta = event->pos() - d->actionPos;
        d->actionPos = event->pos();
        d->viewOrigin -= Vec2f(delta.x(), delta.y()) / d->viewScale;
        d->updateView();
        break;
    }
    case Impl::SelectRegion:
        d->selectRect = QRect(d->actionPos, event->pos());
        break;

    case Impl::Move: {
        if (d->mode == EditPoints || d->mode == EditEntities ||
            d->mode == EditPlanes)
        {
            QPoint delta = event->pos() - d->actionPos;
            d->actionPos = event->pos();

            const Vec2d worldDelta = Vec2d(delta.x(), delta.y()) / d->viewScale;
            for (auto id : d->selection)
            {
                if (d->mode == EditPoints && d->map.points().contains(id))
                {
                    d->map.point(id).coord += worldDelta;
                }
                else if (d->mode == EditEntities && d->map.entities().contains(id))
                {
                    auto &ent = d->map.entity(id);
                    ent.setPosition(ent.position() + Vec3d(worldDelta.x, 0, worldDelta.y));
                }
                else if (d->mode == EditPlanes && d->map.planes().contains(id))
                {
                    d->map.plane(id).point.y -= worldDelta.y;
                }
            }
        }
        break;
    }
    case Impl::Rotate:
    case Impl::Scale:
    {
        QPoint delta = event->pos() - d->actionPos;
        d->actionPos = event->pos();

        Mat4f xf;

        if (d->userAction == Impl::Rotate)
        {
            const auto  pivot = d->viewToWorldPoint(d->pivotPos);
            const float angle = delta.y() / 2.f;

            xf = Mat4f::rotateAround(
                Vec3f(float(pivot.coord.x), float(pivot.coord.y)), angle, Vec3f(0, 0, 1));
        }
        else
        {
            const Vec3d pivot = d->viewToWorldPoint(d->pivotPos).coord;
            Vec3f scaler(1 + delta.x()/100.f, 1 + delta.y()/100.f);
            if (!(event->modifiers() & Qt::AltModifier)) scaler.y = scaler.x;
            xf = Mat4f::translate(pivot) * Mat4f::scale(scaler) * Mat4f::translate(-pivot);
        }

        for (auto id : d->selection)
        {
            if (d->map.points().contains(id))
            {
                d->map.point(id).coord = xf * Vec3d(d->map.point(id).coord);
            }
        }
        break;
    }
    default:
        break;
    }

    update();
}

void Editor::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();

    if (d->mode == EditEntities && event->button() == Qt::RightButton)
    {
        d->hoverEntity = d->findEntityAt(event->pos());

        if (d->hoverEntity)
        {
            QMenu *pop = new QMenu(this);
            auto *header = pop->addAction(QString("Entity %1").arg(d->hoverEntity, 0, 16));
            header->setDisabled(true);

            QMenu *eType = pop->addMenu("Type");
            const ID entityId = d->hoverEntity;
            for (auto i = entityMetadata.begin(), end = entityMetadata.end(); i != end; ++i)
            {
                /*QAction *a = */ eType->addAction(i.value(), [this, entityId, i] () {
                    d->map.entity(entityId).setType(i.key());
                });
            }
            pop->popup(mapToGlobal(event->pos()));
            connect(pop, &QMenu::aboutToHide, [pop] () { pop->deleteLater(); });
        }
    }

    if (d->userAction != Impl::None && d->userAction != Impl::AddLines)
    {
        d->finishAction();
        update();
    }
    else
    {
        if ((event->pos() - d->actionPos).manhattanLength() < DRAG_MIN_DIST)
        {
            d->userClick(event->modifiers());
            update();
        }
    }
}

void Editor::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();

    if (d->hoverLine && (d->mode == EditLines || d->mode == EditPoints))
    {
        d->splitLine(d->hoverLine, d->viewToWorldPoint(event->pos()).coord);
    }
}

void Editor::wheelEvent(QWheelEvent *event)
{
    const QPoint delta = event->pixelDelta();
    if (event->modifiers() & Qt::ControlModifier)
    {
        d->viewYawAngle   += delta.x() * .25f;
        d->viewPitchAngle += delta.y() * .25f;
    }
    else if (event->modifiers() & Qt::ShiftModifier)
    {
        d->viewScale *= de::clamp(.1f, 1.f - delta.y()/1000.f, 10.f);
    }
    else
    {
        d->viewOrigin -= Mat4f::rotate(d->viewYawAngle, Vec3f(0, 0, 1)) *
                         Vec2f(delta.x(), delta.y()) / d->viewScale;
    }
    d->updateView();
    update();
}
