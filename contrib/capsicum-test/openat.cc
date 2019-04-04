#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <string>

#include "capsicum.h"
#include "capsicum-test.h"
#include "syscalls.h"

// Check an open call works and close the resulting fd.
#define EXPECT_OPEN_OK(f) do { \
    int _fd = f;               \
    EXPECT_OK(_fd);            \
    close(_fd);                \
  } while (0)

static void CreateFile(const char *filename, const char *contents) {
  int fd = open(filename, O_CREAT|O_RDWR, 0644);
  EXPECT_OK(fd);
  EXPECT_OK(write(fd, contents, strlen(contents)));
  close(fd);
}

// Test openat(2) in a variety of sitations to ensure that it obeys Capsicum
// "strict relative" rules:
//
// 1. Use strict relative lookups in capability mode or when operating
//    relative to a capability.
// 2. When performing strict relative lookups, absolute paths (including
//    symlinks to absolute paths) are not allowed, nor are paths containing
//    '..' components.
//
// These rules apply when:
//  - the directory FD is a Capsicum capability
//  - the process is in capability mode
//  - the openat(2) operation includes the O_BENEATH flag.
FORK_TEST(Openat, Relative) {
  int etc = open("/etc/", O_RDONLY);
  EXPECT_OK(etc);

  cap_rights_t r_base;
  cap_rights_init(&r_base, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_LOOKUP, CAP_FCNTL, CAP_IOCTL);
  cap_rights_t r_ro;
  cap_rights_init(&r_ro, CAP_READ);
  cap_rights_t r_rl;
  cap_rights_init(&r_rl, CAP_READ, CAP_LOOKUP);

  int etc_cap = dup(etc);
  EXPECT_OK(etc_cap);
  EXPECT_OK(cap_rights_limit(etc_cap, &r_ro));
  int etc_cap_ro = dup(etc);
  EXPECT_OK(etc_cap_ro);
  EXPECT_OK(cap_rights_limit(etc_cap_ro, &r_rl));
  int etc_cap_base = dup(etc);
  EXPECT_OK(etc_cap_base);
  EXPECT_OK(cap_rights_limit(etc_cap_base, &r_base));
#ifdef HAVE_CAP_FCNTLS_LIMIT
  // Also limit fcntl(2) subrights.
  EXPECT_OK(cap_fcntls_limit(etc_cap_base, CAP_FCNTL_GETFL));
#endif
#ifdef HAVE_CAP_IOCTLS_LIMIT
  // Also limit ioctl(2) subrights.
  cap_ioctl_t ioctl_nread = FIONREAD;
  EXPECT_OK(cap_ioctls_limit(etc_cap_base, &ioctl_nread, 1));
#endif

  // openat(2) with regular file descriptors in non-capability mode
  // Should Just Work (tm).
  EXPECT_OPEN_OK(openat(etc, "/etc/passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(AT_FDCWD, "/etc/passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(etc, "passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(etc, "../etc/passwd", O_RDONLY));

  // Lookups relative to capabilities should be strictly relative.
  // When not in capability mode, we don't actually require CAP_LOOKUP.
  EXPECT_OPEN_OK(openat(etc_cap_ro, "passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(etc_cap_base, "passwd", O_RDONLY));

  // Performing openat(2) on a path with leading slash ignores
  // the provided directory FD.
  EXPECT_OPEN_OK(openat(etc_cap_ro, "/etc/passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(etc_cap_base, "/etc/passwd", O_RDONLY));
  // Relative lookups that go upward are not allowed.
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc_cap_ro, "../etc/passwd", O_RDONLY);
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc_cap_base, "../etc/passwd", O_RDONLY);

  // A file opened relative to a capability should itself be a capability.
  int fd = openat(etc_cap_base, "passwd", O_RDONLY);
  EXPECT_OK(fd);
  cap_rights_t rights;
  EXPECT_OK(cap_rights_get(fd, &rights));
  EXPECT_RIGHTS_IN(&rights, &r_base);
#ifdef HAVE_CAP_FCNTLS_LIMIT
  cap_fcntl_t fcntls;
  EXPECT_OK(cap_fcntls_get(fd, &fcntls));
  EXPECT_EQ((cap_fcntl_t)CAP_FCNTL_GETFL, fcntls);
#endif
#ifdef HAVE_CAP_IOCTLS_LIMIT
  cap_ioctl_t ioctls[16];
  ssize_t nioctls;
  memset(ioctls, 0, sizeof(ioctls));
  nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(1, nioctls);
  EXPECT_EQ((cap_ioctl_t)FIONREAD, ioctls[0]);
#endif
  close(fd);

  // Enter capability mode; now ALL lookups are strictly relative.
  EXPECT_OK(cap_enter());

  // Relative lookups on regular files or capabilities with CAP_LOOKUP
  // ought to succeed.
  EXPECT_OPEN_OK(openat(etc, "passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(etc_cap_ro, "passwd", O_RDONLY));
  EXPECT_OPEN_OK(openat(etc_cap_base, "passwd", O_RDONLY));

  // Lookup relative to capabilities without CAP_LOOKUP should fail.
  EXPECT_NOTCAPABLE(openat(etc_cap, "passwd", O_RDONLY));

  // Absolute lookups should fail.
  EXPECT_CAPMODE(openat(AT_FDCWD, "/etc/passwd", O_RDONLY));
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc, "/etc/passwd", O_RDONLY);
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc_cap_ro, "/etc/passwd", O_RDONLY);

  // Lookups containing '..' should fail in capability mode.
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc, "../etc/passwd", O_RDONLY);
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc_cap_ro, "../etc/passwd", O_RDONLY);
  EXPECT_OPENAT_FAIL_TRAVERSAL(etc_cap_base, "../etc/passwd", O_RDONLY);

  fd = openat(etc, "passwd", O_RDONLY);
  EXPECT_OK(fd);

  // A file opened relative to a capability should itself be a capability.
  fd = openat(etc_cap_base, "passwd", O_RDONLY);
  EXPECT_OK(fd);
  EXPECT_OK(cap_rights_get(fd, &rights));
  EXPECT_RIGHTS_IN(&rights, &r_base);
  close(fd);

  fd = openat(etc_cap_ro, "passwd", O_RDONLY);
  EXPECT_OK(fd);
  EXPECT_OK(cap_rights_get(fd, &rights));
  EXPECT_RIGHTS_IN(&rights, &r_rl);
  close(fd);
}

#define TOPDIR "cap_topdir"
#define SUBDIR TOPDIR "/subdir"
class OpenatTest : public ::testing::Test {
 public:
  // Build a collection of files, subdirs and symlinks:
  //  /tmp/cap_topdir/
  //                 /topfile
  //                 /subdir/
  //                 /subdir/bottomfile
  //                 /symlink.samedir              -> topfile
  //                 /dsymlink.samedir             -> ./
  //                 /symlink.down                 -> subdir/bottomfile
  //                 /dsymlink.down                -> subdir/
  //                 /symlink.absolute_out         -> /etc/passwd
  //                 /dsymlink.absolute_out        -> /etc/
  //                 /symlink.relative_in          -> ../../tmp/cap_topdir/topfile
  //                 /dsymlink.relative_in         -> ../../tmp/cap_topdir/
  //                 /symlink.relative_out         -> ../../etc/passwd
  //                 /dsymlink.relative_out        -> ../../etc/
  //                 /subdir/dsymlink.absolute_in  -> /tmp/cap_topdir/
  //                 /subdir/dsymlink.up           -> ../
  //                 /subdir/symlink.absolute_in   -> /tmp/cap_topdir/topfile
  //                 /subdir/symlink.up            -> ../topfile
  // (In practice, this is a little more complicated because tmpdir might
  // not be "/tmp".)
  OpenatTest() {
    // Create a couple of nested directories
    int rc = mkdir(TmpFile(TOPDIR), 0755);
    EXPECT_OK(rc);
    if (rc < 0) {
      EXPECT_EQ(EEXIST, errno);
    }
    rc = mkdir(TmpFile(SUBDIR), 0755);
    EXPECT_OK(rc);
    if (rc < 0) {
      EXPECT_EQ(EEXIST, errno);
    }

    // Figure out a path prefix (like "../..") that gets us to the root
    // directory from TmpFile(TOPDIR).
    const char *p = TmpFile(TOPDIR);  // maybe "/tmp/somewhere/cap_topdir"
    std::string dots2root = "..";
    while (*p++ != '\0') {
      if (*p == '/') {
        dots2root += "/..";
      }
    }

    // Create normal files in each.
    CreateFile(TmpFile(TOPDIR "/topfile"), "Top-level file");
    CreateFile(TmpFile(SUBDIR "/bottomfile"), "File in subdirectory");

    // Create various symlinks to files.
    EXPECT_OK(symlink("topfile", TmpFile(TOPDIR "/symlink.samedir")));
    EXPECT_OK(symlink("subdir/bottomfile", TmpFile(TOPDIR "/symlink.down")));
    EXPECT_OK(symlink(TmpFile(TOPDIR "/topfile"), TmpFile(SUBDIR "/symlink.absolute_in")));
    EXPECT_OK(symlink("/etc/passwd", TmpFile(TOPDIR "/symlink.absolute_out")));
    std::string dots2top = dots2root + TmpFile(TOPDIR "/topfile");
    EXPECT_OK(symlink(dots2top.c_str(), TmpFile(TOPDIR "/symlink.relative_in")));
    std::string dots2passwd = dots2root + "/etc/passwd";
    EXPECT_OK(symlink(dots2passwd.c_str(), TmpFile(TOPDIR "/symlink.relative_out")));
    EXPECT_OK(symlink("../topfile", TmpFile(SUBDIR "/symlink.up")));

    // Create various symlinks to directories.
    EXPECT_OK(symlink("./", TmpFile(TOPDIR "/dsymlink.samedir")));
    EXPECT_OK(symlink("subdir/", TmpFile(TOPDIR "/dsymlink.down")));
    EXPECT_OK(symlink(TmpFile(TOPDIR "/"), TmpFile(SUBDIR "/dsymlink.absolute_in")));
    EXPECT_OK(symlink("/etc/", TmpFile(TOPDIR "/dsymlink.absolute_out")));
    std::string dots2cwd = dots2root + tmpdir + "/";
    EXPECT_OK(symlink(dots2cwd.c_str(), TmpFile(TOPDIR "/dsymlink.relative_in")));
    std::string dots2etc = dots2root + "/etc/";
    EXPECT_OK(symlink(dots2etc.c_str(), TmpFile(TOPDIR "/dsymlink.relative_out")));
    EXPECT_OK(symlink("../", TmpFile(SUBDIR "/dsymlink.up")));

    // Open directory FDs for those directories and for cwd.
    dir_fd_ = open(TmpFile(TOPDIR), O_RDONLY);
    EXPECT_OK(dir_fd_);
    sub_fd_ = open(TmpFile(SUBDIR), O_RDONLY);
    EXPECT_OK(sub_fd_);
    cwd_ = openat(AT_FDCWD, ".", O_RDONLY);
    EXPECT_OK(cwd_);
    // Move into the directory for the test.
    EXPECT_OK(fchdir(dir_fd_));
  }
  ~OpenatTest() {
    fchdir(cwd_);
    close(cwd_);
    close(sub_fd_);
    close(dir_fd_);
    unlink(TmpFile(SUBDIR "/symlink.up"));
    unlink(TmpFile(SUBDIR "/symlink.absolute_in"));
    unlink(TmpFile(TOPDIR "/symlink.absolute_out"));
    unlink(TmpFile(TOPDIR "/symlink.relative_in"));
    unlink(TmpFile(TOPDIR "/symlink.relative_out"));
    unlink(TmpFile(TOPDIR "/symlink.down"));
    unlink(TmpFile(TOPDIR "/symlink.samedir"));
    unlink(TmpFile(SUBDIR "/dsymlink.up"));
    unlink(TmpFile(SUBDIR "/dsymlink.absolute_in"));
    unlink(TmpFile(TOPDIR "/dsymlink.absolute_out"));
    unlink(TmpFile(TOPDIR "/dsymlink.relative_in"));
    unlink(TmpFile(TOPDIR "/dsymlink.relative_out"));
    unlink(TmpFile(TOPDIR "/dsymlink.down"));
    unlink(TmpFile(TOPDIR "/dsymlink.samedir"));
    unlink(TmpFile(SUBDIR "/bottomfile"));
    unlink(TmpFile(TOPDIR "/topfile"));
    rmdir(TmpFile(SUBDIR));
    rmdir(TmpFile(TOPDIR));
  }

  // Check openat(2) policing that is common across capabilities, capability mode and O_BENEATH.
  void CheckPolicing(int oflag) {
    // OK for normal access.
    EXPECT_OPEN_OK(openat(dir_fd_, "topfile", O_RDONLY|oflag));
    EXPECT_OPEN_OK(openat(dir_fd_, "subdir/bottomfile", O_RDONLY|oflag));
    EXPECT_OPEN_OK(openat(sub_fd_, "bottomfile", O_RDONLY|oflag));
    EXPECT_OPEN_OK(openat(sub_fd_, ".", O_RDONLY|oflag));

    // Can't open paths with ".." in them.
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "../topfile", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "../subdir/bottomfile", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "..", O_RDONLY|oflag);

#ifdef HAVE_OPENAT_INTERMEDIATE_DOTDOT
    // OK for dotdot lookups that don't escape the top directory
    EXPECT_OPEN_OK(openat(dir_fd_, "subdir/../topfile", O_RDONLY|oflag));
#endif

    // Check that we can't escape the top directory by the cunning
    // ruse of going via a subdirectory.
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "subdir/../../etc/passwd", O_RDONLY|oflag);

    // Should only be able to open symlinks that stay within the directory.
    EXPECT_OPEN_OK(openat(dir_fd_, "symlink.samedir", O_RDONLY|oflag));
    EXPECT_OPEN_OK(openat(dir_fd_, "symlink.down", O_RDONLY|oflag));
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "symlink.absolute_out", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "symlink.relative_in", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "symlink.relative_out", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "symlink.absolute_in", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "symlink.up", O_RDONLY|oflag);

    EXPECT_OPEN_OK(openat(dir_fd_, "dsymlink.samedir/topfile", O_RDONLY|oflag));
    EXPECT_OPEN_OK(openat(dir_fd_, "dsymlink.down/bottomfile", O_RDONLY|oflag));
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "dsymlink.absolute_out/passwd", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "dsymlink.relative_in/topfile", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "dsymlink.relative_out/passwd", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "dsymlink.absolute_in/topfile", O_RDONLY|oflag);
    EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "dsymlink.up/topfile", O_RDONLY|oflag);

    // Although recall that O_NOFOLLOW prevents symlink following in final component.
    EXPECT_SYSCALL_FAIL(E_TOO_MANY_LINKS, openat(dir_fd_, "symlink.samedir", O_RDONLY|O_NOFOLLOW|oflag));
    EXPECT_SYSCALL_FAIL(E_TOO_MANY_LINKS, openat(dir_fd_, "symlink.down", O_RDONLY|O_NOFOLLOW|oflag));
  }

 protected:
  int dir_fd_;
  int sub_fd_;
  int cwd_;
};

