#ifndef DATAWRITER_H
#define DATAWRITER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QStringList>

/**
 * @brief DataWriter class implementation.
 *
 * This class is responsible for managing the writing of data to a file, such as a CSV file.
 * It operates in a separate thread to ensure that file writing operations do not block the main application thread.
 * The class handles starting and stopping the writing process, manages a queue of data to be written,
 * and ensures that data is written safely using mutexes to prevent race conditions.
 */

class DataWriter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief DataWriter. Constructor of the class
     * @param parent
     */
    explicit DataWriter(QObject *parent = nullptr);

    /**
     * @brief Destructor of the class
     */
    ~DataWriter();

    /**
     * @brief Sets the file name where data will be saved.
     *
     * This method sets the file path and name for the output file where data will be written.
     * @param fileName A QString representing the path and name of the file.
     */
    void setFileName(const QString &fileName);

    /**
     * @brief Writes the CSV header to the file.
     *
     * This function opens the file for writing if it is not already open, and then writes the provided header.
     * It uses a mutex lock to ensure that file operations are safe from race conditions.
     * @param header A QStringList containing the column headers for the CSV file.
     */
    void writeHeader(const QStringList &header);

    /**
     * @brief Adds a new data row to the queue for writing.
     *
     * This function enqueues the provided data for writing to the file later.
     * It uses a mutex lock to ensure that access to the data queue is safe from race conditions.
     * @param data A QStringList containing the data to be written as a new row in the CSV file.
     */
    void enqueueData(const QStringList &data);

public slots:
    /**
     * @brief Starts the writing process.
     *
     * This function initiates the writing process if it is not already active. It sets the writing flag to true
     * and invokes the processing loop via Qt's queued connection, ensuring that the function runs in the correct thread.
     */
    void startWriting();

    /**
     * @brief Stops the writing process.
     *
     * This function stops the writing process by setting the writing flag to false.
     * It also wakes up the thread if it is waiting, allowing the processQueue loop to exit.
     */
    void stopWriting();

signals:
    /**
     * @brief finished. Signal emitted when finsihed writing data.
     */
    void finished();

private slots:
    /**
     * @brief Processes the data queue, writing data to the file.
     *
     * This function runs in a loop while the writing flag is true. It dequeues data from the queue and writes it to the file.
     * If no data is available, it waits until new data is enqueued. This function ensures that all data is written before exiting.
     */
    void processQueue();

private:
    QFile m_file;                       //!< The CSV file to write to.
    QTextStream m_stream;               //!< Stream to write text data to the file.
    QMutex m_mutex;                     //!< Ensures thread safety when accessing the file.
    QWaitCondition m_condition;         //!< Manage access to the queue.
    QQueue<QStringList> m_dataQueue;    //!< Store data rows in a Queue.
    bool m_isWriting;                   //!< Indicates if writing is in progress.
};

#endif // DATAWRITER_H
