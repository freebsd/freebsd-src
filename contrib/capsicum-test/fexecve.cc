#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sstream>

#include "syscalls.h"
#include "capsicum.h"
#include "capsicum-test.h"

// Arguments to use in execve() calls.
static char* null_envp[] = {NULL};

class Execve : public ::testing::Test {
 public:
  Execve() : exec_fd_(-1) {
    // We need a program to exec(), but for fexecve() to work in capability
    // mode that program needs to be statically linked (otherwise ld.so will
    // attempt to traverse the filesystem to load (e.g.) /lib/libc.so and
    // fail).
    exec_prog_ = capsicum_test_bindir + "/mini-me";
    exec_prog_noexec_ = capsicum_test_bindir + "/mini-me.noexec";
    exec_prog_setuid_ = capsicum_test_bindir + "/mini-me.setuid";

    exec_fd_ = open(exec_prog_.c_str(), O_RDONLY);
    if (exec_fd_ < 0) {
      fprintf(stderr, "Error! Failed to open %s\n", exec_prog_.c_str());
    }
    argv_checkroot_[0] = (char*)exec_prog_.c_str();
    argv_fail_[0] = (char*)exec_prog_.c_str();
    argv_pass_[0] = (char*)exec_prog_.c_str();
  }
  ~Execve() {
    if (exec_fd_ >= 0) {
      close(exec_fd_);
      exec_fd_ = -1;
    }
  }
protected:
  char* argv_checkroot_[3] = {nullptr, (char*)"--checkroot", nullptr};
  char* argv_fail_[3] = {nullptr, (char*)"--fail", nullptr};
  char* argv_pass_[3] = {nullptr, (char*)"--pass", nullptr};
  std::string exec_prog_, exec_prog_noexec_, exec_prog_setuid_;
  int exec_fd_;
};

class Fexecve : public Execve {
 public:
  Fexecve() : Execve() {}
};

class FexecveWithScript : public Fexecve {
 public:
  FexecveWithScript() :
    Fexecve(), temp_script_filename_(TmpFile("cap_sh_script")) {}

  void SetUp() override {
    // First, build an executable shell script
    int fd = open(temp_script_filename_, O_RDWR|O_CREAT, 0755);
    EXPECT_OK(fd);
    const char* contents = "#!/bin/sh\nexit 99\n";
    EXPECT_OK(write(fd, contents, strlen(contents)));
    close(fd);
  }
  void TearDown() override {
    (void)::unlink(temp_script_filename_);
  }

  const char *temp_script_filename_;
};

FORK_TEST_F(Execve, BasicFexecve) {
  EXPECT_OK(fexecve_(exec_fd_, argv_pass_, null_envp));
  // Should not reach here, exec() takes over.
  EXPECT_TRUE(!"fexecve() should never return");
}

FORK_TEST_F(Execve, InCapMode) {
  EXPECT_OK(cap_enter());
  EXPECT_OK(fexecve_(exec_fd_, argv_pass_, null_envp));
  // Should not reach here, exec() takes over.
  EXPECT_TRUE(!"fexecve() should never return");
}

FORK_TEST_F(Execve, FailWithoutCap) {
  EXPECT_OK(cap_enter());
  int cap_fd = dup(exec_fd_);
  EXPECT_OK(cap_fd);
  cap_rights_t rights;
  cap_rights_init(&rights, 0);
  EXPECT_OK(cap_rights_limit(cap_fd, &rights));
  EXPECT_EQ(-1, fexecve_(cap_fd, argv_fail_, null_envp));
  EXPECT_EQ(ENOTCAPABLE, errno);
}

