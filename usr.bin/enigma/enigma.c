/*
 *	"enigma.c" is in file cbw.tar from
 *	anonymous FTP host watmsg.waterloo.edu: pub/crypt/cbw.tar.Z
 *
 *	A one-rotor machine designed along the lines of Enigma
 *	but considerably trivialized.
 *
 *	A public-domain replacement for the UNIX "crypt" command.
 *
 *	Upgraded to function properly on 64-bit machines.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MINUSKVAR "CrYpTkEy"

#define ECHO 010
#define ROTORSZ 256
#define MASK 0377
char	t1[ROTORSZ];
char	t2[ROTORSZ];
char	t3[ROTORSZ];
char	deck[ROTORSZ];
char	buf[13];

void	shuffle(char *);
void	setup(char *);

void
setup(char *pw)
{
	int ic, i, k, temp, pf[2], pid;
	unsigned rnd;
	long seed;

	strncpy(buf, pw, 8);
	while (*pw)
		*pw++ = '\0';
	buf[8] = buf[0];
	buf[9] = buf[1];
	pipe(pf);
	if ((pid=fork())==0) {
		close(0);
		close(1);
		dup(pf[0]);
		dup(pf[1]);
		execlp("makekey", "-", (char *)0);
		execl("/usr/libexec/makekey", "-", (char *)0);	/* BSDI */
		execl("/usr/lib/makekey", "-", (char *)0);
		execl("/usr/bin/makekey", "-", (char *)0);	/* IBM */
		execl("/lib/makekey", "-", (char *)0);
		perror("makekey");
		fprintf(stderr, "enigma: cannot execute 'makekey', aborting\n");
		exit(1);
	}
	write(pf[1], buf, 10);
	close(pf[1]);
	i=wait((int *)NULL);
	if (i<0) perror("enigma: wait");
	if (i!=pid) {
		fprintf(stderr, "enigma: expected pid %d, got pid %d\n", pid, i);
		exit(1);
	}
	if ((i=read(pf[0], buf, 13)) != 13) {
		fprintf(stderr, "enigma: cannot generate key, read %d\n",i);
		exit(1);
	}
	seed = 123;
	for (i=0; i<13; i++)
		seed = seed*buf[i] + i;
	for(i=0;i<ROTORSZ;i++) {
		t1[i] = i;
		deck[i] = i;
	}
	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		if( sizeof(long) > 4 )  {
			/* Force seed to stay in 32-bit signed math */
			if( seed & 0x80000000 )
				seed = seed | (-1L & ~0xFFFFFFFFL);
			else
				seed &= 0x7FFFFFFF;
		}
		rnd = seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (rnd&MASK)%(k+1);
		rnd >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0) continue;
		ic = (rnd&MASK) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0;i<ROTORSZ;i++)
		t2[t1[i]&MASK] = i;
}

int
main(int argc, char *argv[])
{
	int i, n1, n2, nr1, nr2;
	int secureflg = 0, kflag = 0;
	char *cp;

	if (argc > 1 && argv[1][0] == '-') {
		if (argv[1][1] == 's') {
			argc--;
			argv++;
			secureflg = 1;
		} else if (argv[1][1] == 'k') {
			argc--;
			argv++;
			kflag = 1;
		}
	}
	if (kflag) {
		if ((cp = getenv(MINUSKVAR)) == NULL) {
			fprintf(stderr, "%s not set\n", MINUSKVAR);
			exit(1);
		}
		setup(cp);
	} else if (argc != 2) {
		setup(getpass("Enter key:"));
	}
	else
		setup(argv[1]);
	n1 = 0;
	n2 = 0;
	nr2 = 0;

	while((i=getchar()) != -1) {
		if (secureflg) {
			nr1 = deck[n1]&MASK;
			nr2 = deck[nr1]&MASK;
		} else {
			nr1 = n1;
		}
		i = t2[(t3[(t1[(i+nr1)&MASK]+nr2)&MASK]-nr2)&MASK]-nr1;
		putchar(i);
		n1++;
		if(n1==ROTORSZ) {
			n1 = 0;
			n2++;
			if(n2==ROTORSZ) n2 = 0;
			if (secureflg) {
				shuffle(deck);
			} else {
				nr2 = n2;
			}
		}
	}

	return 0;
}

void
shuffle(char deckary[])
{
	int i, ic, k, temp;
	unsigned rnd;
	static long seed = 123;

	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		rnd = seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (rnd&MASK)%(k+1);
		temp = deckary[k];
		deckary[k] = deckary[ic];
		deckary[ic] = temp;
	}
}
