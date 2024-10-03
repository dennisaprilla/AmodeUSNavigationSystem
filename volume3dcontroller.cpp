#include "volume3dcontroller.h"
#include <omp.h>

Volume3DController::Volume3DController(QObject *parent, Q3DScatter *scatter, MHAReader *mhareader)
    : QObject{parent}, m_scatter(scatter), myMHAReader_(mhareader)
{
    // >> You might ask why do i use Q3DScatter instead of QCustom3DVolumetric to visualize our volumetric data.
    //    I must be very stupid, did not know how to use QCustom3DVolumetric.
    // >> The reason why i am doing this, because there is a rendering problem if i use QCustom3DVolumetric,
    //    a class that is inteded to handle volumetric data.
    // >> However, our goal is also to visualize signal peak along side the volumetric data. I was planning
    //    to use simple sphere to visualize the signal peak.
    // >> The problem comes because of the fact that the spheres will be inside the volumetric data. Then the
    //    rendering started to become wonky. The sphere "always at the back" of the opaque voxels (bones). I
    //    suspect it is because the Qt has unique rendering style for volumetric. The texture for opaque
    //    voxels always on top of anything else within the volume of the data.
    // >> So we instead performing a workaround by visualize the bone voxels as a point cloud.

    QSurfaceFormat format;
    format.setSamples(0);
    scatter->setFormat(format);

    // get the initial data
    myMHAHeader_ = myMHAReader_->getMHAHeader();
    myMHAVolume_ = myMHAReader_->getMHAVolume();

    // Find the max and min elements in the vector
    auto minmax = std::minmax_element(myMHAVolume_.begin(), myMHAVolume_.end());
    pixelintensity_min_ = *minmax.first;
    pixelintensity_max_ = *minmax.second;

    // delete all the series inside the scatter. This is new session of volume reconstruction,
    // i want everything that is reconstructed before, gone.
    for (QScatter3DSeries *series : m_scatter->seriesList()) {
        m_scatter->removeSeries(series);
    }

    // initial pixel intensity
    int init_threshold = pixelintensity_min_ + ((pixelintensity_max_ - pixelintensity_min_)/2);
    // add data
    updateVolume(init_threshold);
}

void Volume3DController::findIndicesWithThreshold(const std::vector<unsigned char>& volume, int threshold, std::vector<int>& result) {
    #pragma omp parallel for
    for (int i = 0; i <  static_cast<int>(volume.size()); ++i)
    {
        if (volume[i] > threshold) {
            #pragma omp critical
            result.push_back(i);
        }
    }
}

void Volume3DController::findIndicesWithThreshold(const std::vector<unsigned char>& volume, std::vector<int> threshold, std::vector<int>& result) {
    if (threshold.size() >2) return;

    #pragma omp parallel for
    for (int i = 0; i <  static_cast<int>(volume.size()); ++i)
    {
        if (volume[i] >= threshold[0] && volume[i] <= threshold[0]) {
            #pragma omp critical
            result.push_back(i);
        }
    }
}

void Volume3DController::getValuesWithIndices(const std::vector<unsigned char>& volume, const std::vector<int>& myindices, std::vector<unsigned char>& result) {
    // #pragma omp parallel for
    // for (int index : myindices) {
    //     #pragma omp critical
    //     result.push_back(volume[index]);
    // }

    // Resize the result vector to avoid concurrent modification of its size.
    result.resize(myindices.size());

    // Use a traditional for loop that OpenMP can parallelize.
    #pragma omp parallel for
    for (int i = 0; i < myindices.size(); ++i) {
        int index = myindices[i];
        // Assign the value to the corresponding position in the result vector.
        result[i] = volume[index];
    }
}

void Volume3DController::ind2sub(const std::vector<int>& indices, const std::vector<int>& dimsize, std::vector<std::vector<int>>& voxelcoordinate) {
    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(indices.size()); ++i)
    {
        // int x = indices[i] % dimsize[0];
        // int y = (indices[i]/dimsize[0]) % dimsize[1];
        // int z = indices[i] / (dimsize[0] * dimsize[1]);
        #pragma omp critical
        voxelcoordinate.push_back({(indices[i] % dimsize[0]), (indices[i] / dimsize[0]) % dimsize[1], (indices[i] / (dimsize[0] * dimsize[1]))});
    }
}

