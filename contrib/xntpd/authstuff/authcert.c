/* authcert.c,v 3.1 1993/07/06 01:04:52 jbj Exp
 * This file, and the certdata file, shamelessly stolen
 * from Phil Karn's DES implementation.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define	DES
#include "ntp_stdlib.h"

u_char ekeys[128];
u_char dkeys[128];

static	void	get8	P((U_LONG *));
static	void	put8	P((U_LONG *));

void
main()
{
	U_LONG key[2], plain[2], cipher[2], answer[2];
	int i;
	int test;
	int fail;

	for(test=0;!feof(stdin);test++){
		get8(key);
		DESauth_subkeys(key, ekeys, dkeys);
		printf(" K: "); put8(key);

		get8(plain);
		printf(" P: "); put8(plain);

		get8(answer);
		printf(" C: "); put8(answer);


		for(i=0;i<2;i++)
			cipher[i] = htonl(plain[i]);
		DESauth_des(cipher, ekeys);

		for(i=0;i<2;i++)
			if(ntohl(cipher[i]) != answer[i])
				break;
		fail = 0;
		if(i != 2){
			printf(" Encrypt FAIL");
			fail++;
		}
		DESauth_des(cipher, dkeys);
		for(i=0;i<2;i++)
			if(ntohl(cipher[i]) != plain[i])
				break;
		if(i != 2){
			printf(" Decrypt FAIL");
			fail++;
		}
		if(fail == 0)
			printf(" OK");
		printf("\n");
	}
}

static void
get8(lp)
U_LONG *lp;
{
	int t;
	U_LONG l[2];
	int i;

	l[0] = l[1] = 0L;
	for(i=0;i<8;i++){
		scanf("%2x",&t);
		if(feof(stdin))
			exit(0);
		l[i/4] <<= 8;
		l[i/4] |= (U_LONG)(t & 0xff);
	}
	*lp = l[0];
	*(lp+1) = l[1];
}

static void
put8(lp)
U_LONG *lp;
{
	int i;

	
	for(i=0;i<2;i++){
		printf("%08x",*lp++);
	}
}
