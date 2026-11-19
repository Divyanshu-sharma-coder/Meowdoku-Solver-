#include "region_detector.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>


// ============================================
// STEP 1: SAMPLE CELL BACKGROUND COLOR
// ============================================

// ============================================
// STEP 1: CAT-RESISTANT CELL BACKGROUND SAMPLING
// ============================================

cv::Vec3b RegionDetector::sampleCellColor(
    const cv::Mat& image,
    const cv::Point2f& center
)
{
    int x = cvRound(center.x);
    int y = cvRound(center.y);

    // Sample several points away from the cell center.
    // This reduces interference from cats and cell icons.

    const std::vector<cv::Point> offsets =
    {
        {  0, -32 },
        {  0,  32 },
        {-32,   0 },
        { 32,   0 },

        {-27, -27 },
        { 27, -27 },
        {-27,  27 },
        { 27,  27 }
    };

    std::vector<cv::Vec3b> samples;

    const int PATCH_RADIUS = 4;

    for (const auto& offset : offsets)
    {
        int sx = x + offset.x;
        int sy = y + offset.y;

        int x1 = std::max(0, sx - PATCH_RADIUS);
        int y1 = std::max(0, sy - PATCH_RADIUS);

        int x2 = std::min(image.cols, sx + PATCH_RADIUS + 1);
        int y2 = std::min(image.rows, sy + PATCH_RADIUS + 1);

        if (x2 <= x1 || y2 <= y1)
            continue;

        cv::Mat patch = image(
            cv::Rect(x1, y1, x2 - x1, y2 - y1)
        );

        cv::Vec3b patchMedian;

        for (int channel = 0; channel < 3; channel++)
        {
            std::vector<uchar> values;

            values.reserve(patch.total());

            for (int py = 0; py < patch.rows; py++)
            {
                for (int px = 0; px < patch.cols; px++)
                {
                    values.push_back(
                        patch.at<cv::Vec3b>(py, px)[channel]
                    );
                }
            }

            if (values.empty())
                continue;

            size_t middle = values.size() / 2;

            std::nth_element(
                values.begin(),
                values.begin() + middle,
                values.end()
            );

            patchMedian[channel] = values[middle];
        }

        samples.push_back(patchMedian);
    }

    // Fallback if sampling fails.
    if (samples.empty())
    {
        return image.at<cv::Vec3b>(y, x);
    }

    // Calculate the median across all sampled patches.
    cv::Vec3b finalColor;

    for (int channel = 0; channel < 3; channel++)
    {
        std::vector<uchar> values;

        for (const auto& sample : samples)
        {
            values.push_back(sample[channel]);
        }

        size_t middle = values.size() / 2;

        std::nth_element(
            values.begin(),
            values.begin() + middle,
            values.end()
        );

        finalColor[channel] = values[middle];
    }

    return finalColor;
}

// ============================================
// STEP 2: CALCULATE COLOR DISTANCE IN LAB
// ============================================

double RegionDetector::colorDistance(
    const cv::Vec3b& a,
    const cv::Vec3b& b
)
{
    cv::Mat colorA(1, 1, CV_8UC3, cv::Scalar(
        a[0], a[1], a[2]
    ));

    cv::Mat colorB(1, 1, CV_8UC3, cv::Scalar(
        b[0], b[1], b[2]
    ));

    cv::Mat labA;
    cv::Mat labB;

    cv::cvtColor(
        colorA,
        labA,
        cv::COLOR_BGR2Lab
    );

    cv::cvtColor(
        colorB,
        labB,
        cv::COLOR_BGR2Lab
    );

    cv::Vec3b ca = labA.at<cv::Vec3b>(0, 0);
    cv::Vec3b cb = labB.at<cv::Vec3b>(0, 0);

    double dL = ca[0] - cb[0];
    double da = ca[1] - cb[1];
    double db = ca[2] - cb[2];

    return std::sqrt(
        dL * dL +
        da * da +
        db * db
    );
}


// ============================================
// STEP 3: REGION DETECTION
// ============================================

