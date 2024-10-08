#include "BmodeConnection.h"

BmodeConnection::BmodeConnection(QObject *parent) : QObject(parent) {
    // set the roi
    // roi = cv::Rect(662, 0, 840, 900);
    roi = cv::Rect(0, 0, 320, 480);

    // declare a timer
    // frameTimer = new QTimer(this);
    // connect the timer to processframe() function. If the camera is not open yet, this function basically does nothing.
    // connect(frameTimer, &QTimer::timeout, this, &BmodeConnection::processFrame);

    // Create the worker thread
    moveToThread(&m_workerThread);

    // Connect the thread's start to the frame processing function
    connect(&m_workerThread, &QThread::started, this, &BmodeConnection::processFrame);

}

std::string BmodeConnection::getCameraInfo(int index) {
    cv::VideoCapture camera(index);

    if (!camera.isOpened()) {
        return "Not available";
    }

    int width = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_HEIGHT));
    // double fps = camera.get(cv::CAP_PROP_FPS);

    std::stringstream ss;
    ss << width << "x" << height;

    // std::stringstream ss;
    // ss << width << "x" << height << "|" << std::fixed << std::setprecision(1) << fps << "fps";

    // cv::Mat frame;
    // if (camera.read(frame)) {
    //     ss << "|" << frame.channels() << "ch|" << (8 * frame.elemSize1()) << "bit";
    // }

    camera.release();
    return ss.str();
}

std::vector<std::string> BmodeConnection::getAllCameraInfo()
{
    std::vector<std::string> allCameraInfo;
    const int MAX_CAMERAS = 10; // Adjust this value based on your system

    for (int i = 0; i < MAX_CAMERAS; ++i) {
        std::string cameraInfo = getCameraInfo(i);
        if (cameraInfo != "Not available") {
            std::stringstream ss;
            ss << "Camera " << i << ": " << cameraInfo;
            allCameraInfo.push_back(ss.str());
        }
    }

    return allCameraInfo;
}

BmodeConnection::~BmodeConnection() {
    stopImageStream();
    m_workerThread.quit();
    m_workerThread.wait();

    closeCamera();
}

bool BmodeConnection::openCamera(int cameraIndex) {
    if(camera.open(cameraIndex, cv::CAP_DSHOW)) {
        return true;
    }
    return false;
}

void BmodeConnection::closeCamera() {
    if(camera.isOpened()) {
        camera.release();
    }
}

// void BmodeConnection::startImageStream() {
//     if(camera.isOpened()) {
//         frameTimer->start(30); // Set the interval in milliseconds (e.g., 30ms for ~33 fps)
//     }
// }

// void BmodeConnection::stopImageStream() {
//     if(frameTimer->isActive()) {
//         frameTimer->stop();
//     }
// }

// void BmodeConnection::processFrame() {
//     cv::Mat frame;
//     if(camera.read(frame)) {
//         // Perform cropping and simple image processing here
//         frame = frame(roi);

//         // For example, convert to grayscale:
//         cv::cvtColor(frame, processedImage, cv::COLOR_BGR2GRAY);

//         // Emit the signal with the processed image
//         emit imageProcessed(processedImage);
//     }
// }

void BmodeConnection::startImageStream() {
    if(camera.isOpened()) {
        QMutexLocker locker(&m_mutex);
        if (!m_isRunning) {
            m_isRunning = true;
            m_workerThread.start();
        }
    }
}

void BmodeConnection::stopImageStream() {
    if (m_workerThread.isRunning()) {
        QMutexLocker locker(&m_mutex);
        m_isRunning = false;
        m_condition.wakeOne();
    }
}

void BmodeConnection::processFrame() {
    while (true)
    {
        // create new scope for QMutexLocker Object
        {
            QMutexLocker locker(&m_mutex);
            while (!m_isRunning) {
                m_condition.wait(&m_mutex);
            }
            if (!m_isRunning) {
                break;
            }
        }

        cv::Mat frame;
        if(camera.read(frame)) {
            // Perform cropping and simple image processing here
            frame = frame(roi);

            // For example, convert to grayscale:
            cv::cvtColor(frame, processedImage, cv::COLOR_BGR2GRAY);

            // Emit the signal with the processed image
            emit imageProcessed(processedImage);
        }

        QThread::msleep(20);  // Sleep to control frame rate (~33 fps)
    }
}
