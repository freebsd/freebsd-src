/*	$OpenBSD: rijndael.c,v 1.7 2001/02/04 15:32:24 stevesk Exp $	*/

/* This is an independent implementation of the encryption algorithm:   */
/*                                                                      */
/*         RIJNDAEL by Joan Daemen and Vincent Rijmen                   */
/*                                                                      */
/* which is a candidate algorithm in the Advanced Encryption Standard   */
/* programme of the US National Institute of Standards and Technology.  */
/*                                                                      */
/* Copyright in this implementation is held by Dr B R Gladman but I     */
/* hereby give permission for its free direct or derivative use subject */
/* to acknowledgment of its origin and compliance with any conditions   */
/* that the originators of the algorithm place on its exploitation.     */
/*                                                                      */
/* Dr Brian Gladman (gladman@seven77.demon.co.uk) 14th January 1999     */

/* Timing data for Rijndael (rijndael.c)

Algorithm: rijndael (rijndael.c)

128 bit key:
Key Setup:    305/1389 cycles (encrypt/decrypt)
Encrypt:       374 cycles =    68.4 mbits/sec
Decrypt:       352 cycles =    72.7 mbits/sec
Mean:          363 cycles =    70.5 mbits/sec

192 bit key:
Key Setup:    277/1595 cycles (encrypt/decrypt)
Encrypt:       439 cycles =    58.3 mbits/sec
Decrypt:       425 cycles =    60.2 mbits/sec
Mean:          432 cycles =    59.3 mbits/sec

256 bit key:
Key Setup:    374/1960 cycles (encrypt/decrypt)
Encrypt:       502 cycles =    51.0 mbits/sec
Decrypt:       498 cycles =    51.4 mbits/sec
Mean:          500 cycles =    51.2 mbits/sec

*/

#include <sys/types.h>
#include "rijndael.h"

void gen_tabs	__P((void));

/* 3. Basic macros for speeding up generic operations               */

/* Circular rotate of 32 bit values                                 */

#define rotr(x,n)   (((x) >> ((int)(n))) | ((x) << (32 - (int)(n))))
#define rotl(x,n)   (((x) << ((int)(n))) | ((x) >> (32 - (int)(n))))

/* Invert byte order in a 32 bit variable                           */

#define bswap(x)    ((rotl(x, 8) & 0x00ff00ff) | (rotr(x, 8) & 0xff00ff00))

/* Extract byte from a 32 bit quantity (little endian notation)     */

#define byte(x,n)   ((u1byte)((x) >> (8 * n)))

#if BYTE_ORDER != LITTLE_ENDIAN
#define BYTE_SWAP
#endif

#ifdef  BYTE_SWAP
#define io_swap(x)  bswap(x)
#else
#define io_swap(x)  (x)
#endif

#define LARGE_TABLES

u1byte  pow_tab[256];
u1byte  log_tab[256];
u1byte  sbx_tab[256];
u1byte  isb_tab[256];
u4byte  rco_tab[ 10];
u4byte  ft_tab[4][256];
u4byte  it_tab[4][256];

#ifdef  LARGE_TABLES
  u4byte  fl_tab[4][256];
  u4byte  il_tab[4][256];
#endif

u4byte  tab_gen = 0;

#define ff_mult(a,b)    (a && b ? pow_tab[(log_tab[a] + log_tab[b]) % 255] : 0)

#define f_rn(bo, bi, n, k)                          \
    bo[n] =  ft_tab[0][byte(bi[n],0)] ^             \
	     ft_tab[1][byte(bi[(n + 1) & 3],1)] ^   \
	     ft_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
	     ft_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rn(bo, bi, n, k)                          \
    bo[n] =  it_tab[0][byte(bi[n],0)] ^             \
	     it_tab[1][byte(bi[(n + 3) & 3],1)] ^   \
	     it_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
	     it_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#ifdef LARGE_TABLES

#define ls_box(x)                \
    ( fl_tab[0][byte(x, 0)] ^    \
      fl_tab[1][byte(x, 1)] ^    \
      fl_tab[2][byte(x, 2)] ^    \
      fl_tab[3][byte(x, 3)] )

