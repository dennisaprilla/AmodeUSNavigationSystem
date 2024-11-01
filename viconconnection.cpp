#include "ViconConnection.h"
#include <QCoreApplication>
#include <iostream>
#include <regex>

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

void ViconConnection::setDataStream(QString datatype, bool useForce)
{
    if (QString::compare(datatype, "rigidbody")==0) {
        isStreamRigidBody = true;
    }
    else if (QString::compare(datatype, "marker")==0){
        isStreamRigidBody = false;
    }
    else {
        qDebug() << "ViconConnection::setDataStream() datatype unrecognized, use rigidbody as default instead";
        isStreamRigidBody = true;
    }

    // set the flag for streaming force plate
    isStreamForce = useForce;
    qDebug() << "ViconConnection::setDataStream() force plate analog data is set to true";
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
        // Get a frame from the Vicon system
        if (ViconClient.GetFrame().Result != ViconDataStreamSDK::CPP::Result::Success)
        {
            std::cout << "Failed to get frame from Vicon." << std::endl;
            continue;
        }

        if (isStreamRigidBody) streamRigidBody();
        else streamMarker();

        if(isStreamForce) streamForce();
    }
}

void ViconConnection::streamMarker()
{
    // // Get a frame from the Vicon system
    // if (ViconClient.GetFrame().Result != ViconDataStreamSDK::CPP::Result::Success)
    // {
    //     std::cout << "Failed to get frame from Vicon." << std::endl;
    //     return;
    // }

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

        // // Prepare a matrix to store the markers
        // Eigen::MatrixXd markers;
        // markers.resize(3, 0);

        // Prepare a vector to store the markers object
        std::vector<ViconConnection::MarkerObject> all_markers;

        // Loop trough all the markers
        for (unsigned int marker_index = 0; marker_index < MarkerCount; ++marker_index)
        {
            std::string marker_name = ViconClient.GetMarkerName(SubjectName, marker_index).MarkerName;
            auto marker_translation = ViconClient.GetMarkerGlobalTranslation(SubjectName, marker_name);

            // std::cout << "Marker " << MarkerName << ": ("
            //           << MarkerTranslation.Translation[0] << ", "
            //           << MarkerTranslation.Translation[1] << ", "
            //           << MarkerTranslation.Translation[2] << ")" << std::endl;

            // Eigen::VectorXd newMarker(3);
            // newMarker << MarkerTranslation.Translation[0], MarkerTranslation.Translation[1], MarkerTranslation.Translation[2];
            // markers.conservativeResize(Eigen::NoChange, markers.cols() + 1);
            // markers.col(markers.cols() - 1) = newMarker;

            // check the validity of the group name and extract the group name
            std::string marker_group = getMarkerGroupName(marker_name);
            // assign to a struct and store it in the vector
            ViconConnection::MarkerObject current_marker(marker_name, marker_group,
                                                         marker_translation.Translation[0],
                                                         marker_translation.Translation[1],
                                                         marker_translation.Translation[2]);
            all_markers.push_back(current_marker);
        }

        // Container to store MarkerObject based on group
        std::unordered_map<std::string, Eigen::MatrixXd> grouped_markers;
        // Group MarkerObject and collect their positions
        groupMarkerObjectByGroup(all_markers, grouped_markers);

        // Loop for each group of markers
        for (const auto& [group, positions] : grouped_markers) {

            // Calculate rigidbody transformation of the current marker group
            Eigen::Isometry3d T;
            try {
                T = estimateRigidBodyTransformation(positions, group);
            } catch (const std::invalid_argument& e) {
                std::cout << "Caught exception in ViconConnection::streamMarker : " << e.what() << std::endl;
                continue;
            }

            // Store the transformation matrix with the transformation manager
            tmanager.addTransformation(group, T);
        }

        // std::cout << "Transformation Matrix:\n";
        // for (int i = 0; i < 4; ++i) {
        //     for (int j = 0; j < 4; ++j) {
        //         printf("%.3f\t", T.matrix()(i, j));
        //     }
        //     std::cout << std::endl; // Newline after each row
        // }
    }

    // Once the task is finished, emit the signal
    emit dataReceived(tmanager);

    // Sleep to prevent flooding the system with requests
    QThread::msleep(1);
}

// Streaming function
void ViconConnection::streamRigidBody()
{
    // // Get a frame from the Vicon system
    // if (ViconClient.GetFrame().Result != ViconDataStreamSDK::CPP::Result::Success)
    // {
    //     std::cout << "Failed to get frame from Vicon." << std::endl;
    //     return;
    // }

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
    QThread::msleep(1);

    // Once the task is finished, emit the signal
    emit dataReceived(tmanager);
}

