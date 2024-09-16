#ifndef BMODECONNECTION_H
#define BMODECONNECTION_H

#include <QObject>
#include <QTimer>
#include <opencv2/opencv.hpp>

/**
 * @class BmodeConnection
 * @brief A class which handle the communcation with the B-mode machine.
 *
 * For the context. The user will perform B-mode freehand scanning using this software. To achieve that, a B-mode
 * ultrasound machine needs to be connected with a frame grabber device (check Epiphan Frame Grabber), and plugged
 * to one of the port of your PC. To make the software more intuitive, it is better to visualize the real time B-mode
 * ultrasound image. The code is practically the same as how we grab a frame from our webcam. So, for testing the
 * software, we can always choose the port to our webcam port, and the software will run just fine.
 *
 */

class BmodeConnection : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Default constructor function, it also initialize the area where we need to crop the B-mode machine screen.
     */
    explicit BmodeConnection(QObject *parent = nullptr);

    /**
     * @brief Default destructor function, closes the port.
     */
    ~BmodeConnection();

    /**
     * @brief GET the camera information given the index of the camera port
     */
    std::string getCameraInfo(int index);

    /**
     * @brief GET all of the camera information, with n=MAX_CAMERAS
     */
    std::vector<std::string> getAllCameraInfo();

    /**
     * @brief Opening the camera port
     */
    bool openCamera(int cameraIndex = 0);

    /**
     * @brief Closing the camera port
     */
    void closeCamera();

    /**
     * @brief Once the user click the start stream button, this function should be called, to start stream the image.
     */
    void startImageStream();

    /**
     * @brief Once the user click the stop/pause button, this function should be called, to stop stream the image.
     */
    void stopImageStream();

signals:
    /**
     * @brief A signal which emits the processed image everytime we finish processing the image.
     */
    void imageProcessed(const cv::Mat &image);

private slots:
    /**
     * @brief A slot which will be called everytime there is a new image arriving.
     */
    void processFrame();

private:
    cv::VideoCapture camera;    //!< Stores the camera object
    cv::Mat processedImage;     //!< Stores the image data
    cv::Rect roi;               //!< A region of interest (cv::Rect object) which defines the cropping of the B-mode screen

    const int MAX_CAMERAS = 10; //!< The maximum number of camera that can be listed

    QTimer *frameTimer;         //!< A timer, in which we will check the arriving image.
};

#endif // BMODECONNECTION_H
