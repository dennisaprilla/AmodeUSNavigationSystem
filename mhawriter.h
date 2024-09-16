#ifndef MHAWRITER_H
#define MHAWRITER_H

#include <QObject>
// #include <QRunnable>
#include <QDateTime>
#include <QElapsedTimer>

#include <string>
#include <vector>
#include <fstream>

#include <Eigen/Geometry>
#include <opencv2/opencv.hpp>

#include "qualisysconnection.h"
#include "qualisystransformationmanager.h"


/**
 * @class MHAWriter
 * @brief For writing Sequence Image (.mha) files.
 *
 * For the context. The user should be able to record b-mode images and its transfromation in order to
 * perform volume reconstruction. This two data usually is encapsulated in a file called Sequence Image
 * with an extension (.mha). You can learn how Sequence Image file is defined here:
 * http://perk-software.cs.queensu.ca/plus/doc/nightly/user/FileSequenceFile.html
 *
 * This class has two slots, onImageReceived and onRigidBodyReceived, which needs to be connected to a
 * signal from BmodeConnection::imageProcessed and QualisysConnection::dataReceived. This class will do
 * the soft-synchronization by making sure there will be pair of data.
 *
 */

class MHAWriter : public QObject
{
    Q_OBJECT

public:

    /**
     * @struct MHAHeader
     * @brief Represents the attribute list for the header of Sequence Image file
     *
     * The header of Sequence Image file consists of relevant information of the Sequence Image
     * themselves. It is important for volume reconstruction.
     */
    struct MHAHeader {
        std::string      ObjectType;
        int              NDims;
        bool             BinaryData;
        bool             BinaryDataByteOrderMSB;
        bool             CompressedData;
        std::vector<std::string> Kinds;
        std::vector<int> TransformMatrix;
        std::vector<int> DimSize;
        std::vector<int> Offset;
        std::vector<int> CenterOfRotation;
        std::string      AnatomicalOrientation;
        std::vector<int> ElementSpacing;
        std::string      ElementType;
        std::string      UltrasoundImageOrientation;
        std::string      UltrasoundImageType;
        std::string      ElementDataFile;

    };

    /**
     * @brief Constructor function, requires the filepath where the Sequence File will be written and its prefix name
     */
    explicit MHAWriter(QObject *parent = nullptr, const std::string& filepath="D:\\", const std::string& prefixname="output");

    /**
     * @brief Start recording the data (image, transformations, and timestamps), stored in local
     */
    void startRecord();

    /**
     * @brief Stop recording the data (image, transformations, and timestamps) and start writing the file
     */
    int stopRecord();

    /**
     * @brief SET the transformation ID for bmode probe and bmode reference (marker on the phantom)
     */
    void setTransformationID(std::string bmodeprobe_transformationID, std::string bmoderef_transformationID);

    /**
     * @brief GET the full path of the Sequence Image file
     */
    std::string getFullfilename();

public slots:

    /**
     * @brief slot function, will be called when an image is received, needs to be connected to signal from BmodeConnection::imageProcessed
     */
    void onImageReceived(const cv::Mat &image);

    /**
     * @brief slot function, will be called when transformations in a timestamp are received, needs to be connected to signal from QualisysConnection::dataReceived class
     */
    void onRigidBodyReceived(const QualisysTransformationManager &tmanager);

private:

    /**
     * @brief writes header for sequence image (.mha) file
     */
    bool writeHeader();

    /**
     * @brief writes transformations, status, timestamps, for sequence image (.mha) file
     */
    bool writeTransformations();

    /**
     * @brief writes binary image (not compressed), for sequence image (.mha) file
     */
    bool writeImages();

    /**
     * @brief generates current time, for file naming purposes
     */
    std::string getCurrentDateTime();

    /**
     * @brief handles pair of data (soft-synchronization)
     */
    void storeDataPair(const cv::Mat& image, const Eigen::Isometry3d& transform_probe, const Eigen::Isometry3d& transform_ref);

    /**
     * @brief reset the pair of data (soft-synchronization)
     */
    void resetData();


    // variables for naming file
    std::string fullfilename_;          //!< Stores the full path and file name of the sequence image (.mha) file.
    MHAWriter::MHAHeader header_;       //!< Stores the necessary information related to the sequence image.
    std::ofstream mhaFile_;             //!< Stream object to write the sequence image.

    // variables for storing data
    bool isRecording;                                           //!< An indicator that whether we are recording or not.
    std::optional<cv::Mat> latestImage;                         //!< The latest image comes from streaming. Using std::optional so it is optional that this variable is empty or not.
    std::optional<Eigen::Isometry3d> latestTransform_probe;     //!< The latest probe transformation. Similar to latestImage.
    std::optional<Eigen::Isometry3d> latestTransform_ref;       //!< The latest of reference transformation. Similar to latestTransform_probe.

    std::vector<cv::Mat>           allImages;                   //!< Stores all the images had been streamed.
    std::vector<Eigen::Isometry3d> allTransforms_probe;         //!< Stores all probe transformation had been streamed.
    std::vector<Eigen::Isometry3d> allTransforms_ref;           //!< Stores all reference transformation had been stream. (Reference transformation is the marker from Calibration Phantom, somehow it is used by the volume reconstructor module from fCal)
    std::vector<double>            allTimestamps;               //!< Stores all the timestamps of the data streamed.
    QElapsedTimer timestamp;                                    //!< Stores the timestamp has been elapsed.

    // variables that is used to grab the necessary rigid bodies
    std::string bmodeprobe_transformationID_ = "B_PROBE";       //!< Default indentifier for B-mode probe rigid body transformation from the Mocap system
    std::string bmoderef_transformationID_   = "B_REF";         //!< Default indentifier for Reference rigid body transformation from the Mocap system

signals:
};

#endif // MHAWRITER_H
