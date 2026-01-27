#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
  if (argc == 2 && !strcmp(argv[1], "--pass")) {
    fprintf(stderr,"[%d] %s immediately returning 0\n", getpid(), argv[0]);
    return 0;
  }

  if (argc == 2 && !strcmp(argv[1], "--fail")) {
    fprintf(stderr,"[%d] %s immediately returning 1\n", getpid(), argv[0]);
    return 1;
  }

  if (argc == 2 && !strcmp(argv[1], "--checkroot")) {
    int rc = (geteuid() == 0);
    fprintf(stderr,"[uid:%d] %s immediately returning (geteuid() == 0) = %d\n", geteuid(), argv[0], rc);
    return rc;
  }

  if (argc == 2 && !strcmp(argv[1], "--capmode")) {
    /* Expect to already be in capability mode: check we can't open a file */
    int rc = 0;

    int fd = open("/etc/passwd", O_RDONLY);
    if (fd > 0) {
      fprintf(stderr,"[%d] %s unexpectedly able to open file\n", getpid(), argv[0]);
      rc = 1;
    }
    fprintf(stderr,"[%d] %s --capmode returning %d\n", getpid(), argv[0], rc);
    return rc;
  }

  return -1;
}
