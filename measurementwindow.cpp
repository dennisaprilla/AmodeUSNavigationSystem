#include "measurementwindow.h"
#include "ui_measurementwindow.h"

#include <QFileDialog>
#include <QMessageBox>

MeasurementWindow::MeasurementWindow(AmodeConnection *amodeConnection, MocapConnection *mocapConnection, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MeasurementWindow)
    , myAmodeConnection(amodeConnection)
    , myMocapConnection(mocapConnection)
{
    ui->setupUi(this);

    // Set the window flags to ensure the second window stays on top
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    // Check if myAmodeConnection and myMocapConnection had been initialized
    if (myAmodeConnection != nullptr)
    {
        ui->label_statusAmode->setText("Connected");
        ui->label_statusAmode->setStyleSheet("QLabel { color: green; background-color: rgb(200, 255, 200); }");
    }
    if (myMocapConnection != nullptr)
    {
        ui->label_statusMocap->setText("Connected");
        ui->label_statusMocap->setStyleSheet("QLabel { color : green; background-color: rgb(200, 255, 200); }");
    }
}

MeasurementWindow::~MeasurementWindow()
{
    delete ui;
}

void MeasurementWindow::on_amodeConnected(AmodeConnection *amodeConnection)
{
    myAmodeConnection = amodeConnection;
    ui->label_statusAmode->setText("Connected");
    ui->label_statusAmode->setStyleSheet("QLabel { color: green; background-color: rgb(200, 255, 200); }");
}

void MeasurementWindow::on_amodeDisconnected()
{
    myAmodeConnection = nullptr;
    ui->label_statusAmode->setText("Not Connected");
    ui->label_statusAmode->setStyleSheet("QLabel { color: green; background-color: rgb(255, 200, 200); }");
}

void MeasurementWindow::on_mocapConnected(MocapConnection *mocapConnection)
{
    myMocapConnection = mocapConnection;
    ui->label_statusMocap->setText("Connected");
    ui->label_statusMocap->setStyleSheet("QLabel { color : green; background-color: rgb(200, 255, 200); }");
}

/*
void MeasurementWindow::on_mocapDisonnected()
{
    myMocapConnection = nullptr;
}
*/


void MeasurementWindow::on_pushButton_recordPath_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "D:\\", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // Check if the user selected a file
    if (!folderPath.isEmpty()) {
        // Set the file path in the QLineEdit
        ui->lineEdit_recordPath->setText(folderPath + "/");
    }
}


void MeasurementWindow::on_pushButton_recordButton_clicked()
{
    if (ui->lineEdit_recordPath->text().isEmpty())
    {
        // Inform the user about the invalid input
        QMessageBox::warning(this, "Empty Directory", "Please select the recoding directory first before conducting the recording");
        return;
    }


    if (myAmodeConnection==nullptr && myMocapConnection == nullptr)
    {
        QMessageBox::warning(this, "No Connection", "Connect both Mocap System and A-mode Ultrasound machine first");
        return;
    }

    if (myAmodeConnection==nullptr)
    {
        QMessageBox::warning(this, "No Connection", "A-mode Ultrasound machine is still yet to be connected");
        return;
    }

    if (myMocapConnection==nullptr)
    {
        QMessageBox::warning(this, "No Connection", "Motion Capture system is still yet to be connected");
        return;
    }

    // If the state is not recording, let's start recording
    if(!isRecord)
    {
        // instantiate the AmodeMocapRecorder class
        myAmodeMocapRecorder = new AmodeMocapRecorder(nullptr);
        myAmodeMocapRecorder->setFilePath(ui->lineEdit_recordPath->text());

        // connect signals to AmodeMocapRecorder slots
        connect(myAmodeConnection, &AmodeConnection::dataReceived, myAmodeMocapRecorder, &AmodeMocapRecorder::onAmodeSignalReceived);
        connect(myMocapConnection, &MocapConnection::dataReceived, myAmodeMocapRecorder, &AmodeMocapRecorder::onRigidBodyReceived);

        // start recording
        myAmodeMocapRecorder->startRecording();

        // change the button text
        ui->pushButton_recordButton->setText("Stop");
        ui->pushButton_recordButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop));
        // reset the flag
        isRecord = true;
    }

    // If the state is recording, let's stop it
    else
    {

        // stop the recording
        myAmodeMocapRecorder->stopRecording();

        // disconnect the slots
        disconnect(myAmodeConnection, &AmodeConnection::dataReceived, myAmodeMocapRecorder, &AmodeMocapRecorder::onAmodeSignalReceived);
        disconnect(myMocapConnection, &MocapConnection::dataReceived, myAmodeMocapRecorder, &AmodeMocapRecorder::onRigidBodyReceived);

        // change the button text
        ui->pushButton_recordButton->setText("Record");
        ui->pushButton_recordButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaRecord));
        // reset the flag
        isRecord = false;
    }

}

