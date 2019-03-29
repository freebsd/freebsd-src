// Test routines to make sure a variety of system calls are or are not
// available in capability mode.  The goal is not to see if they work, just
// whether or not they return the expected ECAPMODE.
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <dirent.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

// Test fixture that opens (and closes) a bunch of files.
class WithFiles : public ::testing::Test {
 public:
  WithFiles() :
    fd_file_(open(TmpFile("cap_capmode"), O_RDWR|O_CREAT, 0644)),
    fd_close_(open("/dev/null", O_RDWR)),
    fd_dir_(open(tmpdir.c_str(), O_RDONLY)),
    fd_socket_(socket(PF_INET, SOCK_DGRAM, 0)),
    fd_tcp_socket_(socket(PF_INET, SOCK_STREAM, 0)) {
    EXPECT_OK(fd_file_);
    EXPECT_OK(fd_close_);
    EXPECT_OK(fd_dir_);
    EXPECT_OK(fd_socket_);
    EXPECT_OK(fd_tcp_socket_);
  }
  ~WithFiles() {
    if (fd_tcp_socket_ >= 0) close(fd_tcp_socket_);
    if (fd_socket_ >= 0) close(fd_socket_);
    if (fd_dir_ >= 0) close(fd_dir_);
    if (fd_close_ >= 0) close(fd_close_);
    if (fd_file_ >= 0) close(fd_file_);
    unlink(TmpFile("cap_capmode"));
  }
 protected:
  int fd_file_;
  int fd_close_;
  int fd_dir_;
  int fd_socket_;
  int fd_tcp_socket_;
};

FORK_TEST_F(WithFiles, DisallowedFileSyscalls) {
  unsigned int mode = -1;
  EXPECT_OK(cap_getmode(&mode));
  EXPECT_EQ(0, (int)mode);
  EXPECT_OK(cap_enter());  // Enter capability mode.
  EXPECT_OK(cap_getmode(&mode));
  EXPECT_EQ(1, (int)mode);

  // System calls that are not permitted in capability mode.
  EXPECT_CAPMODE(access(TmpFile("cap_capmode_access"), F_OK));
  EXPECT_CAPMODE(acct(TmpFile("cap_capmode_acct")));
  EXPECT_CAPMODE(chdir(TmpFile("cap_capmode_chdir")));
#ifdef HAVE_CHFLAGS
  EXPECT_CAPMODE(chflags(TmpFile("cap_capmode_chflags"), UF_NODUMP));
#endif
  EXPECT_CAPMODE(chmod(TmpFile("cap_capmode_chmod"), 0644));
  EXPECT_CAPMODE(chown(TmpFile("cap_capmode_chown"), -1, -1));
  EXPECT_CAPMODE(chroot(TmpFile("cap_capmode_chroot")));
  EXPECT_CAPMODE(creat(TmpFile("cap_capmode_creat"), 0644));
  EXPECT_CAPMODE(fchdir(fd_dir_));
#ifdef HAVE_GETFSSTAT
  struct statfs statfs;
  EXPECT_CAPMODE(getfsstat(&statfs, sizeof(statfs), MNT_NOWAIT));
#endif
  EXPECT_CAPMODE(link(TmpFile("foo"), TmpFile("bar")));
  struct stat sb;
  EXPECT_CAPMODE(lstat(TmpFile("cap_capmode_lstat"), &sb));
  EXPECT_CAPMODE(mknod(TmpFile("capmode_mknod"), 0644 | S_IFIFO, 0));
  EXPECT_CAPMODE(bogus_mount_());
  EXPECT_CAPMODE(open("/dev/null", O_RDWR));
  char buf[64];
  EXPECT_CAPMODE(readlink(TmpFile("cap_capmode_readlink"), buf, sizeof(buf)));
#ifdef HAVE_REVOKE
  EXPECT_CAPMODE(revoke(TmpFile("cap_capmode_revoke")));
#endif
  EXPECT_CAPMODE(stat(TmpFile("cap_capmode_stat"), &sb));
  EXPECT_CAPMODE(symlink(TmpFile("cap_capmode_symlink_from"), TmpFile("cap_capmode_symlink_to")));
  EXPECT_CAPMODE(unlink(TmpFile("cap_capmode_unlink")));
  EXPECT_CAPMODE(umount2("/not_mounted", 0));
}

