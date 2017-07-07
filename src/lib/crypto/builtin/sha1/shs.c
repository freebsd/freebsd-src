/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "shs.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <string.h>

/* The SHS f()-functions.  The f1 and f3 functions can be optimized to
   save one boolean operation each - thanks to Rich Schroeppel,
   rcs@cs.arizona.edu for discovering this */

#define f1(x,y,z)   ( z ^ ( x & ( y ^ z ) ) )           /* Rounds  0-19 */
#define f2(x,y,z)   ( x ^ y ^ z )                       /* Rounds 20-39 */
#define f3(x,y,z)   ( ( x & y ) | ( z & ( x | y ) ) )   /* Rounds 40-59 */
#define f4(x,y,z)   ( x ^ y ^ z )                       /* Rounds 60-79 */

/* The SHS Mysterious Constants */

#define K1  0x5A827999L                                 /* Rounds  0-19 */
#define K2  0x6ED9EBA1L                                 /* Rounds 20-39 */
#define K3  0x8F1BBCDCL                                 /* Rounds 40-59 */
#define K4  0xCA62C1D6L                                 /* Rounds 60-79 */

/* SHS initial values */

#define h0init  0x67452301L
#define h1init  0xEFCDAB89L
#define h2init  0x98BADCFEL
#define h3init  0x10325476L
#define h4init  0xC3D2E1F0L

/* Note that it may be necessary to add parentheses to these macros if they
   are to be called with expressions as arguments */

/* 32-bit rotate left - kludged with shifts */

#define ROTL(n,X)  ((((X) << (n)) & 0xffffffff) | ((X) >> (32 - n)))

/* The initial expanding function.  The hash function is defined over an
   80-word expanded input array W, where the first 16 are copies of the input
   data, and the remaining 64 are defined by

   W[ i ] = W[ i - 16 ] ^ W[ i - 14 ] ^ W[ i - 8 ] ^ W[ i - 3 ]

   This implementation generates these values on the fly in a circular
   buffer - thanks to Colin Plumb, colin@nyx10.cs.du.edu for this
   optimization.

   The updated SHS changes the expanding function by adding a rotate of 1
   bit.  Thanks to Jim Gillogly, jim@rand.org, and an anonymous contributor
   for this information */

#ifdef NEW_SHS
#define expand(W,i) ( W[ i & 15 ] = ROTL( 1, ( W[ i & 15 ] ^ W[ (i - 14) & 15 ] ^ \
                                               W[ (i - 8) & 15 ] ^ W[ (i - 3) & 15 ] )))
#else
#define expand(W,i) ( W[ i & 15 ] ^= W[ (i - 14) & 15 ] ^       \
                      W[ (i - 8) & 15 ] ^ W[ (i - 3) & 15 ] )
#endif /* NEW_SHS */

/* The prototype SHS sub-round.  The fundamental sub-round is:

   a' = e + ROTL( 5, a ) + f( b, c, d ) + k + data;
   b' = a;
   c' = ROTL( 30, b );
   d' = c;
   e' = d;

   but this is implemented by unrolling the loop 5 times and renaming the
   variables ( e, a, b, c, d ) = ( a', b', c', d', e' ) each iteration.
   This code is then replicated 20 times for each of the 4 functions, using
   the next 20 values from the W[] array each time */

#define subRound(a, b, c, d, e, f, k, data)             \
    ( e += ROTL( 5, a ) + f( b, c, d ) + k + data,      \
      e &= 0xffffffff, b = ROTL( 30, b ) )

/* Initialize the SHS values */

void shsInit(SHS_INFO *shsInfo)
{
    /* Set the h-vars to their initial values */
    shsInfo->digest[ 0 ] = h0init;
    shsInfo->digest[ 1 ] = h1init;
    shsInfo->digest[ 2 ] = h2init;
    shsInfo->digest[ 3 ] = h3init;
    shsInfo->digest[ 4 ] = h4init;

    /* Initialise bit count */
    shsInfo->countLo = shsInfo->countHi = 0;
}

/* Perform the SHS transformation.  Note that this code, like MD5, seems to
   break some optimizing compilers due to the complexity of the expressions
   and the size of the basic block.  It may be necessary to split it into
   sections, e.g. based on the four subrounds

   Note that this corrupts the shsInfo->data area */

static void SHSTransform (SHS_LONG *digest, const SHS_LONG *data);

