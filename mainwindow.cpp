#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <opencv2/imgproc.hpp>
#include <QMessageBox>
#include <QProcess>

#include <regex>
#include "qualisystransformationmanager.h"
#include "amodedatamanipulator.h"
#include "ultrasoundconfig.h"

#include <Qt3DExtras/Qt3DWindow>
#include <QtWidgets/QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize d_vector and t_vector for plotting purposes
    us_dvector_             = Eigen::VectorXd::LinSpaced(UltrasoundConfig::N_SAMPLE, 1, UltrasoundConfig::N_SAMPLE) * UltrasoundConfig::DS;             // [[mm]]
    us_tvector_             = Eigen::VectorXd::LinSpaced(UltrasoundConfig::N_SAMPLE, 1, UltrasoundConfig::N_SAMPLE) * UltrasoundConfig::DT * 1000000;   // [[mu s]]
    us_dvector_downsampled_ = AmodeDataManipulator::downsampleVector(us_dvector_, round((double)UltrasoundConfig::N_SAMPLE / downsample_ratio_));
    us_tvector_downsampled_ = AmodeDataManipulator::downsampleVector(us_tvector_, round((double)UltrasoundConfig::N_SAMPLE / downsample_ratio_));
    downsample_nsample_     = us_dvector_downsampled_.size();

    // test create a new QCustomPlot object
    amodePlot = new QCustomPlotIntervalWindow(this);
    amodePlot->setObjectName("amode_originalplot");
    amodePlot->setShadeColor(QColor(255, 0, 0, 50));
    amodePlot->setInitialSpacing(3);
    amodePlot->xAxis->setLabel("Depth (mm)");
    amodePlot->yAxis->setLabel("Amplitude");
    amodePlot->xAxis->setRange(0, us_dvector_downsampled_.coeff(us_dvector_downsampled_.size() - 1));
    amodePlot->yAxis->setRange(-500, 7500);
    amodePlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->gridLayout_amodeSignals->addWidget(amodePlot);

    // Initalize scatter object, can also only be done programatically
    scatter = new Q3DScatter();
    scatter->setMinimumSize(QSize(2048, 2048));
    scatter->setOrthoProjection(true);
    scatter->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetIsometricLeftHigh);
    scatter->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
    scatter->axisX()->setTitle("X Axis");
    scatter->axisY()->setTitle("Z Axis");
    scatter->axisZ()->setTitle("Y Axis");
    scatter->axisX()->setTitleVisible(true);
    scatter->axisY()->setTitleVisible(true);
    scatter->axisZ()->setTitleVisible(true);
    scatter->axisX()->setLabelFormat("");
    scatter->axisY()->setLabelFormat("");
    scatter->axisZ()->setLabelFormat("");
    scatter->axisX()->setSegmentCount(1);
    scatter->axisY()->setSegmentCount(1);
    scatter->axisZ()->setSegmentCount(1);
    scatter->setAspectRatio(1.0);
    scatter->setHorizontalAspectRatio(1.0);
    scatter->axisX()->setRange(-200, 200);
    scatter->axisY()->setRange(0, 400);
    scatter->axisZ()->setRange(-200, 200);


    // Assuming 'scatter' is a pointer to your Q3DScatter instance
    Q3DCamera *camera = scatter->scene()->activeCamera();
    float currentZoomLevel = camera->zoomLevel();
    float newZoomLevel = currentZoomLevel + 110.0f;
    camera->setZoomLevel(newZoomLevel);

    // Put the scatter object to a container and add it to the UI
    QWidget *containerScatter = QWidget::createWindowContainer(scatter);
    containerScatter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->layout_volume->removeItem(ui->verticalSpacer_volume);
    ui->layout_volume->addWidget(containerScatter);

    // I put the initialization of myBmodeConnection here. It is quite different than
    // the other connection objects. That is okay, because BmodeConnection basically
    // just opening a camera, so it happen locally.
    // Other reason is that i want to list all of the port available. I was planning to
    // list the name of the port, but apparently it is difficult as they are deep in
    // Microsoft media API. So i just give info about the resolution.
    myBmodeConnection = new BmodeConnection();
    std::vector<std::string> allCameraInfo = myBmodeConnection->getAllCameraInfo();
    for(const std::string &str : allCameraInfo) {
        ui->comboBox_camera->addItem(QString::fromStdString(str));
    }

}

MainWindow::~MainWindow()
{
    delete ui;
    // delete myBmodeConnection;
    // delete myMocapConnection;
    // delete myMHAWriter;
    // delete myAmodeConnection;
    // delete myAmodeConfig;
}


/* *****************************************************************************************
 * *****************************************************************************************
 *
 * Everything that is related to B-mode image streaming
 *
 * *****************************************************************************************
 * ***************************************************************************************** */

void MainWindow::displayImage(const cv::Mat &image) {
    // Convert the cv::Mat to a QImage
    // QImage qImage(image.data, image.cols, image.rows, image.step, QImage::Format_Grayscale8);

    QImage qImage(image.cols, image.rows, QImage::Format_Grayscale8);
    memcpy(qImage.bits(), image.data, static_cast<size_t>(image.cols * image.rows));

    // Convert the QImage to a QPixmap and scale it to fit the QLabel while maintaining the aspect ratio
    QPixmap pixmap = QPixmap::fromImage(qImage);
    if (isBmode2dFirstStream)
    {
        bmode2dvisheight = ui->label_imageDisplay->size().height();
        isBmode2dFirstStream = false;
    }

    QPixmap pixmap_scaled = pixmap.scaledToHeight(bmode2dvisheight);
    ui->label_imageDisplay->setPixmap(pixmap_scaled);
}

/*
void MainWindow::on_pushButton_startCamera_clicked()
{
    // instantiate B-mode connection class
    myBmodeConnection = new BmodeConnection();
    // Connect the imageProcessed signal to the displayImage slot
    connect(myBmodeConnection, &BmodeConnection::imageProcessed, this, &MainWindow::displayImage);

    int cameraIndex = ui->comboBox_camera->currentIndex();
    if(myBmodeConnection->openCamera(cameraIndex)) {
        myBmodeConnection->startImageStream();
    } else {
        // Handle the error (e.g., show a message box)
        QMessageBox::critical(this, tr("Error"), tr("Unable to open the camera. Please check the camera index and ensure it is connected properly."));
    }
}
*/





