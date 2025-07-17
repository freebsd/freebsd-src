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

#include "utils/text/templates.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stack>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace text = utils::text;


namespace {


/// Definition of a template statement.
///
/// A template statement is a particular line in the input file that is
/// preceeded by a template marker.  This class provides a high-level
/// representation of the contents of such statement and a mechanism to parse
/// the textual line into this high-level representation.
class statement_def {
public:
    /// Types of the known statements.
    enum statement_type {
        /// Alternative clause of a conditional.
        ///
        /// Takes no arguments.
        type_else,

        /// End of conditional marker.
        ///
        /// Takes no arguments.
        type_endif,

        /// End of loop marker.
        ///
        /// Takes no arguments.
        type_endloop,

        /// Beginning of a conditional.
        ///
        /// Takes a single argument, which denotes the name of the variable or
        /// vector to check for existence.  This is the only expression
        /// supported.
        type_if,

        /// Beginning of a loop over all the elements of a vector.
        ///
        /// Takes two arguments: the name of the vector over which to iterate
        /// and the name of the iterator to later index this vector.
        type_loop,
    };

private:
    /// Internal data describing the structure of a particular statement type.
    struct type_descriptor {
        /// The native type of the statement.
        statement_type type;

        /// The expected number of arguments.
        unsigned int n_arguments;

        /// Constructs a new type descriptor.
        ///
        /// \param type_ The native type of the statement.
        /// \param n_arguments_ The expected number of arguments.
        type_descriptor(const statement_type type_,
                        const unsigned int n_arguments_)
            : type(type_), n_arguments(n_arguments_)
        {
        }
    };

    /// Mapping of statement type names to their definitions.
    typedef std::map< std::string, type_descriptor > types_map;

    /// Description of the different statement types.
    ///
    /// This static map is initialized once and reused later for any statement
    /// lookup.  Unfortunately, we cannot perform this initialization in a
    /// static manner without C++11.
    static types_map _types;

    /// Generates a new types definition map.
    ///
    /// \return A new types definition map, to be assigned to _types.
    static types_map
    generate_types_map(void)
    {
        // If you change this, please edit the comments in the enum above.
        types_map types;
        types.insert(types_map::value_type(
            "else", type_descriptor(type_else, 0)));
        types.insert(types_map::value_type(
            "endif", type_descriptor(type_endif, 0)));
        types.insert(types_map::value_type(
            "endloop", type_descriptor(type_endloop, 0)));
        types.insert(types_map::value_type(
            "if", type_descriptor(type_if, 1)));
        types.insert(types_map::value_type(
            "loop", type_descriptor(type_loop, 2)));
        return types;
    }

public:
    /// The type of the statement.
    statement_type type;

    /// The arguments to the statement, in textual form.
    const std::vector< std::string > arguments;

    /// Creates a new statement.
    ///
    /// \param type_ The type of the statement.
    /// \param arguments_ The arguments to the statement.
    statement_def(const statement_type& type_,
                  const std::vector< std::string >& arguments_) :
        type(type_), arguments(arguments_)
    {
#if !defined(NDEBUG)
        for (types_map::const_iterator iter = _types.begin();
             iter != _types.end(); ++iter) {
            const type_descriptor& descriptor = (*iter).second;
            if (descriptor.type == type_) {
                PRE(descriptor.n_arguments == arguments_.size());
                return;
            }
        }
        UNREACHABLE;
#endif
    }

    /// Parses a statement.
    ///
    /// \param line The textual representation of the statement without any
    ///     prefix.
    ///
    /// \return The parsed statement.
    ///
    /// \throw text::syntax_error If the statement is not correctly defined.
    static statement_def
    parse(const std::string& line)
    {
        if (_types.empty())
            _types = generate_types_map();

        const std::vector< std::string > words = text::split(line, ' ');
        if (words.empty())
            throw text::syntax_error("Empty statement");

        const types_map::const_iterator iter = _types.find(words[0]);
        if (iter == _types.end())
            throw text::syntax_error(F("Unknown statement '%s'") % words[0]);
        const type_descriptor& descriptor = (*iter).second;

        if (words.size() - 1 != descriptor.n_arguments)
            throw text::syntax_error(F("Invalid number of arguments for "
                                       "statement '%s'") % words[0]);

        std::vector< std::string > new_arguments;
        new_arguments.resize(words.size() - 1);
        std::copy(words.begin() + 1, words.end(), new_arguments.begin());

        return statement_def(descriptor.type, new_arguments);
    }
};


statement_def::types_map statement_def::_types;


/// Definition of a loop.
///
/// This simple structure is used to keep track of the parameters of a loop.
struct loop_def {
    /// The name of the vector over which this loop is iterating.
    std::string vector;

