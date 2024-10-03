QT       += core gui 3dcore 3drender 3dinput 3dextras datavisualization

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
# Tell the qcustomplot header that it will be used as library:
DEFINES += QCUSTOMPLOT_USE_LIBRARY

SOURCES += \
    amodeconfig.cpp \
    amodeconnection.cpp \
    amodedatamanipulator.cpp \
    bmode3dvisualizer.cpp \
    bmodeconnection.cpp \
    main.cpp \
    mainwindow.cpp \
    mhareader.cpp \
    mhawriter.cpp \
    qcustomplotintervalwindow.cpp \
    qualisysconnection.cpp \
    qualisystransformationmanager.cpp \
    viconconnection.cpp \
    volume3dcontroller.cpp \
    volumeamodecontroller.cpp \
    volumeamodevisualizer.cpp

HEADERS += \
    amodeconfig.h \
    amodeconnection.h \
    amodedatamanipulator.h \
    bmode3dvisualizer.h \
    bmodeconnection.h \
    mainwindow.h \
    mhareader.h \
    mhawriter.h \
    mocapconnection.h \
    qcustomplotintervalwindow.h \
    qualisysconnection.h \
    qualisystransformationmanager.h \
    ultrasoundconfig.h \
    viconconnection.h \
    volume3dcontroller.h \
    volumeamodecontroller.h \
    volumeamodevisualizer.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# QMAKE_CXXFLAGS += -Wa,-mbig-obj
win32:CONFIG(gcc):QMAKE_CXXFLAGS += -Wa,-mbig-obj
win32:QMAKE_CXXFLAGS += /openmp

# All additional libraries/SDKs for this project
INCLUDEPATH += \
    "C:\eigen-3.4.0" \
    "C:\qualisys_cpp_sdk" \
    "C:\Program Files\Vicon\DataStream SDK\Win64\CPP" \
    "C:\rapidxml-master" \
    "C:\qcustomplot-sharedlib-msvc"

# This is for Qualisys
SOURCES += \
    "C:/qualisys_cpp_sdk/Markup.cpp" \
    "C:/qualisys_cpp_sdk/Network.cpp" \
    "C:/qualisys_cpp_sdk/RTPacket.cpp" \
    "C:/qualisys_cpp_sdk/RTProtocol.cpp"
LIBS += -lws2_32 # Link against the Winsock library
LIBS += -liphlpapi # Link against the IP Helper API library

# This is for Vicon
LIBS += -L"C:\Program Files\Vicon\DataStream SDK\Win64\CPP" -lViconDataStreamSDK_CPP

# This is for OpenCV
win32:CONFIG(release, debug|release): LIBS += -LC:/opencv-4.9.0-msvc/opencv/build/x64/vc16/lib/ -lopencv_world490
else:win32:CONFIG(debug, debug|release): LIBS += -LC:/opencv-4.9.0-msvc/opencv/build/x64/vc16/lib/ -lopencv_world490d
INCLUDEPATH += C:/opencv-4.9.0-msvc/opencv/build/include
DEPENDPATH += C:/opencv-4.9.0-msvc/opencv/build/include

# This is for QCustomPlot
# Link with debug version of qcustomplot if compiling in debug mode, else with release library:
CONFIG(debug, release|debug) {
  win32:QCPLIB = qcustomplotd2
  else: QCPLIB = qcustomplotd
} else {
  win32:QCPLIB = qcustomplot2
  else: QCPLIB = qcustomplot
}
LIBS += -L"C:\qcustomplot-sharedlib-msvc\qcustomplot-sharedlib\sharedlib-compilation\build\Desktop_Qt_6_7_2_MSVC2019_64bit-Debug\debug" -l$$QCPLIB
















