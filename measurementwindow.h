#ifndef MEASUREMENTWINDOW_H
#define MEASUREMENTWINDOW_H

#include <QWidget>

#include "amodeconnection.h"
#include "mocapconnection.h"
#include "amodemocaprecorder.h"

namespace Ui {
class MeasurementWindow;
}

class MeasurementWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementWindow(AmodeConnection *amodeConnection, MocapConnection *mocapConnection, bool isIntermRec, QWidget *parent = nullptr);
    ~MeasurementWindow();

    void setRecordParentPath(const QString &path);

public slots:
    void on_amodeConnected(AmodeConnection *amodeConnection);
    void on_amodeDisconnected();
    void on_mocapConnected(MocapConnection *mocapConnection);
    // void on_mocapDisconnected();
    void on_amodeTimedRecordingStarted();
    void on_amodeTimedRecordingStopped();

private slots:
    void on_pushButton_recordPath_clicked();
    void on_pushButton_recordButton_clicked();

private:
    Ui::MeasurementWindow *ui;

    AmodeConnection *myAmodeConnection       = nullptr;
    MocapConnection *myMocapConnection       = nullptr;
    AmodeMocapRecorder *myAmodeMocapRecorder = nullptr;

    bool isIntermediateRecording = false;
    bool isMeasurementRecording  = false;

    QString record_parentpath_  = "";
    QString record_currentpath_ = "";

    /**
     * @brief Creates a new numbered folder in the given path starting from zero if no folders exist.
     *
     * @param basePath The base directory path where the numbered folder will be created.
     * @return QString The full path of the created folder (e.g., "D:/path/to/base/0000"), or an empty string if an error occurred.
     */
    QString createNumberedFolder(const QString& basePath);

signals:
    void request_stop_amodeTimedRecording();
    void request_start_amodeTimedRecording();
};

#endif // MEASUREMENTWINDOW_H
