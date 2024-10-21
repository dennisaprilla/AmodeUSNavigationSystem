#include "AmodeMocapRecorder.h"
#include <QDebug>
#include <QDateTime>

AmodeMocapRecorder::AmodeMocapRecorder(QObject *parent)
    : QObject(parent),
    m_filePath(""),
    m_hasLatestTManager(false),
    m_hasLatestUSData(false),
    m_isRecording(false),
    m_pendingRecordingRequest(false),
    m_dataWriterThread(nullptr),
    m_dataWriter(nullptr),
    m_imageWriterThread(nullptr),
    m_imageWriter(nullptr)
{
    // ==========================================================
    // Initialize the DataWriter and move it to a separate thread
    // ==========================================================

    // Create the DataWriter instance which will handle the CSV data writing.
    // The data writer is responsible for storing data into a CSV file during recording sessions.
    m_dataWriter = new DataWriter();
    m_dataWriterThread = new QThread();              // Create a new thread for DataWriter to avoid blocking the main UI thread.
    m_dataWriter->moveToThread(m_dataWriterThread);  // Move the DataWriter to the new thread to run its operations concurrently with the main program.

    // Connect signals and slots between AmodeMocapRecorder and DataWriter.
    // These connections ensure that the DataWriter starts or stops writing when we emit the corresponding signals.
    connect(this, &AmodeMocapRecorder::startWriter, m_dataWriter, &DataWriter::startWriting);                       // Start writing when the recording starts.
    connect(this, &AmodeMocapRecorder::stopWriter, m_dataWriter, &DataWriter::stopWriting, Qt::DirectConnection);   // Stop writing immediately when requested using a direct connection.
    connect(m_dataWriter, &DataWriter::finished, m_dataWriterThread, &QThread::quit);                               // Quit the DataWriter thread when the writing is finished.
    connect(m_dataWriterThread, &QThread::finished, m_dataWriter, &QObject::deleteLater);                           // Clean up the DataWriter object after the thread finishes to free memory.
    connect(m_dataWriterThread, &QThread::finished, m_dataWriterThread, &QObject::deleteLater);                     // Clean up the thread object itself after it is finished.

    // Start the DataWriter thread so that it is ready to handle writing tasks.
    m_dataWriterThread->start();
    qDebug() << "AmodeMocapRecorder::AmodeMocapRecorder() m_dataWriterThread started.";

    // ===========================================================
    // Initialize the ImageWriter and move it to a separate thread
    // ===========================================================

    // Create the ImageWriter instance which will handle the image writing.
    // The ImageWriter is responsible for saving ultrasound images captured during the recording sessions.
    m_imageWriter = new ImageWriter();
    m_imageWriterThread = new QThread();                // Create a new thread for ImageWriter to avoid blocking the main UI thread.
    m_imageWriter->moveToThread(m_imageWriterThread);   // Move the ImageWriter to the new thread to ensure its operations do not interfere with the main program.

    // Connect signals and slots for ImageWriter.
    // These connections ensure that the ImageWriter starts or stops writing when we emit the corresponding signals.
    connect(this, &AmodeMocapRecorder::startWriter, m_imageWriter, &ImageWriter::startWriting);                      // Start writing when recording starts.
    connect(this, &AmodeMocapRecorder::stopWriter, m_imageWriter, &ImageWriter::stopWriting, Qt::DirectConnection);  // Stop writing immediately when requested using a direct connection.
    connect(m_imageWriter, &ImageWriter::finished, m_imageWriterThread, &QThread::quit);                             // Quit the ImageWriter thread when writing is finished.
    connect(m_imageWriterThread, &QThread::finished, m_imageWriter, &QObject::deleteLater);                          // Clean up ImageWriter after the thread finishes to free memory.
    connect(m_imageWriterThread, &QThread::finished, m_imageWriterThread, &QObject::deleteLater);                    // Clean up the thread object itself after it is finished.

    // Start the ImageWriter thread so that it is ready to handle writing tasks.
    m_imageWriterThread->start();
    qDebug() << "AmodeMocapRecorder::AmodeMocapRecorder() m_imageWriterThread started.";
}

