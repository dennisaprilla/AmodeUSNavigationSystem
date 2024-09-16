#include "mhareader.h"
#include <iostream>
#include <sstream>
#include <iterator>

#include <QtZlib/zlib.h>

MHAReader::MHAReader(std::string filename) : filename_(filename)
{
    mhaFile_.open(filename_, std::ios::binary);
    if (!mhaFile_.is_open()) {
        throw std::runtime_error("Unable to open file: " + filename_);
    }
}

bool MHAReader::readHeader(const std::string& text)
{
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line))
    {
        // Find the delimiter
        size_t equalSignPos = line.find('=');

        // Get the first part of the string (key)
        std::string key = line.substr(0, equalSignPos);
        key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);

        // Get the second part of the string (value)
        std::string value = line.substr(equalSignPos + 1);
        value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));

        if (key == "ObjectType")
        {
            header_.ObjectType = value;
        }

        else if (key == "NDims")
        {
            header_.NDims = std::stoi(value);
        }

        else if (key == "BinaryData")
        {
            header_.BinaryData = (value == "True");
        }
        else if (key == "BinaryDataByteOrderMSB")
        {
            header_.BinaryDataByteOrderMSB = (value == "True");
        }

        else if (key == "CompressedData")
        {
            header_.CompressedData = (value == "True");
        }

        else if (key == "TransformMatrix")
        {
            std::istringstream iss_value(value);
            std::vector<int> vec((std::istream_iterator<int>(iss_value)), std::istream_iterator<int>());
            header_.TransformMatrix = vec;
        }

        else if (key == "DimSize")
        {
            std::istringstream iss_value(value);
            std::vector<int> vec((std::istream_iterator<int>(iss_value)), std::istream_iterator<int>());
            header_.DimSize = vec;
        }

        else if (key == "Offset")
        {
            std::istringstream iss_value(value);
            std::vector<double> vec((std::istream_iterator<double>(iss_value)), std::istream_iterator<double>());
            header_.Offset = vec;
        }

        else if (key == "CenterOfRotation")
        {
            std::istringstream iss_value(value);
            std::vector<int> vec((std::istream_iterator<int>(iss_value)), std::istream_iterator<int>());
            header_.CenterOfRotation = vec;
        }

        else if (key == "AnatomicalOrientation")
        {
            header_.AnatomicalOrientation = value;
        }

        else if (key == "ElementSpacing")
        {
            std::istringstream iss_value(value);
            std::vector<double> vec((std::istream_iterator<double>(iss_value)), std::istream_iterator<double>());
            header_.ElementSpacing = vec;
        }

        else if (key == "ElementType")
        {
            header_.ElementType = value;
        }

        else if (key == "UltrasoundImageOrientation")
        {
            header_.UltrasoundImageOrientation = value;
        }

        else if (key == "UltrasoundImageType")
        {
            header_.UltrasoundImageType = value;
        }

        else if (key == "ElementDataFile")
        {
            header_.ElementDataFile = value;
        }
    }

    return true;
}

bool MHAReader::readVolumeImage()
{

    // Read the entire file into a string
    std::stringstream buffer;
    buffer << mhaFile_.rdbuf();
    const std::string fileContents = buffer.str();

    // Find the start of the binary data
    std::string separator = "ElementDataFile = LOCAL\n";
    const size_t binaryStart = fileContents.find(separator);
    // Separate the binary data (from binaryStart to the end)
    const std::string binaryData = fileContents.substr(binaryStart+separator.size());
    // Parse the normal text section (up to binaryStart)
    const std::string normalText = fileContents.substr(0, binaryStart+separator.size());

    // Check if there is error in parsing the header values
    if (!readHeader(normalText)) {
        std::cerr << "Error parsing normal text section." << std::endl;
        return false;
    }

    // Check if there is error in transfering all the binary string data to volumeimage_
    try {
        if(!header_.CompressedData)
        {
            volumeimage_.insert(volumeimage_.end(), binaryData.begin(), binaryData.end());
        }
        else
        {
            // I am still struggling with compressed data, please don't use compressed option
            // std::vector<unsigned char> compressedvolumeimage(binaryData.begin(), binaryData.end());
            // std::vector<unsigned char> volumeimage_ = zlib_decompress(compressedvolumeimage);
        }
    }
    catch (std::exception const& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }

    // Close the file
    mhaFile_.close();

    return true;
}


MHAReader::MHAHeader MHAReader::getMHAHeader()
{
    return header_;
}

std::vector<unsigned char> MHAReader::getMHAVolume()
{
    return volumeimage_;
}

std::vector<unsigned char> MHAReader::zlib_decompress(const std::vector<unsigned char>& Z) {
    // Create an input stream from the compressed data
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = static_cast<uInt>(Z.size());
    stream.next_in = const_cast<Bytef*>(Z.data());

    // Initialize zlib for decompression (skip header checks)
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        std::cerr << "Error initializing zlib for decompression." << std::endl;
        exit(EXIT_FAILURE);
    }

    std::vector<unsigned char> decompressedData;
    unsigned char buffer[8192]; // Adjust buffer size as needed

    while (true) {
        stream.avail_out = sizeof(buffer);
        stream.next_out = buffer;

        int ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) {
            // Decompression complete
            decompressedData.insert(decompressedData.end(), buffer, buffer + stream.total_out);
            break;
        } else if (ret == Z_BUF_ERROR) {
            std::cerr << "Error during zlib decompression." << std::endl;
            exit(EXIT_FAILURE);
        }  else if (ret == Z_MEM_ERROR) {
            std::cerr << "Error during zlib decompression." << std::endl;
            exit(EXIT_FAILURE);
        } else if (ret == Z_STREAM_ERROR) {
            std::cerr << "Error during zlib decompression." << std::endl;
            exit(EXIT_FAILURE);
        } else if (ret == Z_NEED_DICT) {
            std::cerr << "Error during zlib decompression." << std::endl;
            exit(EXIT_FAILURE);
        } else {
            std::cerr << "Error during zlib decompression." << std::endl;
            exit(EXIT_FAILURE);
        }

        decompressedData.insert(decompressedData.end(), buffer, buffer + stream.total_out);
    }

    // Clean up
    inflateEnd(&stream);

    return decompressedData;
}