/* *****************************************************************************************
 * *****************************************************************************************
 *
 * Everything that is related to Qualisys Rigid-body streaming
 *
 * *****************************************************************************************
 * ***************************************************************************************** */

/*
void MainWindow::on_pushButton_qualisysConnect_clicked()
{
    QString qualisys_ip = ui->lineEdit_qualisysIP->text();
    std::string qualisys_ipstr = qualisys_ip.toStdString();
    std::regex ipRegex("^(\\d{1,3}\\.){3}\\d{1,3}$");

    // check if the input ip looks like an ip
    if (!std::regex_match(qualisys_ipstr, ipRegex))
    {
        // Inform the user about the invalid input
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid ip address.");
        return;
    }

    QString qualisys_port = ui->lineEdit_qualisysPort->text();
    bool ok; // Variable to check if the conversion was successful
    unsigned short qualisys_portushort = qualisys_port.toUShort(&ok);

    // check if string conversion to ushort is successful
    if(!ok)
    {
        // Inform the user about the invalid input
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid port number (0-65535).");
        return;
    }

    myMocapConnection = new QualisysConnection(nullptr, qualisys_ipstr, qualisys_portushort);
    connect(myMocapConnection, &QualisysConnection::dataReceived, this, &MainWindow::updateQualisysText);

    // Only intantiate Bmode3DVisualizer after myMocapConnection is instantiated (and connected)
    // <!> I was thinking that probably i don't need to pass the myBmodeConnection and myMocapConnection to the constructor
    // <!> Better to just connect the signal and the slot outside the constructor
    // myBmode3Dvisualizer = new Bmode3DVisualizer(nullptr, myBmodeConnection, myMocapConnection);
    myBmode3Dvisualizer = new Bmode3DVisualizer(nullptr);
    connect(myBmodeConnection, &BmodeConnection::imageProcessed, myBmode3Dvisualizer, &Bmode3DVisualizer::onImageReceived);
    connect(myMocapConnection, &QualisysConnection::dataReceived, myBmode3Dvisualizer, &Bmode3DVisualizer::onRigidBodyReceived);

    // adjust the layout
    ui->textEdit_qualisysLog->hide();
    // ui->layout_Bmode3D->removeItem(ui->verticalSpacer_Bmode3D);
    ui->layout_Bmode2D3D_content->addWidget(myBmode3Dvisualizer);
    ui->layout_Bmode2D3D_content->setStretch(0,3);
    ui->layout_Bmode2D3D_content->setStretch(2,7);
}
*/

void MainWindow::updateQualisysText(const QualisysTransformationManager &tmanager) {
    ui->textEdit_qualisysLog->clear(); // Clear the existing text

    // get all the rigid body from QUalisys
    QualisysTransformationManager T_all = tmanager;
    Eigen::IOFormat txt_matrixformat(2, 0, ", ", "; ", "[", "]", "", "");

    // std::vector<Eigen::Isometry3d> allTransforms = T_all.getAllTransformations();
    // for (const auto& transform : allTransforms) {
    //     std::cout << "Transformation Matrix:\n" << transform.matrix() << std::endl;
    // }
    // std::vector<std::string> allIds = T_all.getAllIds();
    // for (const auto& id : allIds) {
    //     std::cout << "ID: " << id << std::endl;
    // }

    std::vector<std::string> allIds = T_all.getAllIds();
    std::stringstream ss;
    for (const auto& id : allIds) {
        Eigen::Isometry3d T = T_all.getTransformationById(id);
        ss << id << " : ";
        ss << T.matrix().format(txt_matrixformat);
        ss << std::endl;
    }

    // spit an output
    ui->textEdit_qualisysLog->setPlainText(QString::fromStdString(ss.str())); // Add new text
}


/* *****************************************************************************************
 * Somehting new
 * ***************************************************************************************** */

void MainWindow::on_pushButton_calibbrowse_clicked()
{
    // Open a file dialog and get the selected file path
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "D:/", tr("Files (*.xml)"));

    // Check if the user selected a file
    if (!filePath.isEmpty()) {
        // Set the file path in the QLineEdit
        ui->lineEdit_calibconfig->setText(filePath);
        // Set also automatically in the QLineEdit in the volume section
        ui->lineEdit_volumeConfig->setText(filePath);
    }
}

