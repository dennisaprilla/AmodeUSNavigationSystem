#ifndef AMODEMOCAPRECORDER_H
#define AMODEMOCAPRECORDER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QDateTime>
#include <vector>
#include <QStringList>
#include <Eigen/Geometry>

#include "QualisysTransformationManager.h"
#include "datawriter.h"
#include "imagewriter.h"

/**
 * @brief The AmodeMocapRecorder class.
 *
 * This class is responsible for managing the process of recording data from two sources:
 * Rigid Body data and Ultrasound data. It manages separate threads for handling data writing
 * (CSV files) and image writing (TIFF files). The class listens for incoming data, processes it,
 * and records it when instructed. It also provides methods to start and stop the recording process.
 * Signals are emitted to coordinate the starting and stopping of writing operations in associated
 * classes, ensuring that all data is properly saved to disk without blocking the main application thread.
 */
class AmodeMocapRecorder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for the AmodeMocapRecorder class.
     * Initializes the AmodeMocapRecorder instance, sets up separate threads for data and image writers,
     * and connects appropriate signals and slots to handle recording operations.
     * @param parent Pointer to the parent QObject. Defaults to nullptr if not provided.
     */
    explicit AmodeMocapRecorder(QObject *parent = nullptr);

    /**
     * @brief Destructor of the AmodeMocapRecorder class.
     * Ensures all threads are properly closed before the object is destroyed, preventing data loss or memory leaks.
     */
    ~AmodeMocapRecorder();

    /**
     * @brief Sets the file path where recorded data will be saved.
     * This method sets the directory path that will be used by both DataWriter and ImageWriter to store CSV and image files.
     * @param filePath A QString that specifies the path where the files should be saved.
     */
    void setFilePath(const QString &filePath);

signals:
    /**
     * @brief Signal to initiate data writing.
     * Emitted when recording starts to notify the DataWriter and ImageWriter to begin writing data to files.
     */
    void startWriter();

    /**
     * @brief Signal to stop data writing.
     * Emitted when recording stops to notify the DataWriter and ImageWriter to stop writing data to files.
     */
    void stopWriter();

public slots:
    /**
     * @brief Slot to handle incoming Rigid Body data.
     * This function is called when new Rigid Body data is received. It stores the data, checks if recording conditions are met,
     * and attempts to start or continue data processing.
     * @param tmanager An instance of QualisysTransformationManager containing the latest Rigid Body transformation data.
     */
    void onRigidBodyReceived(const QualisysTransformationManager &tmanager);

    /**
     * @brief Slot to handle incoming Ultrasound data.
     * This function is called when new ultrasound data is received. It stores the data, checks if recording conditions are met,
     * and attempts to start or continue data processing.
     * @param usdata_uint16_ A vector of 16-bit unsigned integers representing the latest ultrasound data.
     */
    void onAmodeSignalReceived(const std::vector<uint16_t> &usdata_uint16_);

    /**
     * @brief Starts recording data.
     * Initiates the recording process if both Rigid Body and Ultrasound data are available. If data is not yet available, sets a pending request flag.
     */
    void startRecording();

    /**
     * @brief Stops recording data.
     * Stops the ongoing recording process by emitting the appropriate signal to stop the DataWriter and ImageWriter.
     */
    void stopRecording();

private:
    /**
     * @brief Attempts to process the latest pair of Rigid Body and Ultrasound data.
     * This function checks if both the Rigid Body and Ultrasound data are available. If both are available, it proceeds to process them together.
     */
    void tryProcessDataPair();

    /**
     * @brief Processes a pair of Rigid Body and Ultrasound data.
     * Processes the provided Rigid Body transformation data and ultrasound data, reshapes the ultrasound data into an image,
     * and saves both data types using the DataWriter and ImageWriter.
     * @param tmanager An instance of QualisysTransformationManager containing Rigid Body data.
     * @param usdata A vector of 16-bit unsigned integers representing ultrasound data.
     */
    void processDataPair(const QualisysTransformationManager &tmanager, const std::vector<uint16_t> &usdata);

    /**
     * @brief Proceeds to initiate recording.
     * Sets the recording state to active, prepares CSV headers, and emits the signal to start data writing.
     */
    void proceedToStartRecording();

    QualisysTransformationManager m_latestTManager; //!< Stores the most recent Rigid Body data received from the Qualisys system.
    std::vector<uint16_t> m_latestUSData;           //!< Stores the most recent Ultrasound data received from the ultrasound sensor.
    QString m_filePath;                             //!< Path where the recorded CSV and image files will be stored.

    bool m_hasLatestTManager;       //!< Flag indicating whether the latest Rigid Body data has been received.
    bool m_hasLatestUSData;         //!< Flag indicating whether the latest Ultrasound data has been received.
    bool m_isRecording;             //!< Indicates whether recording is currently active.
    bool m_pendingRecordingRequest; //!< Indicates whether there is a pending request to start recording (occurs if the request is made before data is available).

    QThread *m_dataWriterThread;    //!< Thread where the DataWriter will run to handle file writing without blocking the main thread.
    DataWriter *m_dataWriter;       //!< Instance of DataWriter used to handle CSV file operations.

    QThread *m_imageWriterThread;   //!< Thread where the ImageWriter will run to handle image writing without blocking the main thread.
    ImageWriter *m_imageWriter;     //!< Instance of ImageWriter used to handle image file operations.

    QMutex m_dataMutex;             //!< Mutex used to protect shared data between threads to ensure thread safety.
    QStringList m_csvHeader;        //!< Stores the header information for the CSV file used during recording.
};

#endif // AMODEMOCAPRECORDER_H
