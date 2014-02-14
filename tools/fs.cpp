//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if defined(HAVE_CONFIG_H)
#include "bconfig.h"
#endif

extern "C" {
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
}

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "auto_array.hpp"
#include "env.hpp"
#include "exceptions.hpp"
#include "fs.hpp"
#include "process.hpp"
#include "text.hpp"
#include "user.hpp"

namespace impl = tools::fs;
#define IMPL_NAME "tools::fs"

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

static void cleanup_aux(const impl::path&, dev_t, bool);
static void cleanup_aux_dir(const impl::path&, const impl::file_info&,
                            bool);
static void do_unmount(const impl::path&);
static bool safe_access(const impl::path&, int, int);

static const int access_f = 1 << 0;
static const int access_r = 1 << 1;
static const int access_w = 1 << 2;
static const int access_x = 1 << 3;

//!
//! An implementation of access(2) but using the effective user value
//! instead of the real one.  Also avoids false positives for root when
//! asking for execute permissions, which appear in SunOS.
//!
static
void
eaccess(const tools::fs::path& p, int mode)
{
    assert(mode & access_f || mode & access_r ||
           mode & access_w || mode & access_x);

    struct stat st;
    if (lstat(p.c_str(), &st) == -1)
        throw tools::system_error(IMPL_NAME "::eaccess",
                                  "Cannot get information from file " +
                                  p.str(), errno);

    /* Early return if we are only checking for existence and the file
     * exists (stat call returned). */
    if (mode & access_f)
        return;

    bool ok = false;
    if (tools::user::is_root()) {
        if (!ok && !(mode & access_x)) {
            /* Allow root to read/write any file. */
            ok = true;
        }

        if (!ok && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            /* Allow root to execute the file if any of its execution bits
             * are set. */
            ok = true;
        }
    } else {
        if (!ok && (tools::user::euid() == st.st_uid)) {
            ok = ((mode & access_r) && (st.st_mode & S_IRUSR)) ||
                 ((mode & access_w) && (st.st_mode & S_IWUSR)) ||
                 ((mode & access_x) && (st.st_mode & S_IXUSR));
        }
        if (!ok && tools::user::is_member_of_group(st.st_gid)) {
            ok = ((mode & access_r) && (st.st_mode & S_IRGRP)) ||
                 ((mode & access_w) && (st.st_mode & S_IWGRP)) ||
                 ((mode & access_x) && (st.st_mode & S_IXGRP));
        }
        if (!ok && ((tools::user::euid() != st.st_uid) &&
                    !tools::user::is_member_of_group(st.st_gid))) {
            ok = ((mode & access_r) && (st.st_mode & S_IROTH)) ||
                 ((mode & access_w) && (st.st_mode & S_IWOTH)) ||
                 ((mode & access_x) && (st.st_mode & S_IXOTH));
        }
    }

    if (!ok)
        throw tools::system_error(IMPL_NAME "::eaccess", "Access check failed",
                                  EACCES);
}

//!
//! \brief A controlled version of access(2).
//!
//! This function reimplements the standard access(2) system call to
//! safely control its exit status and raise an exception in case of
//! failure.
//!
static
bool
safe_access(const impl::path& p, int mode, int experr)
{
    try {
        eaccess(p, mode);
        return true;
    } catch (const tools::system_error& e) {
        if (e.code() == experr)
            return false;
        else
            throw e;
    }
}

// The cleanup routines below are tricky: they are executed immediately after
// a test case's death, and after we have forcibly killed any stale processes.
// However, even if the processes are dead, this does not mean that the file
// system we are scanning is stable.  In particular, if the test case has
// mounted file systems through fuse/puffs, the fact that the processes died
// does not mean that the file system is truly unmounted.
//
// The code below attempts to cope with this by catching errors and either
// ignoring them or retrying the actions on the same file/directory a few times
// before giving up.
static const int max_retries = 5;
static const int retry_delay_in_seconds = 1;