void MainWindow::on_pushButton_bmode2d3d_clicked()
{
    // If this is stream for bmode2d3d, let's stream them.
    // Context: I separate stream between bmode2d3d and amode, for efficiency reason. So, when i stream
    // qualisys and bmode, i will pause the amode, and also vice versa.
    if(isBmode2d3dStream)
    {
        // If this is the first stream, initialize everything
        if(isBmode2d3dFirstStream)
        {
            /* **********************************************************************************
             * Form Check
             * ********************************************************************************** */

            // check if the calibration config already set
            if(ui->lineEdit_calibconfig->text().isEmpty())
            {
                // Inform the user about the invalid input
                QMessageBox::warning(this, "Input Missing", "Please select the calibration configuration file.");
                return;
            }

            // Big notes from Dennis. This code started being developed using Qualisys in mind. However,
            // in the later stage of coding, we decided to use Vicon instead of Qualisys. I made a new
            // clase for Vicon connection, however, i am too scared to rename all qualisys related variable.
            // So, keep in mind, that when i say qualisys in a variable, it can be also for vicon.

            QString qualisys_ip = ui->lineEdit_qualisysIP->text();
            std::string qualisys_ipstr = qualisys_ip.toStdString();
            std::regex ipRegex("^(\\d{1,3}\\.){3}\\d{1,3}$");

            // check if the input ip looks like an ip
            if (!std::regex_match(qualisys_ipstr, ipRegex))
            {
                // Inform the user about the invalid input
                QMessageBox::warning(this, "Invalid Input", "Please enter a valid ip address.");
                return;
            }

            QString qualisys_port = ui->lineEdit_qualisysPort->text();
            bool ok; // Variable to check if the conversion was successful
            unsigned short qualisys_portushort = qualisys_port.toUShort(&ok);

            // check if string conversion to ushort is successful
            if(!ok)
            {
                // Inform the user about the invalid input
                QMessageBox::warning(this, "Invalid Input", "Please enter a valid port number (0-65535).");
                return;
            }

            /* **********************************************************************************
             * B-mode Image Stream
             * ********************************************************************************** */

            // myBmodeConnection = new BmodeConnection();
            // std::vector<std::string> allCameraInfo = myBmodeConnection->getAllCameraInfo();
            // Connect the imageProcessed signal to the displayImage slot
            // connect(myBmodeConnection, &BmodeConnection::imageProcessed, this, &MainWindow::displayImage);

            int cameraIndex = ui->comboBox_camera->currentIndex();
            if(myBmodeConnection->openCamera(cameraIndex)) {
                myBmodeConnection->startImageStream();
            } else {
                // Handle the error (e.g., show a message box)
                QMessageBox::critical(this, tr("Error"), tr("Unable to open the camera. Please check the camera index and ensure it is connected properly."));
            }

            // Connect the imageProcessed signal to the displayImage slot
            connect(myBmodeConnection, &BmodeConnection::imageProcessed, this, &MainWindow::displayImage);

            /* **********************************************************************************
             * Qualisys / Vicon Stream
             * ********************************************************************************** */

            // if the current combobox index is zero, it means vicon is selected
            if(ui->comboBox_mocapSystem->currentIndex()==0)
            {
                std::string viconHostname = qualisys_ipstr + ":" + std::to_string(qualisys_portushort);
                myMocapConnection = new ViconConnection(nullptr, viconHostname);
                // myMocapConnection->setDataStream("marker");
                myMocapConnection->startStreaming();
            }
            // if the current combobox index is one, it means qualisys is selected
            else
            {
                myMocapConnection = new QualisysConnection(nullptr, qualisys_ipstr, qualisys_portushort);
                myMocapConnection->startStreaming();
                // connect(myMocapConnection, &QualisysConnection::dataReceived, this, &MainWindow::updateQualisysText);
            }

            // Only intantiate Bmode3DVisualizer after myMocapConnection is instantiated (and connected)
            // <!> I was thinking that probably i don't need to pass the myBmodeConnection and myMocapConnection to the constructor
            // <!> Better to just connect the signal and the slot outside the constructor
            // myBmode3Dvisualizer = new Bmode3DVisualizer(nullptr, myBmodeConnection, myMocapConnection);
            myBmode3Dvisualizer = new Bmode3DVisualizer(nullptr, ui->lineEdit_calibconfig->text());
            connect(myBmodeConnection, &BmodeConnection::imageProcessed, myBmode3Dvisualizer, &Bmode3DVisualizer::onImageReceived);
            connect(myMocapConnection, &MocapConnection::dataReceived, myBmode3Dvisualizer, &Bmode3DVisualizer::onRigidBodyReceived);

            QFrame* borderFrame = new QFrame(this);
            borderFrame->setFrameStyle(QFrame::Box | QFrame::Plain);
            borderFrame->setLineWidth(1);

            // Set the background color to black
            QPalette pal = borderFrame->palette();
            pal.setColor(QPalette::Window, Qt::black);
            borderFrame->setAutoFillBackground(true);
            borderFrame->setPalette(pal);

            QVBoxLayout* frameLayout = new QVBoxLayout(borderFrame);
            frameLayout->setContentsMargins(0, 0, 0, 0);
            frameLayout->addWidget(myBmode3Dvisualizer);

            // adjust the layout
            ui->textEdit_qualisysLog->hide();
            ui->layout_Bmode2D3D_content->addWidget(borderFrame, 1, 1);

            // Change the flag
            isBmode2d3dFirstStream = false;
        }

        // If this is not the first time streaming, don't initialize anything anymore,
        // just connect all the slots for myBmode3Dvisualizer and myMocapConnection
        else
        {
            slotConnect_Bmode2d3d();
        }

        // Change the text
        ui->pushButton_bmode2d3d->setText("Pause");
        // change the isBmode2d3dStream to be false
        isBmode2d3dStream = false;

        // [!] Because of performance issue when visualizing Bmode2d3d and Amode2d3d at the same time,
        // we need to stop one and and continue the other
        isAmodeStream = false;
        on_pushButton_amodeConnect_clicked();
        isAmodeStream = true;

        // [!] We also need to stop the 3d volume amode signal visualization by unchecking the show signal
        on_checkBox_volumeShow3DSignal_clicked(false);
        ui->checkBox_volumeShow3DSignal->setCheckState(Qt::Unchecked);
    }

    // if the user click pause, let's disconnect all the slots to myBmode3Dvisualizer and myMocapConnection
    else
    {


        ui->pushButton_bmode2d3d->setText("Continue");
        slotDisconnect_Bmode2d3d();

        // change the isBmode2d3dStream to be true
        isBmode2d3dStream = true;
    }
}

void MainWindow::slotConnect_Bmode2d3d()
{
    if(myMocapConnection==nullptr || myBmodeConnection==nullptr) return;

    connect(myBmodeConnection, &BmodeConnection::imageProcessed, this, &MainWindow::displayImage);
    // connect(myMocapConnection, &MocapConnection::dataReceived, this, &MainWindow::updateQualisysText);
    connect(myBmodeConnection, &BmodeConnection::imageProcessed, myBmode3Dvisualizer, &Bmode3DVisualizer::onImageReceived);
    connect(myMocapConnection, &MocapConnection::dataReceived, myBmode3Dvisualizer, &Bmode3DVisualizer::onRigidBodyReceived);
}

void MainWindow::slotDisconnect_Bmode2d3d()
{
    if(myMocapConnection==nullptr || myBmodeConnection==nullptr) return;

    disconnect(myBmodeConnection, &BmodeConnection::imageProcessed, this, &MainWindow::displayImage);
    // disconnect(myMocapConnection, &MocapConnection::dataReceived, this, &MainWindow::updateQualisysText);
    disconnect(myBmodeConnection, &BmodeConnection::imageProcessed, myBmode3Dvisualizer, &Bmode3DVisualizer::onImageReceived);
    disconnect(myMocapConnection, &MocapConnection::dataReceived, myBmode3Dvisualizer, &Bmode3DVisualizer::onRigidBodyReceived);
}


