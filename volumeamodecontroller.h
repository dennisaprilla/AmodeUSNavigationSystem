#ifndef VOLUMEAMODECONTROLLER_H
#define VOLUMEAMODECONTROLLER_H

#include <Eigen/Dense>

#include <QObject>
#include <QtDataVisualization>

#include "VolumeAmodeVisualizer.h"
#include "qualisystransformationmanager.h"

/**
 * @class VolumeAmodeController
 * @brief For controling the visualization thread of A-mode signal in 3D environment
 *
 * For the context. The ultimate goal of this software is to visualize A-mode 3D in the 3D space
 * together with the reconstructed volume (bone surface). By gathering- up these two entity into
 * one 3D space, we will be able to navigate the A-mode such that it directed towards bone surface.
 * This class is responsible to do just that. It will stream the A-mode and Mocap data, transform
 * it accordingly so that it represents what the user do in real time.
 *
 * There is one caveat. I plot the A-mode 3d signal with a 3D scatter plot. Every sample in the
 * signal is a 3D point. Sometimes you will see the signal is like a "discrete" dots, not so pleasant
 * to see. To make line in 3D is really difficult to do, so i stick with this.
 *
 * In the development phase, I found out that 3d signal visualization in 3d scatter plot is a
 * quite heavy task. When we had direct connection from amode machine to this pc, the problem seems
 * not really apparent, but when we use local network involving router, somehow amode machine
 * produces data queueing. My solution is to move the visualization to another thread. Please
 * check VolumeAmodeVisualizer.cpp for details
 *
 */

class VolumeAmodeController : public QObject
{
    Q_OBJECT

public:

    /**
     * @brief Constructor function. Requres the A-mode group data, which helds the local transformation information of every individual A-mode.
     */
    VolumeAmodeController(QObject *parent = nullptr, Q3DScatter *scatter = nullptr, std::vector<AmodeConfig::Data> amodegroupdata = std::vector<AmodeConfig::Data>());

    /**
     * @brief Destructor function.
     */
    ~VolumeAmodeController();


    /**
     * @brief Change the signal visualization.
     *
     * The signal that is visualized is an envelope. When i write this class, i want the signal to be symmetric above and below x-axis.
     * So, basically this function make the envelope symmetric (only for visualization).
     */
    void setSignalDisplayMode(int mode);

    /**
     * @brief SET the active ultrasound holder being visualized.
     *
     * The software can only visualize 3D signal from one ultrasound holder at a time. So the user need
     * to specify which ultrasound holder needs to be visualizied so that this class will select the
     * corresponding transformation.
     *
     */
    void setActiveHolder(std::string T_id);

public slots:

    /**
     * @brief slot function, will be called when an amode signal is received, needs to be connected to signal from AmodeConnection::dataReceived
     */
    void onAmodeSignalReceived(const std::vector<uint16_t> &usdata_uint16_);

    /**
     * @brief slot function, will be called when transformations in a timestamp are received, needs to be connected to signal from QualisysConnection::dataReceived class
     */
    void onRigidBodyReceived(const QualisysTransformationManager &tmanager);


private:

    // all variables related to visualization with QCustomPlot
    Q3DScatter *scatter_;                                       //!< 3d Scatter object. Initialized from mainwindow.

    // all variables related to amode data
    std::vector<AmodeConfig::Data> amodegroupdata_;             //!< Stores the configuration of a-mode group. We need the local transformations.
    QVector<int16_t> amodesignal_;                              //!< A-mode signal but in QVector. The datatype is required byAmodeDataManipulator class

    // all variables related to rigid body data
    Eigen::Isometry3d currentT_holder_ref;                      //!< current transformation of holder in camera coordinate system

    // variable that controls "soft synchronization" data from qualisys and A-mode machine.
    bool amodesignalReady = false;                              //!< Set to true if new amode data comes
    bool rigidbodyReady = false;                                //!< Set to ture if new rigid body data comes

    // variable that handle the display of 3d signal
    std::string transformation_id = "";                         //!< The name of the current holder being visualized (relates to its transformation).
    std::string transformation_ref = "B_N_REF";                 //!< The name of reference rigid body (the wire calibration thing). The reconstructed bone is in this CS.

    // variable that handle the threading
    VolumeAmodeVisualizer *m_visualizer;
    QThread *m_visualizerThread;
    bool m_isVisualizing;

signals:
    void newDataPairReceived(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody);
};

#endif // VOLUMEAMODECONTROLLER_H
