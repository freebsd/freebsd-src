/* hash.c: The opiehash() library function.

%%% copyright-cmetz-96
This software is Copyright 1996-2001 by Craig Metz, All Rights Reserved.
The Inner Net License Version 3 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Modified by cmetz for OPIE 2.4. Use struct opie_otpkey for binary arg. 
	Modified by cmetz for OPIE 2.31. Added SHA support (which may
              not be correct). Backed out previous optimizations as
              they killed thread-safety.
        Created by cmetz for OPIE 2.3 using the old hash.c as a guide.
*/

#include "opie_cfg.h"
#if 0
#include "sha.h"
#endif /* 0 */
#include "opie.h"

VOIDRET opiehash FUNCTION((x, algorithm), struct opie_otpkey *results AND
unsigned algorithm)
{
  switch(algorithm) {
#if 0
    case 3:
      {
      SHA_CTX sha;

      SHAInit(&sha);
      SHAUpdate(&sha, (unsigned char *)results, 8);
      SHAFinal(&sha);

      results->words[0] = sha.buffer[0] ^ sha.buffer[2] ^ sha.buffer[4];
      results->words[1] = sha.buffer[1] ^ sha.buffer[3];
      };
      break;
#endif /* 0 */
    case 4:
      {
      struct opiemdx_ctx mdx;
      UINT4 mdx_tmp[4];

      opiemd4init(&mdx);
      opiemd4update(&mdx, (unsigned char *)results, 8);
      opiemd4final((unsigned char *)mdx_tmp, &mdx);

      results->words[0] = mdx_tmp[0] ^ mdx_tmp[2];
      results->words[1] = mdx_tmp[1] ^ mdx_tmp[3];
      };
      break;
    case 5:
      {
      struct opiemdx_ctx mdx;
      UINT4 mdx_tmp[4];

      opiemd5init(&mdx);
      opiemd5update(&mdx, (unsigned char *)results, 8);
      opiemd5final((unsigned char *)mdx_tmp, &mdx);

      results->words[0] = mdx_tmp[0] ^ mdx_tmp[2];
      results->words[1] = mdx_tmp[1] ^ mdx_tmp[3];
      };
      break;
  }
}
