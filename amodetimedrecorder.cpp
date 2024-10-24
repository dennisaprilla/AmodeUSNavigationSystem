#include "AmodeTimedRecorder.h"
#include <QDateTime>

AmodeTimedRecorder::AmodeTimedRecorder(QObject *parent)
    : QObject(parent),
    m_filePath(""),
    m_filePostfix(""),
    m_timerms(1000),
    timer(new QTimer(this)),
    isRecording(false)
{
    // Connect the timer's timeout signal to the processData slot function.
    // This means that every time the timer triggers, processData will be executed.
    connect(timer, &QTimer::timeout, this, &AmodeTimedRecorder::processData);
}

AmodeTimedRecorder::~AmodeTimedRecorder()
{
    // Ensure that recording is stopped and resources are cleaned up properly when the object is destroyed.
    stopRecording();
}

void AmodeTimedRecorder::setFilePostfix(const QString &filePostfix)
{
    // Set a postfix for the filename to help identify recordings (e.g., adding a unique identifier).
    m_filePostfix = "_" + filePostfix;
}

void AmodeTimedRecorder::setFilePath(const QString &filePath)
{
    // Set the path where the recorded files should be saved.
    m_filePath = filePath;
}

void AmodeTimedRecorder::setRecordTimer(int ms)
{
    // Set the interval for recording, in milliseconds.
    // This value controls how often the timer will trigger the recording process.
    m_timerms = ms;
}

void AmodeTimedRecorder::startRecording()
{
    // Starts the recording process if it is not already recording.
    if (!isRecording)
    {
        isRecording = true;
        timer->start(m_timerms); // Start the timer, triggering processData every m_timerms milliseconds.
    }

    // Emit a signal to notify other parts of the application that recording has started.
    emit amodeTimedRecordingStarted();
}

void AmodeTimedRecorder::stopRecording()
{
    // Stops the recording process if it is currently active.
    if (isRecording)
    {
        isRecording = false;
        timer->stop(); // Stop the timer to halt the recording process.
    }

    // Emit a signal to notify other parts of the application that recording has stopped.
    emit amodeTimedRecordingStopped();
}

void AmodeTimedRecorder::on_amodeSignalReceived(const std::vector<uint16_t> &data)
{
    // This slot function is called whenever the AmodeConnection emits a signal containing data.
    // The received ultrasound data is stored in currentData to be processed later.
    currentData = data;
}

void AmodeTimedRecorder::requested_stop_amodeTimedRecording()
{
    // Slot function to stop recording when requested by an external signal.
    // Calls stopRecording() to stop the timer and recording process.
    stopRecording();
}

void AmodeTimedRecorder::processData()
{
    // This function is called each time the timer triggers, to handle the recording of the ultrasound data.
    if (isRecording && !currentData.empty())
    {
        // Set the dimensions for reshaping the ultrasound data into an image.
        // We assume the data represents an image with a height of 30, but we need to calculate the width.
        int height = 30;
        int width = currentData.size() / height;

        // Check if the data can be reshaped properly into the specified dimensions.
        if (currentData.size() % height != 0)
        {
            qWarning() << "Data size is not divisible by the height. Cannot reshape.";
            return;  // If the data size is not divisible by the height, reshaping is impossible, so we exit.
        }

        // Get the current timestamp in milliseconds since the Unix epoch to use as part of the filename.
        qint64 timestamp_currentEpochMillis = QDateTime::currentMSecsSinceEpoch();
        QString timestamp_currentEpochMillis_str = QString::number(timestamp_currentEpochMillis);

        // Call the writeImage function asynchronously to avoid blocking the main thread.
        QtConcurrent::run([=]() {

            // Create an OpenCV matrix (cv::Mat) from the ultrasound data.
            // The matrix will have a height of 30 and the calculated width, using 16-bit unsigned values.
            cv::Mat amodeImage(height, width, CV_16UC1, const_cast<uint16_t*>(currentData.data()));

            // Generate the filename for the image, incorporating the timestamp and file postfix to make it unique.
            QString imageFilename = "AmodeRecording_" + timestamp_currentEpochMillis_str + m_filePostfix + ".tiff";
            QString imageFilepath = m_filePath.isEmpty() ? "D:/" : m_filePath;  // Default path is "D:/" if no path is set.
            QString filepath_filename = imageFilepath + imageFilename;          // Combine the path and filename to get the full path.

            // Save the image using OpenCV's cv::imwrite function.
            // If the save operation fails, output a warning message.
            if (!cv::imwrite(filepath_filename.toStdString(), amodeImage))
            {
                qWarning() << "Failed to write image to file:" << filepath_filename;
            }
        });
    }
}