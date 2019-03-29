#include <stdio.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

/* Utilities for printing rights information */
/* Written in C style to allow for: */
/* TODO(drysdale): migrate these to somewhere in libcaprights/ */
#define RIGHTS_INFO(RR) { (RR), #RR}
typedef struct {
  uint64_t right;
  const char* name;
} right_info;
static right_info known_rights[] = {
  /* Rights that are common to all versions of Capsicum */
  RIGHTS_INFO(CAP_READ),
  RIGHTS_INFO(CAP_WRITE),
  RIGHTS_INFO(CAP_SEEK_TELL),
  RIGHTS_INFO(CAP_SEEK),
  RIGHTS_INFO(CAP_PREAD),
  RIGHTS_INFO(CAP_PWRITE),
  RIGHTS_INFO(CAP_MMAP),
  RIGHTS_INFO(CAP_MMAP_R),
  RIGHTS_INFO(CAP_MMAP_W),
  RIGHTS_INFO(CAP_MMAP_X),
  RIGHTS_INFO(CAP_MMAP_RW),
  RIGHTS_INFO(CAP_MMAP_RX),
  RIGHTS_INFO(CAP_MMAP_WX),
  RIGHTS_INFO(CAP_MMAP_RWX),
  RIGHTS_INFO(CAP_CREATE),
  RIGHTS_INFO(CAP_FEXECVE),
  RIGHTS_INFO(CAP_FSYNC),
  RIGHTS_INFO(CAP_FTRUNCATE),
  RIGHTS_INFO(CAP_LOOKUP),
  RIGHTS_INFO(CAP_FCHDIR),
  RIGHTS_INFO(CAP_FCHFLAGS),
  RIGHTS_INFO(CAP_CHFLAGSAT),
  RIGHTS_INFO(CAP_FCHMOD),
  RIGHTS_INFO(CAP_FCHMODAT),
  RIGHTS_INFO(CAP_FCHOWN),
  RIGHTS_INFO(CAP_FCHOWNAT),
  RIGHTS_INFO(CAP_FCNTL),
  RIGHTS_INFO(CAP_FLOCK),
  RIGHTS_INFO(CAP_FPATHCONF),
  RIGHTS_INFO(CAP_FSCK),
  RIGHTS_INFO(CAP_FSTAT),
  RIGHTS_INFO(CAP_FSTATAT),
  RIGHTS_INFO(CAP_FSTATFS),
  RIGHTS_INFO(CAP_FUTIMES),
  RIGHTS_INFO(CAP_FUTIMESAT),
  RIGHTS_INFO(CAP_MKDIRAT),
  RIGHTS_INFO(CAP_MKFIFOAT),
  RIGHTS_INFO(CAP_MKNODAT),
  RIGHTS_INFO(CAP_RENAMEAT_SOURCE),
  RIGHTS_INFO(CAP_SYMLINKAT),
  RIGHTS_INFO(CAP_UNLINKAT),
  RIGHTS_INFO(CAP_ACCEPT),
  RIGHTS_INFO(CAP_BIND),
  RIGHTS_INFO(CAP_CONNECT),
  RIGHTS_INFO(CAP_GETPEERNAME),
  RIGHTS_INFO(CAP_GETSOCKNAME),
  RIGHTS_INFO(CAP_GETSOCKOPT),
  RIGHTS_INFO(CAP_LISTEN),
  RIGHTS_INFO(CAP_PEELOFF),
  RIGHTS_INFO(CAP_RECV),
  RIGHTS_INFO(CAP_SEND),
  RIGHTS_INFO(CAP_SETSOCKOPT),
  RIGHTS_INFO(CAP_SHUTDOWN),
  RIGHTS_INFO(CAP_BINDAT),
  RIGHTS_INFO(CAP_CONNECTAT),
  RIGHTS_INFO(CAP_LINKAT_SOURCE),
  RIGHTS_INFO(CAP_RENAMEAT_TARGET),
  RIGHTS_INFO(CAP_SOCK_CLIENT),
  RIGHTS_INFO(CAP_SOCK_SERVER),
  RIGHTS_INFO(CAP_MAC_GET),
  RIGHTS_INFO(CAP_MAC_SET),
  RIGHTS_INFO(CAP_SEM_GETVALUE),
  RIGHTS_INFO(CAP_SEM_POST),
  RIGHTS_INFO(CAP_SEM_WAIT),
  RIGHTS_INFO(CAP_EVENT),
  RIGHTS_INFO(CAP_KQUEUE_EVENT),
  RIGHTS_INFO(CAP_IOCTL),
  RIGHTS_INFO(CAP_TTYHOOK),
  RIGHTS_INFO(CAP_PDWAIT),
  RIGHTS_INFO(CAP_PDGETPID),
  RIGHTS_INFO(CAP_PDKILL),
  RIGHTS_INFO(CAP_EXTATTR_DELETE),
  RIGHTS_INFO(CAP_EXTATTR_GET),
  RIGHTS_INFO(CAP_EXTATTR_LIST),
  RIGHTS_INFO(CAP_EXTATTR_SET),
  RIGHTS_INFO(CAP_ACL_CHECK),
  RIGHTS_INFO(CAP_ACL_DELETE),
  RIGHTS_INFO(CAP_ACL_GET),
  RIGHTS_INFO(CAP_ACL_SET),
  RIGHTS_INFO(CAP_KQUEUE_CHANGE),
  RIGHTS_INFO(CAP_KQUEUE),
  /* Rights that are only present in some version or some OS, and so are #ifdef'ed */
  /* LINKAT got split */
#ifdef CAP_LINKAT
  RIGHTS_INFO(CAP_LINKAT),
#endif
#ifdef CAP_LINKAT_SOURCE
  RIGHTS_INFO(CAP_LINKAT_SOURCE),
#endif
#ifdef CAP_LINKAT_TARGET
  RIGHTS_INFO(CAP_LINKAT_TARGET),
#endif
  /* Linux aliased some FD operations for pdgetpid/pdkill */
#ifdef CAP_PDGETPID_FREEBSD
  RIGHTS_INFO(CAP_PDGETPID_FREEBSD),
#endif
#ifdef CAP_PDKILL_FREEBSD
  RIGHTS_INFO(CAP_PDKILL_FREEBSD),
#endif
  /* Linux-specific rights */
#ifdef CAP_FSIGNAL
  RIGHTS_INFO(CAP_FSIGNAL),
#endif
#ifdef CAP_EPOLL_CTL
  RIGHTS_INFO(CAP_EPOLL_CTL),
#endif
#ifdef CAP_NOTIFY
  RIGHTS_INFO(CAP_NOTIFY),
#endif
#ifdef CAP_SETNS
  RIGHTS_INFO(CAP_SETNS),
#endif
#ifdef CAP_PERFMON
  RIGHTS_INFO(CAP_PERFMON),
#endif
#ifdef CAP_BPF
  RIGHTS_INFO(CAP_BPF),
#endif
  /* Rights in later versions of FreeBSD (>10.0) */
};