/* *****************************************************************************************
 * *****************************************************************************************
 *
 * Everything that is related to Volume Reconstruction module
 *
 * *****************************************************************************************
 * ***************************************************************************************** */

void MainWindow::on_pushButton_mhaPath_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "D:\\", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // Check if the user selected a file
    if (!folderPath.isEmpty()) {
        // Set the file path in the QLineEdit
        ui->lineEdit_mhaPath->setText(folderPath + "/");
        ui->lineEdit_volumeOutput->setText(folderPath + "/");
    }
}

void MainWindow::on_pushButton_mhaRecord_clicked()
{
    if (ui->lineEdit_mhaPath->text().isEmpty())
    {
        // Inform the user about the invalid input
        QMessageBox::warning(this, "Empty Directory", "Please select the recoding directory first before conducting the recording");
        return;
    }

    // get the value from the lineedit
    std::string filepath = ui->lineEdit_mhaPath->text().toStdString();

    if(isMHArecord)
    {
        // set the text of the button to stop, indicating the user that the same button is used for stopping
        ui->pushButton_mhaRecord->setText("Stop");
        ui->pushButton_mhaRecord->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop));
        // instantiate new mhawriter, with the file name from textfield
        myMHAWriter = new MHAWriter(nullptr, filepath, "SequenceRecording");
        myMHAWriter->setTransformationID("B_N_PRB", "B_N_REF");
        // start record (for the moment, inside this function is just a bool indicating that we are recording)
        myMHAWriter->startRecord();
        // connect the bmode and qualisys signal data to the mhawriter data receiving slot
        connect(myBmodeConnection, &BmodeConnection::imageProcessed, myMHAWriter, &MHAWriter::onImageReceived);
        connect(myMocapConnection, &MocapConnection::dataReceived, myMHAWriter, &MHAWriter::onRigidBodyReceived);
    }
    else
    {
        // set the text to record again, indicating we finished recording
        ui->pushButton_mhaRecord->setText("Record");
        ui->pushButton_mhaRecord->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaRecord));

        // disconnect the bmode and qualisys data to mhawriter object
        disconnect(myBmodeConnection, &BmodeConnection::imageProcessed, myMHAWriter, &MHAWriter::onImageReceived);
        disconnect(myMocapConnection, &MocapConnection::dataReceived, myMHAWriter, &MHAWriter::onRigidBodyReceived);

        // tell the mhawriter object to stop recording and start writing it to the file
        int recordstatus = myMHAWriter->stopRecord();
        if(recordstatus==1)
        {
            QMessageBox::information(this, "Writing Successful", "Writing Image Sequence (.mha) file successfull.");
            // if successful, let's write the full path to lineEdit_volumeRecording, make user's life easier
            ui->lineEdit_volumeRecording->setText(QString::fromStdString(myMHAWriter->getFullfilename()));
        }
        else if(recordstatus==-1)
            QMessageBox::critical(this, "Writing Error", "Error occurred writing Image Sequence (.mha) file: Error in writing header.");
        else if(recordstatus==-2)
            QMessageBox::critical(this, "Writing Error", "Error occurred writing Image Sequence (.mha) file: Error in writing transformations.");
        else if(recordstatus==-3)
            QMessageBox::critical(this, "Writing Error", "Error occurred writing Image Sequence (.mha) file: Error in writing binary images.");


        // delete the object
        delete myMHAWriter;

        // if the user specified autoReconstruct, execute the code for volume reconstruct
        if(ui->checkBox_autoReconstruct->isChecked()) on_pushButton_volumeReconstruct_clicked();
    }

    isMHArecord = !isMHArecord;
}

void MainWindow::on_checkBox_autoReconstruct_stateChanged(int arg1)
{
    if(arg1)
    {
        // Check if this is the first click...
        if (isAutoReconstructFirstClick)
        {
            // ...we want to give some message to the user for clarity of using this checkbox
            QMessageBox::information(this, "Important Note", "When you enable the auto-reconstruction feature, the system will automatically use the Configuration File (.xml) that you have chosen for setting up the calibration. Additionally, it will select the Sequence Image File (.mha) by itself from the images you've recorded.");
            isAutoReconstructFirstClick = false;
        }

        // Check if the config and the recording path is already initialized in their respecting QLineEdits
        if (ui->lineEdit_volumeOutput->text().isEmpty())
        {
            // Inform the user that they need to fill all the necessary form
            QMessageBox::warning(this, "Empty Form", "Please select the Output Path to allow auto reconstruction");
            // Make the checkbox unchecked again
            ui->checkBox_autoReconstruct->setCheckState(Qt::Unchecked);
            return;
        }

        // disable buttons that might influence the process of auto-reconstruction
        ui->pushButton_volumeBrowseConfig->setEnabled(false);
        ui->pushButton_volumeBrowseRecording->setEnabled(false);
        ui->pushButton_volumeBrowseOutput->setEnabled(false);
        ui->pushButton_volumeReconstruct->setEnabled(false);
        ui->pushButton_volumeLoad->setEnabled(false);
    }
    else
    {
        // enable back buttons that was disabled for auto-reconstruction process
        ui->pushButton_volumeBrowseConfig->setEnabled(true);
        ui->pushButton_volumeBrowseRecording->setEnabled(true);
        ui->pushButton_volumeBrowseOutput->setEnabled(true);
        ui->pushButton_volumeReconstruct->setEnabled(true);
        ui->pushButton_volumeLoad->setEnabled(true);
    }
}

