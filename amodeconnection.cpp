#include "AmodeConnection.h"
#include <QDir>
#include <QtEndian>

AmodeConnection::AmodeConnection(QObject *parent, std::string ip, std::string port)
    : QObject{parent}, ip_(ip), port_(port), tcpSocket(new QTcpSocket(this))
{
    // try to connect with server through socket
    if(!connectToServer()) return;

    // initialize data
    initializeData();

    // connect the signal readyRead to our slot readData, so that the streaming starts right away
    connect(tcpSocket, &QTcpSocket::readyRead, this, &AmodeConnection::readData);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &AmodeConnection::handleError);
}

AmodeConnection::~AmodeConnection()
{
    qDebug() << "Destroying AmodeConnection";
    if (tcpSocket && tcpSocket->isOpen()) {
        qDebug() << "Closing tcpsocket";
        tcpSocket->flush();
        tcpSocket->disconnect();
        tcpSocket->close();
    }
}

int AmodeConnection::connectToServer() {
    bool ok;
    QString host_ip   = QString::fromStdString(ip_);
    quint16 host_port = QString::fromStdString(port_).toUShort(&ok, 10);

    tcpSocket->connectToHost(host_ip, host_port);

    if(tcpSocket->waitForConnected(5000)) { // Wait for up to 5 seconds
        qDebug() << "Successfully connected to" << host_ip << "on port" << host_port;
        return 1;
    } else {
        qDebug() << "Failed to connect to" << host_ip << "on port" << host_port;
        return 0;
    }
}

void AmodeConnection::handleError(QTcpSocket::SocketError socketError) {
    // tell the user there is something wrong
    qDebug() << "Socket Error:" << tcpSocket->errorString();

    // perform cleanup
    tcpSocket->flush();
    tcpSocket->disconnect();
    tcpSocket->close();

    // notify the application
    emit errorOccured();

    // schedule this object for deletion
    this->deleteLater();
}

void AmodeConnection::letsdelete()
{
    qDebug() << "lets delete";
    this->deleteLater();
    qDebug() << "yesy";
}

QByteArray AmodeConnection::convertSTDVectorToQByteArray(const std::vector<uint16_t>& vector) {
    QByteArray byteArray;
    byteArray.reserve(vector.size() * sizeof(uint16_t)); // Reserve space to improve performance

    for (uint16_t value : vector) {
        // Convert the uint16_t to a byte array (2 bytes)
        char bytes[2];
        qToLittleEndian(value, reinterpret_cast<uchar*>(bytes));

        // Append the bytes to the QByteArray
        byteArray.append(bytes, sizeof(bytes));
    }

    return byteArray;
}

