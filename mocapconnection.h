#ifndef MOCAPCONNECTION_H
#define MOCAPCONNECTION_H

#include "qualisystransformationmanager.h"
#include <QObject>

class MocapConnection: public QObject {
    Q_OBJECT

public:
    explicit MocapConnection(QObject *parent = nullptr) : QObject(parent) {}

    // Pure virtual function
    virtual void startStreaming() = 0;

signals:
    /**
     * @brief Emits a tmanager, which consists of transformations of rigid body that is detected by Qualisys
     */
    void dataReceived(const QualisysTransformationManager &tmanager);

};

#endif // MOCAPCONNECTION_H
