#pragma once

#include <set>    // std::set
#include <vector>   // std::vector

namespace solver {

template< typename T >
using Vector = std::vector< T >;

template< typename T >
using Matrix = Vector< Vector< T > >;

/**
 * Решает однородную СЛАУ с двоичными коэффициентами.
 * @return Множество векторов-решений, содержащих индексы, по которым установлены единицы.
 */
template< typename T >
inline std::vector<std::set<int>> GaussJordan(Matrix<T>& matrix) {
    std::vector<std::set<int>> result;
    if (matrix.empty())
        return result;
    const int rows = matrix.size();
    const int cols = matrix.at(0).size();
    // Состояния строк.
    std::vector<std::set<int>> states;
    for (int i = 0; i < rows; ++i) {
        states.push_back({i});
    }
    /**
    * Добавляет одно множество в другое по принципу исключения.
    * Если в другом множестве есть добавляемый элемент, то он удаляется.
    * Если в другом множестве нет добавляемого элемента, то он вставляется.
    */
    auto merge_by_xor = [](const std::set<int>& from, std::set<int>& to) -> void {
        for (const auto& f : from ) {
            if (to.contains(f)) {
                to.erase(f);
            } else {
                to.insert(f);
            }
        }
    };
    // Прямой ход.
    for( int k = 0; k < cols; ++k ) {
        int where_unit = -1;
        for( int i = k; i < rows; ++i ) {
            if (const bool is_not_zero = matrix.at(i).at(k) != 0; is_not_zero) {
                where_unit = i;
                break;
            }
        }
        if (where_unit == -1)
            continue;
        if (where_unit > k) {
            std::set<int>& indices_from = states.at(where_unit);
            std::set<int>& indices_to = states.at(k);
            merge_by_xor(indices_from, indices_to);
            for( int k1 = 0; k1 < cols; ++k1 ) {
                matrix[k][k1] ^= matrix.at( where_unit ).at( k1 );
            }
        }
        for( int i = k + 1; i < rows; ++i ) {
            if (const bool is_not_zero = matrix.at(i).at(k) != 0; is_not_zero) {
                std::set<int>& indices_from = states.at(k);
                std::set<int>& indices_to = states.at(i);
                merge_by_xor(indices_from, indices_to);
                for( int k1 = 0; k1 < cols; ++k1 ) {
                    matrix[i][k1] ^= matrix.at( k ).at( k1 );
                }
            }
        }
    }
    // Обратный ход.
    for( int k = cols - 1; k >= 0; --k ) {
        int i = rows + k - cols;
        if (i < 0)
            continue;
        if (const bool is_zero = matrix.at(i).at(k) == 0; is_zero)
            continue;
        for( int j = i - 1; j >= 0; --j ) {
            if (const bool is_not_zero = matrix.at(j).at(k) != 0; is_not_zero) {
                std::set<int>& indices_from = states.at(i);
                std::set<int>& indices_to = states.at(j);
                merge_by_xor(indices_from, indices_to);
                for( int k1 = 0; k1 < cols; ++k1 ) {
                    matrix[j][k1] ^= matrix.at( i ).at( k1 );
                }
            }
        }
    }
    // Поиск нулевых строк.
    for (int i = 0; i < rows; ++i) {
        bool zero = true;
        for( int j = 0; j < cols; ++j ) {
            zero &= matrix.at(i).at(j) == 0;
        }
        if (zero) {
            result.push_back(states.at(i));
        }
    }
    return result;
}
}
