#ifndef VOLUME3DCONTROLLER_H
#define VOLUME3DCONTROLLER_H

#include <Eigen/Dense>

#include <QObject>
#include <QtDataVisualization>

#include "mhareader.h"

/**
 * @class Volume3DController
 * @brief For handling 3D visualization of the 3D volume (essentially the bone surface).
 *
 * For the context. After the user collect the b-mode images and its transformation (stored in .mha file), user can
 * reconstruct the volume (with the help of PlusToolkit). This class will handle the visualization of that volume.
 * This class assume that the user already loaded the volume data (also was stored in another .mha file) and store in
 * MHAReader class. The visualization that is used in this class is using 3DScatter, the bones will shown as a dots in
 * 3D space, where its intensity is over a specified threshold.
 *
 * Note: Why we don't use a volume renderer, you might ask, instead of 3dscatter? We tried it. However, there is a "bug"
 * in qt when rendering the volume simultaneously with another object (like the A-mode 3D signal). The render seems "confuse"
 * which one is on top of each other. The visualization become very confusing, so that is why we use 3Dscatter here.
 *
 */

class Volume3DController : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor of the class
     */
    explicit Volume3DController(QObject *parent = nullptr, Q3DScatter *scatter = nullptr, MHAReader *mhareader=nullptr);

    /**
     * @brief Returns pixel intensity range (min and max)
     */
    std::array<int, 2> getPixelIntensityRange();

public slots:
    /**
     * @brief A slot where we could update the volume
     * As now i am writing this class, this function will be called when user change the threshold
     */
    void updateVolume(int value);

private:

    /**
     * @brief Returns indices from a vector (volume) that is over the threshold
     */
    void findIndicesWithThreshold(const std::vector<unsigned char>& volume, int threshold, std::vector<int>& result);

    /**
     * @brief Returns indices from a vector (volume) that is within the threshold
     */
    void findIndicesWithThreshold(const std::vector<unsigned char>& volume, std::vector<int> threshold, std::vector<int>& result);

    /**
     * @brief Returns values from a vector that is specified with indices
     */
    void getValuesWithIndices(const std::vector<unsigned char>& volume, const std::vector<int>& myindices, std::vector<unsigned char>& result);

    /**
     * @brief Converts indices into a subscript.
     * Imagine you have a long vector that it is actually a matrix. You have the indices but you want to know,
     * the position (row and column) in the matrix. This function will does that for you.
     */
    void ind2sub(const std::vector<int>& indices, const std::vector<int>& dimsize, std::vector<std::vector<int>>& voxelcoordinate);

    /**
     * @brief Convert std::vector of std::vector to an Eigen Matrix
     */
    Eigen::MatrixXd vectorVector2EigenMatrix(const std::vector<std::vector<int>>& voxel_coordinate);

    /**
     * @brief Convert right-hand CS (from Qualisys) to left-handed CS (Qt3DScatter plot)
     */
    Eigen::Affine3d RightToLeftHandedTransformation(const Eigen::Affine3d& rightHandedTransform);

    /**
     * @brief To calculate the bounding cube of the volume presented in 3D scatter plot
     */
    Eigen::MatrixXd findBoundingCubeBottomAligned(const Eigen::MatrixXd& boxVertices);

    /**
     * @brief Initialize the data that will be shown in the scatter
     */
    // void initData();


    // all variables related to MHA files
    Q3DScatter *m_scatter;                      //!< An object of the Q3Dscatter, initialized outside of this class
    MHAReader *myMHAReader_;                    //!< A pointer to an MHAReader object, contains the header data and the volume of MHA file
    MHAReader::MHAHeader myMHAHeader_;          //!< A pointer to an MHAHeader object, stores the header data from the MHA file
    std::vector<unsigned char> myMHAVolume_;    //!< A vector that stores the volume

    // variables for visualization only
    QLinearGradient gradient;                   //!< A gradient to color the data points in Q3DScatter

    // pixel information of the volume
    int pixelintensity_max_;                    //!< The maximum value of pixel intensity in the volume
    int pixelintensity_min_;                    //!< The minimum value of pixel intensity in the volume



signals:
};

#endif // VOLUME3DCONTROLLER_H
