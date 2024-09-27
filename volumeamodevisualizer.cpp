#include "VolumeAmodeVisualizer.h"
#include <QThread>


#include "amodedatamanipulator.h"
#include "ultrasoundconfig.h"

VolumeAmodeVisualizer::VolumeAmodeVisualizer(QObject *parent, Q3DScatter *scatter, std::vector<AmodeConfig::Data> amodegroupdata)
    : QObject(parent), stopVisualization(false), isVisualizing(false), hasNewData(false), scatter_(scatter), amodegroupdata_(amodegroupdata)
{
    // Calculate necessary constants
    us_dvector_             = Eigen::VectorXd::LinSpaced(UltrasoundConfig::N_SAMPLE, 1, UltrasoundConfig::N_SAMPLE) * UltrasoundConfig::DS;             // [[mm]]
    us_tvector_             = Eigen::VectorXd::LinSpaced(UltrasoundConfig::N_SAMPLE, 1, UltrasoundConfig::N_SAMPLE) * UltrasoundConfig::DT * 1000000;   // [[mu s]]

    // I added option to downsample, for visualization performance
    if (isDownsample)
    {
        // downsample the us_dvector and get the length of the vector
        Eigen::VectorXd us_dvector_downsampled = AmodeDataManipulator::downsampleVector(us_dvector_, round((double)UltrasoundConfig::N_SAMPLE / downsample_ratio));
        downsample_nsample_ = us_dvector_downsampled.size();

        // first resize the amode3dsignal matrix according to nsample_downsample_ (not nsample_)
        amode3dsignal_.resize(Eigen::NoChange, downsample_nsample_);
        // initialize the amode3dsignal
        amode3dsignal_.row(0).setZero();     // x-coordinate
        amode3dsignal_.row(1).setZero();     // y-coordinate
        amode3dsignal_.row(2) = us_dvector_downsampled; // z-coordinate
        amode3dsignal_.row(3).setOnes();     // 1 (homogeneous)
    }
    else
    {
        // first resize the amode3dsignal matrix according to nsample_ of amode signal
        amode3dsignal_.resize(Eigen::NoChange, UltrasoundConfig::N_SAMPLE);
        // initialize the amode3dsignal
        amode3dsignal_.row(0).setZero();     // x-coordinate
        amode3dsignal_.row(1).setZero();     // y-coordinate
        amode3dsignal_.row(2) = us_dvector_; // z-coordinate
        amode3dsignal_.row(3).setOnes();     // 1 (homogeneous)
    }

    // initialize transformations
    currentT_holder_camera = Eigen::Isometry3d::Identity();
    for(std::size_t i = 0; i < amodegroupdata_.size(); ++i)
    {
        currentT_ustip_camera.push_back(Eigen::Isometry3d::Identity());
        currentT_ustip_camera_Qt.push_back(Eigen::Isometry3d::Identity());
    }

    // initialize points
    for(std::size_t i = 0; i < amodegroupdata_.size(); ++i)
    {
        all_amode3dsignal_.push_back(amode3dsignal_);
    }

    // initialize mode
    setSignalDisplayMode(0);
}

VolumeAmodeVisualizer::~VolumeAmodeVisualizer()
{

}

Eigen::Isometry3d VolumeAmodeVisualizer::RightToLeftHandedTransformation(const Eigen::Isometry3d& rightHandedTransform) {
    // Start by copying the input transformation
    Eigen::Isometry3d leftHandedTransform = rightHandedTransform;

    // leftHandedTransform.linear()(0, 2) *= -1.0;
    // leftHandedTransform.linear()(1, 2) *= -1.0;
    // leftHandedTransform.linear()(2, 0) *= -1.0;
    // leftHandedTransform.linear()(2, 1) *= -1.0;

    // leftHandedTransform.linear()(0, 0) *= -1.0;
    // leftHandedTransform.linear()(0, 1) *= -1.0;
    // leftHandedTransform.linear()(1, 0) *= -1.0;
    // leftHandedTransform.linear()(1, 1) *= -1.0;
    // leftHandedTransform.linear()(2, 2) *= -1.0;

    // leftHandedTransform.linear()(0, 1) *= -1.0;
    // leftHandedTransform.linear()(0, 2) *= -1.0;
    // leftHandedTransform.linear()(1, 0) *= -1.0;
    // leftHandedTransform.linear()(2, 0) *= -1.0;
    // leftHandedTransform.linear()(1, 2) *= -1.0;
    // leftHandedTransform.linear()(2, 1) *= -1.0;

    // leftHandedTransform.linear()(0, 0) *= -1.0;
    // leftHandedTransform.linear()(0, 1) *= -1.0;
    // leftHandedTransform.linear()(1, 0) *= -1.0;
    // leftHandedTransform.linear()(1, 1) *= -1.0;

    leftHandedTransform.linear()(0, 0) *= -1.0;
    leftHandedTransform.linear()(0, 1) *= -1.0;
    leftHandedTransform.linear()(0, 2) *= -1.0;
    leftHandedTransform.linear()(1, 0) *= -1.0;
    leftHandedTransform.linear()(1, 1) *= -1.0;
    leftHandedTransform.linear()(1, 2) *= -1.0;

    // Negate the Z component of the translation vector
    leftHandedTransform.translation()(2) *= -1.0;

    return leftHandedTransform;
}

