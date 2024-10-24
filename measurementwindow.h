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

    void setRecordPath(const QString &path);

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

signals:
    void request_stop_amodeTimedRecording();
};

#endif // MEASUREMENTWINDOW_H
