// Copyright 2010 The Kyua Authors.
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

#include "utils/fs/path.hpp"

#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;


namespace {


/// Normalizes an input string to a valid path.
///
/// A normalized path cannot have empty components; i.e. there can be at most
/// one consecutive separator (/).
///
/// \param in The string to normalize.
///
/// \return The normalized string, representing a path.
///
/// \throw utils::fs::invalid_path_error If the path is empty.
static std::string
normalize(const std::string& in)
{
    if (in.empty())
        throw fs::invalid_path_error(in, "Cannot be empty");

    std::string out;

    std::string::size_type pos = 0;
    do {
        const std::string::size_type next_pos = in.find('/', pos);

        const std::string component = in.substr(pos, next_pos - pos);
        if (!component.empty()) {
            if (pos == 0)
                out += component;
            else if (component != ".")
                out += "/" + component;
        }

        if (next_pos == std::string::npos)
            pos = next_pos;
        else
            pos = next_pos + 1;
    } while (pos != std::string::npos);

    return out.empty() ? "/" : out;
}


}  // anonymous namespace


/// Creates a new path object from a textual representation of a path.
///
/// \param text A valid representation of a path in textual form.
///
/// \throw utils::fs::invalid_path_error If the input text does not represent a
///     valid path.
fs::path::path(const std::string& text) :
    _repr(normalize(text))
{
}


/// Gets a view of the path as an array of characters.
///
/// \return A \code const char* \endcode representation for the object.
const char*
fs::path::c_str(void) const
{
    return _repr.c_str();
}


/// Gets a view of the path as a std::string.
///
/// \return A \code std::string& \endcode representation for the object.
const std::string&
fs::path::str(void) const
{
    return _repr;
}


/// Gets the branch path (directory name) of the path.
///
/// The branch path of a path with just one component (no separators) is ".".
///
/// \return A new path representing the branch path.
fs::path
fs::path::branch_path(void) const
{
    const std::string::size_type end_pos = _repr.rfind('/');
    if (end_pos == std::string::npos)
        return fs::path(".");
    else if (end_pos == 0)
        return fs::path("/");
    else
        return fs::path(_repr.substr(0, end_pos));
}


/// Gets the leaf name (base name) of the path.
///
/// \return A new string representing the leaf name.
std::string
fs::path::leaf_name(void) const
{
    const std::string::size_type beg_pos = _repr.rfind('/');

    if (beg_pos == std::string::npos)
        return _repr;
    else
        return _repr.substr(beg_pos + 1);
}


/// Converts a relative path in the current directory to an absolute path.
///
/// \pre The path is relative.
///
/// \return The absolute representation of the relative path.
fs::path
fs::path::to_absolute(void) const
{
    PRE(!is_absolute());
    return fs::current_path() / *this;
}


/// \return True if the representation of the path is absolute.
bool
fs::path::is_absolute(void) const
{
    return _repr[0] == '/';
}


/// Checks whether the path is a parent of another path.
///
/// A path is considered to be a parent of itself.
///
/// \return True if this path is a parent of p.
bool
fs::path::is_parent_of(path p) const
{
    do {
        if ((*this) == p)
            return true;
        p = p.branch_path();
    } while (p != fs::path(".") && p != fs::path("/"));
    return false;
}


/// Counts the number of components in the path.
///
/// \return The number of components.
int
fs::path::ncomponents(void) const
{
    int count = 0;
    if (_repr == "/")
        return 1;
    else {
        for (std::string::const_iterator iter = _repr.begin();
             iter != _repr.end(); ++iter) {
            if (*iter == '/')
                count++;
        }
        return count + 1;
    }
}


/// Less-than comparator for paths.
///
/// This is provided to make identifiers useful as map keys.
///
/// \param p The path to compare to.
///
/// \return True if this identifier sorts before the other identifier; false
///     otherwise.
bool
fs::path::operator<(const fs::path& p) const
{
    return _repr < p._repr;
}


/// Compares two paths for equality.
///
/// Given that the paths are internally normalized, input paths such as
/// ///foo/bar and /foo///bar are exactly the same.  However, this does NOT
/// check for true equality: i.e. this does not access the file system to check
/// if the paths actually point to the same object my means of links.
///
/// \param p The path to compare to.
///
/// \returns A boolean indicating whether the paths are equal.
bool
fs::path::operator==(const fs::path& p) const
{
    return _repr == p._repr;
}


/// Compares two paths for inequality.
///
/// See the description of operator==() for more details on the comparison
/// performed.
///
/// \param p The path to compare to.
///
/// \returns A boolean indicating whether the paths are different.
bool
fs::path::operator!=(const fs::path& p) const
{
    return _repr != p._repr;
}


/// Concatenates this path with one or more components.
///
/// \param components The new components to concatenate to the path.  These are
///     normalized because, in general, they may come from user input.  These
///     components cannot represent an absolute path.
///
/// \return A new path containing the concatenation of this path and the
///     provided components.
///
/// \throw utils::fs::invalid_path_error If components does not represent a
///     valid path.
/// \throw utils::fs::join_error If the join operation is invalid because the
///     two paths are incompatible.
fs::path
fs::path::operator/(const std::string& components) const
{
    return (*this) / fs::path(components);
}


/// Concatenates this path with another path.
///
/// \param rest The path to concatenate to this one.  Cannot be absolute.
///
/// \return A new path containing the concatenation of this path and the other
///     path.
///
/// \throw utils::fs::join_error If the join operation is invalid because the
///     two paths are incompatible.
fs::path
fs::path::operator/(const fs::path& rest) const
{
    if (rest.is_absolute())
        throw fs::join_error(_repr, rest._repr,
                             "Cannot concatenate a path to an absolute path");
    return fs::path(_repr + '/' + rest._repr);
}


/// Formats a path for insertion on a stream.
///
/// \param os The output stream.
/// \param p The path to inject to the stream.
///
/// \return The output stream os.
std::ostream&
fs::operator<<(std::ostream& os, const fs::path& p)
{
    return (os << p.str());
}
