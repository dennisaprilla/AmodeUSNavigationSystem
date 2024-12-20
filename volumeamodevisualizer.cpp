#include "VolumeAmodeVisualizer.h"
#include <QThread>


#include "amodedatamanipulator.h"
#include "ultrasoundconfig.h"

VolumeAmodeVisualizer::VolumeAmodeVisualizer(QObject *parent, Q3DScatter *scatter, std::vector<AmodeConfig::Data> amodegroupdata)
    : QObject(parent), stopVisualization(false), isVisualizing(false), hasNewData(false), scatter_(scatter), amodegroupdata_(amodegroupdata)
{
    // Calculate necessary constants, will be used later for signal visualization
    us_dvector_ = Eigen::VectorXd::LinSpaced(UltrasoundConfig::N_SAMPLE, 1, UltrasoundConfig::N_SAMPLE) * UltrasoundConfig::DS;             // [[mm]]
    us_tvector_ = Eigen::VectorXd::LinSpaced(UltrasoundConfig::N_SAMPLE, 1, UltrasoundConfig::N_SAMPLE) * UltrasoundConfig::DT * 1000000;   // [[mu s]]

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
    currentT_holder_ref = Eigen::Isometry3d::Identity();
    for(std::size_t i = 0; i < amodegroupdata_.size(); ++i)
    {
        currentT_ustip_ref.push_back(Eigen::Isometry3d::Identity());
        currentT_ustip_ref_Qt.push_back(Eigen::Isometry3d::Identity());
    }

    // initialize points
    for(std::size_t i = 0; i < amodegroupdata_.size(); ++i)
    {
        all_amode3dsignal_.push_back(amode3dsignal_);
    }

    // initialize array of boolean that will be used for visualizing xLine in 2D plot
    expectedpeaks_.resize(amodegroupdata_.size());

    // initialize mode
    setSignalDisplayMode(0);
}

VolumeAmodeVisualizer::~VolumeAmodeVisualizer()
{

}

Eigen::Isometry3d VolumeAmodeVisualizer::RightToLeftHandedTransformation(const Eigen::Isometry3d& rightHandedTransform) {
    // Start by copying the input transformation
    Eigen::Isometry3d leftHandedTransform = rightHandedTransform;

    // First attempt
    // leftHandedTransform.linear()(0, 2) *= -1.0;
    // leftHandedTransform.linear()(1, 2) *= -1.0;
    // leftHandedTransform.linear()(2, 0) *= -1.0;
    // leftHandedTransform.linear()(2, 1) *= -1.0;

    // Second attempt
    // leftHandedTransform.linear()(0, 0) *= -1.0;
    // leftHandedTransform.linear()(0, 1) *= -1.0;
    // leftHandedTransform.linear()(1, 0) *= -1.0;
    // leftHandedTransform.linear()(1, 1) *= -1.0;
    // leftHandedTransform.linear()(2, 2) *= -1.0;

    // Third attempt
    // leftHandedTransform.linear()(0, 1) *= -1.0;
    // leftHandedTransform.linear()(0, 2) *= -1.0;
    // leftHandedTransform.linear()(1, 0) *= -1.0;
    // leftHandedTransform.linear()(2, 0) *= -1.0;
    // leftHandedTransform.linear()(1, 2) *= -1.0;
    // leftHandedTransform.linear()(2, 1) *= -1.0;

    // Fourth attempt
    // leftHandedTransform.linear()(0, 0) *= -1.0;
    // leftHandedTransform.linear()(0, 1) *= -1.0;
    // leftHandedTransform.linear()(1, 0) *= -1.0;
    // leftHandedTransform.linear()(1, 1) *= -1.0;

    // Fifth attempt, this is the correct one
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
        current_R << cos(angle_radians), 0, sin(angle_radians), 0,
                                      0, 1,                  0, 0,
                    -sin(angle_radians), 0, cos(angle_radians), 0,
                                      0, 0,                  0, 1;

        // current_R << 1,                  0,                   0, 0,
        //              0, cos(angle_radians), -sin(angle_radians), 0,
        //              0, sin(angle_radians),  cos(angle_radians), 0,
        //              0,                  0,                   0, 1;

        // current_R << cos(angle_radians), -sin(angle_radians),                  0, 0,
        //              sin(angle_radians),  cos(angle_radians),                  0, 0,
        //                               0,                   0, cos(angle_radians), 0,
                                      0,                   0,                  0, 1;
        // store the rotation matrix
        rotation_signaldisplay.push_back(current_R);
    }
}

