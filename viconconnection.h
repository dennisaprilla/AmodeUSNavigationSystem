#ifndef VICON_CONNECTION_H
#define VICON_CONNECTION_H

#include "mocapconnection.h"
// #include "qualisystransformationmanager.h"

#include <DataStreamClient.h>
#include <QThread>
#include <QObject>

#include <Eigen/Dense>
#include <Eigen/Geometry>

class ViconConnection : public MocapConnection
{
    Q_OBJECT

public:

    struct MarkerObject {
        std::string name;         // Name of the marker
        std::string group;        // Group name (default-initialized)
        Eigen::VectorXd position; // 3D position

        // Default constructor
        MarkerObject() : name(""), group(""), position(3) {}

        // Constructor with position and name
        MarkerObject( const std::string& markerName, const std::string& markerGroup, double x, double y, double z)
            : name(markerName), group(markerGroup), position(3) {
            position << x, y, z;
        }
    };

    // Constructor: initializes the connection and starts the streaming
    ViconConnection(QObject *parent = nullptr, const std::string& hostname = "");

    // Destructor: stops the streaming and disconnects
    ~ViconConnection();

    // Function to start the streaming thread
    void startStreaming() override;

    // Funciton to set data stream
    void setDataStream(QString datatype, bool useForce) override;

    /**
     * @brief A get function of the current transformation.
     *
     * @return An object of QualisysTransformationManager which stores the current transformation.
     */
    const QualisysTransformationManager& getTManager() const override;

private:
    /**
     * @brief Client object to interact with the Vicon system
     */
    ViconDataStreamSDK::CPP::Client ViconClient;

    /**
     * @brief Streaming thread object using QThread
     */
    QThread streamingThread;

    /**
     * @brief Streaming function that runs on a separate thread
     */
    void streamRigidBody();
    void streamMarker();
    void streamForce();

    /**
     * @brief Disconnects the client.
     * To be honest, i should name this stopStreaming() so that it is similar to QualisysConnection.
     */
    void disconnect();

    /**
     * @brief To estimate rigid body transformation
     */
    Eigen::Isometry3d estimateRigidBodyTransformation(const Eigen::MatrixXd& points, std::string group);

    /**
     * @brief Checks the validity of the markers name and extract the group name from it
     */
    std::string getMarkerGroupName(const std::string& input);

    /**
     * @brief Checks the validity of the markers name and extract the group name
     */
    void groupMarkerObjectByGroup(const std::vector<MarkerObject>& markers, std::unordered_map<std::string, Eigen::MatrixXd>& groupedPositions);


    QualisysTransformationManager tmanager;     //!< manage the rigid body transformation tracked by qualisys
    Eigen::VectorXd fmagnitudes;                //!< store the force plate data

    bool isStreamRigidBody = false;             //!< flag for what kind of data you want to stream, true=rigidbodies, false=markers;
    bool isStreamForce     = false;             //!< flag if you want to include force data streaming from force plate

    std::string transformationID_probe = "B_N_PRB"; //!< When i want to calculate the basis vector from markers, B-mode probe reigid bodiy has specific arangement, this variable is to check that condition
    std::string transformationID_ref = "B_N_REF";   //!< Similar to transformationID_probe

public slots:
    // Slot to perform the actual streaming
    void run();

// signals:
//     /**
//      * @brief Emits a tmanager, which consists of transformations of rigid body that is detected by Qualisys
//      */
//     void dataReceived(const QualisysTransformationManager &tmanager);
};

#endif // VICON_CONNECTION_H
