/***********************************************************************
**
** Generate Skein rotation constant candidate sets and test them.
**
** Source code author: Doug Whiting, 2008.
**
** This algorithm and source code is released to the public domain.
**
************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include "brg_types.h"                  /* get Brian Gladman's platform-specific definitions */

#define uint    unsigned int
#define u08b    uint_8t
#define u32b    uint_32t
#define u64b    uint_64t

/* Threefish algorithm parameters */
#ifndef BITS_PER_WORD
#define BITS_PER_WORD           (64)    /* number of bits in each word of a Threefish block */
#endif

#define ROUNDS_PER_CYCLE         (8)    /* when do we inject keys and start reusing rotation constants? */
#define MAX_BITS_PER_BLK      (1024)

#define MAX_WORDS_PER_BLK       (MAX_BITS_PER_BLK/BITS_PER_WORD) 
#define MAX_ROTS_PER_CYCLE      (MAX_WORDS_PER_BLK*(ROUNDS_PER_CYCLE/2))  

/* default search parameters for different block sizes */
#define DEFAULT_GEN_CNT_4     (5500)
#define DEFAULT_ROUND_CNT_4     ( 8)
#define MIN_HW_OR_4             (50)
#define MAX_SAT_ROUNDS_4        ( 9)

#define DEFAULT_GEN_CNT_8     (1600)
#define DEFAULT_ROUND_CNT_8     ( 8)
#define MIN_HW_OR_8             (36)
#define MAX_SAT_ROUNDS_8        (10)

#define DEFAULT_GEN_CNT_16     (400)    /* the 1024-bit search is slower, so search for fewer iterations :-( */
#define DEFAULT_ROUND_CNT_16    ( 9)
#define MIN_HW_OR_16            (40)
#define MAX_SAT_ROUNDS_16       (11)

#define MAX_ROT_VER_CNT         ( 4)
#define MAX_ROT_VER_MASK        ((1 << MAX_ROT_VER_CNT ) - 1)

#define MAX_POP_CNT           (1024)    /* size of population */
#define MIN_POP_CNT           (  32)
#define DEFAULT_POP_CNT       (MAX_POP_CNT)

#define ID_RECALC_BIT_NUM       (16)
#define TWIDDLE_CNT_BIT0        (17)
#define TWIDDLE_CNT_MASK    ((1 << TWIDDLE_CNT_BIT0  ) - 1)
#define ID_RECALC_BIT       ( 1 << ID_RECALC_BIT_NUM )
#define ID_NUM_MASK         ((1 << ID_RECALC_BIT_NUM ) - 1)

#if     BITS_PER_WORD == 64
typedef u64b    Word;
#elif   BITS_PER_WORD == 32
typedef u32b    Word;
#else
#error  "Invalid BITS_PER_WORD"
#endif

/* tstFlag bits */
#define TST_FLG_SHOW        (1u << 0)
#define TST_FLG_SHOW_HIST   (1u << 1)
#define TST_FLG_VERBOSE     (1u << 2)
#define TST_FLG_STDERR      (1u << 3)
#define TST_FLG_QUICK_EXIT  (1u << 4)
#define TST_FLG_USE_ABS     (1u << 5)
#define TST_FLG_KEEP_MIN_HW (1u << 6)
#define TST_FLG_WEIGHT_REP  (1u << 7)
#define TST_FLG_CHECK_ONE   (1u << 8)
#define TST_FLG_DO_RAND     (1u << 9)

/* parameters for ShowSearchRec */
#define SHOW_ROTS_FINAL     (4)          
#define SHOW_ROTS_H         (3)
#define SHOW_ROTS_PRELIM    (2)
#define SHOW_ROTS           (1)
#define SHOW_NONE           (0)

typedef struct { Word x[MAX_WORDS_PER_BLK]; } Block;

typedef void cycle_func(Word *b, const u08b *rotates, int rounds);

typedef struct                          /* record for dealing with rotation searches */
    {
    u08b rotList[MAX_ROTS_PER_CYCLE];   /* rotation constants */
    uint CRC;                           /* CRC of rotates[] -- use as a quick "ID" */
    uint ID;                            /* (get_rotation index) + (TwiddleCnt << TWIDDLE_CNT_BIT0) */
    uint parentCRC;                     /* CRC of the parent (allows us to track genealogy a bit) */
    uint rWorst;                        /* "worst" min bit-to-bit differential */
    u08b hw_OR[MAX_ROT_VER_CNT];        /* min hamming weights (over all words), using OR */
    } rSearchRec;

typedef struct                          /* pass a bunch of parameters to RunSearch */
    {
    uint    tstFlags;
    uint    rounds;
    uint    minHW_or;
    uint    minOffs;
    uint    diffBits;
    uint    genCntMax;
    uint    sampleCnt;
    uint    maxSatRnds;
    uint    seed0;
    uint    rotVerMask;
    uint    popCnt;
    uint    runHours;                   /* 0 ==> never */
    uint    dupRotMask;                 /* zero --> allow dup rots within the same round */
    uint    regradeCnt;                 /* default = 3 */
    u64b    goodRotCntMask;             /* which rotation values are ok? */
    } testParms;

/* globals */
cycle_func *fwd_cycle       =   NULL;
cycle_func *rev_cycle       =   NULL;
cycle_func *fwd_cycle_or    =   NULL;   /* slow but steady */
cycle_func *rev_cycle_or    =   NULL;
cycle_func *fwd_cycle_or_rN =   NULL;   /* optimized for the current # rounds (for speed) */
cycle_func *rev_cycle_or_rN =   NULL;
const char *rotFileName     =   NULL;   /* read from file instead of generate random? */
uint        bitsPerBlock    =      0;   /* default is to process all block sizes */
uint        rotsPerCycle;
uint        wordsPerBlock;

/* macro "functions" */
#define RotCnt_Bad(rotCnt) (((t.goodRotCntMask >> (rotCnt)) & 1) == 0)
#define  left_rot(a,N)     (((a) << (N)) | ((a) >> (BITS_PER_WORD - (N))))
#define right_rot(a,N)     (((a) >> (N)) | ((a) << (BITS_PER_WORD - (N))))
#define DUP_64(w32)        ((w32) | (((u64b) (w32)) << 32))

/********************** use RC4 to generate test data ******************/
/* Note: this works identically on all platforms (big/little-endian)   */
static struct
    {
    uint I,J;                           /* RC4 vars */
    u08b state[256];
    } prng;

void RandBytes(void *dst,uint byteCnt)
    {
    u08b a,b;
    u08b *d = (u08b *) dst;

    for (;byteCnt;byteCnt--,d++)        /* run RC4  */
        {
        prng.I  = (prng.I+1) & 0xFF;
        a       =  prng.state[prng.I];
        prng.J  = (prng.J+a) & 0xFF;
        b       =  prng.state[prng.J];
        prng.state[prng.I] = b;
        prng.state[prng.J] = a;
        *d      =  prng.state[(a+b) & 0xFF];
        }
    }

/* get a pseudo-random 8-bit integer */
uint Rand08(void)
    {
    u08b b;
    RandBytes(&b,1);
    return (uint) b;
    }

/* get a pseudo-random 32-bit integer in a portable way */
uint Rand32(void)
    {
    uint i,n;
    u08b tmp[sizeof(uint)];

    RandBytes(tmp,sizeof(tmp));

    for (i=n=0;i<sizeof(tmp);i++)
        n = n*256 + tmp[i];
    
    return n;
    }

/* get a pseudo-random 64-bit integer in a portable way */
u64b Rand64(void)
    {
    uint i;
    u64b n;
    u08b tmp[sizeof(u64b)];

    RandBytes(tmp,sizeof(tmp));

    n=0;
    for (i=0;i<sizeof(tmp);i++)
        n = n*256 + tmp[i];
    
    return n;
    }

/* init the (RC4-based) prng */
void Rand_Init(u64b seed)
    {
    uint i,j;
    u08b tmp[4*256];

    /* init the "key" in an endian-independent fashion */
    for (i=0;i<8;i++)
        tmp[i] = (u08b) (seed >> (8*i));

    /* initialize the permutation */
    for (i=0;i<256;i++)
        prng.state[i]=(u08b) i;

    /* now run the RC4 key schedule */
    for (i=j=0;i<256;i++)
        {                   
        j = (j + prng.state[i] + tmp[i%8]) & 0xFF;
        tmp[256]      = prng.state[i];
        prng.state[i] = prng.state[j];
        prng.state[j] = tmp[256];
        }
    prng.I = prng.J = 0;  /* init I,J variables for RC4 */
    
    /* discard some initial RC4 keystream before returning */
    RandBytes(tmp,sizeof(tmp));
    }

/* implementations of Skein round functions for various block sizes */
void fwd_cycle_16(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds -=8)
        {
        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 0]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 1]); b[ 3] ^= b[ 2];
        b[ 4] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[ 2]); b[ 5] ^= b[ 4];
        b[ 6] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[ 3]); b[ 7] ^= b[ 6];
        b[ 8] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[ 4]); b[ 9] ^= b[ 8];
        b[10] += b[11]; b[11] = left_rot(b[11], rotates[ 5]); b[11] ^= b[10];
        b[12] += b[13]; b[13] = left_rot(b[13], rotates[ 6]); b[13] ^= b[12];
        b[14] += b[15]; b[15] = left_rot(b[15], rotates[ 7]); b[15] ^= b[14];
        if (rounds == 1) break;                           
                                                          
        b[ 0] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[ 8]); b[ 9] ^= b[ 0];
        b[ 2] += b[13]; b[13] = left_rot(b[13], rotates[ 9]); b[13] ^= b[ 2];
        b[ 6] += b[11]; b[11] = left_rot(b[11], rotates[10]); b[11] ^= b[ 6];
        b[ 4] += b[15]; b[15] = left_rot(b[15], rotates[11]); b[15] ^= b[ 4];
        b[10] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[12]); b[ 7] ^= b[10];
        b[12] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[13]); b[ 3] ^= b[12];
        b[14] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[14]); b[ 5] ^= b[14];
        b[ 8] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[15]); b[ 1] ^= b[ 8];
        if (rounds == 2) break;                           
                                                          
        b[ 0] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[16]); b[ 7] ^= b[ 0];
        b[ 2] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[17]); b[ 5] ^= b[ 2];
        b[ 4] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[18]); b[ 3] ^= b[ 4];
        b[ 6] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[19]); b[ 1] ^= b[ 6];
        b[12] += b[15]; b[15] = left_rot(b[15], rotates[20]); b[15] ^= b[12];
        b[14] += b[13]; b[13] = left_rot(b[13], rotates[21]); b[13] ^= b[14];
        b[ 8] += b[11]; b[11] = left_rot(b[11], rotates[22]); b[11] ^= b[ 8];
        b[10] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[23]); b[ 9] ^= b[10];
        if (rounds == 3) break;                           
                                                          
        b[ 0] += b[15]; b[15] = left_rot(b[15], rotates[24]); b[15] ^= b[ 0];
        b[ 2] += b[11]; b[11] = left_rot(b[11], rotates[25]); b[11] ^= b[ 2];
        b[ 6] += b[13]; b[13] = left_rot(b[13], rotates[26]); b[13] ^= b[ 6];
        b[ 4] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[27]); b[ 9] ^= b[ 4];
        b[14] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[28]); b[ 1] ^= b[14];
        b[ 8] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[29]); b[ 5] ^= b[ 8];
        b[10] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[30]); b[ 3] ^= b[10];
        b[12] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[31]); b[ 7] ^= b[12];
        if (rounds == 4) break;                           
                                                          
        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[32]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[33]); b[ 3] ^= b[ 2];
        b[ 4] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[34]); b[ 5] ^= b[ 4];
        b[ 6] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[35]); b[ 7] ^= b[ 6];
        b[ 8] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[36]); b[ 9] ^= b[ 8];
        b[10] += b[11]; b[11] = left_rot(b[11], rotates[37]); b[11] ^= b[10];
        b[12] += b[13]; b[13] = left_rot(b[13], rotates[38]); b[13] ^= b[12];
        b[14] += b[15]; b[15] = left_rot(b[15], rotates[39]); b[15] ^= b[14];
        if (rounds == 5) break;                           
                                                          
        b[ 0] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[40]); b[ 9] ^= b[ 0];
        b[ 2] += b[13]; b[13] = left_rot(b[13], rotates[41]); b[13] ^= b[ 2];
        b[ 6] += b[11]; b[11] = left_rot(b[11], rotates[42]); b[11] ^= b[ 6];
        b[ 4] += b[15]; b[15] = left_rot(b[15], rotates[43]); b[15] ^= b[ 4];
        b[10] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[44]); b[ 7] ^= b[10];
        b[12] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[45]); b[ 3] ^= b[12];
        b[14] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[46]); b[ 5] ^= b[14];
        b[ 8] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[47]); b[ 1] ^= b[ 8];
        if (rounds == 6) break;                           
                                                          
        b[ 0] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[48]); b[ 7] ^= b[ 0];
        b[ 2] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[49]); b[ 5] ^= b[ 2];
        b[ 4] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[50]); b[ 3] ^= b[ 4];
        b[ 6] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[51]); b[ 1] ^= b[ 6];
        b[12] += b[15]; b[15] = left_rot(b[15], rotates[52]); b[15] ^= b[12];
        b[14] += b[13]; b[13] = left_rot(b[13], rotates[53]); b[13] ^= b[14];
        b[ 8] += b[11]; b[11] = left_rot(b[11], rotates[54]); b[11] ^= b[ 8];
        b[10] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[55]); b[ 9] ^= b[10];
        if (rounds == 7) break;                           
                                                          
        b[ 0] += b[15]; b[15] = left_rot(b[15], rotates[56]); b[15] ^= b[ 0];
        b[ 2] += b[11]; b[11] = left_rot(b[11], rotates[57]); b[11] ^= b[ 2];
        b[ 6] += b[13]; b[13] = left_rot(b[13], rotates[58]); b[13] ^= b[ 6];
        b[ 4] += b[ 9]; b[ 9] = left_rot(b[ 9], rotates[59]); b[ 9] ^= b[ 4];
        b[14] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[60]); b[ 1] ^= b[14];
        b[ 8] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[61]); b[ 5] ^= b[ 8];
        b[10] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[62]); b[ 3] ^= b[10];
        b[12] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[63]); b[ 7] ^= b[12];
        }
    }

void fwd_cycle_8(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds -=8)
        {
        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 0]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 1]); b[ 3] ^= b[ 2];
        b[ 4] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[ 2]); b[ 5] ^= b[ 4];
        b[ 6] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[ 3]); b[ 7] ^= b[ 6];
        if (rounds == 1) break;

        b[ 2] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 4]); b[ 1] ^= b[ 2];
        b[ 4] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[ 5]); b[ 7] ^= b[ 4];
        b[ 6] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[ 6]); b[ 5] ^= b[ 6];
        b[ 0] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 7]); b[ 3] ^= b[ 0];
        if (rounds == 2) break;

        b[ 4] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 8]); b[ 1] ^= b[ 4];
        b[ 6] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 9]); b[ 3] ^= b[ 6];
        b[ 0] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[10]); b[ 5] ^= b[ 0];
        b[ 2] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[11]); b[ 7] ^= b[ 2];
        if (rounds == 3) break;

        b[ 6] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[12]); b[ 1] ^= b[ 6];
        b[ 0] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[13]); b[ 7] ^= b[ 0];
        b[ 2] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[14]); b[ 5] ^= b[ 2];
        b[ 4] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[15]); b[ 3] ^= b[ 4];
        if (rounds == 4) break;

        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[16]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[17]); b[ 3] ^= b[ 2];
        b[ 4] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[18]); b[ 5] ^= b[ 4];
        b[ 6] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[19]); b[ 7] ^= b[ 6];
        if (rounds == 5) break;

        b[ 2] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[20]); b[ 1] ^= b[ 2];
        b[ 4] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[21]); b[ 7] ^= b[ 4];
        b[ 6] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[22]); b[ 5] ^= b[ 6];
        b[ 0] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[23]); b[ 3] ^= b[ 0];
        if (rounds == 6) break;

        b[ 4] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[24]); b[ 1] ^= b[ 4];
        b[ 6] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[25]); b[ 3] ^= b[ 6];
        b[ 0] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[26]); b[ 5] ^= b[ 0];
        b[ 2] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[27]); b[ 7] ^= b[ 2];
        if (rounds == 7) break;

        b[ 6] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[28]); b[ 1] ^= b[ 6];
        b[ 0] += b[ 7]; b[ 7] = left_rot(b[ 7], rotates[29]); b[ 7] ^= b[ 0];
        b[ 2] += b[ 5]; b[ 5] = left_rot(b[ 5], rotates[30]); b[ 5] ^= b[ 2];
        b[ 4] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[31]); b[ 3] ^= b[ 4];
        }
    }

