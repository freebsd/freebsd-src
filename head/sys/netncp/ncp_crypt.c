/*
 *  Routines in this file based on work of Volker Lendecke
 */
/*$*********************************************************
   $*
   $* This code has been taken from DDJ 11/93, from an 
   $* article by Pawel Szczerbina.
   $*
   $* Password encryption routines follow.
   $* Converted to C from Barry Nance's Pascal
   $* prog published in the March -93 issue of Byte.
   $*
   $* Adapted to be useable for ncpfs by 
   $* Volker Lendecke <lendecke@namu01.gwdg.de> in 
   $* October 1995. 
   $*
   $********************************************************* */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <netncp/ncp.h>
#include <netncp/ncp_subr.h>

typedef unsigned char buf32[32];

static unsigned char encrypttable[256] = {
0x7, 0x8, 0x0, 0x8, 0x6, 0x4, 0xE, 0x4, 0x5, 0xC, 0x1, 0x7, 0xB, 0xF, 0xA, 0x8,
0xF, 0x8, 0xC, 0xC, 0x9, 0x4, 0x1, 0xE, 0x4, 0x6, 0x2, 0x4, 0x0, 0xA, 0xB, 0x9,
0x2, 0xF, 0xB, 0x1, 0xD, 0x2, 0x1, 0x9, 0x5, 0xE, 0x7, 0x0, 0x0, 0x2, 0x6, 0x6,
0x0, 0x7, 0x3, 0x8, 0x2, 0x9, 0x3, 0xF, 0x7, 0xF, 0xC, 0xF, 0x6, 0x4, 0xA, 0x0,
0x2, 0x3, 0xA, 0xB, 0xD, 0x8, 0x3, 0xA, 0x1, 0x7, 0xC, 0xF, 0x1, 0x8, 0x9, 0xD,
0x9, 0x1, 0x9, 0x4, 0xE, 0x4, 0xC, 0x5, 0x5, 0xC, 0x8, 0xB, 0x2, 0x3, 0x9, 0xE,
0x7, 0x7, 0x6, 0x9, 0xE, 0xF, 0xC, 0x8, 0xD, 0x1, 0xA, 0x6, 0xE, 0xD, 0x0, 0x7,
0x7, 0xA, 0x0, 0x1, 0xF, 0x5, 0x4, 0xB, 0x7, 0xB, 0xE, 0xC, 0x9, 0x5, 0xD, 0x1,
0xB, 0xD, 0x1, 0x3, 0x5, 0xD, 0xE, 0x6, 0x3, 0x0, 0xB, 0xB, 0xF, 0x3, 0x6, 0x4,
0x9, 0xD, 0xA, 0x3, 0x1, 0x4, 0x9, 0x4, 0x8, 0x3, 0xB, 0xE, 0x5, 0x0, 0x5, 0x2,
0xC, 0xB, 0xD, 0x5, 0xD, 0x5, 0xD, 0x2, 0xD, 0x9, 0xA, 0xC, 0xA, 0x0, 0xB, 0x3,
0x5, 0x3, 0x6, 0x9, 0x5, 0x1, 0xE, 0xE, 0x0, 0xE, 0x8, 0x2, 0xD, 0x2, 0x2, 0x0,
0x4, 0xF, 0x8, 0x5, 0x9, 0x6, 0x8, 0x6, 0xB, 0xA, 0xB, 0xF, 0x0, 0x7, 0x2, 0x8,
0xC, 0x7, 0x3, 0xA, 0x1, 0x4, 0x2, 0x5, 0xF, 0x7, 0xA, 0xC, 0xE, 0x5, 0x9, 0x3,
0xE, 0x7, 0x1, 0x2, 0xE, 0x1, 0xF, 0x4, 0xA, 0x6, 0xC, 0x6, 0xF, 0x4, 0x3, 0x0,
0xC, 0x0, 0x3, 0x6, 0xF, 0x8, 0x7, 0xB, 0x2, 0xD, 0xC, 0x6, 0xA, 0xA, 0x8, 0xD
};

