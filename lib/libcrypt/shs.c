/*
 ********************************************************************
 *     SHS 180-1 Reference Implementation (with unrolled loops)     *
 *             Copyright 1995-6 by Paul C. Kocher.                  *
 *                                                                  *
 *  This file is provided as-is, without warranty of any kind;      *
 *     use at your own risk.  This file may be copied and used,     *
 *     even for commercial purposes, for free.  For information,    *
 *     updates, updates, or consulting help, send e-mail to the     *
 *     author at pck@cryptography.com.                              *
 *                                                                  *
 *  EXPORT STATUS:  In informal discussions, the U.S. National      *
 *     Security Agency has indicated to me that source code for     *
 *     can be exported from the U.S. freely, but programs using     *
 *     or incorporating this code may be restricted.  Please make   *
 *     sure you understand the applicable export regulations        *
 *     before doing any work with cryptography.                     *
 *                                                                  *
 *  For links to other cryptography source code, papers, etc. see   *
 *     http://www.cryptography.com.                                 *
 ********************************************************************
 */

#include "shs.h"

static void shsCompress(SHS_CTX *ctx);

#define SHS_ROTL(X,n) (((X) << (n)) | ((X) >> (32-(n))))
#define SHS_F1(X,Y,Z) ((((Y)^(Z))&(X))^(Z))
#define SHS_F2(X,Y,Z) ((X)^(Y)^(Z))
#define SHS_F3(X,Y,Z) (((X)&(Y))|((Z)&((X)|(Y))))
#define SHS_F4(X,Y,Z) ((X)^(Y)^(Z))


/*
 *  SHS: Initialize context
 */
void shsInit(SHS_CTX *ctx) {
  ctx->lenW = 0;
  ctx->sizeHi = ctx->sizeLo = 0;

  /*
   *  Initialize H with constants from FIPS180-1.
   */
  ctx->H[0] = 0x67452301L;
  ctx->H[1] = 0xefcdab89L;
  ctx->H[2] = 0x98badcfeL;
  ctx->H[3] = 0x10325476L;
  ctx->H[4] = 0xc3d2e1f0L;
}


/*
 *  SHS: Add data to context.
 */
void shsUpdate(SHS_CTX *ctx, const unsigned char * dataIn, int len) {
  /*
   *  Read the data into W and process blocks as they get full
   *
   *  NOTE: The shifts can be eliminated on big-endian machines, since 
   *      the byte-to-word transformation can be done with a copy.  In 
   *      assembly language on 80486+ computers, the BSWAP instruction 
   *      can be used.
   */
  ctx->sizeLo += 8*len;
  ctx->sizeHi += (ctx->sizeLo < 8*len) + (len >> 29);
  while (len--) {
    ctx->W[ctx->lenW / 4] <<= 8;
    ctx->W[ctx->lenW / 4] |= *(dataIn++);
    if (((++ctx->lenW) & 63) == 0) {
      shsCompress(ctx);
      ctx->lenW = 0;
    }
  }
}


/*
 *  SHS: Generate hash value from context
 */
void shsFinal(SHS_CTX * ctx, unsigned char hashOut[20]) {
  static unsigned char bulk_pad[64] = { 0x80,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  };
  unsigned char length_pad[8];
  int i;

  /*
   *  Pad with a binary 1 (e.g. 0x80), then zeroes, then length
   */
  length_pad[0] = (unsigned char)((ctx->sizeHi >> 24) & 255);
  length_pad[1] = (unsigned char)((ctx->sizeHi >> 16) & 255);
  length_pad[2] = (unsigned char)((ctx->sizeHi >> 8) & 255);
  length_pad[3] = (unsigned char)((ctx->sizeHi >> 0) & 255);
  length_pad[4] = (unsigned char)((ctx->sizeLo >> 24) & 255);
  length_pad[5] = (unsigned char)((ctx->sizeLo >> 16) & 255);
  length_pad[6] = (unsigned char)((ctx->sizeLo >> 8) & 255);
  length_pad[7] = (unsigned char)((ctx->sizeLo >> 0) & 255);
  shsUpdate(ctx, bulk_pad, ((56+64) - ctx->lenW) & 63);
  shsUpdate(ctx, length_pad, 8);

  /*
   *  Output hash
   */
  for (i = 0; i < 5; i++) {
    *(hashOut++) = ((unsigned char)(ctx->H[i] >> 24)) & 255;
    *(hashOut++) = ((unsigned char)(ctx->H[i] >> 16)) & 255;
    *(hashOut++) = ((unsigned char)(ctx->H[i] >>  8)) & 255;
    *(hashOut++) = ((unsigned char)(ctx->H[i]      )) & 255;
  }

  /*
   *  Re-initialize the context (also zeroizes contents)
   */
  shsInit(ctx);
}