void VolumeAmodeVisualizer::updateTransformations(Eigen::Isometry3d currentT_holder_ref)
{
    // // Make base transformation
    // Eigen::Vector3d init_t(0,0,0);
    // Eigen::Matrix3d init_R = Eigen::AngleAxisd(-M_PI / 2, Eigen::Vector3d::UnitX()).toRotationMatrix();
    // Eigen::Isometry3d init_A = Eigen::Isometry3d::Identity();
    // init_A.linear()      = init_R;
    // init_A.translation() = init_t;
    // // Convert the transformation from right-hand (Qualisys) to left-hand (Qt3DScatter plot)
    // Eigen::Isometry3d init_A_LH   = RightToLeftHandedTransformation(init_A);

    // // Convert the coordinate system here, from RHR (Qualisys) to LHR (Qt)
    // // This process ensure that what we see in Qualisys is identical with what we see here
    // Eigen::Isometry3d currentT_holder_ref_LH = RightToLeftHandedTransformation(currentT_holder_ref);

    // // get the current transformation matrix from qualisys
    // Eigen::Matrix3d tmp_R = currentT_holder_ref_LH.rotation();
    // Eigen::Vector3d tmp_t = currentT_holder_ref_LH.translation();

    // // initialize transformation matrix for Qt. In Qt, the representation of rotation matrix
    // // is different. You need to transpose it so that it will be the same as representation in Eigen.
    // Eigen::Isometry3d currentT_holder_ref_Qt    = Eigen::Isometry3d::Identity();
    // Eigen::Isometry3d currentT_holder_ref_Qt_LH = Eigen::Isometry3d::Identity();
    // currentT_holder_ref_Qt_LH.linear()          = tmp_R.transpose();
    // currentT_holder_ref_Qt_LH.translation()     = tmp_t;

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

        // // now, do the same thing also but for Qt matrix representation
        // Eigen::Isometry3d currentT_ustip_holder_Qt = Eigen::Isometry3d::Identity();
        // currentT_ustip_holder_Qt.linear() = local_R_matrix.transpose();
        // currentT_ustip_holder_Qt.translation() = local_t;

        // // Convert the coordinate system here, from RHR (Qualisys) to LHR (Qt)
        // // This process ensure that what we see in Qualisys is identical with what we see here
        // Eigen::Isometry3d currentT_ustip_holder_LH = RightToLeftHandedTransformation(currentT_ustip_holder);
        // Eigen::Isometry3d currentT_ustip_holder_Qt_LH = RightToLeftHandedTransformation(currentT_ustip_holder_Qt);

        // store everything to our vectors. Transformation is now updated
        // this one with LH and with init_A
        // currentT_holder_ref_Qt.at(i) = init_A_LH * currentT_holder_ref_Qt_LH * currentT_ustip_holder_Qt_LH;
        // currentT_holder_ref.at(i) = init_A_LH * currentT_holder_ref_LH * currentT_ustip_holder_LH;
        // this one with LH
        // currentT_holder_ref_Qt.at(i) = currentT_holder_ref_Qt_LH * currentT_ustip_holder_Qt_LH;
        // currentT_ustip_ref.at(i) = currentT_holder_ref_LH * currentT_ustip_holder_LH;

        // currentT_ustip_ref_Qt.at(i) = currentT_holder_ref_Qt * currentT_ustip_holder_Qt;
        currentT_ustip_ref.at(i) = currentT_holder_ref * currentT_ustip_holder;
    }
}