Eigen::MatrixXd Volume3DController::vectorVector2EigenMatrix(const std::vector<std::vector<int>>& voxel_coordinate) {
    // Assuming all inner vectors have the same size
    int rows = voxel_coordinate.size();
    int cols = voxel_coordinate[0].size();

    // Create a temporary vector to hold all the data
    std::vector<int> temp;
    temp.reserve(rows * cols);
    for (const auto& row : voxel_coordinate) {
        temp.insert(temp.end(), row.begin(), row.end());
    }

    // Map the data to an Eigen matrix
    Eigen::MatrixXd matrix = Eigen::Map<Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(temp.data(), rows, cols).cast<double>();

    return matrix;
}

Eigen::Affine3d Volume3DController::RightToLeftHandedTransformation(const Eigen::Affine3d& rightHandedTransform) {
    // Start by copying the input transformation
    Eigen::Affine3d leftHandedTransform = rightHandedTransform;

    // // Negate the Z components of the rotation part (third row and third column of the rotation matrix)
    // leftHandedTransform.linear()(0, 2) *= -1.0;
    // leftHandedTransform.linear()(1, 2) *= -1.0;
    // leftHandedTransform.linear()(2, 0) *= -1.0;
    // leftHandedTransform.linear()(2, 1) *= -1.0;

    leftHandedTransform.linear()(0, 0) *= -1.0;
    leftHandedTransform.linear()(0, 1) *= -1.0;
    leftHandedTransform.linear()(0, 2) *= -1.0;
    leftHandedTransform.linear()(1, 0) *= -1.0;
    leftHandedTransform.linear()(1, 1) *= -1.0;
    leftHandedTransform.linear()(1, 2) *= -1.0;

    // Negate the Z component of the translation vector
    leftHandedTransform.translation()(2) *= -1.0;

    return leftHandedTransform;
}

Eigen::MatrixXd Volume3DController::findBoundingCubeBottomAligned(const Eigen::MatrixXd& boxVertices) {
    // Ensure the input is 4x8 (homogeneous coordinates for 8 vertices)
    assert(boxVertices.rows() == 4 && boxVertices.cols() == 8);

    // Extract the x, y, z coordinates
    Eigen::Vector3d minCoords = boxVertices.block<3, 8>(0, 0).rowwise().minCoeff();
    Eigen::Vector3d maxCoords = boxVertices.block<3, 8>(0, 0).rowwise().maxCoeff();

    // Compute the center of the bounding box in the x and y directions
    Eigen::Vector2d xyCenter = (minCoords.head<2>() + maxCoords.head<2>()) / 2.0;

    // Determine the side length of the bounding cube
    double sideLength = (maxCoords - minCoords).maxCoeff();

    // Compute the half side length of the cube
    double halfSide = sideLength / 2.0;

    // Define the vertices of the cube in the local space
    Eigen::MatrixXd cubeVertices(4, 8);
    cubeVertices << -halfSide,  halfSide,  halfSide, -halfSide, -halfSide,  halfSide,  halfSide, -halfSide,
        -halfSide, -halfSide,  halfSide,  halfSide, -halfSide, -halfSide,  halfSide,  halfSide,
        0,        0,         0,         0,        sideLength, sideLength, sideLength, sideLength,
        1, 1, 1, 1, 1, 1, 1, 1;  // Homogeneous coordinate (w = 1)

    // Translate the cube to align the bottom with the min z-coordinate and center it in x and y
    for (int i = 0; i < 8; ++i) {
        cubeVertices(0, i) += xyCenter.x();  // Shift in x direction
        cubeVertices(1, i) += xyCenter.y();  // Shift in y direction
        cubeVertices(2, i) += minCoords.z(); // Align the bottom face with the min z-coordinate
    }

    return cubeVertices;
}

