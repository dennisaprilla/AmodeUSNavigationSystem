#ifndef AMODECONNECTIONQTCP_H
#define AMODECONNECTIONQTCP_H

#include <QTcpSocket>
#include <QObject>

/**
 * @class AmodeConnection
 * @brief A class that handle the communication with A-mode PC.
 *
 * For the context. We (ET-BE Departement of University of Twente) have a A-mode Ultrasound Machine (Diagnostic Sonar)
 * which comes with its own PC. The PC is old and it is "forbidden" to develop a new software (such as Qt) because it
 * requires softwares installation or OS updates which will disrupt the ultrasound signal acquisition software in the PC.
 * So i created a program, based on LabView (in which the acquisition software based on) to send the data through TCP,
 * so that anybody now could stream the data using their own PC. This class is the class which handle the communication
 * with my software in the ultrasoun PC. See testgarbage_v21_streamWithPeaks.vi in the ultrasound PC for more detail.
 *
 * The structure of the data is very specific. It is in a long array of word (2 byte) which consists (n_probe x n_sample)
 * of data. It starts with array header (4 bytes) and data header (10 bytes). This class ensuring you get a right
 * interpretation of the data.
 *
 */

class AmodeConnection : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor function, it connect to the server directly by calling connectToServer() function
     */
    explicit AmodeConnection(QObject *parent, std::string ip, std::string port);

    /**
     * @brief Destructor function, making sure anything is closed properly
     */
     ~AmodeConnection();

    /**
     * @brief A function to connect to the server
     */
    int connectToServer();

    /**
    * @brief A function to check if the PC and A-mode ultrasound is already connected through TCP.
    * @return              A flag indicating the status.
    */
    bool isConnected();

    /**
     * @brief A function to set the program to record the streamed A-mode signal.
     * @param flag          Set true to record.
     */
    void setRecord(bool flag);

    /**
     * @brief A function to set the program to name the streamed A-mode signal with index.
     * The data that is sent by A-mode Ultrasound Machine contains index, starting from 0 to 1.
     * This index is used for tracking if there any data loss, you can notice it if there is a
     * gap between the index. This is usefull for debugging the code.
     *
     * @param flag          Set true to use undex.
     */
    void useDataIndex(bool flag);

    /**
     * @brief A function to specify the where the streamed data will be stored.
     *
     * @param directory     Path to the directory.
     * @return              A flag indicating the status. -1 if there is something wrong.
     */
    int setDirectory(std::string directory);

    /**
     * @brief This function is intended to perform preparation before we start streaming the data, such as
     * initialing buffer sizes (in bytes), how to handle data header (the data is sent with a header which
     * indicates it is an array), etc.
     */
    void initializeData();

    /**
     * @brief This function should be called if you are ready to start streaming the data.
     */
    void startStream();

    /**
     * @brief A function to get how many sample (element) within the signal.
     */
    int getNsample();


    /**
     * @brief A function to get how many probe used by the machine.
     */
    int getNprobe();

    void letsdelete();


private slots:
    /**
     * @brief Will be called whenever data is ready to read (a signal emitted by QTcpSocket)
     */
    void readData();

    /**
     * @brief Will be called whenever there is an error in the connection
     */
    void handleError(QTcpSocket::SocketError socketError);

private:
    QByteArray convertSTDVectorToQByteArray(const std::vector<uint16_t>& vector);

    // variables that stores the connection spec
    std::string ip_;                        //!< IP address of Ultrasound Machine
    std::string port_;                      //!< Port number of Ultrasound Machine

    // variables that stores amode spesifications
    int samples_       = 3500;              //!< The number of sample points in the signal (default 1500)
    int probes_        = 30;                //!< The number of ultrasound probes being used in the experiment (default 30)
    int datalength_    = samples_*probes_;  //!< samples_ * probes_
    int headersize_    = 4;                 //!< The number of bytes of the header of the data packet (the array, it has 4 bytes of header)
    int separatorsize_ = 10;                //!< The number of bytes of the separator (check amode machine for detail)
    int indexsize_     = 2;                 //!< The number of bytes which contains the information of the index (used for indexing)

    // variables that stores the information of the separators
    std::vector<int16_t>  separator_int16;  //!< Separators in int16 (each elements = 2bytes)
    std::vector<uint16_t> separator_uint16; //!< Separators in uint16 (each elements = 2bytes)
    QByteArray            separator_qbyte;  //!< Separators in qbytes (each elements = 1byte)

    // all variables that handle the receiving data
    QByteArray            buffer;           //!< A temporary array to store the data from QTcpSocket
    std::vector<uint16_t> usdata_uint16_;   //!< The real array/vector that store the raw data of the amode machine
    std::vector<int16_t>  usdata_int16_;    //!< The same as usdata_uint16_ just different datatype
    int usdata_framesize_     = 0;          //!< A frame defined as all the bytes from single timeframe of amode measurement (array header, separator, index, data)
    int usdata_allheadersize_ = 0;          //!< The number of bytes of all headers (array header, separator, and index)
    int usdata_datasize_      = 0;          //!< The number of bytes of only data
    /*
    std::vector<uint16_t> usdata_uint16_;   //!< A vector that will store raw data from ultrasound machine
    int usdata_receivebuffersize_ = 0;      //!< The number of bytes of all data, including header, index, and the data
    int usdata_headerindexsize_   = 0;      //!< The number of bytes of header (with or without index, depends on user choice)
    int usdata_datasize_          = 0;      //!< The number of bytes of only data
    */

    // variable that controls the behavior of this class
    bool setrecord_    = false;             //!< Flag for setting the status of the record
    bool usedatastart_ = false;             //!< Flag for using separator (start) data. True means, i want the separator, don't skip those bytes
    bool usedataindex_ = false;             //!< Flag for using index. False means i dont want the index, skip those bytes
    std::string fullpath_;                  //!< Full path to the directory where the data is stored
    std::string recorddirectory_;           //!< Local directory where the data is stored

    bool isDataReceived = false;

    // for counting the data streamed
    int count_streameddata_ = 0;            //!< Counting variable for how much data is streamed from the beginning of the program
    int count_recordeddata_ = 0;            //!< Counting variable for how much data is recorded

    QTcpSocket *tcpSocket;                  //!< Object to handle the tcp connection

signals:
    void dataReceived(const std::vector<uint16_t> &usdata_uint16_);
    void errorOccured();

};

#endif // AMODECONNECTIONQTCP_H
