/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 * 
 * $FreeBSD$
 * 
 */

#include <sys/types.h>
#include <sys/jail.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	struct jail j;
	int i;
	struct in_addr in;

	if (argc < 5) 
		errx(1, "Usage: %s path hostname ip-number command ...\n",
		    argv[0]);
	i = chdir(argv[1]);
	if (i)
		err(1, "chdir %s", argv[1]);
	memset(&j, 0, sizeof(j));
	j.version = 0;
	j.path = argv[1];
	j.hostname = argv[2];
	i = inet_aton(argv[3], &in);
	if (!i)
		errx(1, "Couldn't make sense of ip-number\n");
	j.ip_number = ntohl(in.s_addr);
	i = jail(&j);
	if (i)
		err(1, "Imprisonment failed");
	i = execv(argv[4], argv + 4);
	if (i)
		err(1, "execv(%s)", argv[4]);
	exit (0);
}
