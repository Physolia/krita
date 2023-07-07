#ifndef WDGTAGPREVIEW_H
#define WDGTAGPREVIEW_H

#include "KisTagSelectionWidget.h"
#include "KoID.h"
#include <KisTag.h>

#include <QWidget>

class KisTag;
class KisWdgTagSelectionControllerBundleTags;

namespace Ui {
class WdgTagPreview;
}

class WdgTagPreview : public QWidget
{
    Q_OBJECT


Q_SIGNALS:
    void tagsAdded(KisTagSP tagSP);
    void tagsRemoved(KisTagSP tagSP);

public:
    explicit WdgTagPreview(QString resourceType, QWidget *parent = nullptr);
    ~WdgTagPreview();

public Q_SLOTS:
    void onTagAdded(KoID custom);
    void onTagRemoved(KoID custom);

private:
    Ui::WdgTagPreview *m_ui;

    QString m_resourceType;
    KisWdgTagSelectionControllerBundleTags *m_tagsController;
    KisTagSelectionWidget* m_wdgResourcesTags;
    QLayout* m_layout {nullptr};
};

#endif // WDGTAGPREVIEW_H
