/* header for the des routines that we will use */
/* $FreeBSD$ */

typedef unsigned char byte, DesData[ 8], IdeaData[16];
#if 0
typedef unsigned long word, DesKeys[32];
#else
#define DesKeys des_key_schedule
#endif

#define DES_DECRYPT 0
#define DES_ENCRYPT 1

#if 0
extern void des_fixup_key_parity();	/* (DesData *key) */
extern int des_key_sched(); 	/* (DesData *key, DesKeys *m) */
extern int des_ecb_encrypt();	/* (DesData *src, *dst, DesKeys *m, int mode) */
extern int des_cbc_encrypt();   /* (char *src, *dst, int length,
				    DesKeys *m, DesData *init, int mode) */
#endif

/* public key routines */
/* functions:
	genkeys(char *public, char *secret)
	common_key(char *secret, char *public, desData *deskey)
      where
	char public[HEXKEYBYTES + 1];
	char secret[HEXKEYBYTES + 1];
 */

#define HEXMODULUS "d4a0ba0250b6fd2ec626e7efd637df76c716e22d0944b88b"
#define HEXKEYBYTES 48
#define KEYSIZE 192
#define KEYBYTES 24
#define PROOT 3

extern void genkeys(char *public, char *secret);
extern void common_key(char *secret, char *public, IdeaData *common,
  DesData *deskey);
extern void pk_encode(char *in, char *out, DesData *deskey);
extern void pk_decode(char *in, char *out, DesData *deskey);

