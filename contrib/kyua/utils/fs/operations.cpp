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

#include "utils/fs/operations.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <sys/param.h>
#if defined(HAVE_SYS_MOUNT_H)
#   include <sys/mount.h>
#endif
#include <sys/stat.h>
#if defined(HAVE_SYS_STATVFS_H) && defined(HAVE_STATVFS)
#   include <sys/statvfs.h>
#endif
#if defined(HAVE_SYS_VFS_H)
#   include <sys/vfs.h>
#endif
#include <sys/wait.h>

#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "utils/auto_array.ipp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/directory.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/units.hpp"

namespace fs = utils::fs;
namespace units = utils::units;

using utils::optional;


namespace {


/// Operating systems recognized by the code below.
enum os_type {
    os_unsupported = 0,
    os_freebsd,
    os_linux,
    os_netbsd,
    os_sunos,
};


/// The current operating system.
static enum os_type current_os =
#if defined(__FreeBSD__)
    os_freebsd
#elif defined(__linux__)
    os_linux
#elif defined(__NetBSD__)
    os_netbsd
#elif defined(__SunOS__)
    os_sunos
#else
    os_unsupported
#endif
    ;


/// Specifies if a real unmount(2) is available.
///
/// We use this as a constant instead of a macro so that we can compile both
/// versions of the unmount code unconditionally.  This is a way to prevent
/// compilation bugs going unnoticed for long.
static const bool have_unmount2 =
#if defined(HAVE_UNMOUNT)
    true;
#else
    false;
#endif


#if !defined(UMOUNT)
/// Fake replacement value to the path to umount(8).
#   define UMOUNT "do-not-use-this-value"
#else
#   if defined(HAVE_UNMOUNT)
#       error "umount(8) detected when unmount(2) is also available"
#   endif
#endif


#if !defined(HAVE_UNMOUNT)
/// Fake unmount(2) function for systems without it.
///
/// This is only provided to allow our code to compile in all platforms
/// regardless of whether they actually have an unmount(2) or not.
///
/// \return -1 to indicate error, although this should never happen.
static int
unmount(const char* /* path */,
        const int /* flags */)
{
    PRE(false);
    return -1;
}
#endif


/// Error code returned by subprocess to indicate a controlled failure.
const int exit_known_error = 123;


static void run_mount_tmpfs(const fs::path&, const uint64_t) UTILS_NORETURN;


/// Executes 'mount -t tmpfs' (or a similar variant).
///
/// This function must be called from a subprocess as it never returns.
///
/// \param mount_point Location on which to mount a tmpfs.
/// \param size The size of the tmpfs to mount.  If 0, use unlimited.
static void
run_mount_tmpfs(const fs::path& mount_point, const uint64_t size)
{
    const char* mount_args[16];
    std::string size_arg;

    std::size_t last = 0;
    switch (current_os) {
    case os_freebsd:
        mount_args[last++] = "mount";
        mount_args[last++] = "-ttmpfs";
        if (size > 0) {
            size_arg = F("-osize=%s") % size;
            mount_args[last++] = size_arg.c_str();
        }
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point.c_str();
        break;

    case os_linux:
        mount_args[last++] = "mount";
        mount_args[last++] = "-ttmpfs";
        if (size > 0) {
            size_arg = F("-osize=%s") % size;
            mount_args[last++] = size_arg.c_str();
        }
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point.c_str();
        break;

    case os_netbsd:
        mount_args[last++] = "mount";
        mount_args[last++] = "-ttmpfs";
        if (size > 0) {
            size_arg = F("-o-s%s") % size;
            mount_args[last++] = size_arg.c_str();
        }
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point.c_str();
        break;

    case os_sunos:
        mount_args[last++] = "mount";
        mount_args[last++] = "-Ftmpfs";
        if (size > 0) {
            size_arg = F("-o-s%s") % size;
            mount_args[last++] = size_arg.c_str();
        }
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point.c_str();
        break;

    default:
        std::cerr << "Don't know how to mount a temporary file system in this "
            "host operating system\n";
        std::exit(exit_known_error);
    }
    mount_args[last] = NULL;

    const char** arg;
    std::cout << "Mounting tmpfs onto " << mount_point << " with:";
    for (arg = &mount_args[0]; *arg != NULL; arg++)
        std::cout << " " << *arg;
    std::cout << "\n";

    const int ret = ::execvp(mount_args[0],
                             UTILS_UNCONST(char* const, mount_args));
    INV(ret == -1);
    std::cerr << "Failed to exec " << mount_args[0] << "\n";
    std::exit(EXIT_FAILURE);
}


/// Unmounts a file system using unmount(2).
///
/// \pre unmount(2) must be available; i.e. have_unmount2 must be true.
///
/// \param mount_point The file system to unmount.
///
/// \throw fs::system_error If the call to unmount(2) fails.
static void
unmount_with_unmount2(const fs::path& mount_point)
{
    PRE(have_unmount2);

    if (::unmount(mount_point.c_str(), 0) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("unmount(%s) failed") % mount_point,
                               original_errno);
    }
}