static
void SHSTransform(SHS_LONG *digest, const SHS_LONG *data)
{
    SHS_LONG A, B, C, D, E;     /* Local vars */
    SHS_LONG eData[ 16 ];       /* Expanded data */

    /* Set up first buffer and local data buffer */
    A = digest[ 0 ];
    B = digest[ 1 ];
    C = digest[ 2 ];
    D = digest[ 3 ];
    E = digest[ 4 ];
    memcpy(eData, data, sizeof (eData));

#if defined(CONFIG_SMALL) && !defined(CONFIG_SMALL_NO_CRYPTO)

    {
        int i;
        SHS_LONG temp;
        for (i = 0; i < 20; i++) {
            SHS_LONG x = (i < 16) ? eData[i] : expand(eData, i);
            subRound(A, B, C, D, E, f1, K1, x);
            temp = E, E = D, D = C, C = B, B = A, A = temp;
        }
        for (i = 20; i < 40; i++) {
            subRound(A, B, C, D, E, f2, K2, expand(eData, i));
            temp = E, E = D, D = C, C = B, B = A, A = temp;
        }
        for (i = 40; i < 60; i++) {
            subRound(A, B, C, D, E, f3, K3, expand(eData, i));
            temp = E, E = D, D = C, C = B, B = A, A = temp;
        }
        for (i = 60; i < 80; i++) {
            subRound(A, B, C, D, E, f4, K4, expand(eData, i));
            temp = E, E = D, D = C, C = B, B = A, A = temp;
        }
    }

#else

    /* Heavy mangling, in 4 sub-rounds of 20 interations each. */
    subRound( A, B, C, D, E, f1, K1, eData[  0 ] );
    subRound( E, A, B, C, D, f1, K1, eData[  1 ] );
    subRound( D, E, A, B, C, f1, K1, eData[  2 ] );
    subRound( C, D, E, A, B, f1, K1, eData[  3 ] );
    subRound( B, C, D, E, A, f1, K1, eData[  4 ] );
    subRound( A, B, C, D, E, f1, K1, eData[  5 ] );
    subRound( E, A, B, C, D, f1, K1, eData[  6 ] );
    subRound( D, E, A, B, C, f1, K1, eData[  7 ] );
    subRound( C, D, E, A, B, f1, K1, eData[  8 ] );
    subRound( B, C, D, E, A, f1, K1, eData[  9 ] );
    subRound( A, B, C, D, E, f1, K1, eData[ 10 ] );
    subRound( E, A, B, C, D, f1, K1, eData[ 11 ] );
    subRound( D, E, A, B, C, f1, K1, eData[ 12 ] );
    subRound( C, D, E, A, B, f1, K1, eData[ 13 ] );
    subRound( B, C, D, E, A, f1, K1, eData[ 14 ] );
    subRound( A, B, C, D, E, f1, K1, eData[ 15 ] );
    subRound( E, A, B, C, D, f1, K1, expand( eData, 16 ) );
    subRound( D, E, A, B, C, f1, K1, expand( eData, 17 ) );
    subRound( C, D, E, A, B, f1, K1, expand( eData, 18 ) );
    subRound( B, C, D, E, A, f1, K1, expand( eData, 19 ) );

    subRound( A, B, C, D, E, f2, K2, expand( eData, 20 ) );
    subRound( E, A, B, C, D, f2, K2, expand( eData, 21 ) );
    subRound( D, E, A, B, C, f2, K2, expand( eData, 22 ) );
    subRound( C, D, E, A, B, f2, K2, expand( eData, 23 ) );
    subRound( B, C, D, E, A, f2, K2, expand( eData, 24 ) );
    subRound( A, B, C, D, E, f2, K2, expand( eData, 25 ) );
    subRound( E, A, B, C, D, f2, K2, expand( eData, 26 ) );
    subRound( D, E, A, B, C, f2, K2, expand( eData, 27 ) );
    subRound( C, D, E, A, B, f2, K2, expand( eData, 28 ) );
    subRound( B, C, D, E, A, f2, K2, expand( eData, 29 ) );
    subRound( A, B, C, D, E, f2, K2, expand( eData, 30 ) );
    subRound( E, A, B, C, D, f2, K2, expand( eData, 31 ) );
    subRound( D, E, A, B, C, f2, K2, expand( eData, 32 ) );
    subRound( C, D, E, A, B, f2, K2, expand( eData, 33 ) );
    subRound( B, C, D, E, A, f2, K2, expand( eData, 34 ) );
    subRound( A, B, C, D, E, f2, K2, expand( eData, 35 ) );
    subRound( E, A, B, C, D, f2, K2, expand( eData, 36 ) );
    subRound( D, E, A, B, C, f2, K2, expand( eData, 37 ) );
    subRound( C, D, E, A, B, f2, K2, expand( eData, 38 ) );
    subRound( B, C, D, E, A, f2, K2, expand( eData, 39 ) );

    subRound( A, B, C, D, E, f3, K3, expand( eData, 40 ) );
    subRound( E, A, B, C, D, f3, K3, expand( eData, 41 ) );
    subRound( D, E, A, B, C, f3, K3, expand( eData, 42 ) );
    subRound( C, D, E, A, B, f3, K3, expand( eData, 43 ) );
    subRound( B, C, D, E, A, f3, K3, expand( eData, 44 ) );
    subRound( A, B, C, D, E, f3, K3, expand( eData, 45 ) );
    subRound( E, A, B, C, D, f3, K3, expand( eData, 46 ) );
    subRound( D, E, A, B, C, f3, K3, expand( eData, 47 ) );
    subRound( C, D, E, A, B, f3, K3, expand( eData, 48 ) );
    subRound( B, C, D, E, A, f3, K3, expand( eData, 49 ) );
    subRound( A, B, C, D, E, f3, K3, expand( eData, 50 ) );
    subRound( E, A, B, C, D, f3, K3, expand( eData, 51 ) );
    subRound( D, E, A, B, C, f3, K3, expand( eData, 52 ) );
    subRound( C, D, E, A, B, f3, K3, expand( eData, 53 ) );
    subRound( B, C, D, E, A, f3, K3, expand( eData, 54 ) );
    subRound( A, B, C, D, E, f3, K3, expand( eData, 55 ) );
    subRound( E, A, B, C, D, f3, K3, expand( eData, 56 ) );
    subRound( D, E, A, B, C, f3, K3, expand( eData, 57 ) );
    subRound( C, D, E, A, B, f3, K3, expand( eData, 58 ) );
    subRound( B, C, D, E, A, f3, K3, expand( eData, 59 ) );

    subRound( A, B, C, D, E, f4, K4, expand( eData, 60 ) );
    subRound( E, A, B, C, D, f4, K4, expand( eData, 61 ) );
    subRound( D, E, A, B, C, f4, K4, expand( eData, 62 ) );
    subRound( C, D, E, A, B, f4, K4, expand( eData, 63 ) );
    subRound( B, C, D, E, A, f4, K4, expand( eData, 64 ) );
    subRound( A, B, C, D, E, f4, K4, expand( eData, 65 ) );
    subRound( E, A, B, C, D, f4, K4, expand( eData, 66 ) );
    subRound( D, E, A, B, C, f4, K4, expand( eData, 67 ) );
    subRound( C, D, E, A, B, f4, K4, expand( eData, 68 ) );
    subRound( B, C, D, E, A, f4, K4, expand( eData, 69 ) );
    subRound( A, B, C, D, E, f4, K4, expand( eData, 70 ) );
    subRound( E, A, B, C, D, f4, K4, expand( eData, 71 ) );
    subRound( D, E, A, B, C, f4, K4, expand( eData, 72 ) );
    subRound( C, D, E, A, B, f4, K4, expand( eData, 73 ) );
    subRound( B, C, D, E, A, f4, K4, expand( eData, 74 ) );
    subRound( A, B, C, D, E, f4, K4, expand( eData, 75 ) );
    subRound( E, A, B, C, D, f4, K4, expand( eData, 76 ) );
    subRound( D, E, A, B, C, f4, K4, expand( eData, 77 ) );
    subRound( C, D, E, A, B, f4, K4, expand( eData, 78 ) );
    subRound( B, C, D, E, A, f4, K4, expand( eData, 79 ) );

#endif

    /* Build message digest */
    digest[ 0 ] += A;
    digest[ 0 ] &= 0xffffffff;
    digest[ 1 ] += B;
    digest[ 1 ] &= 0xffffffff;
    digest[ 2 ] += C;
    digest[ 2 ] &= 0xffffffff;
    digest[ 3 ] += D;
    digest[ 3 ] &= 0xffffffff;
    digest[ 4 ] += E;
    digest[ 4 ] &= 0xffffffff;
}

