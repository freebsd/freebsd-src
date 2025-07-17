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

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/directory.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/stream.hpp"
#include "utils/units.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace units = utils::units;

using utils::optional;


namespace {


/// Checks if a directory entry exists and matches a specific type.
///
/// \param dir The directory in which to look for the entry.
/// \param name The name of the entry to look up.
/// \param expected_type The expected type of the file as given by dir(5).
///
/// \return True if the entry exists and matches the given type; false
/// otherwise.
static bool
lookup(const char* dir, const char* name, const unsigned int expected_type)
{
    DIR* dirp = ::opendir(dir);
    ATF_REQUIRE(dirp != NULL);

    bool found = false;
    struct dirent* dp;
    while (!found && (dp = readdir(dirp)) != NULL) {
        if (std::strcmp(dp->d_name, name) == 0) {
            struct ::stat s;
            const fs::path lookup_path = fs::path(dir) / name;
            ATF_REQUIRE(::stat(lookup_path.c_str(), &s) != -1);
            if ((s.st_mode & S_IFMT) == expected_type) {
                found = true;
            }
        }
    }
    ::closedir(dirp);
    return found;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(copy__ok);
ATF_TEST_CASE_BODY(copy__ok)
{
    const fs::path source("f1.txt");
    const fs::path target("f2.txt");

    atf::utils::create_file(source.str(), "This is the input");
    fs::copy(source, target);
    ATF_REQUIRE(atf::utils::compare_file(target.str(), "This is the input"));
}


ATF_TEST_CASE_WITHOUT_HEAD(copy__fail_open);
ATF_TEST_CASE_BODY(copy__fail_open)
{
    const fs::path source("f1.txt");
    const fs::path target("f2.txt");

    ATF_REQUIRE_THROW_RE(fs::error, "Cannot open copy source f1.txt",
                         fs::copy(source, target));
}


ATF_TEST_CASE(copy__fail_create);
ATF_TEST_CASE_HEAD(copy__fail_create)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(copy__fail_create)
{
    const fs::path source("f1.txt");
    const fs::path target("f2.txt");

    atf::utils::create_file(target.str(), "Do not override");
    ATF_REQUIRE(::chmod(target.c_str(), 0444) != -1);

    atf::utils::create_file(source.str(), "This is the input");
    ATF_REQUIRE_THROW_RE(fs::error, "Cannot create copy target f2.txt",
                         fs::copy(source, target));
}


ATF_TEST_CASE_WITHOUT_HEAD(current_path__ok);
ATF_TEST_CASE_BODY(current_path__ok)
{
    const fs::path previous = fs::current_path();
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::chdir("root") != -1);
    const fs::path cwd = fs::current_path();
    ATF_REQUIRE_EQ(cwd.str().length() - 5, cwd.str().find("/root"));
    ATF_REQUIRE_EQ(previous / "root", cwd);
}


ATF_TEST_CASE_WITHOUT_HEAD(current_path__enoent);
ATF_TEST_CASE_BODY(current_path__enoent)
{
    const fs::path previous = fs::current_path();
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::chdir("root") != -1);
    ATF_REQUIRE(::rmdir("../root") != -1);
    try {
        (void)fs::current_path();
        fail("system_errpr not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(ENOENT, e.original_errno());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(exists);
ATF_TEST_CASE_BODY(exists)
{
    const fs::path dir("dir");
    ATF_REQUIRE(!fs::exists(dir));
    fs::mkdir(dir, 0755);
    ATF_REQUIRE(fs::exists(dir));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__no_path);
ATF_TEST_CASE_BODY(find_in_path__no_path)
{
    utils::unsetenv("PATH");
    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file("ls", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__empty_path);
ATF_TEST_CASE_BODY(find_in_path__empty_path)
{
    utils::setenv("PATH", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file("ls", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__one_component);
ATF_TEST_CASE_BODY(find_in_path__one_component)
{
    const fs::path dir = fs::current_path() / "bin";
    fs::mkdir(dir, 0755);
    utils::setenv("PATH", dir.str());

    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file((dir / "ls").str(), "");
    ATF_REQUIRE_EQ(dir / "ls", fs::find_in_path("ls").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__many_components);
ATF_TEST_CASE_BODY(find_in_path__many_components)
{
    const fs::path dir1 = fs::current_path() / "dir1";
    const fs::path dir2 = fs::current_path() / "dir2";
    fs::mkdir(dir1, 0755);
    fs::mkdir(dir2, 0755);
    utils::setenv("PATH", dir1.str() + ":" + dir2.str());

    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file((dir2 / "ls").str(), "");
    ATF_REQUIRE_EQ(dir2 / "ls", fs::find_in_path("ls").get());
    atf::utils::create_file((dir1 / "ls").str(), "");
    ATF_REQUIRE_EQ(dir1 / "ls", fs::find_in_path("ls").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__current_directory);
ATF_TEST_CASE_BODY(find_in_path__current_directory)
{
    utils::setenv("PATH", "bin:");

    ATF_REQUIRE(!fs::find_in_path("foo-bar"));
    atf::utils::create_file("foo-bar", "");
    ATF_REQUIRE_EQ(fs::path("foo-bar").to_absolute(),
                   fs::find_in_path("foo-bar").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__always_absolute);
ATF_TEST_CASE_BODY(find_in_path__always_absolute)
{
    fs::mkdir(fs::path("my-bin"), 0755);
    utils::setenv("PATH", "my-bin");

    ATF_REQUIRE(!fs::find_in_path("abcd"));
    atf::utils::create_file("my-bin/abcd", "");
    ATF_REQUIRE_EQ(fs::path("my-bin/abcd").to_absolute(),
                   fs::find_in_path("abcd").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(free_disk_space__ok__smoke);
ATF_TEST_CASE_BODY(free_disk_space__ok__smoke)
{
    const units::bytes space = fs::free_disk_space(fs::path("."));
    ATF_REQUIRE(space > units::MB);  // Simple test that should always pass.
}


/// Unmounts a directory without raising errors.
///
/// \param cookie Name of a file that exists while the mount point is still
///     mounted.  Used to prevent a double-unmount, which would print a
///     misleading error message.
/// \param mount_point Path to the mount point to unmount.
static void
cleanup_mount_point(const fs::path& cookie, const fs::path& mount_point)
{
    try {
        if (fs::exists(cookie)) {
            fs::unmount(mount_point);
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed trying to unmount " + mount_point.str() +
            " during cleanup: " << e.what() << '\n';
    }
}


ATF_TEST_CASE_WITH_CLEANUP(free_disk_space__ok__real);
ATF_TEST_CASE_HEAD(free_disk_space__ok__real)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(free_disk_space__ok__real)
{
    try {
        const fs::path mount_point("mount_point");
        fs::mkdir(mount_point, 0755);
        fs::mount_tmpfs(mount_point, units::bytes(32 * units::MB));
        atf::utils::create_file("mounted", "");
        const units::bytes space = fs::free_disk_space(fs::path(mount_point));
        fs::unmount(mount_point);
        fs::unlink(fs::path("mounted"));
        ATF_REQUIRE(space < 35 * units::MB);
        ATF_REQUIRE(space > 28 * units::MB);
    } catch (const fs::unsupported_operation_error& e) {
        ATF_SKIP(e.what());
    }
}
ATF_TEST_CASE_CLEANUP(free_disk_space__ok__real)
{
    cleanup_mount_point(fs::path("mounted"), fs::path("mount_point"));
}


ATF_TEST_CASE_WITHOUT_HEAD(free_disk_space__fail);
ATF_TEST_CASE_BODY(free_disk_space__fail)
{
    ATF_REQUIRE_THROW_RE(fs::error, "Failed to stat file system for missing",
                         fs::free_disk_space(fs::path("missing")));
}


ATF_TEST_CASE_WITHOUT_HEAD(is_directory__ok);
ATF_TEST_CASE_BODY(is_directory__ok)
{
    const fs::path file("file");
    atf::utils::create_file(file.str(), "");
    ATF_REQUIRE(!fs::is_directory(file));

    const fs::path dir("dir");
    fs::mkdir(dir, 0755);
    ATF_REQUIRE(fs::is_directory(dir));
}


ATF_TEST_CASE_WITH_CLEANUP(is_directory__fail);
ATF_TEST_CASE_HEAD(is_directory__fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(is_directory__fail)
{
    fs::mkdir(fs::path("dir"), 0000);
    ATF_REQUIRE_THROW(fs::error, fs::is_directory(fs::path("dir/foo")));
}
ATF_TEST_CASE_CLEANUP(is_directory__fail)
{
    if (::chmod("dir", 0755) == -1) {
        // If we cannot restore the original permissions, we cannot do much
        // more.  However, leaving an unwritable directory behind will cause the
        // runtime engine to report us as broken.
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir__ok);
ATF_TEST_CASE_BODY(mkdir__ok)
{
    fs::mkdir(fs::path("dir"), 0755);
    ATF_REQUIRE(lookup(".", "dir", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir__enoent);
ATF_TEST_CASE_BODY(mkdir__enoent)
{
    try {
        fs::mkdir(fs::path("dir1/dir2"), 0755);
        fail("system_error not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(ENOENT, e.original_errno());
    }
    ATF_REQUIRE(!lookup(".", "dir1", S_IFDIR));
    ATF_REQUIRE(!lookup(".", "dir2", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__one_component);
ATF_TEST_CASE_BODY(mkdir_p__one_component)
{
    ATF_REQUIRE(!lookup(".", "new-dir", S_IFDIR));
    fs::mkdir_p(fs::path("new-dir"), 0755);
    ATF_REQUIRE(lookup(".", "new-dir", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__many_components);
ATF_TEST_CASE_BODY(mkdir_p__many_components)
{
    ATF_REQUIRE(!lookup(".", "a", S_IFDIR));
    fs::mkdir_p(fs::path("a/b/c"), 0755);
    ATF_REQUIRE(lookup(".", "a", S_IFDIR));
    ATF_REQUIRE(lookup("a", "b", S_IFDIR));
    ATF_REQUIRE(lookup("a/b", "c", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__already_exists);
ATF_TEST_CASE_BODY(mkdir_p__already_exists)
{
    fs::mkdir(fs::path("a"), 0755);
    fs::mkdir(fs::path("a/b"), 0755);
    fs::mkdir_p(fs::path("a/b"), 0755);
}


ATF_TEST_CASE(mkdir_p__eacces)
ATF_TEST_CASE_HEAD(mkdir_p__eacces)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(mkdir_p__eacces)
{
    fs::mkdir(fs::path("a"), 0755);
    fs::mkdir(fs::path("a/b"), 0755);
    ATF_REQUIRE(::chmod("a/b", 0555) != -1);
    try {
        fs::mkdir_p(fs::path("a/b/c/d"), 0755);
        fail("system_error not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(EACCES, e.original_errno());
    }
    ATF_REQUIRE(lookup(".", "a", S_IFDIR));
    ATF_REQUIRE(lookup("a", "b", S_IFDIR));
    ATF_REQUIRE(!lookup(".", "c", S_IFDIR));
    ATF_REQUIRE(!lookup("a", "c", S_IFDIR));
    ATF_REQUIRE(!lookup("a/b", "c", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdtemp_public)
ATF_TEST_CASE_BODY(mkdtemp_public)
{
    const fs::path tmpdir = fs::current_path() / "tmp";
    utils::setenv("TMPDIR", tmpdir.str());
    fs::mkdir(tmpdir, 0755);

    const std::string dir_template("tempdir.XXXXXX");
    const fs::path tempdir = fs::mkdtemp_public(dir_template);
    ATF_REQUIRE(!lookup("tmp", dir_template.c_str(), S_IFDIR));
    ATF_REQUIRE(lookup("tmp", tempdir.leaf_name().c_str(), S_IFDIR));
}


ATF_TEST_CASE(mkdtemp_public__getcwd_as_non_root)
ATF_TEST_CASE_HEAD(mkdtemp_public__getcwd_as_non_root)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(mkdtemp_public__getcwd_as_non_root)
{
    const std::string dir_template("dir.XXXXXX");
    const fs::path dir = fs::mkdtemp_public(dir_template);
    const fs::path subdir = dir / "subdir";
    fs::mkdir(subdir, 0755);

    const uid_t old_euid = ::geteuid();
    const gid_t old_egid = ::getegid();

    const passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));
    ATF_REQUIRE(::setegid(unprivileged_user.gid) != -1);
    ATF_REQUIRE(::seteuid(unprivileged_user.uid) != -1);

    // The next code block runs as non-root.  We cannot use any ATF macros nor
    // functions in it because a failure would cause the test to attempt to
    // write to the ATF result file which may not be writable as non-root.
    bool failed = false;
    {
        try {
            if (::chdir(subdir.c_str()) == -1) {
                std::cerr << "Cannot enter directory\n";
                failed |= true;
            } else {
                fs::current_path();
            }
        } catch (const fs::error& e) {
            failed |= true;
            std::cerr << "Failed to query current path in: " << subdir << '\n';
        }

        if (::seteuid(old_euid) == -1) {
            std::cerr << "Failed to restore euid; cannot continue\n";
            std::abort();
        }
        if (::setegid(old_egid) == -1) {
            std::cerr << "Failed to restore egid; cannot continue\n";
            std::abort();
        }
    }

    if (failed)
        fail("Test failed; see stdout for details");
}


ATF_TEST_CASE(mkdtemp_public__search_permissions_as_non_root)
ATF_TEST_CASE_HEAD(mkdtemp_public__search_permissions_as_non_root)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(mkdtemp_public__search_permissions_as_non_root)
{
    const std::string dir_template("dir.XXXXXX");
    const fs::path dir = fs::mkdtemp_public(dir_template);
    const fs::path cookie = dir / "not-secret";
    atf::utils::create_file(cookie.str(), "this is readable");

    // We are running as root so there is no reason to assume that our current
    // work directory is accessible by non-root.  Weaken the permissions so that
    // our code below works.
    ATF_REQUIRE(::chmod(".", 0755) != -1);

    const uid_t old_euid = ::geteuid();
    const gid_t old_egid = ::getegid();

    const passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));
    ATF_REQUIRE(::setegid(unprivileged_user.gid) != -1);
    ATF_REQUIRE(::seteuid(unprivileged_user.uid) != -1);

    // The next code block runs as non-root.  We cannot use any ATF macros nor
    // functions in it because a failure would cause the test to attempt to
    // write to the ATF result file which may not be writable as non-root.
    bool failed = false;
    {
        try {
            const std::string contents = utils::read_file(cookie);
            std::cerr << "Read contents: " << contents << '\n';
            failed |= (contents != "this is readable");
        } catch (const std::runtime_error& e) {
            failed |= true;
            std::cerr << "Failed to read " << cookie << '\n';
        }

        if (::seteuid(old_euid) == -1) {
            std::cerr << "Failed to restore euid; cannot continue\n";
            std::abort();
        }
        if (::setegid(old_egid) == -1) {
            std::cerr << "Failed to restore egid; cannot continue\n";
            std::abort();
        }
    }

    if (failed)
        fail("Test failed; see stdout for details");
}


ATF_TEST_CASE_WITHOUT_HEAD(mkstemp)
ATF_TEST_CASE_BODY(mkstemp)
{
    const fs::path tmpdir = fs::current_path() / "tmp";
    utils::setenv("TMPDIR", tmpdir.str());
    fs::mkdir(tmpdir, 0755);

    const std::string file_template("tempfile.XXXXXX");
    const fs::path tempfile = fs::mkstemp(file_template);
    ATF_REQUIRE(!lookup("tmp", file_template.c_str(), S_IFREG));
    ATF_REQUIRE(lookup("tmp", tempfile.leaf_name().c_str(), S_IFREG));
}


static void
test_mount_tmpfs_ok(const units::bytes& size)
{
    const fs::path mount_point("mount_point");
    fs::mkdir(mount_point, 0755);

    try {
        atf::utils::create_file("outside", "");
        fs::mount_tmpfs(mount_point, size);
        atf::utils::create_file("mounted", "");
        atf::utils::create_file((mount_point / "inside").str(), "");

        struct ::stat outside, inside;
        ATF_REQUIRE(::stat("outside", &outside) != -1);
        ATF_REQUIRE(::stat((mount_point / "inside").c_str(), &inside) != -1);
        ATF_REQUIRE(outside.st_dev != inside.st_dev);
        fs::unmount(mount_point);
    } catch (const fs::unsupported_operation_error& e) {
        ATF_SKIP(e.what());
    }
}


ATF_TEST_CASE_WITH_CLEANUP(mount_tmpfs__ok__default_size)
ATF_TEST_CASE_HEAD(mount_tmpfs__ok__default_size)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(mount_tmpfs__ok__default_size)
{
    test_mount_tmpfs_ok(units::bytes());
}
ATF_TEST_CASE_CLEANUP(mount_tmpfs__ok__default_size)
{
    cleanup_mount_point(fs::path("mounted"), fs::path("mount_point"));
}


ATF_TEST_CASE_WITH_CLEANUP(mount_tmpfs__ok__explicit_size)
ATF_TEST_CASE_HEAD(mount_tmpfs__ok__explicit_size)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(mount_tmpfs__ok__explicit_size)
{
    test_mount_tmpfs_ok(units::bytes(10 * units::MB));
}
ATF_TEST_CASE_CLEANUP(mount_tmpfs__ok__explicit_size)
{
    cleanup_mount_point(fs::path("mounted"), fs::path("mount_point"));
}


ATF_TEST_CASE(mount_tmpfs__fail)
ATF_TEST_CASE_HEAD(mount_tmpfs__fail)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(mount_tmpfs__fail)
{
    try {
        fs::mount_tmpfs(fs::path("non-existent"));
    } catch (const fs::unsupported_operation_error& e) {
        ATF_SKIP(e.what());
    } catch (const fs::error& e) {
        // Expected.
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(rm_r__empty);
ATF_TEST_CASE_BODY(rm_r__empty)
{
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(lookup(".", "root", S_IFDIR));
    fs::rm_r(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(rm_r__files_and_directories);
ATF_TEST_CASE_BODY(rm_r__files_and_directories)
{
    fs::mkdir(fs::path("root"), 0755);
    atf::utils::create_file("root/.hidden_file", "");
    fs::mkdir(fs::path("root/.hidden_dir"), 0755);
    atf::utils::create_file("root/.hidden_dir/a", "");
    atf::utils::create_file("root/file", "");
    atf::utils::create_file("root/with spaces", "");
    fs::mkdir(fs::path("root/dir1"), 0755);
    fs::mkdir(fs::path("root/dir1/dir2"), 0755);
    atf::utils::create_file("root/dir1/dir2/file", "");
    fs::mkdir(fs::path("root/dir1/dir3"), 0755);
    ATF_REQUIRE(lookup(".", "root", S_IFDIR));
    fs::rm_r(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(rm_r__bad_perms);
ATF_TEST_CASE_BODY(rm_r__bad_perms)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir"), 0755);
    atf::utils::create_file("root/dir/file", "");
    ::chmod(fs::path("root/dir").c_str(), 0000);
    ATF_REQUIRE(lookup(".", "root", S_IFDIR));
    fs::rm_r(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", S_IFDIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(rmdir__ok)
ATF_TEST_CASE_BODY(rmdir__ok)
{
    ATF_REQUIRE(::mkdir("foo", 0755) != -1);
    ATF_REQUIRE(::access("foo", X_OK) == 0);
    fs::rmdir(fs::path("foo"));
    ATF_REQUIRE(::access("foo", X_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(rmdir__fail)
ATF_TEST_CASE_BODY(rmdir__fail)
{
    ATF_REQUIRE_THROW_RE(fs::system_error, "Removal of foo failed",
                         fs::rmdir(fs::path("foo")));
}


ATF_TEST_CASE_WITHOUT_HEAD(scan_directory__ok)
ATF_TEST_CASE_BODY(scan_directory__ok)
{
    fs::mkdir(fs::path("dir"), 0755);
    atf::utils::create_file("dir/foo", "");
    atf::utils::create_file("dir/.hidden", "");

    const std::set< fs::directory_entry > contents = fs::scan_directory(
        fs::path("dir"));

    std::set< fs::directory_entry > exp_contents;
    exp_contents.insert(fs::directory_entry("."));
    exp_contents.insert(fs::directory_entry(".."));
    exp_contents.insert(fs::directory_entry(".hidden"));
    exp_contents.insert(fs::directory_entry("foo"));

    ATF_REQUIRE_EQ(exp_contents, contents);
}


ATF_TEST_CASE_WITHOUT_HEAD(scan_directory__fail)
ATF_TEST_CASE_BODY(scan_directory__fail)
{
    ATF_REQUIRE_THROW_RE(fs::system_error, "opendir(.*missing.*) failed",
                         fs::scan_directory(fs::path("missing")));
}


ATF_TEST_CASE_WITHOUT_HEAD(unlink__ok)
ATF_TEST_CASE_BODY(unlink__ok)
{
    atf::utils::create_file("foo", "");
    ATF_REQUIRE(::access("foo", R_OK) == 0);
    fs::unlink(fs::path("foo"));
    ATF_REQUIRE(::access("foo", R_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(unlink__fail)
ATF_TEST_CASE_BODY(unlink__fail)
{
    ATF_REQUIRE_THROW_RE(fs::system_error, "Removal of foo failed",
                         fs::unlink(fs::path("foo")));
}


ATF_TEST_CASE(unmount__ok)
ATF_TEST_CASE_HEAD(unmount__ok)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(unmount__ok)
{
    const fs::path mount_point("mount_point");
    fs::mkdir(mount_point, 0755);

    atf::utils::create_file((mount_point / "test1").str(), "");
    try {
        fs::mount_tmpfs(mount_point);
    } catch (const fs::unsupported_operation_error& e) {
        ATF_SKIP(e.what());
    }

    atf::utils::create_file((mount_point / "test2").str(), "");

    ATF_REQUIRE(!fs::exists(mount_point / "test1"));
    ATF_REQUIRE( fs::exists(mount_point / "test2"));
    fs::unmount(mount_point);
    ATF_REQUIRE( fs::exists(mount_point / "test1"));
    ATF_REQUIRE(!fs::exists(mount_point / "test2"));
}


ATF_TEST_CASE(unmount__fail)
ATF_TEST_CASE_HEAD(unmount__fail)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(unmount__fail)
{
    ATF_REQUIRE_THROW(fs::error, fs::unmount(fs::path("non-existent")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, copy__ok);
    ATF_ADD_TEST_CASE(tcs, copy__fail_open);
    ATF_ADD_TEST_CASE(tcs, copy__fail_create);

    ATF_ADD_TEST_CASE(tcs, current_path__ok);
    ATF_ADD_TEST_CASE(tcs, current_path__enoent);

    ATF_ADD_TEST_CASE(tcs, exists);

    ATF_ADD_TEST_CASE(tcs, find_in_path__no_path);
    ATF_ADD_TEST_CASE(tcs, find_in_path__empty_path);
    ATF_ADD_TEST_CASE(tcs, find_in_path__one_component);
    ATF_ADD_TEST_CASE(tcs, find_in_path__many_components);
    ATF_ADD_TEST_CASE(tcs, find_in_path__current_directory);
    ATF_ADD_TEST_CASE(tcs, find_in_path__always_absolute);

    ATF_ADD_TEST_CASE(tcs, free_disk_space__ok__smoke);
    ATF_ADD_TEST_CASE(tcs, free_disk_space__ok__real);
    ATF_ADD_TEST_CASE(tcs, free_disk_space__fail);

    ATF_ADD_TEST_CASE(tcs, is_directory__ok);
    ATF_ADD_TEST_CASE(tcs, is_directory__fail);

    ATF_ADD_TEST_CASE(tcs, mkdir__ok);
    ATF_ADD_TEST_CASE(tcs, mkdir__enoent);

    ATF_ADD_TEST_CASE(tcs, mkdir_p__one_component);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__many_components);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__already_exists);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__eacces);

    ATF_ADD_TEST_CASE(tcs, mkdtemp_public);
    ATF_ADD_TEST_CASE(tcs, mkdtemp_public__getcwd_as_non_root);
    ATF_ADD_TEST_CASE(tcs, mkdtemp_public__search_permissions_as_non_root);

    ATF_ADD_TEST_CASE(tcs, mkstemp);

    ATF_ADD_TEST_CASE(tcs, mount_tmpfs__ok__default_size);
    ATF_ADD_TEST_CASE(tcs, mount_tmpfs__ok__explicit_size);
    ATF_ADD_TEST_CASE(tcs, mount_tmpfs__fail);

    ATF_ADD_TEST_CASE(tcs, rm_r__empty);
    ATF_ADD_TEST_CASE(tcs, rm_r__files_and_directories);
    ATF_ADD_TEST_CASE(tcs, rm_r__bad_perms);

    ATF_ADD_TEST_CASE(tcs, rmdir__ok);
    ATF_ADD_TEST_CASE(tcs, rmdir__fail);

    ATF_ADD_TEST_CASE(tcs, scan_directory__ok);
    ATF_ADD_TEST_CASE(tcs, scan_directory__fail);

    ATF_ADD_TEST_CASE(tcs, unlink__ok);
    ATF_ADD_TEST_CASE(tcs, unlink__fail);

    ATF_ADD_TEST_CASE(tcs, unmount__ok);
    ATF_ADD_TEST_CASE(tcs, unmount__fail);
}
