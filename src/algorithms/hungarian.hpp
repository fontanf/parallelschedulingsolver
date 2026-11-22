/**
 * Hungarian algorithm for the assignment problem, shared by several
 * algorithms.
 */

#pragma once

#include "parallelschedulingsolver/instance.hpp"

#include <limits>
#include <vector>

namespace parallelschedulingsolver
{

/**
 * Solve a rectangular assignment problem with the Hungarian algorithm.
 *
 * 'costs' has 'number_of_rows' rows and 'number_of_columns' columns, with
 * number_of_rows <= number_of_columns. Each row is assigned to a distinct
 * column. Return the column assigned to each row.
 *
 * Complexity: O(number_of_rows^2 number_of_columns).
 */
inline std::vector<int64_t> hungarian(
        const std::vector<std::vector<Time>>& costs,
        int64_t number_of_rows,
        int64_t number_of_columns)
{
    const Time inf = std::numeric_limits<Time>::max() / 4;
    // 1-indexed; index 0 is a dummy column.
    std::vector<Time> u(number_of_rows + 1, 0);
    std::vector<Time> v(number_of_columns + 1, 0);
    std::vector<int64_t> column_row(number_of_columns + 1, 0);
    std::vector<int64_t> way(number_of_columns + 1, 0);
    for (int64_t row = 1; row <= number_of_rows; ++row) {
        column_row[0] = row;
        int64_t column_0 = 0;
        std::vector<Time> minv(number_of_columns + 1, inf);
        std::vector<bool> used(number_of_columns + 1, false);
        do {
            used[column_0] = true;
            int64_t row_0 = column_row[column_0];
            Time delta = inf;
            int64_t column_1 = 0;
            for (int64_t column = 1; column <= number_of_columns; ++column) {
                if (used[column])
                    continue;
                Time cur = costs[row_0 - 1][column - 1] - u[row_0] - v[column];
                if (cur < minv[column]) {
                    minv[column] = cur;
                    way[column] = column_0;
                }
                if (minv[column] < delta) {
                    delta = minv[column];
                    column_1 = column;
                }
            }
            for (int64_t column = 0; column <= number_of_columns; ++column) {
                if (used[column]) {
                    u[column_row[column]] += delta;
                    v[column] -= delta;
                } else {
                    minv[column] -= delta;
                }
            }
            column_0 = column_1;
        } while (column_row[column_0] != 0);
        do {
            int64_t column_1 = way[column_0];
            column_row[column_0] = column_row[column_1];
            column_0 = column_1;
        } while (column_0 != 0);
    }

    std::vector<int64_t> row_column(number_of_rows, -1);
    for (int64_t column = 1; column <= number_of_columns; ++column)
        if (column_row[column] != 0)
            row_column[column_row[column] - 1] = column - 1;
    return row_column;
}

}
