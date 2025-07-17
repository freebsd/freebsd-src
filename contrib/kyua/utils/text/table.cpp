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

#include "utils/text/table.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <sstream>

#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace text = utils::text;


namespace {


/// Applies user overrides to the column widths of a table.
///
/// \param table The table from which to calculate the column widths.
/// \param user_widths The column widths provided by the user.  This vector must
///     have less or the same number of elements as the columns of the table.
///     Values of width_auto are ignored; any other explicit values are copied
///     to the output widths vector, including width_refill.
///
/// \return A vector with the widths of the columns of the input table with any
/// user overrides applied.
static text::widths_vector
override_column_widths(const text::table& table,
                       const text::widths_vector& user_widths)
{
    PRE(user_widths.size() <= table.ncolumns());
    text::widths_vector widths = table.column_widths();

    // Override the actual width of the columns based on user-specified widths.
    for (text::widths_vector::size_type i = 0; i < user_widths.size(); ++i) {
        const text::widths_vector::value_type& user_width = user_widths[i];
        if (user_width != text::table_formatter::width_auto) {
            PRE_MSG(user_width == text::table_formatter::width_refill ||
                    user_width >= widths[i],
                    "User-provided column widths must be larger than the "
                    "column contents (except for the width_refill column)");
            widths[i] = user_width;
        }
    }

    return widths;
}


/// Locates the refill column, if any.
///
/// \param widths The widths of the columns as returned by
///     override_column_widths().  Note that one of the columns may or may not
///     be width_refill, which is the column we are looking for.
///
/// \return The index of the refill column with a width_refill width if any, or
/// otherwise the index of the last column (which is the default refill column).
static text::widths_vector::size_type
find_refill_column(const text::widths_vector& widths)
{
    text::widths_vector::size_type i = 0;
    for (; i < widths.size(); ++i) {
        if (widths[i] == text::table_formatter::width_refill)
            return i;
    }
    return i - 1;
}


/// Pads the widths of the table to fit within a maximum width.
///
/// On output, a column of the widths vector is truncated to a shorter length
/// than its current value, if the total width of the table would exceed the
/// maximum table width.
///
/// \param [in,out] widths The widths of the columns as returned by
///     override_column_widths().  One of these columns should have a value of
///     width_refill; if not, a default column is refilled.
/// \param user_max_width The target width of the table; must not be zero.
/// \param column_padding The padding between the cells, if any.  The target
///     width should be larger than the padding times the number of columns; if
///     that is not the case, we attempt a readjustment here.
static void
refill_widths(text::widths_vector& widths,
              const text::widths_vector::value_type user_max_width,
              const std::size_t column_padding)
{
    PRE(user_max_width != 0);

    // widths.size() is a proxy for the number of columns of the table.
    const std::size_t total_padding = column_padding * (widths.size() - 1);
    const text::widths_vector::value_type max_width = std::max(
        user_max_width, total_padding) - total_padding;

    const text::widths_vector::size_type refill_column =
        find_refill_column(widths);
    INV(refill_column < widths.size());

    text::widths_vector::value_type width = 0;
    for (text::widths_vector::size_type i = 0; i < widths.size(); ++i) {
        if (i != refill_column)
            width += widths[i];
    }
    widths[refill_column] = max_width - width;
}


/// Pads an input text to a specified width with spaces.
///
/// \param input The text to add padding to (may be empty).
/// \param length The desired length of the output.
/// \param is_last Whether the text being processed belongs to the last column
///     of a row or not.  Values in the last column should not be padded to
///     prevent trailing whitespace on the screen (which affects copy/pasting
///     for example).
///
/// \return The padded cell.  If the input string is longer than the desired
/// length, the input string is returned verbatim.  The padded table won't be
/// correct, but we don't expect this to be a common case to worry about.
static std::string
pad_cell(const std::string& input, const std::size_t length, const bool is_last)
{
    if (is_last)
        return input;
    else {
        if (input.length() < length)
            return input + std::string(length - input.length(), ' ');
        else
            return input;
    }
}


/// Refills a cell and adds it to the output lines.
///
/// \param row The row containing the cell to be refilled.
/// \param widths The widths of the row.
/// \param column The column being refilled.
/// \param [in,out] textual_rows The output lines as processed so far.  This is
///     updated to accomodate for the contents of the refilled cell, extending
///     the rows as necessary.
static void
refill_cell(const text::table_row& row, const text::widths_vector& widths,
            const text::table_row::size_type column,
            std::vector< text::table_row >& textual_rows)
{
    const std::vector< std::string > rows = text::refill(row[column],
                                                         widths[column]);

    if (textual_rows.size() < rows.size())
        textual_rows.resize(rows.size(), text::table_row(row.size()));

    for (std::vector< std::string >::size_type i = 0; i < rows.size(); ++i) {
        for (text::table_row::size_type j = 0; j < row.size(); ++j) {
            const bool is_last = j == row.size() - 1;
            if (j == column)
                textual_rows[i][j] = pad_cell(rows[i], widths[j], is_last);
            else {
                if (textual_rows[i][j].empty())
                    textual_rows[i][j] = pad_cell("", widths[j], is_last);
            }
        }
    }
}


/// Formats a single table row.
///
/// \param row The row to format.
/// \param widths The widths of the columns to apply during formatting.  Cells
///     wider than the specified width are refilled to attempt to fit in the
///     cell.  Cells narrower than the width are right-padded with spaces.
/// \param separator The column separator to use.
///
/// \return The textual lines that contain the formatted row.
static std::vector< std::string >
format_row(const text::table_row& row, const text::widths_vector& widths,
           const std::string& separator)
{
    PRE(row.size() == widths.size());

    std::vector< text::table_row > textual_rows(1, text::table_row(row.size()));

    for (text::table_row::size_type column = 0; column < row.size(); ++column) {
        if (widths[column] > row[column].length())
            textual_rows[0][column] = pad_cell(row[column], widths[column],
                                               column == row.size() - 1);
        else
            refill_cell(row, widths, column, textual_rows);
    }

    std::vector< std::string > lines;
    for (std::vector< text::table_row >::const_iterator
         iter = textual_rows.begin(); iter != textual_rows.end(); ++iter) {
        lines.push_back(text::join(*iter, separator));
    }
    return lines;
}


}  // anonymous namespace