RegionDetection RegionDetector::detectRegions(
    const cv::Mat& image,
    const BoardDetection& board
)
{
    RegionDetection result;

    if (image.empty() || !board.found)
    {
        std::cerr << "[REGION] Invalid input.\n";
        return result;
    }

    int rows = board.rows;
    int cols = board.columns;

    int totalCells = rows * cols;

    std::cout << "\n========== REGION DETECTION ==========\n";

    std::cout << "[REGION] Grid: "
              << rows << " x " << cols << "\n";


    // ========================================
    // STEP 4: SAMPLE EVERY CELL
    // ========================================

    std::vector<cv::Vec3b> colors(totalCells);

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int index = r * cols + c;

            colors[index] = sampleCellColor(
                image,
                board.cellCenters[r][c]
            );
        }
    }

    std::cout << "[REGION] Cell colors sampled: "
              << totalCells << "\n";


    // ========================================
    // STEP 5: DISJOINT SET UNION (DSU)
    // ========================================

    std::vector<int> parent(totalCells);

    std::vector<int> rank(totalCells, 0);

    std::iota(
        parent.begin(),
        parent.end(),
        0
    );

    auto findRoot = [&](int x)
    {
        while (parent[x] != x)
        {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }

        return x;
    };

    auto unite = [&](int a, int b)
    {
        int rootA = findRoot(a);
        int rootB = findRoot(b);

        if (rootA == rootB)
            return;

        if (rank[rootA] < rank[rootB])
            std::swap(rootA, rootB);

        parent[rootB] = rootA;

        if (rank[rootA] == rank[rootB])
            rank[rootA]++;
    };


    // ========================================
    // STEP 6: CONNECT ADJACENT SAME-COLOR CELLS
    // ========================================

    const double COLOR_THRESHOLD = 18.0;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int current = r * cols + c;

            // Right neighbor
            if (c + 1 < cols)
            {
                int right = current + 1;

                double distance = colorDistance(
                    colors[current],
                    colors[right]
                );

                if (distance < COLOR_THRESHOLD)
                {
                    unite(current, right);
                }
            }

            // Bottom neighbor
            if (r + 1 < rows)
            {
                int bottom = current + cols;

                double distance = colorDistance(
                    colors[current],
                    colors[bottom]
                );

                if (distance < COLOR_THRESHOLD)
                {
                    unite(current, bottom);
                }
            }
        }
    }


    // ========================================
    // STEP 7: GENERATE REGION IDS
    // ========================================

    result.regionMap.assign(
        rows,
        std::vector<int>(cols, -1)
    );

    std::vector<int> rootToRegion(totalCells, -1);

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int index = r * cols + c;

            int root = findRoot(index);

            if (rootToRegion[root] == -1)
            {
                rootToRegion[root] = result.regionCount++;

                result.regionColors.push_back(
                    colors[index]
                );

                result.regionSizes.push_back(0);
            }

            int regionID = rootToRegion[root];

            result.regionMap[r][c] = regionID;

            result.regionSizes[regionID]++;
        }
    }


    // ========================================
    // STEP 8: GENERATE VISUAL DEBUG IMAGE
    // ========================================

    result.debugImage = image.clone();

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int regionID = result.regionMap[r][c];

            cv::Point center(
                cvRound(board.cellCenters[r][c].x),
                cvRound(board.cellCenters[r][c].y)
            );

            cv::putText(
                result.debugImage,
                std::to_string(regionID + 1),
                center,
                cv::FONT_HERSHEY_SIMPLEX,
                0.65,
                cv::Scalar(0, 0, 0),
                2
            );
        }
    }

    cv::imwrite(
        "assets/screenshots/region_detected.png",
        result.debugImage
    );


    // ========================================
    // STEP 9: EXPORT REGION MATRIX
    // ========================================

    std::ofstream file(
        "assets/screenshots/region_map.txt"
    );

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            file << result.regionMap[r][c] + 1 << " ";
        }

        file << "\n";
    }

    file.close();


    // ========================================
    // STEP 10: PRINT RESULTS
    // ========================================

    result.found = true;

    std::cout << "\n[REGION] Detection completed!\n";

    std::cout << "Total Regions: "
              << result.regionCount << "\n";

    std::cout << "\nRegion Statistics:\n";

    for (int i = 0; i < result.regionCount; i++)
    {
        std::cout
            << "Region "
            << i + 1
            << " -> "
            << result.regionSizes[i]
            << " cells\n";
    }

    std::cout << "\nSaved: region_detected.png\n";
    std::cout << "Saved: region_map.txt\n";

    std::cout << "======================================\n";

    return result;
}