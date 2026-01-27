// Test that ioctl works in capability mode.
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "capsicum.h"
#include "capsicum-test.h"

// Ensure that ioctl() works consistently for both regular file descriptors and
// capability-wrapped ones.
TEST(Ioctl, Basic) {
  cap_rights_t rights_ioctl;
  cap_rights_init(&rights_ioctl, CAP_IOCTL);
  cap_rights_t rights_many;
  cap_rights_init(&rights_many, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_FSTAT, CAP_FSYNC);

  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);
  int fd_no = dup(fd);
  EXPECT_OK(fd_no);
  EXPECT_OK(cap_rights_limit(fd, &rights_ioctl));
  EXPECT_OK(cap_rights_limit(fd_no, &rights_many));

  // Check that CAP_IOCTL is required.
  int bytes;
  EXPECT_OK(ioctl(fd, FIONREAD, &bytes));
  EXPECT_NOTCAPABLE(ioctl(fd_no, FIONREAD, &bytes));

  int one = 1;
  EXPECT_OK(ioctl(fd, FIOCLEX, &one));
  EXPECT_NOTCAPABLE(ioctl(fd_no, FIOCLEX, &one));

  close(fd);
  close(fd_no);
}

#ifdef HAVE_CAP_IOCTLS_LIMIT
TEST(Ioctl, SubRightNormalFD) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);

  // Restrict the ioctl(2) subrights of a normal FD.
  cap_ioctl_t ioctl_nread = FIONREAD;
  EXPECT_OK(cap_ioctls_limit(fd, &ioctl_nread, 1));
  int bytes;
  EXPECT_OK(ioctl(fd, FIONREAD, &bytes));
  int one = 1;
  EXPECT_NOTCAPABLE(ioctl(fd, FIOCLEX, &one));

  // Expect to have all primary rights.
  cap_rights_t rights;
  EXPECT_OK(cap_rights_get(fd, &rights));
  cap_rights_t all;
  CAP_SET_ALL(&all);
  EXPECT_RIGHTS_EQ(&all, &rights);
  cap_ioctl_t ioctls[16];
  memset(ioctls, 0, sizeof(ioctls));
  ssize_t nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(1, nioctls);
  EXPECT_EQ((cap_ioctl_t)FIONREAD, ioctls[0]);

  // Can't widen the subrights.
  cap_ioctl_t both_ioctls[2] = {FIONREAD, FIOCLEX};
  EXPECT_NOTCAPABLE(cap_ioctls_limit(fd, both_ioctls, 2));

  close(fd);
}

TEST(Ioctl, PreserveSubRights) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_IOCTL);
  EXPECT_OK(cap_rights_limit(fd, &rights));
  cap_ioctl_t ioctl_nread = FIONREAD;
  EXPECT_OK(cap_ioctls_limit(fd, &ioctl_nread, 1));

  cap_rights_t cur_rights;
  cap_ioctl_t ioctls[16];
  ssize_t nioctls;
  EXPECT_OK(cap_rights_get(fd, &cur_rights));
  EXPECT_RIGHTS_EQ(&rights, &cur_rights);
  nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(1, nioctls);
  EXPECT_EQ((cap_ioctl_t)FIONREAD, ioctls[0]);

  // Limiting the top-level rights leaves the subrights unaffected...
  cap_rights_clear(&rights, CAP_READ);
  EXPECT_OK(cap_rights_limit(fd, &rights));
  nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(1, nioctls);
  EXPECT_EQ((cap_ioctl_t)FIONREAD, ioctls[0]);

  // ... until we remove CAP_IOCTL
  cap_rights_clear(&rights, CAP_IOCTL);
  EXPECT_OK(cap_rights_limit(fd, &rights));
  nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(0, nioctls);
  EXPECT_EQ(-1, cap_ioctls_limit(fd, &ioctl_nread, 1));

  close(fd);
}

