// Bmode3DVisualizer.cpp
// NOTE: 25/04/2024 I make the scale all in cm in the 3D visualization

#include "Bmode3DVisualizer.h"

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <Qt3DExtras/QTextureMaterial>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QForwardRenderer.h>
#include <Qt3DExtras/QTorusMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QConeMesh>

#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QCamera>

#include <Qt3DRender/QpointLight.h>
#include <Qt3DRender/QDirectionalLight.h>
#include <Qt3DExtras/QFirstPersonCameraController>
#include <Qt3DExtras/QPhongMaterial>

#include <QImage>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPainter>

// library below is for loading an XML using RapidXML
#include <QFile>
#include <QTextStream>
#include <iostream>
#include "rapidxml.hpp"


Bmode3DVisualizer::Bmode3DVisualizer(QWidget *parent, QString calibconfig_path)
    : QWidget(parent)
{
    initializeScene();

    // Initialize probe base-transform. The probe model has its own pivot and initial orientation.
    // It is not always necessarily align with the transformation that is provided by mocap capture
    // For example, the mesh can be modeled upside down. This transformation is used for the
    // initial transformation of the probe model.
    probeBaseTransform = new Qt3DCore::QTransform();
    probeBaseTransform->setScale(VIZ_SCALE); // i put scale 0.1 because the mesh dimension is in mm, i want it to be visualized in dm
    probeBaseTransform->setRotation(QQuaternion::fromEulerAngles(0.0f, -90.0f, 90.0f));

    // get calibration matrix
    std::vector<double> calib_matrix = getImageToProbeTransformation(calibconfig_path);
    QMatrix3x3 calib_rotation = QMatrix3x3();
    QVector3D calib_translation = QVector3D(0, 0, 0);
    if (!calib_matrix.empty())
    {
        calib_rotation(0,0) = calib_matrix[0]; calib_rotation(0,1) = calib_matrix[1]; calib_rotation(0,2) = calib_matrix[2];
        calib_rotation(1,0) = calib_matrix[4]; calib_rotation(1,1) = calib_matrix[5]; calib_rotation(1,2) = calib_matrix[6];
        calib_rotation(2,0) = calib_matrix[8]; calib_rotation(2,1) = calib_matrix[9]; calib_rotation(2,2) = calib_matrix[10];
        calib_translation = QVector3D(calib_matrix[3]*VIZ_SCALE, calib_matrix[7]*VIZ_SCALE, calib_matrix[11]*VIZ_SCALE);
    }
    normalizeRotationMatrix(calib_rotation);

    // Similar to code above, this one if for the initialization of the image plane
    planeBaseTransform = new Qt3DCore::QTransform();
    // planeBaseTransform->setRotation(QQuaternion::fromEulerAngles(-90.0f, 180.0f, 0.0f));
    planeBaseTransform->setRotation(QQuaternion::fromEulerAngles(-90.0f, 0.0f, 0.0f));
    planeBaseTransform->setTranslation(QVector3D(0,0,0));

    // Similar to code above, this one if for the initialization of the image plane
    planeCalibTransform = new Qt3DCore::QTransform();
    planeCalibTransform->setRotation(QQuaternion::fromRotationMatrix(calib_rotation));
    planeCalibTransform->setTranslation(calib_translation);

}

std::vector<double> Bmode3DVisualizer::getImageToProbeTransformation(const QString& filename) {
    std::vector<double> matrix;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Failed to open file" << std::endl;
        return {};
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Convert QString to std::string once
    std::string contentStr = content.toStdString();

    // Create buffer without relying on c_str()
    std::vector<char> buffer(contentStr.begin(), contentStr.end());

    // Ensure null termination
    buffer.push_back('\0');

    rapidxml::xml_document<> doc;
    try {
        doc.parse<0>(&buffer[0]);
    } catch (const rapidxml::parse_error& e) {
        std::cerr << "XML Parse error: " << e.what() << std::endl;
        return matrix;  // Return default transformation
    }

    rapidxml::xml_node<>* root_node = doc.first_node("PlusConfiguration");
    if (root_node) {
        rapidxml::xml_node<>* coord_def_node = root_node->first_node("CoordinateDefinitions");
        if (coord_def_node) {
            for (rapidxml::xml_node<>* transform_node = coord_def_node->first_node("Transform");
                 transform_node;
                 transform_node = transform_node->next_sibling("Transform")) {

                rapidxml::xml_attribute<>* from_attr = transform_node->first_attribute("From");
                rapidxml::xml_attribute<>* to_attr = transform_node->first_attribute("To");

                if (from_attr && to_attr &&
                    std::string(from_attr->value()) == "Image" &&
                    std::string(to_attr->value()) == "Probe") {

                    rapidxml::xml_attribute<>* matrix_attr = transform_node->first_attribute("Matrix");
                    if (matrix_attr) {
                        std::string matrix_str = matrix_attr->value();
                        std::istringstream iss(matrix_str);
                        double value;
                        while (iss >> value) {
                            matrix.push_back(value);
                        }
                        break;  // We found the matrix we're looking for, so we can stop searching
                    }
                }
            }
        }
    }

    return matrix;
}

