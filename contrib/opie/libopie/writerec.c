/* writerec.c: The __opiewriterec() library function.

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

	History:

	Created by cmetz for OPIE 2.3 from passwd.c.
*/
#include "opie_cfg.h"

#include <stdio.h>
#if TM_IN_SYS_TIME
#include <sys/time.h>
#else /* TM_IN_SYS_TIME */
#include <time.h>
#endif /* TM_IN_SYS_TIME */
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include "opie.h"

char *__opienone = "****************";

int __opiewriterec FUNCTION((opie), struct opie *opie)
{
  char buf[17], buf2[64];
  time_t now;
  FILE *f, *f2 = NULL;
  int i = 0;

  time(&now);
  if (strftime(buf2, sizeof(buf2), " %b %d,%Y %T", localtime(&now)) < 1)
    return -1;

  if (!(opie->opie_flags & __OPIE_FLAGS_READ)) {
    struct opie opie2;
    i = opielookup(&opie2, opie->opie_principal);
  }
    
  switch(i) {
  case 0:
    if (!(f = __opieopen(STD_KEY_FILE, 1, 0644)))
      return -1;
    if (!(f2 = __opieopen(EXT_KEY_FILE, 1, 0600)))
      return -1;
    if (fseek(f, opie->opie_recstart, SEEK_SET))
      return -1;
    if (fseek(f2, opie->opie_extrecstart, SEEK_SET))
      return -1;
    break;
  case 1:
    if (!(f = __opieopen(STD_KEY_FILE, 2, 0644)))
      return -1;
    if (!(f2 = __opieopen(EXT_KEY_FILE, 2, 0600)))
      return -1;
    break;
  default:
    return -1;
  }

  if (fprintf(f, "%s %04d %-16s %s %-21s\n", opie->opie_principal, opie->opie_n, opie->opie_seed, opie->opie_val ? opie->opie_val : __opienone, buf2) < 1)
    return -1;

  fclose(f);

  if (f2) {
    if (fprintf(f2, "%-32s %-16s %-77s\n", opie->opie_principal, opie->opie_reinitkey ? opie->opie_reinitkey : __opienone, "") < 1)
      return -1;
    
    fclose(f2);
  }

  return 0;
}
