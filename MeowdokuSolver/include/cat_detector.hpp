#pragma once

#include "grid_detector.hpp"

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

struct CatPosition
{
    int row;
    int column;

    cv::Point center;

    double confidence;
};

struct CatDetection
{
    bool found = false;

    int totalCats = 0;

    std::vector<CatPosition> cats;

    cv::Mat debugImage;
};

class CatDetector
{
public:

    CatDetection detectCats(
        const cv::Mat& image,
        const BoardDetection& board,
        const std::string& templatePath
    );

private:

    cv::Mat preprocess(
        const cv::Mat& image
    );
};