void AmodeConnection::initializeData()
{
    // Just to clarify, here is how the data looks like:
    // ...[arrayheader][separator][index][data][arrayheader][separator][index][data]...

    // Initialize the separator (i know this because i code the amode machine)
    // It is basically the ASCII number of START, then i add 10000
    separator_int16 = {10083, 10084, 10065, 10082, 10084};
    for (int16_t num : separator_int16) separator_uint16.push_back(static_cast<uint16_t>(num));
    // I need to convert it into QByteArray, because later when i want to search the separator,
    // the data that is acquired from QTcpSocket is in QByteArray
    separator_qbyte  = convertSTDVectorToQByteArray(separator_uint16);
    separatorsize_   = separator_qbyte.size();

    // Initialize the number of bytes of all variables related to data
    usdata_framesize_     = headersize_ + separatorsize_ + indexsize_ + (sizeof(uint16_t) * datalength_);
    usdata_allheadersize_ = separatorsize_ + indexsize_;
    usdata_datasize_      = (sizeof(uint16_t) * datalength_);

    // Initialize the size of usdata_uint16_ and usdata_int16_
    usdata_uint16_.resize(datalength_);
    usdata_int16_.resize(datalength_);

    /*
    // Initial size of the header, this always exist because the amode machine sends an array
    // with several bytes oh header indicating that this is an array
    usdata_headerindexsize_ = headersize_;
    // Initial size of the data. By default, the amode machine sends start+index+data, so we need
    // to expect this amount of bytes
    usdata_datasize_        = indexsize_ + separatorsize_ + (sizeof(uint16_t) * datalength_);

    // First, if this user does not care about the start (separator) bytes, let's increase the allheader size
    // so that we will skip those bytes later and ignore it. We should also expect the usdata_datasize_ to be
    // reduced by the amount of bytes of the start (separator) itself
    if (!usedatastart_)
    {
        usdata_headerindexsize_ += separatorsize_;
        usdata_datasize_        -= separatorsize_;
    }

    // Second, if this user also does not care about the index bytes, let's increase the allheader size
    // so that we will skip separator bytes and index bytes and ignore it. We should also expect the
    // usdata_datasize_ to be reduced further by the amoynt of byte of the index itself
    if (!usedataindex_)
    {
        usdata_headerindexsize_ += indexsize_;
        usdata_datasize_        -= indexsize_;
    }

    // resize the usdata_uint16_ accordingly
    usdata_uint16_.resize(usdata_datasize_ / sizeof(uint16_t));

    // temporary buffer that we will use for store binary string from socket
    // the A-mode ultrasound machine always send full data (header+index+data)
    // so we need to accomodate the full data
    usdata_receivebuffersize_ = (sizeof(uint16_t) * datalength_) + headersize_ + indexsize_;
    */

    /*
    if (usedataindex_) {
        // headerindexsize will be used to skip or not to skip the bytes that represent the index
        // so if the user specified useDataIndex(true), it means we do not skip the index byte
        // thus the headerindexsize is only the size of headersize_
        usdata_headerindexsize_ = headersize_;

        // we should expect how much the size of the data we will get
        // ultrasound_frd needs to be resized with the index byte
        usdata_datasize_ = indexsize_ + (sizeof(uint16_t) * datalength_);
        usdata_uint16_.resize(usdata_datasize_ / sizeof(uint16_t));
    }

    else {
        // if the user specified otherwise, it means we skip the index byte
        // thus the headerindexsize is the total of headersize_ + indexsize_
        usdata_headerindexsize_ = headersize_ + indexsize_;

        // ultrasound_frd needs to be resized with the index byte
        usdata_datasize_ = 0 + (sizeof(uint16_t) * datalength_);
        usdata_uint16_.resize(usdata_datasize_ / sizeof(uint16_t));
    }

    // temporary buffer that we will use for store binary string from socket
    // the A-mode ultrasound machine always send full data (header+index+data)
    // so we need to accomodate the full data
    usdata_receivebuffersize_ = (sizeof(uint16_t) * datalength_) + headersize_ + indexsize_;
    */
}

void AmodeConnection::startStream()
{
    // if you want to start stream separately do this
}

