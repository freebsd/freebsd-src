#include <stdio.h>
#include <err.h>
#include <sys/types.h>
#include <sys/jail.h>
#include <netinet/in.h>

int
main(int argc, char **argv)
{
	struct jail j;
	int i;
	struct in_addr in;

	if (argc < 5) 
		errx(1, "Usage: %s path hostname ip command ...\n", argv[0]);
	i = chdir(argv[1]);
	if (i)
		err(1, "chdir %s", argv[1]);
	j.path = argv[1];
	j.hostname = argv[2];
	i = inet_aton(argv[3], &in);
	if (!i)
		errx(1, "Couldn't make sense if ip number\n");
	j.ip_number = in.s_addr;
	i = jail(&j);
	if (i)
		err(1, "Imprisonment failed");
	i = execv(argv[4], argv + 4);
	if (i)
		err(1, "execv(%s)", argv[4]);
	exit (0);
}
