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

#include "utils/fs/auto_cleaners.hpp"

#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/interrupts.hpp"

namespace fs = utils::fs;
namespace signals = utils::signals;


/// Shared implementation of the auto_directory.
struct utils::fs::auto_directory::impl : utils::noncopyable {
    /// The path to the directory being managed.
    fs::path _directory;

    /// Whether cleanup() has been already executed or not.
    bool _cleaned;

    /// Constructor.
    ///
    /// \param directory_ The directory to grab the ownership of.
    impl(const path& directory_) :
        _directory(directory_),
        _cleaned(false)
    {
    }

    /// Destructor.
    ~impl(void)
    {
        try {
            this->cleanup();
        } catch (const fs::error& e) {
            LW(F("Failed to auto-cleanup directory '%s': %s") % _directory %
               e.what());
        }
    }

    /// Removes the directory.
    ///
    /// See the cleanup() method of the auto_directory class for details.
    void
    cleanup(void)
    {
        if (!_cleaned) {
            // Mark this as cleaned first so that, in case of failure, we don't
            // reraise the error from the destructor.
            _cleaned = true;

            fs::rmdir(_directory);
        }
    }
};


/// Constructs a new auto_directory and grabs ownership of a directory.
///
/// \param directory_ The directory to grab the ownership of.
fs::auto_directory::auto_directory(const path& directory_) :
    _pimpl(new impl(directory_))
{
}


/// Deletes the managed directory; must be empty.
///
/// This should not be relied on because it cannot provide proper error
/// reporting.  Instead, the caller should use the cleanup() method.
fs::auto_directory::~auto_directory(void)
{
}


/// Creates a self-destructing temporary directory.
///
/// See the notes for fs::mkdtemp_public() for details on the permissions
/// given to the temporary directory, which are looser than what the standard
/// mkdtemp would grant.
///
/// \param path_template The template for the temporary path, which is a
///     basename that is created within the TMPDIR.  Must contain the XXXXXX
///     pattern, which is atomically replaced by a random unique string.
///
/// \return The self-destructing directory.
///
/// \throw fs::error If the creation fails.
fs::auto_directory
fs::auto_directory::mkdtemp_public(const std::string& path_template)
{
    signals::interrupts_inhibiter inhibiter;
    const fs::path directory_ = fs::mkdtemp_public(path_template);
    try {
        return auto_directory(directory_);
    } catch (...) {
        fs::rmdir(directory_);
        throw;
    }
}


/// Gets the directory managed by this auto_directory.
///
/// \return The path to the managed directory.
const fs::path&
fs::auto_directory::directory(void) const
{
    return _pimpl->_directory;
}


/// Deletes the managed directory; must be empty.
///
/// This operation is idempotent.
///
/// \throw fs::error If there is a problem removing any directory or file.
void
fs::auto_directory::cleanup(void)
{
    _pimpl->cleanup();
}


/// Shared implementation of the auto_file.
struct utils::fs::auto_file::impl : utils::noncopyable {
    /// The path to the file being managed.
    fs::path _file;

    /// Whether removed() has been already executed or not.
    bool _removed;

    /// Constructor.
    ///
    /// \param file_ The file to grab the ownership of.
    impl(const path& file_) :
        _file(file_),
        _removed(false)
    {
    }

    /// Destructor.
    ~impl(void)
    {
        try {
            this->remove();
        } catch (const fs::error& e) {
            LW(F("Failed to auto-cleanup file '%s': %s") % _file %
               e.what());
        }
    }

    /// Removes the file.
    ///
    /// See the remove() method of the auto_file class for details.
    void
    remove(void)
    {
        if (!_removed) {
            // Mark this as cleaned first so that, in case of failure, we don't
            // reraise the error from the destructor.
            _removed = true;

            fs::unlink(_file);
        }
    }
};


/// Constructs a new auto_file and grabs ownership of a file.
///
/// \param file_ The file to grab the ownership of.
fs::auto_file::auto_file(const path& file_) :
    _pimpl(new impl(file_))
{
}


/// Deletes the managed file.
///
/// This should not be relied on because it cannot provide proper error
/// reporting.  Instead, the caller should use the remove() method.
fs::auto_file::~auto_file(void)
{
}


/// Creates a self-destructing temporary file.
///
/// \param path_template The template for the temporary path, which is a
///     basename that is created within the TMPDIR.  Must contain the XXXXXX
///     pattern, which is atomically replaced by a random unique string.
///
/// \return The self-destructing file.
///
/// \throw fs::error If the creation fails.
fs::auto_file
fs::auto_file::mkstemp(const std::string& path_template)
{
    signals::interrupts_inhibiter inhibiter;
    const fs::path file_ = fs::mkstemp(path_template);
    try {
        return auto_file(file_);
    } catch (...) {
        fs::unlink(file_);
        throw;
    }
}


/// Gets the file managed by this auto_file.
///
/// \return The path to the managed file.
const fs::path&
fs::auto_file::file(void) const
{
    return _pimpl->_file;
}


/// Deletes the managed file.
///
/// This operation is idempotent.
///
/// \throw fs::error If there is a problem removing the file.
void
fs::auto_file::remove(void)
{
    _pimpl->remove();
}
