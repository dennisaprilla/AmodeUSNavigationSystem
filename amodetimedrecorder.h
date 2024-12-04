#ifndef AMODETIMEDRECORDER_H
#define AMODETIMEDRECORDER_H

#include <QObject>
#include <QTimer>
#include <QtConcurrent>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * @brief The AmodeTimedRecorder class handles timed recording of ultrasound A-mode data.
 * This class connects to an ultrasound data stream, processes the incoming data, and saves it at specified intervals.
 *
 * The context is, this class will be used as an "Intermediate Recording" between Navigation Phase and Measurement Phase.
 * This was done to ensure that the initialization of the A-mode signal window peak is relevant to the measurement phase.
 * However, the reason why this class record in a specific intervals is to reduce the amount of process that is happening
 * in the navigation phase. Navigation needs to be real time, no matter what, so we made the "Intermediate Recording" to
 * record not in the full potential.
 *
 */
class AmodeTimedRecorder : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an AmodeTimedRecorder object.
     * Initializes the recording timer and connects it to the data processing function.
     * @param parent The parent QObject.
     */
    explicit AmodeTimedRecorder(QObject *parent = nullptr);

    /**
     * @brief Destructor for AmodeTimedRecorder.
     * Ensures that recording is stopped and resources are cleaned up properly.
     */
    ~AmodeTimedRecorder();

    /**
     * @brief Sets the postfix for recorded file names.
     * This postfix will be added to the generated file name to help identify different recordings.
     * @param filePostfix The postfix string to be added to the file name.
     */
    void setFilePostfix(const QString &filePostfix);

    // /**
    //  * @brief Sets the path where the recorded files will be saved.
    //  * If no path is set, the default path "D:/" will be used.
    //  * @param filePath The file path for saving recorded files.
    //  */
    // void setFilePath(const QString &filePath);

    /**
     * @brief Sets the parent path where the timed-recording will be grouped into numbered folders
     * @param filePath The file parent path for saving recorded files.
     */
    void setFileParentPath(const QString &fileParentPath);

    /**
     * @brief Sets the interval for recording in milliseconds.
     * Defines how frequently the recording process will be triggered by the timer.
     * @param ms The interval in milliseconds.
     */
    void setRecordTimer(int ms);

    /**
     * @brief Starts the recording process.
     * Begins recording by starting the timer and emits a signal to notify other classes.
     */
    void startRecording();

    /**
     * @brief Stops the recording process.
     * Stops the timer, halts recording, and emits a signal to notify other classes.
     */
    void stopRecording();

    /**
     * @brief Returns true if the class is currently recording
     */
    bool isCurrentlyRecording();

public slots:
    /**
     * @brief Slot to receive ultrasound data from an external source.
     * This function captures the data emitted by the AmodeConnection and stores it for later processing.
     * @param data A vector of uint16_t representing the ultrasound data.
     */
    void on_amodeSignalReceived(const std::vector<uint16_t> &data);

    /**
     * @brief Slot to handle an external request to stop recording.
     * Calls stopRecording() to stop the timed recording process.
     */
    void requested_stop_amodeTimedRecording();

private slots:
    /**
     * @brief Processes the received ultrasound data and saves it to a file.
     * This function is called each time the timer triggers and runs asynchronously to avoid blocking the main thread.
     */
    void processData();

private:
    // QString m_filePath;                 //!< The path where the timed-recorded files will be saved.
    QString m_fileCurrentPath;          //!< The path where the current numbered timed-recorded files will be saved.
    QString m_fileParentPath;           //!< The parent path where the timed-recorded files will be grouped into numbered folders.
    QString m_filePostfix;              //!< The postfix added to recorded file names to distinguish them.
    int m_timerms;                      //!< The interval in milliseconds for recording data.

    QTimer *timer;                      //!< Timer to trigger the data processing at specified intervals.
    std::vector<uint16_t> currentData;  //!< Stores the latest received ultrasound data.
    bool isRecording;                   //!< Indicates whether the recording process is currently active.

    /**
     * @brief Creates a new numbered folder in the given path starting from zero if no folders exist.
     *
     * @param basePath The base directory path where the numbered folder will be created.
     * @return QString The full path of the created folder (e.g., "D:/path/to/base/0000"), or an empty string if an error occurred.
     */
    QString createNumberedFolder(const QString& basePath);


signals:
    /**
     * @brief Signal emitted when timed recording starts.
     * Notifies other classes that the recording process has begun.
     */
    void amodeTimedRecordingStarted();

    /**
     * @brief Signal emitted when timed recording stops.
     * Notifies other classes that the recording process has ended.
     */
    void amodeTimedRecordingStopped();
};

#endif // AMODETIMEDRECORDER_H
