// Copyright 2015 The Kyua Authors.
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

#include "utils/fs/directory.hpp"

extern "C" {
#include <sys/types.h>

#include <dirent.h>
}

#include <cerrno>
#include <memory>

#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace detail = utils::fs::detail;
namespace fs = utils::fs;
namespace text = utils::text;


/// Constructs a new directory entry.
///
/// \param name_ Name of the directory entry.
fs::directory_entry::directory_entry(const std::string& name_) : name(name_)
{
}


/// Checks if two directory entries are equal.
///
/// \param other The entry to compare to.
///
/// \return True if the two entries are equal; false otherwise.
bool
fs::directory_entry::operator==(const directory_entry& other) const
{
    return name == other.name;
}


/// Checks if two directory entries are different.
///
/// \param other The entry to compare to.
///
/// \return True if the two entries are different; false otherwise.
bool
fs::directory_entry::operator!=(const directory_entry& other) const
{
    return !(*this == other);
}


/// Checks if this entry sorts before another entry.
///
/// \param other The entry to compare to.
///
/// \return True if this entry sorts before the other entry; false otherwise.
bool
fs::directory_entry::operator<(const directory_entry& other) const
{
    return name < other.name;
}


/// Formats a directory entry.
///
/// \param output Stream into which to inject the formatted entry.
/// \param entry The entry to format.
///
/// \return A reference to output.
std::ostream&
fs::operator<<(std::ostream& output, const directory_entry& entry)
{
    output << F("directory_entry{name=%s}") % text::quote(entry.name, '\'');
    return output;
}


/// Internal implementation details for the directory_iterator.
///
/// In order to support multiple concurrent iterators over the same directory
/// object, this class is the one that performs all directory-level accesses.
/// In particular, even if it may seem surprising, this is the class that
/// handles the DIR object for the directory.
///
/// Note that iterators implemented by this class do not rely on the container
/// directory class at all.  This should not be relied on for object lifecycle
/// purposes.
struct utils::fs::detail::directory_iterator::impl : utils::noncopyable {
    /// Path of the directory accessed by this iterator.
    const fs::path _path;

    /// Raw pointer to the system representation of the directory.
    ///
    /// We also use this to determine if the iterator is valid (at the end) or
    /// not.  A null pointer means an invalid iterator.
    ::DIR* _dirp;

    /// Custom representation of the directory entry.
    ///
    /// We must keep this as a pointer so that we can support the common
    /// operators (* and ->) over iterators.
    std::unique_ptr< directory_entry > _entry;

    /// Constructs an iterator pointing to the "end" of the directory.
    impl(void) : _path("invalid-directory-entry"), _dirp(NULL)
    {
    }

    /// Constructs a new iterator to start scanning a directory.
    ///
    /// \param path The directory that will be scanned.
    ///
    /// \throw system_error If there is a problem opening the directory.
    explicit impl(const path& path) : _path(path)
    {
        DIR* dirp = ::opendir(_path.c_str());
        if (dirp == NULL) {
            const int original_errno = errno;
            throw fs::system_error(F("opendir(%s) failed") % _path,
                                   original_errno);
        }
        _dirp = dirp;

        // Initialize our first directory entry.  Note that this may actually
        // close the directory we just opened if the directory happens to be
        // empty -- but directories are never empty because they at least have
        // '.' and '..' entries.
        next();
    }

    /// Destructor.
    ///
    /// This closes the directory if still open.
    ~impl(void)
    {
        if (_dirp != NULL)
            close();
    }

    /// Closes the directory and invalidates the iterator.
    void
    close(void)
    {
        PRE(_dirp != NULL);
        if (::closedir(_dirp) == -1) {
            UNREACHABLE_MSG("Invalid dirp provided to closedir(3)");
        }
        _dirp = NULL;
    }