FORK_TEST_F(WithFiles, DisallowedSocketSyscalls) {
  EXPECT_OK(cap_enter());  // Enter capability mode.

  // System calls that are not permitted in capability mode.
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = 0;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  EXPECT_CAPMODE(bind_(fd_socket_, (sockaddr*)&addr, sizeof(addr)));
  addr.sin_family = AF_INET;
  addr.sin_port = 53;
  addr.sin_addr.s_addr = htonl(0x08080808);
  EXPECT_CAPMODE(connect_(fd_tcp_socket_, (sockaddr*)&addr, sizeof(addr)));
}

FORK_TEST_F(WithFiles, AllowedFileSyscalls) {
  int rc;
  EXPECT_OK(cap_enter());  // Enter capability mode.

  EXPECT_OK(close(fd_close_));
  fd_close_ = -1;
  int fd_dup = dup(fd_file_);
  EXPECT_OK(fd_dup);
  EXPECT_OK(dup2(fd_file_, fd_dup));
#ifdef HAVE_DUP3
  EXPECT_OK(dup3(fd_file_, fd_dup, 0));
#endif
  if (fd_dup >= 0) close(fd_dup);

  struct stat sb;
  EXPECT_OK(fstat(fd_file_, &sb));
  EXPECT_OK(lseek(fd_file_, 0, SEEK_SET));
  char ch;
  EXPECT_OK(read(fd_file_, &ch, sizeof(ch)));
  EXPECT_OK(write(fd_file_, &ch, sizeof(ch)));

#ifdef HAVE_CHFLAGS
  rc = fchflags(fd_file_, UF_NODUMP);
  if (rc < 0) {
    EXPECT_NE(ECAPMODE, errno);
  }
#endif

  char buf[1024];
  rc = getdents_(fd_dir_, (void*)buf, sizeof(buf));
  EXPECT_OK(rc);

  char data[] = "123";
  EXPECT_OK(pwrite(fd_file_, data, 1, 0));
  EXPECT_OK(pread(fd_file_, data, 1, 0));

  struct iovec io;
  io.iov_base = data;
  io.iov_len = 2;
#if !defined(__i386__) && !defined(__linux__)
  // TODO(drysdale): reinstate these tests for 32-bit runs when possible
  // libc bug is fixed.
  EXPECT_OK(pwritev(fd_file_, &io, 1, 0));
  EXPECT_OK(preadv(fd_file_, &io, 1, 0));
#endif
  EXPECT_OK(writev(fd_file_, &io, 1));
  EXPECT_OK(readv(fd_file_, &io, 1));

#ifdef HAVE_SYNCFS
  EXPECT_OK(syncfs(fd_file_));
#endif
#ifdef HAVE_SYNC_FILE_RANGE
  EXPECT_OK(sync_file_range(fd_file_, 0, 1, 0));
#endif
#ifdef HAVE_READAHEAD
  if (!tmpdir_on_tmpfs) {  // tmpfs doesn't support readahead(2)
    EXPECT_OK(readahead(fd_file_, 0, 1));
  }
#endif
}