void Bmode3DVisualizer::normalizeRotationMatrix(QMatrix3x3& calib_rotation)
{
    // Convert QMatrix3x3 to Eigen::Matrix3d
    Eigen::Matrix3d eigenMatrix;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            eigenMatrix(i, j) = calib_rotation(i, j);
        }
    }

    // Perform Singular Value Decomposition
    Eigen::JacobiSVD<Eigen::Matrix3d> svd(eigenMatrix, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3d U = svd.matrixU();
    Eigen::Matrix3d V = svd.matrixV();

    // Construct the normalized rotation matrix
    Eigen::Matrix3d normalizedMatrix = U * V.transpose();

    // Ensure the determinant is 1 (proper rotation matrix)
    if (normalizedMatrix.determinant() < 0) {
        normalizedMatrix = -normalizedMatrix;
    }

    // Convert back to QMatrix3x3
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            calib_rotation(i, j) = normalizedMatrix(i, j);
        }
    }
}

/*
Bmode3DVisualizer::Bmode3DVisualizer(QWidget *parent, BmodeConnection *bmodeConnection, QualisysConnection *qualisysConnection)
    : QWidget(parent) {

    // Signal and Slot Connection --------------
    // Connect the BmodeConnection imageProcessed signal to the Bmode3DVisualizer onImageReceived slot
    connect(bmodeConnection, &BmodeConnection::imageProcessed,
            this, &Bmode3DVisualizer::onImageReceived);

    // Connect the QualisysConnection dataReceived signal to the Bmode3DVisualizer onRigidBodyReceived slot
    connect(qualisysConnection, &QualisysConnection::dataReceived,
            this, &Bmode3DVisualizer::onRigidBodyReceived);

    initializeScene();
}
*/

