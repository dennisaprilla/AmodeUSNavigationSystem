#include <iostream>

#include "volumeamodecontroller.h"

VolumeAmodeController::VolumeAmodeController(QObject *parent, Q3DScatter *scatter, std::vector<AmodeConfig::Data> amodegroupdata)
    : QObject{parent}, scatter_(scatter), amodegroupdata_(amodegroupdata), m_isVisualizing(false)
{
    // instantiate the visualizer and move it to a thread
    m_visualizer = new VolumeAmodeVisualizer(nullptr, scatter, amodegroupdata);
    m_visualizerThread = new QThread();
    m_visualizer->moveToThread(m_visualizerThread);

    // just in case it is not connected, just ignore this
    bool a = connect(m_visualizerThread, &QThread::started, m_visualizer, &VolumeAmodeVisualizer::processVisualization, Qt::QueuedConnection);
    bool b = connect(this, &VolumeAmodeController::newDataPairReceived, m_visualizer, &VolumeAmodeVisualizer::setData, Qt::QueuedConnection);
    if(!a) qDebug() << "a is not connected";
    if(!b) qDebug() << "b is not connected";

    // start the thread
    m_visualizerThread->start();
    qDebug() << "VolumeAmodeController::VolumeAmodeController() Worker thread started?" << m_visualizerThread->isRunning();
    qDebug() << "VolumeAmodeController::VolumeAmodeController() VisualizationWorker is in thread:" << m_visualizer->thread();
    qDebug() << "VolumeAmodeController::VolumeAmodeController() Expected worker thread:" << m_visualizerThread;
}

VolumeAmodeController::~VolumeAmodeController()
{
    // delete all the series inside the scatter
    for (QScatter3DSeries *series : scatter_->seriesList()) {
        if (series->name() == "amode3dsignal" || series->name() == "amode3dorigin") {
            scatter_->removeSeries(series);
        }
    }

    // Stop the worker and clean up
    m_visualizer->stop();
    m_visualizerThread->quit();
    m_visualizerThread->wait();

    delete m_visualizer;
    delete m_visualizerThread;
}

void VolumeAmodeController::setSignalDisplayMode(int mode)
{
    /* This function used to be directly used here in this class, but since i separate the controller and visualizer for
     * efficiency, now this function just become the bridge to the visualizer class
     *
     * We display our 3D signal with the envelope of our signal.
     *
     * However, because the signal basically a 2D representation in 3D space,
     * in certain angle, you can't see the differences in amplitude in the envelope signal.
     * It becomes a line in certain angle.
     *
     * So, we decided to provide several modes to show our envelope signal: Mode 1, 2, and 4.
     * Mode 1 is just normal 1 envelope signal.
     * Mode 2 is 2 envelope signal. The original, and the original that is rotated 180 degrees along directional axis.
     *        so it is like and envelope in up and down direction.
     * Mode 4 is 4 envelope signal. The original, and the original that is rotated 90, 180, 270 degrees along directional axis.
     *        you can imagine how it will be, come on.
     *
     */

    if (m_visualizer == nullptr) return;
    m_visualizer->setSignalDisplayMode(mode);
}

void VolumeAmodeController::setActiveHolder(std::string T_id)
{
    transformation_id = T_id;
}


void VolumeAmodeController::onAmodeSignalReceived(const std::vector<uint16_t> &usdata_uint16_)
{
    // get the amode data
    QVector<int16_t> tmp(usdata_uint16_.begin(), usdata_uint16_.end());
    amodesignal_ = tmp;

    // set the flag to be true...
    amodesignalReady = true;
    // ...and only continue to visualize data if rigidbody data already arrive
    if (rigidbodyReady) {
        // visualize3DSignal();

        // // Emit signal to the worker thread
        // qDebug() << "onAmodeSignalReceived| data pair ready";
        // emit newDataPairReceived(amodesignal_, currentT_holder_camera);
        // qDebug() << "onAmodeSignalReceived| signal emitted";

        // Hoi, dennis in the future, please check this function name, use the correct name
        m_visualizer->test(amodesignal_, currentT_holder_ref);
    }
}

void VolumeAmodeController::onRigidBodyReceived(const QualisysTransformationManager &tmanager)
{
    if(transformation_id.empty())
    {
        std::cerr << "VolumeAmodeController::onRigidBodyReceived() Tranformation id is empty, please use initialize it using setActiveHolder()" << std::endl;
        return;
    }

    try
    {
        Eigen::Isometry3d currentT_ref_camera = tmanager.getTransformationById(transformation_ref);
        Eigen::Isometry3d currentT_holder_camera = tmanager.getTransformationById(transformation_id);

        currentT_holder_ref = currentT_ref_camera.inverse() * currentT_holder_camera;
    }
    catch (const std::exception& e)
    {
        std::cerr << "An exception occurred: " << e.what() << std::endl;
        return;
    }

    // set the flag to be true...
    rigidbodyReady = true;
    // ...and only continue to visualize data if the amode signal data already arrive
    if (amodesignalReady) {
        // visualize3DSignal();

        // // Emit signal to the worker thread
        // qDebug() << "onRigidBodyReceived| data pair ready";
        // emit newDataPairReceived(amodesignal_, currentT_holder_camera);
        // qDebug() << "onRigidBodyReceived| signal emitted";

        // Hoi, dennis in the future, please check this function name, use the correct name
        m_visualizer->test(amodesignal_, currentT_holder_ref);
    }

}
