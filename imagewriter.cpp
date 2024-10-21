#include "ImageWriter.h"
#include <QDebug>
#include <QMutexLocker>
#include <QMetaObject>

ImageWriter::ImageWriter(QObject *parent)
    : QObject(parent),
    m_isWriting(false) // Initialize m_isWriting to false since no writing has started yet
{
    // Constructor
    // Initializes the ImageWriter instance, setting m_isWriting to false to indicate that no images are currently being written.
}


ImageWriter::~ImageWriter()
{
    // Destructor
    // Ensure that the processing loop can exit properly by calling stopWriting.
    // This makes sure that all resources are released correctly, and no unfinished writing processes are left.
    stopWriting();
}


void ImageWriter::enqueueImage(const cv::Mat &image, const QString &filename)
{
    QMutexLocker locker(&m_mutex);                              // Lock the mutex to protect access to the image queue
    m_imageQueue.enqueue(qMakePair(image.clone(), filename));   // Add the image and its filename to the queue; clone the image to avoid potential data issues
    m_condition.wakeOne();                                      // Wake up any thread waiting for images to be added to the queue
}


void ImageWriter::startWriting()
{
    qDebug() << "ImageWriter::startWriting() called and attempting to start writing";

    QMutexLocker locker(&m_mutex);  // Lock the mutex to safely check and update m_isWriting
    if (m_isWriting) return;        // If already writing, do nothing
    m_isWriting = true;             // Set the flag to indicate that writing is active
    locker.unlock();                // Unlock the mutex before continuing

    qDebug() << "ImageWriter::startWriting() m_isWriting set to true.";

    // Start the processing loop in the ImageWriter's thread using a queued connection.
    // This ensures that the processQueue function will execute within the ImageWriter's thread context.
    QMetaObject::invokeMethod(this, "processQueue", Qt::QueuedConnection);
}


void ImageWriter::stopWriting()
{
    qDebug() << "ImageWriter::stopWriting() called and attempting to stop writing.";

    QMutexLocker locker(&m_mutex);  // Lock the mutex to safely change m_isWriting
    m_isWriting = false;            // Set the flag to indicate that writing should stop
    m_condition.wakeOne();          // Wake up the thread if it's currently waiting for more images to be enqueued

    qDebug() << "ImageWriter::stopWriting() m_isWriting set to false, condition variable signaled.";
}


void ImageWriter::processQueue()
{
    qDebug() << "ImageWriter::processQueue() started.";
    while (true)
    {
        // Lock the mutex to safely access m_isWriting and the image queue
        QMutexLocker locker(&m_mutex);
        if (!m_isWriting && m_imageQueue.isEmpty())
        {
            // If writing has stopped and the queue is empty, exit the loop
            qDebug() << "ImageWriter::processQueue() exiting loop.";
            break;
        }

        if (m_imageQueue.isEmpty())
        {
            // If the queue is empty but writing is still active, wait for new images to be enqueued
            m_condition.wait(&m_mutex);
            continue;
        }

        QPair<cv::Mat, QString> imagePair = m_imageQueue.dequeue(); // Get the next image from the queue
        locker.unlock();                                            // Unlock the mutex before writing to the file

        // Write the image to the specified file
        if (!cv::imwrite(imagePair.second.toStdString(), imagePair.first))
        {
            // If the image cannot be written, print a warning message
            qWarning() << "ImageWriter::processQueue() Failed to write image:" << imagePair.second;
        }
    }

    qDebug() << "ImageWriter::processQueue() emitting finished signal.";
    emit finished(); // Emit the finished signal to indicate that the writing process is complete
}
