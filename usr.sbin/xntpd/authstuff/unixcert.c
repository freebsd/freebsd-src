/* unixcert.c,v 3.1 1993/07/06 01:05:14 jbj Exp
 * This file, and the certdata file, shamelessly stolen
 * from Phil Karn's DES implementation.
 *
 * This version uses the standard Unix setkey() and encrypt()
 * routines to do the encryption.
 */

#include <stdio.h>
#include <sys/types.h>

#include "ntp_stdlib.h"

static	void	get8		P((U_LONG *));
static	void	put8		P((U_LONG *));
static	void	do_setkey	P((U_LONG *));
static	void	do_crypt	P((U_LONG *, int));

void
main()
{
	U_LONG key[2], plain[2], cipher[2], answer[2];
	int i;
	int test;
	int fail;

	for(test=0;!feof(stdin);test++){
		get8(key);
		do_setkey(key);
		printf(" K: "); put8(key);

		get8(plain);
		printf(" P: "); put8(plain);

		get8(answer);
		printf(" C: "); put8(answer);


		for(i=0;i<2;i++)
			cipher[i] = plain[i];
		do_crypt(cipher, 0);

		for(i=0;i<2;i++)
			if(cipher[i] != answer[i])
				break;
		fail = 0;
		if(i != 2){
			printf(" Encrypt FAIL");
			fail++;
		}
		do_crypt(cipher, 1);
		for(i=0;i<2;i++)
			if(cipher[i] != plain[i])
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

static void
do_setkey(key)
	U_LONG *key;
{
	int j;
	register int i;
	register char *kb;
	register U_LONG *kp;
	char keybits[64];

	kb = keybits;
	kp = key;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < 32; i++) {
			if (*kp & (1<<(31-i)))
				*kb++ = 1;
			else
				*kb++ = 0;
		}
		kp++;
	}
	setkey(keybits);
}

static void
do_crypt(data, edflag)
	U_LONG *data;
	int edflag;
{
	int j;
	register int i;
	register char *bp;
	register U_LONG *dp;
	char block[64];

	bp = block;
	dp = data;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < 32; i++) {
			if (*dp & (1<<(31-i)))
				*bp++ = 1;
			else
				*bp++ = 0;
		}
		dp++;
	}

	encrypt(block, edflag);

	bp = block;
	dp = data;
	for (j = 0; j < 2; j++) {
		*dp = 0;
		for (i = 0; i < 32; i++) {
			if (*bp++)
				*dp |= 1<<(31-i);
		}
		dp++;
	}
}