void fwd_cycle_4(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds -=8)
        {
        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 0]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 1]); b[ 3] ^= b[ 2];
        if (rounds == 1) break;

        b[ 0] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 2]); b[ 3] ^= b[ 0];
        b[ 2] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 3]); b[ 1] ^= b[ 2];
        if (rounds == 2) break;

        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 4]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 5]); b[ 3] ^= b[ 2];
        if (rounds == 3) break;

        b[ 0] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 6]); b[ 3] ^= b[ 0];
        b[ 2] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 7]); b[ 1] ^= b[ 2];
        if (rounds == 4) break;

        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[ 8]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[ 9]); b[ 3] ^= b[ 2];
        if (rounds == 5) break;

        b[ 0] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[10]); b[ 3] ^= b[ 0];
        b[ 2] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[11]); b[ 1] ^= b[ 2];
        if (rounds == 6) break;

        b[ 0] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[12]); b[ 1] ^= b[ 0];
        b[ 2] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[13]); b[ 3] ^= b[ 2];
        if (rounds == 7) break;

        b[ 0] += b[ 3]; b[ 3] = left_rot(b[ 3], rotates[14]); b[ 3] ^= b[ 0];
        b[ 2] += b[ 1]; b[ 1] = left_rot(b[ 1], rotates[15]); b[ 1] ^= b[ 2];
        }
    }

/* reverse versions of the cipher */
void rev_cycle_16(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds = (rounds-1) & ~7)
        {
        switch (rounds & 7)
            {
            case 0:
                    b[ 7] ^= b[12]; b[ 7] = right_rot(b[ 7], rotates[63]); b[12] -= b[ 7]; 
                    b[ 3] ^= b[10]; b[ 3] = right_rot(b[ 3], rotates[62]); b[10] -= b[ 3]; 
                    b[ 5] ^= b[ 8]; b[ 5] = right_rot(b[ 5], rotates[61]); b[ 8] -= b[ 5]; 
                    b[ 1] ^= b[14]; b[ 1] = right_rot(b[ 1], rotates[60]); b[14] -= b[ 1]; 
                    b[ 9] ^= b[ 4]; b[ 9] = right_rot(b[ 9], rotates[59]); b[ 4] -= b[ 9]; 
                    b[13] ^= b[ 6]; b[13] = right_rot(b[13], rotates[58]); b[ 6] -= b[13]; 
                    b[11] ^= b[ 2]; b[11] = right_rot(b[11], rotates[57]); b[ 2] -= b[11]; 
                    b[15] ^= b[ 0]; b[15] = right_rot(b[15], rotates[56]); b[ 0] -= b[15];
            case 7:                                                                       
                    b[ 9] ^= b[10]; b[ 9] = right_rot(b[ 9], rotates[55]); b[10] -= b[ 9];
                    b[11] ^= b[ 8]; b[11] = right_rot(b[11], rotates[54]); b[ 8] -= b[11];
                    b[13] ^= b[14]; b[13] = right_rot(b[13], rotates[53]); b[14] -= b[13];
                    b[15] ^= b[12]; b[15] = right_rot(b[15], rotates[52]); b[12] -= b[15];
                    b[ 1] ^= b[ 6]; b[ 1] = right_rot(b[ 1], rotates[51]); b[ 6] -= b[ 1];
                    b[ 3] ^= b[ 4]; b[ 3] = right_rot(b[ 3], rotates[50]); b[ 4] -= b[ 3];
                    b[ 5] ^= b[ 2]; b[ 5] = right_rot(b[ 5], rotates[49]); b[ 2] -= b[ 5];
                    b[ 7] ^= b[ 0]; b[ 7] = right_rot(b[ 7], rotates[48]); b[ 0] -= b[ 7];
            case 6:                                                                       
                    b[ 1] ^= b[ 8]; b[ 1] = right_rot(b[ 1], rotates[47]); b[ 8] -= b[ 1];
                    b[ 5] ^= b[14]; b[ 5] = right_rot(b[ 5], rotates[46]); b[14] -= b[ 5];
                    b[ 3] ^= b[12]; b[ 3] = right_rot(b[ 3], rotates[45]); b[12] -= b[ 3];
                    b[ 7] ^= b[10]; b[ 7] = right_rot(b[ 7], rotates[44]); b[10] -= b[ 7];
                    b[15] ^= b[ 4]; b[15] = right_rot(b[15], rotates[43]); b[ 4] -= b[15];
                    b[11] ^= b[ 6]; b[11] = right_rot(b[11], rotates[42]); b[ 6] -= b[11];
                    b[13] ^= b[ 2]; b[13] = right_rot(b[13], rotates[41]); b[ 2] -= b[13];
                    b[ 9] ^= b[ 0]; b[ 9] = right_rot(b[ 9], rotates[40]); b[ 0] -= b[ 9];
            case 5:                                                                       
                    b[15] ^= b[14]; b[15] = right_rot(b[15], rotates[39]); b[14] -= b[15];
                    b[13] ^= b[12]; b[13] = right_rot(b[13], rotates[38]); b[12] -= b[13];
                    b[11] ^= b[10]; b[11] = right_rot(b[11], rotates[37]); b[10] -= b[11];
                    b[ 9] ^= b[ 8]; b[ 9] = right_rot(b[ 9], rotates[36]); b[ 8] -= b[ 9];
                    b[ 7] ^= b[ 6]; b[ 7] = right_rot(b[ 7], rotates[35]); b[ 6] -= b[ 7];
                    b[ 5] ^= b[ 4]; b[ 5] = right_rot(b[ 5], rotates[34]); b[ 4] -= b[ 5];
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[33]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[32]); b[ 0] -= b[ 1];
            case 4:                                                                       
                    b[ 7] ^= b[12]; b[ 7] = right_rot(b[ 7], rotates[31]); b[12] -= b[ 7];
                    b[ 3] ^= b[10]; b[ 3] = right_rot(b[ 3], rotates[30]); b[10] -= b[ 3];
                    b[ 5] ^= b[ 8]; b[ 5] = right_rot(b[ 5], rotates[29]); b[ 8] -= b[ 5];
                    b[ 1] ^= b[14]; b[ 1] = right_rot(b[ 1], rotates[28]); b[14] -= b[ 1];
                    b[ 9] ^= b[ 4]; b[ 9] = right_rot(b[ 9], rotates[27]); b[ 4] -= b[ 9];
                    b[13] ^= b[ 6]; b[13] = right_rot(b[13], rotates[26]); b[ 6] -= b[13];
                    b[11] ^= b[ 2]; b[11] = right_rot(b[11], rotates[25]); b[ 2] -= b[11];
                    b[15] ^= b[ 0]; b[15] = right_rot(b[15], rotates[24]); b[ 0] -= b[15];
            case 3:                                                                       
                    b[ 9] ^= b[10]; b[ 9] = right_rot(b[ 9], rotates[23]); b[10] -= b[ 9];
                    b[11] ^= b[ 8]; b[11] = right_rot(b[11], rotates[22]); b[ 8] -= b[11];
                    b[13] ^= b[14]; b[13] = right_rot(b[13], rotates[21]); b[14] -= b[13];
                    b[15] ^= b[12]; b[15] = right_rot(b[15], rotates[20]); b[12] -= b[15];
                    b[ 1] ^= b[ 6]; b[ 1] = right_rot(b[ 1], rotates[19]); b[ 6] -= b[ 1];
                    b[ 3] ^= b[ 4]; b[ 3] = right_rot(b[ 3], rotates[18]); b[ 4] -= b[ 3];
                    b[ 5] ^= b[ 2]; b[ 5] = right_rot(b[ 5], rotates[17]); b[ 2] -= b[ 5];
                    b[ 7] ^= b[ 0]; b[ 7] = right_rot(b[ 7], rotates[16]); b[ 0] -= b[ 7];
            case 2:                                                                       
                    b[ 1] ^= b[ 8]; b[ 1] = right_rot(b[ 1], rotates[15]); b[ 8] -= b[ 1];
                    b[ 5] ^= b[14]; b[ 5] = right_rot(b[ 5], rotates[14]); b[14] -= b[ 5];
                    b[ 3] ^= b[12]; b[ 3] = right_rot(b[ 3], rotates[13]); b[12] -= b[ 3];
                    b[ 7] ^= b[10]; b[ 7] = right_rot(b[ 7], rotates[12]); b[10] -= b[ 7];
                    b[15] ^= b[ 4]; b[15] = right_rot(b[15], rotates[11]); b[ 4] -= b[15];
                    b[11] ^= b[ 6]; b[11] = right_rot(b[11], rotates[10]); b[ 6] -= b[11];
                    b[13] ^= b[ 2]; b[13] = right_rot(b[13], rotates[ 9]); b[ 2] -= b[13];
                    b[ 9] ^= b[ 0]; b[ 9] = right_rot(b[ 9], rotates[ 8]); b[ 0] -= b[ 9];
            case 1:                                                                       
                    b[15] ^= b[14]; b[15] = right_rot(b[15], rotates[ 7]); b[14] -= b[15];
                    b[13] ^= b[12]; b[13] = right_rot(b[13], rotates[ 6]); b[12] -= b[13];
                    b[11] ^= b[10]; b[11] = right_rot(b[11], rotates[ 5]); b[10] -= b[11];
                    b[ 9] ^= b[ 8]; b[ 9] = right_rot(b[ 9], rotates[ 4]); b[ 8] -= b[ 9];
                    b[ 7] ^= b[ 6]; b[ 7] = right_rot(b[ 7], rotates[ 3]); b[ 6] -= b[ 7];
                    b[ 5] ^= b[ 4]; b[ 5] = right_rot(b[ 5], rotates[ 2]); b[ 4] -= b[ 5];
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[ 1]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[ 0]); b[ 0] -= b[ 1];
            }                                                                             
                                                                                          
        }                                                                                 
    }                                                                                     
                                                                                          
void rev_cycle_8(Word *b, const u08b *rotates, int rounds)                                
    {                                                                                     
    for (;rounds > 0;rounds = (rounds-1) & ~7)                                            
        {                                                                                 
        switch (rounds & 7)                                                               
            {                                                                             
            case 0:                                                                       
                    b[ 3] ^= b[ 4]; b[ 3] = right_rot(b[ 3], rotates[31]); b[ 4] -= b[ 3];
                    b[ 5] ^= b[ 2]; b[ 5] = right_rot(b[ 5], rotates[30]); b[ 2] -= b[ 5];
                    b[ 7] ^= b[ 0]; b[ 7] = right_rot(b[ 7], rotates[29]); b[ 0] -= b[ 7];
                    b[ 1] ^= b[ 6]; b[ 1] = right_rot(b[ 1], rotates[28]); b[ 6] -= b[ 1];
            case 7:                                                                       
                    b[ 7] ^= b[ 2]; b[ 7] = right_rot(b[ 7], rotates[27]); b[ 2] -= b[ 7];
                    b[ 5] ^= b[ 0]; b[ 5] = right_rot(b[ 5], rotates[26]); b[ 0] -= b[ 5];
                    b[ 3] ^= b[ 6]; b[ 3] = right_rot(b[ 3], rotates[25]); b[ 6] -= b[ 3];
                    b[ 1] ^= b[ 4]; b[ 1] = right_rot(b[ 1], rotates[24]); b[ 4] -= b[ 1];
            case 6:                                                                       
                    b[ 3] ^= b[ 0]; b[ 3] = right_rot(b[ 3], rotates[23]); b[ 0] -= b[ 3];
                    b[ 5] ^= b[ 6]; b[ 5] = right_rot(b[ 5], rotates[22]); b[ 6] -= b[ 5];
                    b[ 7] ^= b[ 4]; b[ 7] = right_rot(b[ 7], rotates[21]); b[ 4] -= b[ 7];
                    b[ 1] ^= b[ 2]; b[ 1] = right_rot(b[ 1], rotates[20]); b[ 2] -= b[ 1];
            case 5:                                                                       
                    b[ 7] ^= b[ 6]; b[ 7] = right_rot(b[ 7], rotates[19]); b[ 6] -= b[ 7];
                    b[ 5] ^= b[ 4]; b[ 5] = right_rot(b[ 5], rotates[18]); b[ 4] -= b[ 5];
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[17]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[16]); b[ 0] -= b[ 1];
            case 4:                                                                       
                    b[ 3] ^= b[ 4]; b[ 3] = right_rot(b[ 3], rotates[15]); b[ 4] -= b[ 3];
                    b[ 5] ^= b[ 2]; b[ 5] = right_rot(b[ 5], rotates[14]); b[ 2] -= b[ 5];
                    b[ 7] ^= b[ 0]; b[ 7] = right_rot(b[ 7], rotates[13]); b[ 0] -= b[ 7];
                    b[ 1] ^= b[ 6]; b[ 1] = right_rot(b[ 1], rotates[12]); b[ 6] -= b[ 1];
            case 3:                                                                       
                    b[ 7] ^= b[ 2]; b[ 7] = right_rot(b[ 7], rotates[11]); b[ 2] -= b[ 7];
                    b[ 5] ^= b[ 0]; b[ 5] = right_rot(b[ 5], rotates[10]); b[ 0] -= b[ 5];
                    b[ 3] ^= b[ 6]; b[ 3] = right_rot(b[ 3], rotates[ 9]); b[ 6] -= b[ 3];
                    b[ 1] ^= b[ 4]; b[ 1] = right_rot(b[ 1], rotates[ 8]); b[ 4] -= b[ 1];
            case 2:                                                                       
                    b[ 3] ^= b[ 0]; b[ 3] = right_rot(b[ 3], rotates[ 7]); b[ 0] -= b[ 3];
                    b[ 5] ^= b[ 6]; b[ 5] = right_rot(b[ 5], rotates[ 6]); b[ 6] -= b[ 5];
                    b[ 7] ^= b[ 4]; b[ 7] = right_rot(b[ 7], rotates[ 5]); b[ 4] -= b[ 7];
                    b[ 1] ^= b[ 2]; b[ 1] = right_rot(b[ 1], rotates[ 4]); b[ 2] -= b[ 1];
            case 1:                                                                       
                    b[ 7] ^= b[ 6]; b[ 7] = right_rot(b[ 7], rotates[ 3]); b[ 6] -= b[ 7];
                    b[ 5] ^= b[ 4]; b[ 5] = right_rot(b[ 5], rotates[ 2]); b[ 4] -= b[ 5];
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[ 1]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[ 0]); b[ 0] -= b[ 1];
            }                                                                             
        }                                                                                 
    }                                                                                     
                                                                                          
void rev_cycle_4(Word *b, const u08b *rotates, int rounds)                                
    {                                                                                     
    for (;rounds > 0;rounds = (rounds-1) & ~7)                                            
        {                                                                                 
        switch (rounds & 7)                                                               
            {                                                                             
            case 0:                                                                       
                    b[ 1] ^= b[ 2]; b[ 1] = right_rot(b[ 1], rotates[15]); b[ 2] -= b[ 1];
                    b[ 3] ^= b[ 0]; b[ 3] = right_rot(b[ 3], rotates[14]); b[ 0] -= b[ 3];
            case 7:                                                                       
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[13]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[12]); b[ 0] -= b[ 1];
            case 6:                                                                       
                    b[ 1] ^= b[ 2]; b[ 1] = right_rot(b[ 1], rotates[11]); b[ 2] -= b[ 1];
                    b[ 3] ^= b[ 0]; b[ 3] = right_rot(b[ 3], rotates[10]); b[ 0] -= b[ 3];
            case 5:                                                                       
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[ 9]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[ 8]); b[ 0] -= b[ 1];
            case 4:                                                                       
                    b[ 1] ^= b[ 2]; b[ 1] = right_rot(b[ 1], rotates[ 7]); b[ 2] -= b[ 1];
                    b[ 3] ^= b[ 0]; b[ 3] = right_rot(b[ 3], rotates[ 6]); b[ 0] -= b[ 3];
            case 3:                                                                       
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[ 5]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[ 4]); b[ 0] -= b[ 1];
            case 2:                                                                       
                    b[ 1] ^= b[ 2]; b[ 1] = right_rot(b[ 1], rotates[ 3]); b[ 2] -= b[ 1];
                    b[ 3] ^= b[ 0]; b[ 3] = right_rot(b[ 3], rotates[ 2]); b[ 0] -= b[ 3];
            case 1:                                                                       
                    b[ 3] ^= b[ 2]; b[ 3] = right_rot(b[ 3], rotates[ 1]); b[ 2] -= b[ 3];
                    b[ 1] ^= b[ 0]; b[ 1] = right_rot(b[ 1], rotates[ 0]); b[ 0] -= b[ 1];
            }
        }
    }

