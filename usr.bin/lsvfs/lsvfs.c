/*
 * lsvfs - list loaded VFSes
 * Garrett A. Wollman, September 1994
 * This file is in the public domain.
 *
 * $FreeBSD: src/usr.bin/lsvfs/lsvfs.c,v 1.13 1999/08/28 01:03:17 peter Exp $
 */

#define _NEW_VFSCONF

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

#define FMT "%-32.32s %5d %s\n"
#define HDRFMT "%-32.32s %5.5s %s\n"
#define DASHES "-------------------------------- ----- ---------------\n"

static const char *fmt_flags(int);

int
main(int argc, char **argv)
{
  int rv = 0;
  struct vfsconf vfc;
  struct ovfsconf *ovfcp;
  argc--, argv++;

  setvfsent(1);

  printf(HDRFMT, "Filesystem", "Refs", "Flags");
  fputs(DASHES, stdout);

  if(argc) {
    for(; argc; argc--, argv++) {
      if (getvfsbyname(*argv, &vfc) == 0) {
        printf(FMT, vfc.vfc_name, vfc.vfc_refcount, fmt_flags(vfc.vfc_flags));
      } else {
	warnx("VFS %s unknown or not loaded", *argv);
        rv++;
      }
    }
  } else {
    while (ovfcp = getvfsent()) {
      printf(FMT, ovfcp->vfc_name, ovfcp->vfc_refcount,
             fmt_flags(ovfcp->vfc_flags));
    }
  }

  endvfsent();
  return rv;
}

static const char *
fmt_flags(int flags)
{
  /*
   * NB: if you add new flags, don't forget to add them here vvvvvv too.
   */
  static char buf[sizeof
    "static, network, read-only, synthetic, loopback, unicode"];
  int comma = 0;

  buf[0] = '\0';

  if(flags & VFCF_STATIC) {
    if(comma++) strcat(buf, ", ");
    strcat(buf, "static");
  }

  if(flags & VFCF_NETWORK) {
    if(comma++) strcat(buf, ", ");
    strcat(buf, "network");
  }

  if(flags & VFCF_READONLY) {
    if(comma++) strcat(buf, ", ");
    strcat(buf, "read-only");
  }

  if(flags & VFCF_SYNTHETIC) {
    if(comma++) strcat(buf, ", ");
    strcat(buf, "synthetic");
  }

  if(flags & VFCF_LOOPBACK) {
    if(comma++) strcat(buf, ", ");
    strcat(buf, "loopback");
  }

  if(flags & VFCF_UNICODE) {
    if(comma++) strcat(buf, ", ");
    strcat(buf, "unicode");
  }

  return buf;
}