void ShowCapRights(FILE *out, int fd) {
  size_t ii;
  bool first = true;
  cap_rights_t rights;
  CAP_SET_NONE(&rights);
  if (cap_rights_get(fd, &rights) < 0) {
    fprintf(out, "Failed to get rights for fd %d: errno %d\n", fd, errno);
    return;
  }

  /* First print out all known rights */
  size_t num_known = (sizeof(known_rights)/sizeof(known_rights[0]));
  for (ii = 0; ii < num_known; ii++) {
    if (cap_rights_is_set(&rights, known_rights[ii].right)) {
      if (!first) fprintf(out, ",");
      first = false;
      fprintf(out, "%s", known_rights[ii].name);
    }
  }
  /* Now repeat the loop, clearing rights we know of; this needs to be
   * a separate loop because some named rights overlap.
   */
  for (ii = 0; ii < num_known; ii++) {
    cap_rights_clear(&rights, known_rights[ii].right);
  }
  /* The following relies on the internal structure of cap_rights_t to
   * try to show rights we don't know about. */
  for (ii = 0; ii < (size_t)CAPARSIZE(&rights); ii++) {
    uint64_t bits = (rights.cr_rights[0] & 0x01ffffffffffffffULL);
    if (bits != 0) {
      uint64_t which = 1;
      for (which = 1; which < 0x0200000000000000 ; which <<= 1) {
        if (bits & which) {
          if (!first) fprintf(out, ",");
          fprintf(out, "CAP_RIGHT(%d, 0x%016llxULL)", (int)ii, (long long unsigned)which);
        }
      }
    }
  }
  fprintf(out, "\n");
}

void ShowAllCapRights(FILE *out) {
  int fd;
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) != 0) {
    fprintf(out, "Failed to getrlimit for max FDs: errno %d\n", errno);
    return;
  }
  for (fd = 0; fd < (int)limits.rlim_cur; fd++) {
    if (fcntl(fd, F_GETFD, 0) != 0) {
      continue;
    }
    fprintf(out, "fd %d: ", fd);
    ShowCapRights(out, fd);
  }
}

FORK_TEST(Capability, CapNew) {
  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_all;
  CAP_SET_ALL(&r_all);

  int cap_fd = dup(STDOUT_FILENO);
  cap_rights_t rights;
  CAP_SET_NONE(&rights);
  EXPECT_OK(cap_rights_get(cap_fd, &rights));
  EXPECT_RIGHTS_EQ(&r_all, &rights);

  EXPECT_OK(cap_fd);
  EXPECT_OK(cap_rights_limit(cap_fd, &r_rws));
  if (cap_fd < 0) return;
  int rc = write(cap_fd, "OK!\n", 4);
  EXPECT_OK(rc);
  EXPECT_EQ(4, rc);
  EXPECT_OK(cap_rights_get(cap_fd, &rights));
  EXPECT_RIGHTS_EQ(&r_rws, &rights);

  // dup/dup2 should preserve rights.
  int cap_dup = dup(cap_fd);
  EXPECT_OK(cap_dup);
  EXPECT_OK(cap_rights_get(cap_dup, &rights));
  EXPECT_RIGHTS_EQ(&r_rws, &rights);
  close(cap_dup);
  EXPECT_OK(dup2(cap_fd, cap_dup));
  EXPECT_OK(cap_rights_get(cap_dup, &rights));
  EXPECT_RIGHTS_EQ(&r_rws, &rights);
  close(cap_dup);
#ifdef HAVE_DUP3
  EXPECT_OK(dup3(cap_fd, cap_dup, 0));
  EXPECT_OK(cap_rights_get(cap_dup, &rights));
  EXPECT_RIGHTS_EQ(&r_rws, &rights);
  close(cap_dup);
#endif

  // Try to get a disjoint set of rights in a sub-capability.
  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_rsmapchmod;
  cap_rights_init(&r_rsmapchmod, CAP_READ, CAP_SEEK, CAP_MMAP, CAP_FCHMOD);
  int cap_cap_fd = dup(cap_fd);
  EXPECT_OK(cap_cap_fd);
  EXPECT_NOTCAPABLE(cap_rights_limit(cap_cap_fd, &r_rsmapchmod));

  // Dump rights info to stderr (mostly to ensure that Show[All]CapRights()
  // is working.
  ShowAllCapRights(stderr);

  EXPECT_OK(close(cap_fd));
}

FORK_TEST(Capability, CapEnter) {
  EXPECT_EQ(0, cap_enter());
}

FORK_TEST(Capability, BasicInterception) {
  cap_rights_t r_0;
  cap_rights_init(&r_0, 0);
  int cap_fd = dup(1);
  EXPECT_OK(cap_fd);
  EXPECT_OK(cap_rights_limit(cap_fd, &r_0));

  EXPECT_NOTCAPABLE(write(cap_fd, "", 0));

  EXPECT_OK(cap_enter());  // Enter capability mode

  EXPECT_NOTCAPABLE(write(cap_fd, "", 0));

  // Create a new capability which does have write permission
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  int cap_fd2 = dup(1);
  EXPECT_OK(cap_fd2);
  EXPECT_OK(cap_rights_limit(cap_fd2, &r_ws));
  EXPECT_OK(write(cap_fd2, "", 0));

  // Tidy up.
  if (cap_fd >= 0) close(cap_fd);
  if (cap_fd2 >= 0) close(cap_fd2);
}

FORK_TEST_ON(Capability, OpenAtDirectoryTraversal, TmpFile("cap_openat_testfile")) {
  int dir = open(tmpdir.c_str(), O_RDONLY);
  EXPECT_OK(dir);

  cap_enter();

  int file = openat(dir, "cap_openat_testfile", O_RDONLY|O_CREAT, 0644);
  EXPECT_OK(file);

  // Test that we are confined to /tmp, and cannot
  // escape using absolute paths or ../.
  int new_file = openat(dir, "../dev/null", O_RDONLY);
  EXPECT_EQ(-1, new_file);

  new_file = openat(dir, "..", O_RDONLY);
  EXPECT_EQ(-1, new_file);

  new_file = openat(dir, "/dev/null", O_RDONLY);
  EXPECT_EQ(-1, new_file);

  new_file = openat(dir, "/", O_RDONLY);
  EXPECT_EQ(-1, new_file);

  // Tidy up.
  close(file);
  close(dir);
}

FORK_TEST_ON(Capability, FileInSync, TmpFile("cap_file_sync")) {
  int fd = open(TmpFile("cap_file_sync"), O_RDWR|O_CREAT, 0644);
  EXPECT_OK(fd);
  const char* message = "Hello capability world";
  EXPECT_OK(write(fd, message, strlen(message)));

  cap_rights_t r_rsstat;
  cap_rights_init(&r_rsstat, CAP_READ, CAP_SEEK, CAP_FSTAT);

  int cap_fd = dup(fd);
  EXPECT_OK(cap_fd);
  EXPECT_OK(cap_rights_limit(cap_fd, &r_rsstat));
  int cap_cap_fd = dup(cap_fd);
  EXPECT_OK(cap_cap_fd);
  EXPECT_OK(cap_rights_limit(cap_cap_fd, &r_rsstat));

  EXPECT_OK(cap_enter());  // Enter capability mode.

  // Changes to one file descriptor affect the others.
  EXPECT_EQ(1, lseek(fd, 1, SEEK_SET));
  EXPECT_EQ(1, lseek(fd, 0, SEEK_CUR));
  EXPECT_EQ(1, lseek(cap_fd, 0, SEEK_CUR));
  EXPECT_EQ(1, lseek(cap_cap_fd, 0, SEEK_CUR));
  EXPECT_EQ(3, lseek(cap_fd, 3, SEEK_SET));
  EXPECT_EQ(3, lseek(fd, 0, SEEK_CUR));
  EXPECT_EQ(3, lseek(cap_fd, 0, SEEK_CUR));
  EXPECT_EQ(3, lseek(cap_cap_fd, 0, SEEK_CUR));
  EXPECT_EQ(5, lseek(cap_cap_fd, 5, SEEK_SET));
  EXPECT_EQ(5, lseek(fd, 0, SEEK_CUR));
  EXPECT_EQ(5, lseek(cap_fd, 0, SEEK_CUR));
  EXPECT_EQ(5, lseek(cap_cap_fd, 0, SEEK_CUR));

  close(cap_cap_fd);
  close(cap_fd);
  close(fd);
}