#ifdef TEST_OR  /* enable this to simplify testing, since OR is not invertible */
#define AddOp(I,J) b[I] += b[J]
#define SubOp(I,J) b[I] -= b[J]
#define XorOp(I,J) b[I] ^= b[J]
#else           /* this is the "real" OR version */
#define AddOp(I,J) b[I] |= b[J]
#define SubOp(I,J) b[I] |= b[J]
#define XorOp(I,J) b[I] |= b[J]
#endif

/* "OR" versions of the cipher: replace ADD, XOR with OR */
void fwd_cycle_16_or(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds -=8)
        {
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[ 2]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[ 3]); XorOp( 7, 6);
        AddOp( 8, 9); b[ 9] = left_rot(b[ 9], rotates[ 4]); XorOp( 9, 8);
        AddOp(10,11); b[11] = left_rot(b[11], rotates[ 5]); XorOp(11,10);
        AddOp(12,13); b[13] = left_rot(b[13], rotates[ 6]); XorOp(13,12);
        AddOp(14,15); b[15] = left_rot(b[15], rotates[ 7]); XorOp(15,14);
        if (rounds == 1) break;                         
                                                        
        AddOp( 0, 9); b[ 9] = left_rot(b[ 9], rotates[ 8]); XorOp( 9, 0);
        AddOp( 2,13); b[13] = left_rot(b[13], rotates[ 9]); XorOp(13, 2);
        AddOp( 6,11); b[11] = left_rot(b[11], rotates[10]); XorOp(11, 6);
        AddOp( 4,15); b[15] = left_rot(b[15], rotates[11]); XorOp(15, 4);
        AddOp(10, 7); b[ 7] = left_rot(b[ 7], rotates[12]); XorOp( 7,10);
        AddOp(12, 3); b[ 3] = left_rot(b[ 3], rotates[13]); XorOp( 3,12);
        AddOp(14, 5); b[ 5] = left_rot(b[ 5], rotates[14]); XorOp( 5,14);
        AddOp( 8, 1); b[ 1] = left_rot(b[ 1], rotates[15]); XorOp( 1, 8);
        if (rounds == 2) break;                         
                                                        
        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[16]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[17]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[18]); XorOp( 3, 4);
        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[19]); XorOp( 1, 6);
        AddOp(12,15); b[15] = left_rot(b[15], rotates[20]); XorOp(15,12);
        AddOp(14,13); b[13] = left_rot(b[13], rotates[21]); XorOp(13,14);
        AddOp( 8,11); b[11] = left_rot(b[11], rotates[22]); XorOp(11, 8);
        AddOp(10, 9); b[ 9] = left_rot(b[ 9], rotates[23]); XorOp( 9,10);
        if (rounds == 3) break;                         
                                                        
        AddOp( 0,15); b[15] = left_rot(b[15], rotates[24]); XorOp(15, 0);
        AddOp( 2,11); b[11] = left_rot(b[11], rotates[25]); XorOp(11, 2);
        AddOp( 6,13); b[13] = left_rot(b[13], rotates[26]); XorOp(13, 6);
        AddOp( 4, 9); b[ 9] = left_rot(b[ 9], rotates[27]); XorOp( 9, 4);
        AddOp(14, 1); b[ 1] = left_rot(b[ 1], rotates[28]); XorOp( 1,14);
        AddOp( 8, 5); b[ 5] = left_rot(b[ 5], rotates[29]); XorOp( 5, 8);
        AddOp(10, 3); b[ 3] = left_rot(b[ 3], rotates[30]); XorOp( 3,10);
        AddOp(12, 7); b[ 7] = left_rot(b[ 7], rotates[31]); XorOp( 7,12);
        if (rounds == 4) break;                         
                                                        
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[32]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[33]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[34]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[35]); XorOp( 7, 6);
        AddOp( 8, 9); b[ 9] = left_rot(b[ 9], rotates[36]); XorOp( 9, 8);
        AddOp(10,11); b[11] = left_rot(b[11], rotates[37]); XorOp(11,10);
        AddOp(12,13); b[13] = left_rot(b[13], rotates[38]); XorOp(13,12);
        AddOp(14,15); b[15] = left_rot(b[15], rotates[39]); XorOp(15,14);
        if (rounds == 5) break;                         
                                                        
        AddOp( 0, 9); b[ 9] = left_rot(b[ 9], rotates[40]); XorOp( 9, 0);
        AddOp( 2,13); b[13] = left_rot(b[13], rotates[41]); XorOp(13, 2);
        AddOp( 6,11); b[11] = left_rot(b[11], rotates[42]); XorOp(11, 6);
        AddOp( 4,15); b[15] = left_rot(b[15], rotates[43]); XorOp(15, 4);
        AddOp(10, 7); b[ 7] = left_rot(b[ 7], rotates[44]); XorOp( 7,10);
        AddOp(12, 3); b[ 3] = left_rot(b[ 3], rotates[45]); XorOp( 3,12);
        AddOp(14, 5); b[ 5] = left_rot(b[ 5], rotates[46]); XorOp( 5,14);
        AddOp( 8, 1); b[ 1] = left_rot(b[ 1], rotates[47]); XorOp( 1, 8);
        if (rounds == 6) break;                         
                                                        
        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[48]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[49]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[50]); XorOp( 3, 4);
        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[51]); XorOp( 1, 6);
        AddOp(12,15); b[15] = left_rot(b[15], rotates[52]); XorOp(15,12);
        AddOp(14,13); b[13] = left_rot(b[13], rotates[53]); XorOp(13,14);
        AddOp( 8,11); b[11] = left_rot(b[11], rotates[54]); XorOp(11, 8);
        AddOp(10, 9); b[ 9] = left_rot(b[ 9], rotates[55]); XorOp( 9,10);
        if (rounds == 7) break;                         
                                                        
        AddOp( 0,15); b[15] = left_rot(b[15], rotates[56]); XorOp(15, 0);
        AddOp( 2,11); b[11] = left_rot(b[11], rotates[57]); XorOp(11, 2);
        AddOp( 6,13); b[13] = left_rot(b[13], rotates[58]); XorOp(13, 6);
        AddOp( 4, 9); b[ 9] = left_rot(b[ 9], rotates[59]); XorOp( 9, 4);
        AddOp(14, 1); b[ 1] = left_rot(b[ 1], rotates[60]); XorOp( 1,14);
        AddOp( 8, 5); b[ 5] = left_rot(b[ 5], rotates[61]); XorOp( 5, 8);
        AddOp(10, 3); b[ 3] = left_rot(b[ 3], rotates[62]); XorOp( 3,10);
        AddOp(12, 7); b[ 7] = left_rot(b[ 7], rotates[63]); XorOp( 7,12);
        }
    }

void fwd_cycle_8_or(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds -=8)
        {
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[ 2]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[ 3]); XorOp( 7, 6);
        if (rounds == 1) break;

        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[ 4]); XorOp( 1, 2);
        AddOp( 4, 7); b[ 7] = left_rot(b[ 7], rotates[ 5]); XorOp( 7, 4);
        AddOp( 6, 5); b[ 5] = left_rot(b[ 5], rotates[ 6]); XorOp( 5, 6);
        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[ 7]); XorOp( 3, 0);
        if (rounds == 2) break;

        AddOp( 4, 1); b[ 1] = left_rot(b[ 1], rotates[ 8]); XorOp( 1, 4);
        AddOp( 6, 3); b[ 3] = left_rot(b[ 3], rotates[ 9]); XorOp( 3, 6);
        AddOp( 0, 5); b[ 5] = left_rot(b[ 5], rotates[10]); XorOp( 5, 0);
        AddOp( 2, 7); b[ 7] = left_rot(b[ 7], rotates[11]); XorOp( 7, 2);
        if (rounds == 3) break;

        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[12]); XorOp( 1, 6);
        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[13]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[14]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[15]); XorOp( 3, 4);
        if (rounds == 4) break;

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[16]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[17]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[18]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[19]); XorOp( 7, 6);
        if (rounds == 5) break;

        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[20]); XorOp( 1, 2);
        AddOp( 4, 7); b[ 7] = left_rot(b[ 7], rotates[21]); XorOp( 7, 4);
        AddOp( 6, 5); b[ 5] = left_rot(b[ 5], rotates[22]); XorOp( 5, 6);
        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[23]); XorOp( 3, 0);
        if (rounds == 6) break;

        AddOp( 4, 1); b[ 1] = left_rot(b[ 1], rotates[24]); XorOp( 1, 4);
        AddOp( 6, 3); b[ 3] = left_rot(b[ 3], rotates[25]); XorOp( 3, 6);
        AddOp( 0, 5); b[ 5] = left_rot(b[ 5], rotates[26]); XorOp( 5, 0);
        AddOp( 2, 7); b[ 7] = left_rot(b[ 7], rotates[27]); XorOp( 7, 2);
        if (rounds == 7) break;

        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[28]); XorOp( 1, 6);
        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[29]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[30]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[31]); XorOp( 3, 4);
        }
    }

void fwd_cycle_4_or(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds -=8)
        {
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);
        if (rounds == 1) break;

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[ 2]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[ 3]); XorOp( 1, 2);
        if (rounds == 2) break;

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 4]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 5]); XorOp( 3, 2);
        if (rounds == 3) break;

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[ 6]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[ 7]); XorOp( 1, 2);
        if (rounds == 4) break;

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 8]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 9]); XorOp( 3, 2);
        if (rounds == 5) break;

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[10]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[11]); XorOp( 1, 2);
        if (rounds == 6) break;

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[12]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[13]); XorOp( 3, 2);
        if (rounds == 7) break;

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[14]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[15]); XorOp( 1, 2);
        }
    }

/* reverse versions of the cipher, using OR */
void rev_cycle_16_or(Word *b, const u08b *rotates, int rounds)
    {
    for (;rounds > 0;rounds = (rounds-1) & ~7)
        {
        switch (rounds & 7)
            {
            case 0:
                    XorOp( 7,12); b[ 7] = right_rot(b[ 7], rotates[63]); SubOp(12, 7); 
                    XorOp( 3,10); b[ 3] = right_rot(b[ 3], rotates[62]); SubOp(10, 3); 
                    XorOp( 5, 8); b[ 5] = right_rot(b[ 5], rotates[61]); SubOp( 8, 5); 
                    XorOp( 1,14); b[ 1] = right_rot(b[ 1], rotates[60]); SubOp(14, 1); 
                    XorOp( 9, 4); b[ 9] = right_rot(b[ 9], rotates[59]); SubOp( 4, 9); 
                    XorOp(13, 6); b[13] = right_rot(b[13], rotates[58]); SubOp( 6,13); 
                    XorOp(11, 2); b[11] = right_rot(b[11], rotates[57]); SubOp( 2,11); 
                    XorOp(15, 0); b[15] = right_rot(b[15], rotates[56]); SubOp( 0,15);
            case 7:
                    XorOp( 9,10); b[ 9] = right_rot(b[ 9], rotates[55]); SubOp(10, 9); 
                    XorOp(11, 8); b[11] = right_rot(b[11], rotates[54]); SubOp( 8,11); 
                    XorOp(13,14); b[13] = right_rot(b[13], rotates[53]); SubOp(14,13); 
                    XorOp(15,12); b[15] = right_rot(b[15], rotates[52]); SubOp(12,15); 
                    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[51]); SubOp( 6, 1); 
                    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[50]); SubOp( 4, 3); 
                    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[49]); SubOp( 2, 5); 
                    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[48]); SubOp( 0, 7);
            case 6:
                    XorOp( 1, 8); b[ 1] = right_rot(b[ 1], rotates[47]); SubOp( 8, 1); 
                    XorOp( 5,14); b[ 5] = right_rot(b[ 5], rotates[46]); SubOp(14, 5); 
                    XorOp( 3,12); b[ 3] = right_rot(b[ 3], rotates[45]); SubOp(12, 3); 
                    XorOp( 7,10); b[ 7] = right_rot(b[ 7], rotates[44]); SubOp(10, 7); 
                    XorOp(15, 4); b[15] = right_rot(b[15], rotates[43]); SubOp( 4,15); 
                    XorOp(11, 6); b[11] = right_rot(b[11], rotates[42]); SubOp( 6,11); 
                    XorOp(13, 2); b[13] = right_rot(b[13], rotates[41]); SubOp( 2,13); 
                    XorOp( 9, 0); b[ 9] = right_rot(b[ 9], rotates[40]); SubOp( 0, 9);
            case 5:
                    XorOp(15,14); b[15] = right_rot(b[15], rotates[39]); SubOp(14,15); 
                    XorOp(13,12); b[13] = right_rot(b[13], rotates[38]); SubOp(12,13); 
                    XorOp(11,10); b[11] = right_rot(b[11], rotates[37]); SubOp(10,11); 
                    XorOp( 9, 8); b[ 9] = right_rot(b[ 9], rotates[36]); SubOp( 8, 9); 
                    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[35]); SubOp( 6, 7); 
                    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[34]); SubOp( 4, 5); 
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[33]); SubOp( 2, 3); 
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[32]); SubOp( 0, 1);
            case 4:
                    XorOp( 7,12); b[ 7] = right_rot(b[ 7], rotates[31]); SubOp(12, 7); 
                    XorOp( 3,10); b[ 3] = right_rot(b[ 3], rotates[30]); SubOp(10, 3); 
                    XorOp( 5, 8); b[ 5] = right_rot(b[ 5], rotates[29]); SubOp( 8, 5); 
                    XorOp( 1,14); b[ 1] = right_rot(b[ 1], rotates[28]); SubOp(14, 1); 
                    XorOp( 9, 4); b[ 9] = right_rot(b[ 9], rotates[27]); SubOp( 4, 9); 
                    XorOp(13, 6); b[13] = right_rot(b[13], rotates[26]); SubOp( 6,13); 
                    XorOp(11, 2); b[11] = right_rot(b[11], rotates[25]); SubOp( 2,11); 
                    XorOp(15, 0); b[15] = right_rot(b[15], rotates[24]); SubOp( 0,15);
            case 3:                                                                       
                    XorOp( 9,10); b[ 9] = right_rot(b[ 9], rotates[23]); SubOp(10, 9);
                    XorOp(11, 8); b[11] = right_rot(b[11], rotates[22]); SubOp( 8,11);
                    XorOp(13,14); b[13] = right_rot(b[13], rotates[21]); SubOp(14,13);
                    XorOp(15,12); b[15] = right_rot(b[15], rotates[20]); SubOp(12,15);
                    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[19]); SubOp( 6, 1);
                    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[18]); SubOp( 4, 3);
                    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[17]); SubOp( 2, 5);
                    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[16]); SubOp( 0, 7);
            case 2:                                                                       
                    XorOp( 1, 8); b[ 1] = right_rot(b[ 1], rotates[15]); SubOp( 8, 1);
                    XorOp( 5,14); b[ 5] = right_rot(b[ 5], rotates[14]); SubOp(14, 5);
                    XorOp( 3,12); b[ 3] = right_rot(b[ 3], rotates[13]); SubOp(12, 3);
                    XorOp( 7,10); b[ 7] = right_rot(b[ 7], rotates[12]); SubOp(10, 7);
                    XorOp(15, 4); b[15] = right_rot(b[15], rotates[11]); SubOp( 4,15);
                    XorOp(11, 6); b[11] = right_rot(b[11], rotates[10]); SubOp( 6,11);
                    XorOp(13, 2); b[13] = right_rot(b[13], rotates[ 9]); SubOp( 2,13);
                    XorOp( 9, 0); b[ 9] = right_rot(b[ 9], rotates[ 8]); SubOp( 0, 9);
            case 1:                                                                       
                    XorOp(15,14); b[15] = right_rot(b[15], rotates[ 7]); SubOp(14,15);
                    XorOp(13,12); b[13] = right_rot(b[13], rotates[ 6]); SubOp(12,13);
                    XorOp(11,10); b[11] = right_rot(b[11], rotates[ 5]); SubOp(10,11);
                    XorOp( 9, 8); b[ 9] = right_rot(b[ 9], rotates[ 4]); SubOp( 8, 9);
                    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[ 3]); SubOp( 6, 7);
                    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[ 2]); SubOp( 4, 5);
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
            }                                                                             
                                                                                          
        }                                                                                 
    }                                                                                     
                                                                                          