    /// The name of the iterator defined by this loop.
    std::string iterator;

    /// Position in the input to which to rewind to on looping.
    ///
    /// This position points to the line after the loop statement, not the loop
    /// itself.  This is one of the reasons why we have this structure, so that
    /// we can maintain the data about the loop without having to re-process it.
    std::istream::pos_type position;

    /// Constructs a new loop definition.
    ///
    /// \param vector_ The name of the vector (first argument).
    /// \param iterator_ The name of the iterator (second argumnet).
    /// \param position_ Position of the next line after the loop statement.
    loop_def(const std::string& vector_, const std::string& iterator_,
             const std::istream::pos_type position_) :
        vector(vector_), iterator(iterator_), position(position_)
    {
    }
};


/// Stateful class to instantiate the templates in an input stream.
///
/// The goal of this parser is to scan the input once and not buffer anything in
/// memory.  The only exception are loops: loops are reinterpreted on every
/// iteration from the same input file by rewidining the stream to the
/// appropriate position.
class templates_parser : utils::noncopyable {
    /// The templates to apply.
    ///
    /// Note that this is not const because the parser has to have write access
    /// to the templates.  In particular, it needs to be able to define the
    /// iterators as regular variables.
    text::templates_def _templates;

    /// Prefix that marks a line as a statement.
    const std::string _prefix;

    /// Delimiter to surround an expression instantiation.
    const std::string _delimiter;

    /// Whether to skip incoming lines or not.
    ///
    /// The top of the stack is true whenever we encounter a conditional that
    /// evaluates to false or a loop that does not have any iterations left.
    /// Under these circumstances, we need to continue scanning the input stream
    /// until we find the matching closing endif or endloop construct.
    ///
    /// This is a stack rather than a plain boolean to allow us deal with
    /// if-else clauses.
    std::stack< bool > _skip;

    /// Current count of nested conditionals.
    unsigned int _if_level;

    /// Level of the top-most conditional that evaluated to false.
    unsigned int _exit_if_level;

    /// Current count of nested loops.
    unsigned int _loop_level;

    /// Level of the top-most loop that does not have any iterations left.
    unsigned int _exit_loop_level;

    /// Information about all the nested loops up to the current point.
    std::stack< loop_def > _loops;

    /// Checks if a line is a statement or not.
    ///
    /// \param line The line to validate.
    ///
    /// \return True if the line looks like a statement, which is determined by
    /// checking if the line starts by the predefined prefix.
    bool
    is_statement(const std::string& line)
    {
        return ((line.length() >= _prefix.length() &&
                 line.substr(0, _prefix.length()) == _prefix) &&
                (line.length() < _delimiter.length() ||
                 line.substr(0, _delimiter.length()) != _delimiter));
    }

    /// Parses a given statement line into a statement definition.
    ///
    /// \param line The line to validate; it must be a valid statement.
    ///
    /// \return The parsed statement.
    ///
    /// \throw text::syntax_error If the input is not a valid statement.
    statement_def
    parse_statement(const std::string& line)
    {
        PRE(is_statement(line));
        return statement_def::parse(line.substr(_prefix.length()));
    }

