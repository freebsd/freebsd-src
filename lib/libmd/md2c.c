/* MD2C.C - RSA Data Security, Inc., MD2 message-digest algorithm
 * $Id: md2c.c,v 1.5 1997/02/22 15:07:15 peter Exp $
 */

/* Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
   rights reserved.

   License to copy and use this software is granted for
   non-commercial Internet Privacy-Enhanced Mail provided that it is
   identified as the "RSA Data Security, Inc. MD2 Message Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#include "md2.h"
#include <string.h>
#include <sys/types.h>


typedef unsigned char *POINTER;
typedef u_int16_t UINT2;
typedef u_int32_t UINT4;

#define PROTO_LIST(list) list

static void MD2Transform PROTO_LIST
  ((unsigned char [16], unsigned char [16], const unsigned char [16]));

/* Permutation of 0..255 constructed from the digits of pi. It gives a
   "random" nonlinear byte substitution operation.
 */
static unsigned char PI_SUBST[256] = {
  41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
  19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
  76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
  138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
  245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
  148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
  39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
  181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
  150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
  112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
  96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
  85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
  234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
  129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
  8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
  203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
  166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
  31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

static unsigned char *PADDING[] = {
  (unsigned char *)"",
  (unsigned char *)"\001",
  (unsigned char *)"\002\002",
  (unsigned char *)"\003\003\003",
  (unsigned char *)"\004\004\004\004",
  (unsigned char *)"\005\005\005\005\005",
  (unsigned char *)"\006\006\006\006\006\006",
  (unsigned char *)"\007\007\007\007\007\007\007",
  (unsigned char *)"\010\010\010\010\010\010\010\010",
  (unsigned char *)"\011\011\011\011\011\011\011\011\011",
  (unsigned char *)"\012\012\012\012\012\012\012\012\012\012",
  (unsigned char *)"\013\013\013\013\013\013\013\013\013\013\013",
  (unsigned char *)"\014\014\014\014\014\014\014\014\014\014\014\014",
  (unsigned char *)
    "\015\015\015\015\015\015\015\015\015\015\015\015\015",
  (unsigned char *)
    "\016\016\016\016\016\016\016\016\016\016\016\016\016\016",
  (unsigned char *)
    "\017\017\017\017\017\017\017\017\017\017\017\017\017\017\017",
  (unsigned char *)
    "\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020"
};

/* MD2 initialization. Begins an MD2 operation, writing a new context.
 */
void MD2Init (context)
MD2_CTX *context;                                        /* context */
{
  context->count = 0;
  memset ((POINTER)context->state, 0, sizeof (context->state));
  memset
    ((POINTER)context->checksum, 0, sizeof (context->checksum));
}

/* MD2 block update operation. Continues an MD2 message-digest
     operation, processing another message block, and updating the
     context.
 */
void MD2Update (context, input, inputLen)
MD2_CTX *context;                                        /* context */
const unsigned char *input;                                /* input block */
unsigned int inputLen;                     /* length of input block */
{
  unsigned int i, index, partLen;

  /* Update number of bytes mod 16 */
  index = context->count;
  context->count = (index + inputLen) & 0xf;

  partLen = 16 - index;

  /* Transform as many times as possible.
    */
  if (inputLen >= partLen) {
    memcpy
      ((POINTER)&context->buffer[index], (POINTER)input, partLen);
    MD2Transform (context->state, context->checksum, context->buffer);

    for (i = partLen; i + 15 < inputLen; i += 16)
      MD2Transform (context->state, context->checksum, &input[i]);

    index = 0;
  }
  else
    i = 0;

  /* Buffer remaining input */
  memcpy
    ((POINTER)&context->buffer[index], (POINTER)&input[i],
     inputLen-i);
}

/* MD2 padding.
 */
void MD2Pad (context)
MD2_CTX *context;                                        /* context */
{
  unsigned int index, padLen;

  /* Pad out to multiple of 16.
   */
  index = context->count;
  padLen = 16 - index;
  MD2Update (context, PADDING[padLen], padLen);

  /* Extend with checksum */
  MD2Update (context, context->checksum, 16);
}

/* MD2 finalization. Ends an MD2 message-digest operation, writing the
     message digest and zeroizing the context.
 */
void MD2Final (digest, context)
unsigned char digest[16];                         /* message digest */
MD2_CTX *context;                                        /* context */
{
  /* Do padding */
  MD2Pad (context);

  /* Store state in digest */
  memcpy ((POINTER)digest, (POINTER)context->state, 16);

  /* Zeroize sensitive information.
   */
  memset ((POINTER)context, 0, sizeof (*context));
}

/* MD2 basic transformation. Transforms state and updates checksum
     based on block.
 */
static void MD2Transform (state, checksum, block)
unsigned char state[16];
unsigned char checksum[16];
const unsigned char block[16];
{
  unsigned int i, j, t;
  unsigned char x[48];

  /* Form encryption block from state, block, state ^ block.
   */
  memcpy ((POINTER)x, (POINTER)state, 16);
  memcpy ((POINTER)x+16, (POINTER)block, 16);
  for (i = 0; i < 16; i++)
    x[i+32] = state[i] ^ block[i];

  /* Encrypt block (18 rounds).
   */
  t = 0;
  for (i = 0; i < 18; i++) {
    for (j = 0; j < 48; j++)
      t = x[j] ^= PI_SUBST[t];
    t = (t + i) & 0xff;
  }

  /* Save new state */
  memcpy ((POINTER)state, (POINTER)x, 16);

  /* Update checksum.
   */
  t = checksum[15];
  for (i = 0; i < 16; i++)
    t = checksum[i] ^= PI_SUBST[block[i] ^ t];

  /* Zeroize sensitive information.
   */
  memset ((POINTER)x, 0, sizeof (x));
}
