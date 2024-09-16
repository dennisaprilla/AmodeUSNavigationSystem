#include "qualisysconnection.h"
#include <iostream>

QualisysConnection::QualisysConnection(QObject *parent, std::string ip, unsigned short port)
    : QObject{parent}, ip_{ip}, port_{port}
{
    if(!this->connectTCP()) return;
    if(!this->readMarkerSettings()) return;
    if(!this->startStreamFrames()) return;

    // // Setup timer for frame updates
    // timer = new QTimer(this);
    // connect(timer, &QTimer::timeout, this, &QualisysConnection::receiveData);
    // // start the timer
    // timer->start(30);

    // connect the workerthread object
    moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::started, this, &QualisysConnection::streamData);
}

QualisysConnection::~QualisysConnection()
{
    // stop streaming and tell the worker to quit
    stopStreaming();
    m_workerThread.quit();
    m_workerThread.wait();

    // disconnect the RTProtocol
    if (!poRTProtocol_.Connected()) return;
    poRTProtocol_.StopCapture();
    poRTProtocol_.Disconnect();
}

int QualisysConnection::connectTCP()
{
    // if there is no connection yet to Qualisys...
    if (!poRTProtocol_.Connected())
    {
        // ...let's make connection
        if (!poRTProtocol_.Connect((char*)ip_.data(), port_, &udpPort, majorVersion, minorVersion, bigEndian))
        {
            // if fails, print the error message and return -1;
            myprintFormat(QualisysConnection::MESSAGE_WARNING, poRTProtocol_.GetErrorString());
            sleep(1);
            return 0;
        }
    }
    // if there is nothing wrong, return 0;
    char strbuffer[100];
    sprintf(strbuffer, "Connected to the Qualisys Motion Tracking system specified at ip: %s, port: %d", ip_.c_str(), port_);
    myprintFormat(QualisysConnection::MESSAGE_OK, strbuffer);
    return 1;
}

int QualisysConnection::readMarkerSettings()
{
    // read the rigid body settings
    bool bDataAvailable;

    // check if the 6D information needed is already available
    if (!poRTProtocol_.Read6DOFSettings(bDataAvailable))
    {
        char strbuffer[100];
        sprintf(strbuffer, "Read settings fails: %s.", poRTProtocol_.GetErrorString());
        myprintFormat(QualisysConnection::MESSAGE_WARNING, strbuffer);
        //synch::setStop(true);
        return 0;
    }

    // get the rigid body settings. well, tbh, here, we don't need to retrive all the settings data
    // (color, position, scale, etc.). So, i will commented this part (until i think that i need it)
    // std::vector<CRTProtocol::SSettings6DOFBody> bodiesSettings;
    // poRTProtocol_.Get6DOFBodySettings(bodiesSettings);

    // get the number of rigid body detected by Qualisys
    int nBodies = poRTProtocol_.Get6DOFBodyCount();
    // get the data only if there is more than one rigid body detected.
    if (nBodies > 0)
    {
        // loop over all the rigid bodies detected
        for (int iBody = 0; iBody < nBodies; iBody++)
        {
            // retrieve each rigid body's name
            //const char* tmpName = poRTProtocol_.Get6DOFBodyName(iBody);
            std::string tmpstr(poRTProtocol_.Get6DOFBodyName(iBody));
            rigidbodyName_.push_back(tmpstr);
        }
    }

    // print notification for user
    myprintFormat(QualisysConnection::MESSAGE_OK, "Recieved 6DoF settings from Qualisys.");
    return 1;
}

int QualisysConnection::startStreamFrames()
{
    if (!poRTProtocol_.StreamFrames(CRTProtocol::RateAllFrames, 0, udpPort, NULL, CRTProtocol::cComponent6d))
    {
        char strbuffer[100];
        sprintf(strbuffer, "Start stream fails: %s.", poRTProtocol_.GetErrorString());
        return 0;
    }

    // print notification for user
    myprintFormat(QualisysConnection::MESSAGE_OK, "Starting to streaming 6DOF data.");
    return 1;
}

