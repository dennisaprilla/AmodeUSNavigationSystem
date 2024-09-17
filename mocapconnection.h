#ifndef MOCAPCONNECTION_H
#define MOCAPCONNECTION_H

#include "qualisystransformationmanager.h"
#include <QObject>

/**
 * @class MocapConnection
 * @brief An abstraction class for Qualisys Connection and Vicon Connection
 *
 * For the context. I was building the software with Qualisys system in mind. However, in the later stage,
 * we need to migrate to Vicon system. So what i did, i just make another class, called ViconConnection that
 * will handle the connection to Vicon system. However, it introduce a problem (but not really actually).
 * Because i will have two object instantiated from two different class, every time there is a line that involve
 * connection to the motion capture system, i need to make a condition, based on user selection, what class should
 * I use. I don't find it pretty, so i make an abstraction class, MocapConnection. This class will give me more
 * flexibility on which connection i use.
 *
 * MocapConnection is the parent class for QualisysConnection and ViconConneciton. It has startStreaming() function which
 * starts the streaming to the motion capture system and dataRecieved() signal which emits the transformation values.
 */

class MocapConnection: public QObject {
    Q_OBJECT

public:

    /**
     * @brief Constructor
     */
    explicit MocapConnection(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief A virtual function such that startStreaming() can be used accross two child class ViconConnection and QualisysConnection
     */
    virtual void startStreaming() = 0;

signals:
    /**
     * @brief Emits a tmanager, which consists of transformations of rigid body that is detected by Qualisys
     */
    void dataReceived(const QualisysTransformationManager &tmanager);

};

#endif // MOCAPCONNECTION_H