TEST(Ioctl, SubRights) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);

  cap_ioctl_t ioctls[16];
  ssize_t nioctls;
  memset(ioctls, 0, sizeof(ioctls));
  nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(CAP_IOCTLS_ALL, nioctls);

  cap_rights_t rights_ioctl;
  cap_rights_init(&rights_ioctl, CAP_IOCTL);
  EXPECT_OK(cap_rights_limit(fd, &rights_ioctl));

  nioctls = cap_ioctls_get(fd, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(CAP_IOCTLS_ALL, nioctls);

  // Check operations that need CAP_IOCTL with subrights pristine => OK.
  int bytes;
  EXPECT_OK(ioctl(fd, FIONREAD, &bytes));
  int one = 1;
  EXPECT_OK(ioctl(fd, FIOCLEX, &one));

  // Check operations that need CAP_IOCTL with all relevant subrights => OK.
  cap_ioctl_t both_ioctls[2] = {FIONREAD, FIOCLEX};
  EXPECT_OK(cap_ioctls_limit(fd, both_ioctls, 2));
  EXPECT_OK(ioctl(fd, FIONREAD, &bytes));
  EXPECT_OK(ioctl(fd, FIOCLEX, &one));


  // Check what happens if we ask for subrights but don't have the space for them.
  cap_ioctl_t before = 0xBBBBBBBB;
  cap_ioctl_t one_ioctl = 0;
  cap_ioctl_t after = 0xAAAAAAAA;
  nioctls = cap_ioctls_get(fd, &one_ioctl, 1);
  EXPECT_EQ(2, nioctls);
  EXPECT_EQ(0xBBBBBBBB, before);
  EXPECT_TRUE(one_ioctl == FIONREAD || one_ioctl == FIOCLEX);
  EXPECT_EQ(0xAAAAAAAA, after);

  // Check operations that need CAP_IOCTL with particular subrights.
  int fd_nread = dup(fd);
  int fd_clex = dup(fd);
  cap_ioctl_t ioctl_nread = FIONREAD;
  cap_ioctl_t ioctl_clex = FIOCLEX;
  EXPECT_OK(cap_ioctls_limit(fd_nread, &ioctl_nread, 1));
  EXPECT_OK(cap_ioctls_limit(fd_clex, &ioctl_clex, 1));
  EXPECT_OK(ioctl(fd_nread, FIONREAD, &bytes));
  EXPECT_NOTCAPABLE(ioctl(fd_clex, FIONREAD, &bytes));
  EXPECT_OK(ioctl(fd_clex, FIOCLEX, &one));
  EXPECT_NOTCAPABLE(ioctl(fd_nread, FIOCLEX, &one));

  // Also check we can retrieve the subrights.
  memset(ioctls, 0, sizeof(ioctls));
  nioctls = cap_ioctls_get(fd_nread, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(1, nioctls);
  EXPECT_EQ((cap_ioctl_t)FIONREAD, ioctls[0]);
  memset(ioctls, 0, sizeof(ioctls));
  nioctls = cap_ioctls_get(fd_clex, ioctls, 16);
  EXPECT_OK(nioctls);
  EXPECT_EQ(1, nioctls);
  EXPECT_EQ((cap_ioctl_t)FIOCLEX, ioctls[0]);
  // And that we can't widen the subrights.
  EXPECT_NOTCAPABLE(cap_ioctls_limit(fd_nread, both_ioctls, 2));
  EXPECT_NOTCAPABLE(cap_ioctls_limit(fd_clex, both_ioctls, 2));
  close(fd_nread);
  close(fd_clex);

  // Check operations that need CAP_IOCTL with no subrights => ENOTCAPABLE.
  EXPECT_OK(cap_ioctls_limit(fd, NULL, 0));
  EXPECT_NOTCAPABLE(ioctl(fd, FIONREAD, &bytes));
  EXPECT_NOTCAPABLE(ioctl(fd, FIOCLEX, &one));

  close(fd);
}

#ifdef CAP_IOCTLS_LIMIT_MAX
TEST(Ioctl, TooManySubRights) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);

  cap_ioctl_t ioctls[CAP_IOCTLS_LIMIT_MAX + 1];
  for (int ii = 0; ii <= CAP_IOCTLS_LIMIT_MAX; ii++) {
    ioctls[ii] = ii + 1;
  }

  cap_rights_t rights_ioctl;
  cap_rights_init(&rights_ioctl, CAP_IOCTL);
  EXPECT_OK(cap_rights_limit(fd, &rights_ioctl));

  // Can only limit to a certain number of ioctls
  EXPECT_EQ(-1, cap_ioctls_limit(fd, ioctls, CAP_IOCTLS_LIMIT_MAX + 1));
  EXPECT_EQ(EINVAL, errno);
  EXPECT_OK(cap_ioctls_limit(fd, ioctls, CAP_IOCTLS_LIMIT_MAX));

  close(fd);
}
#else
TEST(Ioctl, ManySubRights) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);

  const int nioctls = 150000;
  cap_ioctl_t* ioctls = (cap_ioctl_t*)calloc(nioctls, sizeof(cap_ioctl_t));
  for (int ii = 0; ii < nioctls; ii++) {
    ioctls[ii] = ii + 1;
  }

  cap_rights_t rights_ioctl;
  cap_rights_init(&rights_ioctl, CAP_IOCTL);
  EXPECT_OK(cap_rights_limit(fd, &rights_ioctl));

  EXPECT_OK(cap_ioctls_limit(fd, ioctls, nioctls));
  // Limit to a subset; if this takes a long time then there's an
  // O(N^2) implementation of the ioctl list comparison.
  EXPECT_OK(cap_ioctls_limit(fd, ioctls, nioctls - 1));

  close(fd);
}
#endif

#endif
