#include "cat_detector.hpp"

#include <iostream>
#include <algorithm>

using namespace cv;
using namespace std;


// ============================================
// STEP 1: IMAGE PREPROCESSING
// ============================================

Mat CatDetector::preprocess(const Mat& image)
{
    Mat gray;
    Mat edges;

    cvtColor(image, gray, COLOR_BGR2GRAY);

    GaussianBlur(
        gray,
        gray,
        Size(3, 3),
        0
    );

    Canny(
        gray,
        edges,
        60,
        150
    );

    return edges;
}


// ============================================
// STEP 2: CAT DETECTION
// ============================================

CatDetection CatDetector::detectCats(
    const Mat& image,
    const BoardDetection& board,
    const string& templatePath
)
{
    CatDetection result;

    cout << "\n========== CAT DETECTION ==========\n";

    if (image.empty() || !board.found)
    {
        cerr << "[CAT] Invalid input.\n";
        return result;
    }

    // Load cat template.

    Mat catTemplate = imread(
        templatePath,
        IMREAD_COLOR
    );

    if (catTemplate.empty())
    {
        cerr << "\n[CAT] Template not found!\n";

        cerr << "Expected location: "
             << templatePath << "\n";

        cerr << "Cat detection cannot start.\n";

        return result;
    }

    // Convert template into edges.

    Mat templateEdges = preprocess(catTemplate);

    int rows = board.rows;
    int cols = board.columns;

    cout << "[CAT] Grid: "
         << rows << " x " << cols << "\n";

    cout << "[CAT] Scanning "
         << rows * cols
         << " cells...\n";


    // ========================================
    // STEP 3: SCAN EVERY CELL
    // ========================================

    const double MATCH_THRESHOLD = 0.65;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            Rect cell = board.cellBounds[r][c];

            // Ignore cell borders.

            int marginX = cell.width * 0.18;
            int marginY = cell.height * 0.18;

            Rect innerCell(
                cell.x + marginX,
                cell.y + marginY,
                cell.width - 2 * marginX,
                cell.height - 2 * marginY
            );

            if (innerCell.width < templateEdges.cols ||
                innerCell.height < templateEdges.rows)
            {
                continue;
            }

            Mat cellImage = image(innerCell);

            Mat cellEdges = preprocess(cellImage);

            Mat matchResult;

            matchTemplate(
                cellEdges,
                templateEdges,
                matchResult,
                TM_CCOEFF_NORMED
            );

            double minValue;
            double maxValue;

            Point minLocation;
            Point maxLocation;

            minMaxLoc(
                matchResult,
                &minValue,
                &maxValue,
                &minLocation,
                &maxLocation
            );

            if (maxValue >= MATCH_THRESHOLD)
            {
                CatPosition cat;

                cat.row = r;
                cat.column = c;

                cat.center = Point(
                    cvRound(board.cellCenters[r][c].x),
                    cvRound(board.cellCenters[r][c].y)
                );

                cat.confidence = maxValue;

                result.cats.push_back(cat);

                cout << "\n[CAT] Detected!\n";

                cout << "Row: "
                     << r + 1 << "\n";

                cout << "Column: "
                     << c + 1 << "\n";

                cout << "Confidence: "
                     << maxValue << "\n";
            }
        }
    }


    // ========================================
    // STEP 4: GENERATE DEBUG IMAGE
    // ========================================

    result.debugImage = image.clone();

    for (const auto& cat : result.cats)
    {
        circle(
            result.debugImage,
            cat.center,
            28,
            Scalar(0, 255, 0),
            3
        );

        putText(
            result.debugImage,
            "CAT",
            Point(
                cat.center.x - 20,
                cat.center.y - 32
            ),
            FONT_HERSHEY_SIMPLEX,
            0.6,
            Scalar(0, 255, 0),
            2
        );
    }

    imwrite(
        "assets/screenshots/cat_detected.png",
        result.debugImage
    );


    // ========================================
    // STEP 5: FINAL REPORT
    // ========================================

    result.totalCats = static_cast<int>(
        result.cats.size()
    );

    result.found = true;

    cout << "\n========== CAT DETECTION REPORT ==========\n";

    cout << "Total Cats: "
         << result.totalCats << "\n";

    cout << "Scanned Cells: "
         << rows * cols << "\n";

    cout << "Detection Complete!\n";

    cout << "Saved: cat_detected.png\n";

    cout << "==========================================\n";

    return result;
}