#include "mhawriter.h"
#include <opencv2/imgcodecs.hpp>
#include <QThread>
#include <QMessageBox>


MHAWriter::MHAWriter(QObject *parent,  const std::string& filepath, const std::string& prefixname)
    : QObject{parent}, isRecording(false)
{
    // open a file to write the .mha
    fullfilename_ = filepath + prefixname + "_" + getCurrentDateTime() + ".mha";
    mhaFile_.open(fullfilename_, std::ios::binary);
    if (!mhaFile_.is_open()) {
        throw std::runtime_error("Unable to open file: " + fullfilename_);
    }

    // start the timestamp
    timestamp.start();
}

void MHAWriter::setTransformationID(std::string bmodeprobe_transformationID, std::string bmoderef_transformationID)
{
    bmodeprobe_transformationID_ = bmodeprobe_transformationID;
    bmoderef_transformationID_ = bmoderef_transformationID;
}

void MHAWriter::onImageReceived(const cv::Mat &image) {
    // if there is already data from mocap let's store
    // here i only check one of the data from mocap, they are coupled anyway, so..
    if (latestTransform_probe) {
        storeDataPair(image, *latestTransform_probe, *latestTransform_ref);
        resetData();
    } else {
        latestImage = image;
    }
}

void MHAWriter::onRigidBodyReceived(const QualisysTransformationManager &tmanager) {
    if (latestImage) {
        storeDataPair(*latestImage, tmanager.getTransformationById(bmodeprobe_transformationID_), tmanager.getTransformationById(bmoderef_transformationID_));
        resetData();
    } else {
        latestTransform_probe = tmanager.getTransformationById(bmodeprobe_transformationID_);
        latestTransform_ref = tmanager.getTransformationById(bmoderef_transformationID_);
    }
}

void MHAWriter::storeDataPair(const cv::Mat& image, const Eigen::Isometry3d& transform_probe, const Eigen::Isometry3d& transform_ref) {
    // make a complete copy of cv::Mat using copyTo() function, it is super necessary so that i am not referencing the streaming image
    cv::Mat imageCopy;
    image.copyTo(imageCopy);
    // make a copy of Eigen::Isometry3d, not like cv::Mat, assigning new object like this will not affect the original object
    Eigen::Isometry3d transformCopy_probe = transform_probe;
    Eigen::Isometry3d transformCopy_ref   = transform_ref;

    allImages.push_back(imageCopy);
    allTransforms_probe.push_back(transformCopy_probe);
    allTransforms_ref.push_back(transformCopy_ref);
    allTimestamps.push_back(timestamp.elapsed()/1000.0);
}

void MHAWriter::resetData() {
    latestImage.reset();
    latestTransform_probe.reset();
    latestTransform_ref.reset();
}

void MHAWriter::startRecord()
{
    isRecording = true;
}

int MHAWriter::stopRecord()
{
    isRecording = false;
    resetData();

    if (!mhaFile_.is_open()) {
        std::cerr << "Error opening MHA file for writing." << std::endl;
        return false;
    }

    // Header initalization
    // read this    : http://perk-software.cs.queensu.ca/plus/doc/nightly/user/FileSequenceFile.html
    // example file : https://github.com/PlusToolkit/PlusLibData/blob/master/TestImages/SpinePhantomFreehandReconstructed.mha
    header_.ObjectType                 = "Image";                               // Type of the MetaIO object (must be Image)
    header_.NDims                      = 3;                                     // Number of dimensions for a 3D image (must be 3)
    header_.BinaryData                 = true;                                  // Data is in binary format (must be true)
    header_.BinaryDataByteOrderMSB     = false;                                 // Little-endian byte order (must be false)
    header_.CompressedData             = false;                                 // Data is not compressed
    header_.Kinds                      = {"domain", "domain", "list"};
    header_.TransformMatrix            = {1, 0, 0, 0, 1, 0, 0, 0, 1};           // Initialize with an identity matrix (not used by Plus Toolkit, typical value is identity matrix)
    header_.DimSize                    = { allImages.at(1).cols, allImages.at(1).rows, static_cast<int>(allImages.size())}; // Assuming a 2D image with one slice
    header_.Offset                     = {0, 0, 0};                             // Origin of the image (not used by Plus Toolkit, typical value is 0 0 0)
    header_.CenterOfRotation           = {0, 0, 0};                             // Center of rotation (not used by Plus Toolkit, typical value is 0 0 0)
    header_.AnatomicalOrientation      = "RAI";                                 // Anatomical orientation (not used by Plus Toolkit, typical value is RAI)
    header_.ElementSpacing             = {1, 1, 1};                             // Spacing between elements (not used by Plus Toolkit, typical value is 1 1 1)
    header_.ElementType                = "MET_UCHAR";                           // Data type of the image elements
    header_.UltrasoundImageOrientation = "MFA";                                 // Default value is MFA
    header_.UltrasoundImageType        = "BRIGHTNESS";                          // Default value is BRIGHTNESS (for B-mode)
    header_.ElementDataFile            = "LOCAL";                               // Location of the image data. For *.mha files (must be LOCAL)

    if(!writeHeader()) return -1;
    if(!writeTransformations()) return -2;
    if(!writeImages()) return -3;

    // close the file to save
    mhaFile_.close();

    return 1;
}

