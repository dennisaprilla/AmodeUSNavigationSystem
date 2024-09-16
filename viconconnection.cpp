#include "ViconConnection.h"
#include <QCoreApplication>
#include <iostream>

// Constructor
ViconConnection::ViconConnection(QObject *parent, const std::string& hostname)
    : MocapConnection{parent}
{
    std::cout << "Connecting to Vicon DataStream on " << hostname << "..." << std::endl;

    // Attempt to connect to the Vicon system
    while (!ViconClient.IsConnected().Connected)
    {
        ViconClient.Connect(hostname);
        QThread::sleep(1); // Retry every second
    }

    std::cout << "Connected to Vicon!" << std::endl;

    // Enable necessary data streams
    ViconClient.EnableMarkerData();
    ViconClient.EnableSegmentData();

    // Move this object to a new QThread
    this->moveToThread(&streamingThread);

    // Connect the thread's started signal to the run slot
    connect(&streamingThread, &QThread::started, this, &ViconConnection::run);
}

// Destructor
ViconConnection::~ViconConnection()
{
    // Stop the streaming thread
    streamingThread.quit();
    streamingThread.wait(); // Wait for the thread to finish

    // Disconnect from the Vicon system
    disconnect();
}

void ViconConnection::setDataStream(QString datatype)
{
    if (QString::compare(datatype, "rigidbody")==0) {
        isStreamRigidBody = true;
    }
    else if (QString::compare(datatype, "marker")==0){
        isStreamRigidBody = false;
    }
    else {
        std::cout << "ViconConnection::setDataStream | datatype unrecognized, use rigidbody as default instead";
        isStreamRigidBody = true;
    }
}

// Call this to start the thread and begin streaming
void ViconConnection::startStreaming()
{
    // Start the streaming thread
    streamingThread.start();
}

// The run function that gets called when the thread starts
void ViconConnection::run()
{
    while (streamingThread.isRunning())
    {
        if (isStreamRigidBody) streamRigidBody();
        else streamMarker();
    }
}