AmodeMocapRecorder::~AmodeMocapRecorder()
{
    qDebug() << "AmodeMocapRecorder::~AmodeMocapRecorder() called.";

    // Ensure the writer threads are properly closed before the object is destroyed to avoid data corruption or memory leaks.
    if (m_isRecording || m_pendingRecordingRequest)
    {
        stopRecording();  // Stop recording if it is still ongoing, ensuring no data is lost.
    }

    // Wait for the DataWriter thread to finish before destructing to ensure all pending tasks are completed.
    if (m_dataWriterThread && m_dataWriterThread->isRunning())
    {
        qDebug() << "AmodeMocapRecorder::~AmodeMocapRecorder() Waiting for m_dataWriterThread to finish.";
        m_dataWriterThread->wait();  // Wait for the thread to finish its operations safely.
        qDebug() << "AmodeMocapRecorder::~AmodeMocapRecorder() m_dataWriterThread has finished.";
    }

    // Wait for the ImageWriter thread to finish before destructing to ensure all pending tasks are completed.
    if (m_imageWriterThread && m_imageWriterThread->isRunning())
    {
        qDebug() << "AmodeMocapRecorder::~AmodeMocapRecorder() Waiting for m_imageWriterThread to finish.";
        m_imageWriterThread->wait();  // Wait for the thread to finish its operations safely.
        qDebug() << "AmodeMocapRecorder::~AmodeMocapRecorder() m_imageWriterThread has finished.";
    }
}

void AmodeMocapRecorder::setFilePath(const QString &filePath)
{
    // Store the provided file path to be used later when writing files.
    // This path is where the CSV files and image files will be saved during recording.
    m_filePath = filePath;
}

void AmodeMocapRecorder::onRigidBodyReceived(const QualisysTransformationManager &tmanager)
{
    QMutexLocker locker(&m_dataMutex);  // Lock the mutex to ensure that shared data is accessed in a thread-safe manner.
    m_latestTManager = tmanager;        // Store the latest Rigid Body data received from the Qualisys system.
    m_hasLatestTManager = true;         // Set the flag to indicate that the latest rigid body data is available.

    // Check if there is a pending request to start recording and verify that recording is not already active.
    // If both conditions are true and both rigid body and ultrasound data are available, start recording.
    if (m_pendingRecordingRequest && !m_isRecording)
    {
        bool hasRigidBodyData = !m_latestTManager.getAllIds().empty();  // Check if rigid body data is available.
        bool hasUSData = !m_latestUSData.empty();                       // Check if ultrasound data is available.

        if (hasRigidBodyData && hasUSData)
        {
            proceedToStartRecording();          // Proceed to start recording now that both datasets are available.
            m_pendingRecordingRequest = false;  // Reset the pending request flag since recording has started.
        }
    }

    // Attempt to process the latest data pair of rigid body and ultrasound data.
    tryProcessDataPair();
}

void AmodeMocapRecorder::onAmodeSignalReceived(const std::vector<uint16_t> &usdata_uint16_)
{
    QMutexLocker locker(&m_dataMutex);      // Lock the mutex to ensure that shared data is accessed in a thread-safe manner.
    m_latestUSData = usdata_uint16_;        // Store the latest Ultrasound data received from the sensor.
    m_hasLatestUSData = true;               // Set the flag to indicate that the latest ultrasound data is available.

    // Check if there is a pending request to start recording and verify that recording is not already active.
    // If both conditions are true and both rigid body and ultrasound data are available, start recording.
    if (m_pendingRecordingRequest && !m_isRecording)
    {
        bool hasRigidBodyData = !m_latestTManager.getAllIds().empty();  // Check if rigid body data is available.
        bool hasUSData = !m_latestUSData.empty();                       // Check if ultrasound data is available.

        if (hasRigidBodyData && hasUSData)
        {
            proceedToStartRecording();          // Proceed to start recording now that both datasets are available.
            m_pendingRecordingRequest = false;  // Reset the pending request flag since recording has started.
        }
    }

    // Attempt to process the latest data pair of rigid body and ultrasound data.
    tryProcessDataPair();
}

