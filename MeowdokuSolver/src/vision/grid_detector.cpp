#include "grid_detector.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>


// ============================================
// STEP 1: FIND INDIVIDUAL CELL CONTOURS
// ============================================

std::vector<cv::Rect> GridDetector::findCellCandidates(
    const cv::Mat& image
)
{
    cv::Mat gray;
    cv::Mat blurred;
    cv::Mat edges;

    cv::cvtColor(
        image,
        gray,
        cv::COLOR_BGR2GRAY
    );

    cv::GaussianBlur(
        gray,
        blurred,
        cv::Size(3, 3),
        0
    );

    cv::Canny(
        blurred,
        edges,
        30,
        120
    );

    std::vector<std::vector<cv::Point>> contours;

    cv::findContours(
        edges.clone(),
        contours,
        cv::RETR_LIST,
        cv::CHAIN_APPROX_SIMPLE
    );

    std::vector<cv::Rect> candidates;

    double imageArea =
        static_cast<double>(image.cols) * image.rows;

    for (const auto& contour : contours)
    {
        double area = cv::contourArea(contour);

        cv::Rect rect = cv::boundingRect(contour);

        double aspect =
            static_cast<double>(rect.width) /
            rect.height;

        // Reject tiny contours
        if (area < imageArea * 0.002)
            continue;

        // Reject excessively large contours
        if (area > imageArea * 0.04)
            continue;

        // Cells should be approximately square
        if (aspect < 0.80 || aspect > 1.20)
            continue;

        // Reject very small or unusually large boxes
        if (rect.width < image.cols * 0.035 ||
            rect.width > image.cols * 0.25)
            continue;

        if (rect.height < image.rows * 0.035 ||
            rect.height > image.rows * 0.25)
            continue;

        candidates.push_back(rect);
    }

    std::cout << "[GRID] Raw cell candidates: "
              << candidates.size() << "\n";

    return candidates;
}


// ============================================
// STEP 2: GROUP NEARBY COORDINATES
// ============================================

std::vector<float> GridDetector::clusterCoordinates(
    std::vector<float> coordinates,
    float tolerance
)
{
    if (coordinates.empty())
        return {};

    std::sort(
        coordinates.begin(),
        coordinates.end()
    );

    std::vector<float> clusters;

    float sum = coordinates[0];

    int count = 1;

    float previous = coordinates[0];

    for (size_t i = 1; i < coordinates.size(); i++)
    {
        if (coordinates[i] - previous <= tolerance)
        {
            sum += coordinates[i];
            count++;
        }
        else
        {
            clusters.push_back(sum / count);

            sum = coordinates[i];
            count = 1;
        }

        previous = coordinates[i];
    }

    clusters.push_back(sum / count);

    return clusters;
}


// ============================================
// STEP 3: DETECT AND RECONSTRUCT THE GRID
// ============================================