void MainWindow::on_pushButton_volumeLoad_clicked()
{
    // Open a file dialog and get the selected file path
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "D:/", tr("Files (*.mha)"));

    // Check if the user selected a file
    if (filePath.isEmpty()) return;

    // Set the file path in the QLineEdit
    ui->lineEdit_volumeSource->setText(filePath);

    // Delete everything in the scatterplot
    for (QScatter3DSeries *series : scatter->seriesList()) {
        scatter->removeSeries(series);
    }

    // delete the related object, somehow there is bug if i dont do this.
    if (myMHAReader!=nullptr) delete myMHAReader;
    if (myVolume3DController!=nullptr) delete myVolume3DController;

    // Instantiate MHAReader object to read the mha file (special for volume)
    myMHAReader = new MHAReader(filePath.toStdString());
    // Read the volume image
    myMHAReader->readVolumeImage();
    // Instantiate Volume3DController, pass the scatter object and mhareader object so that the class can
    // manipulate the scatter and decode the data according to the mha
    myVolume3DController = new Volume3DController(nullptr, scatter, myMHAReader);

    // set initial threshold for slider
    std::array<int, 2> pixelintensityrange = myVolume3DController->getPixelIntensityRange();
    int init_range     = pixelintensityrange[1] - pixelintensityrange[0];
    int init_threshold = pixelintensityrange[0] + (init_range/2);
    ui->horizontalSlider_volumeThreshold->setMinimum(pixelintensityrange[0]+init_range*0.4);
    ui->horizontalSlider_volumeThreshold->setMaximum(pixelintensityrange[1]-init_range*0.1);
    ui->horizontalSlider_volumeThreshold->setSliderPosition(init_threshold);
    // set the label for slider
    ui->label_volumePixelValMin->setText(QString::number(pixelintensityrange[0]+init_range*0.4));
    ui->label_volumePixelValMax->setText(QString::number(pixelintensityrange[1]-init_range*0.1));

    // Connect the slider signal to updateVolume, if the user slide the threshold, the volume also change accordingly    
    connect(ui->horizontalSlider_volumeThreshold, &QSlider::valueChanged, myVolume3DController, &Volume3DController::updateVolume);
}


void MainWindow::on_pushButton_volumeBrowseConfig_clicked()
{
    // Open a file dialog and get the selected file path
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "D:/", tr("Files (*.xml)"));

    // Check if the user selected a file
    if (!filePath.isEmpty()) {
        // Set the file path in the QLineEdit
        ui->lineEdit_volumeConfig->setText(filePath);
    }
}


void MainWindow::on_pushButton_volumeBrowseRecording_clicked()
{
    // Open a file dialog and get the selected file path
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "D:/", tr("Files (*.mha)"));

    // Check if the user selected a file
    if (!filePath.isEmpty()) {
        // Set the file path in the QLineEdit
        ui->lineEdit_volumeRecording->setText(filePath);
    }
}

void MainWindow::on_pushButton_volumeBrowseOutput_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "D:\\", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // Check if the user selected a file
    if (!folderPath.isEmpty()) {
        // Set the file path in the QLineEdit
        ui->lineEdit_volumeOutput->setText(folderPath + "/");
    }
}

void MainWindow::on_pushButton_volumeReconstruct_clicked()
{
    // Check if the config and the recording path is already initialized in their respecting QLineEdits
    if (ui->lineEdit_volumeConfig->text().isEmpty() ||
        ui->lineEdit_volumeRecording->text().isEmpty() ||
        ui->lineEdit_volumeOutput->text().isEmpty())
    {
        // Inform the user about the invalid input
        QMessageBox::warning(this, "Empty Form", "Either configuration or recording file is empty. Make sure both files are already selected.");
        return;
    }

    // Create postfix for file naming based on time
    // Format the date and time as a string (e.g., "2024-04-22_16-18-42")
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&time_t);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &local_tm);
    QString string_time(buffer);
    QString output_volume_file = ui->lineEdit_volumeOutput->text() + "VolumeOutput_" + string_time + ".mha";

    // Set the output form
    ui->lineEdit_volumeSource->setText(output_volume_file);

    // ATTENTION: You need to change this absolute value
    QString file_exe    = "C:/Users/DennisChristie/PlusApp-2.9.0.20240320-Win64/bin/VolumeReconstructor.exe";
    QString file_config = "--config-file=" + ui->lineEdit_volumeConfig->text();
    QString file_source = "--source-seq-file=" + ui->lineEdit_volumeRecording->text();
    QString file_output = "--output-volume-file=" + output_volume_file;
    QString param1      = "--image-to-reference-transform=ImageToReference";
    QString param2      = "--disable-compression";

    // QString command     = file_exe+file_config+file_source+file_output+param1+param2;
    // qDebug() << command;

    process = new QProcess(this);
    QString executablePath = file_exe;
    QStringList arguments;
    arguments << file_config << file_source << file_output << param2;

    process->start(executablePath, arguments);
    qDebug() << process->state();
    if (process->state() == QProcess::NotRunning) {
        qDebug() << "The process failed to start.";
    }

    connect(process, &QProcess::finished, this, &MainWindow::volumeReconstructorCmdFinished);
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::volumeReconstructorCmdStandardOutput);
    connect(process, &QProcess::readyReadStandardError, this, &MainWindow::volumeReconstructorCmdStandardError);
}

void MainWindow::volumeReconstructorCmdStandardOutput() {
    QByteArray standardOutput = process->readAllStandardOutput();
    qDebug() << "Standard Output:" << QString(standardOutput);
}

void MainWindow::volumeReconstructorCmdStandardError() {
    QByteArray standardError = process->readAllStandardError();
    qDebug() << "Standard Error:" << QString(standardError);
}

void MainWindow::volumeReconstructorCmdFinished()
{
    qDebug() << "Process exited with code:" << process->exitCode();

    // Instantiate MHAReader object to read the mha file (special for volume)
    myMHAReader = new MHAReader(ui->lineEdit_volumeSource->text().toStdString());
    // Read the volume image
    myMHAReader->readVolumeImage();
    // Instantiate Volume3DController, pass the scatter object and mhareader object so that the class can
    // manipulate the scatter and decode the data according to the mha
    myVolume3DController = new Volume3DController(nullptr, scatter, myMHAReader);

    // set initial threshold for slider (i copy paste from MainWindow::on_pushButton_volumeLoad_clicked)
    std::array<int, 2> pixelintensityrange = myVolume3DController->getPixelIntensityRange();
    int init_range     = pixelintensityrange[1] - pixelintensityrange[0];
    int init_threshold = pixelintensityrange[0] + (init_range/2);
    ui->horizontalSlider_volumeThreshold->setMinimum(pixelintensityrange[0]+init_range*0.4);
    ui->horizontalSlider_volumeThreshold->setMaximum(pixelintensityrange[1]-init_range*0.1);
    ui->horizontalSlider_volumeThreshold->setSliderPosition(init_threshold);
    // set the label for slider
    ui->label_volumePixelValMin->setText(QString::number(pixelintensityrange[0]+init_range*0.4));
    ui->label_volumePixelValMax->setText(QString::number(pixelintensityrange[1]-init_range*0.1));

    // Connect the slider signal to updateVolume, if the user slide the threshold, the volume also change accordingly
    connect(ui->horizontalSlider_volumeThreshold, &QSlider::valueChanged, myVolume3DController, &Volume3DController::updateVolume);
}





