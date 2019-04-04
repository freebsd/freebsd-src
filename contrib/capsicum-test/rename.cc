#include <fcntl.h>
#include <sys/stat.h>

#include "./capsicum-test.h"

// There was a Capsicum-related regression in FreeBSD renameat,
// which affects certain cases independent of Capsicum or capability mode
//
// added to test the renameat syscall for the case that
//    - the "to" file already exists
//    - the "to" file is specified by an absolute path
//    - the "to" file descriptor is used
//          (this descriptor should be ignored if absolute path is provided)
//
// details at: https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=222258


const char * create_tmp_src(const char* filename) {
    const char *src_path = TmpFile(filename);
    int src_fd = open(src_path, O_CREAT|O_RDWR, 0644);
    close(src_fd);
    return src_path;
}

TEST(Rename, AbsDesignationSame) {
    const char *src_path = create_tmp_src("rename_test");
    EXPECT_OK(rename(src_path, src_path));
    unlink(src_path);
}

TEST(RenameAt, AbsDesignationSame) {
    const char *src_path = create_tmp_src("renameat_test");
    const char *dir_path = TmpFile("renameat_test_dir");

    EXPECT_OK(mkdir(dir_path, 0755));
    // random temporary directory descriptor
    int dfd = open(dir_path, O_DIRECTORY);

    // Various rename from/to the same absolute path; in each case the source
    // and dest directory FDs should be irrelevant.
    EXPECT_OK(renameat(AT_FDCWD, src_path, AT_FDCWD, src_path));
    EXPECT_OK(renameat(AT_FDCWD, src_path, dfd, src_path));
    EXPECT_OK(renameat(dfd, src_path, AT_FDCWD, src_path));
    EXPECT_OK(renameat(dfd, src_path, dfd, src_path));

    close(dfd);
    rmdir(dir_path);
    unlink(src_path);
}