// Create a capability on /tmp that does not allow CAP_WRITE,
// and check that this restriction is inherited through openat().
FORK_TEST_ON(Capability, Inheritance, TmpFile("cap_openat_write_testfile")) {
  int dir = open(tmpdir.c_str(), O_RDONLY);
  EXPECT_OK(dir);

  cap_rights_t r_rl;
  cap_rights_init(&r_rl, CAP_READ, CAP_LOOKUP);

  int cap_dir = dup(dir);
  EXPECT_OK(cap_dir);
  EXPECT_OK(cap_rights_limit(cap_dir, &r_rl));

  const char *filename = "cap_openat_write_testfile";
  int file = openat(dir, filename, O_WRONLY|O_CREAT, 0644);
  EXPECT_OK(file);
  EXPECT_EQ(5, write(file, "TEST\n", 5));
  if (file >= 0) close(file);

  EXPECT_OK(cap_enter());
  file = openat(cap_dir, filename, O_RDONLY);
  EXPECT_OK(file);

  cap_rights_t rights;
  cap_rights_init(&rights, 0);
  EXPECT_OK(cap_rights_get(file, &rights));
  EXPECT_RIGHTS_EQ(&r_rl, &rights);
  if (file >= 0) close(file);

  file = openat(cap_dir, filename, O_WRONLY|O_APPEND);
  EXPECT_NOTCAPABLE(file);
  if (file > 0) close(file);

  if (dir > 0) close(dir);
  if (cap_dir > 0) close(cap_dir);
}


// Ensure that, if the capability had enough rights for the system call to
// pass, then it did. Otherwise, ensure that the errno is ENOTCAPABLE;
// capability restrictions should kick in before any other error logic.
#define CHECK_RIGHT_RESULT(result, rights, ...) do {    \
  cap_rights_t rights_needed;                           \
  cap_rights_init(&rights_needed, __VA_ARGS__);         \
  if (cap_rights_contains(&rights, &rights_needed)) {   \
    EXPECT_OK(result) << std::endl                      \
                      << " need: " << rights_needed     \
                      << std::endl                      \
                      << " got:  " << rights;           \
  } else {                                              \
    EXPECT_EQ(-1, result) << " need: " << rights_needed \
                          << std::endl                  \
                          << " got:  "<< rights;        \
    EXPECT_EQ(ENOTCAPABLE, errno);                      \
  }                                                     \
} while (0)

#define EXPECT_MMAP_NOTCAPABLE(result) do {         \
  void *rv = result;                                \
  EXPECT_EQ(MAP_FAILED, rv);                        \
  EXPECT_EQ(ENOTCAPABLE, errno);                    \
  if (rv != MAP_FAILED) munmap(rv, getpagesize());  \
} while (0)

#define EXPECT_MMAP_OK(result) do {                     \
  void *rv = result;                                    \
  EXPECT_NE(MAP_FAILED, rv) << " with errno " << errno; \
  if (rv != MAP_FAILED) munmap(rv, getpagesize());      \
} while (0)


// As above, but for the special mmap() case: unmap after successful mmap().
#define CHECK_RIGHT_MMAP_RESULT(result, rights, ...) do { \
  cap_rights_t rights_needed;                             \
  cap_rights_init(&rights_needed, __VA_ARGS__);           \
  if (cap_rights_contains(&rights, &rights_needed)) {     \
    EXPECT_MMAP_OK(result);                               \
  } else {                                                \
    EXPECT_MMAP_NOTCAPABLE(result);                       \
  }                                                       \
} while (0)

FORK_TEST_ON(Capability, Mmap, TmpFile("cap_mmap_operations")) {
  int fd = open(TmpFile("cap_mmap_operations"), O_RDWR | O_CREAT, 0644);
  EXPECT_OK(fd);
  if (fd < 0) return;

  cap_rights_t r_0;
  cap_rights_init(&r_0, 0);
  cap_rights_t r_mmap;
  cap_rights_init(&r_mmap, CAP_MMAP);
  cap_rights_t r_r;
  cap_rights_init(&r_r, CAP_PREAD);
  cap_rights_t r_rmmap;
  cap_rights_init(&r_rmmap, CAP_PREAD, CAP_MMAP);

  // If we're missing a capability, it will fail.
  int cap_none = dup(fd);
  EXPECT_OK(cap_none);
  EXPECT_OK(cap_rights_limit(cap_none, &r_0));
  int cap_mmap = dup(fd);
  EXPECT_OK(cap_mmap);
  EXPECT_OK(cap_rights_limit(cap_mmap, &r_mmap));
  int cap_read = dup(fd);
  EXPECT_OK(cap_read);
  EXPECT_OK(cap_rights_limit(cap_read, &r_r));
  int cap_both = dup(fd);
  EXPECT_OK(cap_both);
  EXPECT_OK(cap_rights_limit(cap_both, &r_rmmap));

  EXPECT_OK(cap_enter());  // Enter capability mode.

  EXPECT_MMAP_NOTCAPABLE(mmap(NULL, getpagesize(), PROT_READ, MAP_PRIVATE, cap_none, 0));
  EXPECT_MMAP_NOTCAPABLE(mmap(NULL, getpagesize(), PROT_READ, MAP_PRIVATE, cap_mmap, 0));
  EXPECT_MMAP_NOTCAPABLE(mmap(NULL, getpagesize(), PROT_READ, MAP_PRIVATE, cap_read, 0));

  EXPECT_MMAP_OK(mmap(NULL, getpagesize(), PROT_READ, MAP_PRIVATE, cap_both, 0));

  // A call with MAP_ANONYMOUS should succeed without any capability requirements.
  EXPECT_MMAP_OK(mmap(NULL, getpagesize(), PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));

  EXPECT_OK(close(cap_both));
  EXPECT_OK(close(cap_read));
  EXPECT_OK(close(cap_mmap));
  EXPECT_OK(close(cap_none));
  EXPECT_OK(close(fd));
}

// Given a file descriptor, create a capability with specific rights and
// make sure only those rights work.
#define TRY_FILE_OPS(fd, ...) do {       \
  cap_rights_t rights;                   \
  cap_rights_init(&rights, __VA_ARGS__); \
  TryFileOps((fd), rights);              \
} while (0)