void QualisysConnection::receiveData() {
    // variable to capture packettype (error/packetdata/end)
    CRTPacket::EPacketType ePacketType;

    // check if receiving data is a success
    if (poRTProtocol_.Receive(ePacketType, true) != CNetwork::ResponseType::success)
    {
        return;
    }

    // check if packet type is packet data
    if (ePacketType != CRTPacket::PacketData)
    {
        return;
    }

    // get a packet
    CRTPacket* rtPacket = poRTProtocol_.GetRTPacket();

    // variable for 6DOF value (translation and rotation)
    float tmp_tX, tmp_tY, tmp_tZ;
    float tmp_R[9];

    // clear the transformation manager
    tmanager.clearTransformations();

    // loop for all rigid body detected
    for (unsigned int i = 0; i < rtPacket->Get6DOFBodyCount(); i++)
    {
        // get the rigid body values
        if(!rtPacket->Get6DOFBody(i, tmp_tX, tmp_tY, tmp_tZ, tmp_R))
        {
            return;
        }

        // do something with the values
        const char* name_6DOF = poRTProtocol_.Get6DOFBodyName(i);
        std::string str(name_6DOF);

        // // print values, for debugging purposes
        // std::cout << name_6DOF;
        // std::cout << std::fixed << std::setprecision(2);
        // std::cout << "\tt: [" <<  tmp_tX << ", " << tmp_tY << ", " <<  tmp_tZ << "]";
        // std::cout << "\tR: [" << tmp_R[0] << ", " << tmp_R[1] << ", " << tmp_R[2] << ", ";
        // std::cout << tmp_R[3] << ", " << tmp_R[4] << ", " << tmp_R[5] << ", ";
        // std::cout << tmp_R[6] << ", " << tmp_R[7] << ", " << tmp_R[8] << "]" << std::endl;

        // Create the Eigen objects for rotation and translation
        Eigen::Matrix3d R;
        Eigen::Vector3d t;

        // Copy the data from your arrays to the Eigen objects
        // R << tmp_R[0], tmp_R[1], tmp_R[2],
        //      tmp_R[3], tmp_R[4], tmp_R[5],
        //      tmp_R[6], tmp_R[7], tmp_R[8];
        // The element of rotation matrix (tmp_R) from qualisys is defined like below
        // the first three element of tmp_R defined as first column of the matrix, not first row
        R << tmp_R[0], tmp_R[3], tmp_R[6],
             tmp_R[1], tmp_R[4], tmp_R[7],
             tmp_R[2], tmp_R[5], tmp_R[8];
        t << tmp_tX, tmp_tY, tmp_tZ;

        // Create an Isometry3d object and set its rotation and translation components
        Eigen::Isometry3d T = Eigen::Isometry3d::Identity();
        T.linear() = R;
        T.translation() = t;

        // store the transformation matrix with the transformation manager
        tmanager.addTransformation(name_6DOF, T);
    }

    // std::cout << std::endl << std::flush;

    // Once the task is finished, emit the signal
    emit dataReceived(tmanager);
}

void QualisysConnection::startStreaming()
{
    QMutexLocker locker(&m_mutex);
    if (!m_isRunning) {
        m_isRunning = true;
        m_workerThread.start();
    }
}

void QualisysConnection::stopStreaming()
{
    QMutexLocker locker(&m_mutex);
    m_isRunning = false;
    m_condition.wakeOne();
}

void QualisysConnection::streamData()
{
    while (true)
    {
        // create new scope for QMutexLocker Object
        {
            QMutexLocker locker(&m_mutex);
            while (!m_isRunning) {
                m_condition.wait(&m_mutex);
            }
            if (!m_isRunning) {
                break;
            }
        }

        // variable to capture packettype (error/packetdata/end)
        CRTPacket::EPacketType ePacketType;

        // check if receiving data is a success
        if (poRTProtocol_.Receive(ePacketType, true) != CNetwork::ResponseType::success)
        {
            return;
        }

        // check if packet type is packet data
        if (ePacketType != CRTPacket::PacketData)
        {
            return;
        }

        // get a packet
        CRTPacket* rtPacket = poRTProtocol_.GetRTPacket();

        // variable for 6DOF value (translation and rotation)
        float tmp_tX, tmp_tY, tmp_tZ;
        float tmp_R[9];

        // clear the transformation manager
        tmanager.clearTransformations();

        // loop for all rigid body detected
        for (unsigned int i = 0; i < rtPacket->Get6DOFBodyCount(); i++)
        {
            // get the rigid body values
            if(!rtPacket->Get6DOFBody(i, tmp_tX, tmp_tY, tmp_tZ, tmp_R))
            {
                return;
            }

            // do something with the values
            const char* name_6DOF = poRTProtocol_.Get6DOFBodyName(i);
            std::string str(name_6DOF);

            // Create the Eigen objects for rotation and translation
            Eigen::Matrix3d R;
            Eigen::Vector3d t;

            // Copy the data from your arrays to the Eigen objects
            // R << tmp_R[0], tmp_R[1], tmp_R[2],
            //     tmp_R[3], tmp_R[4], tmp_R[5],
            //     tmp_R[6], tmp_R[7], tmp_R[8];
            // t << tmp_tX, tmp_tY, tmp_tZ;
            // The element of rotation matrix (tmp_R) from qualisys is defined like below
            // the first three element of tmp_R defined as first column of the matrix, not first row
            R << tmp_R[0], tmp_R[3], tmp_R[6],
                 tmp_R[1], tmp_R[4], tmp_R[7],
                 tmp_R[2], tmp_R[5], tmp_R[8];
            t << tmp_tX, tmp_tY, tmp_tZ;

            // Create an Isometry3d object and set its rotation and translation components
            Eigen::Isometry3d T = Eigen::Isometry3d::Identity();
            T.linear() = R;
            T.translation() = t;

            // store the transformation matrix with the transformation manager
            tmanager.addTransformation(name_6DOF, T);
        }

        // Once the task is finished, emit the signal
        emit dataReceived(tmanager);

        // Add a small delay to prevent excessive CPU usage
        QThread::msleep(1);
    }
}


QualisysTransformationManager QualisysConnection::getTManager(){
    return tmanager;
}

void QualisysConnection::myprintFormat(QualisysConnection::enumMessageType messagetype, std::string message)
{
    std::string header = "[Qualisys]";
    std::string icon = "";

    if (messagetype == QualisysConnection::MESSAGE_OK) icon = "[OK]";
    else if (messagetype == QualisysConnection::MESSAGE_WARNING) icon = "[!!]";
    else if (messagetype == QualisysConnection::MESSAGE_RUNNING) icon = "[..]";
    else printf(" ?? ");

    //printf("%s\t%s %s\n", header.c_str(), icon.c_str(), message.c_str());
    std::cout << header.c_str() << '\t' << icon.c_str() << ' ' << message.c_str() << '\n' << std::flush;
}