FORK_TEST_F(WithFiles, AllowedSocketSyscalls) {
  EXPECT_OK(cap_enter());  // Enter capability mode.

  // recvfrom() either returns -1 with EAGAIN, or 0.
  int rc = recvfrom(fd_socket_, NULL, 0, MSG_DONTWAIT, NULL, NULL);
  if (rc < 0) {
    EXPECT_EQ(EAGAIN, errno);
  }
  char ch;
  EXPECT_OK(write(fd_file_, &ch, sizeof(ch)));

  // These calls will fail for lack of e.g. a proper name to send to,
  // but they are allowed in capability mode, so errno != ECAPMODE.
  EXPECT_FAIL_NOT_CAPMODE(accept(fd_socket_, NULL, NULL));
  EXPECT_FAIL_NOT_CAPMODE(getpeername(fd_socket_, NULL, NULL));
  EXPECT_FAIL_NOT_CAPMODE(getsockname(fd_socket_, NULL, NULL));
  EXPECT_FAIL_NOT_CAPMODE(recvmsg(fd_socket_, NULL, 0));
  EXPECT_FAIL_NOT_CAPMODE(sendmsg(fd_socket_, NULL, 0));
  EXPECT_FAIL_NOT_CAPMODE(sendto(fd_socket_, NULL, 0, 0, NULL, 0));
  off_t offset = 0;
  EXPECT_FAIL_NOT_CAPMODE(sendfile_(fd_socket_, fd_file_, &offset, 1));

  // The socket/socketpair syscalls are allowed, but they don't give
  // anything externally useful (can't call bind/connect on them).
  int fd_socket2 = socket(PF_INET, SOCK_DGRAM, 0);
  EXPECT_OK(fd_socket2);
  if (fd_socket2 >= 0) close(fd_socket2);
  int fd_pair[2] = {-1, -1};
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd_pair));
  if (fd_pair[0] >= 0) close(fd_pair[0]);
  if (fd_pair[1] >= 0) close(fd_pair[1]);
}