/// Unmounts a file system using umount(8).
///
/// \pre umount(2) must not be available; i.e. have_unmount2 must be false.
///
/// \param mount_point The file system to unmount.
///
/// \throw fs::error If the execution of umount(8) fails.
static void
unmount_with_umount8(const fs::path& mount_point)
{
    PRE(!have_unmount2);

    const pid_t pid = ::fork();
    if (pid == -1) {
        const int original_errno = errno;
        throw fs::system_error("Cannot fork to execute unmount tool",
                               original_errno);
    } else if (pid == 0) {
        const int ret = ::execlp(UMOUNT, "umount", mount_point.c_str(), NULL);
        INV(ret == -1);
        std::cerr << "Failed to exec " UMOUNT "\n";
        std::exit(EXIT_FAILURE);
    }

    int status;
retry:
    if (::waitpid(pid, &status, 0) == -1) {
        const int original_errno = errno;
        if (errno == EINTR)
            goto retry;
        throw fs::system_error("Failed to wait for unmount subprocess",
                               original_errno);
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS)
            return;
        else
            throw fs::error(F("Failed to unmount %s; returned exit code %s")
                              % mount_point % WEXITSTATUS(status));
    } else
        throw fs::error(F("Failed to unmount %s; unmount tool received signal")
                        % mount_point);
}


/// Stats a file, without following links.
///
/// \param path The file to stat.
///
/// \return The stat structure on success.
///
/// \throw system_error An error on failure.
static struct ::stat
safe_stat(const fs::path& path)
{
    struct ::stat sb;
    if (::lstat(path.c_str(), &sb) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot get information about %s") % path,
                               original_errno);
    }
    return sb;
}


}  // anonymous namespace


/// Copies a file.
///
/// \param source The file to copy.
/// \param target The destination of the new copy; must be a file name, not a
///     directory.
///
/// \throw error If there is a problem copying the file.
void
fs::copy(const fs::path& source, const fs::path& target)
{
    std::ifstream input(source.c_str());
    if (!input)
        throw error(F("Cannot open copy source %s") % source);

    std::ofstream output(target.c_str());
    if (!output)
        throw error(F("Cannot create copy target %s") % target);

    char buffer[1024];
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        if (input.good() || input.eof())
            output.write(buffer, input.gcount());
    }
    if (!input.good() && !input.eof())
        throw error(F("Error while reading input file %s") % source);
}


/// Queries the path to the current directory.
///
/// \return The path to the current directory.
///
/// \throw fs::error If there is a problem querying the current directory.
fs::path
fs::current_path(void)
{
    char* cwd;
#if defined(HAVE_GETCWD_DYN)
    cwd = ::getcwd(NULL, 0);
#else
    cwd = ::getcwd(NULL, MAXPATHLEN);
#endif
    if (cwd == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to get current working directory"),
                               original_errno);
    }

    try {
        const fs::path result(cwd);
        std::free(cwd);
        return result;
    } catch (...) {
        std::free(cwd);
        throw;
    }
}