/* *****************************************************************************************
 * *****************************************************************************************
 *
 * Everything that is related to A-mode signal streaming
 *
 * *****************************************************************************************
 * ***************************************************************************************** */


void MainWindow::on_pushButton_amodeConnect_clicked()
{
    // Everytime the user click this button, isAmodeStream state is changed true and false.
    // If it is true, it means that the system is ready to stream, hence when clicked it will start streaming.
    if (isAmodeStream)
    {
        QString amode_ip = ui->lineEdit_amodeIP->text();
        std::string amode_ipstr = amode_ip.toStdString();
        std::regex ipRegex("^(\\d{1,3}\\.){3}\\d{1,3}$");

        // check if the input ip looks like an ip
        if (!std::regex_match(amode_ipstr, ipRegex))
        {
            // Inform the user about the invalid input
            QMessageBox::warning(this, "Invalid Input", "Please enter a valid ip address.");
            return;
        }

        QString amode_port = ui->lineEdit_amodePort->text();
        bool ok; // Variable to check if the conversion was successful
        unsigned short amode_portushort = amode_port.toUShort(&ok);

        // check if string conversion to ushort is successful
        if(!ok)
        {
            // Inform the user about the invalid input
            QMessageBox::warning(this, "Invalid Input", "Please enter a valid port number (0-65535).");
            return;
        }

        // Change the text of the button
        ui->pushButton_amodeConnect->setText("Disconnect");
        // Disable the loadconfig button, so the user don't mess up the process
        ui->pushButton_amodeConfig->setEnabled(false);

        // Instantiate amodeconnection class
        myAmodeConnection = new AmodeConnection(nullptr, amode_ipstr, amode_port.toStdString());
        connect(myAmodeConnection, &AmodeConnection::dataReceived, this, &MainWindow::displayUSsignal);
        connect(myAmodeConnection, &AmodeConnection::errorOccured, this, &MainWindow::disconnectUSsignal);

        // Check if amode config already loaded. When myAmodeConfig is nullptr it means the config is not yet loaded.
        if (myAmodeConfig == nullptr)
        {
            // if not loaded yet, i need to initialize the combobox to select the probe
            ui->comboBox_amodeNumber->clear();
            for (int i=0; i<=myAmodeConnection->getNprobe(); i++)
            {
                std::string str_num = "Probe #" + std::to_string(i);
                ui->comboBox_amodeNumber->addItem(QString::fromStdString(str_num));
            }
        }

        // change the state of isAmodeStream to be false
        isAmodeStream = false;

        // [!] Because of performance issue when visualizing Bmode2d3d and Amode2d3d at the same time,
        // we need to stop one and and continue the other
        isBmode2d3dStream = false;
        on_pushButton_bmode2d3d_clicked();
        isBmode2d3dStream = true;
    }


    // If isAmodeStream is false, it means that the system is ready to be stopped.
    // Hence when clicked all connection to Amode Machine will be disconnected.
    else
    {
        // I add this condition, because if the user clicked the connect button for bmode2d3d, it will trigger
        // this function with isAmodeStream=false (which is this block of code). The problem is if the user never
        // click amodeConnect button before, this part will be executed and myAmodeConnection is still not initialized
        // yet. To prevent this to happen, i just put this condition below.
        if(myAmodeConnection == nullptr) return;

        // disconnect the slots
        disconnect(myAmodeConnection, &AmodeConnection::dataReceived, this, &MainWindow::displayUSsignal);
        disconnect(myAmodeConnection, &AmodeConnection::errorOccured, this, &MainWindow::disconnectUSsignal);
        // delete the amodeconnection object, and set the pointer to nullptr to prevent pointer dangling
        delete myAmodeConnection;        
        myAmodeConnection = nullptr;

        // set the isAmodeStream to be true again, means that the system is ready to stream again now
        isAmodeStream = true;
        // Change the text of the button
        ui->pushButton_amodeConnect->setText("Connect");
        // Enable again the button for loading the Amode config
        ui->pushButton_amodeConfig->setEnabled(true);

        // disconnectUSsignal();
    }
}


void MainWindow::disconnectUSsignal()
{
    ui->pushButton_amodeConnect->setText("Connect");
    myAmodeConnection = nullptr;
    isAmodeStream = true;

    // ui->pushButton_amodeConnect->setText("Connect");
    // disconnect(myAmodeConnection, &AmodeConnection::dataReceived, this, &MainWindow::displayUSsignal);
    // myAmodeConnection->deleteLater();
    // myAmodeConnection = nullptr;
    // delete myAmodeConnection;

    // disconnect(myAmodeConnection, &AmodeConnection::dataReceived, this, &MainWindow::displayUSsignal);
    // delete myAmodeConnection;
    // myAmodeConnection = nullptr;
}