static buf32 encryptkeys = {
    0x48, 0x93, 0x46, 0x67, 0x98, 0x3D, 0xE6, 0x8D,
    0xB7, 0x10, 0x7A, 0x26, 0x5A, 0xB9, 0xB1, 0x35,
    0x6B, 0x0F, 0xD5, 0x70, 0xAE, 0xFB, 0xAD, 0x11,
    0xF4, 0x47, 0xDC, 0xA7, 0xEC, 0xCF, 0x50, 0xC0
};

/*
 * Create table-based 16-bytes hash from a 32-bytes array
 */
static void
nw_hash(buf32 temp, unsigned char *target) {
	short sum;
	unsigned char b3;
	int s, b2, i;

	sum = 0;

	for (b2 = 0; b2 <= 1; ++b2) {
		for (s = 0; s <= 31; ++s) {
			b3 = (temp[s] + sum) ^ (temp[(s + sum) & 31] - encryptkeys[s]);
			sum += b3;
			temp[s] = b3;
		}
	}

	for (i = 0; i <= 15; ++i) {
		target[i] = encrypttable[temp[2 * i]]
		    | (encrypttable[temp[2 * i + 1]] << 4);
	}
}


/*
 * Create a 16-bytes pattern from given buffer based on a four bytes key
 */
void
nw_keyhash(const u_char *key, const u_char *buf, int buflen, u_char *target) {
	int b2, d, s;
	buf32 temp;

	while (buflen > 0 && buf[buflen - 1] == 0)
		buflen--;

	bzero(temp, sizeof(temp));

	d = 0;
	while (buflen >= 32) {
		for (s = 0; s <= 31; ++s)
			temp[s] ^= buf[d++];
		buflen -= 32;
	}
	b2 = d;
	if (buflen > 0)	{
		for (s = 0; s <= 31; ++s) {
			if (d + buflen == b2) {
				temp[s] ^= encryptkeys[s];
				b2 = d;
			} else
				temp[s] ^= buf[b2++];
		}
	}
	for (s = 0; s <= 31; ++s)
		temp[s] ^= key[s & 3];

	nw_hash(temp, target);
}

/*
 * Create an 8-bytes pattern from an 8-bytes key and 16-bytes of data
 */
void
nw_encrypt(const u_char *fra, const u_char *buf, u_char *target) {
	buf32 k;
	int s;

	nw_keyhash(fra, buf, 16, k);
	nw_keyhash(fra + 4, buf, 16, k + 16);

	for (s = 0; s < 16; s++)
		k[s] ^= k[31 - s];

	for (s = 0; s < 8; s++)
		*target++ = k[s] ^ k[15 - s];
}

#ifdef _KERNEL
/*
 * MD4 routine taken from libmd sources
 */
typedef u_int32_t	UINT4;
typedef unsigned char *POINTER;

#define PROTO_LIST(list) list

static void Decode PROTO_LIST
  ((UINT4 *, const unsigned char *, unsigned int));

/* Constants for MD4Transform routine.
 */
#define S11 3
#define S12 7
#define S13 11
#define S14 19
#define S21 3
#define S22 5
#define S23 9
#define S24 13
#define S31 3
#define S32 9
#define S33 11
#define S34 15

