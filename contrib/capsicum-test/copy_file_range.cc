#include <sys/types.h>
#include <fcntl.h>

#include <string>

#include "capsicum.h"
#include "capsicum-test.h"
#include "syscalls.h"

#define TOPDIR "cap_copy_file_range"
#define INFILE "infile"
#define OUTFILE "outfile"

/* Test that copy_file_range() checks capabilities correctly.
 * When used without offset arguments, copy_file_range() should
 * require only CAP_READ on the source and CAP_WRITE on the destination
 * file descriptors, respectively.
 * When used with offset arguments, copy_file_range() should
 * additionally require CAP_SEEK.
 */
class CopyFileRangeTest : public ::testing::Test {
 public:
  CopyFileRangeTest() {
    int rc = mkdir(TmpFile(TOPDIR), 0755);
    EXPECT_OK(rc);
    if (rc < 0) {
      EXPECT_EQ(EEXIST, errno);
    }
    wd_ = open(TmpFile(TOPDIR), O_DIRECTORY);
    EXPECT_OK(wd_);
    CreateFile(TmpFile(TOPDIR "/" INFILE));
    CreateFile(TmpFile(TOPDIR "/" OUTFILE));
  }
  ~CopyFileRangeTest() {
    close(wd_);
    unlink(TmpFile(TOPDIR "/" INFILE));
    unlink(TmpFile(TOPDIR "/" OUTFILE));
    rmdir(TmpFile(TOPDIR));
  }

 private:
  void CreateFile(const char *filename) {
    int fd = open(filename, O_CREAT|O_RDWR, 0644);
    const char *contents = "lorem ipsum dolor sit amet";
    EXPECT_OK(fd);
    for (int i = 0; i < 100; i++) {
      EXPECT_OK(write(fd, contents, strlen(contents)));
    }
    close(fd);
  }

 protected:
  int wd_;

  int openInFile(cap_rights_t *rights) {
    int fd = openat(wd_, INFILE, O_RDONLY);
    EXPECT_OK(fd);
    EXPECT_OK(cap_rights_limit(fd, rights));
    return fd;
  }
  int openOutFile(cap_rights_t *rights) {
    int fd = openat(wd_, OUTFILE, O_WRONLY);
    EXPECT_OK(fd);
    EXPECT_OK(cap_rights_limit(fd, rights));
    return fd;
  }
};

TEST_F(CopyFileRangeTest, WriteReadNeg) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_WRITE);
  cap_rights_init(&rights_out, CAP_READ);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}

TEST_F(CopyFileRangeTest, ReadReadNeg) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_READ);
  cap_rights_init(&rights_out, CAP_READ);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}

TEST_F(CopyFileRangeTest, WriteWriteNeg) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_WRITE);
  cap_rights_init(&rights_out, CAP_WRITE);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}

TEST_F(CopyFileRangeTest, ReadWrite) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_READ);
  cap_rights_init(&rights_out, CAP_WRITE);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}

TEST_F(CopyFileRangeTest, ReadSeekWrite) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_READ, CAP_SEEK);
  cap_rights_init(&rights_out, CAP_WRITE);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_OK(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}

TEST_F(CopyFileRangeTest, ReadWriteSeek) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_READ);
  cap_rights_init(&rights_out, CAP_WRITE, CAP_SEEK);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_NOTCAPABLE(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}

TEST_F(CopyFileRangeTest, ReadSeekWriteSeek) {
  cap_rights_t rights_in, rights_out;

  cap_rights_init(&rights_in, CAP_READ, CAP_SEEK);
  cap_rights_init(&rights_out, CAP_WRITE, CAP_SEEK);

  int fd_in = openInFile(&rights_in);
  int fd_out = openOutFile(&rights_out);
  off_t off_in = 0, off_out = 0;

  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, NULL, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  off_in = 20;
  off_out = 20;
  EXPECT_OK(copy_file_range(fd_in, NULL, fd_out, &off_out, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, &off_in, fd_out, NULL, 8, 0));
  EXPECT_OK(copy_file_range(fd_in, &off_in, fd_out, &off_out, 8, 0));
  close(fd_in);
  close(fd_out);
}