void MainWindow::displayUSsignal(const std::vector<uint16_t> &usdata_uint16_)
{
    // Check if Amode config file is already loaded. Why matters? because i need to adjust the UI if the user load the config
    // When myAmodeConfig is nullptr it means the config is not yet loaded.
    if (myAmodeConfig == nullptr)
    {
        // convert data to int
        QVector<int16_t> usdata_qvint16_ (usdata_uint16_.begin(), usdata_uint16_.end());
        // select row
        QVector<int16_t> usdata_qvint16_rowsel = AmodeDataManipulator::getRow(usdata_qvint16_, ui->comboBox_amodeNumber->currentIndex(), myAmodeConnection->getNsample());
        // down sample for display purposes
        QVector<int16_t> usdata_qvint16_downsmp = AmodeDataManipulator::downsampleVector(usdata_qvint16_rowsel, downsample_nsample_);
        // skip the data if we got all zeros, usually because of the TCP connection
        if (usdata_qvint16_rowsel.size()==0) return;
        // convert to double
        QVector<double> usdata_qvdouble;
        std::transform(usdata_qvint16_downsmp.begin(), usdata_qvint16_downsmp.end(), std::back_inserter(usdata_qvdouble), [] (double value) { return static_cast<double>(value); });

        // create x-axis
        QVector<double> x(us_dvector_downsampled_.data(), us_dvector_downsampled_.data() + us_dvector_downsampled_.size());
        // draw the plot
        amodePlot->graph(0)->setData(x, usdata_qvdouble);
        amodePlot->replot();
    }

    // If the config file is already loaded, do almost similar thing but with several signal at once.
    else
    {
        // convert data to int
        QVector<int16_t> usdata_qvint16_ (usdata_uint16_.begin(), usdata_uint16_.end());
        // get the current selected group
        std::vector<AmodeConfig::Data> amode_group = myAmodeConfig->getDataByGroupName(ui->comboBox_amodeNumber->currentText().toStdString());
        // create x-axis
        QVector<double> x(us_dvector_downsampled_.data(), us_dvector_downsampled_.data() + us_dvector_downsampled_.size());

        // for every element in the selected group..
        // #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(amode_group.size()); i++)
        {
            // select row
            QVector<int16_t> usdata_qvint16_rowsel = AmodeDataManipulator::getRow(usdata_qvint16_, amode_group.at(i).number-1, myAmodeConnection->getNsample());
            // skip the data if we got all zeros, usually because of the TCP connection
            if (usdata_qvint16_rowsel.size()==0) continue;
            // down sample for display purposes
            QVector<int16_t> usdata_qvint16_downsmp = AmodeDataManipulator::downsampleVector(usdata_qvint16_rowsel, downsample_nsample_);
            // convert to double
            QVector<double> usdata_qvdouble;
            std::transform(usdata_qvint16_downsmp.begin(), usdata_qvint16_downsmp.end(), std::back_inserter(usdata_qvdouble), [] (double value) { return static_cast<double>(value); });

            // plot the data
            amodePlots.at(i)->graph(0)->setData(x, usdata_qvdouble);
            amodePlots.at(i)->replot();

            // // Use QMetaObject::invokeMethod to safely update GUI in the main thread
            // QMetaObject::invokeMethod(this, [=]() {
            //         amodePlots.at(i)->graph(0)->setData(x, usdata_qvdouble);
            //         amodePlots.at(i)->replot();
            //     }, Qt::QueuedConnection);
        }
    }
}





/* *****************************************************************************************
 * *****************************************************************************************
 *
 * Everything that is related to A-mode configuration
 *
 * *****************************************************************************************
 * ***************************************************************************************** */

void MainWindow::on_pushButton_amodeConfig_clicked()
{
    // open file dialog to only search for csv file
    QString filter = "CSV file (*.csv)";
    QString fileName = QFileDialog::getOpenFileName(nullptr, "Open CSV file", "D:/", filter);

    // Check if the user select the file
    if (fileName.isEmpty()) return;

    // Set the file edit
    ui->lineEdit_amodeConfig->setText(fileName);

    // Instantiate AmodeConfig object that will handle the reading of the config file
    myAmodeConfig = new AmodeConfig(fileName.toStdString());
    // get all the group names, so that later we can show the amode signal based on group we selected
    std::vector<std::string> amode_groupnames = myAmodeConfig->getAllGroupNames();

    // add the group names to the combobox
    ui->comboBox_amodeNumber->clear();
    for (std::size_t i=0; i<amode_groupnames.size(); i++)
    {
        ui->comboBox_amodeNumber->addItem(QString::fromStdString(amode_groupnames.at(i)));
    }

    // call this function which will show the plot according to the group we selected
    on_comboBox_amodeNumber_textActivated(QString::fromStdString(amode_groupnames.at(0)));
}