/// Checks if a file exists.
///
/// Be aware that this is racy in the same way as access(2) is.
///
/// \param path The file to check the existance of.
///
/// \return True if the file exists; false otherwise.
bool
fs::exists(const fs::path& path)
{
    return ::access(path.c_str(), F_OK) == 0;
}


/// Locates a file in the PATH.
///
/// \param name The file to locate.
///
/// \return The path to the located file or none if it was not found.  The
/// returned path is always absolute.
optional< fs::path >
fs::find_in_path(const char* name)
{
    const optional< std::string > current_path = utils::getenv("PATH");
    if (!current_path || current_path.get().empty())
        return none;

    std::istringstream path_input(current_path.get() + ":");
    std::string path_component;
    while (std::getline(path_input, path_component, ':').good()) {
        const fs::path candidate = path_component.empty() ?
            fs::path(name) : (fs::path(path_component) / name);
        if (exists(candidate)) {
            if (candidate.is_absolute())
                return utils::make_optional(candidate);
            else
                return utils::make_optional(candidate.to_absolute());
        }
    }
    return none;
}


/// Calculates the free space in a given file system.
///
/// \param path Path to a file in the file system for which to check the free
///     disk space.
///
/// \return The amount of free space usable by a non-root user.
///
/// \throw system_error If the call to statfs(2) fails.
utils::units::bytes
fs::free_disk_space(const fs::path& path)
{
#if defined(HAVE_STATVFS)
    struct ::statvfs buf;
    if (::statvfs(path.c_str(), &buf) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to stat file system for %s") % path,
                               original_errno);
    }
    return units::bytes(uint64_t(buf.f_bsize) * buf.f_bavail);
#elif defined(HAVE_STATFS)
    struct ::statfs buf;
    if (::statfs(path.c_str(), &buf) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to stat file system for %s") % path,
                               original_errno);
    }
    return units::bytes(uint64_t(buf.f_bsize) * buf.f_bavail);
#else
#   error "Don't know how to query free disk space"
#endif
}


/// Checks if the given path is a directory or not.
///
/// \return True if the path is a directory; false otherwise.
bool
fs::is_directory(const fs::path& path)
{
    const struct ::stat sb = safe_stat(path);
    return S_ISDIR(sb.st_mode);
}


/// Creates a directory.
///
/// \param dir The path to the directory to create.
/// \param mode The permissions for the new directory.
///
/// \throw system_error If the call to mkdir(2) fails.
void
fs::mkdir(const fs::path& dir, const int mode)
{
    if (::mkdir(dir.c_str(), static_cast< mode_t >(mode)) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to create directory %s") % dir,
                               original_errno);
    }
}


/// Creates a directory and any missing parents.
///
/// This is separate from the fs::mkdir function to clearly differentiate the
/// libc wrapper from the more complex algorithm implemented here.
///
/// \param dir The path to the directory to create.
/// \param mode The permissions for the new directories.
///
/// \throw system_error If any call to mkdir(2) fails.
void
fs::mkdir_p(const fs::path& dir, const int mode)
{
    try {
        fs::mkdir(dir, mode);
    } catch (const fs::system_error& e) {
        if (e.original_errno() == ENOENT) {
            fs::mkdir_p(dir.branch_path(), mode);
            fs::mkdir(dir, mode);
        } else if (e.original_errno() != EEXIST)
            throw e;
    }
}