TEST_F(OpenatTest, WithCapability) {
  // Any kind of symlink can be opened relative to an ordinary directory FD.
  EXPECT_OPEN_OK(openat(dir_fd_, "symlink.samedir", O_RDONLY));
  EXPECT_OPEN_OK(openat(dir_fd_, "symlink.down", O_RDONLY));
  EXPECT_OPEN_OK(openat(dir_fd_, "symlink.absolute_out", O_RDONLY));
  EXPECT_OPEN_OK(openat(dir_fd_, "symlink.relative_in", O_RDONLY));
  EXPECT_OPEN_OK(openat(dir_fd_, "symlink.relative_out", O_RDONLY));
  EXPECT_OPEN_OK(openat(sub_fd_, "symlink.absolute_in", O_RDONLY));
  EXPECT_OPEN_OK(openat(sub_fd_, "symlink.up", O_RDONLY));

  // Now make both DFDs into Capsicum capabilities.
  cap_rights_t r_rl;
  cap_rights_init(&r_rl, CAP_READ, CAP_LOOKUP, CAP_FCHDIR);
  EXPECT_OK(cap_rights_limit(dir_fd_, &r_rl));
  EXPECT_OK(cap_rights_limit(sub_fd_, &r_rl));
  CheckPolicing(0);
  // Use of AT_FDCWD is independent of use of a capability.
  // Can open paths starting with "/" against a capability dfd, because the dfd is ignored.
}