    /// Processes a line from the input when not in skip mode.
    ///
    /// \param line The line to be processed.
    /// \param input The input stream from which the line was read.  The current
    ///     position in the stream must be after the line being processed.
    /// \param output The output stream into which to write the results.
    ///
    /// \throw text::syntax_error If the input is not valid.
    void
    handle_normal(const std::string& line, std::istream& input,
                  std::ostream& output)
    {
        if (!is_statement(line)) {
            // Fast path.  Mostly to avoid an indentation level for the big
            // chunk of code below.
            output << line << '\n';
            return;
        }

        const statement_def statement = parse_statement(line);

        switch (statement.type) {
        case statement_def::type_else:
            _skip.top() = !_skip.top();
            break;

        case statement_def::type_endif:
            _if_level--;
            break;

        case statement_def::type_endloop: {
            PRE(_loops.size() == _loop_level);
            loop_def& loop = _loops.top();

            const std::size_t next_index = 1 + text::to_type< std::size_t >(
                _templates.get_variable(loop.iterator));

            if (next_index < _templates.get_vector(loop.vector).size()) {
                _templates.add_variable(loop.iterator, F("%s") % next_index);
                input.seekg(loop.position);
            } else {
                _loop_level--;
                _loops.pop();
                _templates.remove_variable(loop.iterator);
            }
        } break;

        case statement_def::type_if: {
            _if_level++;
            const std::string value = _templates.evaluate(
                statement.arguments[0]);
            if (value.empty() || value == "0" || value == "false") {
                _exit_if_level = _if_level;
                _skip.push(true);
            } else {
                _skip.push(false);
            }
        } break;

        case statement_def::type_loop: {
            _loop_level++;

            const loop_def loop(statement.arguments[0], statement.arguments[1],
                                input.tellg());
            if (_templates.get_vector(loop.vector).empty()) {
                _exit_loop_level = _loop_level;
                _skip.push(true);
            } else {
                _templates.add_variable(loop.iterator, "0");
                _loops.push(loop);
                _skip.push(false);
            }
        } break;
        }
    }

    /// Processes a line from the input when in skip mode.
    ///
    /// \param line The line to be processed.
    ///
    /// \throw text::syntax_error If the input is not valid.
    void
    handle_skip(const std::string& line)
    {
        PRE(_skip.top());

        if (!is_statement(line))
            return;

        const statement_def statement = parse_statement(line);
        switch (statement.type) {
        case statement_def::type_else:
            if (_exit_if_level == _if_level)
                _skip.top() = !_skip.top();
            break;

        case statement_def::type_endif:
            INV(_if_level >= _exit_if_level);
            if (_if_level == _exit_if_level)
                _skip.top() = false;
            _if_level--;
            _skip.pop();
            break;

        case statement_def::type_endloop:
            INV(_loop_level >= _exit_loop_level);
            if (_loop_level == _exit_loop_level)
                _skip.top() = false;
            _loop_level--;
            _skip.pop();
            break;

        case statement_def::type_if:
            _if_level++;
            _skip.push(true);
            break;

        case statement_def::type_loop:
            _loop_level++;
            _skip.push(true);
            break;

        default:
            break;
        }
    }

    /// Evaluates expressions on a given input line.
    ///
    /// An expression is surrounded by _delimiter on both sides.  We scan the
    /// string from left to right finding any expressions that may appear, yank
    /// them out and call templates_def::evaluate() to get their value.
    ///
    /// Lonely or unbalanced appearances of _delimiter on the input line are
    /// not considered an error, given that the user may actually want to supply
    /// that character sequence without being interpreted as a template.
    ///
    /// \param in_line The input line from which to evaluate expressions.
    ///
    /// \return The evaluated line.
    ///
    /// \throw text::syntax_error If the expressions in the line are malformed.
    std::string
    evaluate(const std::string& in_line)
    {
        std::string out_line;

        std::string::size_type last_pos = 0;
        while (last_pos != std::string::npos) {
            const std::string::size_type open_pos = in_line.find(
                _delimiter, last_pos);
            if (open_pos == std::string::npos) {
                out_line += in_line.substr(last_pos);
                last_pos = std::string::npos;
            } else {
                const std::string::size_type close_pos = in_line.find(
                    _delimiter, open_pos + _delimiter.length());
                if (close_pos == std::string::npos) {
                    out_line += in_line.substr(last_pos);
                    last_pos = std::string::npos;
                } else {
                    out_line += in_line.substr(last_pos, open_pos - last_pos);
                    out_line += _templates.evaluate(in_line.substr(
                        open_pos + _delimiter.length(),
                        close_pos - open_pos - _delimiter.length()));
                    last_pos = close_pos + _delimiter.length();
                }
            }
        }

        return out_line;
    }

public:
    /// Constructs a new template parser.
    ///
    /// \param templates_ The templates to apply to the processed file.
    /// \param prefix_ The prefix that identifies lines as statements.
    /// \param delimiter_ Delimiter to surround a variable instantiation.
    templates_parser(const text::templates_def& templates_,
                     const std::string& prefix_,
                     const std::string& delimiter_) :
        _templates(templates_),
        _prefix(prefix_),
        _delimiter(delimiter_),
        _if_level(0),
        _exit_if_level(0),
        _loop_level(0),
        _exit_loop_level(0)
    {
    }