/* Update SHS for a block of data */

void shsUpdate(SHS_INFO *shsInfo, const SHS_BYTE *buffer, unsigned int count)
{
    SHS_LONG tmp;
    unsigned int dataCount;
    int canfill;
    SHS_LONG *lp;

    /* Update bitcount */
    tmp = shsInfo->countLo;
    shsInfo->countLo = tmp + (((SHS_LONG) count) << 3 );
    if ((shsInfo->countLo &= 0xffffffff) < tmp)
        shsInfo->countHi++;     /* Carry from low to high */
    shsInfo->countHi += count >> 29;

    /* Get count of bytes already in data */
    dataCount = (tmp >> 3) & 0x3F;

    /* Handle any leading odd-sized chunks */
    if (dataCount) {
        lp = shsInfo->data + dataCount / 4;
        dataCount = SHS_DATASIZE - dataCount;
        canfill = (count >= dataCount);

        if (dataCount % 4) {
            /* Fill out a full 32 bit word first if needed -- this
               is not very efficient (computed shift amount),
               but it shouldn't happen often. */
            while (dataCount % 4 && count > 0) {
                *lp |= (SHS_LONG) *buffer++ << ((--dataCount % 4) * 8);
                count--;
            }
            lp++;
        }
        while (lp < shsInfo->data + 16) {
            if (count < 4) {
                *lp = 0;
                switch (count % 4) {
                case 3:
                    *lp |= (SHS_LONG) buffer[2] << 8;
                case 2:
                    *lp |= (SHS_LONG) buffer[1] << 16;
                case 1:
                    *lp |= (SHS_LONG) buffer[0] << 24;
                }
                count = 0;
                break;          /* out of while loop */
            }
            *lp++ = load_32_be(buffer);
            buffer += 4;
            count -= 4;
        }
        if (canfill) {
            SHSTransform(shsInfo->digest, shsInfo->data);
        }
    }

    /* Process data in SHS_DATASIZE chunks */
    while (count >= SHS_DATASIZE) {
        lp = shsInfo->data;
        while (lp < shsInfo->data + 16) {
            *lp++ = load_32_be(buffer);
            buffer += 4;
        }
        SHSTransform(shsInfo->digest, shsInfo->data);
        count -= SHS_DATASIZE;
    }

    if (count > 0) {
        lp = shsInfo->data;
        while (count > 4) {
            *lp++ = load_32_be(buffer);
            buffer += 4;
            count -= 4;
        }
        *lp = 0;
        switch (count % 4) {
        case 0:
            *lp |= ((SHS_LONG) buffer[3]);
        case 3:
            *lp |= ((SHS_LONG) buffer[2]) << 8;
        case 2:
            *lp |= ((SHS_LONG) buffer[1]) << 16;
        case 1:
            *lp |= ((SHS_LONG) buffer[0]) << 24;
        }
    }
}