// The erase parameter in this routine is to control nested mount points.
// We want to descend into a mount point to unmount anything that is
// mounted under it, but we do not want to delete any files while doing
// this traversal.  In other words, we erase files until we cross the
// first mount point, and after that point we only scan and unmount.
static
void
cleanup_aux(const impl::path& p, dev_t parent_device, bool erase)
{
    try {
        impl::file_info fi(p);

        if (fi.get_type() == impl::file_info::dir_type)
            cleanup_aux_dir(p, fi, fi.get_device() == parent_device);

        if (fi.get_device() != parent_device)
            do_unmount(p);

        if (erase) {
            if (fi.get_type() == impl::file_info::dir_type)
                impl::rmdir(p);
            else
                impl::remove(p);
        }
    } catch (const tools::system_error& e) {
        if (e.code() != ENOENT && e.code() != ENOTDIR)
            throw e;
    }
}

static
void
cleanup_aux_dir(const impl::path& p, const impl::file_info& fi,
                bool erase)
{
    if (erase && ((fi.get_mode() & S_IRWXU) != S_IRWXU)) {
        int retries = max_retries;
retry_chmod:
        if (chmod(p.c_str(), fi.get_mode() | S_IRWXU) == -1) {
            if (retries > 0) {
                retries--;
                ::sleep(retry_delay_in_seconds);
                goto retry_chmod;
            } else {
                throw tools::system_error(IMPL_NAME "::cleanup(" +
                                        p.str() + ")", "chmod(2) failed",
                                        errno);
            }
        }
    }

    std::set< std::string > subdirs;
    {
        bool ok = false;
        int retries = max_retries;
        while (!ok) {
            assert(retries > 0);
            try {
                const impl::directory d(p);
                subdirs = d.names();
                ok = true;
            } catch (const tools::system_error& e) {
                retries--;
                if (retries == 0)
                    throw e;
                ::sleep(retry_delay_in_seconds);
            }
        }
        assert(ok);
    }

    for (std::set< std::string >::const_iterator iter = subdirs.begin();
         iter != subdirs.end(); iter++) {
        const std::string& name = *iter;
        if (name != "." && name != "..")
            cleanup_aux(p / name, fi.get_device(), erase);
    }
}

static
void
do_unmount(const impl::path& in_path)
{
    // At least, FreeBSD's unmount(2) requires the path to be absolute.
    // Let's make it absolute in all cases just to be safe that this does
    // not affect other systems.
    const impl::path& abs_path = in_path.is_absolute() ?
        in_path : in_path.to_absolute();

#if defined(HAVE_UNMOUNT)
    int retries = max_retries;
retry_unmount:
    if (unmount(abs_path.c_str(), 0) == -1) {
        if (errno == EBUSY && retries > 0) {
            retries--;
            ::sleep(retry_delay_in_seconds);
            goto retry_unmount;
        } else {
            throw tools::system_error(IMPL_NAME "::cleanup(" + in_path.str() +
                                    ")", "unmount(2) failed", errno);
        }
    }
#else
    // We could use umount(2) instead if it was available... but
    // trying to do so under, e.g. Linux, is a nightmare because we
    // also have to update /etc/mtab to match what we did.  It is
    // stools::fser to just leave the system-specific umount(8) tool deal
    // with it, at least for now.

    const impl::path prog("umount");
    tools::process::argv_array argv("umount", abs_path.c_str(), NULL);

    tools::process::status s = tools::process::exec(prog, argv,
        tools::process::stream_inherit(), tools::process::stream_inherit());
    if (!s.exited() || s.exitstatus() != EXIT_SUCCESS)
        throw std::runtime_error("Call to unmount failed");
#endif
}