void rev_cycle_8_or(Word *b, const u08b *rotates, int rounds)                             
    {                                                                                     
    for (;rounds > 0;rounds = (rounds-1) & ~7)                                            
        {                                                                                 
        switch (rounds & 7)                                                               
            {                                                                             
            case 0:                                                                       
                    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[31]); SubOp( 4, 3);
                    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[30]); SubOp( 2, 5);
                    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[29]); SubOp( 0, 7);
                    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[28]); SubOp( 6, 1);
            case 7:                                                                       
                    XorOp( 7, 2); b[ 7] = right_rot(b[ 7], rotates[27]); SubOp( 2, 7);
                    XorOp( 5, 0); b[ 5] = right_rot(b[ 5], rotates[26]); SubOp( 0, 5);
                    XorOp( 3, 6); b[ 3] = right_rot(b[ 3], rotates[25]); SubOp( 6, 3);
                    XorOp( 1, 4); b[ 1] = right_rot(b[ 1], rotates[24]); SubOp( 4, 1);
            case 6:                                                                       
                    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[23]); SubOp( 0, 3);
                    XorOp( 5, 6); b[ 5] = right_rot(b[ 5], rotates[22]); SubOp( 6, 5);
                    XorOp( 7, 4); b[ 7] = right_rot(b[ 7], rotates[21]); SubOp( 4, 7);
                    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[20]); SubOp( 2, 1);
            case 5:                                                                       
                    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[19]); SubOp( 6, 7);
                    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[18]); SubOp( 4, 5);
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[17]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[16]); SubOp( 0, 1);
            case 4:                                                                       
                    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[15]); SubOp( 4, 3);
                    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[14]); SubOp( 2, 5);
                    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[13]); SubOp( 0, 7);
                    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[12]); SubOp( 6, 1);
            case 3:                                                                       
                    XorOp( 7, 2); b[ 7] = right_rot(b[ 7], rotates[11]); SubOp( 2, 7);
                    XorOp( 5, 0); b[ 5] = right_rot(b[ 5], rotates[10]); SubOp( 0, 5);
                    XorOp( 3, 6); b[ 3] = right_rot(b[ 3], rotates[ 9]); SubOp( 6, 3);
                    XorOp( 1, 4); b[ 1] = right_rot(b[ 1], rotates[ 8]); SubOp( 4, 1);
            case 2:                                                                       
                    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[ 7]); SubOp( 0, 3);
                    XorOp( 5, 6); b[ 5] = right_rot(b[ 5], rotates[ 6]); SubOp( 6, 5);
                    XorOp( 7, 4); b[ 7] = right_rot(b[ 7], rotates[ 5]); SubOp( 4, 7);
                    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[ 4]); SubOp( 2, 1);
            case 1:                                                                       
                    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[ 3]); SubOp( 6, 7);
                    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[ 2]); SubOp( 4, 5);
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
            }                                                                             
        }                                                                                 
    }                                                                                     
                                                                                          
void rev_cycle_4_or(Word *b, const u08b *rotates, int rounds)                             
    {                                                                                     
    for (;rounds > 0;rounds = (rounds-1) & ~7)                                            
        {                                                                                 
        switch (rounds & 7)                                                               
            {                                                                             
            case 0:                                                                       
                    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[15]); SubOp( 2, 1);
                    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[14]); SubOp( 0, 3);
            case 7:                                                                       
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[13]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[12]); SubOp( 0, 1);
            case 6:                                                                       
                    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[11]); SubOp( 2, 1);
                    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[10]); SubOp( 0, 3);
            case 5:                                                                       
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 9]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 8]); SubOp( 0, 1);
            case 4:                                                                       
                    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[ 7]); SubOp( 2, 1);
                    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[ 6]); SubOp( 0, 3);
            case 3:                                                                       
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 5]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 4]); SubOp( 0, 1);
            case 2:                                                                       
                    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[ 3]); SubOp( 2, 1);
                    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[ 2]); SubOp( 0, 3);
            case 1:                                                                       
                    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
                    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
            }
        }
    }

/* optimized versions for default round counts */
#if   defined(__BORLANDC__)
#pragma argsused
#elif defined(_MSC_VER)
#pragma warning(disable:4100)
#endif
void fwd_cycle_16_or_r9(Word *b, const u08b *rotates, int rounds)
    {
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[ 2]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[ 3]); XorOp( 7, 6);
        AddOp( 8, 9); b[ 9] = left_rot(b[ 9], rotates[ 4]); XorOp( 9, 8);
        AddOp(10,11); b[11] = left_rot(b[11], rotates[ 5]); XorOp(11,10);
        AddOp(12,13); b[13] = left_rot(b[13], rotates[ 6]); XorOp(13,12);
        AddOp(14,15); b[15] = left_rot(b[15], rotates[ 7]); XorOp(15,14);

        AddOp( 0, 9); b[ 9] = left_rot(b[ 9], rotates[ 8]); XorOp( 9, 0);
        AddOp( 2,13); b[13] = left_rot(b[13], rotates[ 9]); XorOp(13, 2);
        AddOp( 6,11); b[11] = left_rot(b[11], rotates[10]); XorOp(11, 6);
        AddOp( 4,15); b[15] = left_rot(b[15], rotates[11]); XorOp(15, 4);
        AddOp(10, 7); b[ 7] = left_rot(b[ 7], rotates[12]); XorOp( 7,10);
        AddOp(12, 3); b[ 3] = left_rot(b[ 3], rotates[13]); XorOp( 3,12);
        AddOp(14, 5); b[ 5] = left_rot(b[ 5], rotates[14]); XorOp( 5,14);
        AddOp( 8, 1); b[ 1] = left_rot(b[ 1], rotates[15]); XorOp( 1, 8);

        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[16]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[17]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[18]); XorOp( 3, 4);
        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[19]); XorOp( 1, 6);
        AddOp(12,15); b[15] = left_rot(b[15], rotates[20]); XorOp(15,12);
        AddOp(14,13); b[13] = left_rot(b[13], rotates[21]); XorOp(13,14);
        AddOp( 8,11); b[11] = left_rot(b[11], rotates[22]); XorOp(11, 8);
        AddOp(10, 9); b[ 9] = left_rot(b[ 9], rotates[23]); XorOp( 9,10);

        AddOp( 0,15); b[15] = left_rot(b[15], rotates[24]); XorOp(15, 0);
        AddOp( 2,11); b[11] = left_rot(b[11], rotates[25]); XorOp(11, 2);
        AddOp( 6,13); b[13] = left_rot(b[13], rotates[26]); XorOp(13, 6);
        AddOp( 4, 9); b[ 9] = left_rot(b[ 9], rotates[27]); XorOp( 9, 4);
        AddOp(14, 1); b[ 1] = left_rot(b[ 1], rotates[28]); XorOp( 1,14);
        AddOp( 8, 5); b[ 5] = left_rot(b[ 5], rotates[29]); XorOp( 5, 8);
        AddOp(10, 3); b[ 3] = left_rot(b[ 3], rotates[30]); XorOp( 3,10);
        AddOp(12, 7); b[ 7] = left_rot(b[ 7], rotates[31]); XorOp( 7,12);

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[32]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[33]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[34]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[35]); XorOp( 7, 6);
        AddOp( 8, 9); b[ 9] = left_rot(b[ 9], rotates[36]); XorOp( 9, 8);
        AddOp(10,11); b[11] = left_rot(b[11], rotates[37]); XorOp(11,10);
        AddOp(12,13); b[13] = left_rot(b[13], rotates[38]); XorOp(13,12);
        AddOp(14,15); b[15] = left_rot(b[15], rotates[39]); XorOp(15,14);

        AddOp( 0, 9); b[ 9] = left_rot(b[ 9], rotates[40]); XorOp( 9, 0);
        AddOp( 2,13); b[13] = left_rot(b[13], rotates[41]); XorOp(13, 2);
        AddOp( 6,11); b[11] = left_rot(b[11], rotates[42]); XorOp(11, 6);
        AddOp( 4,15); b[15] = left_rot(b[15], rotates[43]); XorOp(15, 4);
        AddOp(10, 7); b[ 7] = left_rot(b[ 7], rotates[44]); XorOp( 7,10);
        AddOp(12, 3); b[ 3] = left_rot(b[ 3], rotates[45]); XorOp( 3,12);
        AddOp(14, 5); b[ 5] = left_rot(b[ 5], rotates[46]); XorOp( 5,14);
        AddOp( 8, 1); b[ 1] = left_rot(b[ 1], rotates[47]); XorOp( 1, 8);

        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[48]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[49]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[50]); XorOp( 3, 4);
        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[51]); XorOp( 1, 6);
        AddOp(12,15); b[15] = left_rot(b[15], rotates[52]); XorOp(15,12);
        AddOp(14,13); b[13] = left_rot(b[13], rotates[53]); XorOp(13,14);
        AddOp( 8,11); b[11] = left_rot(b[11], rotates[54]); XorOp(11, 8);
        AddOp(10, 9); b[ 9] = left_rot(b[ 9], rotates[55]); XorOp( 9,10);

        AddOp( 0,15); b[15] = left_rot(b[15], rotates[56]); XorOp(15, 0);
        AddOp( 2,11); b[11] = left_rot(b[11], rotates[57]); XorOp(11, 2);
        AddOp( 6,13); b[13] = left_rot(b[13], rotates[58]); XorOp(13, 6);
        AddOp( 4, 9); b[ 9] = left_rot(b[ 9], rotates[59]); XorOp( 9, 4);
        AddOp(14, 1); b[ 1] = left_rot(b[ 1], rotates[60]); XorOp( 1,14);
        AddOp( 8, 5); b[ 5] = left_rot(b[ 5], rotates[61]); XorOp( 5, 8);
        AddOp(10, 3); b[ 3] = left_rot(b[ 3], rotates[62]); XorOp( 3,10);
        AddOp(12, 7); b[ 7] = left_rot(b[ 7], rotates[63]); XorOp( 7,12);

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[ 2]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[ 3]); XorOp( 7, 6);
        AddOp( 8, 9); b[ 9] = left_rot(b[ 9], rotates[ 4]); XorOp( 9, 8);
        AddOp(10,11); b[11] = left_rot(b[11], rotates[ 5]); XorOp(11,10);
        AddOp(12,13); b[13] = left_rot(b[13], rotates[ 6]); XorOp(13,12);
        AddOp(14,15); b[15] = left_rot(b[15], rotates[ 7]); XorOp(15,14);
    }

#if   defined(__BORLANDC__)
#pragma argsused
#endif
void fwd_cycle_8_or_r8(Word *b, const u08b *rotates, int rounds)
    {
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[ 2]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[ 3]); XorOp( 7, 6);

        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[ 4]); XorOp( 1, 2);
        AddOp( 4, 7); b[ 7] = left_rot(b[ 7], rotates[ 5]); XorOp( 7, 4);
        AddOp( 6, 5); b[ 5] = left_rot(b[ 5], rotates[ 6]); XorOp( 5, 6);
        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[ 7]); XorOp( 3, 0);

        AddOp( 4, 1); b[ 1] = left_rot(b[ 1], rotates[ 8]); XorOp( 1, 4);
        AddOp( 6, 3); b[ 3] = left_rot(b[ 3], rotates[ 9]); XorOp( 3, 6);
        AddOp( 0, 5); b[ 5] = left_rot(b[ 5], rotates[10]); XorOp( 5, 0);
        AddOp( 2, 7); b[ 7] = left_rot(b[ 7], rotates[11]); XorOp( 7, 2);

        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[12]); XorOp( 1, 6);
        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[13]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[14]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[15]); XorOp( 3, 4);

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[16]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[17]); XorOp( 3, 2);
        AddOp( 4, 5); b[ 5] = left_rot(b[ 5], rotates[18]); XorOp( 5, 4);
        AddOp( 6, 7); b[ 7] = left_rot(b[ 7], rotates[19]); XorOp( 7, 6);

        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[20]); XorOp( 1, 2);
        AddOp( 4, 7); b[ 7] = left_rot(b[ 7], rotates[21]); XorOp( 7, 4);
        AddOp( 6, 5); b[ 5] = left_rot(b[ 5], rotates[22]); XorOp( 5, 6);
        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[23]); XorOp( 3, 0);

        AddOp( 4, 1); b[ 1] = left_rot(b[ 1], rotates[24]); XorOp( 1, 4);
        AddOp( 6, 3); b[ 3] = left_rot(b[ 3], rotates[25]); XorOp( 3, 6);
        AddOp( 0, 5); b[ 5] = left_rot(b[ 5], rotates[26]); XorOp( 5, 0);
        AddOp( 2, 7); b[ 7] = left_rot(b[ 7], rotates[27]); XorOp( 7, 2);

        AddOp( 6, 1); b[ 1] = left_rot(b[ 1], rotates[28]); XorOp( 1, 6);
        AddOp( 0, 7); b[ 7] = left_rot(b[ 7], rotates[29]); XorOp( 7, 0);
        AddOp( 2, 5); b[ 5] = left_rot(b[ 5], rotates[30]); XorOp( 5, 2);
        AddOp( 4, 3); b[ 3] = left_rot(b[ 3], rotates[31]); XorOp( 3, 4);
    }

#ifdef __BORLANDC__
#pragma argsused
#endif
void fwd_cycle_4_or_r8(Word *b, const u08b *rotates, int rounds)
    {
        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 0]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 1]); XorOp( 3, 2);

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[ 2]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[ 3]); XorOp( 1, 2);

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 4]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 5]); XorOp( 3, 2);

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[ 6]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[ 7]); XorOp( 1, 2);

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[ 8]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[ 9]); XorOp( 3, 2);

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[10]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[11]); XorOp( 1, 2);

        AddOp( 0, 1); b[ 1] = left_rot(b[ 1], rotates[12]); XorOp( 1, 0);
        AddOp( 2, 3); b[ 3] = left_rot(b[ 3], rotates[13]); XorOp( 3, 2);

        AddOp( 0, 3); b[ 3] = left_rot(b[ 3], rotates[14]); XorOp( 3, 0);
        AddOp( 2, 1); b[ 1] = left_rot(b[ 1], rotates[15]); XorOp( 1, 2);
    }

