#include "constraint_solver.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>

using namespace std;


// ============================================
// INITIALIZE SOLVER
// ============================================

SolverResult ConstraintSolver::solve(
    const vector<vector<int>>& regionMap,
    const vector<SolverPosition>& existingCats
)
{
    result = SolverResult();

    placedCats.clear();

    if (regionMap.empty())
        return result;

    N = static_cast<int>(regionMap.size());

    // Validate square grid.

    for (const auto& row : regionMap)
    {
        if (static_cast<int>(row.size()) != N)
        {
            cout << "[SOLVER] Grid must be square.\n";
            return result;
        }
    }

    grid = regionMap;

    int maxRegion = -1;

    for (const auto& row : grid)
    {
        for (int region : row)
        {
            if (region < 0)
                return result;

            maxRegion = max(maxRegion, region);
        }
    }

    int regionCount = maxRegion + 1;

    if (regionCount != N)
    {
        cout << "[SOLVER] Region count must equal grid size.\n";

        cout << "Grid size: " << N
             << " Regions: " << regionCount << "\n";

        return result;
    }

    regionCells.assign(N, {});

    rowUsed.assign(N, 0);
    colUsed.assign(N, 0);
    regionUsed.assign(N, 0);

    // Build region cell lists.

    for (int r = 0; r < N; r++)
    {
        for (int c = 0; c < N; c++)
        {
            regionCells[grid[r][c]].push_back(
                r * N + c
            );
        }
    }

    cout << "\n========== CONSTRAINT SOLVER ==========\n";

    cout << "[SOLVER] Grid: "
         << N << " x " << N << "\n";

    cout << "[SOLVER] Regions: "
         << regionCount << "\n";


    // ========================================
    // LOAD EXISTING CATS
    // ========================================

    for (const auto& cat : existingCats)
    {
        int r = cat.row;
        int c = cat.column;

        if (r < 0 || r >= N || c < 0 || c >= N)
            return result;

        int region = grid[r][c];

        if (cat.region >= 0 && cat.region != region)
            return result;

        if (!isSafe(r, c, region))
        {
            cout << "[SOLVER] Existing cat conflict!\n";
            return result;
        }

        rowUsed[r] = 1;
        colUsed[c] = 1;
        regionUsed[region] = 1;

        placedCats.push_back({r, c, region});
    }

    cout << "[SOLVER] Existing cats: "
         << placedCats.size() << "\n";


    // ========================================
    // START SEARCH
    // ========================================

    cout << "[SOLVER] Starting MRV + Backtracking...\n";

    backtrack();

    result.solved = validateSolution();

    result.solution = placedCats;

    if (result.solved)
    {
        cout << "\n[SOLVER] SOLUTION FOUND!\n";
    }
    else
    {
        cout << "\n[SOLVER] No valid solution found.\n";
    }

    cout << "[SOLVER] Nodes explored: "
         << result.nodesExplored << "\n";

    cout << "[SOLVER] Backtracks: "
         << result.backtracks << "\n";

    cout << "[SOLVER] Total cats: "
         << result.solution.size() << "\n";

    cout << "=======================================\n";

    return result;
}


// ============================================
// CHECK DIAGONAL AND BASIC VALIDITY
// ============================================

bool ConstraintSolver::isValid(int row, int col)
{
    for (const auto& cat : placedCats)
    {
        int rowDistance = abs(row - cat.row);
        int colDistance = abs(col - cat.column);

        // Cats cannot touch diagonally.

        if (rowDistance == 1 && colDistance == 1)
            return false;
    }

    return true;
}


// ============================================
// CHECK ALL CONSTRAINTS
// ============================================

bool ConstraintSolver::isSafe(
    int row,
    int col,
    int region
)
{
    if (rowUsed[row])
        return false;

    if (colUsed[col])
        return false;

    if (regionUsed[region])
        return false;

    if (!isValid(row, col))
        return false;

    return true;
}


// ============================================
// FORWARD CHECKING
// ============================================

bool ConstraintSolver::forwardCheck()
{
    for (int region = 0; region < N; region++)
    {
        if (regionUsed[region])
            continue;

        bool hasCandidate = false;

        for (int cell : regionCells[region])
        {
            int r = cell / N;
            int c = cell % N;

            if (isSafe(r, c, region))
            {
                hasCandidate = true;
                break;
            }
        }

        // A remaining region has no legal position.

        if (!hasCandidate)
            return false;
    }

    return true;
}


// ============================================
// MINIMUM REMAINING VALUES (MRV)
// ============================================

int ConstraintSolver::selectNextRegion()
{
    int bestRegion = -1;

    int minimumCandidates = N * N + 1;

    for (int region = 0; region < N; region++)
    {
        if (regionUsed[region])
            continue;

        int candidates = 0;

        for (int cell : regionCells[region])
        {
            int r = cell / N;
            int c = cell % N;

            if (isSafe(r, c, region))
                candidates++;
        }

        if (candidates < minimumCandidates)
        {
            minimumCandidates = candidates;
            bestRegion = region;
        }
    }

    return bestRegion;
}


// ============================================
// BACKTRACKING ENGINE
// ============================================

void ConstraintSolver::backtrack()
{
    result.nodesExplored++;

    // All regions assigned.

    if (placedCats.size() == static_cast<size_t>(N))
    {
        result.totalSolutions++;

        if (validateSolution())
            return;

        return;
    }

    // Select most restricted region.

    int region = selectNextRegion();

    if (region == -1)
        return;

    // Try every cell in selected region.

    for (int cell : regionCells[region])
    {
        int r = cell / N;
        int c = cell % N;

        if (!isSafe(r, c, region))
            continue;

        // PLACE CAT.

        rowUsed[r] = 1;
        colUsed[c] = 1;
        regionUsed[region] = 1;

        placedCats.push_back({r, c, region});

        // FORWARD CHECK.

        if (forwardCheck())
        {
            backtrack();

            // Stop after first verified solution.

            if (result.totalSolutions > 0)
                return;
        }

        // UNDO CAT.

        placedCats.pop_back();

        rowUsed[r] = 0;
        colUsed[c] = 0;
        regionUsed[region] = 0;

        result.backtracks++;
    }
}


// ============================================
// INDEPENDENT SOLUTION VALIDATION
// ============================================

bool ConstraintSolver::validateSolution()
{
    if (placedCats.size() != static_cast<size_t>(N))
        return false;

    vector<int> rows(N, 0);
    vector<int> cols(N, 0);
    vector<int> regions(N, 0);

    for (const auto& cat : placedCats)
    {
        int r = cat.row;
        int c = cat.column;
        int region = cat.region;

        if (r < 0 || r >= N ||
            c < 0 || c >= N ||
            region < 0 || region >= N)
            return false;

        if (grid[r][c] != region)
            return false;

        if (rows[r]++ || cols[c]++ || regions[region]++)
            return false;
    }

    for (int i = 0; i < N; i++)
    {
        if (rows[i] != 1 ||
            cols[i] != 1 ||
            regions[i] != 1)
            return false;
    }

    for (int i = 0; i < N; i++)
    {
        for (int j = i + 1; j < N; j++)
        {
            int dr = abs(placedCats[i].row - placedCats[j].row);
            int dc = abs(placedCats[i].column - placedCats[j].column);

            if (dr == 1 && dc == 1)
                return false;
        }
    }

    return true;
}