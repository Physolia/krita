/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "FisheyePointAssistant.h"

#include "kis_debug.h"
#include <klocalizedstring.h>

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QTransform>

#include <kis_canvas2.h>
#include <kis_coordinates_converter.h>
#include <kis_algebra_2d.h>

#include <math.h>
#include <limits>

FisheyePointAssistant::FisheyePointAssistant()
    : KisPaintingAssistant("fisheye-point", i18n("Fish Eye Point assistant"))
{
}

FisheyePointAssistant::FisheyePointAssistant(const FisheyePointAssistant &rhs, QMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , e(rhs.e)
    , extraE(rhs.extraE)
{
}

KisPaintingAssistantSP FisheyePointAssistant::clone(QMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new FisheyePointAssistant(*this, handleMap));
}

QPointF FisheyePointAssistant::project(const QPointF& pt, const QPointF& strokeBegin, qreal moveThresholdPt)
{
    const static QPointF nullPoint(std::numeric_limits<qreal>::quiet_NaN(), std::numeric_limits<qreal>::quiet_NaN());
    Q_ASSERT(isAssistantComplete());
    e.set(*handles()[0], *handles()[1], *handles()[2]);

    if (KisAlgebra2D::norm(pt - strokeBegin) < moveThresholdPt) {
        // allow some movement before snapping
        return strokeBegin;
    }

    //set the extrapolation ellipse.
    if (e.set(*handles()[0], *handles()[1], *handles()[2])){
        QLineF radius(*handles()[1], *handles()[0]);
        radius.setAngle(fmod(radius.angle()+180.0,360.0));
        QLineF radius2(*handles()[0], *handles()[1]);
        radius2.setAngle(fmod(radius2.angle()+180.0,360.0));
        if ( extraE.set(*handles()[0], *handles()[1],strokeBegin ) ) {
            return extraE.project(pt);
        } else if (extraE.set(radius.p1(), radius.p2(),strokeBegin)) {
            return extraE.project(pt);
        } else if (extraE.set(radius2.p1(), radius2.p2(),strokeBegin)){
            return extraE.project(pt);
        }
    }

    return nullPoint;

}

QPointF FisheyePointAssistant::adjustPosition(const QPointF& pt, const QPointF& strokeBegin, const bool /*snapToAny*/, qreal moveThresholdPt)
{
    return project(pt, strokeBegin, moveThresholdPt);
}

void FisheyePointAssistant::adjustLine(QPointF &point, QPointF &strokeBegin)
{
    point = QPointF();
    strokeBegin = QPointF();
}

void FisheyePointAssistant::drawAssistant(QPainter& gc, const QRectF& updateRect, const KisCoordinatesConverter* converter, bool cached, KisCanvas2* canvas, bool assistantVisible, bool previewVisible)
{
    gc.save();
    gc.resetTransform();

    QPointF mousePos(0,0);

    if (canvas){
        //simplest, cheapest way to get the mouse-position//
        mousePos= canvas->canvasWidget()->mapFromGlobal(QCursor::pos());
    }
    else {
        //...of course, you need to have access to a canvas-widget for that.//
        mousePos = QCursor::pos();//this'll give an offset//
        dbgFile<<"canvas does not exist in ruler, you may have passed arguments incorrectly:"<<canvas;
    }


    if (isSnappingActive() && previewVisible == true ) {

        if (isAssistantComplete()){

            QTransform initialTransform = converter->documentToWidgetTransform();

            if (m_followBrushPosition && m_adjustedPositionValid) {
                mousePos = initialTransform.map(m_adjustedBrushPosition);
            }

            if (e.set(*handles()[0], *handles()[1], *handles()[2])) {
                if (extraE.set(*handles()[0], *handles()[1], initialTransform.inverted().map(mousePos))){
                    gc.setTransform(initialTransform);
                    gc.setTransform(e.getInverse(), true);
                    QPainterPath path;
                    // Draw the ellipse
                    path.addEllipse(QPointF(0, 0), extraE.semiMajor(), extraE.semiMinor());
                    drawPreview(gc, path);
                }
                QLineF radius(*handles()[1], *handles()[0]);
                radius.setAngle(fmod(radius.angle()+180.0,360.0));
                if (extraE.set(radius.p1(), radius.p2(), initialTransform.inverted().map(mousePos))){
                    gc.setTransform(initialTransform);
                    gc.setTransform(extraE.getInverse(), true);
                    QPainterPath path;
                    // Draw the ellipse
                    path.addEllipse(QPointF(0, 0), extraE.semiMajor(), extraE.semiMinor());
                    drawPreview(gc, path);
                }
                QLineF radius2(*handles()[0], *handles()[1]);
                radius2.setAngle(fmod(radius2.angle()+180.0,360.0));
                if (extraE.set(radius2.p1(), radius2.p2(), initialTransform.inverted().map(mousePos))){
                    gc.setTransform(initialTransform);
                    gc.setTransform(extraE.getInverse(), true);
                    QPainterPath path;
                    // Draw the ellipse
                    path.addEllipse(QPointF(0, 0), extraE.semiMajor(), extraE.semiMinor());
                    drawPreview(gc, path);
                }

            }
        }
    }
    gc.restore();

    KisPaintingAssistant::drawAssistant(gc, updateRect, converter, cached, canvas, assistantVisible, previewVisible);

}

