/* status.c
   Strings for status codes.  */

#include "uucp.h"

#include "uudefs.h"

/* Status strings.  These must match enum tstatus_type.  */

const char *azStatus[] =
{
  "Conversation complete",
  "Port unavailable",
  "Dial failed",
  "Login failed",
  "Handshake failed",
  "Call failed",
  "Talking",
  "Wrong time to call"
};
