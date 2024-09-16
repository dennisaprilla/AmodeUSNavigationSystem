#ifndef AMODEDATAMANIPULATOR_H
#define AMODEDATAMANIPULATOR_H

#include <QVector>
#include <Eigen/Dense>

/**
 * @class AmodeDataManipulator
 * @brief This class is just a collection of functions that will be used for manipulating A-mode data vector.
 */

class AmodeDataManipulator
{
public:

    /**
     * @brief A function that will return the full one row
     */
    static QVector<int16_t> getRow(const QVector<int16_t>& input, int rowNumber, int totalColumns);

    /**
     * @brief A function for downsampling the amode data vector. It takes QVector<int16_t> and returns QVector<int16_t>
     */
    static QVector<int16_t> downsampleVector(const QVector<int16_t>& input, int targetSize);

    /**
     * @brief A function for downsampling the amode data vector. It takes Eigen::VectorXd and returns Eigen::VectorXd
     */
    static Eigen::VectorXd downsampleVector(const Eigen::VectorXd& input, int targetSize);
};

#endif // AMODEDATAMANIPULATOR_H
