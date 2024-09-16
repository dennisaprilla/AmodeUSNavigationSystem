#ifndef QCUSTOMPLOTINTERVALWINDOW_H
#define QCUSTOMPLOTINTERVALWINDOW_H

#include "qcustomplot.h"
#include <array>

/**
 * @class QCustomPlotIntervalWindow
 * @brief Inherits QCustomPlot, a class specific for visualizing A-mode 2D plot.
 *
 * This class basically the same as QCustomPlot but with added feature such as
 * listening to a mouse event (right, left, middle click) and using it as a
 * definition of a interval window for A-mode peaks.
 */

class QCustomPlotIntervalWindow : public QCustomPlot
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor.
     */
    explicit QCustomPlotIntervalWindow(QWidget *parent = nullptr);

    /**
     * @brief Set the shade color of the interval window.
     */
    void setShadeColor(const QColor& color);

    /**
     * @brief Set the initial spacing when the user click the 2d plot for the first time.
     */
    void setInitialSpacing(double spacing);

    /**
     * @brief If the user have the window lines definition already, they can set them using this.
     */
    void setInitialLines(std::array<std::optional<double>, 3> window);

    /**
     * @brief Get the lines positions which defines the window
     */
    std::array<std::optional<double>, 3> getLinePositions() const;

private slots:
    void onMousePress(QMouseEvent *event);

private:

    /**
     * @brief Update lines once the user click the 2d plot
     */
    void updateLines(double centerX);

    /**
     * @brief Update labels (text for x-positions of the lines) once the user click the 2d plot
     */
    void updateLabels();

    /**
     * @brief Update shading once the user click the 2d plot
     */
    void updateShading();

    /**
     * @brief Clear all elements (lines, shading, labels) once the user middle click
     */
    void clearAllElements();


    std::array<QCPItemLine*, 3> verticalLines;          //!< Stores the vertical lines (QCPItemLine) for the interval window.
    std::array<QPointer<QCPItemText>, 3> xValueLabels;  //!< Stores the labels (text for x-positions of the lines) (QCPItemText) for the interval window.
    QCPItemRect *shadeRect;                             //!< Stores shading object (QCPItemRect) for the interval window.

    double lineSpacing = 0.1;       //!< Initial line spacing.
    double centerX = 0;             //!< Initial centerX.
    bool elementsVisible = false;   //!< Initial visibility of the line.
};

#endif // QCUSTOMPLOTINTERVALWINDOW_H