void Bmode3DVisualizer::initializeScene()
{
    // QtWidget container for Qt3D --------------
    // Create View from Qt3DWindow
    qputenv("QSG_RHI_BACKEND", QByteArray("opengl"));
    view = new Qt3DExtras::Qt3DWindow();
    view->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
    // I don't want them to be separate window, so i will make a container
    QWidget *container = QWidget::createWindowContainer(view);
    // Make a vertical layout and add the container to this layout
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(container);
    // Set the layout
    setLayout(layout);

    // Qt3D setup and cameras --------------
    // Initialize the root entity for Qt3DWindow
    rootEntity = new Qt3DCore::QEntity();

    // Camera Entity
    Qt3DRender::QCamera *cameraEntity = view->camera();
    cameraEntity->setPosition(QVector3D(10.0f, 10.0f, 10.0f));
    cameraEntity->setViewCenter(QVector3D(0, 0, 0));
    cameraEntity->setUpVector(QVector3D(0, 1, 0));
    cameraEntity->setNearPlane(0.1f);
    cameraEntity->setFarPlane(1000.0f);
    cameraEntity->setFieldOfView(25.0f);
    Qt3DExtras::QOrbitCameraController *camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setLinearSpeed(20.0);
    camController->setCamera(cameraEntity);

    // Light Entity
    // Qt3DRender::QPointLight *lightPoint = new Qt3DRender::QPointLight();
    // lightPoint->setColor("white");
    // lightPoint->setIntensity(1);
    Qt3DRender::QDirectionalLight *directionalLight1 = new Qt3DRender::QDirectionalLight();
    directionalLight1->setColor("white");
    directionalLight1->setIntensity(1.0f);
    directionalLight1->setWorldDirection(QVector3D(-1, -1, -1)); // Set the direction of the light
    Qt3DRender::QDirectionalLight *directionalLight2 = new Qt3DRender::QDirectionalLight();
    directionalLight2->setColor("white");
    directionalLight2->setIntensity(1.0f);
    directionalLight2->setWorldDirection(QVector3D(-1, -1, 1)); // Set the direction of the light
    Qt3DCore::QTransform *lightTransform = new Qt3DCore::QTransform();
    lightTransform->setTranslation(cameraEntity->position());
    Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(rootEntity);
    lightEntity->addComponent(directionalLight1);
    lightEntity->addComponent(directionalLight2);
    lightEntity->addComponent(lightTransform);

    // Origin axis entity (X,Y,Z), (red, green, blue)
    // createAxis(rootEntity, QVector3D(0, 0, 0), QVector3D(1, 0, 0), Qt::red);
    // createAxis(rootEntity, QVector3D(0, 0, 0), QVector3D(0, 1, 0), Qt::green);
    // createAxis(rootEntity, QVector3D(0, 0, 0), QVector3D(0, 0, 1), Qt::blue);
    Qt3DCore::QEntity *axisOriginEntity = new Qt3DCore::QEntity(rootEntity);
    createAxisVector(axisOriginEntity);

    // Ground Mesh Entity
    Qt3DExtras::QPlaneMesh *groundMesh = new Qt3DExtras::QPlaneMesh();
    groundMesh->setWidth(10);
    groundMesh->setHeight(10);
    Qt3DCore::QTransform *groundTransform = new Qt3DCore::QTransform();
    // groundTransform->setRotation(QQuaternion::fromEulerAngles(90.0f, 0.0f, 0.0f));
    // groundTransform->setTranslation(QVector3D(0.1f, 0.0f, 0.0f));
    Qt3DExtras::QPhongMaterial *groundMaterial  = new Qt3DExtras::QPhongMaterial();
    groundMaterial->setDiffuse(QColor(QRgb(0x3498db)));
    Qt3DCore::QEntity *groundEntity = new Qt3DCore::QEntity(rootEntity);
    groundEntity->addComponent(groundMesh);
    groundEntity->addComponent(groundTransform);
    groundEntity->addComponent(groundMaterial);

    // Set root entity
    view->setRootEntity(rootEntity);
}

void Bmode3DVisualizer::onImageReceived(const cv::Mat &image) {
    currentImage = image;
    imageReady = true;
    if (rigidbodyReady) {
        visualizeImage();
    }
}

void Bmode3DVisualizer::onRigidBodyReceived(const QualisysTransformationManager &tmanager) {
    currentTransform = tmanager.getTransformationById("B_PROBE");
    rigidbodyReady = true;
    if (imageReady) {
        visualizeImage();
    }
}