void AmodeMocapRecorder::tryProcessDataPair()
{
    // Check if both the latest rigid body data and ultrasound data are available.
    // If both are available, process the data pair to extract relevant information.
    if (m_hasLatestTManager && m_hasLatestUSData)
    {
        processDataPair(m_latestTManager, m_latestUSData);  // Process the available data.

        // Reset the flags as we have used the current data, indicating that we need new data for further processing.
        m_hasLatestTManager = false;
        m_hasLatestUSData = false;
    }
    // If either data is not available, wait until both are received before processing.
}

void AmodeMocapRecorder::processDataPair(const QualisysTransformationManager &tmanager, const std::vector<uint16_t> &usdata)
{
    // Only process the data if recording is currently active and exit the function without processing if not recording.
    if (!m_isRecording)
        return;

    // Check the size of the ultrasound data to determine reshaping parameters.
    // We expect the data to be reshaped into an image format, with a fixed height of 30.
    int height = 30;
    int width = usdata.size() / height;
    if (usdata.size() % height != 0)
    {
        qWarning() << "Data size is not divisible by the height. Cannot reshape.";
        return;  // Exit if the data cannot be reshaped correctly into the expected dimensions.
    }

    // Get the current timestamp in milliseconds since epoch for logging and file naming purposes.
    qint64 timestamp_currentEpochMillis = QDateTime::currentMSecsSinceEpoch();
    QString timestamp_currentEpochMillis_str = QString::number(timestamp_currentEpochMillis);

    // Extract transformations from the tmanager to get the position and orientation data of the rigid body.
    // This information is essential for analyzing the movement and orientation of the tracked object.
    std::vector<Eigen::Isometry3d> transformations_values = tmanager.getAllTransformations();
    std::vector<std::string> transformations_id = tmanager.getAllIds();

    // ==========================================================
    // Handle rigid body data writing to CSV file
    // ==========================================================

    // Prepare a row of data to write into the CSV file.
    // The row contains a timestamp followed by the position and orientation (quaternion) of each tracked rigid body.
    QStringList dataRow;
    dataRow << timestamp_currentEpochMillis_str;  // Add the timestamp to indicate when the data was recorded.

    // Iterate over each transformation and add the relevant data to the CSV row.
    for (size_t i = 0; i < transformations_values.size(); ++i)
    {
        const Eigen::Isometry3d &transform = transformations_values[i];  // Extract the transformation data.
        const std::string &id = transformations_id[i];                   // Extract the unique ID of the transformation.

        // Extract translation (position) components (x, y, z).
        Eigen::Vector3d translation = transform.translation();

        // Extract rotation matrix and convert it to a quaternion representation (x, y, z, w).
        Eigen::Quaterniond quaternion(transform.rotation());

        // Append quaternion components q1, q2, q3, q4 (which represent the rotation of the rigid body).
        dataRow << QString::number(quaternion.x());
        dataRow << QString::number(quaternion.y());
        dataRow << QString::number(quaternion.z());
        dataRow << QString::number(quaternion.w());

        // Append translation components t1, t2, t3 (which represent the position of the rigid body).
        dataRow << QString::number(translation.x());
        dataRow << QString::number(translation.y());
        dataRow << QString::number(translation.z());
    }
    // Enqueue data to be written into the CSV file via DataWriter.
    // The DataWriter runs in a separate thread to ensure smooth data handling without blocking the main thread.
    m_dataWriter->enqueueData(dataRow);

    // ==========================================================
    // Handle ultrasound image writing
    // ==========================================================

    // Create an OpenCV matrix (cv::Mat) from the ultrasound data.
    // This matrix represents the ultrasound image which will be saved for analysis.
    cv::Mat amodeImage(height, width, CV_16UC1, const_cast<uint16_t*>(usdata.data()));

    // Generate the filename for the ultrasound image using the current timestamp to make it unique.
    QString imageFilename = "AmodeRecording_" + timestamp_currentEpochMillis_str + ".tiff";
    QString imageFilepath = m_filePath.isEmpty() ? "D:/" : m_filePath;  // Use the specified file path or a default path if none is provided.
    QString filepath_filename = imageFilepath + imageFilename;          // Combine the path and filename to get the full path.

    // Enqueue the image to be written in the image writer thread.
    // The ImageWriter runs in a separate thread to ensure that saving the image does not block the main application.
    m_imageWriter->enqueueImage(amodeImage, filepath_filename);
}

