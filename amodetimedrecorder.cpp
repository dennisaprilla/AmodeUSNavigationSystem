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
    // Connect the timer to the processData slot
    connect(timer, &QTimer::timeout, this, &AmodeTimedRecorder::processData);
}

AmodeTimedRecorder::~AmodeTimedRecorder()
{
    stopRecording();
}

void AmodeTimedRecorder::setFilePostfix(const QString &filePostfix)
{
    m_filePostfix = "_" + filePostfix;
}


void AmodeTimedRecorder::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

void AmodeTimedRecorder::setRecordTimer(int ms)
{
    m_timerms = ms;
}

void AmodeTimedRecorder::startRecording()
{
    if (!isRecording)
    {
        isRecording = true;
        timer->start(m_timerms); // Default is triggered every 1 second
    }
    emit amodeTimedRecordingStarted();
}

void AmodeTimedRecorder::stopRecording()
{
    if (isRecording)
    {
        isRecording = false;
        timer->stop();
    }
    emit amodeTimedRecordingStopped();
}

void AmodeTimedRecorder::on_amodeSignalReceived(const std::vector<uint16_t> &data)
{
    // Always update currentData
    currentData = data;
}

void AmodeTimedRecorder::requested_stop_amodeTimedRecording()
{
    stopRecording();
}

void AmodeTimedRecorder::processData()
{
    if (isRecording && !currentData.empty())
    {
        // Check the size of the ultrasound data to determine reshaping parameters.
        // We expect the data to be reshaped into an image format, with a fixed height of 30.
        int height = 30;
        int width = currentData.size() / height;
        if (currentData.size() % height != 0)
        {
            qWarning() << "Data size is not divisible by the height. Cannot reshape.";
            return;  // Exit if the data cannot be reshaped correctly into the expected dimensions.
        }

        // Get the current timestamp in milliseconds since epoch for logging and file naming purposes.
        qint64 timestamp_currentEpochMillis = QDateTime::currentMSecsSinceEpoch();
        QString timestamp_currentEpochMillis_str = QString::number(timestamp_currentEpochMillis);

        // Call writeImage asynchronously
        QtConcurrent::run([=]() {

            // Create an OpenCV matrix (cv::Mat) from the ultrasound data.
            // This matrix represents the ultrasound image which will be saved for analysis.
            cv::Mat amodeImage(height, width, CV_16UC1, const_cast<uint16_t*>(currentData.data()));

            // Generate the filename for the ultrasound image using the current timestamp to make it unique.
            QString imageFilename = "AmodeRecording_" + timestamp_currentEpochMillis_str + m_filePostfix + ".tiff";
            QString imageFilepath = m_filePath.isEmpty() ? "D:/" : m_filePath;  // Use the specified file path or a default path if none is provided.
            QString filepath_filename = imageFilepath + imageFilename;          // Combine the path and filename to get the full path.

            // Use cv::imwrite to save the image
            if (!cv::imwrite(filepath_filename.toStdString(), amodeImage))
            {
                qWarning() << "Failed to write image to file:" << filepath_filename;
            }
        });
    }
}
