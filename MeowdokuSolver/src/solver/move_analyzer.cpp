#include "move_analyzer.hpp"

#include <iostream>
#include <cmath>
#include <sstream>

using namespace std;


// ============================================
// CHECK IMMEDIATE GAME RULES
// ============================================

bool MoveAnalyzer::violatesImmediateRules(
    int row,
    int column,
    int region,
    const vector<SolverPosition>& cats,
    const vector<int>& usedRows,
    const vector<int>& usedColumns,
    const vector<int>& usedRegions
)
{
    // RULE 1: One cat per region.

    if (usedRegions[region])
        return true;


    // RULE 2: One cat per row.

    if (usedRows[row])
        return true;


    // RULE 3: One cat per column.

    if (usedColumns[column])
        return true;


    // RULE 4: No diagonal touching.

    for (const auto& cat : cats)
    {
        int dr = abs(row - cat.row);
        int dc = abs(column - cat.column);

        if (dr == 1 && dc == 1)
            return true;
    }

    return false;
}


// ============================================
// MAIN ANALYSIS ENGINE
// ============================================

MoveAnalyzerResult MoveAnalyzer::analyze(
    const vector<vector<int>>& regionMap,
    const vector<SolverPosition>& existingCats
)
{
    MoveAnalyzerResult report;

    if (regionMap.empty())
        return report;

    int N = static_cast<int>(regionMap.size());

    for (const auto& row : regionMap)
    {
        if (static_cast<int>(row.size()) != N)
        {
            cout << "[MOVE] Invalid grid dimensions.\n";
            return report;
        }
    }

    vector<int> usedRows(N, 0);
    vector<int> usedColumns(N, 0);
    vector<int> usedRegions(N, 0);


    // ========================================
    // REGISTER EXISTING CATS
    // ========================================

    for (const auto& cat : existingCats)
    {
        if (cat.row < 0 || cat.row >= N ||
            cat.column < 0 || cat.column >= N)
        {
            cout << "[MOVE] Invalid existing cat coordinates.\n";
            return report;
        }

        int actualRegion = regionMap[cat.row][cat.column];

        if (actualRegion < 0 || actualRegion >= N)
            return report;

        if (cat.region >= 0 && cat.region != actualRegion)
            return report;

        if (usedRows[cat.row] ||
            usedColumns[cat.column] ||
            usedRegions[actualRegion])
        {
            cout << "[MOVE] Existing cat conflict.\n";
            return report;
        }

        for (const auto& other : existingCats)
        {
            if (&other == &cat)
                break;

            if (abs(cat.row - other.row) == 1 &&
                abs(cat.column - other.column) == 1)
            {
                cout << "[MOVE] Existing diagonal conflict.\n";
                return report;
            }
        }

        usedRows[cat.row] = 1;
        usedColumns[cat.column] = 1;
        usedRegions[actualRegion] = 1;
    }


    cout << "\n========== INTELLIGENT MOVE ANALYZER ==========\n";

    cout << "[MOVE] Grid: " << N << " x " << N << "\n";

    cout << "[MOVE] Existing cats: "
         << existingCats.size() << "\n";


    // ========================================
    // ANALYZE EVERY CELL
    // ========================================

    for (int r = 0; r < N; r++)
    {
        for (int c = 0; c < N; c++)
        {
            report.totalCells++;

            int region = regionMap[r][c];

            if (region < 0 || region >= N)
            {
                report.rejectedByRules++;
                continue;
            }

            // Skip occupied regions.

            if (violatesImmediateRules(
                r, c, region,
                existingCats,
                usedRows,
                usedColumns,
                usedRegions))
            {
                report.rejectedByRules++;
                continue;
            }


            // =================================
            // HYPOTHETICAL MOVE SIMULATION
            // =================================

            vector<SolverPosition> hypotheticalCats =
                existingCats;

            hypotheticalCats.push_back({
                r, c, region
            });

            ConstraintSolver solver;

            SolverResult simulation;

            // Suppress repetitive solver diagnostics.

            ostringstream hiddenOutput;

            streambuf* oldBuffer =
                cout.rdbuf(hiddenOutput.rdbuf());

            simulation = solver.solve(
                regionMap,
                hypotheticalCats
            );

            cout.rdbuf(oldBuffer);


            // =================================
            // CLASSIFY THE MOVE
            // =================================

            MoveAnalysis move;

            move.row = r;
            move.column = c;
            move.region = region;

            if (simulation.solved)
            {
                move.status = MoveStatus::VIABLE;

                move.reason =
                    "Valid move; remaining puzzle is solvable.";

                report.viableMoves++;
            }
            else
            {
                move.status = MoveStatus::DEAD_END;

                move.reason =
                    "Legal locally, but creates an unsolvable puzzle.";

                report.deadEnds++;
            }

            report.moves.push_back(move);
        }
    }


    // ========================================
    // FINAL REPORT
    // ========================================

    cout << "\n========== MOVE ANALYSIS REPORT ==========\n";

    cout << "Total Cells: "
         << report.totalCells << "\n";

    cout << "Rejected by Rules: "
         << report.rejectedByRules << "\n";

    cout << "Dead-End Moves: "
         << report.deadEnds << "\n";

    cout << "Globally Viable Moves: "
         << report.viableMoves << "\n";


    cout << "\n========== VIABLE POSITIONS ==========\n";

    for (const auto& move : report.moves)
    {
        if (move.status == MoveStatus::VIABLE)
        {
            cout
                << "Region " << move.region + 1
                << " | Row " << move.row + 1
                << " | Column " << move.column + 1
                << "\n";
        }
    }

    cout << "=======================================\n";

    return report;
}