/* reverse versions of the cipher, using OR, for fixed round numbers */
#ifdef __BORLANDC__
#pragma argsused
#endif
void rev_cycle_16_or_r9(Word *b, const u08b *rotates, int rounds)
    {
    XorOp(15,14); b[15] = right_rot(b[15], rotates[ 7]); SubOp(14,15);
    XorOp(13,12); b[13] = right_rot(b[13], rotates[ 6]); SubOp(12,13);
    XorOp(11,10); b[11] = right_rot(b[11], rotates[ 5]); SubOp(10,11);
    XorOp( 9, 8); b[ 9] = right_rot(b[ 9], rotates[ 4]); SubOp( 8, 9);
    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[ 3]); SubOp( 6, 7);
    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[ 2]); SubOp( 4, 5);
    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
                                                     
    XorOp( 7,12); b[ 7] = right_rot(b[ 7], rotates[63]); SubOp(12, 7); 
    XorOp( 3,10); b[ 3] = right_rot(b[ 3], rotates[62]); SubOp(10, 3); 
    XorOp( 5, 8); b[ 5] = right_rot(b[ 5], rotates[61]); SubOp( 8, 5); 
    XorOp( 1,14); b[ 1] = right_rot(b[ 1], rotates[60]); SubOp(14, 1); 
    XorOp( 9, 4); b[ 9] = right_rot(b[ 9], rotates[59]); SubOp( 4, 9); 
    XorOp(13, 6); b[13] = right_rot(b[13], rotates[58]); SubOp( 6,13); 
    XorOp(11, 2); b[11] = right_rot(b[11], rotates[57]); SubOp( 2,11); 
    XorOp(15, 0); b[15] = right_rot(b[15], rotates[56]); SubOp( 0,15);
                                                     
    XorOp( 9,10); b[ 9] = right_rot(b[ 9], rotates[55]); SubOp(10, 9); 
    XorOp(11, 8); b[11] = right_rot(b[11], rotates[54]); SubOp( 8,11); 
    XorOp(13,14); b[13] = right_rot(b[13], rotates[53]); SubOp(14,13); 
    XorOp(15,12); b[15] = right_rot(b[15], rotates[52]); SubOp(12,15); 
    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[51]); SubOp( 6, 1); 
    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[50]); SubOp( 4, 3); 
    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[49]); SubOp( 2, 5); 
    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[48]); SubOp( 0, 7);
                                                     
    XorOp( 1, 8); b[ 1] = right_rot(b[ 1], rotates[47]); SubOp( 8, 1); 
    XorOp( 5,14); b[ 5] = right_rot(b[ 5], rotates[46]); SubOp(14, 5); 
    XorOp( 3,12); b[ 3] = right_rot(b[ 3], rotates[45]); SubOp(12, 3); 
    XorOp( 7,10); b[ 7] = right_rot(b[ 7], rotates[44]); SubOp(10, 7); 
    XorOp(15, 4); b[15] = right_rot(b[15], rotates[43]); SubOp( 4,15); 
    XorOp(11, 6); b[11] = right_rot(b[11], rotates[42]); SubOp( 6,11); 
    XorOp(13, 2); b[13] = right_rot(b[13], rotates[41]); SubOp( 2,13); 
    XorOp( 9, 0); b[ 9] = right_rot(b[ 9], rotates[40]); SubOp( 0, 9);
                                                     
    XorOp(15,14); b[15] = right_rot(b[15], rotates[39]); SubOp(14,15); 
    XorOp(13,12); b[13] = right_rot(b[13], rotates[38]); SubOp(12,13); 
    XorOp(11,10); b[11] = right_rot(b[11], rotates[37]); SubOp(10,11); 
    XorOp( 9, 8); b[ 9] = right_rot(b[ 9], rotates[36]); SubOp( 8, 9); 
    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[35]); SubOp( 6, 7); 
    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[34]); SubOp( 4, 5); 
    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[33]); SubOp( 2, 3); 
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[32]); SubOp( 0, 1);
                                                     
    XorOp( 7,12); b[ 7] = right_rot(b[ 7], rotates[31]); SubOp(12, 7); 
    XorOp( 3,10); b[ 3] = right_rot(b[ 3], rotates[30]); SubOp(10, 3); 
    XorOp( 5, 8); b[ 5] = right_rot(b[ 5], rotates[29]); SubOp( 8, 5); 
    XorOp( 1,14); b[ 1] = right_rot(b[ 1], rotates[28]); SubOp(14, 1); 
    XorOp( 9, 4); b[ 9] = right_rot(b[ 9], rotates[27]); SubOp( 4, 9); 
    XorOp(13, 6); b[13] = right_rot(b[13], rotates[26]); SubOp( 6,13); 
    XorOp(11, 2); b[11] = right_rot(b[11], rotates[25]); SubOp( 2,11); 
    XorOp(15, 0); b[15] = right_rot(b[15], rotates[24]); SubOp( 0,15);
                                                     
    XorOp( 9,10); b[ 9] = right_rot(b[ 9], rotates[23]); SubOp(10, 9);
    XorOp(11, 8); b[11] = right_rot(b[11], rotates[22]); SubOp( 8,11);
    XorOp(13,14); b[13] = right_rot(b[13], rotates[21]); SubOp(14,13);
    XorOp(15,12); b[15] = right_rot(b[15], rotates[20]); SubOp(12,15);
    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[19]); SubOp( 6, 1);
    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[18]); SubOp( 4, 3);
    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[17]); SubOp( 2, 5);
    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[16]); SubOp( 0, 7);
                                                     
    XorOp( 1, 8); b[ 1] = right_rot(b[ 1], rotates[15]); SubOp( 8, 1);
    XorOp( 5,14); b[ 5] = right_rot(b[ 5], rotates[14]); SubOp(14, 5);
    XorOp( 3,12); b[ 3] = right_rot(b[ 3], rotates[13]); SubOp(12, 3);
    XorOp( 7,10); b[ 7] = right_rot(b[ 7], rotates[12]); SubOp(10, 7);
    XorOp(15, 4); b[15] = right_rot(b[15], rotates[11]); SubOp( 4,15);
    XorOp(11, 6); b[11] = right_rot(b[11], rotates[10]); SubOp( 6,11);
    XorOp(13, 2); b[13] = right_rot(b[13], rotates[ 9]); SubOp( 2,13);
    XorOp( 9, 0); b[ 9] = right_rot(b[ 9], rotates[ 8]); SubOp( 0, 9);
                                                     
    XorOp(15,14); b[15] = right_rot(b[15], rotates[ 7]); SubOp(14,15);
    XorOp(13,12); b[13] = right_rot(b[13], rotates[ 6]); SubOp(12,13);
    XorOp(11,10); b[11] = right_rot(b[11], rotates[ 5]); SubOp(10,11);
    XorOp( 9, 8); b[ 9] = right_rot(b[ 9], rotates[ 4]); SubOp( 8, 9);
    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[ 3]); SubOp( 6, 7);
    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[ 2]); SubOp( 4, 5);
    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
    }
                                                                                          
#ifdef __BORLANDC__
#pragma argsused
#endif
void rev_cycle_8_or_r8(Word *b, const u08b *rotates, int rounds)                             
    {                                                                                     
    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[31]); SubOp( 4, 3);
    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[30]); SubOp( 2, 5);
    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[29]); SubOp( 0, 7);
    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[28]); SubOp( 6, 1);

    XorOp( 7, 2); b[ 7] = right_rot(b[ 7], rotates[27]); SubOp( 2, 7);
    XorOp( 5, 0); b[ 5] = right_rot(b[ 5], rotates[26]); SubOp( 0, 5);
    XorOp( 3, 6); b[ 3] = right_rot(b[ 3], rotates[25]); SubOp( 6, 3);
    XorOp( 1, 4); b[ 1] = right_rot(b[ 1], rotates[24]); SubOp( 4, 1);

    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[23]); SubOp( 0, 3);
    XorOp( 5, 6); b[ 5] = right_rot(b[ 5], rotates[22]); SubOp( 6, 5);
    XorOp( 7, 4); b[ 7] = right_rot(b[ 7], rotates[21]); SubOp( 4, 7);
    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[20]); SubOp( 2, 1);

    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[19]); SubOp( 6, 7);
    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[18]); SubOp( 4, 5);
    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[17]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[16]); SubOp( 0, 1);

    XorOp( 3, 4); b[ 3] = right_rot(b[ 3], rotates[15]); SubOp( 4, 3);
    XorOp( 5, 2); b[ 5] = right_rot(b[ 5], rotates[14]); SubOp( 2, 5);
    XorOp( 7, 0); b[ 7] = right_rot(b[ 7], rotates[13]); SubOp( 0, 7);
    XorOp( 1, 6); b[ 1] = right_rot(b[ 1], rotates[12]); SubOp( 6, 1);

    XorOp( 7, 2); b[ 7] = right_rot(b[ 7], rotates[11]); SubOp( 2, 7);
    XorOp( 5, 0); b[ 5] = right_rot(b[ 5], rotates[10]); SubOp( 0, 5);
    XorOp( 3, 6); b[ 3] = right_rot(b[ 3], rotates[ 9]); SubOp( 6, 3);
    XorOp( 1, 4); b[ 1] = right_rot(b[ 1], rotates[ 8]); SubOp( 4, 1);

    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[ 7]); SubOp( 0, 3);
    XorOp( 5, 6); b[ 5] = right_rot(b[ 5], rotates[ 6]); SubOp( 6, 5);
    XorOp( 7, 4); b[ 7] = right_rot(b[ 7], rotates[ 5]); SubOp( 4, 7);
    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[ 4]); SubOp( 2, 1);

    XorOp( 7, 6); b[ 7] = right_rot(b[ 7], rotates[ 3]); SubOp( 6, 7);
    XorOp( 5, 4); b[ 5] = right_rot(b[ 5], rotates[ 2]); SubOp( 4, 5);
    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
    }                                                                                     
                                                                                          
#ifdef __BORLANDC__
#pragma argsused
#endif
void rev_cycle_4_or_r8(Word *b, const u08b *rotates, int rounds)                             
    {                                                                                     
    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[15]); SubOp( 2, 1);
    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[14]); SubOp( 0, 3);

    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[13]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[12]); SubOp( 0, 1);

    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[11]); SubOp( 2, 1);
    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[10]); SubOp( 0, 3);

    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 9]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 8]); SubOp( 0, 1);

    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[ 7]); SubOp( 2, 1);
    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[ 6]); SubOp( 0, 3);

    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 5]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 4]); SubOp( 0, 1);

    XorOp( 1, 2); b[ 1] = right_rot(b[ 1], rotates[ 3]); SubOp( 2, 1);
    XorOp( 3, 0); b[ 3] = right_rot(b[ 3], rotates[ 2]); SubOp( 0, 3);

    XorOp( 3, 2); b[ 3] = right_rot(b[ 3], rotates[ 1]); SubOp( 2, 3);
    XorOp( 1, 0); b[ 1] = right_rot(b[ 1], rotates[ 0]); SubOp( 0, 1);
    }


/* test that fwd and rev ciphers are truly inverses */
void InverseChecks(void)
    {
    uint  i,j,k,wCnt,tstCnt;
    int   r,rN;
    Block pt,ct,xt;
    u08b  rots[MAX_ROTS_PER_CYCLE];
    uint  TEST_CNT = (sizeof(size_t) == 8) ? 64 : 8;

    cycle_func *fwd;
    cycle_func *rev;
    cycle_func *fwd_or;
    cycle_func *fwd_or_rN;
#ifdef TEST_OR
    cycle_func *rev_or;
    cycle_func *rev_or_rN;
#endif
    
    Rand_Init(0);
    for (wCnt=4;wCnt<=MAX_WORDS_PER_BLK;wCnt *= 2)
        {
        switch (wCnt)
            {
            case  4: fwd       = fwd_cycle_4        ; rev       = rev_cycle_4        ;
                     fwd_or    = fwd_cycle_4_or     ; fwd_or_rN = fwd_cycle_4_or_r8  ; break;
            case  8: fwd       = fwd_cycle_8        ; rev       = rev_cycle_8        ;
                     fwd_or    = fwd_cycle_8_or     ; fwd_or_rN = fwd_cycle_8_or_r8  ; break;
            default: fwd       = fwd_cycle_16       ; rev       = rev_cycle_16       ; 
                     fwd_or    = fwd_cycle_16_or    ; fwd_or_rN = fwd_cycle_16_or_r9 ; break;
            }
#ifdef TEST_OR
        switch (wCnt)
            {
            case  4: rev_or_rN = rev_cycle_4_or_r8  ; rev_or    = rev_cycle_4_or     ; break;
            case  8: rev_or_rN = rev_cycle_8_or_r8  ; rev_or    = rev_cycle_8_or     ; break;
            default: rev_or_rN = rev_cycle_16_or_r9 ; rev_or    = rev_cycle_16_or    ; break;
            }
#endif
        for (tstCnt=0;tstCnt<TEST_CNT;tstCnt++)
            {
            if (tstCnt == 0)
                {
                memset(pt.x,0,sizeof(pt));      /* make the first test simple, for debug */
                pt.x[0]++;
                }
            else
                RandBytes(pt.x,wCnt*sizeof(pt.x[0]));

            RandBytes(rots,sizeof(rots));       /* use random rotation constants */
            for (i=0;i<MAX_ROTS_PER_CYCLE;i++)
                rots[i] &= (BITS_PER_WORD-1);
            for (r=1;r<32;r++)
                {
                ct=pt;
                rev(ct.x,rots,r);
                fwd(ct.x,rots,r);
                if (memcmp(pt.x,ct.x,wCnt*sizeof(pt.x[0])))
                    {
                    printf("Inverse failure: #%03d: wCnt=%d. r=%2d",tstCnt,wCnt,r);
                    exit(8);
                    }
                fwd(ct.x,rots,r);
                rev(ct.x,rots,r);
                if (memcmp(pt.x,ct.x,wCnt*sizeof(pt.x[0])))
                    {
                    printf("Inverse failure: #%03d: wCnt=%d. r=%2d",tstCnt,wCnt,r);
                    exit(8);
                    }
#ifdef TEST_OR
                fwd_or(ct.x,rots,r);
                rev   (ct.x,rots,r);
                if (memcmp(pt.x,ct.x,wCnt*sizeof(pt.x[0])))
                    {
                    printf("Inverse failure (fwd_or): #%03d: wCnt=%d. r=%2d",tstCnt,wCnt,r);
                    exit(8);
                    }
                fwd   (ct.x,rots,r);
                rev_or(ct.x,rots,r);
                if (memcmp(pt.x,ct.x,wCnt*sizeof(pt.x[0])))
                    {
                    printf("Inverse failure (rev_or): #%03d: wCnt=%d. r=%2d",tstCnt,wCnt,r);
                    exit(8);
                    }                
                if (r != ((wCnt == 16) ? 9 : 8))
                    continue;
                fwd_or_rN(ct.x,rots,r);
                rev      (ct.x,rots,r);
                if (memcmp(pt.x,ct.x,wCnt*sizeof(pt.x[0])))
                    {
                    printf("Inverse failure (fwd_or_rN): #%03d: wCnt=%d. r=%2d",tstCnt,wCnt,r);
                    exit(8);
                    }                
                fwd      (ct.x,rots,r);
                rev_or_rN(ct.x,rots,r);
                if (memcmp(pt.x,ct.x,wCnt*sizeof(pt.x[0])))
                    {
                    printf("Inverse failure (rev_or_rN): #%03d: wCnt=%d. r=%2d",tstCnt,wCnt,r);
                    exit(8);
                    }                
#else
                /* validate that "quick" Hamming weight checks are ok, using OR */
                for (i=0;i<wCnt;i++)
                    {
                    memset(ct.x,0,sizeof(ct.x));
                    ct.x[i]=1;
                    fwd_or(ct.x,rots,r);
                    for (j=1;j<64;j++)
                        {
                        memset(xt.x,0,sizeof(xt.x));
                        xt.x[i]=((u64b) 1) << j;
                        fwd_or(xt.x,rots,r);
                        for (k=0;k<wCnt;k++)
                            if (left_rot(ct.x[k],j) != xt.x[k])
                                {
                                printf("Quick HW check failure: blk=%4d bits. r=%d. j=%d",wCnt*64,r,j);
                                exit(2);
                                }
                        }
                    }
#endif
                }
            }
        /* test the "hard coded" versions against variable versions of OR routines */
        for (tstCnt=0;tstCnt<TEST_CNT;tstCnt++)
            {
            RandBytes(rots,sizeof(rots));
            for (i=0;i<MAX_ROTS_PER_CYCLE;i++)
                rots[i] &= (BITS_PER_WORD-1);
            rN = (wCnt == 16) ? 9 : 8;
            for (i=0;i<wCnt*64;i++)
                {
                memset(pt.x,0,sizeof(pt));
                pt.x[i / 64] = ((u64b) 1) << (i % 64);
                ct=pt;
                xt=pt;
                fwd_or   (ct.x,rots,rN);
                fwd_or_rN(xt.x,rots,rN);
                if (memcmp(xt.x,ct.x,wCnt*sizeof(xt.x[0])))
                    {
                    printf("OR failure: #%03d: wCnt=%d. i=%2d",tstCnt,wCnt,i);
                    exit(8);
                    }
                }
            }
        }
    }

/* count the bits set in the word */
uint HammingWeight(Word x)
    {
#if BITS_PER_WORD == 64
    x = (x & DUP_64(0x55555555)) + ((x >> 1) & DUP_64(0x55555555));
    x = (x & DUP_64(0x33333333)) + ((x >> 2) & DUP_64(0x33333333));
    x = (x & DUP_64(0x0F0F0F0F)) + ((x >> 4) & DUP_64(0x0F0F0F0F));
    x = (x & DUP_64(0x00FF00FF)) + ((x >> 8) & DUP_64(0x00FF00FF));
    x = (x & DUP_64(0x0000FFFF)) + ((x >>16) & DUP_64(0x0000FFFF));
    x = (x & DUP_64(0x000000FF)) + ((x >>32) & DUP_64(0x000000FF));
#else
    x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x & 0x0F0F0F0F) + ((x >> 4) & 0x0F0F0F0F);
    x = (x & 0x00FF00FF) + ((x >> 8) & 0x00FF00FF);
    x = (x & 0x0000FFFF) + ((x >>16) & 0x000000FF);
#endif
    return (uint) x;
    }


/* use the CRC value as quick ID to help identify/verify rotation sets */
void Set_CRC(rSearchRec *r)
    {
#define CRC_FDBK ((0x04C11DB7u >> 1) ^ 0x80000000u) /* CRC-32-IEEE-802.3 (from Wikipedia) */
    uint i,h=~0u;

    for (i=0;i<rotsPerCycle;i++)
        {
        h ^= r->rotList[i];
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);

        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        h = (h & 1) ? (h >> 1) ^ CRC_FDBK : (h >> 1);
        }
    r->CRC = h;
    }

/* qsort routine for search records: keep in descending order */
int Compare_SearchRec_Descending(const void *aPtr,const void *bPtr)
    {
    uint wA = ((const rSearchRec *) aPtr)->rWorst;
    uint wB = ((const rSearchRec *) bPtr)->rWorst;

    if (wA < wB)
        return +1;
    if (wA > wB)
        return -1;
    else
        {   /* equal metric. Sort by ID number */
        wA = ((const rSearchRec *) aPtr)->ID;
        wB = ((const rSearchRec *) bPtr)->ID;
        if (wA < wB)
            return -1;
        if (wA > wB)
            return +1;
        return  0;
        }
    }