#ifdef HAVE_SEND_RECV_MMSG
FORK_TEST(Capmode, AllowedMmsgSendRecv) {
  int fd_socket = socket(PF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  EXPECT_OK(bind(fd_socket, (sockaddr*)&addr, sizeof(addr)));

  EXPECT_OK(cap_enter());  // Enter capability mode.

  char buffer[256] = {0};
  struct iovec iov;
  iov.iov_base = buffer;
  iov.iov_len = sizeof(buffer);
  struct mmsghdr mm;
  memset(&mm, 0, sizeof(mm));
  mm.msg_hdr.msg_iov = &iov;
  mm.msg_hdr.msg_iovlen = 1;
  struct timespec ts;
  ts.tv_sec = 1;
  ts.tv_nsec = 100;
  EXPECT_FAIL_NOT_CAPMODE(recvmmsg(fd_socket, &mm, 1, MSG_DONTWAIT, &ts));
  EXPECT_FAIL_NOT_CAPMODE(sendmmsg(fd_socket, &mm, 1, 0));
  close(fd_socket);
}
#endif

FORK_TEST(Capmode, AllowedIdentifierSyscalls) {
  // Record some identifiers
  gid_t my_gid = getgid();
  pid_t my_pid = getpid();
  pid_t my_ppid = getppid();
  uid_t my_uid = getuid();
  pid_t my_sid = getsid(my_pid);

  EXPECT_OK(cap_enter());  // Enter capability mode.

  EXPECT_EQ(my_gid, getegid_());
  EXPECT_EQ(my_uid, geteuid_());
  EXPECT_EQ(my_gid, getgid_());
  EXPECT_EQ(my_pid, getpid());
  EXPECT_EQ(my_ppid, getppid());
  EXPECT_EQ(my_uid, getuid_());
  EXPECT_EQ(my_sid, getsid(my_pid));
  gid_t grps[128];
  EXPECT_OK(getgroups_(128, grps));
  uid_t ruid;
  uid_t euid;
  uid_t suid;
  EXPECT_OK(getresuid(&ruid, &euid, &suid));
  gid_t rgid;
  gid_t egid;
  gid_t sgid;
  EXPECT_OK(getresgid(&rgid, &egid, &sgid));
#ifdef HAVE_GETLOGIN
  EXPECT_TRUE(getlogin() != NULL);
#endif

  // Set various identifiers (to their existing values).
  EXPECT_OK(setgid(my_gid));
#ifdef HAVE_SETFSGID
  EXPECT_OK(setfsgid(my_gid));
#endif
  EXPECT_OK(setuid(my_uid));
#ifdef HAVE_SETFSUID
  EXPECT_OK(setfsuid(my_uid));
#endif
  EXPECT_OK(setregid(my_gid, my_gid));
  EXPECT_OK(setresgid(my_gid, my_gid, my_gid));
  EXPECT_OK(setreuid(my_uid, my_uid));
  EXPECT_OK(setresuid(my_uid, my_uid, my_uid));
  EXPECT_OK(setsid());
}

FORK_TEST(Capmode, AllowedSchedSyscalls) {
  EXPECT_OK(cap_enter());  // Enter capability mode.
  int policy = sched_getscheduler(0);
  EXPECT_OK(policy);
  struct sched_param sp;
  EXPECT_OK(sched_getparam(0, &sp));
  if (policy >= 0 && (!SCHED_SETSCHEDULER_REQUIRES_ROOT || getuid() == 0)) {
    EXPECT_OK(sched_setscheduler(0, policy, &sp));
  }
  EXPECT_OK(sched_setparam(0, &sp));
  EXPECT_OK(sched_get_priority_max(policy));
  EXPECT_OK(sched_get_priority_min(policy));
  struct timespec ts;
  EXPECT_OK(sched_rr_get_interval(0, &ts));
  EXPECT_OK(sched_yield());
}


FORK_TEST(Capmode, AllowedTimerSyscalls) {
  EXPECT_OK(cap_enter());  // Enter capability mode.
  struct timespec ts;
  EXPECT_OK(clock_getres(CLOCK_REALTIME, &ts));
  EXPECT_OK(clock_gettime(CLOCK_REALTIME, &ts));
  struct itimerval itv;
  EXPECT_OK(getitimer(ITIMER_REAL, &itv));
  EXPECT_OK(setitimer(ITIMER_REAL, &itv, NULL));
  struct timeval tv;
  struct timezone tz;
  EXPECT_OK(gettimeofday(&tv, &tz));
  ts.tv_sec = 0;
  ts.tv_nsec = 1;
  EXPECT_OK(nanosleep(&ts, NULL));
}


FORK_TEST(Capmode, AllowedProfilSyscall) {
  EXPECT_OK(cap_enter());  // Enter capability mode.
  char sbuf[32];
  EXPECT_OK(profil((profil_arg1_t*)sbuf, sizeof(sbuf), 0, 1));
}


FORK_TEST(Capmode, AllowedResourceSyscalls) {
  EXPECT_OK(cap_enter());  // Enter capability mode.
  errno = 0;
  int rc = getpriority(PRIO_PROCESS, 0);
  EXPECT_EQ(0, errno);
  EXPECT_OK(setpriority(PRIO_PROCESS, 0, rc));
  struct rlimit rlim;
  EXPECT_OK(getrlimit_(RLIMIT_CORE, &rlim));
  EXPECT_OK(setrlimit(RLIMIT_CORE, &rlim));
  struct rusage ruse;
  EXPECT_OK(getrusage(RUSAGE_SELF, &ruse));
}

FORK_TEST(CapMode, AllowedMmapSyscalls) {
  // mmap() some memory.
  size_t mem_size = getpagesize();
  void *mem = mmap(NULL, mem_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  EXPECT_TRUE(mem != NULL);
  EXPECT_OK(cap_enter());  // Enter capability mode.

  EXPECT_OK(msync(mem, mem_size, MS_ASYNC));
  EXPECT_OK(madvise(mem, mem_size, MADV_NORMAL));
  unsigned char vec[2];
  EXPECT_OK(mincore_(mem, mem_size, vec));
  EXPECT_OK(mprotect(mem, mem_size, PROT_READ|PROT_WRITE));

  if (!MLOCK_REQUIRES_ROOT || getuid() == 0) {
    EXPECT_OK(mlock(mem, mem_size));
    EXPECT_OK(munlock(mem, mem_size));
    int rc = mlockall(MCL_CURRENT);
    if (rc != 0) {
      // mlockall may well fail with ENOMEM for non-root users, as the
      // default RLIMIT_MEMLOCK value isn't that big.
      EXPECT_NE(ECAPMODE, errno);
    }
    EXPECT_OK(munlockall());
  }
  // Unmap the memory.
  EXPECT_OK(munmap(mem, mem_size));
}

FORK_TEST(Capmode, AllowedPipeSyscalls) {
  EXPECT_OK(cap_enter());  // Enter capability mode
  int fd2[2];
  int rc = pipe(fd2);
  EXPECT_EQ(0, rc);

#ifdef HAVE_VMSPLICE
  char buf[11] = "0123456789";
  struct iovec iov;
  iov.iov_base = buf;
  iov.iov_len = sizeof(buf);
  EXPECT_FAIL_NOT_CAPMODE(vmsplice(fd2[0], &iov, 1, SPLICE_F_NONBLOCK));
#endif

  if (rc == 0) {
    close(fd2[0]);
    close(fd2[1]);
  };
#ifdef HAVE_PIPE2
  rc = pipe2(fd2, 0);
  EXPECT_EQ(0, rc);
  if (rc == 0) {
    close(fd2[0]);
    close(fd2[1]);
  };
#endif
}

TEST(Capmode, AllowedAtSyscalls) {
  int rc = mkdir(TmpFile("cap_at_syscalls"), 0755);
  EXPECT_OK(rc);
  if (rc < 0 && errno != EEXIST) return;
  int dfd = open(TmpFile("cap_at_syscalls"), O_RDONLY);
  EXPECT_OK(dfd);

  int file = openat(dfd, "testfile", O_RDONLY|O_CREAT, 0644);
  EXPECT_OK(file);
  EXPECT_OK(close(file));


  pid_t child = fork();
  if (child == 0) {
    // Child: enter cap mode and run tests
    EXPECT_OK(cap_enter());  // Enter capability mode

    struct stat fs;
    EXPECT_OK(fstatat(dfd, "testfile", &fs, 0));
    EXPECT_OK(mkdirat(dfd, "subdir", 0600));
    EXPECT_OK(fchmodat(dfd, "subdir", 0644, 0));
    EXPECT_OK(faccessat(dfd, "subdir", F_OK, 0));
    EXPECT_OK(renameat(dfd, "subdir", dfd, "subdir2"));
    EXPECT_OK(renameat(dfd, "subdir2", dfd, "subdir"));
    struct timeval tv[2];
    struct timezone tz;
    EXPECT_OK(gettimeofday(&tv[0], &tz));
    EXPECT_OK(gettimeofday(&tv[1], &tz));
    EXPECT_OK(futimesat(dfd, "testfile", tv));

    EXPECT_OK(fchownat(dfd, "testfile",  fs.st_uid, fs.st_gid, 0));
    EXPECT_OK(linkat(dfd, "testfile", dfd, "linky", 0));
    EXPECT_OK(symlinkat("testfile", dfd, "symlink"));
    char buffer[256];
    EXPECT_OK(readlinkat(dfd, "symlink", buffer, sizeof(buffer)));
    EXPECT_OK(unlinkat(dfd, "linky", 0));
    EXPECT_OK(unlinkat(dfd, "subdir", AT_REMOVEDIR));

    // Check that invalid requests get a non-Capsicum errno.
    errno = 0;
    rc = readlinkat(-1, "symlink", buffer, sizeof(buffer));
    EXPECT_GE(0, rc);
    EXPECT_NE(ECAPMODE, errno);

    exit(HasFailure());
  }

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  // Tidy up.
  close(dfd);
  rmdir(TmpFile("cap_at_syscalls/subdir"));
  unlink(TmpFile("cap_at_syscalls/symlink"));
  unlink(TmpFile("cap_at_syscalls/linky"));
  unlink(TmpFile("cap_at_syscalls/testfile"));
  rmdir(TmpFile("cap_at_syscalls"));
}

TEST(Capmode, AllowedAtSyscallsCwd) {
  int rc = mkdir(TmpFile("cap_at_syscalls_cwd"), 0755);
  EXPECT_OK(rc);
  if (rc < 0 && errno != EEXIST) return;
  int dfd = open(TmpFile("cap_at_syscalls_cwd"), O_RDONLY);
  EXPECT_OK(dfd);

  int file = openat(dfd, "testfile", O_RDONLY|O_CREAT, 0644);
  EXPECT_OK(file);
  EXPECT_OK(close(file));

  pid_t child = fork();
  if (child == 0) {
    // Child: move into temp dir, enter cap mode and run tests
    EXPECT_OK(fchdir(dfd));
    EXPECT_OK(cap_enter());  // Enter capability mode

    // Test that *at(AT_FDCWD, path,...) is policed with ECAPMODE.
    EXPECT_CAPMODE(openat(AT_FDCWD, "testfile", O_RDONLY));
    struct stat fs;
    EXPECT_CAPMODE(fstatat(AT_FDCWD, "testfile", &fs, 0));
    EXPECT_CAPMODE(mkdirat(AT_FDCWD, "subdir", 0600));
    EXPECT_CAPMODE(fchmodat(AT_FDCWD, "subdir", 0644, 0));
    EXPECT_CAPMODE(faccessat(AT_FDCWD, "subdir", F_OK, 0));
    EXPECT_CAPMODE(renameat(AT_FDCWD, "subdir", AT_FDCWD, "subdir2"));
    EXPECT_CAPMODE(renameat(AT_FDCWD, "subdir2", AT_FDCWD, "subdir"));
    struct timeval tv[2];
    struct timezone tz;
    EXPECT_OK(gettimeofday(&tv[0], &tz));
    EXPECT_OK(gettimeofday(&tv[1], &tz));
    EXPECT_CAPMODE(futimesat(AT_FDCWD, "testfile", tv));

    EXPECT_CAPMODE(fchownat(AT_FDCWD, "testfile",  fs.st_uid, fs.st_gid, 0));
    EXPECT_CAPMODE(linkat(AT_FDCWD, "testfile", AT_FDCWD, "linky", 0));
    EXPECT_CAPMODE(symlinkat("testfile", AT_FDCWD, "symlink"));
    char buffer[256];
    EXPECT_CAPMODE(readlinkat(AT_FDCWD, "symlink", buffer, sizeof(buffer)));
    EXPECT_CAPMODE(unlinkat(AT_FDCWD, "linky", 0));

    exit(HasFailure());
  }

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  // Tidy up.
  close(dfd);
  rmdir(TmpFile("cap_at_syscalls_cwd/subdir"));
  unlink(TmpFile("cap_at_syscalls_cwd/symlink"));
  unlink(TmpFile("cap_at_syscalls_cwd/linky"));
  unlink(TmpFile("cap_at_syscalls_cwd/testfile"));
  rmdir(TmpFile("cap_at_syscalls_cwd"));
}

TEST(Capmode, Abort) {
  // Check that abort(3) works even in capability mode.
  pid_t child = fork();
  if (child == 0) {
    // Child: enter capability mode and call abort(3).
    // Triggers something like kill(getpid(), SIGABRT).
    cap_enter();  // Enter capability mode.
    abort();
    exit(99);
  }
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  EXPECT_TRUE(WIFSIGNALED(status)) << " status = " << std::hex << status;
  EXPECT_EQ(SIGABRT, WTERMSIG(status)) << " status = " << std::hex << status;
}

FORK_TEST_F(WithFiles, AllowedMiscSyscalls) {
  umask(022);
  mode_t um_before = umask(022);
  EXPECT_OK(cap_enter());  // Enter capability mode.

  mode_t um = umask(022);
  EXPECT_NE(-ECAPMODE, (int)um);
  EXPECT_EQ(um_before, um);
  stack_t ss;
  EXPECT_OK(sigaltstack(NULL, &ss));

  // Finally, tests for system calls that don't fit the pattern very well.
  pid_t pid = fork();
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: almost immediately exit.
    sleep(1);
    exit(0);
  } else if (pid > 0) {
    errno = 0;
    EXPECT_CAPMODE(ptrace_(PTRACE_PEEKDATA_, pid, &pid, NULL));
    EXPECT_CAPMODE(waitpid(pid, NULL, 0));
  }

  // No error return from sync(2) to test, but check errno remains unset.
  errno = 0;
  sync();
  EXPECT_EQ(0, errno);

  // TODO(FreeBSD): ktrace

#ifdef HAVE_SYSARCH
  // sysarch() is, by definition, architecture-dependent
#if defined (__amd64__) || defined (__i386__)
  long sysarch_arg = 0;
  EXPECT_CAPMODE(sysarch(I386_SET_IOPERM, &sysarch_arg));
#else
  // TOOD(jra): write a test for other architectures, like arm
#endif
#endif
}

void *thread_fn(void *p) {
  int delay = *(int *)p;
  sleep(delay);
  EXPECT_OK(getpid_());
  EXPECT_CAPMODE(open("/dev/null", O_RDWR));
  return NULL;
}

// Check that restrictions are the same in subprocesses and threads
FORK_TEST(Capmode, NewThread) {
  // Fire off a new thread before entering capability mode
  pthread_t early_thread;
  int one = 1;  // second
  EXPECT_OK(pthread_create(&early_thread, NULL, thread_fn, &one));

  // Fire off a new process before entering capability mode.
  int early_child = fork();
  EXPECT_OK(early_child);
  if (early_child == 0) {
    // Child: wait and then confirm this process is unaffect by capability mode in the parent.
    sleep(1);
    int fd = open("/dev/null", O_RDWR);
    EXPECT_OK(fd);
    close(fd);
    exit(0);
  }

  EXPECT_OK(cap_enter());  // Enter capability mode.
  // Do an allowed syscall.
  EXPECT_OK(getpid_());
  int child = fork();
  EXPECT_OK(child);
  if (child == 0) {
    // Child: do an allowed and a disallowed syscall.
    EXPECT_OK(getpid_());
    EXPECT_CAPMODE(open("/dev/null", O_RDWR));
    exit(0);
  }
  // Don't (can't) wait for either child.

  // Wait for the early-started thread.
  EXPECT_OK(pthread_join(early_thread, NULL));

  // Fire off a new thread.
  pthread_t child_thread;
  int zero = 0; // seconds
  EXPECT_OK(pthread_create(&child_thread, NULL, thread_fn, &zero));
  EXPECT_OK(pthread_join(child_thread, NULL));

  // Fork a subprocess which fires off a new thread.
  child = fork();
  EXPECT_OK(child);
  if (child == 0) {
    pthread_t child_thread2;
    EXPECT_OK(pthread_create(&child_thread2, NULL, thread_fn, &zero));
    EXPECT_OK(pthread_join(child_thread2, NULL));
    exit(0);
  }
  // Sleep for a bit to allow the subprocess to finish.
  sleep(2);
}

static int had_signal = 0;
static void handle_signal(int) { had_signal = 1; }

FORK_TEST(Capmode, SelfKill) {
  pid_t me = getpid();
  sighandler_t original = signal(SIGUSR1, handle_signal);

  pid_t child = fork();
  if (child == 0) {
    // Child: sleep and exit
    sleep(1);
    exit(0);
  }

  EXPECT_OK(cap_enter());  // Enter capability mode.

  // Can only kill(2) to own pid.
  EXPECT_CAPMODE(kill(child, SIGUSR1));
  EXPECT_OK(kill(me, SIGUSR1));
  EXPECT_EQ(1, had_signal);

  signal(SIGUSR1, original);
}