    /// Advances the directory entry to the next one.
    ///
    /// It is possible to use this function on a new directory_entry object to
    /// initialize the first entry.
    ///
    /// \throw system_error If the call to readdir fails.
    void
    next(void)
    {
        ::dirent* result;

        errno = 0;
        if ((result = ::readdir(_dirp)) == NULL && errno != 0) {
            const int original_errno = errno;
            throw fs::system_error(F("readdir(%s) failed") % _path,
                                   original_errno);
        }
        if (result == NULL) {
            _entry.reset();
            close();
        } else {
            _entry.reset(new directory_entry(result->d_name));
        }
    }
};


/// Constructs a new directory iterator.
///
/// \param pimpl The constructed internal implementation structure to use.
detail::directory_iterator::directory_iterator(std::shared_ptr< impl > pimpl) :
    _pimpl(pimpl)
{
}


/// Destructor.
detail::directory_iterator::~directory_iterator(void)
{
}


/// Creates a new directory iterator for a directory.
///
/// \return The directory iterator.  Note that the result may be invalid.
///
/// \throw system_error If opening the directory or reading its first entry
///     fails.
detail::directory_iterator
detail::directory_iterator::new_begin(const path& path)
{
    return directory_iterator(std::shared_ptr< impl >(new impl(path)));
}


/// Creates a new invalid directory iterator.
///
/// \return The invalid directory iterator.
detail::directory_iterator
detail::directory_iterator::new_end(void)
{
    return directory_iterator(std::shared_ptr< impl >(new impl()));
}


/// Checks if two iterators are equal.
///
/// We consider two iterators to be equal if both of them are invalid or,
/// otherwise, if they have the exact same internal representation (as given by
/// equality of the pimpl pointers).
///
/// \param other The object to compare to.
///
/// \return True if the two iterators are equal; false otherwise.
bool
detail::directory_iterator::operator==(const directory_iterator& other) const
{
    return (_pimpl->_dirp == NULL && other._pimpl->_dirp == NULL) ||
        _pimpl == other._pimpl;
}


/// Checks if two iterators are different.
///
/// \param other The object to compare to.
///
/// \return True if the two iterators are different; false otherwise.
bool
detail::directory_iterator::operator!=(const directory_iterator& other) const
{
    return !(*this == other);
}


/// Moves the iterator one element forward.
///
/// \return A reference to the iterator.
///
/// \throw system_error If advancing the iterator fails.
detail::directory_iterator&
detail::directory_iterator::operator++(void)
{
    _pimpl->next();
    return *this;
}


/// Dereferences the iterator to its contents.
///
/// \return A reference to the directory entry pointed to by the iterator.
const fs::directory_entry&
detail::directory_iterator::operator*(void) const
{
    PRE(_pimpl->_entry.get() != NULL);
    return *_pimpl->_entry;
}


/// Dereferences the iterator to its contents.
///
/// \return A pointer to the directory entry pointed to by the iterator.
const fs::directory_entry*
detail::directory_iterator::operator->(void) const
{
    PRE(_pimpl->_entry.get() != NULL);
    return _pimpl->_entry.get();
}


/// Internal implementation details for the directory.
struct utils::fs::directory::impl : utils::noncopyable {
    /// Path to the directory to scan.
    fs::path _path;

    /// Constructs a new directory.
    ///
    /// \param path_ Path to the directory to scan.
    impl(const fs::path& path_) : _path(path_)
    {
    }
};


/// Constructs a new directory.
///
/// \param path_ Path to the directory to scan.
fs::directory::directory(const path& path_) : _pimpl(new impl(path_))
{
}


/// Returns an iterator to start scanning the directory.
///
/// \return An iterator on the directory.
///
/// \throw system_error If the directory cannot be opened to obtain its first
///     entry.
fs::directory::const_iterator
fs::directory::begin(void) const
{
    return const_iterator::new_begin(_pimpl->_path);
}


/// Returns an invalid iterator to check for the end of an scan.
///
/// \return An invalid iterator.
fs::directory::const_iterator
fs::directory::end(void) const
{
    return const_iterator::new_end();
}