bool MHAWriter::writeHeader()
{
    // Write header
    mhaFile_ << "ObjectType = "                 << header_.ObjectType << std::endl;
    mhaFile_ << "NDims = "                      << header_.NDims << std::endl;
    mhaFile_ << "BinaryData = "                 << (header_.BinaryData ? "True" : "False") << std::endl;
    mhaFile_ << "BinaryDataByteOrderMSB = "     << (header_.BinaryDataByteOrderMSB ? "True" : "False") << std::endl;
    mhaFile_ << "CompressedData = "             << (header_.CompressedData ? "True" : "False") << std::endl;
    mhaFile_ << "Kinds = "                      << header_.Kinds.at(0) << " " << header_.Kinds.at(1) << " " << header_.Kinds.at(2) << std::endl;
    mhaFile_ << "TransformMatrix = "            << header_.TransformMatrix.at(0) << " " << header_.TransformMatrix.at(1) << " " << header_.TransformMatrix.at(2) << " "
                                                << header_.TransformMatrix.at(3) << " " << header_.TransformMatrix.at(4) << " " << header_.TransformMatrix.at(5) << " "
                                                << header_.TransformMatrix.at(6) << " " << header_.TransformMatrix.at(7) << " " << header_.TransformMatrix.at(8) << std::endl;
    mhaFile_ << "DimSize = "                    << header_.DimSize.at(0) << " "
                                                << header_.DimSize.at(1) << " "
                                                << header_.DimSize.at(2) << std::endl;
    mhaFile_ << "Offset = "                     << header_.Offset.at(0) << " "
                                                << header_.Offset.at(1) << " "
                                                << header_.Offset.at(2) << std::endl;
    mhaFile_ << "CenterOfRotation = "           << header_.CenterOfRotation.at(0) << " "
                                                << header_.CenterOfRotation.at(1) << " "
                                                << header_.CenterOfRotation.at(2) << std::endl;
    mhaFile_ << "AnatomicalOrientation = "      << header_.AnatomicalOrientation << std::endl;
    mhaFile_ << "ElementSpacing = "             << header_.ElementSpacing.at(0) << " "
                                                << header_.ElementSpacing.at(1) << " "
                                                << header_.ElementSpacing.at(2) << std::endl;
    mhaFile_ << "ElementType = "                << header_.ElementType << std::endl;
    mhaFile_ << "UltrasoundImageOrientation = " << header_.UltrasoundImageOrientation << std::endl;
    mhaFile_ << "UltrasoundImageType = "        << header_.UltrasoundImageType << std::endl;

    // Check if any write operation failed
    if (!mhaFile_)
    {
        // Handle the error
        std::cerr << "Error occurred writing Image Sequence (.mha) file: Error in writing header." << std::endl;
        // Perform necessary cleanup or error handling
        return false; // Return a non-zero value to indicate failure
    }

    return true;
}

