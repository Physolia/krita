/*
 *  SPDX-FileCopyrightText: 2019 Shi Yan <billconan@gmail.net>
 *  SPDX-FileCopyrightText: 2020 Dmitrii Utkin <loentar@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#include "recorderdocker_dock.h"
#include "recorder_config.h"
#include "recorder_writer.h"
#include "ui_recorderdocker.h"
#include "recorder_snapshots_manager.h"
#include "recorder_export.h"
#include "recorder_export_settings.h"
#include "recorder_export_config.h"

#include <klocalizedstring.h>
#include <kis_action_registry.h>
#include <kis_canvas2.h>
#include <kis_icon_utils.h>
#include <kis_statusbar.h>
#include <KisDocument.h>
#include <KisViewManager.h>
#include <KoDocumentInfo.h>
#include <kactioncollection.h>
#include <KisPart.h>
#include <KisKineticScroller.h>
#include "KisMainWindow.h"

#include <QFileInfo>
#include <QPointer>
#include <QFileDialog>
#include <QMessageBox>

namespace
{
const QString keyActionRecordToggle = "recorder_record_toggle";
const QString keyActionExport = "recorder_export";
}


class RecorderDockerDock::Private
{
public:
    RecorderDockerDock *const q;
    QScopedPointer<Ui::RecorderDocker> ui;
    QPointer<KisCanvas2> canvas;
    RecorderWriter writer;

    QAction *recordToggleAction = nullptr;
    QAction *exportAction = nullptr;

    QString snapshotDirectory;
    QString prefix;
    QString outputDirectory;
    double captureInterval = 0.;
    RecorderFormat format = RecorderFormat::JPEG;
    int quality = 0;
    int compression = 0;
    int resolution = 0;
    bool realTimeCaptureMode = false;
    bool recordIsolateLayerMode = false;
    bool recordAutomatically = false;

    QLabel* statusBarLabel;
    QLabel* statusBarWarningLabel;
    QTimer warningTimer;

    QMap<QString, bool> enabledIds;

    Private(const RecorderExportSettings &es, RecorderDockerDock *q_ptr)
        : q(q_ptr)
        , ui(new Ui::RecorderDocker())
        , writer(es)
        , statusBarLabel(new QLabel())
        , statusBarWarningLabel(new QLabel())
    {
        updateRecIndicator(false);
        statusBarWarningLabel->setPixmap(KisIconUtils::loadIcon("warning").pixmap(16, 16));
        statusBarWarningLabel->hide();
        warningTimer.setInterval(10000);
        warningTimer.setSingleShot(true);
        connect(&warningTimer, SIGNAL(timeout()), q, SLOT(onWarningTimeout()));
    }

    void loadSettings()
    {
        RecorderConfig config(true);
        snapshotDirectory = config.snapshotDirectory();
        captureInterval = config.captureInterval();
        format = config.format();
        quality = config.quality();
        compression = config.compression();
        resolution = config.resolution();
        realTimeCaptureMode = config.realTimeCaptureMode();
        if (realTimeCaptureMode) {
            q->exportSettings->lockFps = true;
            q->exportSettings->realTimeCaptureModeWasSet = true;
        }
        recordIsolateLayerMode = config.recordIsolateLayerMode();
        recordAutomatically = config.recordAutomatically();

        updateUiFormat();
    }

    void loadRelevantExportSettings()
    {
        RecorderExportConfig config(true);
        q->exportSettings->fps = config.fps();
    }

    void updateUiFormat() {
        int index = 0;
        QString title;
        QString hint;
        int minValue = 0;
        int maxValue = 0;
        QString suffix;
        int factor = 0;
        switch (format) {
            case RecorderFormat::JPEG:
                index = 0;
                title = i18nc("Title for label. JPEG Quality level", "Quality:");
                hint = i18nc("@tooltip", "Greater value will produce a larger file and a better quality. Doesn't affect CPU consumption.\nValues lower than 50 are not recommended due to high artifacts.");
                minValue = 1;
                maxValue = 100;
                suffix = "%";
                factor = quality;
                break;
            case RecorderFormat::PNG:
                index = 1;
                title = i18nc("Title for label. PNG Compression level", "Compression:");
                hint = i18nc("@tooltip", "Greater value will produce a smaller file but will require more from your CPU. Doesn't affect quality.\nCompression set to 0 is not recommended due to high disk space consumption.\nValues above 3 are not recommended due to high performance impact.");
                minValue = 0;
                maxValue = 5;
                suffix = "";
                factor = compression;
                break;
        }

        ui->comboFormat->setCurrentIndex(index);
        ui->labelQuality->setText(title);
        ui->spinQuality->setToolTip(hint);
        QSignalBlocker blocker(ui->spinQuality);
        ui->spinQuality->setMinimum(minValue);
        ui->spinQuality->setMaximum(maxValue);
        ui->spinQuality->setValue(factor);
        ui->spinQuality->setSuffix(suffix);
    }

    void updateUiForRealTimeMode() {
        QString title;
        double minValue = 0;
        double maxValue = 0;
        double value = 0;
        int decimals = 0;
        QString suffix;
        QSignalBlocker blocker(ui->spinRate);

        if (realTimeCaptureMode) {
                title = i18nc("Title for label. Video frames per second", "Video FPS:");
                minValue = 1;
                maxValue = 60;
                decimals = 0;
                value = q->exportSettings->fps;
                suffix = "";
                disconnect(ui->spinRate, SIGNAL(valueChanged(double)), q, SLOT(onCaptureIntervalChanged(double)));
                connect(ui->spinRate, SIGNAL(valueChanged(double)), q, SLOT(onVideoFPSChanged(double)));
        } else {
                title = i18nc("Title for label. Capture rate", "Capture interval:");
                minValue = 0.10;
                maxValue = 100.0;
                decimals = 1;
                value = captureInterval;
                suffix = " sec.";
                disconnect(ui->spinRate, SIGNAL(valueChanged(double)), q, SLOT(onVideoFPSChanged(double)));
                connect(ui->spinRate, SIGNAL(valueChanged(double)), q, SLOT(onCaptureIntervalChanged(double)));
        }

        ui->labelRate->setText(title);
        ui->spinRate->setDecimals(decimals);
        ui->spinRate->setMinimum(minValue);
        ui->spinRate->setMaximum(maxValue);
        ui->spinRate->setSuffix(suffix);
        ui->spinRate->setValue(value);
    }

    void updateWriterSettings()
    {
        outputDirectory = snapshotDirectory % QDir::separator() % prefix % QDir::separator();
        writer.setup({
            outputDirectory,
            format,
            quality,
            compression,
            resolution,
            captureInterval,
            recordIsolateLayerMode,
            realTimeCaptureMode});
    }

    QString getPrefix()
    {
        return !canvas ? ""
               : canvas->imageView()->document()->documentInfo()->aboutInfo("creation-date").remove(QRegExp("[^0-9]"));
    }

    void updateComboResolution(quint32 width, quint32 height)
    {
        const QStringList titles = {
            i18nc("Use original resolution for the frames when recording the canvas", "Original"),
            i18nc("Use the resolution two times smaller than the original resolution for the frames when recording the canvas", "Half"),
            i18nc("Use the resolution four times smaller than the original resolution for the frames when recording the canvas", "Quarter")
        };

        QStringList items;
        for (int index = 0, len = titles.length(); index < len; ++index) {
            int divider = 1 << index;
            items += QString("%1 (%2x%3)").arg(titles[index])
                    .arg((width / divider) & ~1)
                    .arg((height / divider) & ~1);
        }
        QSignalBlocker blocker(ui->comboResolution);
        const int currentIndex = ui->comboResolution->currentIndex();
        ui->comboResolution->clear();
        ui->comboResolution->addItems(items);
        ui->comboResolution->setCurrentIndex(currentIndex);
    }

    void updateRecordStatus(bool isRecording)
    {
        recordToggleAction->setChecked(isRecording);
        recordToggleAction->setEnabled(true);

        QSignalBlocker blocker(ui->buttonRecordToggle);
        ui->buttonRecordToggle->setChecked(isRecording);
        ui->buttonRecordToggle->setIcon(KisIconUtils::loadIcon(isRecording ? "media-playback-stop" : "media-record"));
        ui->buttonRecordToggle->setText(isRecording ? i18nc("Stop recording the canvas", "Stop")
                                        : i18nc("Start recording the canvas", "Record"));
        ui->buttonRecordToggle->setEnabled(true);

        ui->widgetSettings->setEnabled(!isRecording);

        statusBarLabel->setVisible(isRecording);

        if (!canvas)
            return;

        KisStatusBar *statusBar = canvas->viewManager()->statusBar();
        if (isRecording) {
            statusBar->addExtraWidget(statusBarLabel);
            statusBar->addExtraWidget(statusBarWarningLabel);
        } else {
            statusBar->removeExtraWidget(statusBarLabel);
            statusBar->removeExtraWidget(statusBarWarningLabel);
        }
    }

    void updateRecIndicator(bool paused)
    {
        // don't remove empty <font></font> tag else label will jump a few pixels around
        statusBarLabel->setText(QString("<font%1>●</font><font> %2</font>")
                                .arg(paused ? "" : " color='#da4453'").arg(i18nc("Recording symbol", "REC")));
        statusBarLabel->setToolTip(paused ? i18n("Recorder is paused") : i18n("Recorder is active"));
    }

    void showWarning(const QString &hint) {
        if (statusBarWarningLabel->isHidden()) {
            statusBarWarningLabel->setToolTip(hint);
            statusBarWarningLabel->show();
            warningTimer.start();
        }
    }
};

RecorderDockerDock::RecorderDockerDock()
    : QDockWidget(i18nc("Title of the docker", "Recorder"))
    , exportSettings(new RecorderExportSettings())
    , d(new Private(*exportSettings, this))
{
    QWidget* page = new QWidget(this);
    d->ui->setupUi(page);

    d->ui->buttonManageRecordings->setIcon(KisIconUtils::loadIcon("configure-thicker"));
    d->ui->buttonBrowse->setIcon(KisIconUtils::loadIcon("folder"));
    d->ui->buttonRecordToggle->setIcon(KisIconUtils::loadIcon("media-record"));
    d->ui->buttonExport->setIcon(KisIconUtils::loadIcon("document-export-16"));

    d->loadSettings();
    d->loadRelevantExportSettings();

    d->ui->editDirectory->setText(d->snapshotDirectory);
    d->ui->spinQuality->setValue(d->quality);
    d->ui->comboResolution->setCurrentIndex(d->resolution);
    d->ui->checkBoxRealTimeCaptureMode->setChecked(d->realTimeCaptureMode);
    d->ui->checkBoxRecordIsolateMode->setChecked(d->recordIsolateLayerMode);
    d->ui->checkBoxAutoRecord->setChecked(d->recordAutomatically);

    KisActionRegistry *actionRegistry = KisActionRegistry::instance();
    d->recordToggleAction = actionRegistry->makeQAction(keyActionRecordToggle, this);
    d->exportAction = actionRegistry->makeQAction(keyActionExport, this);

    connect(d->recordToggleAction, SIGNAL(toggled(bool)), d->ui->buttonRecordToggle, SLOT(setChecked(bool)));
    connect(d->exportAction, SIGNAL(triggered()), d->ui->buttonExport, SIGNAL(clicked()));
    connect(d->ui->buttonRecordToggle, SIGNAL(toggled(bool)), d->ui->buttonExport, SLOT(setDisabled(bool)));
    if (d->recordAutomatically)
        d->ui->buttonExport->setDisabled(true);

    // Need to register toolbar actions before attaching canvas else it wont appear after restart.
    // Is there any better way to do this?
    connect(KisPart::instance(), SIGNAL(sigMainWindowIsBeingCreated(KisMainWindow *)),
            this, SLOT(onMainWindowIsBeingCreated(KisMainWindow *)));

    connect(d->ui->buttonManageRecordings, SIGNAL(clicked()), this, SLOT(onManageRecordingsButtonClicked()));
    connect(d->ui->buttonBrowse, SIGNAL(clicked()), this, SLOT(onSelectRecordFolderButtonClicked()));
    connect(d->ui->comboFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(onFormatChanged(int)));
    connect(d->ui->spinQuality, SIGNAL(valueChanged(int)), this, SLOT(onQualityChanged(int)));
    connect(d->ui->comboResolution, SIGNAL(currentIndexChanged(int)), this, SLOT(onResolutionChanged(int)));
    connect(d->ui->checkBoxRealTimeCaptureMode, SIGNAL(toggled(bool)), this, SLOT(onRealTimeCaptureModeToggled(bool)));
    connect(d->ui->checkBoxRecordIsolateMode, SIGNAL(toggled(bool)), this, SLOT(onRecordIsolateLayerModeToggled(bool)));
    connect(d->ui->checkBoxAutoRecord, SIGNAL(toggled(bool)), this, SLOT(onAutoRecordToggled(bool)));
    connect(d->ui->buttonRecordToggle, SIGNAL(toggled(bool)), this, SLOT(onRecordButtonToggled(bool)));
    connect(d->ui->buttonExport, SIGNAL(clicked()), this, SLOT(onExportButtonClicked()));

    connect(&d->writer, SIGNAL(started()), this, SLOT(onWriterStarted()));
    connect(&d->writer, SIGNAL(finished()), this, SLOT(onWriterFinished()));
    connect(&d->writer, SIGNAL(pausedChanged(bool)), this, SLOT(onWriterPausedChanged(bool)));
    connect(&d->writer, SIGNAL(frameWriteFailed()), this, SLOT(onWriterFrameWriteFailed()));
    connect(&d->writer, SIGNAL(lowPerformanceWarning()), this, SLOT(onLowPerformanceWarning()));


    QScroller *scroller = KisKineticScroller::createPreconfiguredScroller(d->ui->scrollArea);
    if (scroller) {
        connect(scroller, SIGNAL(stateChanged(QScroller::State)),
                this, SLOT(slotScrollerStateChanged(QScroller::State)));
    }


    setWidget(page);
}

RecorderDockerDock::~RecorderDockerDock()
{
    delete d;
    delete exportSettings;
}

void RecorderDockerDock::setCanvas(KoCanvasBase* canvas)
{
    setEnabled(canvas != nullptr);

    if (d->canvas == canvas)
        return;

    d->canvas = dynamic_cast<KisCanvas2*>(canvas);
    d->writer.setCanvas(d->canvas);

    if (!d->canvas)
        return;

    KisDocument *document = d->canvas->imageView()->document();
    if (d->recordAutomatically && !d->enabledIds.contains(document->linkedResourcesStorageId()))
        onRecordButtonToggled(true);

    d->updateComboResolution(document->image()->width(), document->image()->height());

    d->prefix = d->getPrefix();
    d->updateWriterSettings();
    d->updateUiFormat();
    d->updateUiForRealTimeMode();

    bool enabled = d->enabledIds.value(document->linkedResourcesStorageId(), false);
    d->writer.setEnabled(enabled);
    d->updateRecordStatus(enabled);
}

void RecorderDockerDock::unsetCanvas()
{
    d->updateRecordStatus(false);
    d->recordToggleAction->setChecked(false);
    setEnabled(false);
    d->writer.stop();
    d->writer.setCanvas(nullptr);
    d->canvas = nullptr;
    d->enabledIds.clear();
}

void RecorderDockerDock::onMainWindowIsBeingCreated(KisMainWindow *window)
{
    KisKActionCollection *actionCollection = window->viewManager()->actionCollection();
    actionCollection->addAction(keyActionRecordToggle, d->recordToggleAction);
    actionCollection->addAction(keyActionExport, d->exportAction);
}

void RecorderDockerDock::onRecordButtonToggled(bool checked)
{
    QSignalBlocker blocker(d->ui->buttonRecordToggle);
    d->recordToggleAction->setChecked(checked);

    if (!d->canvas)
        return;

    const QString &id = d->canvas->imageView()->document()->linkedResourcesStorageId();

    bool wasEmpty = !d->enabledIds.values().contains(true);

    d->enabledIds[id] = checked;

    bool isEmpty = !d->enabledIds.values().contains(true);

    d->writer.setEnabled(checked);

    if (isEmpty == wasEmpty) {
        d->updateRecordStatus(checked);
        return;
    }


    d->ui->buttonRecordToggle->setEnabled(false);

    if (checked) {
        d->updateWriterSettings();
        d->updateUiFormat();
        d->writer.start();
    } else {
        d->writer.stop();
    }
}

void RecorderDockerDock::onExportButtonClicked()
{
    if (!d->canvas)
        return;

    KisDocument *document = d->canvas->imageView()->document();

    exportSettings->videoFileName = QFileInfo(document->caption().trimmed()).completeBaseName();
    exportSettings->inputDirectory = d->outputDirectory;
    exportSettings->format = d->format;
    exportSettings->realTimeCaptureMode = d->realTimeCaptureMode;

    RecorderExport exportDialog(exportSettings, this);
    exportDialog.setup();
    exportDialog.exec();

    if (d->realTimeCaptureMode)
        d->ui->spinRate->setValue(exportSettings->fps);
}

void RecorderDockerDock::onManageRecordingsButtonClicked()
{
    RecorderSnapshotsManager snapshotsManager(this);
    snapshotsManager.execFor(d->snapshotDirectory);
}


void RecorderDockerDock::onSelectRecordFolderButtonClicked()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::DirectoryOnly);
    const QString &directory = dialog.getExistingDirectory(this,
                               i18n("Select a Directory for Recordings"),
                               d->ui->editDirectory->text(),
                               QFileDialog::ShowDirsOnly);
    if (!directory.isEmpty()) {
        d->ui->editDirectory->setText(directory);
        RecorderConfig(false).setSnapshotDirectory(directory);
        d->loadSettings();
    }
}

void RecorderDockerDock::onRecordIsolateLayerModeToggled(bool checked)
{
    d->recordIsolateLayerMode = checked;
    RecorderConfig(false).setRecordIsolateLayerMode(checked);
    d->loadSettings();
}

void RecorderDockerDock::onAutoRecordToggled(bool checked)
{
    d->recordAutomatically = checked;
    RecorderConfig(false).setRecordAutomatically(checked);
    d->loadSettings();
}

void RecorderDockerDock::onRealTimeCaptureModeToggled(bool checked)
{
    d->realTimeCaptureMode = checked;
    RecorderConfig(false).setRealTimeCaptureMode(checked);
    d->loadSettings();
    d->updateUiForRealTimeMode();
    if (d->realTimeCaptureMode) {
        exportSettings->lockFps = true;
        exportSettings->realTimeCaptureModeWasSet = true;
    }
}

void RecorderDockerDock::onCaptureIntervalChanged(double interval)
{
    d->captureInterval = interval;
    RecorderConfig(false).setCaptureInterval(interval);
    d->loadSettings();
}
void RecorderDockerDock::onVideoFPSChanged(double fps)
{
    exportSettings->fps = fps;
    RecorderExportConfig(false).setFps(fps);
    d->loadRelevantExportSettings();
}

void RecorderDockerDock::onQualityChanged(int value)
{
    switch (d->format) {
    case RecorderFormat::JPEG:
        d->quality = value;
        RecorderConfig(false).setQuality(value);
        d->loadSettings();
        break;
    case RecorderFormat::PNG:
        d->compression = value;
        RecorderConfig(false).setCompression(value);
        d->loadSettings();
        break;
    }
}

void RecorderDockerDock::onFormatChanged(int format)
{
    d->format = static_cast<RecorderFormat>(format);
    d->updateUiFormat();

    RecorderConfig(false).setFormat(d->format);
    d->loadSettings();
}

void RecorderDockerDock::onResolutionChanged(int resolution)
{
    d->resolution = resolution;
    RecorderConfig(false).setResolution(resolution);
    d->loadSettings();
}

void RecorderDockerDock::onWriterStarted()
{
    d->updateRecordStatus(true);
}

void RecorderDockerDock::onWriterFinished()
{
    d->updateRecordStatus(false);
}

void RecorderDockerDock::onWriterPausedChanged(bool paused)
{
    d->updateRecIndicator(paused);
}

void RecorderDockerDock::onWriterFrameWriteFailed()
{
    QMessageBox::warning(this, i18nc("@title:window", "Recorder"),
                         i18n("The recorder has been stopped due to failure while writing a frame. Please check free disk space and start the recorder again."));
}

void RecorderDockerDock::onLowPerformanceWarning()
{
    if (d->realTimeCaptureMode) {
        d->showWarning(i18n("Low performance warning. The recorder is not able to write all the frames in time during Real Time Capture mode.\nTry to reduce the frame rate for the ffmpeg export or reduce the scaling filtering in the canvas acceleration settings."));
    } else {
        d->showWarning(i18n("Low performance warning. The recorder is not able to write all the frames in time.\nTry to increase the capture interval or reduce the scaling filtering in the canvas acceleration settings."));
    }
}

void RecorderDockerDock::onWarningTimeout()
{
    d->statusBarWarningLabel->hide();
}

void RecorderDockerDock::slotScrollerStateChanged(QScroller::State state)
{
    KisKineticScroller::updateCursor(this, state);
}