void AmodeConnection::readData() {
    // Retrieve all the data inside the socket, store it to buffer
    buffer.append(tcpSocket->readAll());

    // Initialize the startIndex and endIndex. For clarity:
    // startIndex is the starting bytes of the first separator,
    // endIndex is the starting bytes of the second separator.
    int startIndex = 0;
    int endIndex   = 0;

    // First, let's find our first separator in the buffer. Since endIndex=0, we search from the beginning of buffer.
    // If we find no separator (function indexOf() returns -1) we skip it and wait for more data.
    // If we find a separator, we will execute the loop. Why loop? Because there can be multiple separators in the buffer.
    // We will process the data as long as there are separators inside our buffer until there is no next separator.
    while ((startIndex = buffer.indexOf(separator_qbyte, endIndex)) != -1)
    {
        // At this point, we found a separator. Now, lets search for another separator inside our buffer.
        // Search in the buffer but with a different starting point, we exclude the first separator.
        endIndex = buffer.indexOf(separator_qbyte, startIndex + separatorsize_);

        // If endIndex == -1, we don't see another separator yet. It means that it is an incomplete frame.
        // So, let's get out from the loop and wait for more data
        if (endIndex == -1) break;

        // At this point, we found another separator. Let's take the data now. For reminder, the data looks like this:
        // ...[arrayheader_1][separator_1][index_1][data_1][arrayheader_2][separator_2][index_2][data_2]...
        //                    ^                     ^                      ^
        //                    startIndex            actualData             endIndex
        //
        // To get data, the starting point should be:
        // startIndex + separator + index
        //
        // The amount of bytes we should take will be:
        // (endIndex - startIndex) <== The whole frame, this includes separator & data (at the front), and arrayheader (at the rear)
        // (endIndex - startIndex) - (separator+index+headerarray) <== This is pure data
        QByteArray frame = buffer.mid( (startIndex-sizeof(uint16_t)) + separatorsize_ + indexsize_, (endIndex - startIndex) - (separatorsize_ + indexsize_ + headersize_));

        // Let's process the frame here
        memcpy(usdata_uint16_.data(), frame.constData(), usdata_datasize_);
        // memcpy(usdata_int16_.data(), usdata_uint16_.data(), usdata_datasize_);
        isDataReceived = true;
        // emit dataReceived(usdata_uint16_);
        // qDebug() << usdata_uint16_.size();
        // QString output;
        // for (size_t i = 0; i < usdata_int16_.size() && i < 50; i++) {
        //     output += QString::number(usdata_int16_[i]) + " ";
        // }
        // qDebug().noquote() << output.trimmed();

        // Remove the processed frame from the buffer, mid() function will return the bytes starting at endIndex,
        // which is the starting bytes of the second separator. So after this process, the frame will looks like this
        // [separator_2][index_2][data_2][header_2][separator_3][index_3]...
        buffer = buffer.mid(endIndex);
        // Reset endIndex for the next iteration
        endIndex = 0;

        // Note:
        // >> In the next iteration, the buffer will still have [separator_2] in the beginning.
        // >> Consequently, it will fulfil the condition of while loop. It will execute the line inside the loop.
        // >> However, if there is no next separator found in the buffer ([separator_3]), it will break the loop,
        //    and the code continues.
        // >> Subsequenly, there will be new data availabe in the buffer and this function will be called again.
        // >> As the [separator_2] is still in the buffer, the code still go inside this while loop.
        // >> If this time we found the next separator ([separator_3]), the process will go further like previous loop.
        // >> The code in this while loop also generalize if there is more then 2 separators inside the buffer.
        //    The proccess repeated multiple times within the loop until we don't find the next separator.
    }

    if (isDataReceived) emit dataReceived(usdata_uint16_);
    isDataReceived = false;

    /*
    std::vector<uint16_t> buffer_uint16;
    buffer_uint16.reserve(buffer.size()/ sizeof(uint16_t));
    memcpy(buffer_uint16.data(), buffer.constData(), buffer.size());

    auto startIt = buffer_uint16.begin();
    auto endIt = buffer_uint16.end();

    while (true)
    {
        // Find the start of the frame
        auto frameStartIt = std::search(startIt, endIt, separator_uint16.begin(), separator_uint16.end());
        if (frameStartIt == endIt) break; // Separator not found, incomplete frame

        // Move the iterator past the separator to find the end of the frame
        auto frameEndIt = std::search(frameStartIt + separator_uint16.size(), endIt, separator_uint16.begin(), separator_uint16.end());
        if (frameEndIt == endIt) break; // End separator not found, incomplete frame

        // Extract the frame data
        std::vector<uint16_t> frame(frameStartIt + separator_uint16.size(), frameEndIt);

        // Process the frame here

        // Prepare for the next iteration
        startIt = frameEndIt;
    }

    // Remove the processed data from the vector
    buffer_uint16.erase(separator_uint16.begin(), startIt);
    */

    /*
    static QByteArray buffer;
    buffer.append(tcpSocket->readAll());

    // This loop is making sure that i will only process the data if it has the right amount of byte
    while (buffer.size() >= usdata_receivebuffersize_) {

        // get the first (left) usdata_receivebuffersize_ bytes
        QByteArray frame = buffer.left(usdata_receivebuffersize_);
        // remove first usdata_receivebuffersize_ bytes from the buffer
        buffer.remove(0, usdata_receivebuffersize_);

        // Process the complete frame
        memcpy(usdata_uint16_.data(), frame.constData()+usdata_headerindexsize_, usdata_datasize_);
    }

    emit dataReceived(usdata_uint16_);
    */
}

void AmodeConnection::setRecord(bool flag)
{
    setrecord_ = flag;
}


void AmodeConnection::useDataIndex(bool flag)
{
    usedataindex_ = flag;
}


int AmodeConnection::setDirectory(std::string dirPath)
{
    QDir dir(QString::fromStdString(dirPath));

    if (dir.exists()) {
        recorddirectory_ = dirPath;
        return 1;
    } else {
        if (dir.mkpath(dirPath.c_str())) {
            recorddirectory_ = dirPath;
            return 1;
        } else {
            qDebug() << "Unable to create directory for A-mode Ultrasound logging";
            return 0;
        }
    }
}

int AmodeConnection::getNsample()
{
    return samples_;
}

int AmodeConnection::getNprobe()
{
    return probes_;
}



