void AmodeMocapRecorder::startRecording()
{
    // Check if recording is already active or if there is a pending request to start recording.
    // If either of these conditions is true, do nothing.
    if (m_isRecording || m_pendingRecordingRequest)
        return;

    // Check if both Rigid Body data and Ultrasound data are available before starting the recording.
    bool hasRigidBodyData = !m_latestTManager.getAllIds().empty();  // Check if there is valid rigid body data.
    bool hasUSData = !m_latestUSData.empty();                       // Check if there is valid ultrasound data.

    // If either data is unavailable, we cannot start recording immediately.
    if (!hasRigidBodyData || !hasUSData)
    {
        qWarning() << "Necessary data not available to start recording. Recording will start when data becomes available.";
        m_pendingRecordingRequest = true;  // Set a flag to start recording once the data becomes available.
        return;  // Exit and wait for the data to be available.
    }

    // If both data types are available, proceed to start recording immediately.
    proceedToStartRecording();
}

void AmodeMocapRecorder::proceedToStartRecording()
{
    // Set the flag indicating that recording is now active.
    m_isRecording = true;

    // Set up the CSV file to store recorded data.
    // The filename includes a timestamp to make it unique and easy to identify later.
    QString filename = "MocapRecording_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss_zzz") + ".csv";
    QString filepath = m_filePath.isEmpty() ? "D:/" : m_filePath;   // Use the specified file path or a default path if none is provided.
    QString filepath_filename = filepath + filename;                // Combine the path and filename to get the full path.
    m_dataWriter->setFileName(filepath_filename);                   // Set the filename for the DataWriter to use.

    // Prepare the header row for the CSV file to describe the data structure.
    // The header includes a timestamp followed by identifiers for each transformation's position and orientation.
    m_csvHeader.clear();
    m_csvHeader << "timestamp";  // The first column in the CSV is the timestamp.

    // Get the IDs of all the transformations and add them to the CSV header.
    // Each transformation includes quaternion components (q1, q2, q3, q4) and translation components (t1, t2, t3).
    std::vector<std::string> transformations_id = m_latestTManager.getAllIds();
    for (const std::string &id : transformations_id)
    {
        QString q1 = QString::fromStdString(id) + "_q1";
        QString q2 = QString::fromStdString(id) + "_q2";
        QString q3 = QString::fromStdString(id) + "_q3";
        QString q4 = QString::fromStdString(id) + "_q4";
        QString t1 = QString::fromStdString(id) + "_t1";
        QString t2 = QString::fromStdString(id) + "_t2";
        QString t3 = QString::fromStdString(id) + "_t3";

        m_csvHeader << q1 << q2 << q3 << q4 << t1 << t2 << t3;  // Add each component to the header.
    }

    // Write the header to the CSV file using the DataWriter.
    m_dataWriter->writeHeader(m_csvHeader);

    // Emit the signal to start writing data to the file.
    // This will trigger the DataWriter to start its writing operation.
    emit startWriter();

    qDebug() << "AmodeMocapRecorder::proceedToStartRecording() recording started.";
}

void AmodeMocapRecorder::stopRecording()
{
    qDebug() << "AmodeMocapRecorder::stopRecording() attempting to stop the recording";

    // Check if recording is neither active nor pending; if so, do nothing.
    if (!m_isRecording && !m_pendingRecordingRequest)
        return;

    // Set the flags to indicate that recording is stopping.
    m_isRecording = false;
    m_pendingRecordingRequest = false;

    // Emit the signal to stop writing.
    // This will trigger both DataWriter and ImageWriter to stop their respective writing tasks.
    emit stopWriter();
    qDebug() << "AmodeMocapRecorder::stopRecording() emits stopWriter()";
}