static
std::string
normalize(const std::string& in)
{
    assert(!in.empty());

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

// ------------------------------------------------------------------------
// The "path" class.
// ------------------------------------------------------------------------

impl::path::path(const std::string& s) :
    m_data(normalize(s))
{
}

impl::path::~path(void)
{
}

const char*
impl::path::c_str(void)
    const
{
    return m_data.c_str();
}

std::string
impl::path::str(void)
    const
{
    return m_data;
}

bool
impl::path::is_absolute(void)
    const
{
    return !m_data.empty() && m_data[0] == '/';
}

bool
impl::path::is_root(void)
    const
{
    return m_data == "/";
}

impl::path
impl::path::branch_path(void)
    const
{
    const std::string::size_type endpos = m_data.rfind('/');
    if (endpos == std::string::npos)
        return path(".");
    else if (endpos == 0)
        return path("/");
    else
        return path(m_data.substr(0, endpos));
}

std::string
impl::path::leaf_name(void)
    const
{
    std::string::size_type begpos = m_data.rfind('/');
    if (begpos == std::string::npos)
        begpos = 0;
    else
        begpos++;

    return m_data.substr(begpos);
}

impl::path
impl::path::to_absolute(void)
    const
{
    assert(!is_absolute());
    return get_current_dir() / m_data;
}

bool
impl::path::operator==(const path& p)
    const
{
    return m_data == p.m_data;
}

bool
impl::path::operator!=(const path& p)
    const
{
    return m_data != p.m_data;
}

impl::path
impl::path::operator/(const std::string& p)
    const
{
    return path(m_data + "/" + normalize(p));
}

impl::path
impl::path::operator/(const path& p)
    const
{
    return path(m_data) / p.m_data;
}

bool
impl::path::operator<(const path& p)
    const
{
    return std::strcmp(m_data.c_str(), p.m_data.c_str()) < 0;
}

// ------------------------------------------------------------------------
// The "file_info" class.
// ------------------------------------------------------------------------

const int impl::file_info::blk_type = 1;
const int impl::file_info::chr_type = 2;
const int impl::file_info::dir_type = 3;
const int impl::file_info::fifo_type = 4;
const int impl::file_info::lnk_type = 5;
const int impl::file_info::reg_type = 6;
const int impl::file_info::sock_type = 7;
const int impl::file_info::wht_type = 8;

impl::file_info::file_info(const path& p)
{
    if (lstat(p.c_str(), &m_sb) == -1)
        throw system_error(IMPL_NAME "::file_info",
                           "Cannot get information of " + p.str() + "; " +
                           "lstat(2) failed", errno);

    int type = m_sb.st_mode & S_IFMT;
    switch (type) {
    case S_IFBLK:  m_type = blk_type;  break;
    case S_IFCHR:  m_type = chr_type;  break;
    case S_IFDIR:  m_type = dir_type;  break;
    case S_IFIFO:  m_type = fifo_type; break;
    case S_IFLNK:  m_type = lnk_type;  break;
    case S_IFREG:  m_type = reg_type;  break;
    case S_IFSOCK: m_type = sock_type; break;
#if defined(S_IFWHT)
    case S_IFWHT:  m_type = wht_type;  break;
#endif
    default:
        throw system_error(IMPL_NAME "::file_info", "Unknown file type "
                           "error", EINVAL);
    }
}

impl::file_info::~file_info(void)
{
}

dev_t
impl::file_info::get_device(void)
    const
{
    return m_sb.st_dev;
}

ino_t
impl::file_info::get_inode(void)
    const
{
    return m_sb.st_ino;
}

mode_t
impl::file_info::get_mode(void)
    const
{
    return m_sb.st_mode & ~S_IFMT;
}

off_t
impl::file_info::get_size(void)
    const
{
    return m_sb.st_size;
}

int
impl::file_info::get_type(void)
    const
{
    return m_type;
}

bool
impl::file_info::is_owner_readable(void)
    const
{
    return m_sb.st_mode & S_IRUSR;
}

bool
impl::file_info::is_owner_writable(void)
    const
{
    return m_sb.st_mode & S_IWUSR;
}

bool
impl::file_info::is_owner_executable(void)
    const
{
    return m_sb.st_mode & S_IXUSR;
}

bool
impl::file_info::is_group_readable(void)
    const
{
    return m_sb.st_mode & S_IRGRP;
}

bool
impl::file_info::is_group_writable(void)
    const
{
    return m_sb.st_mode & S_IWGRP;
}

bool
impl::file_info::is_group_executable(void)
    const
{
    return m_sb.st_mode & S_IXGRP;
}

bool
impl::file_info::is_other_readable(void)
    const
{
    return m_sb.st_mode & S_IROTH;
}

bool
impl::file_info::is_other_writable(void)
    const
{
    return m_sb.st_mode & S_IWOTH;
}

bool
impl::file_info::is_other_executable(void)
    const
{
    return m_sb.st_mode & S_IXOTH;
}

// ------------------------------------------------------------------------
// The "directory" class.
// ------------------------------------------------------------------------

impl::directory::directory(const path& p)
{
    DIR* dp = ::opendir(p.c_str());
    if (dp == NULL)
        throw system_error(IMPL_NAME "::directory::directory(" +
                           p.str() + ")", "opendir(3) failed", errno);

    struct dirent* dep;
    while ((dep = ::readdir(dp)) != NULL) {
        path entryp = p / dep->d_name;
        insert(value_type(dep->d_name, file_info(entryp)));
    }

    if (::closedir(dp) == -1)
        throw system_error(IMPL_NAME "::directory::directory(" +
                           p.str() + ")", "closedir(3) failed", errno);
}

std::set< std::string >
impl::directory::names(void)
    const
{
    std::set< std::string > ns;

    for (const_iterator iter = begin(); iter != end(); iter++)
        ns.insert((*iter).first);

    return ns;
}

// ------------------------------------------------------------------------
// The "temp_dir" class.
// ------------------------------------------------------------------------

impl::temp_dir::temp_dir(const path& p)
{
    tools::auto_array< char > buf(new char[p.str().length() + 1]);
    std::strcpy(buf.get(), p.c_str());
    if (::mkdtemp(buf.get()) == NULL)
        throw tools::system_error(IMPL_NAME "::temp_dir::temp_dir(" +
                                p.str() + ")", "mkdtemp(3) failed",
                                errno);

    m_path.reset(new path(buf.get()));
}

impl::temp_dir::~temp_dir(void)
{
    cleanup(*m_path);
}

const impl::path&
impl::temp_dir::get_path(void)
    const
{
    return *m_path;
}

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

bool
impl::exists(const path& p)
{
    try {
        eaccess(p, access_f);
        return true;
    } catch (const system_error& e) {
        if (e.code() == ENOENT)
            return false;
        else
            throw;
    }
}

bool
impl::have_prog_in_path(const std::string& prog)
{
    assert(prog.find('/') == std::string::npos);

    // Do not bother to provide a default value for PATH.  If it is not
    // there something is broken in the user's environment.
    if (!tools::env::has("PATH"))
        throw std::runtime_error("PATH not defined in the environment");
    std::vector< std::string > dirs =
        tools::text::split(tools::env::get("PATH"), ":");

    bool found = false;
    for (std::vector< std::string >::const_iterator iter = dirs.begin();
         !found && iter != dirs.end(); iter++) {
        const path& dir = path(*iter);

        if (is_executable(dir / prog))
            found = true;
    }
    return found;
}

bool
impl::is_executable(const path& p)
{
    if (!exists(p))
        return false;
    return safe_access(p, access_x, EACCES);
}

void
impl::remove(const path& p)
{
    if (file_info(p).get_type() == file_info::dir_type)
        throw tools::system_error(IMPL_NAME "::remove(" + p.str() + ")",
                                  "Is a directory",
                                  EPERM);
    if (::unlink(p.c_str()) == -1)
        throw tools::system_error(IMPL_NAME "::remove(" + p.str() + ")",
                                  "unlink(" + p.str() + ") failed",
                                  errno);
}

void
impl::rmdir(const path& p)
{
    if (::rmdir(p.c_str())) {
        if (errno == EEXIST) {
            /* Some operating systems (e.g. OpenSolaris 200906) return
             * EEXIST instead of ENOTEMPTY for non-empty directories.
             * Homogenize the return value so that callers don't need
             * to bother about differences in operating systems. */
            errno = ENOTEMPTY;
        }
        throw system_error(IMPL_NAME "::rmdir", "Cannot remove directory",
                           errno);
    }
}

impl::path
impl::change_directory(const path& dir)
{
    path olddir = get_current_dir();

    if (olddir != dir) {
        if (::chdir(dir.c_str()) == -1)
            throw tools::system_error(IMPL_NAME "::chdir(" + dir.str() + ")",
                                    "chdir(2) failed", errno);
    }

    return olddir;
}

void
impl::cleanup(const path& p)
{
    impl::file_info fi(p);
    cleanup_aux(p, fi.get_device(), true);
}

impl::path
impl::get_current_dir(void)
{
    std::auto_ptr< char > cwd;
#if defined(HAVE_GETCWD_DYN)
    cwd.reset(getcwd(NULL, 0));
#else
    cwd.reset(getcwd(NULL, MAXPATHLEN));
#endif
    if (cwd.get() == NULL)
        throw tools::system_error(IMPL_NAME "::get_current_dir()",
                                "getcwd() failed", errno);

    return path(cwd.get());
}