#define f_rl(bo, bi, n, k)                          \
    bo[n] =  fl_tab[0][byte(bi[n],0)] ^             \
	     fl_tab[1][byte(bi[(n + 1) & 3],1)] ^   \
	     fl_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
	     fl_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rl(bo, bi, n, k)                          \
    bo[n] =  il_tab[0][byte(bi[n],0)] ^             \
	     il_tab[1][byte(bi[(n + 3) & 3],1)] ^   \
	     il_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
	     il_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#else

#define ls_box(x)                            \
    ((u4byte)sbx_tab[byte(x, 0)] <<  0) ^    \
    ((u4byte)sbx_tab[byte(x, 1)] <<  8) ^    \
    ((u4byte)sbx_tab[byte(x, 2)] << 16) ^    \
    ((u4byte)sbx_tab[byte(x, 3)] << 24)

#define f_rl(bo, bi, n, k)                                      \
    bo[n] = (u4byte)sbx_tab[byte(bi[n],0)] ^                    \
	rotl(((u4byte)sbx_tab[byte(bi[(n + 1) & 3],1)]),  8) ^  \
	rotl(((u4byte)sbx_tab[byte(bi[(n + 2) & 3],2)]), 16) ^  \
	rotl(((u4byte)sbx_tab[byte(bi[(n + 3) & 3],3)]), 24) ^ *(k + n)

#define i_rl(bo, bi, n, k)                                      \
    bo[n] = (u4byte)isb_tab[byte(bi[n],0)] ^                    \
	rotl(((u4byte)isb_tab[byte(bi[(n + 3) & 3],1)]),  8) ^  \
	rotl(((u4byte)isb_tab[byte(bi[(n + 2) & 3],2)]), 16) ^  \
	rotl(((u4byte)isb_tab[byte(bi[(n + 1) & 3],3)]), 24) ^ *(k + n)

#endif

void
gen_tabs(void)
{
	u4byte  i, t;
	u1byte  p, q;

	/* log and power tables for GF(2**8) finite field with  */
	/* 0x11b as modular polynomial - the simplest prmitive  */
	/* root is 0x11, used here to generate the tables       */

	for(i = 0,p = 1; i < 256; ++i) {
		pow_tab[i] = (u1byte)p; log_tab[p] = (u1byte)i;

		p = p ^ (p << 1) ^ (p & 0x80 ? 0x01b : 0);
	}

	log_tab[1] = 0; p = 1;

	for(i = 0; i < 10; ++i) {
		rco_tab[i] = p;

		p = (p << 1) ^ (p & 0x80 ? 0x1b : 0);
	}

	/* note that the affine byte transformation matrix in   */
	/* rijndael specification is in big endian format with  */
	/* bit 0 as the most significant bit. In the remainder  */
	/* of the specification the bits are numbered from the  */
	/* least significant end of a byte.                     */

	for(i = 0; i < 256; ++i) {
		p = (i ? pow_tab[255 - log_tab[i]] : 0); q = p;
		q = (q >> 7) | (q << 1); p ^= q;
		q = (q >> 7) | (q << 1); p ^= q;
		q = (q >> 7) | (q << 1); p ^= q;
		q = (q >> 7) | (q << 1); p ^= q ^ 0x63;
		sbx_tab[i] = (u1byte)p; isb_tab[p] = (u1byte)i;
	}

	for(i = 0; i < 256; ++i) {
		p = sbx_tab[i];

#ifdef  LARGE_TABLES

		t = p; fl_tab[0][i] = t;
		fl_tab[1][i] = rotl(t,  8);
		fl_tab[2][i] = rotl(t, 16);
		fl_tab[3][i] = rotl(t, 24);
#endif
		t = ((u4byte)ff_mult(2, p)) |
			((u4byte)p <<  8) |
			((u4byte)p << 16) |
			((u4byte)ff_mult(3, p) << 24);

		ft_tab[0][i] = t;
		ft_tab[1][i] = rotl(t,  8);
		ft_tab[2][i] = rotl(t, 16);
		ft_tab[3][i] = rotl(t, 24);

		p = isb_tab[i];

#ifdef  LARGE_TABLES

		t = p; il_tab[0][i] = t;
		il_tab[1][i] = rotl(t,  8);
		il_tab[2][i] = rotl(t, 16);
		il_tab[3][i] = rotl(t, 24);
#endif
		t = ((u4byte)ff_mult(14, p)) |
			((u4byte)ff_mult( 9, p) <<  8) |
			((u4byte)ff_mult(13, p) << 16) |
			((u4byte)ff_mult(11, p) << 24);

		it_tab[0][i] = t;
		it_tab[1][i] = rotl(t,  8);
		it_tab[2][i] = rotl(t, 16);
		it_tab[3][i] = rotl(t, 24);
	}

	tab_gen = 1;
}

