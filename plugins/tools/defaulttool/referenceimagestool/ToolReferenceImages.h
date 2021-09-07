/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef TOOL_REFERENCE_IMAGES_H
#define TOOL_REFERENCE_IMAGES_H

#include <QPointer>

#include <KoToolFactoryBase.h>
#include <KoIcon.h>

#include <kis_tool.h>
#include "kis_painting_assistant.h"
#include <kis_icon.h>
#include <kis_canvas2.h>
#include "KisReferenceImageSelectionDecorator.h"

#include <defaulttool/DefaultTool.h>
#include <defaulttool/DefaultToolFactory.h>

class ToolReferenceImagesWidget;
class KisReferenceImagesLayer;

class ToolReferenceImages : public DefaultTool
{
    Q_OBJECT

public:
    ToolReferenceImages(KoCanvasBase * canvas);
    ~ToolReferenceImages() override;

    virtual quint32 priority() {
        return 3;
    }

    void paint(QPainter &painter, const KoViewConverter &/*converter*/) override;

    void mouseDoubleClickEvent(KoPointerEvent */*event*/) override {}
    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;

    void deleteSelection() override;

    QMenu* popupActionsMenu() override;

    KisReferenceImage* activeReferenceImage();

Q_SIGNALS:
    void cropRectChanged();
protected:
    QList<QPointer<QWidget>> createOptionWidgets() override;
    QWidget *createOptionWidget() override;

    bool isValidForCurrentLayer() const override;
    KoShapeManager *shapeManager() const override;
    KoSelection *koSelection() const override;

    void updateDistinctiveActions(const QList<KoShape*> &editableShapes) override;

    KoInteractionStrategy *createStrategy(KoPointerEvent *event) override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;

    void addReferenceImage();
    void pasteReferenceImage();
    void removeAllReferenceImages();
    void saveReferenceImages();
    void loadReferenceImages();

    void slotNodeAdded(KisNodeSP node);
    void slotSelectionChanged();

    void cut() override;
    void copy() const override;
    bool paste() override;



private:
    KisDocument *document() const;
    void setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer);

    KoFlake::SelectionHandle handleAt(const QPointF &point, bool *innerHandleMeaning = 0) override;
    void recalcCropHandles(KisReferenceImage *referenceImage);
    void updateCursor();
    QRectF handlesSize() override;
    void updateReferenceImages();

    friend class ToolReferenceImagesWidget;
    ToolReferenceImagesWidget *m_optionsWidget = nullptr;
    KisWeakSharedPtr<KisReferenceImagesLayer> m_layer;
    KisReferenceImageSelectionDecorator* m_cropDecorator;
    KoFlake::SelectionHandle m_lastHandle;
    bool m_mouseWasInsideHandles;
    QPolygonF m_outline;
    QPointF m_cropHandles[8];
    QCursor m_sizeCursors[8];
};


class ToolReferenceImagesFactory : public DefaultToolFactory
{
public:
    ToolReferenceImagesFactory()
    : DefaultToolFactory("ToolReferenceImages") {
        setToolTip(i18n("Reference Images Tool"));
        setSection(ToolBoxSection::View);
        setIconName(koIconNameCStr("krita_tool_reference_images"));
        setPriority(2);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    };


    ~ToolReferenceImagesFactory() override {}

    KoToolBase * createTool(KoCanvasBase * canvas) override {
        return new ToolReferenceImages(canvas);
    }

    QList<QAction *> createActionsImpl() override;

};


#endif