/*
 *  SHS: Hash a block in memory
 */
void shsBlock(const unsigned char *dataIn, int len, unsigned char hashOut[20]) {
  SHS_CTX ctx;

  shsInit(&ctx);
  shsUpdate(&ctx, dataIn, len);
  shsFinal(&ctx, hashOut);
}


/*
 *  SHS: Compression function, unrolled.
 */
static void shsCompress(SHS_CTX *ctx) {
  int t;
  register unsigned long A,B,C,D,E;

  /*
   *  This can be moved into the main code block below, but doing
   *  so can cause some compilers to run out of registers and resort
   *  to storing intermediates in RAM.
   */
  for (t = 16; t <= 79; t++)
    ctx->W[t] =
      SHS_ROTL(ctx->W[t-3] ^ ctx->W[t-8] ^ ctx->W[t-14] ^ ctx->W[t-16], 1);

  A = ctx->H[0];
  B = ctx->H[1];
  C = ctx->H[2];
  D = ctx->H[3];
  E = ctx->H[4];

  E = SHS_ROTL(A,5)+SHS_F1(B,C,D)+E+ctx->W[ 0]+0x5a827999L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F1(A,B,C)+D+ctx->W[ 1]+0x5a827999L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F1(E,A,B)+C+ctx->W[ 2]+0x5a827999L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F1(D,E,A)+B+ctx->W[ 3]+0x5a827999L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F1(C,D,E)+A+ctx->W[ 4]+0x5a827999L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F1(B,C,D)+E+ctx->W[ 5]+0x5a827999L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F1(A,B,C)+D+ctx->W[ 6]+0x5a827999L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F1(E,A,B)+C+ctx->W[ 7]+0x5a827999L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F1(D,E,A)+B+ctx->W[ 8]+0x5a827999L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F1(C,D,E)+A+ctx->W[ 9]+0x5a827999L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F1(B,C,D)+E+ctx->W[10]+0x5a827999L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F1(A,B,C)+D+ctx->W[11]+0x5a827999L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F1(E,A,B)+C+ctx->W[12]+0x5a827999L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F1(D,E,A)+B+ctx->W[13]+0x5a827999L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F1(C,D,E)+A+ctx->W[14]+0x5a827999L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F1(B,C,D)+E+ctx->W[15]+0x5a827999L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F1(A,B,C)+D+ctx->W[16]+0x5a827999L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F1(E,A,B)+C+ctx->W[17]+0x5a827999L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F1(D,E,A)+B+ctx->W[18]+0x5a827999L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F1(C,D,E)+A+ctx->W[19]+0x5a827999L; C=SHS_ROTL(C,30); 

  E = SHS_ROTL(A,5)+SHS_F2(B,C,D)+E+ctx->W[20]+0x6ed9eba1L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F2(A,B,C)+D+ctx->W[21]+0x6ed9eba1L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F2(E,A,B)+C+ctx->W[22]+0x6ed9eba1L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F2(D,E,A)+B+ctx->W[23]+0x6ed9eba1L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F2(C,D,E)+A+ctx->W[24]+0x6ed9eba1L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F2(B,C,D)+E+ctx->W[25]+0x6ed9eba1L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F2(A,B,C)+D+ctx->W[26]+0x6ed9eba1L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F2(E,A,B)+C+ctx->W[27]+0x6ed9eba1L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F2(D,E,A)+B+ctx->W[28]+0x6ed9eba1L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F2(C,D,E)+A+ctx->W[29]+0x6ed9eba1L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F2(B,C,D)+E+ctx->W[30]+0x6ed9eba1L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F2(A,B,C)+D+ctx->W[31]+0x6ed9eba1L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F2(E,A,B)+C+ctx->W[32]+0x6ed9eba1L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F2(D,E,A)+B+ctx->W[33]+0x6ed9eba1L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F2(C,D,E)+A+ctx->W[34]+0x6ed9eba1L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F2(B,C,D)+E+ctx->W[35]+0x6ed9eba1L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F2(A,B,C)+D+ctx->W[36]+0x6ed9eba1L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F2(E,A,B)+C+ctx->W[37]+0x6ed9eba1L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F2(D,E,A)+B+ctx->W[38]+0x6ed9eba1L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F2(C,D,E)+A+ctx->W[39]+0x6ed9eba1L; C=SHS_ROTL(C,30); 

  E = SHS_ROTL(A,5)+SHS_F3(B,C,D)+E+ctx->W[40]+0x8f1bbcdcL; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F3(A,B,C)+D+ctx->W[41]+0x8f1bbcdcL; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F3(E,A,B)+C+ctx->W[42]+0x8f1bbcdcL; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F3(D,E,A)+B+ctx->W[43]+0x8f1bbcdcL; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F3(C,D,E)+A+ctx->W[44]+0x8f1bbcdcL; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F3(B,C,D)+E+ctx->W[45]+0x8f1bbcdcL; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F3(A,B,C)+D+ctx->W[46]+0x8f1bbcdcL; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F3(E,A,B)+C+ctx->W[47]+0x8f1bbcdcL; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F3(D,E,A)+B+ctx->W[48]+0x8f1bbcdcL; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F3(C,D,E)+A+ctx->W[49]+0x8f1bbcdcL; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F3(B,C,D)+E+ctx->W[50]+0x8f1bbcdcL; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F3(A,B,C)+D+ctx->W[51]+0x8f1bbcdcL; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F3(E,A,B)+C+ctx->W[52]+0x8f1bbcdcL; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F3(D,E,A)+B+ctx->W[53]+0x8f1bbcdcL; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F3(C,D,E)+A+ctx->W[54]+0x8f1bbcdcL; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F3(B,C,D)+E+ctx->W[55]+0x8f1bbcdcL; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F3(A,B,C)+D+ctx->W[56]+0x8f1bbcdcL; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F3(E,A,B)+C+ctx->W[57]+0x8f1bbcdcL; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F3(D,E,A)+B+ctx->W[58]+0x8f1bbcdcL; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F3(C,D,E)+A+ctx->W[59]+0x8f1bbcdcL; C=SHS_ROTL(C,30); 

  E = SHS_ROTL(A,5)+SHS_F4(B,C,D)+E+ctx->W[60]+0xca62c1d6L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F4(A,B,C)+D+ctx->W[61]+0xca62c1d6L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F4(E,A,B)+C+ctx->W[62]+0xca62c1d6L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F4(D,E,A)+B+ctx->W[63]+0xca62c1d6L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F4(C,D,E)+A+ctx->W[64]+0xca62c1d6L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F4(B,C,D)+E+ctx->W[65]+0xca62c1d6L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F4(A,B,C)+D+ctx->W[66]+0xca62c1d6L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F4(E,A,B)+C+ctx->W[67]+0xca62c1d6L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F4(D,E,A)+B+ctx->W[68]+0xca62c1d6L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F4(C,D,E)+A+ctx->W[69]+0xca62c1d6L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F4(B,C,D)+E+ctx->W[70]+0xca62c1d6L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F4(A,B,C)+D+ctx->W[71]+0xca62c1d6L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F4(E,A,B)+C+ctx->W[72]+0xca62c1d6L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F4(D,E,A)+B+ctx->W[73]+0xca62c1d6L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F4(C,D,E)+A+ctx->W[74]+0xca62c1d6L; C=SHS_ROTL(C,30); 
  E = SHS_ROTL(A,5)+SHS_F4(B,C,D)+E+ctx->W[75]+0xca62c1d6L; B=SHS_ROTL(B,30); 
  D = SHS_ROTL(E,5)+SHS_F4(A,B,C)+D+ctx->W[76]+0xca62c1d6L; A=SHS_ROTL(A,30); 
  C = SHS_ROTL(D,5)+SHS_F4(E,A,B)+C+ctx->W[77]+0xca62c1d6L; E=SHS_ROTL(E,30); 
  B = SHS_ROTL(C,5)+SHS_F4(D,E,A)+B+ctx->W[78]+0xca62c1d6L; D=SHS_ROTL(D,30); 
  A = SHS_ROTL(B,5)+SHS_F4(C,D,E)+A+ctx->W[79]+0xca62c1d6L; C=SHS_ROTL(C,30); 

  ctx->H[0] += A;
  ctx->H[1] += B;
  ctx->H[2] += C;
  ctx->H[3] += D;
  ctx->H[4] += E;
}

