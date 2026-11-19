#pragma once

#include <vector>

struct SolverPosition
{
    int row;
    int column;
    int region;
};

struct SolverResult
{
    bool solved = false;

    int totalSolutions = 0;
    long long nodesExplored = 0;
    long long backtracks = 0;

    std::vector<SolverPosition> solution;
};

class ConstraintSolver
{
public:

    SolverResult solve(
        const std::vector<std::vector<int>>& regionMap,
        const std::vector<SolverPosition>& existingCats = {}
    );

private:

    int N = 0;

    std::vector<std::vector<int>> grid;

    std::vector<std::vector<int>> regionCells;

    std::vector<int> rowUsed;
    std::vector<int> colUsed;

    std::vector<int> regionUsed;

    std::vector<SolverPosition> placedCats;

    SolverResult result;

    bool isValid(int row, int col);

    bool isSafe(
        int row,
        int col,
        int region
    );

    bool forwardCheck();

    int selectNextRegion();

    void backtrack();

    bool validateSolution();
};