#include "DataWriter.h"
#include <QDebug>
#include <QMutexLocker>
#include <QMetaObject>

DataWriter::DataWriter(QObject *parent)
    : QObject(parent),
    m_isWriting(false) // Initialize m_isWriting to false since writing has not started yet
{
    // Constructor
    // This initializes the DataWriter instance. Initially, the writing flag is set to false,
    // indicating that no data is being written at the moment.
}

DataWriter::~DataWriter()
{
    // Destructor
    // Ensure that the processing loop can exit properly by calling stopWriting.
    // This prevents potential issues related to incomplete file operations.
    stopWriting();
}

void DataWriter::setFileName(const QString &fileName)
{
    // Set the output file name where the data will be written
    m_file.setFileName(fileName);
}

void DataWriter::writeHeader(const QStringList &header)
{
    // Lock the mutex to ensure thread safety during file access
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen())
    {
        // Attempt to open the file in write-only mode and as a text file
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qWarning() << "Cannot open file for writing:" << m_file.errorString();
            return; // If the file cannot be opened, print a warning and return
        }
        // Set the QTextStream to write to the file
        m_stream.setDevice(&m_file);
    }
    m_stream << header.join(",") << "\n";   // Write the header to the file, separated by commas
    m_stream.flush();                       // Ensure the header is written immediately to the file
}

void DataWriter::enqueueData(const QStringList &data)
{
    QMutexLocker locker(&m_mutex);          // Lock the mutex to protect access to the data queue
    m_dataQueue.enqueue(data);              // Add the data to the queue
    m_condition.wakeOne();                  // Wake up any thread waiting for data to be added to the queue
}


void DataWriter::startWriting()
{
    qDebug() << "DataWriter::startWriting() called and attempting to start writing";

    QMutexLocker locker(&m_mutex);  // Lock the mutex to safely check and update m_isWriting
    if (m_isWriting) return;        // If already writing, do nothing
    m_isWriting = true;             // Set the flag to indicate that writing is active
    locker.unlock();                // Unlock the mutex before continuing

    qDebug() << "DataWriter::startWriting() m_isWriting set to true.";

    // Start the processing loop in the DataWriter's thread using a queued connection.
    // This ensures that the processQueue function will execute within the DataWriter's thread context.
    QMetaObject::invokeMethod(this, "processQueue", Qt::QueuedConnection);
}

void DataWriter::stopWriting()
{
    qDebug() << "DataWriter::stopWriting() called and attempting to stop writing.";

    QMutexLocker locker(&m_mutex);  // Lock the mutex to safely change m_isWriting
    m_isWriting = false;            // Set the flag to indicate that writing should stop
    m_condition.wakeOne();          // Wake up the thread if it's currently waiting for more data to be enqueued

    qDebug() << "DataWriter::stopWriting() m_isWriting set to false, condition variable signaled.";
}

void DataWriter::processQueue()
{
    qDebug() << "DataWriter::processQueue() started.";
    while (true)
    {
        // Lock the mutex to safely access m_isWriting and the data queue
        QMutexLocker locker(&m_mutex);
        if (!m_isWriting && m_dataQueue.isEmpty())
        {
            // If writing has stopped and the queue is empty, exit the loop
            qDebug() << "DataWriter::processQueue() exiting loop.";
            break;
        }

        if (m_dataQueue.isEmpty())
        {
            // If the queue is empty but writing is still active, wait for new data to be enqueued
            m_condition.wait(&m_mutex);
            continue;
        }

        QStringList dataRow = m_dataQueue.dequeue();    // Get the next data row from the queue
        locker.unlock();                                // Unlock the mutex before writing to the file

        // Write the data row to the file
        if (m_file.isOpen())
        {
            m_stream << dataRow.join(",") << "\n";  // Write the data as a comma-separated row
            m_stream.flush();                       // Ensure the data is written to the file immediately
        }
        else
        {
            // If the file is not open, print a warning
            qWarning() << "DataWriter::processQueue() File is not open for writing.";
        }
    }

    // After the loop, close the file if it is still open
    if (m_file.isOpen())
    {
        m_file.close();
    }

    qDebug() << "DataWriter::processQueue() emitting finished signal.";
    emit finished(); // Emit the finished signal to indicate that the writing process is complete
}