static void TryFileOps(int fd, cap_rights_t rights) {
  int cap_fd = dup(fd);
  EXPECT_OK(cap_fd);
  EXPECT_OK(cap_rights_limit(cap_fd, &rights));
  if (cap_fd < 0) return;
  cap_rights_t erights;
  EXPECT_OK(cap_rights_get(cap_fd, &erights));
  EXPECT_RIGHTS_EQ(&rights, &erights);

  // Check creation of a capability from a capability.
  int cap_cap_fd = dup(cap_fd);
  EXPECT_OK(cap_cap_fd);
  EXPECT_OK(cap_rights_limit(cap_cap_fd, &rights));
  EXPECT_NE(cap_fd, cap_cap_fd);
  EXPECT_OK(cap_rights_get(cap_cap_fd, &erights));
  EXPECT_RIGHTS_EQ(&rights, &erights);
  close(cap_cap_fd);

  char ch;
  CHECK_RIGHT_RESULT(read(cap_fd, &ch, sizeof(ch)), rights, CAP_READ, CAP_SEEK_ASWAS);

  ssize_t len1 = pread(cap_fd, &ch, sizeof(ch), 0);
  CHECK_RIGHT_RESULT(len1, rights, CAP_PREAD);
  ssize_t len2 = pread(cap_fd, &ch, sizeof(ch), 0);
  CHECK_RIGHT_RESULT(len2, rights, CAP_PREAD);
  EXPECT_EQ(len1, len2);

  CHECK_RIGHT_RESULT(write(cap_fd, &ch, sizeof(ch)), rights, CAP_WRITE, CAP_SEEK_ASWAS);
  CHECK_RIGHT_RESULT(pwrite(cap_fd, &ch, sizeof(ch), 0), rights, CAP_PWRITE);
  CHECK_RIGHT_RESULT(lseek(cap_fd, 0, SEEK_SET), rights, CAP_SEEK);

#ifdef HAVE_CHFLAGS
  // Note: this is not expected to work over NFS.
  struct statfs sf;
  EXPECT_OK(fstatfs(fd, &sf));
  bool is_nfs = (strncmp("nfs", sf.f_fstypename, sizeof(sf.f_fstypename)) == 0);
  if (!is_nfs) {
    CHECK_RIGHT_RESULT(fchflags(cap_fd, UF_NODUMP), rights, CAP_FCHFLAGS);
  }
#endif

  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_NONE, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_READ, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_R);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_WRITE, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_W);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_EXEC, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_X);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_RW);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_READ | PROT_EXEC, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_RX);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_EXEC | PROT_WRITE, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_WX);
  CHECK_RIGHT_MMAP_RESULT(mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, cap_fd, 0),
                          rights, CAP_MMAP_RWX);

  CHECK_RIGHT_RESULT(fsync(cap_fd), rights, CAP_FSYNC);
#ifdef HAVE_SYNC_FILE_RANGE
  CHECK_RIGHT_RESULT(sync_file_range(cap_fd, 0, 1, 0), rights, CAP_FSYNC, CAP_SEEK);
#endif

  int rc = fcntl(cap_fd, F_GETFL);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FCNTL);
  rc = fcntl(cap_fd, F_SETFL, rc);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FCNTL);

  CHECK_RIGHT_RESULT(fchown(cap_fd, -1, -1), rights, CAP_FCHOWN);

  CHECK_RIGHT_RESULT(fchmod(cap_fd, 0644), rights, CAP_FCHMOD);

  CHECK_RIGHT_RESULT(flock(cap_fd, LOCK_SH), rights, CAP_FLOCK);
  CHECK_RIGHT_RESULT(flock(cap_fd, LOCK_UN), rights, CAP_FLOCK);

  CHECK_RIGHT_RESULT(ftruncate(cap_fd, 0), rights, CAP_FTRUNCATE);

  struct stat sb;
  CHECK_RIGHT_RESULT(fstat(cap_fd, &sb), rights, CAP_FSTAT);

  struct statfs cap_sf;
  CHECK_RIGHT_RESULT(fstatfs(cap_fd, &cap_sf), rights, CAP_FSTATFS);

#ifdef HAVE_FPATHCONF
  CHECK_RIGHT_RESULT(fpathconf(cap_fd, _PC_NAME_MAX), rights, CAP_FPATHCONF);
#endif

  CHECK_RIGHT_RESULT(futimes(cap_fd, NULL), rights, CAP_FUTIMES);

  struct pollfd pollfd;
  pollfd.fd = cap_fd;
  pollfd.events = POLLIN | POLLERR | POLLHUP;
  pollfd.revents = 0;
  int ret = poll(&pollfd, 1, 0);
  if (cap_rights_is_set(&rights, CAP_EVENT)) {
    EXPECT_OK(ret);
  } else {
    EXPECT_NE(0, (pollfd.revents & POLLNVAL));
  }

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100;
  fd_set rset;
  FD_ZERO(&rset);
  FD_SET(cap_fd, &rset);
  fd_set wset;
  FD_ZERO(&wset);
  FD_SET(cap_fd, &wset);
  ret = select(cap_fd+1, &rset, &wset, NULL, &tv);
  if (cap_rights_is_set(&rights, CAP_EVENT)) {
    EXPECT_OK(ret);
  } else {
    EXPECT_NOTCAPABLE(ret);
  }

  // TODO(FreeBSD): kqueue

  EXPECT_OK(close(cap_fd));
}

FORK_TEST_ON(Capability, Operations, TmpFile("cap_fd_operations")) {
  int fd = open(TmpFile("cap_fd_operations"), O_RDWR | O_CREAT, 0644);
  EXPECT_OK(fd);
  if (fd < 0) return;

  EXPECT_OK(cap_enter());  // Enter capability mode.

  // Try a variety of different combinations of rights - a full
  // enumeration is too large (2^N with N~30+) to perform.
  TRY_FILE_OPS(fd, CAP_READ);
  TRY_FILE_OPS(fd, CAP_PREAD);
  TRY_FILE_OPS(fd, CAP_WRITE);
  TRY_FILE_OPS(fd, CAP_PWRITE);
  TRY_FILE_OPS(fd, CAP_READ, CAP_WRITE);
  TRY_FILE_OPS(fd, CAP_PREAD, CAP_PWRITE);
  TRY_FILE_OPS(fd, CAP_SEEK);
  TRY_FILE_OPS(fd, CAP_FCHFLAGS);
  TRY_FILE_OPS(fd, CAP_IOCTL);
  TRY_FILE_OPS(fd, CAP_FSTAT);
  TRY_FILE_OPS(fd, CAP_MMAP);
  TRY_FILE_OPS(fd, CAP_MMAP_R);
  TRY_FILE_OPS(fd, CAP_MMAP_W);
  TRY_FILE_OPS(fd, CAP_MMAP_X);
  TRY_FILE_OPS(fd, CAP_MMAP_RW);
  TRY_FILE_OPS(fd, CAP_MMAP_RX);
  TRY_FILE_OPS(fd, CAP_MMAP_WX);
  TRY_FILE_OPS(fd, CAP_MMAP_RWX);
  TRY_FILE_OPS(fd, CAP_FCNTL);
  TRY_FILE_OPS(fd, CAP_EVENT);
  TRY_FILE_OPS(fd, CAP_FSYNC);
  TRY_FILE_OPS(fd, CAP_FCHOWN);
  TRY_FILE_OPS(fd, CAP_FCHMOD);
  TRY_FILE_OPS(fd, CAP_FTRUNCATE);
  TRY_FILE_OPS(fd, CAP_FLOCK);
  TRY_FILE_OPS(fd, CAP_FSTATFS);
  TRY_FILE_OPS(fd, CAP_FPATHCONF);
  TRY_FILE_OPS(fd, CAP_FUTIMES);
  TRY_FILE_OPS(fd, CAP_ACL_GET);
  TRY_FILE_OPS(fd, CAP_ACL_SET);
  TRY_FILE_OPS(fd, CAP_ACL_DELETE);
  TRY_FILE_OPS(fd, CAP_ACL_CHECK);
  TRY_FILE_OPS(fd, CAP_EXTATTR_GET);
  TRY_FILE_OPS(fd, CAP_EXTATTR_SET);
  TRY_FILE_OPS(fd, CAP_EXTATTR_DELETE);
  TRY_FILE_OPS(fd, CAP_EXTATTR_LIST);
  TRY_FILE_OPS(fd, CAP_MAC_GET);
  TRY_FILE_OPS(fd, CAP_MAC_SET);

  // Socket-specific.
  TRY_FILE_OPS(fd, CAP_GETPEERNAME);
  TRY_FILE_OPS(fd, CAP_GETSOCKNAME);
  TRY_FILE_OPS(fd, CAP_ACCEPT);

  close(fd);
}

