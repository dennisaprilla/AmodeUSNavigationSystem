#ifndef MEASUREMENTWINDOW_H
#define MEASUREMENTWINDOW_H

#include <QWidget>

#include "amodeconnection.h"
#include "qualisysconnection.h"
#include "viconconnection.h"
#include "mocapconnection.h"
#include "amodemocaprecorder.h"

namespace Ui {
class MeasurementWindow;
}

class MeasurementWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementWindow(AmodeConnection *amodeConnection, MocapConnection *mocapConnection, QWidget *parent = nullptr);
    ~MeasurementWindow();

private slots:

    void on_pushButton_recordPath_clicked();

    void on_pushButton_recordButton_clicked();

private:
    Ui::MeasurementWindow *ui;

    AmodeConnection *myAmodeConnection       = nullptr;
    MocapConnection *myMocapConnection       = nullptr;
    AmodeMocapRecorder *myAmodeMocapRecorder = nullptr;

    bool isRecord = false;
};

#endif // MEASUREMENTWINDOW_H
