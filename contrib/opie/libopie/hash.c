/* hash.c: The opiehash() library function.

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

        Created by cmetz for OPIE 2.3 using the old hash.c as a guide.
*/

#include "opie_cfg.h"
#include "opie.h"

#include <md4.h>
#include <md5.h>

static UINT4 mdx_tmp[4];
#if 0
static SHA_INFO sha;
#endif /* 0 */

VOIDRET opiehash FUNCTION((x, algorithm), VOIDPTR x AND unsigned algorithm)
{
  UINT4 *results = (UINT4 *)x;

  switch(algorithm) {
#if 0
    case 3:
      sha_init(&sha);
      sha_update(&sha, (BYTE *)x, 8);
      sha_final(&sha);
      results[0] = sha.digest[0] ^ sha.digest[2] ^ sha.digest[4];
      results[1] = sha.digest[1] ^ sha.digest[3] ^ sha.digest[5];
      break;
#endif /* 0 */
    case 4: {
      MD4_CTX mdx;
      MD4Init(&mdx);
      MD4Update(&mdx, (unsigned char *)x, 8);
      MD4Final((unsigned char *)mdx_tmp, &mdx);
      results[0] = mdx_tmp[0] ^ mdx_tmp[2];
      results[1] = mdx_tmp[1] ^ mdx_tmp[3];
      break;
    }
    case 5: {
      MD5_CTX mdx;
      MD5Init(&mdx);
      MD5Update(&mdx, (unsigned char *)x, 8);
      MD5Final((unsigned char *)mdx_tmp, &mdx);
      results[0] = mdx_tmp[0] ^ mdx_tmp[2];
      results[1] = mdx_tmp[1] ^ mdx_tmp[3];
      break;
    }
  }
}
