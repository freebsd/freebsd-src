/*
 * uname - print system information. Jeff Comstock - Bloomington, MN USA 1992
 * Usage: uname [-asnrvm]
 * -s prints system name
 * -n prints nodename
 * -r prints software release
 * -v prints os version
 * -m prints machine name
 * -a prinst all the above information 
 */

#ifndef lint
static char rcsid[] = "$Id: uname.c,v 1.3 1994/01/24 20:45:41 davidg Exp $";
#endif /* not lint */

#include <stdio.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/utsname.h>

#define SYSNAME 	0
#define NODENAME 	1
#define RELEASE 	2
#define VERSION 	3
#define MACHINE 	4

struct utsname u;

struct utstab {
	char *str;
	int requested;
} uttab[] = {
	{ u.sysname, 	0 },
	{ u.nodename, 	0 },
	{ u.release, 	0 },
	{ u.version, 	0 },
	{ u.machine, 	0 }
};	

main(int argc, char **argv) {
char *opts="amnrsv";
register int c,space, all=0;

	if ( ! uname(&u) ) {
		if ( argc == 1 ) {
			puts(u.sysname);
		} else {
			while ( (c = getopt(argc,argv,opts)) != -1 ) {
					switch ( c ) {
					case 'a' : all++;
						break;
					case 'm' : uttab[MACHINE].requested++;
						break;
					case 'n' : uttab[NODENAME].requested++;
						break;
					case 'r' : uttab[RELEASE].requested++;
						break;
					case 's' : uttab[SYSNAME].requested++;
						break;
					case 'v' : uttab[VERSION].requested++;
						break;
				}
			}
			space=0;
			for(c=0; c <= MACHINE; c++) {
				if ( uttab[c].requested || all ) {
					if ( space )
						putchar(' ');
					printf("%s", uttab[c].str); 
					space++;
				}
			}
			puts("");
		}
		exit (EX_OK);
	} else {
		perror("uname");
		exit (EX_OSERR);
	}
}
