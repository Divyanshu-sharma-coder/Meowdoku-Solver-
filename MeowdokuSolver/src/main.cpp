#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "region_detector.hpp"
#include "grid_detector.hpp"
#include "cat_detector.hpp"
#include "constraint_solver.hpp"
#include "move_analyzer.hpp"

int main(int argc, char* argv[])
{
    std::cout << "\n";
    std::cout << "====================================\n";
    std::cout << "       MEOWDOKU SOLVER ENGINE\n";
    std::cout << "====================================\n";

    if (argc < 2)
    {
        std::cerr << "Usage: MeowdokuSolver <image_path>\n";
        return 1;
    }

    cv::Mat screenshot = cv::imread(argv[1]);

    if (screenshot.empty())
    {
        std::cerr << "ERROR: Screenshot could not be loaded.\n";
        return 1;
    }

    std::cout << "\nScreenshot loaded successfully!\n";

    std::cout << "Resolution: "
              << screenshot.cols << " x "
              << screenshot.rows << "\n";


    // ========================================
    // PHASE 0: AUTOMATIC PHONE SCREEN CROPPING
    // ========================================

    if (screenshot.rows > screenshot.cols * 1.5)
    {
        std::cout << "\n[CAPTURE] Full phone screenshot detected.\n";
        std::cout << "[CROP] Extracting puzzle board...\n";

        // ========================================
        // MEOWDOKU BOARD CROP CALIBRATION
        // ========================================

        // Designed for 1080 x 2412 portrait screenshots.
        // Proportional coordinates support other resolutions.

        double cropTopRatio = 0.337;
        double cropHeightRatio = 0.443;

        int cropX = 0;

        int cropY = static_cast<int>(
            screenshot.rows * cropTopRatio
        );

        int cropWidth = screenshot.cols;

        int cropHeight = static_cast<int>(
            screenshot.rows * cropHeightRatio
        );

        // Safety boundaries
        cropWidth = std::min(
            cropWidth,
            screenshot.cols - cropX
        );

        cropHeight = std::min(
            cropHeight,
            screenshot.rows - cropY
        );

        cv::Rect puzzleBounds(
            cropX,
            cropY,
            cropWidth,
            cropHeight
        );

        screenshot = screenshot(puzzleBounds).clone();

        if (screenshot.empty())
        {
            std::cerr << "[CROP] Failed to extract puzzle board.\n";
            return 1;
        }

        cv::imwrite(
            "assets/screenshots/current_puzzle.png",
            screenshot
        );

        std::cout << "[CROP] Board extracted!\n";

        std::cout << "[CROP] Dimensions: "
                  << screenshot.cols << " x "
                  << screenshot.rows << "\n";

        std::cout << "[CROP] Saved: "
                  << "assets/screenshots/current_puzzle.png\n";
    }
    else
    {
        std::cout << "\n[CAPTURE] Cropped puzzle image detected.\n";
        std::cout << "[CROP] Skipping automatic cropping.\n";
    }


    // ========================================
    // PHASE 1: GRID DETECTION
    // ========================================

    std::cout << "\n[GRID] Starting grid detection...\n";

    GridDetector detector;

    BoardDetection detection =
        detector.detectBoard(screenshot);

    if (!detection.found)
    {
        std::cerr << "\n[GRID] Grid detection failed.\n";
        return 1;
    }

    std::cout << "[GRID] Grid detected successfully!\n";


    // ========================================
    // PHASE 2: REGION DETECTION
    // ========================================

    std::cout << "\n[REGION] Starting region detection...\n";

    RegionDetector regionDetector;

    RegionDetection regions =
        regionDetector.detectRegions(
            screenshot,
            detection
        );

    if (!regions.found)
    {
        std::cerr << "[REGION] Region detection failed.\n";
        return 1;
    }

    std::cout << "\n========== FINAL VISION REPORT ==========\n";

    std::cout << "Grid: "
              << detection.rows << " x "
              << detection.columns << "\n";

    std::cout << "Total Cells: "
              << detection.rows * detection.columns << "\n";

    std::cout << "Total Regions: "
              << regions.regionCount << "\n";

    std::cout << "Vision Pipeline: SUCCESS\n";

    std::cout << "=========================================\n";


    // ========================================
    // PHASE 3: CAT DETECTION
    // ========================================

    std::cout << "\n[CAT] Starting cat detection...\n";

    CatDetector catDetector;

    CatDetection cats = catDetector.detectCats(
        screenshot,
        detection,
        "assets/templates/cat.png"
    );

    if (!cats.found)
    {
        std::cerr << "[CAT] Detection initialization failed.\n";
        return 1;
    }

    std::cout << "\n========== COMPLETE VISION REPORT ==========\n";

    std::cout << "Grid Dimensions: "
              << detection.rows << " x "
              << detection.columns << "\n";

    std::cout << "Total Regions: "
              << regions.regionCount << "\n";

    std::cout << "Total Cats: "
              << cats.totalCats << "\n";

    std::cout << "Vision Pipeline: INITIALIZED\n";

    std::cout << "=============================================\n";


    // ========================================
    // PHASE 4: CONSTRAINT SOLVER
    // ========================================

    std::cout << "\n[SOLVER] Starting constraint solver...\n";

    ConstraintSolver solver;

    std::vector<SolverPosition> existingCats;

    for (const auto& cat : cats.cats)
    {
        int region =
            regions.regionMap[cat.row][cat.column];

        existingCats.push_back({
            cat.row,
            cat.column,
            region
        });
    }

    SolverResult solution = solver.solve(
        regions.regionMap,
        existingCats
    );

    if (!solution.solved)
    {
        std::cout << "\nNo valid solution exists "
                  << "for the supplied board state.\n";
    }
    else
    {
        std::cout << "\n========== FINAL CAT POSITIONS ==========\n";

        for (const auto& cat : solution.solution)
        {
            std::cout
                << "Region " << cat.region + 1
                << " -> Row " << cat.row + 1
                << ", Column " << cat.column + 1
                << "\n";
        }

        std::cout << "==========================================\n";
    }


    // ========================================
    // PHASE 5: INTELLIGENT MOVE ANALYZER
    // ========================================

    if (solution.solved)
    {
        MoveAnalyzer analyzer;

        std::cout << "\nStarting Intelligent Move Analysis...\n";

        MoveAnalyzerResult analysis =
            analyzer.analyze(
                regions.regionMap,
                existingCats
            );

        std::cout << "\n========== PHASE 5 COMPLETE ==========\n";

        std::cout << "Viable Moves: "
                  << analysis.viableMoves << "\n";

        std::cout << "Dead-End Moves: "
                  << analysis.deadEnds << "\n";

        std::cout << "=======================================\n";
    }


    // ========================================
    // PHASE 6: VISUAL SOLUTION GENERATOR
    // ========================================

    if (solution.solved)
    {
        std::cout << "\nStarting Visual Solution Generator...\n";

        cv::Mat solutionImage = screenshot.clone();

        for (const auto& position : solution.solution)
        {
            cv::Point center =
                detection.cellCenters[position.row][position.column];

            // Draw suggested cat position
            cv::circle(
                solutionImage,
                center,
                23,
                cv::Scalar(0, 0, 255),
                4
            );

            // Draw center marker
            cv::circle(
                solutionImage,
                center,
                5,
                cv::Scalar(0, 255, 0),
                -1
            );

            // Display row and column
            std::string label =
                std::to_string(position.row + 1) + "," +
                std::to_string(position.column + 1);

            cv::putText(
                solutionImage,
                label,
                cv::Point(center.x - 25, center.y - 28),
                cv::FONT_HERSHEY_SIMPLEX,
                0.45,
                cv::Scalar(255, 255, 255),
                2
            );
        }

        cv::imwrite(
            "assets/screenshots/solution_visualized.png",
            solutionImage
        );

        std::cout << "\n========== PHASE 6 COMPLETE ==========\n";

        std::cout << "Solution image generated!\n";

        std::cout << "Location: "
                  << "assets/screenshots/solution_visualized.png\n";

        std::cout << "======================================\n";
    }


    // ========================================
    // FINAL BOARD INFORMATION
    // ========================================

    std::cout << "\n========== BOARD INFORMATION ==========\n";

    std::cout << "Rows: "
              << detection.rows << "\n";

    std::cout << "Columns: "
              << detection.columns << "\n";

    std::cout << "Total Cells: "
              << detection.rows * detection.columns << "\n";

    std::cout << "\nFirst Cell Center: "
              << detection.cellCenters[0][0] << "\n";

    std::cout << "Last Cell Center: "
              << detection.cellCenters[detection.rows - 1]
                                            [detection.columns - 1]
              << "\n";

    std::cout << "\nGrid image saved successfully!\n";

    std::cout << "Location: assets/screenshots/grid_detected.png\n";

    std::cout << "\n====================================\n";
    std::cout << "       EXECUTION COMPLETE\n";
    std::cout << "====================================\n";

    return 0;
}