void VolumeAmodeVisualizer::visualize3DSignal()
{
    // update all necessary transformations
    updateTransformations(currentT_holder_ref);

    // Delete all the series inside the scatter. So everytime there is a new rigidbody data coming
    // from qualisys, we should remove all the scatter data in our scatter series
    // >> I need this QMetaObject::invokeMethod because scatter_ object is in the main thread, i can't access it directly
    // >> because this class is meant to be run in another thread. This is the way to access it
    QMetaObject::invokeMethod(scatter_, [this]() {
        for (QScatter3DSeries *series : scatter_->seriesList()) {
            if (series->name() == "amode3dsignal" || series->name() == "amode3dorigin" || series->name() == "amode3dexpectedpeak") {
                scatter_->removeSeries(series);
            }
        }
    });

    // Create a QScatterDataArray to store the origins of the signals.
    // I wrote this outside the loop (below), because i want the scatter for origin point to be one scatter object,
    // so that i could set the configuration of the visualization as a group
    QScatterDataArray *originArray = new QScatterDataArray;
    originArray->resize(amodegroupdata_.size());

    // Create another QScatterDataArray to store the expected peaks
    QScatterDataArray *expectedPeakArray = new QScatterDataArray;
    expectedPeakArray->resize(amodegroupdata_.size());

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
            // convert to Eigen::VectorXd
            amodesignal_rowsel_eigenVector = Eigen::Map<const Eigen::Matrix<int16_t, Eigen::Dynamic, 1>>(usdata_qvint16_downsmp.constData(), usdata_qvint16_downsmp.size()).cast<double>();
            // remove the near field disturbance (first few sample in the signal has a really big amplitude but have no meaning)
            int idx_new = round(double(idx) / downsample_ratio);
            amodesignal_rowsel_eigenVector.head(idx_new).setZero();
        }
        else
        {
            // convert to Eigen::VectorXd
            amodesignal_rowsel_eigenVector = Eigen::Map<const Eigen::Matrix<int16_t, Eigen::Dynamic, 1>>(amodesignal_rowsel.constData(), amodesignal_rowsel.size()).cast<double>();
            // remove the near field disturbance (first few sample in the signal has a really big amplitude but have no meaning)
            amodesignal_rowsel_eigenVector.head(idx).setZero();
        }

        // store it to our amode3dsignal_ while multiplied by a scale (the height of the amplitude in 3d visualization)
        amode3dsignal_.row(0) = amodesignal_rowsel_eigenVector * 0.0015; // x-coordinate

        // // convert the points to accomodate RHR to LHR transformation
        // Eigen::Matrix<double, 4, Eigen::Dynamic> amode3dsignal_LH;
        // amode3dsignal_LH.resize(4, amode3dsignal_.cols());
        // amode3dsignal_LH = amode3dsignal_;
        // amode3dsignal_LH.row(2) *= -1;

        // Get the size of the data (that is the samples in the signal)
        int arraysize = amode3dsignal_.cols();

        // Since we provided several display mode for visualizing amode 3d signal, we need to provide
        // a variable to place all of those display modes
        Eigen::Matrix<double, 4, Eigen::Dynamic> current_amode3dsignal_display(4, arraysize * n_signaldisplay);

        // Let's populate the all the rotated signal data to it
        for (std::size_t j = 0; j < rotation_signaldisplay.size(); ++j)
        {
            int start_column = j*arraysize;

            // current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_ref_Qt.at(i).matrix() * rotation_signaldisplay.at(j) * amode3dsignal_;
            // current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_ref_Qt.at(i).matrix() * amode3dsignal_LH;
            // current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_ref.at(i).matrix() * amode3dsignal_LH;
            current_amode3dsignal_display.block(0, start_column, 4, arraysize) = currentT_ustip_ref.at(i).matrix() * amode3dsignal_;
        }
        current_amode3dsignal_display.row(1).swap(current_amode3dsignal_display.row(2));

        // This is the QScatterDataArray object for the signal data, each loop will be new object
        QScatterDataArray *dataArray = new QScatterDataArray;
        // resize the dataArray (multiply of n_signaldisplay) for display purpose (double envelope)
        dataArray->resize(arraysize * n_signaldisplay);

        // Add the points to the our QScatterDataArray (dataArray)
        // We need to copy copy the points from the Eigen matrix to the QScatterDataArray one by one
        // There is no other way, this is bullshit from QtDataVisualization, i hate it so much
        for (int j = 0; j < arraysize * n_signaldisplay; ++j) {
            (*dataArray)[j].setPosition( QVector3D(current_amode3dsignal_display(0, j),
                                                  current_amode3dsignal_display(1, j),
                                                  current_amode3dsignal_display(2, j)));
        }

        // Add the QScatterDataArray we have (dataArray) to our scatter series by creating a QScatter3DSeries
        // >> I need this QMetaObject::invokeMethod because scatter_ object is in the main thread, i can't access it directly
        // >> because this class is meant to be run in another thread. This is the way to access it
        QMetaObject::invokeMethod(scatter_, [this, dataArray]() {
            QScatter3DSeries *series = new QScatter3DSeries();
            series->setName("amode3dsignal");
            series->setItemSize(0.04f);
            series->setMesh(QAbstract3DSeries::MeshPoint);
            series->dataProxy()->resetArray(dataArray);

            // add the current amode data (series) to our scatter object
            scatter_->addSeries(series);
        });


        // if the user selected the expected peaks in the 2d plot visualization,
        // it means that we need to visualize the expected peak in our 3d signal visualization.
        qDebug() << "VolumeAmodeVisualizer::visualize3DSignal() expectedpeaks_ at " << i << " has value? " << expectedpeaks_.at(i).has_value();
        if(expectedpeaks_.at(i).has_value())
        {
            // initialize the 3d point
            Eigen::Matrix<double, 4, 1> expected3dpeak;
            expected3dpeak(0) = 0.0;
            expected3dpeak(1) = 0.0;
            expected3dpeak(2) = expectedpeaks_.at(i).value();
            expected3dpeak(3) = 1.0;

            // transform the point
            Eigen::Matrix<double, 4, 1> current_expected3dpeak_display;
            current_expected3dpeak_display = currentT_ustip_ref.at(i).matrix() * expected3dpeak;

            // swap between z and y
            current_expected3dpeak_display.row(1).swap(current_expected3dpeak_display.row(2));

            // Add the points to the our QScatterDataArray (expectedPeakArray)
            (*expectedPeakArray)[i].setPosition( QVector3D( current_expected3dpeak_display(0),
                                                            current_expected3dpeak_display(1),
                                                            current_expected3dpeak_display(2)));

        }

        // Add the points to the our QScatterDataArray (originArray)
        // Using this loop, i will add data to my originArray, that is the first data in the signal.
        // To be honest, it is not exactly the origin of the signal, but hey, who the fuck can see 0.01 mm differences in the visualization?
        (*originArray)[i].setPosition( QVector3D(current_amode3dsignal_display(0, 0),
                                                current_amode3dsignal_display(1, 0),
                                                current_amode3dsignal_display(2, 0)));
    }

    // Add the QScatterDataArray we have (expectedPeakArray, originArray) to our scatter series by creating a QScatter3DSeries
    // >> I need this QMetaObject::invokeMethod because scatter_ object is in the main thread, i can't access it directly
    // >> because this class is meant to be run in another thread. This is the way to access it
    QMetaObject::invokeMethod(scatter_, [this, originArray, expectedPeakArray]() {
        QScatter3DSeries *originSeries = new QScatter3DSeries();
        originSeries->setName("amode3dorigin");
        originSeries->setItemSize(0.2f);
        originSeries->setMesh(QAbstract3DSeries::MeshPoint);
        originSeries->setBaseColor(Qt::red);
        originSeries->dataProxy()->resetArray(originArray);
        scatter_->addSeries(originSeries);

        if (expectedPeakArray->isEmpty())
            return;

        QScatter3DSeries *expectedPeakSeries = new QScatter3DSeries();
        expectedPeakSeries->setName("amode3dexpectedpeak");
        expectedPeakSeries->setItemSize(0.2f);
        expectedPeakSeries->setMesh(QAbstract3DSeries::MeshPoint);
        expectedPeakSeries->setBaseColor(Qt::blue);
        expectedPeakSeries->dataProxy()->resetArray(expectedPeakArray);

        // add the current origin point data (series) to our scatter object
        scatter_->addSeries(expectedPeakSeries);
    });

}

