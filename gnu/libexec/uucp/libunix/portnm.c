/* portnm.c
   Get the port name of stdin.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

#if HAVE_TCP
#if HAVE_SYS_TYPES_TCP_H
#include <sys/types.tcp.h>
#endif
#include <sys/socket.h>
#endif

#ifndef ttyname
extern char *ttyname ();
#endif

/* Get the port name of standard input.  I assume that Unix systems
   generally support ttyname.  If they don't, this function can just
   return NULL.  It uses getsockname to see whether standard input is
   a TCP connection.  */

const char *
zsysdep_port_name (ftcp_port)
     boolean *ftcp_port;
{
  const char *z;

  *ftcp_port = FALSE;

#if HAVE_TCP
  {
    size_t clen;
    struct sockaddr s;

    clen = sizeof (struct sockaddr);
    if (getsockname (0, &s, &clen) == 0)
      *ftcp_port = TRUE;
  }
#endif /* HAVE_TCP */

  z = ttyname (0);
  if (z == NULL)
    return NULL;
  if (strncmp (z, "/dev/", sizeof "/dev/" - 1) == 0)
    return z + sizeof "/dev/" - 1;
  else
    return z;
}