const char *ASCII_TimeDate(void)
    {
    time_t t;
    time(&t);   
    return ctime(&t);
    }

/* test the rotation set for minimum hamming weight >= minHW */
/*   [try to do it fast: rely on rotational symmetry using OR, */
/*    and do an early exit if hamming weight is too low] */
int Cycle_Min_HW(uint rounds, const u08b *rotList,uint minHW,uint verMask)
    {
    uint    i,j,v,hw,hMin;
    u08b    rots[MAX_ROTS_PER_CYCLE];
    Block   b;

    hMin = BITS_PER_WORD;
    for (v=0;v<MAX_ROT_VER_CNT;v++)
        {
        if ((verMask & (1 << v)) == 0)
            continue;
        if (v & 1)
            { /* do it on the "half-cycle" */
            for (i=0;i<rotsPerCycle;i++)
                {
                rots[i] = rotList[(i >= rotsPerCycle/2) ? i - rotsPerCycle/2 : i + rotsPerCycle/2];
                }
            }
        else
            memcpy(rots,rotList,rotsPerCycle*sizeof(rots[0]));
        for (i=0;i<wordsPerBlock;i++)
            {
            memset(b.x,0,wordsPerBlock*sizeof(b.x[0]));
            b.x[i] = 1;                     /* test propagation into one word */
            if (minHW)
                {       /* use the "_rN" versions for speed */
                if (v & 2)
                    rev_cycle_or_rN(b.x,rots,(int)rounds);
                else
                    fwd_cycle_or_rN(b.x,rots,(int)rounds);
                }
            else
                {       /* saturation check */
                if (v & 2)
                    rev_cycle_or   (b.x,rots,(int)rounds);
                else
                    fwd_cycle_or   (b.x,rots,(int)rounds);
                }
            for (j=0;j<wordsPerBlock;j++)
                {
                hw = HammingWeight(b.x[j]);
                if (minHW > hw)
                    return 0;               /* stop if this isn't good enough */
                if (hMin  > hw)             /* else keep track of min */
                    hMin  = hw;
                }
            }
        }
    return hMin;
    }

/* compute/set the minimum hamming weight of the rotation set */
/*   [more thorough check than Cycle_Min_HW] */
uint Set_Min_hw_OR(rSearchRec *r,uint verMask,uint rounds)
    { 
    uint  i,j,v,hw,hwMin;
    u08b  rots[MAX_ROTS_PER_CYCLE];
    Block b;

    Set_CRC(r);
    hwMin = BITS_PER_WORD;
    for (v=0;v<MAX_ROT_VER_CNT;v++)
        {
        r->hw_OR[v] = BITS_PER_WORD;
        if ((verMask & (1 << v)) == 0)
            continue;
        if (v & 1)
            { /* do it on the "half-cycle" */
            for (i=0;i<rotsPerCycle;i++)
                {
                rots[i] = r->rotList[(i >= rotsPerCycle/2) ? i - rotsPerCycle/2 : i + rotsPerCycle/2];
                }
            }
        else
            memcpy(rots,r->rotList,rotsPerCycle*sizeof(rots[0]));
        for (i=0;i<bitsPerBlock;i+=BITS_PER_WORD)
            {
            memset(b.x,0,sizeof(b.x));
            b.x[i/BITS_PER_WORD] |= (((u64b) 1) << (i%BITS_PER_WORD));
            if (v & 2)
                rev_cycle_or(b.x,rots,(int) rounds);
            else
                fwd_cycle_or(b.x,rots,(int) rounds);
            for (j=0;j<wordsPerBlock;j++)
                {
                hw = HammingWeight(b.x[j]);
                if (hwMin > hw)
                    hwMin = hw;
                if (r->hw_OR[v] > (u08b) hw)
                    r->hw_OR[v] = (u08b) hw;
                }
            }
        }
    return hwMin;
    }

/* show how the Hamming weight varies as a function of # rounds */
void Show_HW_rounds(const u08b *rotates)
    {
    uint i,r,minHW,hw[4];

    for (r=4;r<12;r++)
        {  
        minHW = bitsPerBlock;
        for (i=0;i<4;i++)
            {
            hw[i]=Cycle_Min_HW(r,rotates,0,1 << i);
            if (minHW > hw[i])
                minHW = hw[i];
            }
        printf("%2d rounds: minHW = %2d  [",r,minHW);
        for (i=0;i<4;i++)   /* show the different "versions" */
            printf(" %2d",hw[i]);
        printf(" ]\n");
        }
    }

/* read rotations value from file */
const u08b *get_rotation_file(const char *rfName)
    {
    enum   { MAX_LINE = 512 };
    char   line[MAX_LINE+4];
    uint   i,rotVal;
    uint   rotShow=0;
    static FILE *rf=NULL;
    static u08b rotates[MAX_ROTS_PER_CYCLE];
    static uint rotCnt =0;
/**** sample format: 
+++++++++++++ Preliminary results: sampleCnt =  1024, block =  256 bits
rMin = 0.425. #079C[*21] [CRC=D89E7C72. hw_OR=62. cnt= 1024. blkSize= 256]          
   46   52
   21   38
   13   13
   20   27
   14   40
   43   26
   35   29
   19   63
rMin = 0.425. #0646[*17] [CRC=527174F3. hw_OR=61. cnt= 1024. blkSize= 256]          
   26   24
   50   48
   40   25
   36   55
   10   20
   10   16
   60   55
   18    7
...
****/
    if (rfName[0] == '+')
        {
        rfName++;
        rotShow = 1;
        }
    if (rf == NULL)
        {
        rf = fopen(rfName,"rt");
        if (rf == NULL)
            {
            printf("Unable to open rotation file '%s'",rfName);
            exit(2);
            }
        rotCnt=0;
        for (;;)        /* skip to "preliminary results" section */
            {
            line[0]=0;
            if (fgets(line,sizeof(line)-4,rf) == NULL || line[0] == 0)
                {
                fclose(rf);                 /* eof --> stop */
                rf = NULL;
                return NULL;
                }
            /* check for the header */
            if (line[0] != '+' || line[1] != '+' || line[2] != '+' ||
                strstr(line,"reliminary results:") == NULL)
                continue;
            /* now check for the correct block size */
            for (i=strlen(line);i;i--)      /* start at eol and look backwards */
                if (line[i-1] == '=')       /* check for '=' sign for block size */
                    break;
            if (i > 0 && sscanf(line+i,"%u bits",&i) == 1 && i == bitsPerBlock)
                break;
            }
        }
    /* now at the rMin line */
    line[0]=0;
    if (fgets(line,sizeof(line)-4,rf) == NULL || line[0] == 0 || strncmp(line,"rMin =",6))  
        {
        fclose(rf);
        rf = NULL;
        return NULL;
        }

    /* now read in all the rotation values */
    for (i=0;i<rotsPerCycle;i++)
        {
        if (fscanf(rf,"%u",&rotVal) != 1 || rotVal >= bitsPerBlock)
            {   /* Invalid rotation value */
            fclose(rf);
            rf = NULL;
            return NULL;
            }
        rotates[i] = (u08b) rotVal;
        }
    if (fgets(line,sizeof(line)-4,rf) == NULL)          /* skip eol */
        {
        fclose(rf);
        rf = NULL;
        }
    if (rotShow)
        {   /* show the hamming weight profile */
        printf("\n:::::::::::\n");
        printf("Rot #%02d [%4d-bit blocks] read from file '%s':\n",rotCnt,bitsPerBlock,rfName);
        for (i=0;i<rotsPerCycle;i++)
            printf("%4d%s",rotates[i],((i+1)%(wordsPerBlock/2))?"":"\n");
        Show_HW_rounds(rotates);     /* show HW results for different numbers of rounds */
        printf(":::::::::::\n");
        }
    rotCnt++;
    return rotates;
    }

/* generate a randomly chosen set of rotation constants of given minimum hamming weight (using OR) */
/* (this may take a while, depending on minHW,rounds) */
uint get_rotation(rSearchRec *r,testParms t)
    {
    static  u64b rCnt    = 1;
    static  u64b rCntOK  = 0;
    static  uint rScale  = BITS_PER_WORD;
    static  uint hwBase  = 0;
    static  uint rID     = 1;
    uint    i,j,k,m,n,b,hw,q,qMask;
    static  u08b rotates[MAX_ROTS_PER_CYCLE];   /*  last generated rotation set */
    u08b    goodRots[BITS_PER_WORD];
    uint    goodRotCnt;
    
    r->rWorst       =  0;
    r->parentCRC    = ~0u;

    if (rotFileName)                            /* get from search results file? */
        {
        const u08b *rf = get_rotation_file(rotFileName);
        if (rf)
            {
            for (i=0;i<rotsPerCycle;i++)
                r->rotList[i] = rf[i];
            Set_Min_hw_OR(r,t.rotVerMask,t.rounds);
            r->ID = rID++;
            return 1;
            }
        /* here with file exhausted. Keep going with randomized values */
        rotFileName = NULL;                     /* don't use file any more */
        return 0;
        }
    for (i=goodRotCnt=0;i<BITS_PER_WORD;i++)
        if (!RotCnt_Bad(i))
            {
            goodRots[goodRotCnt++] = (u08b) i;
            }
    
    qMask   = ((wordsPerBlock/2)-1) & t.dupRotMask;     /* filter for dup rotate counts in the same round? */
    for (;;rCnt++)
        {
        if (hwBase == 0)
            {   /* pick a rotation set at random */
            for (i=0;i<rotsPerCycle;)
                {
                rotates[i] = goodRots[Rand32() % goodRotCnt];
                /* filter out unapproved rotation sets here */
                for (q=i & ~qMask;q < i;q++)    /* check for dups in the same round */
                    if (rotates[i] == rotates[q])
                        break;
                if (q >= i)                     /* no dup, value ok, so this value is ok */
                    i++;
                }
            hw = Cycle_Min_HW(t.rounds,rotates,t.minHW_or-t.minOffs,t.rotVerMask);
            if (hw == 0)                /* did we get close? */
                continue;
            rCntOK++;

            hwBase = hw;
            if (hw >= t.minHW_or)
                if (Cycle_Min_HW(t.maxSatRnds, rotates,0,t.rotVerMask) == BITS_PER_WORD)
                    {
                    for (i=0;i<rotsPerCycle;i++)
                        r->rotList[i] = rotates[i];
                    rScale = 1;         /* set up for scaling below */
                    }
            }
        /* use odd scaling for randomly generated rotations */
        for (;rScale < BITS_PER_WORD;)
            {
            for (i=0;i<rotsPerCycle;i++)
                {
                r->rotList[i] = (rotates[i] * rScale) % BITS_PER_WORD;
                if (RotCnt_Bad(r->rotList[i]))
                    break;
                }
            rScale+=2;                  /* bump scale factor for next time */
            if (i >= rotsPerCycle)
                {   /* all values ok: this one's a keeper */
                Set_Min_hw_OR(r,t.rotVerMask,t.rounds);
                r->ID = rID++;
                return 1;
                }
            }
        /* Try nearby values to see if hw gets better: monotonic hill climb. */
        /*      -- exhaustively try all possible values of pairs of changes  */
        for (m=0;m<rotsPerCycle;m++)
        for (b=0;b<BITS_PER_WORD ;b++)
            {
            k = rotsPerCycle-1-m;           /* work backwards, since we're already close */
            rotates[k]++;
            rotates[k] &= (BITS_PER_WORD-1);
            if (RotCnt_Bad(rotates[k]))
                continue;
            for (q=k | qMask;q > k;q--)    /* check for dups in the same round */
                if (rotates[k] == rotates[q])
                    break;
            if (q > k)      
                continue;
            for (i=m+1;i<rotsPerCycle;i++)
                {
                n = rotsPerCycle-1-i;   /* work backwards */
                for (j=0;j<BITS_PER_WORD;j++)
                    {
                    rotates[n]++;       /* try another rotation value */
                    rotates[n] &= (BITS_PER_WORD-1);
                    if (RotCnt_Bad(rotates[n]))
                        continue;
                    for (q=n | qMask;q > n;q--)    /* check for dups in the same round */
                        if (rotates[n] == rotates[q])
                            break;
                    if (q > n)      
                        continue;  
                    k  = (t.minHW_or > hwBase) ? t.minHW_or : hwBase;
                    hw = Cycle_Min_HW(t.rounds,rotates,k,t.rotVerMask);
                    if (hw > hwBase)
                        if (Cycle_Min_HW(t.maxSatRnds, rotates,0,t.rotVerMask) == BITS_PER_WORD)
                            {   /* must improve hw to accept this new rotation set */
                            assert(hw >= t.minHW_or);
                            hwBase = hw;
                            rScale = 3; /* set up for scaling next time */
                            for (i=0;i<rotsPerCycle;i++)
                                r->rotList[i] = rotates[i];
                            Set_Min_hw_OR(r,t.rotVerMask,t.rounds);
                            r->ID = rID++;
                            return 1;
                            }
                    }
                }
            }
        hwBase = 0;                     /* back to random  */
        }
    }

/* display a search record result */
void ShowSearchRec(FILE *f,const rSearchRec *r,testParms t,uint showMode,char markCh,uint showNum)
    {
    uint  i,j,n,hwMin;
    const char *s;
    char  fStr[200];

    hwMin=BITS_PER_WORD;
    for (i=0;i<MAX_ROT_VER_CNT;i++)
        if (hwMin > (uint) r->hw_OR[i])
            hwMin = (uint) r->hw_OR[i];

    switch (showMode)
        {
        case SHOW_ROTS_FINAL:  sprintf(fStr,".final:%02d " ,showNum); s = fStr; break;
        case SHOW_ROTS_H:      s = ".format";  break;
        case SHOW_ROTS_PRELIM: s = ".prelim";  break;
        default:               s = "";         break;
        }

    fprintf(f,"rMin = %5.3f.%c [CRC=%08X. parent=%08X. ID=%08X. hw_OR=%2d. cnt=%5d. bits=%4u]%-10s%s%s\n",
            r->rWorst/(double)t.sampleCnt,markCh,r->CRC,r->parentCRC,r->ID,
            hwMin,t.sampleCnt,bitsPerBlock,s,
            (t.tstFlags & TST_FLG_USE_ABS)?" useAbs":"",(r->ID & ID_RECALC_BIT)?" recalc":""
           );

    switch (showMode)
        {
        case SHOW_NONE:
            break;
        case SHOW_ROTS_H: /* format for "skein.h" */
            for (j=n=0;j<rotsPerCycle/(wordsPerBlock/2);j++)
                {
                fprintf(f,"   ");
                for (i=0;i<wordsPerBlock/2;i++)
                    {
                    fprintf(f,(wordsPerBlock == 16)?" R%04d":" R_%03d",wordsPerBlock*64);
                    fprintf(f,"_%d_%d=%2d,",j,i,r->rotList[n++]);
                    }
                fprintf(f,"\n");
                }
            break;
        default:
            for (i=0;i<rotsPerCycle;i++)
                fprintf(f,"   %2d%s",r->rotList[i],((i+1)%(wordsPerBlock/2))?"":"\n");
            break;
        }
    }

