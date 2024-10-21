#ifndef IMAGEWRITER_H
#define IMAGEWRITER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <opencv2/opencv.hpp>

/**
 * @brief ImageWriter class implementation.
 *
 * The ImageWriter class is responsible for managing the saving of images to disk.
 * This class works in a separate thread to ensure that image saving operations do not block the main application.
 * The class manages a queue of images, starts and stops the writing process, and ensures that all queued images
 * are saved properly. Thread safety is achieved using mutex locks, and a condition variable is used to coordinate
 * waiting and waking up the writing thread.
 */
class ImageWriter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief ImageWriter
     * @param parent
     */
    explicit ImageWriter(QObject *parent = nullptr);

    /**
     *
     */
    ~ImageWriter();

    /**
     * @brief Adds a new image to the queue for writing.
     *
     * This function enqueues an image along with its intended filename to be saved later.
     * It uses a mutex lock to ensure that access to the image queue is thread-safe.
     * @param image A cv::Mat object representing the image to be saved.
     * @param filename A QString representing the file name (including the path) where the image will be saved.
     */
    void enqueueImage(const cv::Mat &image, const QString &filename);

public slots:
    /**
     * @brief Starts the image writing process.
     *
     * This function initiates the writing process if it is not already active. It sets the writing flag to true
     * and starts the processing loop via a queued connection to ensure that it runs in the correct thread context.
     */
    void startWriting();

    /**
     * @brief Stops the image writing process.
     *
     * This function stops the writing process by setting the writing flag to false.
     * It also wakes up the processing thread if it is waiting, allowing the processQueue loop to exit gracefully.
     */
    void stopWriting();

signals:
    /**
     * @brief A signal which emitted when everything is finished
     */
    void finished();

private slots:
    /**
     * @brief Processes the image queue and saves images to disk.
     *
     * This function runs in a loop while the writing flag is true. It dequeues images from the queue and writes them to disk.
     * If no images are available, it waits until new images are enqueued. This function ensures that all images are saved before exiting.
     */
    void processQueue();

private:
    QMutex m_mutex;                                 //!<
    QWaitCondition m_condition;                     //!<
    QQueue<QPair<cv::Mat, QString>> m_imageQueue;   //!<
    bool m_isWriting;                               //!<
};

#endif // IMAGEWRITER_H
