#pragma once

#include "constraint_solver.hpp"

#include <vector>
#include <string>

enum class MoveStatus
{
    INVALID,
    DEAD_END,
    VIABLE
};

struct MoveAnalysis
{
    int row;
    int column;
    int region;

    MoveStatus status;

    std::string reason;
};

struct MoveAnalyzerResult
{
    std::vector<MoveAnalysis> moves;

    int totalCells = 0;
    int rejectedByRules = 0;
    int deadEnds = 0;
    int viableMoves = 0;
};

class MoveAnalyzer
{
public:

    MoveAnalyzerResult analyze(
        const std::vector<std::vector<int>>& regionMap,
        const std::vector<SolverPosition>& existingCats
    );

private:

    bool violatesImmediateRules(
        int row,
        int column,
        int region,
        const std::vector<SolverPosition>& cats,
        const std::vector<int>& usedRows,
        const std::vector<int>& usedColumns,
        const std::vector<int>& usedRegions
    );

};