void MainWindow::on_comboBox_amodeNumber_textActivated(const QString &arg1)
{
    // if the user didn't load a config file, don't arrange anything with the ui plot
    if (myAmodeConfig == nullptr) return;

    // delete whatever there is in the gridLayout_amodeSignals right now
    while (QLayoutItem* item = ui->gridLayout_amodeSignals->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
    // delete whatever there is inside the amodePlots
    amodePlots.clear();

    // get the a-mode groups
    std::vector<AmodeConfig::Data> amode_group = myAmodeConfig->getDataByGroupName(arg1.toStdString());

    for(std::size_t i = 0; i < amode_group.size(); ++i)
    {
        // create a string for the plot name
        std::string str_num = "amode_plot" + std::to_string(amode_group.at(i).number);

        // create a new QCustomPlot object
        QCustomPlotIntervalWindow *current_plot = new QCustomPlotIntervalWindow(this);
        current_plot->setObjectName(str_num);
        current_plot->setInitialSpacing(3);
        current_plot->xAxis->setLabel("Depth (mm)");
        current_plot->yAxis->setLabel("Amplitude");
        current_plot->xAxis->setRange(0, us_dvector_downsampled_.coeff(us_dvector_downsampled_.size() - 1));
        current_plot->yAxis->setRange(-500, 7500);
        current_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // get the current window data from the current amode, if the window is already set for this
        // current amode, let's draw the inital lines.
        AmodeConfig::Window currentwindow = myAmodeConfig->getWindowByNumber(amode_group.at(i).number);
        // if the currentwindow.middle is 0.0 it means that the window is not set yet,
        // the condition i wrote below is a proper way to compare double
        if(currentwindow.isset)
        {
            std::array<std::optional<double>, 3> window;
            window[0] = currentwindow.lowerbound;
            window[1] = currentwindow.middle;
            window[2] = currentwindow.upperbound;
            current_plot->setInitialLines(window);
        }

        // store the plot to our vector, collection of plots
        amodePlots.push_back(current_plot);

        // this part is just for visualization organization, i want the organization is a bit more automatic
        // so that i could generate 00,01,10,11 according how many amode i want to plot
        short bit1 = (i >> 1) & 1;
        short bit2 = i & 1;
        if (amode_group.size()==1)
        {
            ui->gridLayout_amodeSignals->addWidget(current_plot, 0,0, 2,2);
        }
        if (amode_group.size()==2)
        {
            // ui->gridLayout_amodeSignals->addWidget(current_plot, bit1, bit2, 2,1);
            ui->gridLayout_amodeSignals->addWidget(current_plot, bit2, bit1, 1,2);
        }
        if (amode_group.size()==3 || amode_group.size()==4)
        {
            ui->gridLayout_amodeSignals->addWidget(current_plot, bit1, bit2, 1,1);
        }
    }

    // I want to make this pushbutton, when changed, also change the 3d amode visualization
    if(myVolumeAmodeController == nullptr) return;

    // I already implemented a function inside myVolumeAmodeController to set a new amodegroupdata
    // however, when i set it from here, it crashed, i don't know why. i am tired of this shit.
    // myVolumeAmodeController->setAmodeGroupData(amode_group)

    // So, what i do? just delete the object...
    disconnect(myMocapConnection, &MocapConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onRigidBodyReceived);
    disconnect(myAmodeConnection, &AmodeConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onAmodeSignalReceived);
    delete myVolumeAmodeController;
    myVolumeAmodeController = nullptr;

    // ...then reinitialize again. It's working. I don't care it is ugly. Bye.
    myVolumeAmodeController = new VolumeAmodeController(nullptr, scatter, amode_group);
    connect(myMocapConnection, &MocapConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onRigidBodyReceived);
    connect(myAmodeConnection, &AmodeConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onAmodeSignalReceived);
    // don't forget to set which holder should be visualized
    myVolumeAmodeController->setActiveHolder(arg1.toStdString());
}


void MainWindow::on_pushButton_amodeWindow_clicked()
{
    if(myAmodeConfig==nullptr)
    {
        // Inform the user about the invalid input
        QMessageBox::warning(this, "Cannot save window", "You need to open the amode configuration file first");
        return;
    }

    // get the a-mode groups
    std::vector<AmodeConfig::Data> amode_group = myAmodeConfig->getDataByGroupName(ui->comboBox_amodeNumber->currentText().toStdString());

    // loop for all member of groups
    for(std::size_t i = 0; i < amode_group.size(); ++i)
    {
        // get the current Config Data
        AmodeConfig::Data current_data = amode_group.at(i);

        // get the current amodePlots object and get the positions of the line
        QCustomPlotIntervalWindow *current_plot = amodePlots.at(i);
        auto positions = current_plot->getLinePositions();

        if(!positions[1].has_value())
        {
            QMessageBox::warning(this, "Saving cancelled", "There is one or more window that is yet to be set. Cancelling the saving.");
            return;
        }

        // set the window
        myAmodeConfig->setWindowByNumber(current_data.number, positions);
    }

    // save here
    if(myAmodeConfig->exportWindow())
        QMessageBox::information(this, "Saving success", "Window configuration is successfuly saved");
    else
        QMessageBox::information(this, "Saving failed", "There is something wrong when saving the window configuration file");


}


/* *****************************************************************************************
 * *****************************************************************************************
 *
 * Everything that is related to A-mode 3D visualization
 *
 * *****************************************************************************************
 * ***************************************************************************************** */


void MainWindow::on_comboBox_volume3DSignalMode_currentIndexChanged(int index)
{
    // check wheter A-mode config already initialized
    if (myVolumeAmodeController==nullptr) return;

    // if yes, now you can change the 3d signal display mode
    myVolumeAmodeController->setSignalDisplayMode(index);
}


void MainWindow::on_checkBox_volumeShow3DSignal_clicked(bool checked)
{
    // if the checkbox is now true, let's initialize the amode 3d visualization
    if(checked)
    {
        if(myMocapConnection==nullptr || myAmodeConnection==nullptr)
        {
            QMessageBox::warning(this, "Can't show signal", "To show 3D signal, please connect both amode ultrasound system and motion capture system.");
            ui->checkBox_volumeShow3DSignal->setCheckState(Qt::Unchecked);
            return;
        }

        if(myAmodeConfig==nullptr)
        {
            QMessageBox::warning(this, "Can't show signal", "To show 3D signal, please load the amode configuration file first.");
            ui->checkBox_volumeShow3DSignal->setCheckState(Qt::Unchecked);
            return;
        }

        // enable changing the state of combo box for variation display mode for amode 3d signal
        ui->comboBox_volume3DSignalMode->setEnabled(true);

        // get the a-mode groups
        std::vector<AmodeConfig::Data> amode_group = myAmodeConfig->getDataByGroupName(ui->comboBox_amodeNumber->currentText().toStdString());

        // instantiate myVolumeAmodeController
        // for note: i declare intentionally the argument for amode_group as value not the reference (a pointer to amode_group)
        // because amode_group here declared locally, so the reference will be gone outside of this scope.
        myVolumeAmodeController = new VolumeAmodeController(nullptr, scatter, amode_group);
        myVolumeAmodeController->setSignalDisplayMode(ui->comboBox_volume3DSignalMode->currentIndex());
        myVolumeAmodeController->setActiveHolder(ui->comboBox_amodeNumber->currentText().toStdString());

        // connect necessary slots
        connect(myMocapConnection, &MocapConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onRigidBodyReceived);
        connect(myAmodeConnection, &AmodeConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onAmodeSignalReceived);
    }

    // if the checkbox is now false, let's disconnect the signal to the class and delete the class
    else
    {
        // I add this condition, because if the user clicked the connect button for bmode2d3d, it will trigger
        // this function with arg1=false (which is this block of code). The problem is if the user never
        // click show3Dsignal checkboxbefore, this part will be executed and myMocapConnection, myAmodeConnection
        // and myVolumeAmodeController are still not initialized yet. To prevent this to happen, i just put this condition below.

        if(myMocapConnection != nullptr && myVolumeAmodeController != nullptr)
            disconnect(myMocapConnection, &MocapConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onRigidBodyReceived);

        if(myAmodeConnection != nullptr && myVolumeAmodeController != nullptr)
            disconnect(myAmodeConnection, &AmodeConnection::dataReceived, myVolumeAmodeController, &VolumeAmodeController::onAmodeSignalReceived);

        delete myVolumeAmodeController;
        myVolumeAmodeController = nullptr;

        // enable changing the state of combo box for variation display mode for amode 3d signal
        ui->comboBox_volume3DSignalMode->setEnabled(false);
    }
}