#define TRY_DIR_OPS(dfd, ...) do {       \
  cap_rights_t rights;                   \
  cap_rights_init(&rights, __VA_ARGS__); \
  TryDirOps((dfd), rights);              \
} while (0)

static void TryDirOps(int dirfd, cap_rights_t rights) {
  cap_rights_t erights;
  int dfd_cap = dup(dirfd);
  EXPECT_OK(dfd_cap);
  EXPECT_OK(cap_rights_limit(dfd_cap, &rights));
  EXPECT_OK(cap_rights_get(dfd_cap, &erights));
  EXPECT_RIGHTS_EQ(&rights, &erights);

  int rc = openat(dfd_cap, "cap_create", O_CREAT | O_RDONLY, 0600);
  CHECK_RIGHT_RESULT(rc, rights, CAP_CREATE, CAP_READ, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
    EXPECT_OK(unlinkat(dirfd, "cap_create", 0));
  }
  rc = openat(dfd_cap, "cap_create", O_CREAT | O_WRONLY | O_APPEND, 0600);
  CHECK_RIGHT_RESULT(rc, rights, CAP_CREATE, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
    EXPECT_OK(unlinkat(dirfd, "cap_create", 0));
  }
  rc = openat(dfd_cap, "cap_create", O_CREAT | O_RDWR | O_APPEND, 0600);
  CHECK_RIGHT_RESULT(rc, rights, CAP_CREATE, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
    EXPECT_OK(unlinkat(dirfd, "cap_create", 0));
  }

  rc = openat(dirfd, "cap_faccess", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = faccessat(dfd_cap, "cap_faccess", F_OK, 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSTAT, CAP_LOOKUP);
  EXPECT_OK(unlinkat(dirfd, "cap_faccess", 0));

  rc = openat(dirfd, "cap_fsync", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = openat(dfd_cap, "cap_fsync", O_FSYNC | O_RDONLY);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSYNC, CAP_READ, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_FSYNC | O_WRONLY | O_APPEND);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSYNC, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_FSYNC | O_RDWR | O_APPEND);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSYNC, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_SYNC | O_RDONLY);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSYNC, CAP_READ, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_SYNC | O_WRONLY | O_APPEND);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSYNC, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_SYNC | O_RDWR | O_APPEND);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSYNC, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  EXPECT_OK(unlinkat(dirfd, "cap_fsync", 0));

  rc = openat(dirfd, "cap_ftruncate", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = openat(dfd_cap, "cap_ftruncate", O_TRUNC | O_RDONLY);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FTRUNCATE, CAP_READ, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_ftruncate", O_TRUNC | O_WRONLY);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FTRUNCATE, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_ftruncate", O_TRUNC | O_RDWR);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FTRUNCATE, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  EXPECT_OK(unlinkat(dirfd, "cap_ftruncate", 0));

  rc = openat(dfd_cap, "cap_create", O_CREAT | O_WRONLY, 0600);
  CHECK_RIGHT_RESULT(rc, rights, CAP_CREATE, CAP_WRITE, CAP_SEEK, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
    EXPECT_OK(unlinkat(dirfd, "cap_create", 0));
  }
  rc = openat(dfd_cap, "cap_create", O_CREAT | O_RDWR, 0600);
  CHECK_RIGHT_RESULT(rc, rights, CAP_CREATE, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
    EXPECT_OK(unlinkat(dirfd, "cap_create", 0));
  }

  rc = openat(dirfd, "cap_fsync", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = openat(dfd_cap, "cap_fsync", O_FSYNC | O_WRONLY);
  CHECK_RIGHT_RESULT(rc,
               rights, CAP_FSYNC, CAP_WRITE, CAP_SEEK, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_FSYNC | O_RDWR);
  CHECK_RIGHT_RESULT(rc,
               rights, CAP_FSYNC, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_SYNC | O_WRONLY);
  CHECK_RIGHT_RESULT(rc,
               rights, CAP_FSYNC, CAP_WRITE, CAP_SEEK, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  rc = openat(dfd_cap, "cap_fsync", O_SYNC | O_RDWR);
  CHECK_RIGHT_RESULT(rc,
               rights, CAP_FSYNC, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(close(rc));
  }
  EXPECT_OK(unlinkat(dirfd, "cap_fsync", 0));

#ifdef HAVE_CHFLAGSAT
  rc = openat(dirfd, "cap_chflagsat", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = chflagsat(dfd_cap, "cap_chflagsat", UF_NODUMP, 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_CHFLAGSAT, CAP_LOOKUP);
  EXPECT_OK(unlinkat(dirfd, "cap_chflagsat", 0));
#endif

  rc = openat(dirfd, "cap_fchownat", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = fchownat(dfd_cap, "cap_fchownat", -1, -1, 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FCHOWN, CAP_LOOKUP);
  EXPECT_OK(unlinkat(dirfd, "cap_fchownat", 0));

  rc = openat(dirfd, "cap_fchmodat", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = fchmodat(dfd_cap, "cap_fchmodat", 0600, 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FCHMOD, CAP_LOOKUP);
  EXPECT_OK(unlinkat(dirfd, "cap_fchmodat", 0));

  rc = openat(dirfd, "cap_fstatat", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  struct stat sb;
  rc = fstatat(dfd_cap, "cap_fstatat", &sb, 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FSTAT, CAP_LOOKUP);
  EXPECT_OK(unlinkat(dirfd, "cap_fstatat", 0));

  rc = openat(dirfd, "cap_futimesat", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = futimesat(dfd_cap, "cap_futimesat", NULL);
  CHECK_RIGHT_RESULT(rc, rights, CAP_FUTIMES, CAP_LOOKUP);
  EXPECT_OK(unlinkat(dirfd, "cap_futimesat", 0));

  // For linkat(2), need:
  //  - CAP_LINKAT_SOURCE on source
  //  - CAP_LINKAT_TARGET on destination.
  rc = openat(dirfd, "cap_linkat_src", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));

  rc = linkat(dirfd, "cap_linkat_src", dfd_cap, "cap_linkat_dst", 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_LINKAT_TARGET);
  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_linkat_dst", 0));
  }

  rc = linkat(dfd_cap, "cap_linkat_src", dirfd, "cap_linkat_dst", 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_LINKAT_SOURCE);
  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_linkat_dst", 0));
  }

  EXPECT_OK(unlinkat(dirfd, "cap_linkat_src", 0));

  rc = mkdirat(dfd_cap, "cap_mkdirat", 0700);
  CHECK_RIGHT_RESULT(rc, rights, CAP_MKDIRAT, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_mkdirat", AT_REMOVEDIR));
  }

#ifdef HAVE_MKFIFOAT
  rc = mkfifoat(dfd_cap, "cap_mkfifoat", 0600);
  CHECK_RIGHT_RESULT(rc, rights, CAP_MKFIFOAT, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_mkfifoat", 0));
  }
#endif

  if (getuid() == 0) {
    rc = mknodat(dfd_cap, "cap_mknodat", S_IFCHR | 0600, 0);
    CHECK_RIGHT_RESULT(rc, rights, CAP_MKNODAT, CAP_LOOKUP);
    if (rc >= 0) {
      EXPECT_OK(unlinkat(dirfd, "cap_mknodat", 0));
    }
  }

  // For renameat(2), need:
  //  - CAP_RENAMEAT_SOURCE on source
  //  - CAP_RENAMEAT_TARGET on destination.
  rc = openat(dirfd, "cap_renameat_src", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));

  rc = renameat(dirfd, "cap_renameat_src", dfd_cap, "cap_renameat_dst");
  CHECK_RIGHT_RESULT(rc, rights, CAP_RENAMEAT_TARGET);
  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_renameat_dst", 0));
  } else {
    EXPECT_OK(unlinkat(dirfd, "cap_renameat_src", 0));
  }

  rc = openat(dirfd, "cap_renameat_src", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));

  rc = renameat(dfd_cap, "cap_renameat_src", dirfd, "cap_renameat_dst");
  CHECK_RIGHT_RESULT(rc, rights, CAP_RENAMEAT_SOURCE);

  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_renameat_dst", 0));
  } else {
    EXPECT_OK(unlinkat(dirfd, "cap_renameat_src", 0));
  }

  rc = symlinkat("test", dfd_cap, "cap_symlinkat");
  CHECK_RIGHT_RESULT(rc, rights, CAP_SYMLINKAT, CAP_LOOKUP);
  if (rc >= 0) {
    EXPECT_OK(unlinkat(dirfd, "cap_symlinkat", 0));
  }

  rc = openat(dirfd, "cap_unlinkat", O_CREAT, 0600);
  EXPECT_OK(rc);
  EXPECT_OK(close(rc));
  rc = unlinkat(dfd_cap, "cap_unlinkat", 0);
  CHECK_RIGHT_RESULT(rc, rights, CAP_UNLINKAT, CAP_LOOKUP);
  unlinkat(dirfd, "cap_unlinkat", 0);
  EXPECT_OK(mkdirat(dirfd, "cap_unlinkat", 0700));
  rc = unlinkat(dfd_cap, "cap_unlinkat", AT_REMOVEDIR);
  CHECK_RIGHT_RESULT(rc, rights, CAP_UNLINKAT, CAP_LOOKUP);
  unlinkat(dirfd, "cap_unlinkat", AT_REMOVEDIR);

  EXPECT_OK(close(dfd_cap));
}

