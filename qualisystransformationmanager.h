#ifndef QUALISYSTRANSFORMATIONMANAGER_H
#define QUALISYSTRANSFORMATIONMANAGER_H

#include <Eigen/Geometry>
#include <unordered_map>
#include <string>

/**
 * @class QualisysTransformationManager
 * @brief For handling the transformation provided by Qualisys
 *
 * For the context. The motion capture system will detect multiple rigid bodies (b-mode holder or a-mode holders).
 * Each rigid bodies has their own names, specified in qualisys software. This class will help you to work with those
 * transformations, such as get the transformation by its name, get all the transformations, get all the names.
 *
 */

class QualisysTransformationManager {
public:

    /**
     * @brief Adding transformations to this class
     */
    void addTransformation(const std::string& id, const Eigen::Isometry3d& transform);

    /**
     * @brief GET the transformation that is stored in this class using its name (id)
     */
    Eigen::Isometry3d getTransformationById(const std::string& id) const;

    /**
     * @brief GET all the transformation that is stored in this class
     */
    std::vector<Eigen::Isometry3d> getAllTransformations() const;

    /**
     * @brief GET all the name of the transformation that is stored in this class
     */
    std::vector<std::string> getAllIds() const;

    /**
     * @brief Clear all the transformations
     */
    void clearTransformations();

private:
    std::unordered_map<std::string, Eigen::Isometry3d> idToTransformMap;    //!< Stores the transformations from Qualisys
};

#endif // QUALISYSTRANSFORMATIONMANAGER_H