/*
void Volume3DController::initData()
{
    // initial pixel intensity
    int init_threshold = pixelintensity_min_ + ((pixelintensity_max_ - pixelintensity_min_)/2);

    // Get the data
    std::vector<int> indices;
    findIndicesWithThreshold(myMHAVolume_, init_threshold, indices);
    std::vector<unsigned char> volumevalues_overthresh;
    getValuesWithIndices(myMHAVolume_, indices, volumevalues_overthresh);

    // Convert indices to subscripts
    std::vector<std::vector<int>> voxelcoordinate;
    ind2sub(indices, myMHAHeader_.DimSize, voxelcoordinate);

    // Convert std::vector to Eigen::MatrixXd, more efficient to perform calculation
    Eigen::MatrixXd voxelcoordinate_ = vectorVector2EigenMatrix(voxelcoordinate).transpose();
    Eigen::MatrixXd voxelcoordinate_homogeneous(4, voxelcoordinate_.cols());
    voxelcoordinate_homogeneous << voxelcoordinate_, Eigen::MatrixXd::Ones(1, voxelcoordinate_.cols());

    // Corner voxel (to know the borders)
    std::vector<std::vector<int>> bordercoordinate;
    bordercoordinate.push_back({0, 0, 0});
    bordercoordinate.push_back({0, 0, myMHAHeader_.DimSize.at(2)});
    bordercoordinate.push_back({0, myMHAHeader_.DimSize.at(1), 0});
    bordercoordinate.push_back({0, myMHAHeader_.DimSize.at(1), myMHAHeader_.DimSize.at(2)});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), 0, 0});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), 0, myMHAHeader_.DimSize.at(2)});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), myMHAHeader_.DimSize.at(1), 0});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), myMHAHeader_.DimSize.at(1), myMHAHeader_.DimSize.at(2)});
    Eigen::MatrixXd bordercoordinate_ = vectorVector2EigenMatrix(bordercoordinate).transpose();
    Eigen::MatrixXd bordercoordinate_homogeneous(4, bordercoordinate_.cols());
    bordercoordinate_homogeneous << bordercoordinate_, Eigen::MatrixXd::Ones(1, bordercoordinate_.cols());
    Eigen::MatrixXd bordercubecoordinate_homogeneous = findBoundingCubeBottomAligned(bordercoordinate_homogeneous);

    // Get the transformation value from MHAReader
    // Eigen::Vector3d init_t(0, 0, 0);
    Eigen::Vector3d init_t(myMHAHeader_.Offset[0], myMHAHeader_.Offset[1], myMHAHeader_.Offset[2]);
    Eigen::Vector3d init_s(myMHAHeader_.ElementSpacing[0], myMHAHeader_.ElementSpacing[1], myMHAHeader_.ElementSpacing[2]);
    // Eigen::Matrix3d init_R = Eigen::Matrix3d::Identity();
    Eigen::Matrix3d init_R = Eigen::AngleAxisd(-M_PI / 2, Eigen::Vector3d::UnitX()).toRotationMatrix();
    Eigen::Affine3d init_A = Eigen::Affine3d::Identity();
    init_A.translate(init_t);
    init_A.linear() = init_R * init_s.asDiagonal();
    // Convert the transformation from right-hand (Qualisys) to left-hand (Qt3DScatter plot)
    Eigen::Affine3d init_A_lh   = RightToLeftHandedTransformation(init_A);

    // The point cloud also needs to be negated first (in the z component)
    voxelcoordinate_homogeneous.row(2) *= -1;
    // Transform the z-negated point cloud with transformed transformation
    voxelcoordinate_homogeneous = init_A_lh.matrix() * voxelcoordinate_homogeneous;
    voxelcoordinate_ = voxelcoordinate_homogeneous.topRows(3);

    // The point cloud also needs to be negated first (in the z component)
    bordercubecoordinate_homogeneous.row(2) *= -1;
    // Transform the z-negated point cloud with transformed transformation
    bordercubecoordinate_homogeneous = init_A_lh.matrix() * bordercubecoordinate_homogeneous;
    bordercoordinate_ = bordercubecoordinate_homogeneous.topRows(3);

    // // Apply the transformation
    // voxelcoordinate_homogeneous = init_A.matrix() * voxelcoordinate_homogeneous;
    // voxelcoordinate_ = voxelcoordinate_homogeneous.topRows(3);

    // Create a new QScatterDataArray
    QScatterDataArray* dataArray = new QScatterDataArray;
    // Resize the array to hold all points
    dataArray->resize(voxelcoordinate_.cols());
    // Copy the points from the Eigen matrix to the QScatterDataArray
    for (int i = 0; i < voxelcoordinate_.cols(); ++i) {
        (*dataArray)[i].setPosition( QVector3D(voxelcoordinate_(0, i),voxelcoordinate_(1, i),voxelcoordinate_(2, i)));
    }
    // for (int i = 0; i < bordercoordinate_.cols(); ++i) {
    //     (*dataArray)[i].setPosition( QVector3D(bordercoordinate_(0, i),bordercoordinate_(1, i),bordercoordinate_(2, i)));
    // }

    // Create a new scatter series
    QScatter3DSeries *series = new QScatter3DSeries();
    series->setName("tissue_layers");
    series->setItemSize(0.05f);
    series->setMesh(QAbstract3DSeries::MeshPoint);
    series->setBaseGradient(gradient);
    series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    series->dataProxy()->resetArray(dataArray);

    // add the volume to the series
    m_scatter->addSeries(series);

    Eigen::Vector3d minCoords = bordercoordinate_.block<3, 8>(0, 0).rowwise().minCoeff();
    Eigen::Vector3d maxCoords = bordercoordinate_.block<3, 8>(0, 0).rowwise().maxCoeff();
    m_scatter->axisX()->setRange(minCoords(0)-20, maxCoords(0)+20);
    m_scatter->axisY()->setRange(minCoords(1), maxCoords(1)+40);
    m_scatter->axisZ()->setRange(minCoords(2)-20, maxCoords(2)+20);

}
*/