// Function to write transformation data
bool MHAWriter::writeTransformations()
{
    // Write the transformations
    for (size_t i = 0; i < allTransforms_probe.size(); ++i) {

        // 1) Write ProbeToTrackerDeviceTransform
        bool isNaN_probe = false;
        const Eigen::Isometry3d& transform_probe = allTransforms_probe[i];
        mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ProbeToTrackerDeviceTransform = ";
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                mhaFile_ << transform_probe.matrix()(row, col) << " ";
                if(transform_probe.matrix()(row, col)!=transform_probe.matrix()(row, col)) isNaN_probe=true;
            }
        }
        mhaFile_ << std::endl;

        // 2) Write ProbeToTrackerDeviceTransformStatus
        //    if the transformation contains NaN, set TransformStatus to invalid
        if (isNaN_probe) mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ProbeToTrackerDeviceTransformStatus = " << "INVALID" << std::endl;
        else mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ProbeToTrackerDeviceTransformStatus = " << "OK" << std::endl;

        // 3) Write ReferenceToTrackerDeviceTransform
        bool isNaN_ref = false;
        const Eigen::Isometry3d& transform_ref = allTransforms_ref[i];
        mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ReferenceToTrackerDeviceTransform = ";
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                mhaFile_ << transform_ref.matrix()(row, col) << " ";
                if(transform_ref.matrix()(row, col)!=transform_ref.matrix()(row, col)) isNaN_ref=true;
            }
        }
        mhaFile_ << std::endl;

        // 4) Write ProbeToTrackerDeviceTransformStatus
        //    if the transformation contains NaN, set TransformStatus to invalid
        if (isNaN_ref) mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ReferenceToTrackerDeviceTransformStatus = " << "INVALID" << std::endl;
        else mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ReferenceToTrackerDeviceTransformStatus = " << "OK" << std::endl;

        // 5) Write the Timestamp
        mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_Timestamp = " << allTimestamps[i] << std::endl;
        // 6) Write the ImageStatus. I am assuming that the image always ok. Lol, hehe.
        mhaFile_ << "Seq_Frame" << std::setfill('0') << std::setw(4) << i << "_ImageStatus = " << "OK" << std::endl;
    }

    // Write this before the image
    mhaFile_ << "ElementDataFile = " << header_.ElementDataFile << std::endl;

    // Check if any write operation failed
    if (!mhaFile_)
    {
        // Handle the error
        std::cerr << "Error occurred writing Image Sequence (.mha) file: Error in writing transformation." << std::endl;
        // Perform necessary cleanup or error handling
        return false; // Return a non-zero value to indicate failure
    }

    return true;
}

// Function to write raw image data
bool MHAWriter::writeImages()
{
    // // For debugging only, to see the resulting images in a window
    // cv::namedWindow("Image Sequence", cv::WINDOW_NORMAL);
    // // for (const auto& image : allImages) {
    // //     cv::imshow("Image Sequence", image);
    // //     cv::waitKey(500);  // Display each image for 1 second
    // // }
    // for (size_t i = 0; i < allImages.size(); ++i) {
    //     const auto& image = allImages[i];
    //     cv::imshow("Image Sequence", image);
    //     cv::waitKey(10);  // Display each image for 1 second
    // }

    // for (size_t i = 0; i < allImages.size(); ++i) {
    //     const auto& myimage = allImages[i];
    //     mhaFile_.write(reinterpret_cast<const char*>(myimage.data), myimage.total() * myimage.elemSize());
    //     std::cout << myimage.total() << "*"<< myimage.elemSize() << std::endl;
    //     mhaFile_.flush();
    // }

    for (size_t i = 0; i < allImages.size(); ++i) {
        mhaFile_.write(reinterpret_cast<const char*>(allImages[i].data), allImages[i].total() * allImages[i].elemSize());
        mhaFile_.flush();
    }

    // mhaFile_.flush();
    // std::vector<uchar> rawData(allImages[0].data, allImages[0].data + allImages[0].total() * allImages[0].elemSize());
    // mhaFile_.write(reinterpret_cast<const char*>(rawData.data()), rawData.size());
    // mhaFile_.flush();

    // Check if any write operation failed
    if (!mhaFile_)
    {
        // Handle the error
        std::cerr << "Error occurred writing Image Sequence (.mha) file: Error in writing binary images." << std::endl;
        // Perform necessary cleanup or error handling
        return false; // Return a non-zero value to indicate failure
    }

    return true;
}

// Function to get the current date and time as a formatted string
std::string MHAWriter::getCurrentDateTime()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&time_t);

    // Format the date and time as a string (e.g., "2024-04-22_16-18-42")
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &local_tm);
    return std::string(buffer);
}

std::string MHAWriter::getFullfilename()
{
    return fullfilename_;
}