/* F, G and H are basic MD4 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG and HH are transformations for rounds 1, 2 and 3 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s) { \
    (a) += F ((b), (c), (d)) + (x); \
    (a) = ROTATE_LEFT ((a), (s)); \
  }
#define GG(a, b, c, d, x, s) { \
    (a) += G ((b), (c), (d)) + (x) + (UINT4)0x5a827999; \
    (a) = ROTATE_LEFT ((a), (s)); \
  }
#define HH(a, b, c, d, x, s) { \
    (a) += H ((b), (c), (d)) + (x) + (UINT4)0x6ed9eba1; \
    (a) = ROTATE_LEFT ((a), (s)); \
  }

void
ncp_sign(const u_int32_t *state, const char *block, u_int32_t *ostate) {
	UINT4 a, b, c, d, x[16];

	Decode (x, block, 64);
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	/* Round 1 */
	FF (a, b, c, d, x[ 0], S11); /* 1 */
	FF (d, a, b, c, x[ 1], S12); /* 2 */
	FF (c, d, a, b, x[ 2], S13); /* 3 */
	FF (b, c, d, a, x[ 3], S14); /* 4 */
	FF (a, b, c, d, x[ 4], S11); /* 5 */
	FF (d, a, b, c, x[ 5], S12); /* 6 */
	FF (c, d, a, b, x[ 6], S13); /* 7 */
	FF (b, c, d, a, x[ 7], S14); /* 8 */
	FF (a, b, c, d, x[ 8], S11); /* 9 */
	FF (d, a, b, c, x[ 9], S12); /* 10 */
	FF (c, d, a, b, x[10], S13); /* 11 */
	FF (b, c, d, a, x[11], S14); /* 12 */
	FF (a, b, c, d, x[12], S11); /* 13 */
	FF (d, a, b, c, x[13], S12); /* 14 */
	FF (c, d, a, b, x[14], S13); /* 15 */
	FF (b, c, d, a, x[15], S14); /* 16 */

	/* Round 2 */
	GG (a, b, c, d, x[ 0], S21); /* 17 */
	GG (d, a, b, c, x[ 4], S22); /* 18 */
	GG (c, d, a, b, x[ 8], S23); /* 19 */
	GG (b, c, d, a, x[12], S24); /* 20 */
	GG (a, b, c, d, x[ 1], S21); /* 21 */
	GG (d, a, b, c, x[ 5], S22); /* 22 */
	GG (c, d, a, b, x[ 9], S23); /* 23 */
	GG (b, c, d, a, x[13], S24); /* 24 */
	GG (a, b, c, d, x[ 2], S21); /* 25 */
	GG (d, a, b, c, x[ 6], S22); /* 26 */
	GG (c, d, a, b, x[10], S23); /* 27 */
	GG (b, c, d, a, x[14], S24); /* 28 */
	GG (a, b, c, d, x[ 3], S21); /* 29 */
	GG (d, a, b, c, x[ 7], S22); /* 30 */
	GG (c, d, a, b, x[11], S23); /* 31 */
	GG (b, c, d, a, x[15], S24); /* 32 */

	/* Round 3 */
	HH (a, b, c, d, x[ 0], S31); /* 33 */
	HH (d, a, b, c, x[ 8], S32); /* 34 */
	HH (c, d, a, b, x[ 4], S33); /* 35 */
	HH (b, c, d, a, x[12], S34); /* 36 */
	HH (a, b, c, d, x[ 2], S31); /* 37 */
	HH (d, a, b, c, x[10], S32); /* 38 */
	HH (c, d, a, b, x[ 6], S33); /* 39 */
	HH (b, c, d, a, x[14], S34); /* 40 */
	HH (a, b, c, d, x[ 1], S31); /* 41 */
	HH (d, a, b, c, x[ 9], S32); /* 42 */
	HH (c, d, a, b, x[ 5], S33); /* 43 */
	HH (b, c, d, a, x[13], S34); /* 44 */
	HH (a, b, c, d, x[ 3], S31); /* 45 */
	HH (d, a, b, c, x[11], S32); /* 46 */
	HH (c, d, a, b, x[ 7], S33); /* 47 */
	HH (b, c, d, a, x[15], S34); /* 48 */

	ostate[0] = state[0] + a;
	ostate[1] = state[1] + b;
	ostate[2] = state[2] + c;
	ostate[3] = state[3] + d;
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
     a multiple of 4.
 */
static void Decode (output, input, len)

UINT4 *output;
const unsigned char *input;
unsigned int len;
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4)
    output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) |
      (((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
}

#endif /* _KERNEL */