void Volume3DController::updateVolume(int value)
{
    // delete all the series inside the scatter
    for (QScatter3DSeries *series : m_scatter->seriesList()) {
        if (series->name() == "tissue_layers") {
            m_scatter->removeSeries(series);
        }
    }

    // Get the data
    std::vector<int> indices;
    findIndicesWithThreshold(myMHAVolume_, value, indices);
    std::vector<unsigned char> volumevalues_overthresh;
    getValuesWithIndices(myMHAVolume_, indices, volumevalues_overthresh);

    // Convert indices to subscripts
    std::vector<std::vector<int>> voxelcoordinate;
    ind2sub(indices, myMHAHeader_.DimSize, voxelcoordinate);

    // Convert std::vector to Eigen::MatrixXd, more efficient to perform calculation
    Eigen::MatrixXd voxelcoordinate_ = vectorVector2EigenMatrix(voxelcoordinate).transpose();
    Eigen::MatrixXd voxelcoordinate_homogeneous(4, voxelcoordinate_.cols());
    voxelcoordinate_homogeneous << voxelcoordinate_, Eigen::MatrixXd::Ones(1, voxelcoordinate_.cols());

    // Corner voxel (to know the borders)
    std::vector<std::vector<int>> bordercoordinate;
    bordercoordinate.push_back({0, 0, 0});
    bordercoordinate.push_back({0, 0, myMHAHeader_.DimSize.at(2)});
    bordercoordinate.push_back({0, myMHAHeader_.DimSize.at(1), 0});
    bordercoordinate.push_back({0, myMHAHeader_.DimSize.at(1), myMHAHeader_.DimSize.at(2)});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), 0, 0});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), 0, myMHAHeader_.DimSize.at(2)});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), myMHAHeader_.DimSize.at(1), 0});
    bordercoordinate.push_back({myMHAHeader_.DimSize.at(0), myMHAHeader_.DimSize.at(1), myMHAHeader_.DimSize.at(2)});
    Eigen::MatrixXd bordercoordinate_ = vectorVector2EigenMatrix(bordercoordinate).transpose();
    Eigen::MatrixXd bordercoordinate_homogeneous(4, bordercoordinate_.cols());
    bordercoordinate_homogeneous << bordercoordinate_, Eigen::MatrixXd::Ones(1, bordercoordinate_.cols());
    Eigen::MatrixXd bordercubecoordinate_homogeneous = findBoundingCubeBottomAligned(bordercoordinate_homogeneous);

    // Get the transformation value from MHAReader
    // Eigen::Vector3d init_t(0, 0, 0);
    Eigen::Vector3d init_t(myMHAHeader_.Offset[0], myMHAHeader_.Offset[1], myMHAHeader_.Offset[2]);
    Eigen::Vector3d init_s(myMHAHeader_.ElementSpacing[0], myMHAHeader_.ElementSpacing[1], myMHAHeader_.ElementSpacing[2]);
    Eigen::Matrix3d init_R = Eigen::Matrix3d::Identity();
    // Eigen::Matrix3d init_R = Eigen::AngleAxisd(M_PI / 2, Eigen::Vector3d::UnitX()).toRotationMatrix();
    Eigen::Affine3d init_A = Eigen::Affine3d::Identity();
    init_A.translate(init_t);
    init_A.linear() = init_R * init_s.asDiagonal();

    // // Convert the transformation from right-hand (Qualisys) to left-hand (Qt3DScatter plot)
    // Eigen::Affine3d init_A_lh   = RightToLeftHandedTransformation(init_A);

    // // The point cloud also needs to be negated first (in the z component)
    // voxelcoordinate_homogeneous.row(2) *= -1;
    // // Transform the z-negated point cloud with transformed transformation
    // voxelcoordinate_homogeneous = init_A.matrix() * voxelcoordinate_homogeneous;
    // voxelcoordinate_ = voxelcoordinate_homogeneous.topRows(3);

    // // The point cloud also needs to be negated first (in the z component)
    // bordercubecoordinate_homogeneous.row(2) *= -1;
    // // Transform the z-negated point cloud with transformed transformation
    // bordercubecoordinate_homogeneous = init_A.matrix() * bordercubecoordinate_homogeneous;
    // bordercoordinate_ = bordercubecoordinate_homogeneous.topRows(3);

    voxelcoordinate_homogeneous = init_A.matrix() * voxelcoordinate_homogeneous;
    // voxelcoordinate_homogeneous.row(2) *= -1;
    voxelcoordinate_homogeneous.row(1).swap(voxelcoordinate_homogeneous.row(2));
    voxelcoordinate_ = voxelcoordinate_homogeneous.topRows(3);

    // Transform the z-negated point cloud with transformed transformation
    bordercubecoordinate_homogeneous = init_A.matrix() * bordercubecoordinate_homogeneous;
    // bordercubecoordinate_homogeneous.row(2) *= -1;
    bordercubecoordinate_homogeneous.row(1).swap(bordercubecoordinate_homogeneous.row(2));
    bordercoordinate_ = bordercubecoordinate_homogeneous.topRows(3);

    // // Apply the transformation
    // voxelcoordinate_homogeneous = init_A.matrix() * voxelcoordinate_homogeneous;
    // voxelcoordinate_ = voxelcoordinate_homogeneous.topRows(3);

    // Create a new QScatterDataArray
    QScatterDataArray* dataArray = new QScatterDataArray;
    // Resize the array to hold all points
    dataArray->resize(voxelcoordinate_.cols());
    // Copy the points from the Eigen matrix to the QScatterDataArray
    for (int i = 0; i < voxelcoordinate_.cols(); ++i) {
        (*dataArray)[i].setPosition( QVector3D(voxelcoordinate_(0, i),voxelcoordinate_(1, i),voxelcoordinate_(2, i)));
    }
    for (int i = 0; i < bordercoordinate_.cols(); ++i) {
        (*dataArray)[i].setPosition( QVector3D(bordercoordinate_(0, i),bordercoordinate_(1, i),bordercoordinate_(2, i)));
    }

    Eigen::Vector3d minCoords = bordercoordinate_.block<3, 8>(0, 0).rowwise().minCoeff();
    Eigen::Vector3d maxCoords = bordercoordinate_.block<3, 8>(0, 0).rowwise().maxCoeff();
    double maxZ_inScatter     = maxCoords(1)+(extensionvolume_mm*2) - minCoords(1);
    double maxWhite_inScatter = (0.4*maxZ_inScatter)/maxZ_inScatter;

    QLinearGradient gradient;
    gradient.setColorAt(0.0, QColor(50,50,50));                    // Low values of Z
    gradient.setColorAt(maxWhite_inScatter, QColor(255,255,255));  // High values of Z

    // Create a new scatter series
    QScatter3DSeries *series = new QScatter3DSeries();
    series->setName("tissue_layers");
    series->setItemSize(0.05f);
    series->setMesh(QAbstract3DSeries::MeshPoint);
    series->setBaseGradient(gradient);
    series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    series->dataProxy()->resetArray(dataArray);

    // add the volume to the series
    m_scatter->addSeries(series);
    m_scatter->axisX()->setRange(minCoords(0)-extensionvolume_mm, maxCoords(0)+extensionvolume_mm);
    m_scatter->axisY()->setRange(minCoords(1), maxCoords(1)+(extensionvolume_mm*2));
    m_scatter->axisZ()->setRange(minCoords(2)-extensionvolume_mm, maxCoords(2)+extensionvolume_mm);

}

std::array<int, 2> Volume3DController::getPixelIntensityRange() {
    std::array<int, 2> result = {pixelintensity_min_, pixelintensity_max_}; // Replace with your desired integers
    return result;
}
