void ViconConnection::streamForce()
{
    // Output the force plate information.
    unsigned int ForcePlateCount = ViconClient.GetForcePlateCount().ForcePlateCount;

    // Check if there any force plate detected in Vicon system
    if (ForcePlateCount==0) return;

    // Here, i only want to use the first force plate. Because i am not using the force plate as
    // for analysis, but only for media for synchronization
    unsigned int ForcePlateSubsamples = ViconClient.GetForcePlateSubsamples( 0 ).ForcePlateSubsamples;

    // Create an Nx3 matrix to store 3D vectors
    Eigen::MatrixXd vectors(ForcePlateSubsamples, 3);

    // Loop for all of the subsamples
    for( unsigned int ForcePlateSubsample = 0; ForcePlateSubsample < ForcePlateSubsamples; ++ForcePlateSubsample )
    {

        // Get the force vector
        ViconDataStreamSDK::CPP::Output_GetGlobalForceVector _Output_GetForceVector =
            ViconClient.GetGlobalForceVector( 0, ForcePlateSubsample );

        // Store the force vector
        vectors.row(ForcePlateSubsample) << _Output_GetForceVector.ForceVector[0], _Output_GetForceVector.ForceVector[1], _Output_GetForceVector.ForceVector[2];
    }

    // Resize or initialize the magnitudes vector based on N
    fmagnitudes.resize(ForcePlateSubsamples);

    // Calculate and store magnitudes
    fmagnitudes = vectors.rowwise().norm();

    // Emit the signal that pass the fmagnitudes
    emit forceReceived(fmagnitudes);
}

// Disconnect the client
void ViconConnection::disconnect()
{
    std::cout << "Disconnecting from Vicon..." << std::endl;
    ViconClient.Disconnect();
    std::cout << "Disconnected." << std::endl;
}

// Function to estimate rigid body transformation
Eigen::Isometry3d ViconConnection::estimateRigidBodyTransformation(const Eigen::MatrixXd& points, std::string group)
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

    // Additonal step for some very special cases for ref and probe rigid bodies.
    // I have made all of our marker arrangement for amode holder have a similar structure.
    //
    // Viewed from the bird eye view:
    // 1. Origin is always bottom left
    // 2. x is always to the right of origin AND asummed to be the reference axis that can't be skewed
    // 3. y is always to the top of the origin and can be skewed, as i can make adjustment of the
    //    skewedness direction of y to be perpendicular to the x-axis
    // I have made marker definition in Vicon for rigid body as p1=origin, p2=x, p3=y.
    //
    // However, xyz direction of that is used by fCal for ref/probe rigid body is different.
    // Viewed from the bird eye view:
    // 1. Origin is always at the bottom
    // 2. z is always to the top AND asummed to be the reference axis that can't be skewed.\
    // 3. y is always to the right of the origin and can be skewed.
    //
    // See this diagram below for detail:
    //   o (p2)         o (p3)
    // ^ | o (p3)     ^ |
    // | |/           | |(p1)
    // z o (p1)       y o---o(p2)
    //   y-->           x-->
    // (ref/probe)    (amode holders)
    //
    // So, what i will do, when this function is called from markers from ref/probe rigid body,
    // i will pretend their z-direction marker as first vector, and their y-direction as second vector.
    // But i need to remember that later i need to swap the vector when i want to pack them into matrix.
    //
    // With important note. I need to negate the resulting third vector, because if (viewed from bird eye view)
    // first vector is pointed upward, and second vector is pointed to the right, then the resulting
    // third vector will be pointed away from the view (as the result of the right-hand rule cross product).
    // Fcal needs to have the third vector to be pointed to the view. So i need to negate the third vector.

    // Step 6: Construct the rotation matrix from the orthonormal vectors
    Eigen::Matrix3d rotation;
    if(group==transformationID_probe || group==transformationID_ref)
    {
        rotation.col(0) = -v3;
        rotation.col(1) = v2_orthogonal;
        rotation.col(2) = v1;
    }
    else
    {
        rotation.col(0) = v1;
        rotation.col(1) = v2_orthogonal;
        rotation.col(2) = v3;
    }

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

// https://chatgpt.com/share/66eaa73b-8ac0-8010-a5c9-165cd15f82db
std::string ViconConnection::getMarkerGroupName(const std::string& input)
{
    // Define the regular expression based on the given rules
    std::regex pattern(R"((A|B)_(T|F|N)_[A-Z]{3}_[1-5])");

    // if does not match, return empty string
    if(!std::regex_match(input, pattern)) return "INVALID";

    // if match, let's continue, get the last underscore
    size_t pos = input.rfind('_');
    // return the group name
    return (pos != std::string::npos) ? input.substr(0, pos) : input;
}

// https://chatgpt.com/share/66eaa713-9218-8010-936f-0b608985cb58
void ViconConnection::groupMarkerObjectByGroup(const std::vector<MarkerObject>& markers, std::unordered_map<std::string, Eigen::MatrixXd>& groupedPositions)
{
    std::unordered_map<std::string, std::vector<Eigen::VectorXd>> tempGroups;

    // Group markers by group and collect positions
    for (const auto& marker : markers) {
        tempGroups[marker.group].push_back(marker.position);
    }

    // Convert to Eigen::MatrixXd, each group gets a 3xN matrix
    for (const auto& [group, positions] : tempGroups) {
        size_t N = positions.size();
        Eigen::MatrixXd groupMatrix(3, N);

        for (size_t i = 0; i < N; ++i) {
            groupMatrix.col(i) = positions[i];
        }

        groupedPositions[group] = groupMatrix;
    }
}