void FisheyePointAssistant::drawCache(QPainter& gc, const KisCoordinatesConverter *converter, bool assistantVisible)
{
    if (assistantVisible == false){
        return;
    }

    QTransform initialTransform = converter->documentToWidgetTransform();

    if (handles().size() == 2) {
        // just draw the axis
        gc.setTransform(initialTransform);
        QPainterPath path;
        path.moveTo(*handles()[0]);
        path.lineTo(*handles()[1]);
        drawPath(gc, path, isSnappingActive());
        return;
    }
    if (e.set(*handles()[0], *handles()[1], *handles()[2])) {
        // valid ellipse

        gc.setTransform(initialTransform);
        gc.setTransform(e.getInverse(), true);
        QPainterPath path;
        //path.moveTo(QPointF(-e.semiMajor(), -e.semiMinor())); path.lineTo(QPointF(e.semiMajor(), -e.semiMinor()));
        path.moveTo(QPointF(-e.semiMajor(), -e.semiMinor())); path.lineTo(QPointF(-e.semiMajor(), e.semiMinor()));
        //path.moveTo(QPointF(-e.semiMajor(), e.semiMinor())); path.lineTo(QPointF(e.semiMajor(), e.semiMinor()));
        path.moveTo(QPointF(e.semiMajor(), -e.semiMinor())); path.lineTo(QPointF(e.semiMajor(), e.semiMinor()));
        path.moveTo(QPointF(-(e.semiMajor()*3), -e.semiMinor())); path.lineTo(QPointF(-(e.semiMajor()*3), e.semiMinor()));
        path.moveTo(QPointF((e.semiMajor()*3), -e.semiMinor())); path.lineTo(QPointF((e.semiMajor()*3), e.semiMinor()));
        path.moveTo(QPointF(-e.semiMajor(), 0)); path.lineTo(QPointF(e.semiMajor(), 0));
        //path.moveTo(QPointF(0, -e.semiMinor())); path.lineTo(QPointF(0, e.semiMinor()));
        // Draw the ellipse
        path.addEllipse(QPointF(0, 0), e.semiMajor(), e.semiMinor());
        drawPath(gc, path, isSnappingActive());
    }

}

QRect FisheyePointAssistant::boundingRect() const
{
    if (!isAssistantComplete()) {
        return KisPaintingAssistant::boundingRect();
    }

    if (e.set(*handles()[0], *handles()[1], *handles()[2])) {
        return e.boundingRect().adjusted(-(e.semiMajor()*2), -2, (e.semiMajor()*2), 2).toAlignedRect();
    } else {
        return QRect();
    }
}

QPointF FisheyePointAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0] + *handles()[1]) * 0.5;
}

bool FisheyePointAssistant::isAssistantComplete() const
{
    return handles().size() >= 3;
}


FisheyePointAssistantFactory::FisheyePointAssistantFactory()
{
}

FisheyePointAssistantFactory::~FisheyePointAssistantFactory()
{
}

QString FisheyePointAssistantFactory::id() const
{
    return "fisheye-point";
}

QString FisheyePointAssistantFactory::name() const
{
    return i18n("Fish Eye Point");
}

KisPaintingAssistant* FisheyePointAssistantFactory::createPaintingAssistant() const
{
    return new FisheyePointAssistant;
}
