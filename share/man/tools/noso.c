#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

main(argc, argv)
	char *argv[];
{
	struct stat sb;
	register char *cp;
	int i, fd, count = 0;
	char buf[10];

	for (cp = "", i = 1; i < argc; cp = " ", i++) {
		if (lstat(argv[i], &sb) < 0)
			continue;
		if ((sb.st_mode & S_IFMT) != S_IFREG)
			continue;
		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			perror(argv[i]);
			continue;
		}
		if (read(fd, buf, 3) != 3) {
			close(fd);
			continue;
		}
		if (strncmp(buf, ".so", 3))
			count++, printf("%s%s", cp, argv[i]);
		close(fd);
	}
	if (count > 0)
		putchar('\n');
}