void Bmode3DVisualizer::visualizeImage() {
    // Reset flags
    imageReady = false;
    rigidbodyReady = false;

    if (firstData)
    {
        // Convert the OpenCV image to a QImage
        QImage qtImage = cvMatToQImage(currentImage);
        // Convert Eigen Matrix to QMatrix and store it in QTransform
        currentQTransform = new Qt3DCore::QTransform();
        currentQTransform->setMatrix(eigenToQMatrix(currentTransform));

        // Creating Mesh Entity for B-mode probe model in 3D space
        // 1) Probe Mesh
        probeMesh = new Qt3DRender::QMesh();
        probeMesh->setSource(QUrl::fromLocalFile("C:/UsProbe_Telemed_L12.stl"));
        // 2) Probe Entity
        probeMaterial = new Qt3DExtras::QPhongMaterial();
        probeMaterial->setDiffuse(QColor(Qt::white));
        // 3) Probe Transformation
        probeTransform = new Qt3DCore::QTransform();
        probeTransform->setMatrix(currentQTransform->matrix()*probeBaseTransform->matrix());
        // 4) Probe Entity
        probeEntity = new Qt3DCore::QEntity(rootEntity);
        // probeEntity->addComponent(probeMesh);
        probeEntity->addComponent(probeMaterial);
        probeEntity->addComponent(probeTransform);

        // Creating Axis entity (to understand the transformation better in visualization)sss
        axisProbeEntity = new Qt3DCore::QEntity(rootEntity);
        createAxisVector(axisProbeEntity);
        axisProbeEntity->addComponent(currentQTransform);

        // Creating Plane Entity for B-mode image in 3D space
        // 1) Plane Mesh
        planeMesh = new Qt3DExtras::QPlaneMesh();
        planeMesh->setWidth(VIZ_SCALE*(4.0/90.0)*currentImage.cols);  // i put scale VIZ_SCALE because pix2mm (4.0/90.0) is in mm, i want it to be visualized in VIZ_SCALE
        planeMesh->setHeight(VIZ_SCALE*(4.0/90.0)*currentImage.rows); // same here
        // 2) Plane Texture (it is a but workaround, i want texture from QImage instead of external image file)
        // 2.a) Create an instance of PaintedTextureImage and set the QImage
        paintedTextureImage = new PaintedTextureImage();
        paintedTextureImage->setImage(qtImage);
        // 2.b) Create a QTexture2D instance and add the PaintedTextureImage
        planeTexture2D = new Qt3DRender::QTexture2D();
        planeTexture2D->addTextureImage(paintedTextureImage);
        // 2.c) Create a texture material and set the QTexture2D as its texture
        planeTextureMaterial = new Qt3DExtras::QTextureMaterial();
        planeTextureMaterial->setTexture(planeTexture2D);
        // 3) Plane Transformation
        // 3.a) Plane base Transformation (additional translation)
        planeBaseTransform->setTranslation(QVector3D(planeMesh->width()/2, planeMesh->height()/2, 0.0f));
        // 3.b) Plane current Transformation
        planeTransform = new Qt3DCore::QTransform();
        planeTransform->setMatrix(currentQTransform->matrix()*planeCalibTransform->matrix()*planeBaseTransform->matrix());

        // Creating Axis entity (to understand the transformation better in visualization)sss
        axisImageEntity = new Qt3DCore::QEntity(rootEntity);
        createAxisVector(axisImageEntity);
        planeOriginTransform = new Qt3DCore::QTransform();
        planeOriginTransform->setMatrix(currentQTransform->matrix()*planeCalibTransform->matrix());
        axisImageEntity->addComponent(planeOriginTransform);

        // 4) Plane Entity
        // 4.a) The image plane itself
        imageEntity = new Qt3DCore::QEntity(rootEntity);
        imageEntity->addComponent(planeMesh);
        imageEntity->addComponent(planeTransform);
        imageEntity->addComponent(planeTextureMaterial);

        firstData = false;
    }

    else
    {
        // Convert the OpenCV image to a QImagete
        QImage qtImage = cvMatToQImage(currentImage);
        // Convert Eigen Matrix to QMatrix and store it in QTransform
        currentQTransform->setMatrix(eigenToQMatrix(currentTransform));

        // Probe Transform
        probeTransform->setMatrix(currentQTransform->matrix()*probeBaseTransform->matrix());

        // Plane Transform
        planeOriginTransform->setMatrix(currentQTransform->matrix()*planeCalibTransform->matrix());
        planeTransform->setMatrix(currentQTransform->matrix()*planeCalibTransform->matrix()*planeBaseTransform->matrix());

        // Plane texture, the pipeline following the above
        paintedTextureImage->setImage(qtImage);
        planeTexture2D->addTextureImage(paintedTextureImage);
        planeTextureMaterial->setTexture(planeTexture2D);

    }
}

// Function to convert cv::Mat to QImage
QImage Bmode3DVisualizer::cvMatToQImage(const cv::Mat &mat) {

    switch (mat.type()) {

    // 8-bit, 4 channel
    case CV_8UC4: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        // Use copy to detach from original data, to ensure that the QImage is a deep copy
        // and does not depend on the original cv::Mat data
        return image.copy();
    }

    // 8-bit, 3 channel
    case CV_8UC3: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        // Swap RGB to BGR and detach because OpenCV uses BGR format by default, while QImage expects RGB.
        return image.rgbSwapped().copy();
    }

    // 8-bit, 1 channel
    case CV_8UC1: {
        QVector<QRgb> colorTable;
        for (int i = 0; i < 256; i++)
            colorTable.push_back(qRgb(i, i, i));
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Indexed8);
        image.setColorTable(colorTable);
        return image.copy(); // Use copy to detach from original data
    }

    default:
        qWarning("cvMatToQImage() - cv::Mat image type not handled in switch:");
        return QImage();
    }

}

