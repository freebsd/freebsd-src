/* hashlen.c: The opiehashlen() library function.

%%% copyright-cmetz-96
This software is Copyright 1996-2001 by Craig Metz, All Rights Reserved.
The Inner Net License Version 3 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Modified by cmetz for OPIE 2.4. Use struct opie_otpkey, isolate variables.
	Created by cmetz for OPIE 2.3.
*/

#include "opie_cfg.h"
#include "opie.h"

VOIDRET opiehashlen FUNCTION((algorithm, in, out, n), int algorithm AND
VOIDPTR in AND struct opie_otpkey *results AND int n)
{
  switch(algorithm) {
#if 0
    case 3:
      {
      SHA_INFO sha;

      sha_init(&sha);
      sha_update(&sha, (BYTE *)in, n);
      sha_final(&sha);

      results->words[0] = sha.digest[0] ^ sha.digest[2] ^ sha.digest[4];
      results->words[1] = sha.digest[1] ^ sha.digest[3] ^ sha.digest[5];
      };
      break;
#endif /* 0 */
    case 4:
      {
      struct opiemdx_ctx mdx;
      UINT4 mdx_tmp[4];

      opiemd4init(&mdx);
      opiemd4update(&mdx, (unsigned char *)in, n);
      opiemd4final((unsigned char *)mdx_tmp, &mdx);

      results->words[0] = mdx_tmp[0] ^ mdx_tmp[2];
      results->words[1] = mdx_tmp[1] ^ mdx_tmp[3];
      }
      break;
    case 5:
      {
      struct opiemdx_ctx mdx;
      UINT4 mdx_tmp[4];

      opiemd5init(&mdx);
      opiemd5update(&mdx, (unsigned char *)in, n);
      opiemd5final((unsigned char *)mdx_tmp, &mdx);

      results->words[0] = mdx_tmp[0] ^ mdx_tmp[2];
      results->words[1] = mdx_tmp[1] ^ mdx_tmp[3];
      }
      break;
  }
}
