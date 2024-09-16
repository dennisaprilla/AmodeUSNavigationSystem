#pragma once

#include <QObject>
#include <QtDataVisualization>
#include <QVector>
#include <QMutex>
#include <QWaitCondition>
#include <cstdint>

#include <Eigen/Dense>

#include "amodeconfig.h"

class VolumeAmodeVisualizer : public QObject
{
    Q_OBJECT

public:
    explicit VolumeAmodeVisualizer(QObject *parent = nullptr, Q3DScatter *scatter = nullptr,  std::vector<AmodeConfig::Data> amodegroupdata = std::vector<AmodeConfig::Data>());
    ~VolumeAmodeVisualizer();

    /**
     * @brief Change the signal visualization.
     *
     * The signal that is visualized is an envelope. When i write this class, i want the signal to be symmetric above and below x-axis.
     * So, basically this function make the envelope symmetric (only for visualization).
     */
    void setSignalDisplayMode(int mode);

    /**
     * @brief Updates the transformation of a-mode local coordinate system to global from Mocap feed
     */
    void updateTransformations(Eigen::Isometry3d T);

    /**
     * @brief a function to stop visualization
     */
    void stop();

    /**
     * @brief SET the data that is from volumeamodecontroller to this class.
     */
    void test(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody);

signals:

public slots:
    /**
     * @brief [Deprecated] SET the data that is from volumeamodecontroller to this class.
     */
    void setData(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody);

    /**
     * @brief Controlling visualization in multithreading way.
     *
     * This function will runs idefinitely once this class is instantiated and the thread started.
     * It will only perform visualization when there is new data coming, that is when setData function() called.
     */
    void processVisualization();

private:
    /**
     * @brief Handle the visualization of the 3D signal. Only being called when pair of data (A-mode and Mocap) is arrived.
     */
    void visualize3DSignal();

    /**
     * @brief Convert right-hand CS (from Qualisys) to left-handed CS (Qt3DScatter plot)
     */
    Eigen::Isometry3d RightToLeftHandedTransformation(const Eigen::Isometry3d& rightHandedTransform);

    // all related to multithreading
    QMutex mutex;
    QWaitCondition condition;
    bool stopVisualization;
    bool isVisualizing;
    bool hasNewData;

    // all related to visualization
    Q3DScatter *scatter_;                                       //!< 3d Scatter object. Initialized from mainwindow.
    QScatter3DSeries *series;
    std::vector<QScatterDataArray> all_dataArray_;              //!< Stores multiple a-mode 3D signal data.
    std::vector<QScatter3DSeries> all_series_;                  //!< Stores multiple series (which contains a-mode 3d signal data).

    // all variables related to ultrasound specification
    Eigen::VectorXd us_dvector_;                                //!< vector of distances (ds), used for plotting the A-mode.
    Eigen::VectorXd us_tvector_;                                //!< vector of times (dt), used for plotting the A-mode

    // all variables related to amode signal
    int downsample_nsample_;                                    //!< the number of sample after downsampling. used when we do downsample
    double downsample_ratio = 2.0;                              //!< specifiy the ratio of the downsampling. the default is half less.
    bool isDownsample       = true;                             //!< a flag to signify the class that we are doing downsampling.

    std::vector<AmodeConfig::Data> amodegroupdata_;             //!< Stores the configuration of a-mode group. We need the local transformations.
    QVector<int16_t> amodesignal_;
    Eigen::Matrix<double, 4, Eigen::Dynamic> amode3dsignal_;    //!< A-mode signal but in Eigen::Matrix. For transformation manupulation, easier with this class.
    std::vector<Eigen::Matrix<double, 4, Eigen::Dynamic>> all_amode3dsignal_;   //!< all amode3dsignal_ in a holder

    // all variables related to transformations
    Eigen::Isometry3d currentT_holder_camera;                   //!< current transformation of holder in camera coordinate system
    std::vector<Eigen::Isometry3d> currentT_ustip_camera;       //!< current transformation of ultrasound tip in camera coordinate system
    std::vector<Eigen::Isometry3d> currentT_ustip_camera_Qt;    //!< similar to currentT_ustip_camera, but with Qt format, the T is transposed compared to common homogeneous T format

    // variable that handle the display of 3d signal
    int n_signaldisplay = 1;                                    //!< controls the visualization of the 3d signal. See setSignalDisplayMode() description for detail.
    std::vector<Eigen::Matrix4d> rotation_signaldisplay;        //!< controls the visualization of the 3d signal.
};