// Function to convert Eigen::Isometry3d to QMatrix4x4
QMatrix4x4 Bmode3DVisualizer::eigenToQMatrix(const Eigen::Isometry3d &eigen_mat) {

    // These process below is the adjustment of the coordinate system. This is needed
    // because Qualisys has different rules of 3D visualization compared Qt3DCore:
    // a) Qualisys has a right hand rule rotation, Qt3Dcore left hand rule rotation
    // b) Qualisys has XYZ order of rotation, Qt3Dcore has ZYX order of rotation.

    // 1) Transformation from qualisys has XYZ order, so let's decompose it first in XYZ manner
    Eigen::Vector3d euler_angles = eigen_mat.rotation().eulerAngles(0, 1, 2);
    // Eigen::Vector3d euler_tmp = euler_angles * (180.0 / M_PI);
    // std::cout << "Euler angles: " << euler_tmp.transpose() << std::endl;

    // 2) Transformation from qualisys has different hand rule, let's negate them
    euler_angles = euler_angles * -1;

    // 3) Transformation in Qt has ZYX order, so let's combine them in ZYX manner
    Eigen::Matrix3d rotationMatrix;
    rotationMatrix =
        Eigen::AngleAxisd(euler_angles(2), Eigen::Vector3d::UnitZ())
        * Eigen::AngleAxisd(euler_angles(1), Eigen::Vector3d::UnitY())
        * Eigen::AngleAxisd(euler_angles(0), Eigen::Vector3d::UnitX());

    // 4) For Qt visualization requirement, convert our Eigen::Matrix3d to QMatrix3x3
    Eigen::Matrix3f R = rotationMatrix.cast<float>();
    // We need to transpose it because Eigen uses row-major order while Qt uses column-major order
    QMatrix3x3 qt_R = QMatrix3x3(R.data()).transposed();

    // 5) The same for translation, convert it into QVector3D
    Eigen::Vector3d t = eigen_mat.translation();
    QVector3D qt_t(t.x(), t.y(), t.z());

    // 6) Pack everything up and construct the QMatrix4x4
    QMatrix4x4 qt_T(qt_R);
    // Let's convert the translation in VIZ_SCALE for visualization consistency, by default it is in decimeters
    qt_T.setColumn(3, QVector4D(VIZ_SCALE*qt_t, 1.0f));

    return qt_T;


    /* This block is my attempt to do the same thing as the current working code but in quaternion without
     * involving euler angles conversion. It was because I afraid that gimbal lock will screw the visualization.
     *
    // swap the order of rotation and invert the rotation
    Eigen::Quaterniond q_xyz(eigen_mat.rotation());
    Eigen::Quaterniond q_zyx_inverted = swapQuaternionOrderXYZtoZYX(q_xyz, false);

    // convert quaternion rotation to matrix rotation
    Eigen::Matrix3f R = q_zyx_inverted.toRotationMatrix().cast<float>();
    // transpose Eigen::Matrix3f data because Eigen uses row-major order while Qt uses column-major order
    QMatrix3x3 qt_R = QMatrix3x3(R.data()).transposed();

    // convert Eigen::Vector3d to QVector3D
    Eigen::Vector3d t = eigen_mat.translation();
    QVector3D qt_t(t.x(), t.y(), t.z());

    // construct the QMatrix4x4
    QMatrix4x4 qt_T(qt_R);
    qt_T.setColumn(3, QVector4D(VIZ_SCALE*qt_t, 1.0f));

    return qt_T;
    */

    /* This block is the original one, in which i was confuse why the visualisation in Qt is different
     * when i compare it with Qualisys interface
     *
    // Convert to Matrix4f
    Eigen::Matrix4f mat = eigen_mat.matrix().cast<float>();

    // Transpose to match row-major order because Eigen uses column-major order by default,
    // while Qt uses row-major order for its matrices.
    QMatrix4x4 currentQMat = QMatrix4x4(mat.data()).transposed();

    // Let's convert the translation in VIZ_SCALE, by default it is in decimeters
    QVector3D translation  = currentQMat.column(3).toVector3D();
    currentQMat.setColumn(3, QVector4D(VIZ_SCALE*translation, 1.0f));

    return currentQMat;
    */
}

// Function to swap rotation order but with quaternion instead of involving conversion to euler angles
Eigen::Quaterniond Bmode3DVisualizer::swapQuaternionOrderXYZtoZYX(const Eigen::Quaterniond& q, bool invert) {
    // Extract Z rotation
    double qz_w = std::sqrt(q.w() * q.w() + q.z() * q.z());
    Eigen::Quaterniond qz(qz_w, 0, 0, q.z() / qz_w);

    // Extract Y rotation
    Eigen::Quaterniond qy_temp = q * qz.inverse();
    double qy_w = std::sqrt(qy_temp.w() * qy_temp.w() + qy_temp.y() * qy_temp.y());
    Eigen::Quaterniond qy(qy_w, 0, qy_temp.y() / qy_w, 0);

    // Extract X rotation
    Eigen::Quaterniond qx = qy.inverse() * qy_temp;

    if (invert) {
        // Invert all rotations and reverse the order
        return qx.conjugate() * qy.conjugate() * qz.conjugate();
    } else {
        // Original ZYX order without inversion
        return qz * qy * qx;
    }
}