void DirOperationsTest(int extra) {
  int rc = mkdir(TmpFile("cap_dirops"), 0755);
  EXPECT_OK(rc);
  if (rc < 0 && errno != EEXIST) return;
  int dfd = open(TmpFile("cap_dirops"), O_RDONLY | O_DIRECTORY | extra);
  EXPECT_OK(dfd);
  int tmpfd = open(tmpdir.c_str(), O_RDONLY | O_DIRECTORY);
  EXPECT_OK(tmpfd);

  EXPECT_OK(cap_enter());  // Enter capability mode.

  TRY_DIR_OPS(dfd, CAP_LINKAT_SOURCE);
  TRY_DIR_OPS(dfd, CAP_LINKAT_TARGET);
  TRY_DIR_OPS(dfd, CAP_CREATE, CAP_READ, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_CREATE, CAP_WRITE, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_CREATE, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FSYNC, CAP_READ, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FSYNC, CAP_WRITE, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FSYNC, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FTRUNCATE, CAP_READ, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FTRUNCATE, CAP_WRITE, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FTRUNCATE, CAP_READ, CAP_WRITE, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FCHOWN, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FCHMOD, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FSTAT, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_FUTIMES, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_MKDIRAT, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_MKFIFOAT, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_MKNODAT, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_SYMLINKAT, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_UNLINKAT, CAP_LOOKUP);
  // Rename needs CAP_RENAMEAT_SOURCE on source directory and
  // CAP_RENAMEAT_TARGET on destination directory.
  TRY_DIR_OPS(dfd, CAP_RENAMEAT_SOURCE, CAP_UNLINKAT, CAP_LOOKUP);
  TRY_DIR_OPS(dfd, CAP_RENAMEAT_TARGET, CAP_UNLINKAT, CAP_LOOKUP);

  EXPECT_OK(unlinkat(tmpfd, "cap_dirops", AT_REMOVEDIR));
  EXPECT_OK(close(tmpfd));
  EXPECT_OK(close(dfd));
}

FORK_TEST(Capability, DirOperations) {
  DirOperationsTest(0);
}

#ifdef O_PATH
FORK_TEST(Capability, PathDirOperations) {
  // Make the dfd in the test a path-only file descriptor.
  DirOperationsTest(O_PATH);
}
#endif

static void TryReadWrite(int cap_fd) {
  char buffer[64];
  EXPECT_OK(read(cap_fd, buffer, sizeof(buffer)));
  int rc = write(cap_fd, "", 0);
  EXPECT_EQ(-1, rc);
  EXPECT_EQ(ENOTCAPABLE, errno);
}

FORK_TEST_ON(Capability, SocketTransfer, TmpFile("cap_fd_transfer")) {
  int sock_fds[2];
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds));

  struct msghdr mh;
  mh.msg_name = NULL;  // No address needed
  mh.msg_namelen = 0;
  char buffer1[1024];
  struct iovec iov[1];
  iov[0].iov_base = buffer1;
  iov[0].iov_len = sizeof(buffer1);
  mh.msg_iov = iov;
  mh.msg_iovlen = 1;
  char buffer2[1024];
  mh.msg_control = buffer2;
  mh.msg_controllen = sizeof(buffer2);
  struct cmsghdr *cmptr;

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);

  pid_t child = fork();
  if (child == 0) {
    // Child: enter cap mode
    EXPECT_OK(cap_enter());

    // Child: wait to receive FD over socket
    int rc = recvmsg(sock_fds[0], &mh, 0);
    EXPECT_OK(rc);
    EXPECT_LE(CMSG_LEN(sizeof(int)), mh.msg_controllen);
    cmptr = CMSG_FIRSTHDR(&mh);
    int cap_fd = *(int*)CMSG_DATA(cmptr);
    EXPECT_EQ(CMSG_LEN(sizeof(int)), cmptr->cmsg_len);
    cmptr = CMSG_NXTHDR(&mh, cmptr);
    EXPECT_TRUE(cmptr == NULL);

    // Child: confirm we can do the right operations on the capability
    cap_rights_t rights;
    EXPECT_OK(cap_rights_get(cap_fd, &rights));
    EXPECT_RIGHTS_EQ(&r_rs, &rights);
    TryReadWrite(cap_fd);

    // Child: wait for a normal read
    int val;
    read(sock_fds[0], &val, sizeof(val));
    exit(0);
  }

  int fd = open(TmpFile("cap_fd_transfer"), O_RDWR | O_CREAT, 0644);
  EXPECT_OK(fd);
  if (fd < 0) return;
  int cap_fd = dup(fd);
  EXPECT_OK(cap_fd);
  EXPECT_OK(cap_rights_limit(cap_fd, &r_rs));

  EXPECT_OK(cap_enter());  // Enter capability mode.

  // Confirm we can do the right operations on the capability
  TryReadWrite(cap_fd);

  // Send the file descriptor over the pipe to the sub-process
  mh.msg_controllen = CMSG_LEN(sizeof(int));
  cmptr = CMSG_FIRSTHDR(&mh);
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  *(int *)CMSG_DATA(cmptr) = cap_fd;
  buffer1[0] = 0;
  iov[0].iov_len = 1;
  sleep(3);
  int rc = sendmsg(sock_fds[1], &mh, 0);
  EXPECT_OK(rc);

  sleep(1);  // Ensure subprocess runs
  int zero = 0;
  write(sock_fds[1], &zero, sizeof(zero));
}

