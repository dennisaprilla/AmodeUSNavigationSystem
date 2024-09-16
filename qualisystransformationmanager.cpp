#include "qualisystransformationmanager.h"
#include <stdexcept>

void QualisysTransformationManager::addTransformation(const std::string& id, const Eigen::Isometry3d& transform) {
    if (idToTransformMap.find(id) != idToTransformMap.end()) {
        throw std::runtime_error("QualisysTransformationManager: ID already exists");
    }
    idToTransformMap[id] = transform;
}

Eigen::Isometry3d QualisysTransformationManager::getTransformationById(const std::string& id) const {
    auto it = idToTransformMap.find(id);
    if (it != idToTransformMap.end()) {
        return it->second;
    } else {
        throw std::runtime_error("QualisysTransformationManager: Transformation ID not found");
    }
}

std::vector<Eigen::Isometry3d> QualisysTransformationManager::getAllTransformations() const {
    std::vector<Eigen::Isometry3d> transformations;
    for (const auto& pair : idToTransformMap) {
        transformations.push_back(pair.second);
    }
    return transformations;
}

std::vector<std::string> QualisysTransformationManager::getAllIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : idToTransformMap) {
        ids.push_back(pair.first);
    }
    return ids;
}

void QualisysTransformationManager::clearTransformations() {
    idToTransformMap.clear();
}