// Function to create a line entity representing an axis
Qt3DCore::QEntity* Bmode3DVisualizer::createAxis(Qt3DCore::QEntity* parent, const QVector3D& start, const QVector3D& end, const QColor& color) {
    auto *geometry = new Qt3DCore::QGeometry(parent);

    // Position vertices
    QByteArray bufferBytes;
    bufferBytes.resize(3 * 2 * sizeof(float)); // 2 vertices, 3 floats each
    float *positions = reinterpret_cast<float*>(bufferBytes.data());
    *positions++ = start.x();
    *positions++ = start.y();
    *positions++ = start.z();
    *positions++ = end.x();
    *positions++ = end.y();
    *positions++ = end.z();

    // Create buffer and fill it with vertex data
    auto *buf = new Qt3DCore::QBuffer(geometry);
    buf->setData(bufferBytes);

    // Create position attribute
    auto *positionAttribute = new Qt3DCore::QAttribute(geometry);
    positionAttribute->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
    positionAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(buf);
    positionAttribute->setByteStride(3 * sizeof(float));
    positionAttribute->setCount(2);
    geometry->addAttribute(positionAttribute);

    // Create a line mesh
    auto *line = new Qt3DRender::QGeometryRenderer(parent);
    line->setGeometry(geometry);
    line->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);

    // Create material
    auto *material = new Qt3DExtras::QPhongMaterial(parent);
    material->setAmbient(color);

    // Create entity for the axis
    auto *axisEntity = new Qt3DCore::QEntity(parent);
    axisEntity->addComponent(line);
    axisEntity->addComponent(material);

    return axisEntity;
}

void Bmode3DVisualizer::createArrow(Qt3DCore::QEntity* rootEntity, const QColor& color, const QVector3D& rotation) {
    auto *cylinder = new Qt3DExtras::QCylinderMesh();
    cylinder->setRadius(0.05f);
    cylinder->setLength(1.0f);
    cylinder->setRings(100);
    cylinder->setSlices(20);

    auto *cone = new Qt3DExtras::QConeMesh();
    cone->setTopRadius(0.0f);
    cone->setBottomRadius(0.1f);
    cone->setLength(0.2f);
    cone->setRings(50);
    cone->setSlices(20);

    auto *cylinderEntity = new Qt3DCore::QEntity(rootEntity);
    cylinderEntity->addComponent(cylinder);

    auto *coneEntity = new Qt3DCore::QEntity(rootEntity);
    coneEntity->addComponent(cone);

    auto *material = new Qt3DExtras::QPhongMaterial();
    material->setDiffuse(color);

    cylinderEntity->addComponent(material);
    coneEntity->addComponent(material);

    auto *cylinderTransform = new Qt3DCore::QTransform();
    cylinderTransform->setTranslation(QVector3D(0, 0.5f, 0));
    cylinderEntity->addComponent(cylinderTransform);

    auto *coneTransform = new Qt3DCore::QTransform();
    coneTransform->setTranslation(QVector3D(0, 1.0f, 0));
    coneEntity->addComponent(coneTransform);

    auto *arrowTransform = new Qt3DCore::QTransform();
    arrowTransform->setRotation(QQuaternion::fromEulerAngles(rotation.x(), rotation.y(), rotation.z()));
    rootEntity->addComponent(arrowTransform);
}

void Bmode3DVisualizer::createAxisVector(Qt3DCore::QEntity* parentEntity) {
    Qt3DCore::QEntity *arrowX = new Qt3DCore::QEntity(parentEntity);
    createArrow(arrowX, Qt::red, QVector3D(0, 0, -90));
    Qt3DCore::QEntity *arrowY = new Qt3DCore::QEntity(parentEntity);
    createArrow(arrowY, Qt::green, QVector3D(0, 0, 0));
    Qt3DCore::QEntity *arrowZ = new Qt3DCore::QEntity(parentEntity);
    createArrow(arrowZ, Qt::blue, QVector3D(90, 0, 0));
}