/* compute Skein differentials for a given rotation set */
uint CheckDifferentials(rSearchRec *r,testParms t)
    {
    enum  { HIST_BINS =  20 };

    uint    i,j,k,v,n,d,dMax,minCnt,maxCnt,vCnt,q;
    uint    rMin,rMax,hwMin,hwMax,hw,rMinCnt,rMaxCnt,iMin,jMin,iMax,jMax;
    uint    hist[HIST_BINS+1];
    u08b    rots[MAX_ROTS_PER_CYCLE];
    u64b    totSum,w,y,z,oMask;
    double  fSum,fSqr,x,var,denom;
    static  u64b onesCnt[3][MAX_BITS_PER_BLK][MAX_BITS_PER_BLK/8]; /* pack eight 8-bit counts into each u64b (for speed) */
    u64b   *oPtr;
    struct
        {
        Block pt,ct;
        } a,b;

    r->rWorst = t.sampleCnt;
    dMax = 1u << (t.diffBits & (BITS_PER_WORD-1));
    iMin = jMin = iMax = jMax = bitsPerBlock + 1;

    for (v=vCnt=0;v < MAX_ROT_VER_CNT; v++)  
        { /* different versions of rotation schedule, including "inverse" cipher */
        if ((t.rotVerMask & (1 << v)) == 0)
            continue;
        vCnt++;     /* number of versions processed */
        if (v & 1)
            { /* do it on the "half-cycle" */
            for (i=0;i<rotsPerCycle;i++)
                {
                rots[i] = r->rotList[(i >= rotsPerCycle/2) ? i - rotsPerCycle/2 : i + rotsPerCycle/2];
                }
            }
        else
            memcpy(rots,r->rotList,rotsPerCycle*sizeof(rots[0]));
        for (d=1; d < dMax; d+=2)    /* multi-bit difference patterns (must start with a '1' bit)  */
            {
            hwMax=0;
            hwMin=bitsPerBlock+1;
            memset(onesCnt,0,sizeof(onesCnt));      /* clear stats before starting */
                
            oMask = DUP_64(0x01010101);             /* mask for adding, 8 bins at a time */
            for (n=1;n<=t.sampleCnt;n++)
                {
                for (i=0;i<wordsPerBlock;i++)       /* generate input blocks in a portable way */
                    a.pt.x[i] = Rand64();
                a.ct = a.pt;
                if (v & 2)
                    rev_cycle(a.ct.x,rots,t.rounds);
                else
                    fwd_cycle(a.ct.x,rots,t.rounds);
                for (i=0;i<bitsPerBlock;i++)
                    {
                    b.pt = a.pt;
                    b.pt.x[i/BITS_PER_WORD] ^= left_rot((u64b)d,(i%BITS_PER_WORD));  /* inject input difference  */
                    b.ct = b.pt;
                    if (t.tstFlags & TST_FLG_DO_RAND)
                        RandBytes(b.ct.x,sizeof(b.ct.x));       /* random results as a comparison point */
                    else if (v & 2)
                        rev_cycle(b.ct.x,rots,t.rounds);        /* let Skein do the mixing */
                    else
                        fwd_cycle(b.ct.x,rots,t.rounds);        /* let Skein do the mixing */
                    z  = 0;                                     /* accumulate total hamming weight in z */
                    oPtr = onesCnt[0][i];
                    for (j=0;j<wordsPerBlock;j++)
                        {                                       /* inner-most loop: unroll it fully */
                        w = b.ct.x[j] ^ a.ct.x[j];              /* xor difference in each ciphertext word */
                        y = (w     ) & oMask; oPtr[0] += y; z += y;   /* sum 8 bins at a time (bits 0,8,16,24...,56) */
                        y = (w >> 1) & oMask; oPtr[1] += y; z += y;
                        y = (w >> 2) & oMask; oPtr[2] += y; z += y;   /* do it 8 times to cover all bits in w */
                        y = (w >> 3) & oMask; oPtr[3] += y; z += y;
                                                                                    
                        y = (w >> 4) & oMask; oPtr[4] += y; z += y;
                        y = (w >> 5) & oMask; oPtr[5] += y; z += y;
                        y = (w >> 6) & oMask; oPtr[6] += y; z += y;
                        y = (w >> 7) & oMask; oPtr[7] += y; z += y;
                        oPtr += 8;
                        }
                    /* sum up the total hamming weight bins (very carefully) */
                    z = (z & DUP_64(0x00FF00FF)) + ((z >> 8) & DUP_64(0x00FF00FF));
                    hw  = (uint) (z + (z >> 16) + (z >> 32) + (z >> 48)) & 0xFFFF;
                    if (hwMin > hw) hwMin = hw;                 /* update total hw min/max stats */
                    if (hwMax < hw) hwMax = hw;
                    }
                if ((n & 0x7F) == 0)
                    {   /* prevent onesCnt[0] overflow by "transferring" MSBs of 8-bit bytes into onesCnt[1] */
                    for (i=0;i<bitsPerBlock  ;i++)
                    for (j=0;j<bitsPerBlock/8;j++)
                        {   /* add the MSB (bit 7) of each byte into onesCnt[1], then mask it off in onesCnt[0] */
                        onesCnt[1][i][j] += (onesCnt[0][i][j] >> 7) & oMask;
                        onesCnt[0][i][j] &= ~(oMask << 7);
                        }
                    if ((n & 0x3FFF) == 0)
                        {   /* propagate overflow into onesCnt[2] (occasionally, as needed) */
                        for (i=0;i<bitsPerBlock  ;i++)
                        for (j=0;j<bitsPerBlock/8;j++)
                            { 
                            onesCnt[2][i][j] += (onesCnt[1][i][j] >> 7) & oMask;
                            onesCnt[1][i][j] &= ~(oMask << 7);
                            }
                        }
                    }
                if (n == 32 && d == 1 && (t.tstFlags & TST_FLG_QUICK_EXIT))
                    {   /* quick exit if not even close to random looking after a few samples */
                    for (i=0;i<bitsPerBlock  ;i++)
                    for (j=0;j<bitsPerBlock/8;j++)
                        {
                        if ((onesCnt[0][i][j] & ~oMask) == 0)  /* any count less than 2? */
                            {
                            /** Since an ideal random function has prob=0.5 each for input/output bit 
                             ** pair, the expected distribution of onesCnt[i][j] is binomial. 
                             ** Thus, at this point, the probability of onesCnt[i][j] < 2 is:
                             **     ((1+32)/2)/(2**-32)
                             ** This probability is roughly 2**(-27), so when we observe such an
                             ** occurrence, we exit immediately to save taking a lot of stats just
                             ** to fail later. This filter significantly speeds up the search, at a
                             ** very low probability of improperly dismissing a "good" rotation set.
                             **/
                            if (t.tstFlags & TST_FLG_SHOW && vCnt > 1)
                                {   /* show why we stopped, if we already showed something */
                                printf("%23s/* quick exit: %d/%d */\n","",(uint)onesCnt[0][i][j],n);
                                }
                            return r->rWorst = 0;   /* not a good result */
                            }
                        }
                    }
                }
            /* now process the stats from the samples we just generated */
            assert(t.sampleCnt < (1 << 22));            /* 2**22 is big enough not to worry! */
            memset(hist,0,sizeof(hist));
            fSum  = fSqr = 0.0;
            denom = 1.0 / (double) t.sampleCnt;
            rMin  = minCnt = ~0u;
            totSum= rMax = rMinCnt = rMaxCnt = maxCnt = 0;
            for (i=0;i<bitsPerBlock;i++)
                {
                for (j=0;j<bitsPerBlock/8;j++)
                    {
                    w = onesCnt[0][i][j];               /* 7+ bits here */
                    y = onesCnt[1][i][j];               /* 7+ bits here */
                    z = onesCnt[2][i][j];               /* 8  bits here.  Total = 22 bits */
                    for (k=0;k<8;k++,w >>= 8,y >>= 8,z >>= 8)
                        {
                        q = (uint) ((w & 0xFF) + ((y & 0xFF) << 7) + ((z & 0xFF) << 14));
                        if (maxCnt < q) { maxCnt = q; iMax = i; jMax = j; if (rMax < q) { rMax = q; rMaxCnt = 0; } }
                        if (minCnt > q) { minCnt = q; iMin = i; jMin = j; if (rMin > q) { rMin = q; rMinCnt = 0; } }
                        if (rMin == minCnt) rMinCnt++;
                        if (rMax == maxCnt) rMaxCnt++;
                        if (t.tstFlags & TST_FLG_SHOW)
                            {   /* compute more extensive stats only if showing results below */
                            totSum  += q;
                            x        = q*denom;                 /* update stats for stdDev  */
                            fSum    += x;
                            fSqr    += x*x;
                            hist[(uint)floor(x*HIST_BINS)]++;   /* track histogram  */
                            }
                        }
                    }
                }
            if (t.tstFlags & TST_FLG_USE_ABS && rMin > t.sampleCnt - rMax)
                {
                rMin = t.sampleCnt - rMax;                      /* use max variation from 1/2 */
                iMin = iMax;
                jMin = jMax;
                }
            if (r->rWorst > rMin)
                {
                r->rWorst = rMin;
                if (rMin == 0)
                    {  /* if far worse than current best, stop now (to speed up the search) */
                    if (t.tstFlags & TST_FLG_SHOW && (d > 1 || vCnt > 1)) /* show why we stopped, if we already showed something */
                        printf("%23s/* early exit */\n","");
                    return r->rWorst = 0;
                    }
                }
            if (t.tstFlags & TST_FLG_SHOW)
                {         /* show some detailed results of the test */
                if (d == 1)
                    {     /* put out the rotation info the first time thru */
                    if ((t.tstFlags & TST_FLG_DO_RAND) == 0)
                        {
                        printf("Rotation set [CRC=%08X. hw_OR=%2d. sampleCnt=%5d. block=%4d bits. v=%d]:\n",
                               r->CRC,r->hw_OR[v],t.sampleCnt,bitsPerBlock,v);
                        if (vCnt == 0)
                            for (i=0;i<rotsPerCycle;i++)
                                printf("   %2d%s",r->rotList[i],((i+1)%(wordsPerBlock/2))?"":"\n");
                        }
                    }
                printf("rnds=%2d,cnt=%5d",t.rounds,t.sampleCnt);
                x  =  fSum/(bitsPerBlock*bitsPerBlock);
                var= (fSqr/(bitsPerBlock*bitsPerBlock)) - x*x;
                printf(" min=%5.3f.[%c] max=%5.3f.[%c]  hw=%3d..%3d.  avg=%7.5f. std=%6.4f. d=%X. [%3d,%3d]",
                       rMin*denom,(rMinCnt > 9) ? '+' : '0'+rMinCnt,
                       rMax*denom,(rMaxCnt > 9) ? '+' : '0'+rMaxCnt,
                       hwMin,hwMax,
                       (totSum*denom)/(bitsPerBlock*bitsPerBlock),sqrt(var),(uint)d,iMin,jMin);
                if (t.tstFlags & TST_FLG_SHOW_HIST)
                    { /* very wide histogram display */
                    for (i=0;i<=HIST_BINS;i++)
                        if (hist[i])
                            printf(" %7.5f",hist[i]/(double)(bitsPerBlock*bitsPerBlock));
                        else
                            printf("  _     ");
                    }
                if (t.tstFlags & TST_FLG_DO_RAND)
                    printf(" [RANDOM] ");
                printf("\n");
                fflush(stdout);
                }
            if (t.tstFlags & TST_FLG_DO_RAND)
                break;        /* no need to do more than one random setting per rotation set */
            }   /* for (d=1;d<dMax;d+=2) */
        if (t.tstFlags & TST_FLG_DO_RAND)
            break;        /* no need to do more than one random setting per rotation set */
        }
    return r->rWorst;
    }

/* twiddle a bit with an entry, but keep maxSatRounds satisfied */
void Twiddle(rSearchRec *r,testParms t)
    {
    enum { MAX_TWIDDLE_CNT = 100, MAX_ROT_CNT = 6 };
    uint i,j,k,n,v[MAX_ROT_CNT];
    u08b old[MAX_ROT_CNT];
    u64b usedBitmap;
    u08b goodRots[BITS_PER_WORD];
    uint goodRotCnt;

    assert(rotsPerCycle <= sizeof(usedBitmap)*8);
    r->ID += (1 << TWIDDLE_CNT_BIT0);           /* bump count of number of times twiddled */
    r->ID &= ~ID_RECALC_BIT;                    /* show this one hasn't been had recalc yet */
    r->parentCRC = r->CRC;                      /* track genealogy */

    for (i=goodRotCnt=0;i<BITS_PER_WORD;i++)
        if (!RotCnt_Bad(i))
            {
            goodRots[goodRotCnt++] = (u08b) i;
            }

    n = 1 + (Rand08() % MAX_ROT_CNT);
    for (i=0;i<4;i++)
        {
        usedBitmap = 0; 
        for (j=0;j<n;j++)
            {               /* pick which set of n rotation constants to change */
            do  {
                v[j] = Rand08() % rotsPerCycle; /* rotation index */
                }
            while ((usedBitmap >> v[j]) & 1);   /* make sure all v[j] values are unique */
            usedBitmap |= (((u64b) 1) << v[j]);
            old[j] = r->rotList[v[j]];          /* save current value */
            }
        for (k=0;k<MAX_TWIDDLE_CNT/4;k++)
            {  /* here with n rotation indices (v[0..n-1]) to be changed */
            for (j=0;j<n;j++)
                {
                do  {
                    r->rotList[v[j]] = goodRots[Rand32() % goodRotCnt];
                    }   /* make sure new rotation value changes */
                while (r->rotList[v[j]] == old[j]);
                }
            if (Cycle_Min_HW(t.maxSatRnds,r->rotList,0,t.rotVerMask) == BITS_PER_WORD)
                {
                if (i >= 2 || !(t.tstFlags & TST_FLG_KEEP_MIN_HW) ||
                    Cycle_Min_HW(t.rounds,r->rotList,t.minHW_or,t.rotVerMask) >= (int) t.minHW_or)
                    {
                    Set_Min_hw_OR(r,t.rotVerMask,t.rounds);
                    return;
                    }
                }
            for (j=0;j<n;j++)   /* didn't work: go back to the old values */
                r->rotList[v[j]] = old[j];
            }
        }
    /* twiddling failed to produce a valid set (very rare). Select a brand new one */
    get_rotation(r,t);
    }