#define star_x(x) (((x) & 0x7f7f7f7f) << 1) ^ ((((x) & 0x80808080) >> 7) * 0x1b)

#define imix_col(y,x)       \
    u   = star_x(x);        \
    v   = star_x(u);        \
    w   = star_x(v);        \
    t   = w ^ (x);          \
   (y)  = u ^ v ^ w;        \
   (y) ^= rotr(u ^ t,  8) ^ \
	  rotr(v ^ t, 16) ^ \
	  rotr(t,24)

/* initialise the key schedule from the user supplied key   */

#define loop4(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= e_key[4 * i];     e_key[4 * i + 4] = t;    \
    t ^= e_key[4 * i + 1]; e_key[4 * i + 5] = t;    \
    t ^= e_key[4 * i + 2]; e_key[4 * i + 6] = t;    \
    t ^= e_key[4 * i + 3]; e_key[4 * i + 7] = t;    \
}

#define loop6(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= e_key[6 * i];     e_key[6 * i + 6] = t;    \
    t ^= e_key[6 * i + 1]; e_key[6 * i + 7] = t;    \
    t ^= e_key[6 * i + 2]; e_key[6 * i + 8] = t;    \
    t ^= e_key[6 * i + 3]; e_key[6 * i + 9] = t;    \
    t ^= e_key[6 * i + 4]; e_key[6 * i + 10] = t;   \
    t ^= e_key[6 * i + 5]; e_key[6 * i + 11] = t;   \
}

#define loop8(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= e_key[8 * i];     e_key[8 * i + 8] = t;    \
    t ^= e_key[8 * i + 1]; e_key[8 * i + 9] = t;    \
    t ^= e_key[8 * i + 2]; e_key[8 * i + 10] = t;   \
    t ^= e_key[8 * i + 3]; e_key[8 * i + 11] = t;   \
    t  = e_key[8 * i + 4] ^ ls_box(t);              \
    e_key[8 * i + 12] = t;                          \
    t ^= e_key[8 * i + 5]; e_key[8 * i + 13] = t;   \
    t ^= e_key[8 * i + 6]; e_key[8 * i + 14] = t;   \
    t ^= e_key[8 * i + 7]; e_key[8 * i + 15] = t;   \
}

rijndael_ctx *
rijndael_set_key(rijndael_ctx *ctx, const u4byte *in_key, const u4byte key_len,
		 int encrypt)
{
	u4byte  i, t, u, v, w;
	u4byte *e_key = ctx->e_key;
	u4byte *d_key = ctx->d_key;

	ctx->decrypt = !encrypt;

	if(!tab_gen)
		gen_tabs();

	ctx->k_len = (key_len + 31) / 32;

	e_key[0] = io_swap(in_key[0]); e_key[1] = io_swap(in_key[1]);
	e_key[2] = io_swap(in_key[2]); e_key[3] = io_swap(in_key[3]);

	switch(ctx->k_len) {
	case 4: t = e_key[3];
		for(i = 0; i < 10; ++i)
			loop4(i);
		break;

	case 6: e_key[4] = io_swap(in_key[4]); t = e_key[5] = io_swap(in_key[5]);
		for(i = 0; i < 8; ++i)
			loop6(i);
		break;

	case 8: e_key[4] = io_swap(in_key[4]); e_key[5] = io_swap(in_key[5]);
		e_key[6] = io_swap(in_key[6]); t = e_key[7] = io_swap(in_key[7]);
		for(i = 0; i < 7; ++i)
			loop8(i);
		break;
	}

	if (!encrypt) {
		d_key[0] = e_key[0]; d_key[1] = e_key[1];
		d_key[2] = e_key[2]; d_key[3] = e_key[3];

		for(i = 4; i < 4 * ctx->k_len + 24; ++i) {
			imix_col(d_key[i], e_key[i]);
		}
	}

	return ctx;
}