/// Creates a temporary directory that is world readable/accessible.
///
/// The temporary directory is created using mkdtemp(3) using the provided
/// template.  This should be most likely used in conjunction with
/// fs::auto_directory.
///
/// The temporary directory is given read and execute permissions to everyone
/// and thus should not be used to protect data that may be subject to snooping.
/// This goes together with the assumption that this function is used to create
/// temporary directories for test cases, and that those test cases may
/// sometimes be executed as an unprivileged user.  In those cases, we need to
/// support two different things:
///
/// - Allow the unprivileged code to write to files in the work directory by
///   name (e.g. to write the results file, whose name is provided by the
///   monitor code running as root).  This requires us to grant search
///   permissions.
///
/// - Allow the test cases themselves to call getcwd(3) at any point.  At least
///   on NetBSD 7.x, getcwd(3) requires both read and search permissions on all
///   path components leading to the current directory.  This requires us to
///   grant both read and search permissions.
///
/// TODO(jmmv): A cleaner way to support this would be for the test executor to
/// create two work directory hierarchies directly rooted under TMPDIR: one for
/// root and one for the unprivileged user.  However, that requires more
/// bookkeeping for no real gain, because we are not really trying to protect
/// the data within our temporary directories against attacks.
///
/// \param path_template The template for the temporary path, which is a
///     basename that is created within the TMPDIR.  Must contain the XXXXXX
///     pattern, which is atomically replaced by a random unique string.
///
/// \return The generated path for the temporary directory.
///
/// \throw fs::system_error If the call to mkdtemp(3) fails.
fs::path
fs::mkdtemp_public(const std::string& path_template)
{
    PRE(path_template.find("XXXXXX") != std::string::npos);

    const fs::path tmpdir(utils::getenv_with_default("TMPDIR", "/tmp"));
    const fs::path full_template = tmpdir / path_template;

    utils::auto_array< char > buf(new char[full_template.str().length() + 1]);
    std::strcpy(buf.get(), full_template.c_str());
    if (::mkdtemp(buf.get()) == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot create temporary directory using "
                                 "template %s") % full_template,
                               original_errno);
    }
    const fs::path path(buf.get());

    if (::chmod(path.c_str(), 0755) == -1) {
        const int original_errno = errno;

        try {
            rmdir(path);
        } catch (const fs::system_error& e) {
            // This really should not fail.  We just created the directory and
            // have not written anything to it so there is no reason for this to
            // fail.  But better handle the failure just in case.
            LW(F("Failed to delete just-created temporary directory %s")
               % path);
        }

        throw fs::system_error(F("Failed to grant search permissions on "
                                 "temporary directory %s") % path,
                               original_errno);
    }

    return path;
}


/// Creates a temporary file.
///
/// The temporary file is created using mkstemp(3) using the provided template.
/// This should be most likely used in conjunction with fs::auto_file.
///
/// \param path_template The template for the temporary path, which is a
///     basename that is created within the TMPDIR.  Must contain the XXXXXX
///     pattern, which is atomically replaced by a random unique string.
///
/// \return The generated path for the temporary directory.
///
/// \throw fs::system_error If the call to mkstemp(3) fails.
fs::path
fs::mkstemp(const std::string& path_template)
{
    PRE(path_template.find("XXXXXX") != std::string::npos);

    const fs::path tmpdir(utils::getenv_with_default("TMPDIR", "/tmp"));
    const fs::path full_template = tmpdir / path_template;

    utils::auto_array< char > buf(new char[full_template.str().length() + 1]);
    std::strcpy(buf.get(), full_template.c_str());
    if (::mkstemp(buf.get()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot create temporary file using template "
                                 "%s") % full_template, original_errno);
    }
    return fs::path(buf.get());
}


/// Mounts a temporary file system with unlimited size.
///
/// \param in_mount_point The path on which the file system will be mounted.
///
/// \throw fs::system_error If the attempt to mount process fails.
/// \throw fs::unsupported_operation_error If the code does not know how to
///     mount a temporary file system in the current operating system.
void
fs::mount_tmpfs(const fs::path& in_mount_point)
{
    mount_tmpfs(in_mount_point, units::bytes());
}


