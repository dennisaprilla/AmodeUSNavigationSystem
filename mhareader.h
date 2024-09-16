#ifndef MHAREADER_H
#define MHAREADER_H

#include <string>
#include <vector>
#include <fstream>

/**
 * @class MHAReader
 * @brief For Reading Sequence Image (.mha) files BUT specifically for VOLUME Sequence Image.
 *
 * For the context. The user should be able to load (or the software should be able to automatically load)
 * the reconstructed volume data from volume reconstruction module from fCal. Please note that even though
 * reconstructed volume is stored in a Sequence Image (.mha) file, but it is not a regular sequence image.
 * The formatting is a little bit different (you can always check yourself and compare both of them). Here
 * I called it a Volume Sequence Image.
 *
 * Another note. This class only working with the NON-COMPRESSED Volume Sequence Image. The different is just
 * how the voxel data is stored. The non-compressed one is just raw binary data of each of the voxel, while the
 * compressed one is... the compressed one. The volume reconstruction module from fCal has option on how the
 * data should be stored, however, by the time i write this class, i still don't know with what compressing
 * algorithm they compress the data. I can't find it anywhere in the documentation. How the fuck do i know to
 * decompress them.
 *
 */

class MHAReader
{
public:

    /**
     * @brief Constructor function
     */
    MHAReader(std::string filename);

    /**
     * @struct MHAHeader
     * @brief Represents the attribute list for the header of Sequence Image file
     *
     * The header of Sequence Image file consists of relevant information of the Sequence Image
     * themselves. It is important for volume reconstruction.
     */
    struct MHAHeader {
        std::string         ObjectType;
        int                 NDims;
        bool                BinaryData;
        bool                BinaryDataByteOrderMSB;
        bool                CompressedData;
        std::vector<int>    TransformMatrix;
        std::vector<int>    DimSize;
        std::vector<double> Offset;
        std::vector<int>    CenterOfRotation;
        std::string         AnatomicalOrientation;
        std::vector<double> ElementSpacing;
        std::string         ElementType;
        std::string         UltrasoundImageOrientation;
        std::string         UltrasoundImageType;
        std::string         ElementDataFile;
    };

    /**
     * @brief Read the volume sequence image, it will read the header and the data.
     */
    bool readVolumeImage();

    /**
     * @brief GET the header of the volume sequence image.
     */
    MHAReader::MHAHeader getMHAHeader();

    /**
     * @brief GET the volume data of the volume sequence image.
     */
    std::vector<unsigned char> getMHAVolume();

private:

    /**
     * @brief Read and parse the header part of the volume sequence image.
     */
    bool readHeader(const std::string &text);

    /**
     * @brief [Deprecated] My failed attempt to decompress the compressed volume sequence image.
     */
    std::vector<unsigned char> zlib_decompress(const std::vector<unsigned char>& Z);



    std::vector<unsigned char> volumeimage_;    //!< Stores the raw binary data of the voxels.
    std::string filename_;                      //!< Stores the full path and file name of the volume sequence image.
    MHAReader::MHAHeader header_;               //!< Stores the necessary information related to the volume sequence image.
    std::ifstream mhaFile_;                     //!< Stores the stream object of the volume sequence image.
};

#endif // MHAREADER_H
