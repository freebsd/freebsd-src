/* uuto.c
   Translate a destination for uuto.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* Translate a uuto destination for Unix.  */

char *
zsysdep_uuto (zdest, zlocalname)
     const char *zdest;
     const char *zlocalname;
{
  const char *zexclam;
  char *zto;

  zexclam = strrchr (zdest, '!');
  if (zexclam == NULL)
    return NULL;
  zto = (char *) zbufalc (zexclam - zdest
			  + sizeof "!~/receive///"
			  + strlen (zexclam)
			  + strlen (zlocalname));
  memcpy (zto, zdest, (size_t) (zexclam - zdest));
  sprintf (zto + (zexclam - zdest), "!~/receive/%s/%s/",
	   zexclam + 1, zlocalname);
  return zto;
}