BoardDetection GridDetector::detectBoard(
    const cv::Mat& image
)
{
    BoardDetection result;

    if (image.empty())
    {
        std::cerr << "[ERROR] Empty screenshot.\n";
        return result;
    }

    result.debugImage = image.clone();

    std::cout << "\n[GRID] Detecting individual cells...\n";

    // Find cell candidates
    auto candidates = findCellCandidates(image);

    if (candidates.empty())
    {
        std::cerr << "[GRID] No cell candidates found.\n";
        return result;
    }

    // ========================================
    // STEP 4: REMOVE DUPLICATE CONTOURS
    // ========================================

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const cv::Rect& a, const cv::Rect& b)
        {
            return a.area() > b.area();
        }
    );

    std::vector<cv::Rect> cells;

    for (const auto& candidate : candidates)
    {
        bool duplicate = false;

        cv::Point2f center(
            candidate.x + candidate.width / 2.0f,
            candidate.y + candidate.height / 2.0f
        );

        for (const auto& existing : cells)
        {
            cv::Point2f existingCenter(
                existing.x + existing.width / 2.0f,
                existing.y + existing.height / 2.0f
            );

            float distance = cv::norm(
                center - existingCenter
            );

            float threshold =
                std::min(
                    candidate.width,
                    candidate.height
                ) * 0.25f;

            if (distance < threshold)
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
        {
            cells.push_back(candidate);
        }
    }

    std::cout << "[GRID] Unique cell candidates: "
              << cells.size() << "\n";


    if (cells.empty())
        return result;


    // ========================================
    // STEP 5: EXTRACT X AND Y COORDINATES
    // ========================================

    std::vector<float> xCoordinates;
    std::vector<float> yCoordinates;

    std::vector<float> widths;
    std::vector<float> heights;

    for (const auto& cell : cells)
    {
        xCoordinates.push_back(
            cell.x + cell.width / 2.0f
        );

        yCoordinates.push_back(
            cell.y + cell.height / 2.0f
        );

        widths.push_back(cell.width);
        heights.push_back(cell.height);
    }


    // Calculate median cell dimensions

    auto median = [](std::vector<float> values)
    {
        std::sort(
            values.begin(),
            values.end()
        );

        return values[values.size() / 2];
    };

    float medianWidth = median(widths);
    float medianHeight = median(heights);


    // ========================================
    // STEP 6: CLUSTER ROWS AND COLUMNS
    // ========================================

    float xTolerance = medianWidth * 0.40f;
    float yTolerance = medianHeight * 0.40f;

    auto columns = clusterCoordinates(
        xCoordinates,
        xTolerance
    );

    auto rows = clusterCoordinates(
        yCoordinates,
        yTolerance
    );

    result.rows = static_cast<int>(rows.size());

    result.columns = static_cast<int>(columns.size());

    std::cout << "[GRID] Detected rows: "
              << result.rows << "\n";

    std::cout << "[GRID] Detected columns: "
              << result.columns << "\n";


    if (result.rows < 2 || result.columns < 2)
    {
        std::cerr << "[GRID] Invalid grid dimensions.\n";
        return result;
    }


    // ========================================
    // STEP 7: RECONSTRUCT CELL MATRIX
    // ========================================

    result.cellCenters.resize(result.rows);

    result.cellBounds.resize(result.rows);

    for (int r = 0; r < result.rows; r++)
    {
        for (int c = 0; c < result.columns; c++)
        {
            cv::Point2f expectedCenter(
                columns[c],
                rows[r]
            );

            const cv::Rect* bestCell = nullptr;

            float bestDistance =
                std::numeric_limits<float>::max();

            for (const auto& cell : cells)
            {
                cv::Point2f actualCenter(
                    cell.x + cell.width / 2.0f,
                    cell.y + cell.height / 2.0f
                );

                float distance = cv::norm(
                    expectedCenter - actualCenter
                );

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestCell = &cell;
                }
            }

            result.cellCenters[r].push_back(
                expectedCenter
            );

            if (bestCell != nullptr &&
                bestDistance < medianWidth * 0.45f)
            {
                result.cellBounds[r].push_back(
                    *bestCell
                );
            }
            else
            {
                result.cellBounds[r].push_back(
                    cv::Rect()
                );
            }
        }
    }


    // ========================================
    // STEP 8: CALCULATE BOARD BOUNDARIES
    // ========================================

    int minX = image.cols;
    int minY = image.rows;

    int maxX = 0;
    int maxY = 0;

    for (const auto& cell : cells)
    {
        minX = std::min(minX, cell.x);
        minY = std::min(minY, cell.y);

        maxX = std::max(
            maxX,
            cell.x + cell.width
        );

        maxY = std::max(
            maxY,
            cell.y + cell.height
        );
    }

    result.boardBounds = cv::Rect(
        minX,
        minY,
        maxX - minX,
        maxY - minY
    );


    // ========================================
    // STEP 9: DRAW DETECTED GRID
    // ========================================

    for (int r = 0; r < result.rows; r++)
    {
        for (int c = 0; c < result.columns; c++)
        {
            cv::Point center(
                cvRound(result.cellCenters[r][c].x),
                cvRound(result.cellCenters[r][c].y)
            );

            cv::circle(
                result.debugImage,
                center,
                4,
                cv::Scalar(0, 0, 255),
                -1
            );

            cv::putText(
                result.debugImage,
                std::to_string(r) + "," + std::to_string(c),
                center + cv::Point(-20, -12),
                cv::FONT_HERSHEY_SIMPLEX,
                0.28,
                cv::Scalar(255, 0, 0),
                1
            );
        }
    }

    cv::rectangle(
        result.debugImage,
        result.boardBounds,
        cv::Scalar(0, 255, 0),
        3
    );


    // ========================================
    // STEP 10: SAVE DEBUG OUTPUT
    // ========================================

    cv::imwrite(
        "assets/screenshots/grid_detected.png",
        result.debugImage
    );

    result.boardCrop = image(
        result.boardBounds
    ).clone();

    cv::imwrite(
        "assets/screenshots/board_crop.png",
        result.boardCrop
    );


    result.found = true;

    result.confidence = 1.0;

    std::cout << "\n========== GRID DETECTION COMPLETE ==========\n";

    std::cout << "Rows: " << result.rows << "\n";

    std::cout << "Columns: " << result.columns << "\n";

    std::cout << "Total Cells: "
              << result.rows * result.columns << "\n";

    std::cout << "Cell Width: "
              << medianWidth << "\n";

    std::cout << "Cell Height: "
              << medianHeight << "\n";

    std::cout << "==============================================\n";

    return result;
}