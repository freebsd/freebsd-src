/*
**  This program tests your system to see if you have the lovely
**  security-defeating semantics that an open with O_CREAT|O_EXCL
**  set will successfully open a file named by a symbolic link that
**  points to a non-existent file.  Sadly, Posix is mute on what
**  should happen in this situation.
**
**  Results to date:
**	AIX 3.2		OK
**	BSD family	OK
**	  BSD/OS 2.1	OK
**	  FreeBSD 2.1	OK
**	DEC OSF/1 3.0	OK
**	HP-UX 9.04	FAIL
**	HP-UX 9.05	FAIL
**	HP-UX 9.07	OK
**	HP-UX 10.01	OK
**	HP-UX 10.10	OK
**	HP-UX 10.20	OK
**	Irix 5.3	OK
**	Irix 6.2	OK
**	Irix 6.3	OK
**	Irix 6.4	OK
**	Linux		OK
**	NeXT 2.1	OK
**	Solaris 2.x	OK
**	SunOS 4.x	OK
**	Ultrix 4.3	OK
*/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char Attacker[128];
char Attackee[128];

main(argc, argv)
	int argc;
	char **argv;
{
	struct stat st;

	sprintf(Attacker, "/tmp/attacker.%d.%ld", getpid(), time(NULL));
	sprintf(Attackee, "/tmp/attackee.%d.%ld", getpid(), time(NULL));

	if (symlink(Attackee, Attacker) < 0)
	{
		printf("Could not create %s->%s symlink: %d\n",
			Attacker, Attackee, errno);
		bail(1);
	}
	(void) unlink(Attackee);
	if (stat(Attackee, &st) >= 0)
	{
		printf("%s already exists -- remove and try again.\n",
			Attackee);
		bail(1);
	}
	if (open(Attacker, O_WRONLY|O_CREAT|O_EXCL, 0644) < 0)
	{
		int saveerr = errno;

		if (stat(Attackee, &st) >= 0)
		{
			printf("Weird.  Open failed but %s was created anyhow (errno = %d)\n",
				Attackee, saveerr);
			bail(1);
		}
		printf("Good show!  Exclusive open works properly with symbolic links (errno = %d).\n",
			saveerr);
		bail(0);
	}
	if (stat(Attackee, &st) < 0)
	{
		printf("Weird.  Open succeeded but %s was not created\n",
			Attackee);
		bail(2);
	}
	printf("Bad news: you can do an exclusive open through a symbolic link\n");
	printf("\tBe sure you #define BOGUS_O_EXCL in conf.h\n");
	bail(1);
}

bail(stat)
	int stat;
{
	(void) unlink(Attacker);
	(void) unlink(Attackee);
	exit(stat);
}
