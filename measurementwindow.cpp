#include "measurementwindow.h"
#include "qregularexpression.h"
#include "ui_measurementwindow.h"

#include <QFileDialog>
#include <QMessageBox>

MeasurementWindow::MeasurementWindow(AmodeConnection *amodeConnection, MocapConnection *mocapConnection, bool isIntermRec, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MeasurementWindow)
    , myAmodeConnection(amodeConnection)
    , myMocapConnection(mocapConnection)
    , isIntermediateRecording(isIntermRec)
    , record_parentpath_("")
    , record_currentpath_("")
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
    if (isIntermRec)
    {
        ui->label_statusIntermRec->setText("Active");
        ui->label_statusIntermRec->setStyleSheet("QLabel { color : green; background-color: rgb(200, 255, 200); }");
    }
}

MeasurementWindow::~MeasurementWindow()
{
    delete ui;
}

void MeasurementWindow::setRecordParentPath(const QString &path)
{
    // Set the parent path
    record_parentpath_ = path;
    // create numbered folder in the parent path
    updateCurrentRecordPath();
}

void MeasurementWindow::updateCurrentRecordPath()
{
    // create a numbered folder for each of the recording
    QString foldernumber = createNumberedFolder(record_parentpath_);

    // If the string is empty, there is something wrong when creating the numbered folder
    if (foldernumber.isEmpty())
    {
        qDebug() << "MeasurementWindow::setRecordParentPath Something wrong when creating a new folder. Use the path_measurement_ directory instead.";
        record_currentpath_ = record_parentpath_;
    }
    else
    {
        // If everyting is good, use this path for the next recording
        record_currentpath_ = record_parentpath_ + "/" + foldernumber;
    }

    ui->lineEdit_recordPath->setText(record_currentpath_+"/");
    qDebug() << "MeasurementWindow::setRecordParentPath() current record path is: " << record_currentpath_;
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
    ui->label_statusAmode->setStyleSheet("QLabel { color: red; background-color: rgb(255, 200, 200); }");
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

void MeasurementWindow::on_amodeTimedRecordingStarted()
{
    isIntermediateRecording = true;
    ui->label_statusIntermRec->setText("Active");
    ui->label_statusIntermRec->setStyleSheet("QLabel { color : green; background-color: rgb(200, 255, 200); }");
}

void MeasurementWindow::on_amodeTimedRecordingStopped()
{
    isIntermediateRecording = false;
    ui->label_statusIntermRec->setText("Idle");
    ui->label_statusIntermRec->setStyleSheet("QLabel { color : rgb(100, 100, 100); background-color : rgb(200, 200, 200); }");
}


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
    if(!isMeasurementRecording)
    {
        // instantiate the AmodeMocapRecorder class
        myAmodeMocapRecorder = new AmodeMocapRecorder(nullptr);
        myAmodeMocapRecorder->setFilePath(ui->lineEdit_recordPath->text());

        // connect signals to AmodeMocapRecorder slots
        connect(myAmodeConnection, &AmodeConnection::dataReceived, myAmodeMocapRecorder, &AmodeMocapRecorder::onAmodeSignalReceived);
        connect(myMocapConnection, &MocapConnection::dataReceived, myAmodeMocapRecorder, &AmodeMocapRecorder::onRigidBodyReceived);

        // start recording
        myAmodeMocapRecorder->startRecording();

        // if the myAmodeTimedRecorder still recording, stop it
        if(isIntermediateRecording)
        {
            emit request_stop_amodeTimedRecording();
            ui->label_statusIntermRec->setText("Idle");
            ui->label_statusIntermRec->setStyleSheet("QLabel { color : rgb(100, 100, 100); background-color : rgb(200, 200, 200); }");
            isIntermediateRecording = false;
        }

        // change the button text
        ui->pushButton_recordButton->setText("Stop");
        ui->pushButton_recordButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop));
        // reset the flag
        isMeasurementRecording = true;
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

        // give information to user that it finished recording
        QMessageBox::information(this, "Finsihed recording", "Recording is finished. Check measurement folder.");
        // reset the flag
        isMeasurementRecording = false;


        // create a numbered folder for the next recording
        updateCurrentRecordPath();
        // signals the mainwindow that it can start the timed recording (if later the user confirm it)
        emit request_start_amodeTimedRecording();

    }

}

QString MeasurementWindow::createNumberedFolder(const QString& basePath) {
    // Check if the base path exists
    QDir dir(basePath);
    if (!dir.exists()) {
        qWarning() << "MeasurementWindow::createNumberedFolder() Base path does not exist:" << basePath;
        return QString();
    }

    // Regular expression to match folder names containing numbers
    QRegularExpression regex(R"((\d+))");
    int maxNum = -1; // Start with -1 so the first folder will be 0 (0000)

    // Iterate through the existing folders in the directory
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QRegularExpressionMatch match = regex.match(entry.fileName());
        if (match.hasMatch()) {
            int currentNum = match.captured(1).toInt();
            if (currentNum > maxNum) {
                maxNum = currentNum;
            }
        }
    }

    // Generate the next folder name with leading zeros (4 digits)
    int nextNum = maxNum + 1; // Start from 0 if no folders exist
    QString newFolderName = QString("%1").arg(nextNum, 4, 10, QChar('0'));

    // Create the new folder
    if (!dir.mkdir(newFolderName)) {
        qWarning() << "MeasurementWindow::createNumberedFolder() Failed to create folder:" << newFolderName;
        return QString();
    }

    qDebug() << "MeasurementWindow::createNumberedFolder() Created folder:" << newFolderName;
    return newFolderName;
}