TEST(Capability, SyscallAt) {
  int rc = mkdir(TmpFile("cap_at_topdir"), 0755);
  EXPECT_OK(rc);
  if (rc < 0 && errno != EEXIST) return;

  cap_rights_t r_all;
  cap_rights_init(&r_all, CAP_READ, CAP_LOOKUP, CAP_MKNODAT, CAP_UNLINKAT, CAP_MKDIRAT, CAP_MKFIFOAT);
  cap_rights_t r_no_unlink;
  cap_rights_init(&r_no_unlink, CAP_READ, CAP_LOOKUP, CAP_MKDIRAT, CAP_MKFIFOAT);
  cap_rights_t r_no_mkdir;
  cap_rights_init(&r_no_mkdir, CAP_READ, CAP_LOOKUP, CAP_UNLINKAT, CAP_MKFIFOAT);
  cap_rights_t r_no_mkfifo;
  cap_rights_init(&r_no_mkfifo, CAP_READ, CAP_LOOKUP, CAP_UNLINKAT, CAP_MKDIRAT);
  cap_rights_t r_no_mknod;
  cap_rights_init(&r_no_mknod, CAP_READ, CAP_LOOKUP, CAP_UNLINKAT, CAP_MKDIRAT);
  cap_rights_t r_create;
  cap_rights_init(&r_create, CAP_READ, CAP_LOOKUP, CAP_CREATE);
  cap_rights_t r_bind;
  cap_rights_init(&r_bind, CAP_READ, CAP_LOOKUP, CAP_BIND);

  int dfd = open(TmpFile("cap_at_topdir"), O_RDONLY);
  EXPECT_OK(dfd);
  int cap_dfd_all = dup(dfd);
  EXPECT_OK(cap_dfd_all);
  EXPECT_OK(cap_rights_limit(cap_dfd_all, &r_all));
  int cap_dfd_no_unlink = dup(dfd);
  EXPECT_OK(cap_dfd_no_unlink);
  EXPECT_OK(cap_rights_limit(cap_dfd_no_unlink, &r_no_unlink));
  int cap_dfd_no_mkdir = dup(dfd);
  EXPECT_OK(cap_dfd_no_mkdir);
  EXPECT_OK(cap_rights_limit(cap_dfd_no_mkdir, &r_no_mkdir));
  int cap_dfd_no_mkfifo = dup(dfd);
  EXPECT_OK(cap_dfd_no_mkfifo);
  EXPECT_OK(cap_rights_limit(cap_dfd_no_mkfifo, &r_no_mkfifo));
  int cap_dfd_no_mknod = dup(dfd);
  EXPECT_OK(cap_dfd_no_mknod);
  EXPECT_OK(cap_rights_limit(cap_dfd_no_mknod, &r_no_mknod));
  int cap_dfd_create = dup(dfd);
  EXPECT_OK(cap_dfd_create);
  EXPECT_OK(cap_rights_limit(cap_dfd_create, &r_create));
  int cap_dfd_bind = dup(dfd);
  EXPECT_OK(cap_dfd_bind);
  EXPECT_OK(cap_rights_limit(cap_dfd_bind, &r_bind));

  // Need CAP_MKDIRAT to mkdirat(2).
  EXPECT_NOTCAPABLE(mkdirat(cap_dfd_no_mkdir, "cap_subdir", 0755));
  rmdir(TmpFile("cap_at_topdir/cap_subdir"));
  EXPECT_OK(mkdirat(cap_dfd_all, "cap_subdir", 0755));

  // Need CAP_UNLINKAT to unlinkat(dfd, name, AT_REMOVEDIR).
  EXPECT_NOTCAPABLE(unlinkat(cap_dfd_no_unlink, "cap_subdir", AT_REMOVEDIR));
  EXPECT_OK(unlinkat(cap_dfd_all, "cap_subdir", AT_REMOVEDIR));
  rmdir(TmpFile("cap_at_topdir/cap_subdir"));

  // Need CAP_MKFIFOAT to mkfifoat(2).
  EXPECT_NOTCAPABLE(mkfifoat(cap_dfd_no_mkfifo, "cap_fifo", 0755));
  unlink(TmpFile("cap_at_topdir/cap_fifo"));
  EXPECT_OK(mkfifoat(cap_dfd_all, "cap_fifo", 0755));
  unlink(TmpFile("cap_at_topdir/cap_fifo"));

#ifdef HAVE_MKNOD_REG
  // Need CAP_CREATE to create a regular file with mknodat(2).
  EXPECT_NOTCAPABLE(mknodat(cap_dfd_all, "cap_regular", S_IFREG|0755, 0));
  unlink(TmpFile("cap_at_topdir/cap_regular"));
  EXPECT_OK(mknodat(cap_dfd_create, "cap_regular", S_IFREG|0755, 0));
  unlink(TmpFile("cap_at_topdir/cap_regular"));
#endif

#ifdef HAVE_MKNOD_SOCKET
  // Need CAP_BIND to create a UNIX domain socket with mknodat(2).
  EXPECT_NOTCAPABLE(mknodat(cap_dfd_all, "cap_socket", S_IFSOCK|0755, 0));
  unlink(TmpFile("cap_at_topdir/cap_socket"));
  EXPECT_OK(mknodat(cap_dfd_bind, "cap_socket", S_IFSOCK|0755, 0));
  unlink(TmpFile("cap_at_topdir/cap_socket"));
#endif

  if (getuid() == 0) {
    // Need CAP_MKNODAT to mknodat(2) a device
    EXPECT_NOTCAPABLE(mknodat(cap_dfd_no_mknod, "cap_device", S_IFCHR|0755, makedev(99, 123)));
    unlink(TmpFile("cap_at_topdir/cap_device"));
    EXPECT_OK(mknodat(cap_dfd_all, "cap_device", S_IFCHR|0755, makedev(99, 123)));
    unlink(TmpFile("cap_at_topdir/cap_device"));

    // Need CAP_MKFIFOAT to mknodat(2) for a FIFO.
    EXPECT_NOTCAPABLE(mknodat(cap_dfd_no_mkfifo, "cap_fifo", S_IFIFO|0755, 0));
    unlink(TmpFile("cap_at_topdir/cap_fifo"));
    EXPECT_OK(mknodat(cap_dfd_all, "cap_fifo", S_IFIFO|0755, 0));
    unlink(TmpFile("cap_at_topdir/cap_fifo"));
  } else {
    TEST_SKIPPED("requires root (partial)");
  }

  close(cap_dfd_all);
  close(cap_dfd_no_mknod);
  close(cap_dfd_no_mkfifo);
  close(cap_dfd_no_mkdir);
  close(cap_dfd_no_unlink);
  close(cap_dfd_create);
  close(cap_dfd_bind);
  close(dfd);

  // Tidy up.
  rmdir(TmpFile("cap_at_topdir"));
}

