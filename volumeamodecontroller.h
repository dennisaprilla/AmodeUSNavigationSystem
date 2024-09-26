#ifndef VOLUMEAMODECONTROLLER_H
#define VOLUMEAMODECONTROLLER_H

#include <Eigen/Dense>

#include <QObject>
#include <QtDataVisualization>

#include "VolumeAmodeVisualizer.h"
#include "amodeconfig.h"
#include "qualisysconnection.h"
#include "qualisystransformationmanager.h"

/**
 * @class VolumeAmodeController
 * @brief For visualization of A-mode signal in 3D environment
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
    // void setAmodeGroupData(std::vector<AmodeConfig::Data> amodegroupdata);

    // /**
    //  * @brief [Deprecated] This function supposed to be used for visualizing the a-mode holder but failed.
    //  *
    //  * The reason is because Q3DScatter scales object relative to the size of the plot. It is annoying because i want absolute scale.
    //  * The mesh that was built by other 3D software (solid works, blender) is the actual size, but Q3DScatter scale it in an arbitrary
    //  * way. I tried fix it but it took so much time, so i left it.
    //  */
    // void newObject(std::string path, std::string name);

    // /**
    //  * @brief Updates the transformation of a-mode local coordinate system to global from Mocap feed
    //  */
    // void updateTransformations(Eigen::Isometry3d T);

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

    // /**
    //  * @brief [Deprecated] This function supposed to be used for initialized a-mode holder but failed. See newObject function.
    //  */
    // void initMeshItem();

    // /**
    //  * @brief Initialized the value of the signal. Basically zeros, but the length of A-mode sample.
    //  */
    // void initSeries();

    // /**
    //  * @brief Handle the visualization of the 3D signal. Only being called when pair of data (A-mode and Mocap) is arrived.
    //  */
    // void visualize3DSignal();



    // all variables related to visualization with QCustomPlot
    QCustom3DItem *meshItem_ = nullptr;                         //!< [Deprecated]. See initMeshItem and newObject function.
    Q3DScatter *scatter_;                                       //!< 3d Scatter object. Initialized from mainwindow.
    std::vector<QScatterDataArray> all_dataArray_;              //!< Stores multiple a-mode 3D signal data.
    std::vector<QScatter3DSeries> all_series_;                  //!< Stores multiple series (which contains a-mode 3d signal data).

    // all variables related to amode signal
    int downsample_nsample_;                                    //!< the number of sample after downsampling. used when we do downsample
    double downsample_ratio = 1.0;                              //!< specifiy the ratio of the downsampling. the default is half less.
    bool isDownsample       = true;                             //!< a flag to signify the class that we are doing downsampling.

    std::vector<AmodeConfig::Data> amodegroupdata_;                             //!< Stores the configuration of a-mode group. We need the local transformations.
    QVector<int16_t> amodesignal_;                                              //!< A-mode signal but in QVector. The datatype is required byAmodeDataManipulator class
    Eigen::Matrix<double, 4, Eigen::Dynamic> amode3dsignal_;                    //!< A-mode signal but in Eigen::Matrix. For transformation manupulation, easier with this class.
    std::vector<Eigen::Matrix<double, 4, Eigen::Dynamic>> all_amode3dsignal_;   //!< all amode3dsignal_ in a holder

    // all variables related to ultrasound specification
    Eigen::VectorXd us_dvector_;                                //!< vector of distances (ds), used for plotting the A-mode.
    Eigen::VectorXd us_tvector_;                                //!< vector of times (dt), used for plotting the A-mode

    // all variables related to transformations
    Eigen::Isometry3d currentT_holder_ref;                      //!< current transformation of holder in camera coordinate system
    std::vector<Eigen::Isometry3d> currentT_ustip_camera;       //!< current transformation of ultrasound tip in camera coordinate system
    std::vector<Eigen::Isometry3d> currentT_ustip_camera_Qt;    //!< similar to currentT_ustip_camera, but with Qt format, the T is transposed compared to common homogeneous T format

    // variable that controls "soft synchronization" data from qualisys and A-mode machine.
    bool amodesignalReady = false;                              //!< Set to true if new amode data comes
    bool rigidbodyReady = false;                                //!< Set to ture if new rigid body data comes

    // variable that handle the display of 3d signal
    int n_signaldisplay = 1;                                    //!< controls the visualization of the 3d signal. See setSignalDisplayMode() description for detail.
    std::vector<Eigen::Matrix4d> rotation_signaldisplay;        //!< controls the visualization of the 3d signal.
    std::string transformation_id = "";                         //!< The name of the current holder being visualized (relates to its transformation).
    std::string transformation_ref = "B_N_REF";                 //!< The name of reference rigid body (the wire calibration thing). The reconstructed bone is in this CS.

    VolumeAmodeVisualizer *m_visualizer;
    QThread *m_visualizerThread;
    bool m_isVisualizing;

signals:
    void newDataPairReceived(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody);
};

#endif // VOLUMEAMODECONTROLLER_H
