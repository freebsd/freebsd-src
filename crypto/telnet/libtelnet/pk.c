/* public key routines */
/* $FreeBSD$ */
/* functions:
	genkeys(char *public, char *secret)
	common_key(char *secret, char *public, desData *deskey)
        pk_encode(char *in, *out, DesData *deskey);
        pk_decode(char *in, *out, DesData *deskey);
      where
	char public[HEXKEYBYTES + 1];
	char secret[HEXKEYBYTES + 1];
 */

#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <openssl/des.h>
#include "mp.h"
#include "pk.h"
#if defined(SOLARIS2) || defined(LINUX)
#include <stdlib.h>
#endif
 
/*
 * Choose top 128 bits of the common key to use as our idea key.
 */
static
extractideakey(ck, ideakey)
        MINT *ck;
        IdeaData *ideakey;
{
        MINT *a;
        MINT *z;
        short r;
        int i;
        short base = (1 << 8);
        char *k;

        z = itom(0);
        a = itom(0);
        madd(ck, z, a);
        for (i = 0; i < ((KEYSIZE - 128) / 8); i++) {
                sdiv(a, base, a, &r);
        }
        k = (char *)ideakey;
        for (i = 0; i < 16; i++) {
                sdiv(a, base, a, &r);
                *k++ = r;
        }
	mfree(z);
        mfree(a);
}

/*
 * Choose middle 64 bits of the common key to use as our des key, possibly
 * overwriting the lower order bits by setting parity. 
 */
static
extractdeskey(ck, deskey)
        MINT *ck;
        DesData *deskey;
{
        MINT *a;
        MINT *z;
        short r;
        int i;
        short base = (1 << 8);
        char *k;

        z = itom(0);
        a = itom(0);
        madd(ck, z, a);
        for (i = 0; i < ((KEYSIZE - 64) / 2) / 8; i++) {
                sdiv(a, base, a, &r);
        }
        k = (char *)deskey;
        for (i = 0; i < 8; i++) {
                sdiv(a, base, a, &r);
                *k++ = r;
        }
	mfree(z);
        mfree(a);
}

/*
 * get common key from my secret key and his public key
 */
void common_key(char *xsecret, char *xpublic, IdeaData *ideakey, DesData *deskey)
{
        MINT *public;
        MINT *secret;
        MINT *common;
	MINT *modulus = xtom(HEXMODULUS);

        public = xtom(xpublic);
        secret = xtom(xsecret);
        common = itom(0);
        pow(public, secret, modulus, common);
        extractdeskey(common, deskey);
        extractideakey(common, ideakey);
#if DES_OSTHOLM
	des_fixup_key_parity(deskey);
#else
	des_set_odd_parity(deskey);
#endif
        mfree(common);
        mfree(secret);
        mfree(public);
	mfree(modulus);
}


/*
 * Generate a seed
 */
void getseed(seed, seedsize)
        char *seed;
        int seedsize;
{
#if 0
        int i,f;
        int rseed;
        struct timeval tv;
	long devrand;

        (void)gettimeofday(&tv, (struct timezone *)NULL);
        rseed = tv.tv_sec + tv.tv_usec;
/* XXX What the hell is this?! */
        for (i = 0; i < 8; i++) {
                rseed ^= (rseed << 8);
        }

	f=open("/dev/random",O_NONBLOCK|O_RDONLY);
	if (f>=0)
	{
		read(f,&devrand,sizeof(devrand));
		close(f);
	}
        srand48((long)rseed^devrand);

        for (i = 0; i < seedsize; i++) {
                seed[i] = (lrand48() & 0xff);
        }
#else
	srandomdev();
	for (i = 0; i < seedsize; i++) {
		seed[i] = random() & 0xff;
	}
#endif
}


/*
 * Generate a random public/secret key pair
 */
void genkeys(public, secret)
        char *public;
        char *secret;
{
        int i;
 
#       define BASEBITS (8*sizeof(short) - 1)
#       define BASE (1 << BASEBITS)
 
        MINT *pk = itom(0);
        MINT *sk = itom(0);
        MINT *tmp;
        MINT *base = itom(BASE);
        MINT *root = itom(PROOT);
        MINT *modulus = xtom(HEXMODULUS);
        short r;
        unsigned short seed[KEYSIZE/BASEBITS + 1];
        char *xkey;

        getseed((char *)seed, sizeof(seed));    
        for (i = 0; i < KEYSIZE/BASEBITS + 1; i++) {
                r = seed[i] % BASE;
                tmp = itom(r);
                mult(sk, base, sk);
                madd(sk, tmp, sk);
                mfree(tmp);  
        }
        tmp = itom(0);
        mdiv(sk, modulus, tmp, sk);
        mfree(tmp);
        pow(root, sk, modulus, pk); 
        xkey = mtox(sk);   
        adjust(secret, xkey);
        xkey = mtox(pk);
        adjust(public, xkey);
        mfree(sk);
        mfree(base);
        mfree(pk);
        mfree(root);
        mfree(modulus);
} 

/*
 * Adjust the input key so that it is 0-filled on the left
 */
adjust(keyout, keyin)
        char keyout[HEXKEYBYTES+1];
        char *keyin;
{
        char *p;
        char *s;

        for (p = keyin; *p; p++) 
                ;
        for (s = keyout + HEXKEYBYTES; p >= keyin; p--, s--) {
                *s = *p;
        }
        while (s >= keyout) {
                *s-- = '0';
        }
}

static char hextab[17] = "0123456789ABCDEF";

/* given a DES key, cbc encrypt and translate input to terminated hex */
void pk_encode(in, out, key)
char *in,*out;
DesData *key;
{
	char buf[256];
	DesData i;
	des_key_schedule k;
	int l,op,deslen;

	memset(&i,0,sizeof(i));
	memset(buf,0,sizeof(buf));
	deslen = ((strlen(in) + 7)/8)*8;
	des_key_sched(key, k);
	des_cbc_encrypt((des_cblock *)in,(des_cblock *)buf,deslen,
		k,&i,DES_ENCRYPT);
	for (l=0,op=0;l<deslen;l++) {
		out[op++] = hextab[(buf[l] & 0xf0) >> 4];
		out[op++] = hextab[(buf[l] & 0x0f)];
	}
	out[op] = '\0';
}

/* given a DES key, translate input from hex and decrypt */
void pk_decode(in, out, key)
char *in,*out;
DesData *key;
{
	char buf[256];
	DesData i;
	des_key_schedule k;
	int l,n1,n2,op;

	memset(&i,0,sizeof(i));
	memset(buf,0,sizeof(buf));
	for (l=0,op=0;l<strlen(in)/2;l++,op+=2) {
		if(in[op] == '0' && in[op+1] == '0') {
			buf[l] = '\0';
			break;
		}
		if (in[op] > '9')
			n1 = in[op] - 'A' + 10;
		else
			n1 = in[op] - '0';
		if (in[op+1] > '9')
			n2 = in[op+1] - 'A' + 10;
		else
			n2 = in[op+1] - '0';
		buf[l] = n1*16 +n2;
	}
	des_key_sched(key, k);
	des_cbc_encrypt((des_cblock *)buf,(des_cblock *)out,strlen(in)/2,
		k,&i,DES_DECRYPT);
	out[strlen(in)/2] = '\0';
}