/* Final wrapup - pad to SHS_DATASIZE-byte boundary with the bit pattern
   1 0* (64-bit count of bits processed, MSB-first) */

void shsFinal(SHS_INFO *shsInfo)
{
    int count;
    SHS_LONG *lp;

    /* Compute number of bytes mod 64 */
    count = (int) shsInfo->countLo;
    count = (count >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    lp = shsInfo->data + count / 4;
    switch (count % 4) {
    case 3:
        *lp++ |= (SHS_LONG) 0x80;
        break;
    case 2:
        *lp++ |= (SHS_LONG) 0x80 << 8;
        break;
    case 1:
        *lp++ |= (SHS_LONG) 0x80 << 16;
        break;
    case 0:
        *lp++ = (SHS_LONG) 0x80 << 24;
    }

    /* at this point, lp can point *past* shsInfo->data.  If it points
       there, just Transform and reset.  If it points to the last
       element, set that to zero.  This pads out to 64 bytes if not
       enough room for length words */

    if (lp == shsInfo->data + 15)
        *lp++ = 0;

    if (lp == shsInfo->data + 16) {
        SHSTransform(shsInfo->digest, shsInfo->data);
        lp = shsInfo->data;
    }

    /* Pad out to 56 bytes */
    while (lp < shsInfo->data + 14)
        *lp++ = 0;

    /* Append length in bits and transform */
    *lp++ = shsInfo->countHi;
    *lp++ = shsInfo->countLo;
    SHSTransform(shsInfo->digest, shsInfo->data);
}
