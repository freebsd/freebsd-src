// Copyright 2012 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file utils/text/table.hpp
/// Table construction and formatting.

#if !defined(UTILS_TEXT_TABLE_HPP)
#define UTILS_TEXT_TABLE_HPP

#include "utils/text/table_fwd.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace utils {
namespace text {


/// Representation of a table.
///
/// A table is nothing more than a matrix of rows by columns.  The number of
/// columns is hardcoded at construction times, and the rows can be accumulated
/// at a later stage.
///
/// The only value of this class is a simpler and more natural mechanism of the
/// construction of a table, with additional sanity checks.  We could as well
/// just expose the internal data representation to our users.
class table {
    /// Widths of the table columns so far.
    widths_vector _column_widths;

    /// Type defining the collection of rows in the table.
    typedef std::vector< table_row > rows_vector;

    /// The rows of the table.
    ///
    /// This is actually the matrix representing the table.  Every element of
    /// this vector (which are vectors themselves) must have _ncolumns items.
    rows_vector _rows;

public:
    table(const table_row::size_type);

    widths_vector::size_type ncolumns(void) const;
    widths_vector::value_type column_width(const widths_vector::size_type)
        const;
    const widths_vector& column_widths(void) const;

    void add_row(const table_row&);

    bool empty(void) const;

    /// Constant iterator on the rows of the table.
    typedef rows_vector::const_iterator const_iterator;

    const_iterator begin(void) const;
    const_iterator end(void) const;
};


/// Settings to format a table.
///
/// This class implements a builder pattern to construct an object that contains
/// all the knowledge to format a table.  Once all the settings have been set,
/// the format() method provides the algorithm to apply such formatting settings
/// to any input table.
class table_formatter {
    /// Text to use as the separator between cells.
    std::string _separator;

    /// Colletion of widths of the columns of a table.
    std::size_t _table_width;

    /// Widths of the table columns.
    ///
    /// Note that this only includes widths for the column widths explicitly
    /// overriden by the caller.  In other words, this vector can be shorter
    /// than the table passed to the format() method, which is just fine.  Any
    /// non-specified column widths are assumed to be width_auto.
    widths_vector _column_widths;

public:
    table_formatter(void);

    static const std::size_t width_auto;
    static const std::size_t width_refill;
    table_formatter& set_column_width(const table_row::size_type,
                                      const std::size_t);
    table_formatter& set_separator(const char*);
    table_formatter& set_table_width(const std::size_t);

    std::vector< std::string > format(const table&) const;
};


}  // namespace text
}  // namespace utils

#endif  // !defined(UTILS_TEXT_TABLE_HPP)