FORK_TEST_F(Execve, SucceedWithCap) {
  EXPECT_OK(cap_enter());
  int cap_fd = dup(exec_fd_);
  EXPECT_OK(cap_fd);
  cap_rights_t rights;
  // TODO(drysdale): would prefer that Linux Capsicum not need all of these
  // rights -- just CAP_FEXECVE|CAP_READ or CAP_FEXECVE would be preferable.
  cap_rights_init(&rights, CAP_FEXECVE, CAP_LOOKUP, CAP_READ);
  EXPECT_OK(cap_rights_limit(cap_fd, &rights));
  EXPECT_OK(fexecve_(cap_fd, argv_pass_, null_envp));
  // Should not reach here, exec() takes over.
  EXPECT_TRUE(!"fexecve() should have succeeded");
}

FORK_TEST_F(Fexecve, ExecutePermissionCheck) {
  int fd = open(exec_prog_noexec_.c_str(), O_RDONLY);
  EXPECT_OK(fd);
  if (fd >= 0) {
    struct stat data;
    EXPECT_OK(fstat(fd, &data));
    EXPECT_EQ((mode_t)0, data.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
    EXPECT_EQ(-1, fexecve_(fd, argv_fail_, null_envp));
    EXPECT_EQ(EACCES, errno);
    close(fd);
  }
}

FORK_TEST_F(Fexecve, SetuidIgnored) {
  if (geteuid() == 0) {
    TEST_SKIPPED("requires non-root");
    return;
  }
  int fd = open(exec_prog_setuid_.c_str(), O_RDONLY);
  EXPECT_OK(fd);
  EXPECT_OK(cap_enter());
  if (fd >= 0) {
    struct stat data;
    EXPECT_OK(fstat(fd, &data));
    EXPECT_EQ((mode_t)S_ISUID, data.st_mode & S_ISUID);
    EXPECT_OK(fexecve_(fd, argv_checkroot_, null_envp));
    // Should not reach here, exec() takes over.
    EXPECT_TRUE(!"fexecve() should have succeeded");
    close(fd);
  }
}

FORK_TEST_F(Fexecve, ExecveFailure) {
  EXPECT_OK(cap_enter());
  EXPECT_EQ(-1, execve(argv_fail_[0], argv_fail_, null_envp));
  EXPECT_EQ(ECAPMODE, errno);
}

FORK_TEST_F(FexecveWithScript, CapModeScriptFail) {
  int fd;

  // Open the script file, with CAP_FEXECVE rights.
  fd = open(temp_script_filename_, O_RDONLY);
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_FEXECVE, CAP_READ, CAP_SEEK);
  EXPECT_OK(cap_rights_limit(fd, &rights));

  EXPECT_OK(cap_enter());  // Enter capability mode

  // Attempt fexecve; should fail, because "/bin/sh" is inaccessible.
  EXPECT_EQ(-1, fexecve_(fd, argv_pass_, null_envp));
}

#ifdef HAVE_EXECVEAT
class Execveat : public Execve {
 public:
  Execveat() : Execve() {}
};

TEST_F(Execveat, NoUpwardTraversal) {
  char *abspath = realpath(exec_prog_, NULL);
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));

  int dfd = open(".", O_DIRECTORY|O_RDONLY);
  pid_t child = fork();
  if (child == 0) {
    EXPECT_OK(cap_enter());  // Enter capability mode.
    // Can't execveat() an absolute path, even relative to a dfd.
    EXPECT_SYSCALL_FAIL(ECAPMODE,
                        execveat(AT_FDCWD, abspath, argv_pass_, null_envp, 0));
    EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_CAPABILITY,
                        execveat(dfd, abspath, argv_pass_, null_envp, 0));

    // Can't execveat() a relative path ("../<dir>/./<exe>").
    char *p = cwd + strlen(cwd);
    while (*p != '/') p--;
    char buffer[1024] = "../";
    strcat(buffer, ++p);
    strcat(buffer, "/");
    strcat(buffer, exec_prog_);
    EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_CAPABILITY,
                        execveat(dfd, buffer, argv_pass_, null_envp, 0));
    exit(HasFailure() ? 99 : 123);
  }
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  EXPECT_TRUE(WIFEXITED(status)) << "0x" << std::hex << status;
  EXPECT_EQ(123, WEXITSTATUS(status));
  free(abspath);
  close(dfd);
}
#endif
