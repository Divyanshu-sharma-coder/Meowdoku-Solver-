#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include "grid_detector.hpp"

struct RegionDetection
{
    bool found = false;

    int regionCount = 0;

    std::vector<std::vector<int>> regionMap;

    std::vector<cv::Vec3b> regionColors;

    std::vector<int> regionSizes;

    cv::Mat debugImage;
};

class RegionDetector
{
public:

    RegionDetection detectRegions(
        const cv::Mat& image,
        const BoardDetection& board
    );

private:

    cv::Vec3b sampleCellColor(
        const cv::Mat& image,
        const cv::Point2f& center
    );

    double colorDistance(
        const cv::Vec3b& a,
        const cv::Vec3b& b
    );
};