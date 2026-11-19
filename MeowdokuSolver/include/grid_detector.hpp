#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

struct BoardDetection
{
    bool found = false;

    int rows = 0;
    int columns = 0;

    cv::Rect boardBounds;

    std::vector<std::vector<cv::Point2f>> cellCenters;

    std::vector<std::vector<cv::Rect>> cellBounds;

    double confidence = 0.0;

    cv::Mat debugImage;
    cv::Mat boardCrop;
};

class GridDetector
{
public:

    BoardDetection detectBoard(const cv::Mat& image);

private:

    std::vector<cv::Rect> findCellCandidates(
        const cv::Mat& image
    );

    std::vector<float> clusterCoordinates(
        std::vector<float> coordinates,
        float tolerance
    );
};