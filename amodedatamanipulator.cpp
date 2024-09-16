#include "amodedatamanipulator.h"
#include <algorithm>
#include <cmath>

QVector<int16_t> AmodeDataManipulator::getRow(const QVector<int16_t>& input, int rowNumber, int totalColumns) {
    int startIndex = rowNumber * totalColumns;
    return input.mid(startIndex, totalColumns);
}

/*
QVector<int16_t> AmodeDataManipulator::downsampleVector(const QVector<int16_t>& input, int targetSize) {
    QVector<int16_t> output;
    output.reserve(targetSize);

    // Calculate the step size based on the ratio of the target size to the input size
    int step = input.size() / targetSize;

    // Use std::transform with a stride to downsample without explicit looping
    std::transform(input.begin(), input.end(), std::back_inserter(output),
                   [step, n = 0] (int16_t value) mutable {
                       return n++ % step == 0 ? value : 0;
                   });

    // Remove the '0' elements which are placeholders for non-sampled elements
    output.erase(std::remove(output.begin(), output.end(), 0), output.end());

    return output;
}
*/

QVector<int16_t> AmodeDataManipulator::downsampleVector(const QVector<int16_t>& input, int targetSize) {
    if (targetSize <= 0 || input.isEmpty()) {
        return QVector<int16_t>();
    }

    QVector<int16_t> output(targetSize);
    double step = static_cast<double>(input.size() - 1) / (targetSize - 1);

    std::generate(output.begin(), output.end(), [&input, step, n = 0]() mutable {
        int index = std::min<int>(static_cast<int>(std::round(n * step)), input.size() - 1);
        ++n;
        return input[index];
    });

    return output;
}

/*
Eigen::VectorXd AmodeDataManipulator::downsampleVector(const Eigen::VectorXd& input, int targetSize) {
    // Calculate the step size based on the ratio of the target size to the input size
    int step = input.size() / targetSize;

    // Create a vector of indices for downsampling
    Eigen::VectorXi indices = Eigen::VectorXi::LinSpaced(targetSize, 0, step * (targetSize - 1));

    // Use Eigen's fancy indexing to create the downsampled vector
    Eigen::VectorXd output = input(indices.array());

    return output;
}
*/

Eigen::VectorXd AmodeDataManipulator::downsampleVector(const Eigen::VectorXd& input, int targetSize) {
    // Calculate the step size as a floating-point value
    double step = static_cast<double>(input.size()) / targetSize;

    // Create a vector to store the downsampled data
    Eigen::VectorXd output(targetSize);

    // Use a for loop to downsample the input vector
    for (int i = 0; i < targetSize; ++i) {
        // Calculate the index in the input vector to sample
        int index = static_cast<int>(std::floor(i * step));
        output(i) = input(index);
    }

    return output;
}


