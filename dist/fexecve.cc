#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#include <sstream>

#include "syscalls.h"
#include "capsicum.h"
#include "capsicum-test.h"

// We need a program to exec(), but for fexecve() to work in capability
// mode that program needs to be statically linked (otherwise ld.so will
// attempt to traverse the filesystem to load (e.g.) /lib/libc.so and
// fail).
#define EXEC_PROG "./mini-me"
#define EXEC_PROG_NOEXEC  EXEC_PROG ".noexec"
#define EXEC_PROG_SETUID  EXEC_PROG ".setuid"

// Arguments to use in execve() calls.
static char* argv_pass[] = {(char*)EXEC_PROG, (char*)"--pass", NULL};
static char* argv_fail[] = {(char*)EXEC_PROG, (char*)"--fail", NULL};
static char* argv_checkroot[] = {(char*)EXEC_PROG, (char*)"--checkroot", NULL};
static char* null_envp[] = {NULL};

class Execve : public ::testing::Test {
 public:
  Execve() : exec_fd_(open(EXEC_PROG, O_RDONLY)) {
    if (exec_fd_ < 0) {
      fprintf(stderr, "Error! Failed to open %s\n", EXEC_PROG);
    }
  }
  ~Execve() { if (exec_fd_ >= 0) close(exec_fd_); }
protected:
  int exec_fd_;
};

FORK_TEST_F(Execve, BasicFexecve) {
  EXPECT_OK(fexecve_(exec_fd_, argv_pass, null_envp));
  // Should not reach here, exec() takes over.
  EXPECT_TRUE(!"fexecve() should never return");
}

FORK_TEST_F(Execve, InCapMode) {
  EXPECT_OK(cap_enter());
  EXPECT_OK(fexecve_(exec_fd_, argv_pass, null_envp));
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
  EXPECT_EQ(-1, fexecve_(cap_fd, argv_fail, null_envp));
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
  EXPECT_OK(fexecve_(cap_fd, argv_pass, null_envp));
  // Should not reach here, exec() takes over.
  EXPECT_TRUE(!"fexecve() should have succeeded");
}

FORK_TEST(Fexecve, ExecutePermissionCheck) {
  int fd = open(EXEC_PROG_NOEXEC, O_RDONLY);
  EXPECT_OK(fd);
  if (fd >= 0) {
    struct stat data;
    EXPECT_OK(fstat(fd, &data));
    EXPECT_EQ((mode_t)0, data.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
    EXPECT_EQ(-1, fexecve_(fd, argv_fail, null_envp));
    EXPECT_EQ(EACCES, errno);
    close(fd);
  }
}

FORK_TEST(Fexecve, SetuidIgnored) {
  if (geteuid() == 0) {
    TEST_SKIPPED("requires non-root");
    return;
  }
  int fd = open(EXEC_PROG_SETUID, O_RDONLY);
  EXPECT_OK(fd);
  EXPECT_OK(cap_enter());
  if (fd >= 0) {
    struct stat data;
    EXPECT_OK(fstat(fd, &data));
    EXPECT_EQ((mode_t)S_ISUID, data.st_mode & S_ISUID);
    EXPECT_OK(fexecve_(fd, argv_checkroot, null_envp));
    // Should not reach here, exec() takes over.
    EXPECT_TRUE(!"fexecve() should have succeeded");
    close(fd);
  }
}

FORK_TEST(Fexecve, ExecveFailure) {
  EXPECT_OK(cap_enter());
  EXPECT_EQ(-1, execve(argv_fail[0], argv_fail, null_envp));
  EXPECT_EQ(ECAPMODE, errno);
}

FORK_TEST_ON(Fexecve, CapModeScriptFail, TmpFile("cap_sh_script")) {
  // First, build an executable shell script
  int fd = open(TmpFile("cap_sh_script"), O_RDWR|O_CREAT, 0755);
  EXPECT_OK(fd);
  const char* contents = "#!/bin/sh\nexit 99\n";
  EXPECT_OK(write(fd, contents, strlen(contents)));
  close(fd);

  // Open the script file, with CAP_FEXECVE rights.
  fd = open(TmpFile("cap_sh_script"), O_RDONLY);
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_FEXECVE, CAP_READ, CAP_SEEK);
  EXPECT_OK(cap_rights_limit(fd, &rights));

  EXPECT_OK(cap_enter());  // Enter capability mode

  // Attempt fexecve; should fail, because "/bin/sh" is inaccessible.
  EXPECT_EQ(-1, fexecve_(fd, argv_pass, null_envp));
}

#ifdef HAVE_EXECVEAT
TEST(Execveat, NoUpwardTraversal) {
  char *abspath = realpath(EXEC_PROG, NULL);
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));

  int dfd = open(".", O_DIRECTORY|O_RDONLY);
  pid_t child = fork();
  if (child == 0) {
    EXPECT_OK(cap_enter());  // Enter capability mode.
    // Can't execveat() an absolute path, even relative to a dfd.
    EXPECT_SYSCALL_FAIL(ECAPMODE,
                        execveat(AT_FDCWD, abspath, argv_pass, null_envp, 0));
    EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_CAPABILITY,
                        execveat(dfd, abspath, argv_pass, null_envp, 0));

    // Can't execveat() a relative path ("../<dir>/./<exe>").
    char *p = cwd + strlen(cwd);
    while (*p != '/') p--;
    char buffer[1024] = "../";
    strcat(buffer, ++p);
    strcat(buffer, "/");
    strcat(buffer, EXEC_PROG);
    EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_CAPABILITY,
                        execveat(dfd, buffer, argv_pass, null_envp, 0));
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