/* encrypt a block of text  */

#define f_nround(bo, bi, k) \
    f_rn(bo, bi, 0, k);     \
    f_rn(bo, bi, 1, k);     \
    f_rn(bo, bi, 2, k);     \
    f_rn(bo, bi, 3, k);     \
    k += 4

#define f_lround(bo, bi, k) \
    f_rl(bo, bi, 0, k);     \
    f_rl(bo, bi, 1, k);     \
    f_rl(bo, bi, 2, k);     \
    f_rl(bo, bi, 3, k)

void
rijndael_encrypt(rijndael_ctx *ctx, const u4byte *in_blk, u4byte *out_blk)
{
	u4byte k_len = ctx->k_len;
	u4byte *e_key = ctx->e_key;
	u4byte  b0[4], b1[4], *kp;

	b0[0] = io_swap(in_blk[0]) ^ e_key[0];
	b0[1] = io_swap(in_blk[1]) ^ e_key[1];
	b0[2] = io_swap(in_blk[2]) ^ e_key[2];
	b0[3] = io_swap(in_blk[3]) ^ e_key[3];

	kp = e_key + 4;

	if(k_len > 6) {
		f_nround(b1, b0, kp); f_nround(b0, b1, kp);
	}

	if(k_len > 4) {
		f_nround(b1, b0, kp); f_nround(b0, b1, kp);
	}

	f_nround(b1, b0, kp); f_nround(b0, b1, kp);
	f_nround(b1, b0, kp); f_nround(b0, b1, kp);
	f_nround(b1, b0, kp); f_nround(b0, b1, kp);
	f_nround(b1, b0, kp); f_nround(b0, b1, kp);
	f_nround(b1, b0, kp); f_lround(b0, b1, kp);

	out_blk[0] = io_swap(b0[0]); out_blk[1] = io_swap(b0[1]);
	out_blk[2] = io_swap(b0[2]); out_blk[3] = io_swap(b0[3]);
}

/* decrypt a block of text  */

#define i_nround(bo, bi, k) \
    i_rn(bo, bi, 0, k);     \
    i_rn(bo, bi, 1, k);     \
    i_rn(bo, bi, 2, k);     \
    i_rn(bo, bi, 3, k);     \
    k -= 4

#define i_lround(bo, bi, k) \
    i_rl(bo, bi, 0, k);     \
    i_rl(bo, bi, 1, k);     \
    i_rl(bo, bi, 2, k);     \
    i_rl(bo, bi, 3, k)

void
rijndael_decrypt(rijndael_ctx *ctx, const u4byte *in_blk, u4byte *out_blk)
{
	u4byte  b0[4], b1[4], *kp;
	u4byte k_len = ctx->k_len;
	u4byte *e_key = ctx->e_key;
	u4byte *d_key = ctx->d_key;

	b0[0] = io_swap(in_blk[0]) ^ e_key[4 * k_len + 24];
	b0[1] = io_swap(in_blk[1]) ^ e_key[4 * k_len + 25];
	b0[2] = io_swap(in_blk[2]) ^ e_key[4 * k_len + 26];
	b0[3] = io_swap(in_blk[3]) ^ e_key[4 * k_len + 27];

	kp = d_key + 4 * (k_len + 5);

	if(k_len > 6) {
		i_nround(b1, b0, kp); i_nround(b0, b1, kp);
	}

	if(k_len > 4) {
		i_nround(b1, b0, kp); i_nround(b0, b1, kp);
	}

	i_nround(b1, b0, kp); i_nround(b0, b1, kp);
	i_nround(b1, b0, kp); i_nround(b0, b1, kp);
	i_nround(b1, b0, kp); i_nround(b0, b1, kp);
	i_nround(b1, b0, kp); i_nround(b0, b1, kp);
	i_nround(b1, b0, kp); i_lround(b0, b1, kp);

	out_blk[0] = io_swap(b0[0]); out_blk[1] = io_swap(b0[1]);
	out_blk[2] = io_swap(b0[2]); out_blk[3] = io_swap(b0[3]);
}