FORK_TEST_ON(Capability, ExtendedAttributes, TmpFile("cap_extattr")) {
  int fd = open(TmpFile("cap_extattr"), O_RDONLY|O_CREAT, 0644);
  EXPECT_OK(fd);

  char buffer[1024];
  int rc = fgetxattr_(fd, "user.capsicumtest", buffer, sizeof(buffer));
  if (rc < 0 && errno == ENOTSUP) {
    // Need user_xattr mount option for non-root users on Linux
    TEST_SKIPPED("/tmp doesn't support extended attributes");
    close(fd);
    return;
  }

  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_xlist;
  cap_rights_init(&r_xlist, CAP_EXTATTR_LIST);
  cap_rights_t r_xget;
  cap_rights_init(&r_xget, CAP_EXTATTR_GET);
  cap_rights_t r_xset;
  cap_rights_init(&r_xset, CAP_EXTATTR_SET);
  cap_rights_t r_xdel;
  cap_rights_init(&r_xdel, CAP_EXTATTR_DELETE);

  int cap = dup(fd);
  EXPECT_OK(cap);
  EXPECT_OK(cap_rights_limit(cap, &r_rws));
  int cap_xlist = dup(fd);
  EXPECT_OK(cap_xlist);
  EXPECT_OK(cap_rights_limit(cap_xlist, &r_xlist));
  int cap_xget = dup(fd);
  EXPECT_OK(cap_xget);
  EXPECT_OK(cap_rights_limit(cap_xget, &r_xget));
  int cap_xset = dup(fd);
  EXPECT_OK(cap_xset);
  EXPECT_OK(cap_rights_limit(cap_xset, &r_xset));
  int cap_xdel = dup(fd);
  EXPECT_OK(cap_xdel);
  EXPECT_OK(cap_rights_limit(cap_xdel, &r_xdel));

  const char* value = "capsicum";
  int len = strlen(value) + 1;
  EXPECT_NOTCAPABLE(fsetxattr_(cap, "user.capsicumtest", value, len, 0));
  EXPECT_NOTCAPABLE(fsetxattr_(cap_xlist, "user.capsicumtest", value, len, 0));
  EXPECT_NOTCAPABLE(fsetxattr_(cap_xget, "user.capsicumtest", value, len, 0));
  EXPECT_NOTCAPABLE(fsetxattr_(cap_xdel, "user.capsicumtest", value, len, 0));
  EXPECT_OK(fsetxattr_(cap_xset, "user.capsicumtest", value, len, 0));

  EXPECT_NOTCAPABLE(flistxattr_(cap, buffer, sizeof(buffer)));
  EXPECT_NOTCAPABLE(flistxattr_(cap_xget, buffer, sizeof(buffer)));
  EXPECT_NOTCAPABLE(flistxattr_(cap_xset, buffer, sizeof(buffer)));
  EXPECT_NOTCAPABLE(flistxattr_(cap_xdel, buffer, sizeof(buffer)));
  EXPECT_OK(flistxattr_(cap_xlist, buffer, sizeof(buffer)));

  EXPECT_NOTCAPABLE(fgetxattr_(cap, "user.capsicumtest", buffer, sizeof(buffer)));
  EXPECT_NOTCAPABLE(fgetxattr_(cap_xlist, "user.capsicumtest", buffer, sizeof(buffer)));
  EXPECT_NOTCAPABLE(fgetxattr_(cap_xset, "user.capsicumtest", buffer, sizeof(buffer)));
  EXPECT_NOTCAPABLE(fgetxattr_(cap_xdel, "user.capsicumtest", buffer, sizeof(buffer)));
  EXPECT_OK(fgetxattr_(cap_xget, "user.capsicumtest", buffer, sizeof(buffer)));

  EXPECT_NOTCAPABLE(fremovexattr_(cap, "user.capsicumtest"));
  EXPECT_NOTCAPABLE(fremovexattr_(cap_xlist, "user.capsicumtest"));
  EXPECT_NOTCAPABLE(fremovexattr_(cap_xget, "user.capsicumtest"));
  EXPECT_NOTCAPABLE(fremovexattr_(cap_xset, "user.capsicumtest"));
  EXPECT_OK(fremovexattr_(cap_xdel, "user.capsicumtest"));

  close(cap_xdel);
  close(cap_xset);
  close(cap_xget);
  close(cap_xlist);
  close(cap);
  close(fd);
}

TEST(Capability, PipeUnseekable) {
  int fds[2];
  EXPECT_OK(pipe(fds));

  // Some programs detect pipes by calling seek() and getting ESPIPE.
  EXPECT_EQ(-1, lseek(fds[0], 0, SEEK_SET));
  EXPECT_EQ(ESPIPE, errno);

  cap_rights_t rights;
  cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_SEEK);
  EXPECT_OK(cap_rights_limit(fds[0], &rights));

  EXPECT_EQ(-1, lseek(fds[0], 0, SEEK_SET));
  EXPECT_EQ(ESPIPE, errno);

  // Remove CAP_SEEK and see if ENOTCAPABLE trumps ESPIPE.
  cap_rights_init(&rights, CAP_READ, CAP_WRITE);
  EXPECT_OK(cap_rights_limit(fds[0], &rights));
  EXPECT_EQ(-1, lseek(fds[0], 0, SEEK_SET));
  EXPECT_EQ(ENOTCAPABLE, errno);
  // TODO(drysdale): in practical terms it might be nice if ESPIPE trumped ENOTCAPABLE.
  // EXPECT_EQ(ESPIPE, errno);

  close(fds[0]);
  close(fds[1]);
}

TEST(Capability, NoBypassDAC) {
  REQUIRE_ROOT();
  int fd = open(TmpFile("cap_root_owned"), O_RDONLY|O_CREAT, 0644);
  EXPECT_OK(fd);
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_FCHMOD, CAP_FSTAT);
  EXPECT_OK(cap_rights_limit(fd, &rights));

  pid_t child = fork();
  if (child == 0) {
    // Child: change uid to a lesser being
    setuid(other_uid);
    // Attempt to fchmod the file, and fail.
    // Having CAP_FCHMOD doesn't bypass the need to comply with DAC policy.
    int rc = fchmod(fd, 0666);
    EXPECT_EQ(-1, rc);
    EXPECT_EQ(EPERM, errno);
    exit(HasFailure());
  }
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  EXPECT_TRUE(WIFEXITED(status)) << "0x" << std::hex << status;
  EXPECT_EQ(0, WEXITSTATUS(status));
  struct stat info;
  EXPECT_OK(fstat(fd, &info));
  EXPECT_EQ((mode_t)(S_IFREG|0644), info.st_mode);
  close(fd);
  unlink(TmpFile("cap_root_owned"));
}
