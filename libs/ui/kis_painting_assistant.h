/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_PAINTING_ASSISTANT_H_
#define _KIS_PAINTING_ASSISTANT_H_

#include <QString>
#include <QPointF>
#include <QRect>
#include <QFile>
#include <QObject>
#include <QColor>
#include <QXmlStreamWriter>
#include <QMap>

#include <kritaui_export.h>
#include <kis_shared.h>
#include <kis_types.h>

class QPainter;
class QRect;
class QRectF;
class KoStore;
class KisCoordinatesConverter;
class KisCanvas2;
class QDomDocument;
class QDomElement;

#include <kis_shared_ptr.h>
#include <KoGenericRegistry.h>

class KisPaintingAssistantHandle;
typedef KisSharedPtr<KisPaintingAssistantHandle> KisPaintingAssistantHandleSP;
class KisPaintingAssistant;
class QPainterPath;

enum HandleType {
    NORMAL,
    SIDE,
    CORNER,
    VANISHING_POINT,
    ANCHOR
};


/**
  * Represent an handle of the assistant, used to edit the parameters
  * of an assistants. Handles can be shared between assistants.
  */
class KRITAUI_EXPORT KisPaintingAssistantHandle : public QPointF, public KisShared
{
    friend class KisPaintingAssistant;

public:
    KisPaintingAssistantHandle(double x, double y);
    explicit KisPaintingAssistantHandle(QPointF p);
    KisPaintingAssistantHandle(const KisPaintingAssistantHandle&);
    ~KisPaintingAssistantHandle();
    void mergeWith(KisPaintingAssistantHandleSP);
    void uncache();
    KisPaintingAssistantHandle& operator=(const QPointF&);
    void setType(char type);
    char handleType() const;

    /**
     * Returns the pointer to the "chief" assistant,
     * which is supposed to handle transformations of the
     * handle, when all the assistants are transformed
     */
    KisPaintingAssistant* chiefAssistant() const;

private:
    void registerAssistant(KisPaintingAssistant*);
    void unregisterAssistant(KisPaintingAssistant*);
    bool containsAssistant(KisPaintingAssistant*) const;

private:
    struct Private;
    Private* const d;
};

/**
 * A KisPaintingAssistant is an object that assist the drawing on the canvas.
 * With this class you can implement virtual equivalent to ruler or compass.
 */
