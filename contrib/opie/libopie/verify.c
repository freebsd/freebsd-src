/* verify.c: The opieverify() library function.

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

	History:

	Created by cmetz for OPIE 2.3 using the old verify.c as a guide.
*/

#include "opie_cfg.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include "opie.h"

#define RESPONSE_STANDARD  0
#define RESPONSE_WORD      1
#define RESPONSE_HEX       2
#define RESPONSE_INIT      3
#define RESPONSE_INIT_WORD 4
#define RESPONSE_UNKNOWN   5

struct _rtrans {
  int type;
  char *name;
};

static struct _rtrans rtrans[] = {
  { RESPONSE_WORD, "word" },
  { RESPONSE_HEX, "hex" },
  { RESPONSE_INIT, "init" },
  { RESPONSE_INIT_WORD, "init-word" },
  { RESPONSE_STANDARD, "" },
  { RESPONSE_UNKNOWN, NULL }
};

static char *algids[] = { NULL, NULL, NULL, "sha1", "md4", "md5" };

static int changed FUNCTION((opie), struct opie *opie)
{
  struct opie opie2;

  memset(&opie2, 0, sizeof(struct opie));
  opie2.opie_principal = opie->opie_principal;
  if (__opiereadrec(&opie2))
    return 1;

  if ((opie2.opie_n != opie->opie_n) || strcmp(opie2.opie_val, opie->opie_val) || strcmp(opie2.opie_seed, opie->opie_seed))
    return 1;

  memset(&opie2, 0, sizeof(struct opie));
  return 0;
}

int opieverify FUNCTION((opie, response), struct opie *opie AND char *response)
{
  int i, rval = -1;
  char *c;
  char key[8], fkey[8], lastkey[8];
  struct opie nopie;

  if (!opie || !response)
    goto verret;

  if (!opie->opie_principal)
#if DEBUG
    abort();
#else /* DEBUG */
    goto verret;
#endif /* DEBUG */

  if (!opieatob8(lastkey, opie->opie_val))
    goto verret;

  if (c = strchr(response, ':')) {
    *(c++) = 0;
    {
      struct _rtrans *r;
      for (r = rtrans; r->name && strcmp(r->name, response); r++);
      i = r->type;
    }
  } else
    i = RESPONSE_STANDARD;

  switch(i) {
  case RESPONSE_STANDARD:
    i = 1;
    
    if (opieetob(key, response) == 1) {
      memcpy(fkey, key, sizeof(key));
      opiehash(fkey, MDX);
      i = memcmp(fkey, lastkey, sizeof(key));
    }
    if (i && opieatob8(key, response)) {
      memcpy(fkey, key, sizeof(key));
      opiehash(fkey, MDX);
      i = memcmp(fkey, lastkey, sizeof(key));
    }
    break;
  case RESPONSE_WORD:
    i = 1;

    if (opieetob(key, c) == 1) {
      memcpy(fkey, key, sizeof(key));
      opiehash(fkey, MDX);
      i = memcmp(fkey, lastkey, sizeof(key));
    }
    break;
  case RESPONSE_HEX:
    i = 1;

    if (opieatob8(key, c)) {
      memcpy(fkey, key, sizeof(key));
      opiehash(fkey, MDX);
      i = memcmp(fkey, lastkey, sizeof(key));
    }
    break;
  case RESPONSE_INIT:
  case RESPONSE_INIT_WORD:
    {
      char *c2;
      char newkey[8], ckxor[8], ck[8], cv[8], cvc[8];
      char buf[OPIE_SEED_MAX + 48 + 1];

      if (!(c2 = strchr(c, ':')))
	goto verret;

      *(c2++) = 0;

      if (i == RESPONSE_INIT) {
	if (!opieatob8(key, c))
	  goto verret;
      } else {
	if (opieetob(key, c) != 1)
	  goto verret;
      }

      memcpy(fkey, key, sizeof(key));
      opiehash(fkey, MDX);

      if (memcmp(fkey, lastkey, sizeof(key)))
	goto verret;

      if (changed(opie))
	goto verret;
      
      opie->opie_n--;

      if (!opiebtoa8(opie->opie_val, key))
	goto verret;

      if (__opiewriterec(opie))
	goto verret;

      if (!(c2 = strchr(c = c2, ':')))
	goto verret;

      *(c2++) = 0;

      {
	int j;

	if (_opieparsechallenge(c, &j, &(opie->opie_n), &(opie->opie_seed)) || (j != MDX))
	  goto verret;
      }

      if (!(c2 = strchr(c = c2, ':')))
	goto verret;

      *(c2++) = 0;

      if (i == RESPONSE_INIT) {
	if (!opieatob8(newkey, c))
	  goto verret;
      } else {
	if (opieetob(newkey, c) != 1)
	  goto verret;
      }

      if (!opie->opie_reinitkey || (opie->opie_reinitkey[0] == '*'))
	goto verwrt;

      if (!(c2 = strchr(c = c2, ':')))
	goto verret;

      *(c2++) = 0;

      if (i == RESPONSE_INIT) {
	if (!opieatob8(ckxor, c))
	  goto verret;
	if (!opieatob8(cv, c2))
	  goto verret;
      } else {
	if (opieetob(ckxor, c) != 1)
	  goto verret;
	if (opieetob(cv, c2) != 1)
	  goto verret;
      }

      if (!opieatob8(ck, opie->opie_reinitkey))
	goto verret;

      c = buf;
      memcpy(c, ck, sizeof(ck)); c += sizeof(ck);
      memcpy(c, key, sizeof(key)); c += sizeof(key);
      c += sprintf(c, "%s 499 %s", algids[MDX], opie->opie_seed);
      memcpy(c, newkey, sizeof(newkey)); c += sizeof(newkey);
      memcpy(c, ckxor, sizeof(ckxor)); c += sizeof(ckxor);
      memcpy(c, ck, sizeof(ck)); c += sizeof(ck);
      opiehashlen(MDX, buf, cvc, (unsigned int)c - (unsigned int)buf);

      if (memcmp(cv, cvc, sizeof(cv)))
	goto verret;

      for (i = 0; i < 8; i++)
	ck[i] ^= ckxor[i];

      if (!opiebtoa8(opie->opie_reinitkey, ck))
	goto verret;

      memcpy(key, newkey, sizeof(key));
    }
    goto verwrt;
  case RESPONSE_UNKNOWN:
    rval = 1;
    goto verret;
  default:
    rval = -1;
    goto verret;
  }

  if (i) {
    rval = 1;
    goto verret;
  }

  if (changed(opie))
    goto verret;
  
  opie->opie_n--;

verwrt:
  if (!opiebtoa8(opie->opie_val, key))
    goto verret;
  rval = __opiewriterec(opie);

verret:
  opieunlock();
  memset(opie, 0, sizeof(struct opie));
  return rval;
}