/* run a full search */
void RunSearch(testParms t)
    {
    enum        { KEEP_DIV = 16, KEEP_REP = 10, SHOW_CNT = 8 };
    rSearchRec  popList[MAX_POP_CNT+2];
    uint        i,j,k,n,repCnt,genCnt,keepCnt,prevBest[SHOW_CNT],showMask;
    const       char *timeStr;
    time_t      t0,t1;

    Rand_Init(t.seed0 + (((u64b) bitsPerBlock) << 32));
    memset(prevBest,0,sizeof(prevBest));

    /* now set up the globals according to selected Skein blocksize */
    switch (bitsPerBlock)
        {
        case  256:
            t.genCntMax      = (t.genCntMax) ? t.genCntMax    : DEFAULT_GEN_CNT_4  ;
            t.rounds         = (t.rounds)    ? t.rounds       : DEFAULT_ROUND_CNT_4;
            t.minHW_or       = (t.minHW_or)  ? t.minHW_or     :         MIN_HW_OR_4;
            t.maxSatRnds     = (t.maxSatRnds)? t.maxSatRnds   :    MAX_SAT_ROUNDS_4;
            fwd_cycle_or_rN  = (t.rounds!=8) ? fwd_cycle_4_or :  fwd_cycle_4_or_r8 ;
            rev_cycle_or_rN  = (t.rounds!=8) ? rev_cycle_4_or :  rev_cycle_4_or_r8 ;
            fwd_cycle_or     = fwd_cycle_4_or;
            rev_cycle_or     = fwd_cycle_4_or;
            fwd_cycle        = fwd_cycle_4;
            rev_cycle        = rev_cycle_4;
            showMask         = 7;
            break;
        case  512:
            t.genCntMax      = (t.genCntMax) ? t.genCntMax    : DEFAULT_GEN_CNT_8  ;
            t.rounds         = (t.rounds)    ? t.rounds       : DEFAULT_ROUND_CNT_8;
            t.minHW_or       = (t.minHW_or)  ? t.minHW_or     :         MIN_HW_OR_8;
            t.maxSatRnds     = (t.maxSatRnds)? t.maxSatRnds   :    MAX_SAT_ROUNDS_8;
            fwd_cycle_or_rN  = (t.rounds!=8) ? fwd_cycle_8_or :  fwd_cycle_8_or_r8 ;
            rev_cycle_or_rN  = (t.rounds!=8) ? rev_cycle_8_or :  rev_cycle_8_or_r8 ;
            fwd_cycle_or     = fwd_cycle_8_or;
            rev_cycle_or     = rev_cycle_8_or;
            fwd_cycle        = fwd_cycle_8;
            rev_cycle        = rev_cycle_8;
            showMask         = 3;
            break;
        case 1024:
            t.genCntMax      = (t.genCntMax) ? t.genCntMax    : DEFAULT_GEN_CNT_16  ;
            t.rounds         = (t.rounds)    ? t.rounds       : DEFAULT_ROUND_CNT_16;
            t.minHW_or       = (t.minHW_or)  ? t.minHW_or     :         MIN_HW_OR_16;
            t.maxSatRnds     = (t.maxSatRnds)? t.maxSatRnds   :    MAX_SAT_ROUNDS_16;
            fwd_cycle_or_rN  = (t.rounds!=9) ? fwd_cycle_16_or: fwd_cycle_16_or_r9  ;
            rev_cycle_or_rN  = (t.rounds!=9) ? rev_cycle_16_or: rev_cycle_16_or_r9  ;
            fwd_cycle_or     = fwd_cycle_16_or;
            rev_cycle_or     = rev_cycle_16_or;
            fwd_cycle        = fwd_cycle_16;
            rev_cycle        = rev_cycle_16;
            showMask         = 1;
            break;
        default:
            printf("Invalid block size!");
            exit(2);
        }
    if (t.popCnt > MAX_POP_CNT)
        t.popCnt = MAX_POP_CNT;
    if (t.popCnt < MIN_POP_CNT)
        t.popCnt = MIN_POP_CNT;
    wordsPerBlock =   bitsPerBlock /      BITS_PER_WORD;
    rotsPerCycle  = (wordsPerBlock / 2) * ROUNDS_PER_CYCLE;

    keepCnt = t.popCnt/KEEP_DIV;
    assert(keepCnt*(1+KEEP_REP) <= t.popCnt);
    
    printf("******************************************************************\n");
    printf("Random seed = %u. BlockSize =%4d bits. sampleCnt =%6d. rounds = %2d. minHW_or=%d. CPU = %d-bit\n",
                       t.seed0,bitsPerBlock,t.sampleCnt,t.rounds,t.minHW_or,(uint)sizeof(size_t)*8);
    printf("Population  = %d. keepCnt = %d. repCnt = %d. rest = %d. keepMinHW = %d\n",
            t.popCnt,keepCnt,KEEP_REP,t.popCnt-keepCnt*(1+KEEP_REP),(t.tstFlags & TST_FLG_KEEP_MIN_HW)?1:0); 
    timeStr = ASCII_TimeDate();
    if (t.tstFlags & TST_FLG_STDERR)
        {
        fprintf(stderr,"Start: %sBlock size = %d bits. popCnt = %d. sampleCnt = %d. keepMinHW = %d",
                        timeStr,bitsPerBlock,t.popCnt,t.sampleCnt,(t.tstFlags & TST_FLG_KEEP_MIN_HW)?1:0);
        if (t.runHours)
            fprintf(stderr,". run time = %d hours",t.runHours);
        fprintf(stderr,"\n");
        }
    else
        showMask = 0;
    printf("Start: %s  \n",timeStr);
    time(&t0);
    fflush(stdout);

    for (n=0;n<t.popCnt;n++)
        {   /* initialize the population with rotations that have "reasonable" hw_OR */
        if (t.tstFlags & TST_FLG_STDERR)
            fprintf(stderr,"\rGetRot: %04X    \r",t.popCnt-n);
        if (get_rotation(&popList[n],t) == 0)
            t.popCnt = n;               /* stop after end of file read in */
        }
    if (t.tstFlags & TST_FLG_STDERR)
        fprintf(stderr,"\r%25s\r","");

    for (genCnt=0;genCnt < t.genCntMax;genCnt++)
        {   /* advance to the next generation */
        for (i=0;i<t.popCnt;i++)
            {   /* generate stats for all entries (this loop is where all the time is spent!) */
            if ((i & showMask) == 1)
                fprintf(stderr,"#%04X \r",t.popCnt-i);
            if (genCnt == 0 || i >= keepCnt)
                {
                CheckDifferentials(&popList[i],t);
                }
            else if (i <= keepCnt/2 && (popList[i].ID & ID_RECALC_BIT) == 0)
                {   /* recalc with bigger sampleCnt for better accuracy */
                t.sampleCnt <<= 2;
                CheckDifferentials(&popList[i],t);
                t.sampleCnt >>= 2;
                popList[i].rWorst = (popList[i].rWorst + 2) / 4;
                popList[i].ID |= ID_RECALC_BIT;
                }
            }
        qsort(popList,t.popCnt,sizeof(popList[0]),Compare_SearchRec_Descending);
        if (t.genCntMax == 1)
            { keepCnt = t.popCnt; break; }  /* allow quick processing from file */
        /* now update the population for the next generation */
        n = t.popCnt-1;                 /* start discarding at the end of the list */
        for (i=0;i<keepCnt;i++)
            {
            if (t.tstFlags & TST_FLG_WEIGHT_REP)
                repCnt = (i < keepCnt/2) ? KEEP_REP+2 : KEEP_REP-2 ;
            else
                repCnt = KEEP_REP;
            for (j=0;j<repCnt;j++,n--)
                {                       /* replicate the best ones, replacing the worst ones */
                popList[n] = popList[i];
                if (j == 0)
                    {   /* splice two together, but only if they are from the same initial rotation set */
                    k = Rand32() %  keepCnt;    
                    if (((popList[n].ID ^ popList[k].ID) & ID_NUM_MASK) == 0)
                        memcpy(popList[n].rotList,
                               popList[k].rotList,
                               rotsPerCycle*sizeof(popList[n].rotList[0])/2);
                    }
                Twiddle(&popList[n],t); /* tweak the replicate entry a bit */
                assert(n >= keepCnt);   /* sanity check  */
                }
            }
        for (;n>=keepCnt;n--)           /* just tweak the rest */
            {
            Twiddle(&popList[n],t);
            }
        time(&t1);
        /* show current best */
        if (t.tstFlags & TST_FLG_STDERR)
            {   /* first to stderr (assuming redirected stdout */
            fprintf(stderr,"\r%4d: ",genCnt+1);
            for (i=j=0;i<SHOW_CNT;i++)
                {
                fprintf(stderr," %5.3f%c",popList[i].rWorst/(double)t.sampleCnt,(popList[i].ID & ID_RECALC_BIT)?'r':' ');
                j |= (popList[i].rWorst ^ prevBest[i]); /* track changes */
                prevBest[i] = popList[i].rWorst;
                }
            fprintf(stderr,"  {%6d sec%c}\n",(uint)(t1-t0),(j) ? '*':' ');
            }
        if (t.tstFlags & TST_FLG_VERBOSE)
            {   /* then more details to stdout */
            printf("::::: Gen =%5d. Best =%6.3f. PopCnt =%5d. SampleCnt =%5d. time=%6d.\n",
                   genCnt+1,popList[0].rWorst/(double)t.sampleCnt,t.popCnt,t.sampleCnt,(uint)(t1-t0));
            for (i=0;i<keepCnt;i++)
                ShowSearchRec(stdout,&popList[i],t,SHOW_ROTS_PRELIM,(i)?' ':'-',i+1);
            fflush(stdout);
            }
        if (t.runHours && t.runHours*3600 < (uint) (t1 - t0))
            break;      /* timeout? */
        }

    /* re-grade the top entries using larger sampleCnt values */
    printf("\n+++++++++++++ Preliminary results: sampleCnt = %5d, block = %4d bits\n",t.sampleCnt,bitsPerBlock);
    qsort(popList,keepCnt,sizeof(popList[0]),Compare_SearchRec_Descending);
    for (i=0;i<keepCnt;i++)
        ShowSearchRec(stdout,&popList[i],t,SHOW_ROTS_PRELIM,' ',i+1);

    /* re-run several times, since there will be statistical variations */
    t.rotVerMask = MAX_ROT_VER_MASK;
    t.diffBits   = (t.diffBits & 0x100) ? t.diffBits : 3;
    t.sampleCnt *= 2;
    t.tstFlags  |= TST_FLG_SHOW;
    t.tstFlags  &= (TST_FLG_STDERR | TST_FLG_SHOW | TST_FLG_USE_ABS | TST_FLG_CHECK_ONE | TST_FLG_SHOW_HIST);

    for (j=0;j < ((t.tstFlags & TST_FLG_CHECK_ONE) ? 1u:2u) ;j++)
        {   /* do it twice, once with and once without USE_ABS, unless TST_FLG_CHECK_ONE set */
        if (!(t.tstFlags & TST_FLG_CHECK_ONE))
            t.tstFlags  ^= TST_FLG_USE_ABS;
        for (n=0;n<t.regradeCnt;n++)
            {
            t.sampleCnt *= 2;
            printf("+++ Re-running differentials with sampleCnt = %d, blockSize = %4d bits.%s\n",
                   t.sampleCnt,bitsPerBlock,(t.tstFlags & TST_FLG_USE_ABS)?" absDiff":"" );
            for (i=0;i<keepCnt;i++)
                {
                if (t.tstFlags & TST_FLG_STDERR)
                    fprintf(stderr,"       Re-run: samples=%d, blk=%4d. #%02d.%s    \r",
                            t.sampleCnt,bitsPerBlock,keepCnt-i,(t.tstFlags & TST_FLG_USE_ABS)?" absDiff":"" );
                CheckDifferentials(&popList[i],t);
                fflush(stdout);
                }
            if (keepCnt == 1)
                {   /* show random comparison for final values */
                printf("        RANDOM OUTPUT: /* useful stats for comparison to 'ideal' */\n");
                t.tstFlags |=  TST_FLG_DO_RAND;
                for (i=0;i<2;i++)
                    {
                    popList[keepCnt] =  popList[keepCnt-1];
                    CheckDifferentials(&popList[keepCnt],t);
                    }
                t.tstFlags &= ~TST_FLG_DO_RAND;
                }
            /* sort per new stats */
            if (t.tstFlags & TST_FLG_STDERR)
                fprintf(stderr,"\r%60s\r","");
            printf("\n+++++++++++++ Final results: sampleCnt = %5d, blockSize = %4d bits.%s\n",
                   t.sampleCnt,bitsPerBlock,(t.tstFlags & TST_FLG_USE_ABS)?" absDiff":"" );
            qsort(popList,keepCnt,sizeof(popList[0]),Compare_SearchRec_Descending);
            for (i=keepCnt;i;i--)
                ShowSearchRec(stdout,&popList[i-1],t,SHOW_ROTS_FINAL,(i==1)?'-':' ',i);
            fflush(stdout);
            }
        printf("\n+++++++++++++ Formatted results: sampleCnt = %5d, blockSize = %4d bits. %s\n",
               t.sampleCnt,bitsPerBlock,(t.tstFlags & TST_FLG_USE_ABS)?" absDiff":"" );
        for (i=keepCnt;i;i--)
            {
            ShowSearchRec(stdout,&popList[i-1],t,SHOW_ROTS_H,' ',i);
            printf("\n");
            Show_HW_rounds(popList[i-1].rotList);
            printf("\n");
            }
        fflush(stdout);
        t.sampleCnt >>= n;  /* revert to original sampleCnt */
        }

    time(&t1);
    printf("End:   %s\n",ASCII_TimeDate());
    printf("Elapsed time = %6.3f hours\n\n",(t1-t0)/(double)3600.0);
    if (t.tstFlags & TST_FLG_STDERR)
        fprintf(stderr,"\r%60s\n","");    /* clear the screen if needed */
    fflush(stdout);
    }

void GiveHelp(void)
    {
    printf("Usage:   skein_rot_search [options/flags]\n"
           "Options: -Bnn   = set Skein block size in bits (default=512)\n"
           "         -Cnn   = set count of random differentials taken\n"
           "         -Dnn   = set number bits of difference pattern tested (default=1)\n"
           "         -Gnn   = set min invalid rotation value (default 0)\n"
           "         -Inn   = set rotation version mask\n"
           "         -Onn   = set Hamming weight offset\n"
           "         -Pnn   = set population count\n"
           "         -Rnn   = set round count\n"
           "         -Snn   = set initial random seed (0 --> randomize)\n"
           "         -Tnn   = set max time to run (in hours)\n"
           "         -Wnn   = set minimum hamming weight\n"
           "         -Xnn   = set max test rotation count\n"
           "         -Znn   = set max rounds needed for saturation using OR\n"
           "         @file  = read rotations from file\n"
           "Flags:   -A     = use min, not absolute difference\n"
           "         -E     = no stderr output\n"
           "         -H     = show histogram (very wide)\n"
           "         -K     = keep minHW_or during twiddling\n"
           "         -Q     = disable quick exit in search\n"
           "         -U     = weighted repeat count (repeat best more frequently)\n"
           "         -V     = verbose mode\n"
          );
    exit(0);
    }

int main(int argc,char *argv[])
    {
    uint        i,bMin,bMax;
    testParms   t;
    uint chkInv =        1;   /* check inverse functions at startup (slow for debbuging) */
    uint goodRot=        2;   /* first allowed rotation value (+/-) */
    uint seed   =        1;   /* 0 = randomize based on time, else use specified seed */
    uint do8    =        0;   /* optimize 8-bit CPU performance */

    t.rounds    =        0;   /* number of Skein rounds to test */
    t.minHW_or  =        0;   /* minHW (using OR) required */
    t.minOffs   =        4;   /* heuristic used to speed up rotation search */
    t.diffBits  =        1;   /* # consecutive bits of differential inputs tested */
    t.sampleCnt =     1024;   /* number of differential pairs tested */
    t.genCntMax =        0;   /* number of "generations" tested */
    t.maxSatRnds=        0;   /* number of rounds to Hamming weight "saturation" */
    t.rotVerMask=        3;   /* mask of which versions to run */
    t.runHours  =        0;   /* stop searching after this many hours */
    t.dupRotMask=        0;   /* default is to allow same rotation value in a round */
    t.regradeCnt=        3;   /* how many scaled up counts to try */
    t.popCnt    = DEFAULT_POP_CNT;                      /* size of population */
    t.tstFlags  = TST_FLG_STDERR | TST_FLG_VERBOSE | TST_FLG_USE_ABS | TST_FLG_CHECK_ONE; /* default flags */

    for (i=1;i<(uint)argc;i++)
        {   /* parse command line args */
        if (argv[i][0] == '?')
            GiveHelp();
        else if (argv[i][0] == '-' || argv[i][0] == '+')
            {
#define arg_toi(s) atoi(s + ((s[2] == '=') ? 3 : 2))
            switch (toupper(argv[i][1]))
                {
                case '?': GiveHelp();                            break;
                                                                 
                case 'A': t.tstFlags   &= ~TST_FLG_USE_ABS;      break;
                case 'E': t.tstFlags   &= ~TST_FLG_STDERR;       break;
                case 'H': t.tstFlags   |=  TST_FLG_SHOW_HIST;    break;
                case 'K': t.tstFlags   |=  TST_FLG_KEEP_MIN_HW;  break;
                case 'Q': t.tstFlags   |=  TST_FLG_QUICK_EXIT;   break;
                case 'U': t.tstFlags   |=  TST_FLG_WEIGHT_REP;   break;
                case 'V': t.tstFlags   &= ~TST_FLG_VERBOSE;      break;
                case '1': t.tstFlags   &= ~TST_FLG_CHECK_ONE;    break;

                case 'B': bitsPerBlock  =  arg_toi(argv[i]);     break;
                case 'C': t.sampleCnt   =  arg_toi(argv[i]);     break;
                case 'D': t.diffBits    =  arg_toi(argv[i]);     break;
                case 'G': goodRot       =  arg_toi(argv[i]);     break;
                case 'I': t.rotVerMask  =  arg_toi(argv[i]);     break;
                case 'J': t.regradeCnt  =  arg_toi(argv[i]);     break;
                case 'O': t.minOffs     =  arg_toi(argv[i]);     break;
                case 'P': t.popCnt      =  arg_toi(argv[i]);     break;
                case 'R': t.rounds      =  arg_toi(argv[i]);     break;
                case 'S': seed          =  arg_toi(argv[i]);     break;
                case 'T': t.runHours    =  arg_toi(argv[i]);     break;
                case 'W': t.minHW_or    =  arg_toi(argv[i]);     break;
                case 'X': t.genCntMax   =  arg_toi(argv[i]);     break;
                case 'Z': t.maxSatRnds  =  arg_toi(argv[i]);     break;
                case '2': t.dupRotMask  = ~0u;                   break;
                case '0': chkInv        =  0;                    break;
                case '8': do8           =  1;                    break;

                default : printf("Unknown option: %s\n",argv[i]); GiveHelp();     break;
                }
            }
        else if (argv[i][0] == '@')
            {
            rotFileName = argv[i]+1;
            t.genCntMax = 1;            /* stop after one generation */
            }
        }

    if (chkInv)
        InverseChecks();    /* check fwd vs. rev transforms (slow in debugger) */

    t.goodRotCntMask = 0;
    for (i=goodRot; i <= BITS_PER_WORD - goodRot ;i++)
        t.goodRotCntMask |= (((u64b) 1) << i);
    if (do8) 
        t.goodRotCntMask = (((u64b) 0x03838383) << 32) | 0x83838380;

    if (bitsPerBlock == 0)
        {
        printf("Running search for all Skein block sizes (256, 512, and 1024)\n");
        t.rounds   = 0;   /* use defaults, since otherwise it makes little sense */
        t.minHW_or = 0;
        }

    bMin = (bitsPerBlock) ? bitsPerBlock :  256;
    bMax = (bitsPerBlock) ? bitsPerBlock : 1024;

    for (bitsPerBlock=bMin;bitsPerBlock<=bMax;bitsPerBlock*=2)
        {
        t.seed0 = (seed) ? seed : (uint) time(NULL);   /* randomize based on time if -s0 is given */
        RunSearch(t);
        }
    
    return 0;
    }
