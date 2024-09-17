# A-mode Ultrasound Navigation Software
An A-mode ultrasound (US) + Motion capture -based skeletal kinematic estimation is being developed here in University of Twente. The system used A-mode ultrasound, which is attached to subject's leg, to estimate the distance between skin and the bone surface. At the same time, the USwere tracked by an optical tracking syste, such that the position and the orientation of the sensors can be estimated. Ultimately, bone surface points can be recovered in 3D space, providing a sparse (but a small number) point cloud data which represents the position and the orientation of the bone at a certain timestamp.

Problem arises when the A-mode US were used for detecting the bone surface. A-mode US only provides 1D signal and the bone surface is represented as a disturbance (refered as "peak") in the signal, that is essentially an echo from the bone surface to the skin. However, bone surface is not the only one that reflecting the ultrasound, another layers of soft-tissues also reflects the US. The bone peak and soft-tissue peaks are practically indistinguishable. Hence, some of the times, we detect an incorrect bone peak, which ultimately leads to inaccurate kinematic estimation.

This is a software that can be used by the user to navigate A-mode ultrasound such that its direction is pointing towards the bone, and such that the signal can be represented in 3D. Therfore, allowing the user to select which peaks wihtin the signal that is the true bone peak.

## How Does It Works (in a nutshell)
 - This software combines three hardwares: A-mode Ultrasound Machine, Motion Capture system (currently only Qualisys), and B-mode Ultrasound Machine. 
 - Prior to using this software, a B-mode US calibration is required. This calibration can be done using fCal software from PlusToolkit. The generated "configuration file" (.xml) from fCal, which contains the calibration matrix then can be fed to this software. 
 - With B-mode US probe, the user will perform a swiping gesture to the person's leg. The software then gather the B-mode US images and the corresponding transformation in space. A volume will be automatically reconstructed where the bone surface should be visible.
 - Then the user take an A-mode holder with the probes attached, where they can see the live feed of the signal represented in 3D space. The user can orient ant attach the holder to the person's leg, such that the signal is pointing towards the bone surface. 
 - Using the 3D visualization, the user can select which peak that is corresponding the bone peak. 

 

## Dependencies
1. MSVC2019 installed
2. Qt6 or above, including `3dcore`, `3drender`, `3dinput`, `3dextras`, `datavisualization` modules
3. QCustomPlot(o%09https://www.qcustomplot.com/index.php/download) | Handling the visualization of A-mode ultrasound 2D signal
4. [Qualisys-SDK](o%09https://github.com/qualisys/qualisys_cpp_sdk) | Handling the data streaming from Qualisys Motion Capture System.
5. [Vicon-SDK](o%09https://www.vicon.com/software/datastream-sdk/) | Handling the data streaming from Vicon Motion Capture System.
6. [Eigen-3.4.0](o%09https://gitlab.com/libeigen/eigen/-/releases/3.4.0) | Handling matrix multiplications for coodinate system transformations.
7. [Opencv-4.9.0](o%09https://sourceforge.net/projects/opencvlibrary/files/4.9.0/) | Handling image streaming and processing from B-mode US machine. 
8. [Rapidxml](o%09https://rapidxml.sourceforge.net/) | Parsing the configuration files from fCal to obtain B-mode US calibration matrix.

## Installations

### qcustomplot
1. Download QCustomPlot-sharedlib.tar.gz and QCustomPlot-source.tar.gz from their download page
2. Extract QCustomPlot-sharedlib.tar.gz somewhere (for me, i extract it in C:\ for easy access).
3. Make sure the directory tree shaped like this: 
   root_folder/qcustomplot-sharedlib/sharedlib-compilation/
   root_folder/qcustomplot-sharedlib/sharedlib-usage/
4. Extract QCustomPlot-source.tar.gz to [root folder]. Make sure the qcustomplot.cpp and qcustomplot.h two directory above sharedlib-compilation.pro. The directory should shape like this:
   root_folder/qcustomplot-sharedlib
   root_folder/qcustomplot.cpp
   root_folder/qcustomplot.h
5. Open Qt6 and open the sharedlib-compilation.pro project.
6. Make sure in the project tab (sidebar menu on the left) that the project compiled with MSVC2019. 
7. Back to edit tab, right click on the project root, click build.
8. Once built, there will be [root folder]/qcustomplot-sharedlib/sharedlib-compilation/build directory is generated.
9. Open our Qt6 software project, open the AmodeBmodeMocap.pro
10. Focus on QCustomPlot part, change the LIBS part to match where your lib files are. It should be like:
    LIBS += -L"[root folder]/qcustomplot-sharedlib/sharedlib-compilation/build/Desktop\_Qt\_[[version]]\_MSVC2019\_64bit-Debug/debug"

### Qualisys


### Vicon


### Eigen


### OpenCV
1. Go to opencv download page and download the prebuilt windows version of opencv.
2. Select the directory of where the opencv library will be extracted.
3. The library files are in root_folder\opencv\build\x64\vc16\bin. I don't know that it is necessary to add this directory to your system PATH variable, but better do it.  
4. The header files are in root_folder\opencv\build\include
5. Open our Qt6 software project, open the AmodeBmodeMocap.pro
6. Focus on OpenCV part, change the LIBS, INCLUDEPATH, and DEPENDPATH to match where your lib files and header files are. 