class KRITAUI_EXPORT KisPaintingAssistant
{
public:
    KisPaintingAssistant(const QString& id, const QString& name);
    virtual ~KisPaintingAssistant();
    virtual KisPaintingAssistantSP clone(QMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const = 0;
    const QString& id() const;
    const QString& name() const;
    bool isSnappingActive() const;
    void setSnappingActive(bool set);
    //copy SharedData from an assistant to this
    void copySharedData(KisPaintingAssistantSP assistant);


    /**
     * Adjust the position given in parameter.
     * @param point the coordinates in point in the document reference
     * @param strokeBegin the coordinates of the beginning of the stroke
     * @param snapToAny because now assistants can be composited out of multiple inside assistants.
     *         snapToAny true means that you can use any of the inside assistant, while it being false
     *         means you should use the last used one. The logic determining when it happens (first stroke etc.)
     *         is in the decoration, so those two options are enough.
     * @param moveThresholdPt the threshold for the "move" of the cursor measured in pt
     *                        (usually equals to 2px in screen coordinates converted to pt)
     */
    virtual QPointF adjustPosition(const QPointF& point, const QPointF& strokeBegin, bool snapToAny, qreal moveThresholdPt) = 0;
    virtual void adjustLine(QPointF& point, QPointF& strokeBegin) = 0;
    virtual void endStroke();
    virtual void setAdjustedBrushPosition(const QPointF position);
    virtual void setFollowBrushPosition(bool follow);
    virtual QPointF getDefaultEditorPosition() const = 0; // Returns standard editor widget position for this assistant
    virtual QPointF getEditorPosition() const; // Returns editor widget position in document-space coordinates.
    virtual int numHandles() const = 0;

    /**
     * @brief canBeLocal
     * @return if the assistant can be potentially a "local assistant" (limited to rectangular area) or not
     */
    virtual bool canBeLocal() const;
    /**
     * @brief isLocal
     * @return if the assistant is limited to a rectangular area or not
     */
    bool isLocal() const;
    /**
     * @brief setLocal
     * @param value set the indication if the assistant is limited to a rectangular area or not
     */
    void setLocal(bool value);

    /**
     * @brief isLocked
     * @return if the assistant is locked (= cannot be moved, or edited in any way), or not
     */
    bool isLocked();
    /**
     * @brief setLocked
     * @param value set the indication if the assistant is locked (= cannot be moved, or edited in any way) or not
     */
    void setLocked(bool value);
    /**
     * @brief isDuplicating
     * @return If the duplication button is pressed
     */
    /*The duplication button must be depressed when the user clicks it. This getter function indicates to the 
    render function when the button is clicked*/
    bool isDuplicating();
    /**
     * @brief setDuplicating
     * @param value setter function sets the indication that the duplication button is pressed
     */
    void setDuplicating(bool value);

    QPointF editorWidgetOffset();
    void setEditorWidgetOffset(QPointF offset);

    void replaceHandle(KisPaintingAssistantHandleSP _handle, KisPaintingAssistantHandleSP _with);
    void addHandle(KisPaintingAssistantHandleSP handle, HandleType type);

    QPointF viewportConstrainedEditorPosition(const KisCoordinatesConverter* converter, const QSize editorSize);

    QColor effectiveAssistantColor() const;
    bool useCustomColor();
    void setUseCustomColor(bool useCustomColor);
    void setAssistantCustomColor(QColor color);
    QColor assistantCustomColor();
    void setAssistantGlobalColorCache(const QColor &color);

    virtual void drawAssistant(QPainter& gc, const QRectF& updateRect, const KisCoordinatesConverter *converter, bool cached = true,KisCanvas2 *canvas=0, bool assistantVisible=true, bool previewVisible=true);
    void uncache();
    const QList<KisPaintingAssistantHandleSP>& handles() const;
    QList<KisPaintingAssistantHandleSP> handles();
    const QList<KisPaintingAssistantHandleSP>& sideHandles() const;
    QList<KisPaintingAssistantHandleSP> sideHandles();

    QByteArray saveXml( QMap<KisPaintingAssistantHandleSP, int> &handleMap);
    virtual void saveCustomXml(QXmlStreamWriter* xml); //in case specific assistants have custom properties (like vanishing point)

    void loadXml(KoStore *store, QMap<int, KisPaintingAssistantHandleSP> &handleMap, QString path);
    virtual bool loadCustomXml(QXmlStreamReader* xml);

    void saveXmlList(QDomDocument& doc, QDomElement& assistantsElement, int count);
    void findPerspectiveAssistantHandleLocation();
    KisPaintingAssistantHandleSP oppHandleOne();

    /**
      * Get the topLeft, bottomLeft, topRight and BottomRight corners of the assistant
      * Some assistants like the perspective grid have custom logic built around certain handles
      */
    const KisPaintingAssistantHandleSP topLeft() const;
    KisPaintingAssistantHandleSP topLeft();
    const KisPaintingAssistantHandleSP topRight() const;
    KisPaintingAssistantHandleSP topRight();
    const KisPaintingAssistantHandleSP bottomLeft() const;
    KisPaintingAssistantHandleSP bottomLeft();
    const KisPaintingAssistantHandleSP bottomRight() const;
    KisPaintingAssistantHandleSP bottomRight();
    const KisPaintingAssistantHandleSP topMiddle() const;
    KisPaintingAssistantHandleSP topMiddle();
    const KisPaintingAssistantHandleSP rightMiddle() const;
    KisPaintingAssistantHandleSP rightMiddle();
    const KisPaintingAssistantHandleSP leftMiddle() const;
    KisPaintingAssistantHandleSP leftMiddle();
    const KisPaintingAssistantHandleSP bottomMiddle() const;
    KisPaintingAssistantHandleSP bottomMiddle();


    // calculates whether a point is near one of the corner points of the assistant
    // returns: a corner point from the perspective assistant if the given node is close
    // only called once in code when calculating the perspective assistant
    KisPaintingAssistantHandleSP closestCornerHandleFromPoint(QPointF point);

    // determines if two points are close to each other
    // only used by the nodeNearPoint function (perspective grid assistant).
    bool areTwoPointsClose(const QPointF& pointOne, const QPointF& pointTwo);

    /// determines if the assistant has enough handles to be considered created
    /// new assistants get in a "creation" phase where they are currently being made on the canvas
    /// it will return false if we are in the middle of creating the assistant.
    virtual bool isAssistantComplete() const;

    /// Transform the assistant using the given \p transform. Please note that \p transform
    /// should be in 'document' coordinate system.
    /// Used with image-wide transformations.
    virtual void transform(const QTransform &transform);

public:
    /**
     * This will render the final output. The drawCache does rendering most of the time so be sure to check that
     */
    void drawPath(QPainter& painter, const QPainterPath& path, bool drawActive=true);
    void drawPreview(QPainter& painter, const QPainterPath& path);
    // draw a path in a red color, signalizing incorrect state
    void drawError(QPainter& painter, const QPainterPath& path);
    // draw a vanishing point marker
    void drawX(QPainter& painter, const QPointF& pt);
    static double norm2(const QPointF& p);

protected:
    explicit KisPaintingAssistant(const KisPaintingAssistant &rhs, QMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

    virtual QRect boundingRect() const;

    /// performance layer where the graphics can be drawn from a cache instead of generated every render update
    virtual void drawCache(QPainter& gc, const KisCoordinatesConverter *converter, bool assistantVisible=true) = 0;

    void initHandles(QList<KisPaintingAssistantHandleSP> _handles);
    QList<KisPaintingAssistantHandleSP> m_handles;

    QPointF pixelToView(const QPoint pixelCoords) const;

    /**
     * @brief firstLocalHandle
     * Note: this doesn't guarantee it will be the topleft corner!
     * For that, use getLocalRect().topLeft()
     * The only purpose of those functions to exist is to be able to
     * put getLocalRect() function in the KisPaintingAssistant
     * instead of reimplementing it in every specific assistant.
     * @return the first handle of the rectangle of the limited area
     */
    virtual KisPaintingAssistantHandleSP firstLocalHandle() const;
    /**
     * @brief secondLocalHandle
     * Note: this doesn't guarantee it will be the bottomRight corner!
     * For that, use getLocalRect().bottomRight()
     * (and remember that for QRect bottomRight() works differently than for QRectF,
     * so don't convert to QRect before accessing the corner)
     * @return
     */
    virtual KisPaintingAssistantHandleSP secondLocalHandle() const;
    /**
     * @brief getLocalRect
     * The function deals with local handles not being topLeft and bottomRight
     * gracefully and returns a correct rectangle.
     * Thanks to that the user can place handles in a "wrong" order or move them around
     * but the local rectangle will still be correct.
     * @return the rectangle of the area that the assistant is limited to
     */
    QRectF getLocalRect() const;


public:
    /// clones the list of assistants
    /// the originally shared handles will still be shared
    /// the cloned assistants do not share any handle with the original assistants
    static QList<KisPaintingAssistantSP> cloneAssistantList(const QList<KisPaintingAssistantSP> &list);

protected:

    bool m_followBrushPosition {false};
    bool m_adjustedPositionValid {false};
    QPointF m_adjustedBrushPosition;

    bool m_hasBeenInsideLocalRect {false};

private:
    struct Private;
    Private* const d;

};

/**
 * Allow to create a painting assistant.
 */
class KRITAUI_EXPORT KisPaintingAssistantFactory
{
public:
    KisPaintingAssistantFactory();
    virtual ~KisPaintingAssistantFactory();
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual KisPaintingAssistant* createPaintingAssistant() const = 0;

};

class KRITAUI_EXPORT KisPaintingAssistantFactoryRegistry : public KoGenericRegistry<KisPaintingAssistantFactory*>
{
  public:
    KisPaintingAssistantFactoryRegistry();
    ~KisPaintingAssistantFactoryRegistry() override;

    static KisPaintingAssistantFactoryRegistry* instance();

};

#endif