    /// Applies the templates to a given input.
    ///
    /// \param input The stream to which to apply the templates.
    /// \param output The stream into which to write the results.
    ///
    /// \throw text::syntax_error If the input is not valid.  Note that the
    ///     is not guaranteed to be unmodified on exit if an error is
    ///     encountered.
    void
    instantiate(std::istream& input, std::ostream& output)
    {
        std::string line;
        while (std::getline(input, line).good()) {
            if (!_skip.empty() && _skip.top())
                handle_skip(line);
            else
                handle_normal(evaluate(line), input, output);
        }
    }
};


}  // anonymous namespace


/// Constructs an empty templates definition.
text::templates_def::templates_def(void)
{
}


/// Sets a string variable in the templates.
///
/// If the variable already exists, its value is replaced.  This behavior is
/// required to implement iterators, but client code should really not be
/// redefining variables.
///
/// \pre The variable must not already exist as a vector.
///
/// \param name The name of the variable to set.
/// \param value The value to set the given variable to.
void
text::templates_def::add_variable(const std::string& name,
                                  const std::string& value)
{
    PRE(_vectors.find(name) == _vectors.end());
    _variables[name] = value;
}


/// Unsets a string variable from the templates.
///
/// Client code has no reason to use this.  This is only required to implement
/// proper scoping of loop iterators.
///
/// \pre The variable must exist.
///
/// \param name The name of the variable to remove from the templates.
void
text::templates_def::remove_variable(const std::string& name)
{
    PRE(_variables.find(name) != _variables.end());
    _variables.erase(_variables.find(name));
}


/// Creates a new vector in the templates.
///
/// If the vector already exists, it is cleared.  Client code should really not
/// be redefining variables.
///
/// \pre The vector must not already exist as a variable.
///
/// \param name The name of the vector to set.
void
text::templates_def::add_vector(const std::string& name)
{
    PRE(_variables.find(name) == _variables.end());
    _vectors[name] = strings_vector();
}


/// Adds a value to an existing vector in the templates.
///
/// \pre name The vector must exist.
///
/// \param name The name of the vector to append the value to.
/// \param value The textual value to append to the vector.
void
text::templates_def::add_to_vector(const std::string& name,
                                   const std::string& value)
{
    PRE(_variables.find(name) == _variables.end());
    PRE(_vectors.find(name) != _vectors.end());
    _vectors[name].push_back(value);
}


/// Checks whether a given identifier exists as a variable or a vector.
///
/// This is used to implement the evaluation of conditions in if clauses.
///
/// \param name The name of the variable or vector.
///
/// \return True if the given name exists as a variable or a vector; false
/// otherwise.
bool
text::templates_def::exists(const std::string& name) const
{
    return (_variables.find(name) != _variables.end() ||
            _vectors.find(name) != _vectors.end());
}


/// Gets the value of a variable.
///
/// \param name The name of the variable.
///
/// \return The value of the requested variable.
///
/// \throw text::syntax_error If the variable does not exist.
const std::string&
text::templates_def::get_variable(const std::string& name) const
{
    const variables_map::const_iterator iter = _variables.find(name);
    if (iter == _variables.end())
        throw text::syntax_error(F("Unknown variable '%s'") % name);
    return (*iter).second;
}