void VolumeAmodeVisualizer::setData(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody)
{
    qDebug() << "VolumeAmodeVisualizer::setData() setAmodeSignal called in thread:" << QThread::currentThread();
    qDebug() << "VolumeAmodeVisualizer::setData() Expected worker thread:" << this->thread();
    qDebug() << "VolumeAmodeVisualizer::setData() called, visualizing status: " << isVisualizing;

    QMutexLocker locker(&mutex);

    // If visualization is in progress, ignore the new data
    if (isVisualizing) return;

    qDebug() << "VolumeAmodeVisualizer::setData() visualization is idle, set the data";

    // set the data
    amodesignal_ = data_amode;
    currentT_holder_ref = data_rigidbody;

    // Set the flag to indicate new data has arrived
    hasNewData = true;
    // Wake up the thread if it was waiting
    // condition.wakeOne();
}

void VolumeAmodeVisualizer::test(const QVector<int16_t>& data_amode, const Eigen::Isometry3d& data_rigidbody)
{
    // qDebug() << "VolumeAmodeVisualizer::test() got paired data";

    if(isVisualizing)
    {
        // qDebug() << "VolumeAmodeVisualizer::test() but it still visualizing, ignore";
        return;
    }

    // QMutexLocker locker(&mutex);

    amodesignal_ = data_amode;
    currentT_holder_ref = data_rigidbody;
    hasNewData = true;      // Set the flag to indicate new data has arrived
}

void VolumeAmodeVisualizer::setExpectedPeak(int plotid, std::optional<double> xLineValue)
{
    expectedpeaks_.at(plotid) = xLineValue;
}

void VolumeAmodeVisualizer::processVisualization()
{
    while (true)
    {
        if (stopVisualization)
            break;

        // qDebug() << "VolumeAmodeVisualizer::processVisualization";

        if (!hasNewData)
        {
            QThread::msleep(10);
            continue;
        }

        QMutexLocker locker(&mutex);

        // Set flag to indicate visualization is in progress
        // qDebug() << "VolumeAmodeVisualizer::processVisualization() start visualization";
        isVisualizing = true;

        // Perform visualization task
        visualize3DSignal();        

        // something weird is going on, if i reduce the sleep, it seems like the threading
        // is unstable. There will be more queue if you reduce the sleep time. However,
        // the more you put the sleep, the more stable, no queueing. Fucking weird!
        // i need to implement better multi threading architecture, but i don't want to do it
        // now. It is (somewhat) working to some extent, so be it.
        QThread::msleep(200);


        // After visualization is done, reset the flag
        // qDebug() << "VolumeAmodeVisualizer::processVisualization() finish visualization";
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
