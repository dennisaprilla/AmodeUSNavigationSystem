#ifndef AMODETIMEDRECORDER_H
#define AMODETIMEDRECORDER_H

#include <QObject>
#include <QTimer>
#include <QtConcurrent>
#include <vector>
#include <opencv2/opencv.hpp>

class AmodeTimedRecorder : public QObject
{
    Q_OBJECT
public:
    explicit AmodeTimedRecorder(QObject *parent = nullptr);
    ~AmodeTimedRecorder();

    void setFilePostfix(const QString &filePostfix);
    void setFilePath(const QString &filePath);
    void setRecordTimer(int ms);
    void startRecording();
    void stopRecording();

public slots:
    void onAmodeSignalReceived(const std::vector<uint16_t> &data);

private slots:
    void processData();

private:
    QString m_filePath;
    QString m_filePostfix;
    int m_timerms;

    QTimer *timer;
    std::vector<uint16_t> currentData;
    bool isRecording;

signals:
    void amodeTimedRecordingStopped();
};

#endif // AMODETIMEDRECORDER_H