/// Gets a vector.
///
/// \param name The name of the vector.
///
/// \return A reference to the requested vector.
///
/// \throw text::syntax_error If the vector does not exist.
const text::templates_def::strings_vector&
text::templates_def::get_vector(const std::string& name) const
{
    const vectors_map::const_iterator iter = _vectors.find(name);
    if (iter == _vectors.end())
        throw text::syntax_error(F("Unknown vector '%s'") % name);
    return (*iter).second;
}


/// Indexes a vector and gets the value.
///
/// \param name The name of the vector to index.
/// \param index_name The name of a variable representing the index to use.
///     This must be convertible to a natural.
///
/// \return The value of the vector at the given index.
///
/// \throw text::syntax_error If the vector does not existor if the index is out
///     of range.
const std::string&
text::templates_def::get_vector(const std::string& name,
                                const std::string& index_name) const
{
    const strings_vector& vector = get_vector(name);
    const std::string& index_str = get_variable(index_name);

    std::size_t index;
    try {
        index = text::to_type< std::size_t >(index_str);
    } catch (const text::syntax_error& e) {
        throw text::syntax_error(F("Index '%s' not an integer, value '%s'") %
                                 index_name % index_str);
    }
    if (index >= vector.size())
        throw text::syntax_error(F("Index '%s' out of range at position '%s'") %
                                 index_name % index);

    return vector[index];
}


/// Evaluates a expression using these templates.
///
/// An expression is a query on the current templates to fetch a particular
/// value.  The value is always returned as a string, as this is how templates
/// are internally stored.
///
/// \param expression The expression to evaluate.  This should not include any
///     of the delimiters used in the user input, as otherwise the expression
///     will not be evaluated properly.
///
/// \return The result of the expression evaluation as a string.
///
/// \throw text::syntax_error If there is any problem while evaluating the
///     expression.
std::string
text::templates_def::evaluate(const std::string& expression) const
{
    const std::string::size_type paren_open = expression.find('(');
    if (paren_open == std::string::npos) {
        return get_variable(expression);
    } else {
        const std::string::size_type paren_close = expression.find(
            ')', paren_open);
        if (paren_close == std::string::npos)
            throw text::syntax_error(F("Expected ')' in expression '%s')") %
                                     expression);
        if (paren_close != expression.length() - 1)
            throw text::syntax_error(F("Unexpected text found after ')' in "
                                       "expression '%s'") % expression);

        const std::string arg0 = expression.substr(0, paren_open);
        const std::string arg1 = expression.substr(
            paren_open + 1, paren_close - paren_open - 1);
        if (arg0 == "defined") {
            return exists(arg1) ? "true" : "false";
        } else if (arg0 == "length") {
            return F("%s") % get_vector(arg1).size();
        } else {
            return get_vector(arg0, arg1);
        }
    }
}


/// Applies a set of templates to an input stream.
///
/// \param templates The templates to use.
/// \param input The input to process.
/// \param output The stream to which to write the processed text.
///
/// \throw text::syntax_error If there is any problem processing the input.
void
text::instantiate(const templates_def& templates,
                  std::istream& input, std::ostream& output)
{
    templates_parser parser(templates, "%", "%%");
    parser.instantiate(input, output);
}


/// Applies a set of templates to an input file and writes an output file.
///
/// \param templates The templates to use.
/// \param input_file The path to the input to process.
/// \param output_file The path to the file into which to write the output.
///
/// \throw text::error If the input or output files cannot be opened.
/// \throw text::syntax_error If there is any problem processing the input.
void
text::instantiate(const templates_def& templates,
                  const fs::path& input_file, const fs::path& output_file)
{
    std::ifstream input(input_file.c_str());
    if (!input)
        throw text::error(F("Failed to open %s for read") % input_file);

    std::ofstream output(output_file.c_str());
    if (!output)
        throw text::error(F("Failed to open %s for write") % output_file);

    instantiate(templates, input, output);
}