void ViconConnection::streamMarker()
{
    // Get a frame from the Vicon system
    if (ViconClient.GetFrame().Result != ViconDataStreamSDK::CPP::Result::Success)
    {
        std::cout << "Failed to get frame from Vicon." << std::endl;
        return;
    }

    // clear the transformation manager
    tmanager.clearTransformations();

    // Example of processing marker data
    unsigned int SubjectCount = ViconClient.GetSubjectCount().SubjectCount;
    // std::cout << "Number of subjects: " << SubjectCount << std::endl;

    // Loop for all subject that is detected
    for (unsigned int SubjectIndex = 0; SubjectIndex < SubjectCount; ++SubjectIndex)
    {
        // Get the current subject name
        std::string SubjectName = ViconClient.GetSubjectName(SubjectIndex).SubjectName;
        // std::cout << "Subject: " << SubjectName << std::endl;

        // Get the markers
        unsigned int MarkerCount = ViconClient.GetMarkerCount(SubjectName).MarkerCount;

        // Prepare a matrix to store the markers
        Eigen::MatrixXd markers;
        markers.resize(3, 0);

        // Loop trough all the markers
        for (unsigned int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
        {
            std::string MarkerName = ViconClient.GetMarkerName(SubjectName, MarkerIndex).MarkerName;
            auto MarkerTranslation = ViconClient.GetMarkerGlobalTranslation(SubjectName, MarkerName);
            // std::cout << "Marker " << MarkerName << ": ("
            //           << MarkerTranslation.Translation[0] << ", "
            //           << MarkerTranslation.Translation[1] << ", "
            //           << MarkerTranslation.Translation[2] << ")" << std::endl;

            Eigen::VectorXd newMarker(3);
            newMarker << MarkerTranslation.Translation[0], MarkerTranslation.Translation[1], MarkerTranslation.Translation[2];
            markers.conservativeResize(Eigen::NoChange, markers.cols() + 1);
            markers.col(markers.cols() - 1) = newMarker;
        }

        // Calculate rigidbody transformation
        Eigen::Isometry3d T;
        try {
            T = estimateRigidBodyTransformation(markers);
        } catch (const std::invalid_argument& e) {
            std::cout << "Caught exception in ViconConnection::streamMarker : " << e.what() << std::endl;
            continue;
        }

        // std::cout << "Transformation Matrix:\n";
        // for (int i = 0; i < 4; ++i) {
        //     for (int j = 0; j < 4; ++j) {
        //         printf("%.3f\t", T.matrix()(i, j));
        //     }
        //     std::cout << std::endl; // Newline after each row
        // }

        // store the transformation matrix with the transformation manager
        tmanager.addTransformation(SubjectName, T);
    }

    // Once the task is finished, emit the signal
    emit dataReceived(tmanager);

    // Sleep to prevent flooding the system with requests
    QThread::msleep(1000);
}

// Streaming function
void ViconConnection::streamRigidBody()
{
    // Get a frame from the Vicon system
    if (ViconClient.GetFrame().Result != ViconDataStreamSDK::CPP::Result::Success)
    {
        std::cout << "Failed to get frame from Vicon." << std::endl;
        return;
    }

    // clear the transformation manager
    tmanager.clearTransformations();

    // Example of processing marker data
    unsigned int SubjectCount = ViconClient.GetSubjectCount().SubjectCount;
    // std::cout << "Number of subjects: " << SubjectCount << std::endl;

    // Loop for all subject that is detected
    for (unsigned int SubjectIndex = 0; SubjectIndex < SubjectCount; ++SubjectIndex)
    {
        // Initialize rigid body transformation
        Eigen::Isometry3d T = Eigen::Isometry3d::Identity();
        Eigen::Vector3d t;
        Eigen::Matrix3d R;

        // Get the current subject name
        std::string SubjectName = ViconClient.GetSubjectName(SubjectIndex).SubjectName;
        // std::cout << "Subject: " << SubjectName << std::endl;

        // Get the translation (from the root segment)
        auto tmp_t = ViconClient.GetSegmentGlobalTranslation(SubjectName, "CalibObj");
        if (tmp_t.Result == ViconDataStreamSDK::CPP::Result::Success)
        {
            // std::cout << "t = ("
            //           << tmp_t.Translation[0] << ", "
            //           << tmp_t.Translation[1] << ", "
            //           << tmp_t.Translation[2] << ")" << std::endl;
            t << tmp_t.Translation[0], tmp_t.Translation[1], tmp_t.Translation[2];
        }
        else
        {
            std::cout << "Failed to get subject translation!" << std::endl;
            return;
        }

        // Get the rotation (from the root segment)
        auto tmp_R = ViconClient.GetSegmentGlobalRotationMatrix(SubjectName, "CalibObj");
        if (tmp_R.Result == ViconDataStreamSDK::CPP::Result::Success)
        {
            // Accessing the elements of the 3x3 matrix, which is likely stored as a flat array of 9 elements
            // std::cout << "R = ("
            //           << tmp_R.Rotation[0] << ", " << tmp_R.Rotation[1] << ", " << tmp_R.Rotation[2] << ", "
            //           << tmp_R.Rotation[3] << ", " << tmp_R.Rotation[4] << ", " << tmp_R.Rotation[5] << ", "
            //           << tmp_R.Rotation[6] << ", " << tmp_R.Rotation[7] << ", " << tmp_R.Rotation[8] << ")" << std::endl;
            R << tmp_R.Rotation[0], tmp_R.Rotation[1], tmp_R.Rotation[2],
                 tmp_R.Rotation[3], tmp_R.Rotation[4], tmp_R.Rotation[5],
                 tmp_R.Rotation[6], tmp_R.Rotation[7], tmp_R.Rotation[8];
        }
        else
        {
            std::cout << "Failed to get subject rotation!" << std::endl;
            return;
        }

        // store the rotation and translation into one rigid body transformation
        T.translation() = t;
        T.linear() = R;

        // store the transformation matrix with the transformation manager
        tmanager.addTransformation(SubjectName, T);
    }

    // Sleep to prevent flooding the system with requests
    QThread::msleep(1000);

    // Once the task is finished, emit the signal
    emit dataReceived(tmanager);
}

// Disconnect the client
void ViconConnection::disconnect()
{
    std::cout << "Disconnecting from Vicon..." << std::endl;
    ViconClient.Disconnect();
    std::cout << "Disconnected." << std::endl;
}

// Function to estimate rigid body transformation
Eigen::Isometry3d ViconConnection::estimateRigidBodyTransformation(const Eigen::MatrixXd& points)
{
    // Check that the input matrix has at least 3 points (N >= 3)
    if (points.cols() < 3) {
        throw std::invalid_argument("Matrix must have at least 3 points (3 columns).");
    }

    // Extract the first three points
    Eigen::Vector3d p1 = points.col(0);
    Eigen::Vector3d p2 = points.col(1);
    Eigen::Vector3d p3 = points.col(2);

    // Step 1: Compute the centroid of the three points
    Eigen::Vector3d centroid = p1;

    // Step 2: Translate the points so that the centroid is at the origin
    Eigen::Vector3d p1_prime = p1 - centroid;
    Eigen::Vector3d p2_prime = p2 - centroid;
    Eigen::Vector3d p3_prime = p3 - centroid;

    // Step 3: Define the first vector (V1) as p2' - p1'
    Eigen::Vector3d v1 = p2_prime - p1_prime;
    v1.normalize(); // Normalize V1

    // Step 4: Define the second vector (V2) as p3' - p1'
    Eigen::Vector3d v2 = p3_prime - p1_prime;

    // Orthogonalize V2 by subtracting the projection of V2 onto V1
    Eigen::Vector3d v2_proj = v1 * (v1.dot(v2));
    Eigen::Vector3d v2_orthogonal = v2 - v2_proj;
    v2_orthogonal.normalize(); // Normalize the orthogonalized V2

    // Step 5: Compute the third orthogonal vector (V3) using the cross product
    Eigen::Vector3d v3 = v1.cross(v2_orthogonal);
    v3.normalize(); // Normalize V3

    // Step 6: Construct the rotation matrix from the orthonormal vectors
    Eigen::Matrix3d rotation;
    rotation.col(0) = v1;
    rotation.col(1) = v2_orthogonal;
    rotation.col(2) = v3;

    // Step 7a: Apply SVD to ensure the matrix is orthogonal
    Eigen::JacobiSVD<Eigen::Matrix3d> svd(rotation, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3d U = svd.matrixU();
    Eigen::Matrix3d V = svd.matrixV();
    rotation = U * V.transpose();

    // Step 7b: Ensure the determinant is +1 (valid rotation matrix)
    if (rotation.determinant() < 0) {
        rotation.col(2) *= -1; // Flip the third column to ensure a positive determinant
    }

    // Step 8: Create the transformation matrix (Eigen::Isometry3D)
    Eigen::Isometry3d rigidbodyT = Eigen::Isometry3d::Identity();
    rigidbodyT.linear() = rotation;    // Set the rotation part
    rigidbodyT.translation() = centroid; // Set the translation part

    // Return the rigid body transformation
    return rigidbodyT;
}