/// Mounts a temporary file system.
///
/// \param in_mount_point The path on which the file system will be mounted.
/// \param size The size of the tmpfs to mount.  If 0, use unlimited.
///
/// \throw fs::system_error If the attempt to mount process fails.
/// \throw fs::unsupported_operation_error If the code does not know how to
///     mount a temporary file system in the current operating system.
void
fs::mount_tmpfs(const fs::path& in_mount_point, const units::bytes& size)
{
    // SunOS's mount(8) requires paths to be absolute.  To err on the side of
    // caution, let's make the mount point absolute in all cases.
    const fs::path mount_point = in_mount_point.is_absolute() ?
        in_mount_point : in_mount_point.to_absolute();

    const pid_t pid = ::fork();
    if (pid == -1) {
        const int original_errno = errno;
        throw fs::system_error("Cannot fork to execute mount tool",
                               original_errno);
    }
    if (pid == 0)
        run_mount_tmpfs(mount_point, size);

    int status;
retry:
    if (::waitpid(pid, &status, 0) == -1) {
        const int original_errno = errno;
        if (errno == EINTR)
            goto retry;
        throw fs::system_error("Failed to wait for mount subprocess",
                               original_errno);
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == exit_known_error)
            throw fs::unsupported_operation_error(
                "Don't know how to mount a tmpfs on this operating system");
        else if (WEXITSTATUS(status) == EXIT_SUCCESS)
            return;
        else
            throw fs::error(F("Failed to mount tmpfs on %s; returned exit "
                              "code %s") % mount_point % WEXITSTATUS(status));
    } else {
        throw fs::error(F("Failed to mount tmpfs on %s; mount tool "
                          "received signal") % mount_point);
    }
}


/// Recursively removes a directory.
///
/// This operation simulates a "rm -r".  No effort is made to forcibly delete
/// files and no attention is paid to mount points.
///
/// \param directory The directory to remove.
///
/// \throw fs::error If there is a problem removing any directory or file.
void
fs::rm_r(const fs::path& directory)
{
    const fs::directory dir(directory);

    ::chmod(directory.c_str(), 0700);
    for (fs::directory::const_iterator iter = dir.begin(); iter != dir.end();
         ++iter) {
        if (iter->name == "." || iter->name == "..")
            continue;

        const fs::path entry = directory / iter->name;

        if (fs::is_directory(entry)) {
            LD(F("Descending into %s") % entry);
            ::chmod(entry.c_str(), 0700);
            fs::rm_r(entry);
        } else {
            LD(F("Removing file %s") % entry);
            fs::unlink(entry);
        }
    }

    LD(F("Removing empty directory %s") % directory);
    fs::rmdir(directory);
}


/// Removes an empty directory.
///
/// \param file The directory to remove.
///
/// \throw fs::system_error If the call to rmdir(2) fails.
void
fs::rmdir(const path& file)
{
    if (::rmdir(file.c_str()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Removal of %s failed") % file,
                               original_errno);
    }
}


/// Obtains all the entries in a directory.
///
/// \param path The directory to scan.
///
/// \return The set of all directory entries in the given directory.
///
/// \throw fs::system_error If reading the directory fails for any reason.
std::set< fs::directory_entry >
fs::scan_directory(const fs::path& path)
{
    std::set< fs::directory_entry > contents;

    fs::directory dir(path);
    for (fs::directory::const_iterator iter = dir.begin(); iter != dir.end();
         ++iter) {
        contents.insert(*iter);
    }

    return contents;
}


/// Removes a file.
///
/// \param file The file to remove.
///
/// \throw fs::system_error If the call to unlink(2) fails.
void
fs::unlink(const path& file)
{
    if (::unlink(file.c_str()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Removal of %s failed") % file,
                               original_errno);
    }
}


/// Unmounts a file system.
///
/// \param in_mount_point The file system to unmount.
///
/// \throw fs::error If the unmount fails.
void
fs::unmount(const fs::path& in_mount_point)
{
    // FreeBSD's unmount(2) requires paths to be absolute.  To err on the side
    // of caution, let's make it absolute in all cases.
    const fs::path mount_point = in_mount_point.is_absolute() ?
        in_mount_point : in_mount_point.to_absolute();

    static const int unmount_retries = 3;
    static const int unmount_retry_delay_seconds = 1;

    int retries = unmount_retries;
retry:
    try {
        if (have_unmount2) {
            unmount_with_unmount2(mount_point);
        } else {
            unmount_with_umount8(mount_point);
        }
    } catch (const fs::system_error& error) {
        if (error.original_errno() == EBUSY && retries > 0) {
            LW(F("%s busy; unmount retries left %s") % mount_point % retries);
            retries--;
            ::sleep(unmount_retry_delay_seconds);
            goto retry;
        }
        throw;
    }
}
