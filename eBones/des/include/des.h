/* des.h */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: des.h,v 1.2 1994/07/19 19:22:17 g89r4222 Exp $
 */

#ifndef DES_DEFS
#define DES_DEFS

typedef unsigned char des_cblock[8];
typedef struct des_ks_struct
	{
	union	{
		des_cblock _;
		/* make sure things are correct size on machines with
		 * 8 byte longs */
		unsigned long pad[2];
		} ks;
#define _	ks._
	} des_key_schedule[16];

#define DES_KEY_SZ 	(sizeof(des_cblock))
#define DES_ENCRYPT	1
#define DES_DECRYPT	0

#define DES_CBC_MODE	0
#define DES_PCBC_MODE	1

#define C_Block des_cblock
#define Key_schedule des_key_schedule
#define ENCRYPT DES_ENCRYPT
#define DECRYPT DES_DECRYPT
#define KEY_SZ DES_KEY_SZ
#define string_to_key des_string_to_key
#define read_pw_string des_read_pw_string
#define random_key des_random_key
#define pcbc_encrypt des_pcbc_encrypt
#define set_key des_set__key
#define key_sched des_key_sched
#define ecb_encrypt des_ecb_encrypt
#define cbc_encrypt des_cbc_encrypt
#define cbc_cksum des_cbc_cksum
#define quad_cksum des_quad_cksum

/* For compatibility with the MIT lib - eay 20/05/92 */
typedef struct des_ks_struct bit_64;

extern int des_check_key;	/* defaults to false */
extern int des_rw_mode;		/* defaults to DES_PCBC_MODE */

/* The next line is used to disable full ANSI prototypes, if your
 * compiler has problems with the prototypes, make sure this line always
 * evaluates to true :-) */
#if !defined(MSDOS) && !defined(__STDC__)
#ifndef KERBEROS
int des_3ecb_encrypt();
int des_cbc_encrypt();
int des_3cbc_encrypt();
int des_cfb_encrypt();
int des_ecb_encrypt();
int des_encrypt();
int des_enc_read();
int des_enc_write();
int des_ofb_encrypt();
int des_pcbc_encrypt();
int des_random_key();
int des_read_password();
int des_read_2passwords();
int des_read_pw_string();
int des_is_weak_key();
int des_set__key();
int des_key_sched();
int des_string_to_key();
int des_string_to_2keys();
#endif
char *crypt();
unsigned long des_cbc_cksum();
unsigned long des_quad_cksum();
unsigned long des_cbc_cksum();
void des_set_odd_parity();
#else /* PROTO */
int des_3ecb_encrypt(des_cblock *input,des_cblock *output,\
	des_key_schedule ks1,des_key_schedule ks2,int encrypt);
unsigned long des_cbc_cksum(des_cblock *input,des_cblock *output,\
	long length,des_key_schedule schedule,des_cblock *ivec);
int des_cbc_encrypt(des_cblock *input,des_cblock *output,long length,\
	des_key_schedule schedule,des_cblock *ivec,int encrypt);
int des_3cbc_encrypt(des_cblock *input,des_cblock *output,long length,\
	des_key_schedule sk1,des_key_schedule sk2,\
	des_cblock *ivec1,des_cblock *ivec2,int encrypt);
int des_cfb_encrypt(unsigned char *in,unsigned char *out,int numbits,\
	long length,des_key_schedule schedule,des_cblock *ivec,int encrypt);
int des_ecb_encrypt(des_cblock *input,des_cblock *output,\
	des_key_schedule ks,int encrypt);
int des_encrypt(unsigned long *input,unsigned long *output,
	des_key_schedule ks, int encrypt);
int des_enc_read(int fd,char *buf,int len,des_key_schedule sched,\
	des_cblock *iv);
int des_enc_write(int fd,char *buf,int len,des_key_schedule sched,\
	des_cblock *iv);
char *crypt(char *buf,char *salt);
int des_ofb_encrypt(unsigned char *in,unsigned char *out,\
	int numbits,long length,des_key_schedule schedule,des_cblock *ivec);
int des_pcbc_encrypt(des_cblock *input,des_cblock *output,long length,\
	des_key_schedule schedule,des_cblock *ivec,int encrypt);
unsigned long des_quad_cksum(des_cblock *input,des_cblock *output,\
	long length,int out_count,des_cblock *seed);
int des_random_key(des_cblock ret);
int des_read_password(des_cblock *key,char *prompt,int verify);
int des_read_2passwords(des_cblock *key1,des_cblock *key2, \
	char *prompt,int verify);
int des_read_pw_string(char *buf,int length,char *prompt,int verify);
void des_set_odd_parity(des_cblock *key);
int des_is_weak_key(des_cblock *key);
int des_set__key(des_cblock *key,des_key_schedule schedule);
int des_key_sched(des_cblock *key,des_key_schedule schedule);
int des_string_to_key(char *str,des_cblock *key);
int des_string_to_2keys(char *str,des_cblock *key1,des_cblock *key2);
#endif
#endif
