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
    // Constructor: initializes the connection and starts the streaming
    ViconConnection(QObject *parent = nullptr, const std::string& hostname = "");

    // Destructor: stops the streaming and disconnects
    ~ViconConnection();

    // Function to start the streaming thread
    void startStreaming() override;

    // Funciton to set data stream
    void setDataStream(QString datatype);

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

    /**
     * @brief Disconnects the client
     */
    void disconnect();

    /**
     * @brief To estimate rigid body transformation
     */
    Eigen::Isometry3d estimateRigidBodyTransformation(const Eigen::MatrixXd& points);


    QualisysTransformationManager tmanager;     //!< manage the rigid body transformation tracked by qualisys

    bool isStreamRigidBody = true;              //!< flag for streaming markers

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