void VolumeAmodeVisualizer::setSignalDisplayMode(int mode)
{
    /* We display our 3D signal with the envelope of our signal.
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

    // delete all the rotation matrix inside this variable
    rotation_signaldisplay.clear();

    // initialize angle degrees
    std::vector<int> angle_degrees;
    // angle_degrees will depend on user choice
    if (mode==0)
    {
        n_signaldisplay = 1;
        angle_degrees.insert(angle_degrees.end(), {0});
    }
    else if (mode==1)
    {
        n_signaldisplay = 2;
        angle_degrees.insert(angle_degrees.end(), {0, 180});
    }
    else if (mode == 2)
    {
        n_signaldisplay = 4;
        angle_degrees.insert(angle_degrees.end(), {0, 90, 180, 270});
    }

    for (size_t i = 0; i < angle_degrees.size(); ++i)
    {
        // specify the rotation in radian
        double angle_radians = angle_degrees.at(i) * M_PI / 180.0;
        // create the rotation matrix
        Eigen::Matrix4d current_R;
        current_R << cos(angle_radians),  0, sin(angle_radians), 0,
            0,                   1,                  0, 0,
            -sin(angle_radians), 0, cos(angle_radians), 0,
            0,                   0,                  0, 1;
        // store the rotation matrix
        rotation_signaldisplay.push_back(current_R);
    }
}

void VolumeAmodeVisualizer::updateTransformations(Eigen::Isometry3d currentT_holder_camera)
{
    // Make base transformation
    Eigen::Vector3d init_t(0,0,0);
    Eigen::Matrix3d init_R = Eigen::AngleAxisd(-M_PI / 2, Eigen::Vector3d::UnitX()).toRotationMatrix();
    Eigen::Isometry3d init_A = Eigen::Isometry3d::Identity();
    init_A.linear()      = init_R;
    init_A.translation() = init_t;
    // Convert the transformation from right-hand (Qualisys) to left-hand (Qt3DScatter plot)
    Eigen::Isometry3d init_A_LH   = RightToLeftHandedTransformation(init_A);

    // Convert the coordinate system here, from RHR (Qualisys) to LHR (Qt)
    // This process ensure that what we see in Qualisys is identical with what we see here
    Eigen::Isometry3d currentT_holder_camera_LH = RightToLeftHandedTransformation(currentT_holder_camera);

    // get the current transformation matrix from qualisys
    Eigen::Matrix3d tmp_R = currentT_holder_camera_LH.rotation();
    Eigen::Vector3d tmp_t = currentT_holder_camera_LH.translation();

    // initialize transformation matrix for Qt. In Qt, the representation of rotation matrix
    // is different. You need to transpose it so that it will be the same as representation in Eigen.
    Eigen::Isometry3d currentT_holder_camera_Qt    = Eigen::Isometry3d::Identity();
    Eigen::Isometry3d currentT_holder_camera_Qt_LH = Eigen::Isometry3d::Identity();
    currentT_holder_camera_Qt_LH.linear()          = tmp_R.transpose();
    currentT_holder_camera_Qt_LH.translation()     = tmp_t;

    // update the T_ustip_holder. Loop for each ustip inside the holder
    for(std::size_t i = 0; i < amodegroupdata_.size(); ++i)
    {
        // get the local_R data from amodeconfig
        Eigen::Vector3d local_R_euler(amodegroupdata_.at(i).local_R.at(0), amodegroupdata_.at(i).local_R.at(1), amodegroupdata_.at(i).local_R.at(2));
        // convert it to radians
        Eigen::Vector3d local_R_radians = local_R_euler * (M_PI / 180.0);
        // convert it to rotation matrix
        Eigen::Matrix3d local_R_matrix;
        local_R_matrix = Eigen::AngleAxisd(local_R_radians.x(), Eigen::Vector3d::UnitX()) *
                         Eigen::AngleAxisd(local_R_radians.y(), Eigen::Vector3d::UnitY()) *
                         Eigen::AngleAxisd(local_R_radians.z(), Eigen::Vector3d::UnitZ());

        // convert local_t from amode config data in with Eigen
        Eigen::Vector3d local_t(amodegroupdata_.at(i).local_t.at(0), amodegroupdata_.at(i).local_t.at(1), amodegroupdata_.at(i).local_t.at(2));

        // pack local_R and local_t into transformation matrix with Eigen
        Eigen::Isometry3d currentT_ustip_holder = Eigen::Isometry3d::Identity();
        currentT_ustip_holder.linear() = local_R_matrix;
        currentT_ustip_holder.translation() = local_t;

        // now, do the same thing also but for Qt matrix representation
        Eigen::Isometry3d currentT_ustip_holder_Qt = Eigen::Isometry3d::Identity();
        currentT_ustip_holder_Qt.linear() = local_R_matrix.transpose();
        currentT_ustip_holder_Qt.translation() = local_t;

        // Convert the coordinate system here, from RHR (Qualisys) to LHR (Qt)
        // This process ensure that what we see in Qualisys is identical with what we see here
        Eigen::Isometry3d currentT_ustip_holder_LH = RightToLeftHandedTransformation(currentT_ustip_holder);
        Eigen::Isometry3d currentT_ustip_holder_Qt_LH = RightToLeftHandedTransformation(currentT_ustip_holder_Qt);

        // store everything to our vectors. Transformation is now updated
        // currentT_ustip_camera.at(i) = init_A_LH * currentT_holder_camera_LH * currentT_ustip_holder_LH;
        // currentT_ustip_camera_Qt.at(i) = init_A_LH * currentT_holder_camera_Qt_LH * currentT_ustip_holder_Qt_LH;

        // currentT_ustip_camera.at(i) = currentT_holder_camera_LH * currentT_ustip_holder_LH;
        // currentT_ustip_camera_Qt.at(i) = currentT_holder_camera_Qt_LH * currentT_ustip_holder_Qt_LH;

        currentT_ustip_camera.at(i) = currentT_holder_camera * currentT_ustip_holder;
        currentT_ustip_camera_Qt.at(i) = currentT_holder_camera_Qt * currentT_ustip_holder_Qt;
    }
}

void VolumeAmodeVisualizer::visualize3DSignal()
{
    // update all necessary transformations
    updateTransformations(currentT_holder_camera);

    // [prototyping]
    int offset_x = 0;
    int offset_y = 0;
    int offset_z = 0;


    QMetaObject::invokeMethod(scatter_, [this]() {
        // delete all the series inside the scatter. So everytime there is a new rigidbody data coming
        // from qualisys, we should remove all the scatter data in our scatter series
        for (QScatter3DSeries *series : scatter_->seriesList()) {
            if (series->name() == "amode3dsignal" || series->name() == "amode3dorigin") {
                scatter_->removeSeries(series);
            }
        }
    });

    // Create a QScatterDataArray to store the origins of the signals.
    // I wrote this outside the loop (below), because i want the scatter for origin point to be one scatter object,
    // so that i could set the configuration of the visualization as a group
    QScatterDataArray *originArray = new QScatterDataArray;
    originArray->resize(amodegroupdata_.size());

    // For the case of scatter for the signal data themshelves, i will make them separately,
    // each signal has its own scatter object, so that i could make differentiation in color for each signal.
    // So i will need a loop for how much signal i have
    for(std::size_t i = 0; i < amodegroupdata_.size(); ++i)
    {
        // select the row from the whole amode data
        QVector<int16_t> amodesignal_rowsel = AmodeDataManipulator::getRow(amodesignal_, amodegroupdata_.at(i).number-1, UltrasoundConfig::N_SAMPLE);

        // if we decided to downsample, we need to downsample the amodesignal_rowsel first
        // before we assign to amode3dsignal_ so that the dimension will match
        Eigen::VectorXd amodesignal_rowsel_eigenVector;
        int idx = 175;
        if(isDownsample)
        {
            // downsample
            QVector<int16_t> usdata_qvint16_downsmp = AmodeDataManipulator::downsampleVector(amodesignal_rowsel, round((double)UltrasoundConfig::N_SAMPLE / downsample_ratio));

            // // Let's check if the size of usdata_qvint16_downsmp different than amode3dsignal_.row(0), if it is different,
            // // it will be a disaster when i want to assign the value of usdata_qvint16_downsmp to amode3dsignal_.row(0)
            // //
            // // And why the hell, you ask, do we need to do this whereas we downsample the same value to both usdata_qvint16_downsmp and amode3dsignal_.row(0)?
            // // The answer is i don't fucking know. It annoys me so much.
            // // What i can say, most probably, it because the implementation of downsampleVector() for QVector<int16_t> is for speed, not for downsampling accuracy.
            // // This block of code is super ugly, but who cares. It works. That's all we need. Bye.
            // if (usdata_qvint16_downsmp.size() != amode3dsignal_.row(0).size())
            // {
            //     int diff = usdata_qvint16_downsmp.size() - amode3dsignal_.row(0).size();
            //     if (diff > 0)
            //         for (int j=0; j<abs(diff); j++) usdata_qvint16_downsmp.removeLast();
            //     else
            //     {
            //         for (int j=0; j<abs(diff); j++) usdata_qvint16_downsmp.append(usdata_qvint16_downsmp.last());
            //     }
            // }

            // convert to Eigen::VectorXd
            amodesignal_rowsel_eigenVector = Eigen::Map<const Eigen::Matrix<int16_t, Eigen::Dynamic, 1>>(usdata_qvint16_downsmp.constData(), usdata_qvint16_downsmp.size()).cast<double>();
            // remove the near field disturbance
            int idx_new = round(double(idx) / downsample_ratio);
            amodesignal_rowsel_eigenVector.head(idx_new).setZero();
        }
        else
        {
            // convert to Eigen::VectorXd
            amodesignal_rowsel_eigenVector = Eigen::Map<const Eigen::Matrix<int16_t, Eigen::Dynamic, 1>>(amodesignal_rowsel.constData(), amodesignal_rowsel.size()).cast<double>();
            // remove the near field disturbance
            amodesignal_rowsel_eigenVector.head(idx).setZero();
        }

        // store it to our amode3dsignal_
        amode3dsignal_.row(0) = amodesignal_rowsel_eigenVector * 0.001; // x-coordinate

        // convert the points to accomodate RHR to LHR transformation
        Eigen::Matrix<double, 4, Eigen::Dynamic> amode3dsignal_LH;
        amode3dsignal_LH.resize(4, amode3dsignal_.cols());
        amode3dsignal_LH = amode3dsignal_;
        amode3dsignal_LH.row(2) *= -1;

        // Get the size of the data (that is the samples in the signal)
        int arraysize = amode3dsignal_.cols();
        // Since we provided several display mode for visualizing amode 3d signal, we need to provide
        // a variable to place all of those display modes
        Eigen::Matrix<double, 4, Eigen::Dynamic> current_amode3dsignal_display(4, arraysize * n_signaldisplay);

        // Let's populate the all the rotated signal data to it
        for (std::size_t j = 0; j < rotation_signaldisplay.size(); ++j)
        {
            int start_column = j*arraysize;
            // current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_camera_Qt.at(i).matrix() * rotation_signaldisplay.at(j) * amode3dsignal_;

            // current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_camera_Qt.at(i).matrix() * amode3dsignal_LH;
            // current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_camera.at(i).matrix() * amode3dsignal_LH;
            current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_camera.at(i).matrix() * amode3dsignal_;
        }
        current_amode3dsignal_display.row(1).swap(current_amode3dsignal_display.row(2));


        // This is the QScatterDataArray object for the signal data, each loop will be new object
        QScatterDataArray *dataArray = new QScatterDataArray;
        // resize the data array
        dataArray->resize(arraysize * n_signaldisplay);

        // We need to copy copy the points from the Eigen matrix to the QScatterDataArray one by one
        // There is no other way, this is bullshit from QtDataVisualization, i hate it so much
        for (int j = 0; j < arraysize * n_signaldisplay; ++j) {

            (*dataArray)[j].setPosition( QVector3D(current_amode3dsignal_display(0, j) +offset_x,
                                                  current_amode3dsignal_display(1, j) +offset_y,
                                                  current_amode3dsignal_display(2, j) +offset_z));
        }

        // Using this loop, i will add data to my originArray, that is the first data in the signal.
        // To be honest, it is not exactly the origin of the signal, but hey, who the fuck can see 0.01 mm differences in the visualization?
        (*originArray)[i].setPosition( QVector3D(current_amode3dsignal_display(0, 0) +offset_x,
                                                current_amode3dsignal_display(1, 0) +offset_y,
                                                current_amode3dsignal_display(2, 0) +offset_z));

        QMetaObject::invokeMethod(scatter_, [this, dataArray]() {
            QScatter3DSeries *series = new QScatter3DSeries();
            series->setName("amode3dsignal");
            series->setItemSize(0.04f);
            series->setMesh(QAbstract3DSeries::MeshPoint);
            series->dataProxy()->resetArray(dataArray);

            // add the current amode data (series) to our scatter object
            scatter_->addSeries(series);
        });


        // // Create new series where the QScatterDataArray object will be added
        // QScatter3DSeries *series = new QScatter3DSeries();
        // series->setName("amode3dsignal");
        // series->setItemSize(0.04f);
        // series->setMesh(QAbstract3DSeries::MeshPoint);
        // series->dataProxy()->resetArray(dataArray);

        // // add the current amode data (series) to our scatter object
        // scatter_->addSeries(series);
    }

    QMetaObject::invokeMethod(scatter_, [this, originArray]() {
        QScatter3DSeries *series = new QScatter3DSeries();
        series->setName("amode3dorigin");
        series->setItemSize(0.2f);
        series->setMesh(QAbstract3DSeries::MeshPoint);
        series->setBaseColor(Qt::red);
        series->dataProxy()->resetArray(originArray);

        // add the current origin point data (series) to our scatter object
        scatter_->addSeries(series);
    });

    // // Now, i want to create new series for my originData
    // QScatter3DSeries *series = new QScatter3DSeries();
    // series->setName("amode3dorigin");
    // series->setItemSize(0.2f);
    // series->setMesh(QAbstract3DSeries::MeshPoint);
    // series->setBaseColor(Qt::red);
    // series->dataProxy()->resetArray(originArray);

    // // add the current origin point data (series) to our scatter object
    // scatter_->addSeries(series);
}

void VolumeAmodeVisualizer::setData(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody)
{
    qDebug() << "setAmodeSignal called in thread:" << QThread::currentThread();
    qDebug() << "Expected worker thread:" << this->thread();

    QMutexLocker locker(&mutex);
    qDebug() << "VolumeAmodeVisualizer::setData| setData called | " << isVisualizing;

    // If visualization is in progress, ignore the new data
    if (isVisualizing) return;

    qDebug() << "VolumeAmodeVisualizer::setData| visualization is idle, set the data";

    amodesignal_ = data_amode;
    currentT_holder_camera = data_rigidbody;

    hasNewData = true;      // Set the flag to indicate new data has arrived
    // condition.wakeOne();    // Wake up the thread if it was waiting
}

void VolumeAmodeVisualizer::test(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody)
{
    // qDebug() << "VolumeAmodeVisualizer::test| got paired data";

    if(isVisualizing)
    {
        // qDebug() << "VolumeAmodeVisualizer::test| but it still visualizing, ignore";
        return;
    }

    // QMutexLocker locker(&mutex);

    amodesignal_ = data_amode;
    currentT_holder_camera = data_rigidbody;
    hasNewData = true;      // Set the flag to indicate new data has arrived
}

void VolumeAmodeVisualizer::processVisualization()
{
    while (true)
    {
        if (stopVisualization)
            break;

        // qDebug() << "VolumeAmodeVisualizer::processVisualization";
        QThread::msleep(10);

        if (!hasNewData)
            continue;

        QMutexLocker locker(&mutex);

        // // Set flag to indicate visualization is in progress
        // qDebug() << "VolumeAmodeVisualizer::processVisualization| start visualization";
        isVisualizing = true;

        // Perform visualization task
        // something weird is going on, if i reduce the sleep, it seems like the threading
        // is unstable. There will be more queue if you reduce the sleep time. However,
        // the more you put the sleep, the more stable, no queueing. Fucking weird!
        // i need to implement better multi threading architecture, but i don't want to do it
        // now. It is (somewhat) working to some extent, so be it.
        visualize3DSignal();
        QThread::msleep(200);


        // // After visualization is done, reset the flag4
        // qDebug() << "VolumeAmodeVisualizer::processVisualization| finish visualization";
        hasNewData = false;
        isVisualizing = false;
    }
}

void VolumeAmodeVisualizer::stop()
{
    QMutexLocker locker(&mutex);
    stopVisualization = true;
    // condition.wakeOne(); // Wake up the thread to allow it to exit
}