FORK_TEST_F(OpenatTest, InCapabilityMode) {
  EXPECT_OK(cap_enter());  // Enter capability mode
  CheckPolicing(0);

  // Use of AT_FDCWD is banned in capability mode.
  EXPECT_CAPMODE(openat(AT_FDCWD, "topfile", O_RDONLY));
  EXPECT_CAPMODE(openat(AT_FDCWD, "subdir/bottomfile", O_RDONLY));
  EXPECT_CAPMODE(openat(AT_FDCWD, "/etc/passwd", O_RDONLY));

  // Can't open paths starting with "/" in capability mode.
  EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "/etc/passwd", O_RDONLY);
  EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "/etc/passwd", O_RDONLY);
}

#ifdef O_BENEATH
TEST_F(OpenatTest, WithFlag) {
  CheckPolicing(O_BENEATH);

  // Check with AT_FDCWD.
  EXPECT_OPEN_OK(openat(AT_FDCWD, "topfile", O_RDONLY|O_BENEATH));
  EXPECT_OPEN_OK(openat(AT_FDCWD, "subdir/bottomfile", O_RDONLY|O_BENEATH));

  // Can't open paths starting with "/" with O_BENEATH specified.
  EXPECT_OPENAT_FAIL_TRAVERSAL(AT_FDCWD, "/etc/passwd", O_RDONLY|O_BENEATH);
  EXPECT_OPENAT_FAIL_TRAVERSAL(dir_fd_, "/etc/passwd", O_RDONLY|O_BENEATH);
  EXPECT_OPENAT_FAIL_TRAVERSAL(sub_fd_, "/etc/passwd", O_RDONLY|O_BENEATH);
}

FORK_TEST_F(OpenatTest, WithFlagInCapabilityMode) {
  EXPECT_OK(cap_enter());  // Enter capability mode
  CheckPolicing(O_BENEATH);
}
#endif