/// Constructs a new table.
///
/// \param ncolumns_ The number of columns that the table will have.
text::table::table(const table_row::size_type ncolumns_)
{
    _column_widths.resize(ncolumns_, 0);
}


/// Gets the number of columns in the table.
///
/// \return The number of columns in the table.  This value remains constant
/// during the existence of the table.
text::widths_vector::size_type
text::table::ncolumns(void) const
{
    return _column_widths.size();
}


/// Gets the width of a column.
///
/// The returned value is not valid if add_row() is called again, as the column
/// may have grown in width.
///
/// \param column The index of the column of which to get the width.  Must be
///     less than the total number of columns.
///
/// \return The width of a column.
text::widths_vector::value_type
text::table::column_width(const widths_vector::size_type column) const
{
    PRE(column < _column_widths.size());
    return _column_widths[column];
}


/// Gets the widths of all columns.
///
/// The returned value is not valid if add_row() is called again, as the columns
/// may have grown in width.
///
/// \return A vector with the width of all columns.
const text::widths_vector&
text::table::column_widths(void) const
{
    return _column_widths;
}


/// Checks whether the table is empty or not.
///
/// \return True if the table is empty; false otherwise.
bool
text::table::empty(void) const
{
    return _rows.empty();
}


/// Adds a row to the table.
///
/// \param row The row to be added.  This row must have the same amount of
///     columns as defined during the construction of the table.
void
text::table::add_row(const table_row& row)
{
    PRE(row.size() == _column_widths.size());
    _rows.push_back(row);

    for (table_row::size_type i = 0; i < row.size(); ++i)
        if (_column_widths[i] < row[i].length())
            _column_widths[i] = row[i].length();
}


/// Gets an iterator pointing to the beginning of the rows of the table.
///
/// \return An iterator on the rows.
text::table::const_iterator
text::table::begin(void) const
{
    return _rows.begin();
}


/// Gets an iterator pointing to the end of the rows of the table.
///
/// \return An iterator on the rows.
text::table::const_iterator
text::table::end(void) const
{
    return _rows.end();
}


/// Column width to denote that the column has to fit all of its cells.
const std::size_t text::table_formatter::width_auto = 0;


/// Column width to denote that the column can be refilled to fit the table.
const std::size_t text::table_formatter::width_refill =
    std::numeric_limits< std::size_t >::max();


/// Constructs a new table formatter.
text::table_formatter::table_formatter(void) :
    _separator(""),
    _table_width(0)
{
}


/// Sets the width of a column.
///
/// All columns except one must have a width that is, at least, as wide as the
/// widest cell in the column.  One of the columns can have a width of
/// width_refill, which indicates that the column will be refilled if the table
/// does not fit in its maximum width.
///
/// \param column The index of the column to set the width for.
/// \param width The width to set the column to.
///
/// \return A reference to this formatter to allow using the builder pattern.
text::table_formatter&
text::table_formatter::set_column_width(const table_row::size_type column,
                                        const std::size_t width)
{
#if !defined(NDEBUG)
    if (width == width_refill) {
        for (widths_vector::size_type i = 0; i < _column_widths.size(); i++) {
            if (i != column)
                PRE_MSG(_column_widths[i] != width_refill,
                        "Only one column width can be set to width_refill");
        }
    }
#endif

    if (_column_widths.size() < column + 1)
        _column_widths.resize(column + 1, width_auto);
    _column_widths[column] = width;
    return *this;
}


/// Sets the separator to use between the cells.
///
/// \param separator The separator to use.
///
/// \return A reference to this formatter to allow using the builder pattern.
text::table_formatter&
text::table_formatter::set_separator(const char* separator)
{
    _separator = separator;
    return *this;
}


/// Sets the maximum width of the table.
///
/// \param table_width The maximum width of the table; cannot be zero.
///
/// \return A reference to this formatter to allow using the builder pattern.
text::table_formatter&
text::table_formatter::set_table_width(const std::size_t table_width)
{
    PRE(table_width > 0);
    _table_width = table_width;
    return *this;
}


/// Formats a table into a collection of textual lines.
///
/// \param t Table to format.
///
/// \return A collection of textual lines.
std::vector< std::string >
text::table_formatter::format(const table& t) const
{
    std::vector< std::string > lines;

    if (!t.empty()) {
        widths_vector widths = override_column_widths(t, _column_widths);
        if (_table_width != 0)
            refill_widths(widths, _table_width, _separator.length());

        for (table::const_iterator iter = t.begin(); iter != t.end(); ++iter) {
            const std::vector< std::string > sublines =
                format_row(*iter, widths, _separator);
            std::copy(sublines.begin(), sublines.end(),
                      std::back_inserter(lines));
        }
    }

    return lines;
}
