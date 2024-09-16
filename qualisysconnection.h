#ifdef _WIN32
#define sleep Sleep
#else
#include <unistd.h>
#endif

#ifndef QUALISYSCONNECTION_H
#define QUALISYSCONNECTION_H

#include "RTProtocol.h"
#include "RTPacket.h"
#include "qualisystransformationmanager.h"

#include "Eigen/Dense"

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

/**
 * @class QualisysConnection
 * @brief A class that hangle the communication with Qualisys Motion Capture System
 *
 * For the context. Qualisys motion tracking system is used to provides the position and oritentation of both
 * B-mode ultrasound (for freehand tracking to generate bone surface) and A-mode ultrasound (for the navigation).
 * Qualisys has their own SDK, and i include them in the .pro file for this Qt project. All the protocol is
 * handled by the SDK, however, we still need to make our own program to call the functions, configure the settings,
 * grab the data, etc.
 *
 */

class QualisysConnection : public QObject
{
    Q_OBJECT
public:

    /**
     * @enum enumMessageType
     * @brief For the type of message in the terminal
     *
     * Deprecated. I used this when i develop the software for CMD only. But now, it is not really that relevent.
     * It is basically just to differentiate between message from A-mode machine and from Qualisys, and also to
     * differentiate type of message. That's all.
     */
    enum enumMessageType {
        MESSAGE_OK,
        MESSAGE_WARNING,
        MESSAGE_RUNNING
    };

    explicit QualisysConnection(QObject *parent = nullptr, std::string ip = "", unsigned short port = 22222);
    ~QualisysConnection();

    /**
     * @brief A print function which specify the source device (Qualisys) and type of message.
     *
     * There are 3 types of message: MESSAGE_OK [OK], MESSAGE_WARNING [!!], MESSAGE_RUNNING [>>].
     */
    void myprintFormat(QualisysConnection::enumMessageType messagetype, std::string message);

    /**
     * @brief A get function of the current transformation.
     *
     * @return An object of QualisysTransformationManager which stores the current transformation.
     */
    QualisysTransformationManager getTManager();

    /**
     * @brief Start qualisys streaming with QThread
     */
    void startStreaming();

    /**
     * @brief Stop qualisys streaming with QThread
     */
    void stopStreaming();

protected:
    /**
     * @brief Initializing connection to Qualisys, called in constructor.
     *
     * @return 0 success, -1 error occured.
    */
    int connectTCP();

    /**
     * @brief Reading the marker settings from Qualisys.
     * Retrieve the name of the rigid bodies that is detected by Qualisis. Will be used for logging.
     *
     * @return 0 success, -1 error occured.
    */
    int readMarkerSettings();

    /**
     * @brief Telling Qualisys that we are ready to start streaming
     *
     * @return 0 success, -1 error occured.
    */
    int startStreamFrames();


private slots:

    /**
     * @brief receive data from qualisys, called by QTimer
     */
    void receiveData();

    /**
     * @brief stream data from qualisys, called by QThread
     */
    void streamData();

private:
    CRTProtocol poRTProtocol_;

    std::string    ip_   = "127.0.0.1";         //!< Default IP address
    unsigned short port_ = 22222;               //!< Default port number (22222).

    const int      majorVersion = 1;            //!< Qualisys major version used for connecting to Qualisys (1).
    const int      minorVersion = 19;           //!< Qualisys minor version used for connecting to Qualisys (19).
    const bool     bigEndian = false;           //!< Default order of sequence (false).
    unsigned short udpPort = 6734;              //!< Default udp port (6734).

    bool statusQConnection_ = false;
    bool statusQMarker_     = false;
    bool statusQStream_     = false;

    std::vector<std::string> rigidbodyName_;    //!< Contains list of rigidbody names.
    std::vector<double> rigidbodyData_;         //!< Contains value of rigidbodies.
    QualisysTransformationManager tmanager;     //!< manage the rigid body transformation tracked by qualisys
    QTimer *timer;                              //!< A timer, in which we check the data from Qualisys

    // variables that handles multithreading for streaming qualisys data
    QThread m_workerThread;                     //!< The worker thread to run streaming data from qualisys function
    QMutex m_mutex;                             //!< Mutex variable that keep m_isRunning variable to be accessed by one thread at a time
    QWaitCondition m_condition;                 //!< Allows thread to sleep when idle and wake up on demand
    bool m_isRunning = false;                   //!< A flag that tells that the thread is running or not

signals:
    /**
     * @brief Emits a tmanager, which consists of transformations of rigid body that is detected by Qualisys
     */
    void dataReceived(const QualisysTransformationManager &tmanager);
};

#endif // QUALISYSCONNECTION_H
