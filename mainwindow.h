#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "amodeconnection.h"
#include "amodeconfig.h"
#include "bmodeconnection.h"
#include "bmode3dvisualizer.h"
#include "qcustomplot.h"
#include "qcustomplotintervalwindow.h"
#include "qualisysconnection.h"
#include "mhawriter.h"
#include "mhareader.h"
#include "volume3dcontroller.h"
#include "volumeamodecontroller.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void displayImage(const cv::Mat &image);
    void displayUSsignal(const std::vector<uint16_t> &usdata_uint16_);
    void disconnectUSsignal();
    void updateQualisysText(const QualisysTransformationManager &tmanager);

    void volumeReconstructorCmdFinished();
    void volumeReconstructorCmdStandardOutput();
    void volumeReconstructorCmdStandardError();

private slots:
    // void on_pushButton_startCamera_clicked();
    // void on_pushButton_qualisysConnect_clicked();
    void on_pushButton_calibbrowse_clicked();
    void on_pushButton_bmode2d3d_clicked();
    void on_pushButton_mhaRecord_clicked();

    void on_pushButton_amodeConnect_clicked();
    void on_pushButton_amodeConfig_clicked();
    void on_comboBox_amodeNumber_textActivated(const QString &arg1);
    void on_pushButton_amodeWindow_clicked();

    void on_pushButton_volumeLoad_clicked();
    void on_pushButton_volumeReconstruct_clicked();
    void on_pushButton_volumeBrowseConfig_clicked();
    void on_pushButton_volumeBrowseRecording_clicked();
    void on_checkBox_volumeShow3DSignal_clicked(bool checked);

    void on_pushButton_mhaPath_clicked();
    void on_pushButton_volumeBrowseOutput_clicked();
    void on_checkBox_autoReconstruct_stateChanged(int arg1);
    void on_comboBox_volume3DSignalMode_currentIndexChanged(int index);

private:
    void slotConnect_Bmode2d3d();
    void slotDisconnect_Bmode2d3d();
    void slotConnect_Amode();
    void slotDisconnect_Amode();

    Ui::MainWindow *ui;

    AmodeConnection *myAmodeConnection              = nullptr;
    AmodeConfig *myAmodeConfig                      = nullptr;
    BmodeConnection *myBmodeConnection              = nullptr;
    QualisysConnection *myQualisysConnection        = nullptr;
    Bmode3DVisualizer *myBmode3Dvisualizer          = nullptr;
    MHAWriter *myMHAWriter                          = nullptr;
    MHAReader *myMHAReader                          = nullptr;
    Volume3DController *myVolume3DController        = nullptr;
    VolumeAmodeController *myVolumeAmodeController  = nullptr;

    // for
    Q3DScatter *scatter;                        //!< For handling amode 3d plots and 3d volume visualization
    QProcess* process;                          //!< For invoking command prompt

    // for amode 2d plots
    QCustomPlotIntervalWindow *amodePlot;
    std::vector<QCustomPlotIntervalWindow*> amodePlots; //!< For handling amode 2d plots visualization
    Eigen::VectorXd us_dvector_;                //!< Stores the array of distances, used by plots
    Eigen::VectorXd us_dvector_downsampled_;    //!< Same as us_dvector_, but downsampled
    Eigen::VectorXd us_tvector_;                //!< Stores the array of time, used by plots
    Eigen::VectorXd us_tvector_downsampled_;    //!< Same as us_tvector_, but downsampled
    double downsample_ratio_ = 4.0;             //!< Downsample ratio, used to reduce the amount of data being visualized in 2d plots
    int downsample_nsample_;                    //!< The real length of the downsampled array

    bool isMHArecord            = true;         //!< Flag to inform whether we are ready for recording MHA or not

    bool isBmode2d3dStream      = true;         //!< Flag to inform whether we are ready to stream B-mode 2d image and 3d visualize it or not
    bool isAmodeStream          = true;         //!< Flag to inform whether we are ready to stream A-mode data or not

    bool isBmode2dFirstStream   = true;         //!< Flag to inform whether it is the first data from B-mode 2d image stream comes (for image scaling (in the gui) purpose)
    bool isBmode2d3dFirstStream = true;         //!< Flag to inform whether it is the first time to stream B-mode 2d image and qualisys or not (first one need initialization)
    bool isAutoReconstructFirstClick = true;    //!< Flag to inform whether it is the first time auto reconstruct checkbox being clicked (for information about auto reconstruct)

    int bmode2dvisheight = 1;                   //!< Stores the height of the layout where we draw B-mode 2d image (there is a bug that the height keep increasing)

};
#endif // MAINWINDOW_H
