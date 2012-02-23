/*
 * lsvfs - list loaded VFSes
 * Garrett A. Wollman, September 1994
 * This file is in the public domain.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FMT "%-32.32s %5d %s\n"
#define HDRFMT "%-32.32s %5.5s %s\n"
#define DASHES "-------------------------------- ----- ---------------\n"

static const char *fmt_flags(int);

int
main(int argc, char **argv)
{
  int cnt, rv = 0, i; 
  struct xvfsconf vfc, *xvfsp;
  size_t buflen;
  argc--, argv++;

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
    if (sysctlbyname("vfs.conflist", NULL, &buflen, NULL, 0) < 0)
      err(1, "sysctl(vfs.conflist)");
    xvfsp = malloc(buflen);
    if (xvfsp == NULL)
      errx(1, "malloc failed");
    if (sysctlbyname("vfs.conflist", xvfsp, &buflen, NULL, 0) < 0)
      err(1, "sysctl(vfs.conflist)");
    cnt = buflen / sizeof(struct xvfsconf);

    for (i = 0; i < cnt; i++) {
      printf(FMT, xvfsp[i].vfc_name, xvfsp[i].vfc_refcount,
             fmt_flags(xvfsp[i].vfc_flags));
    }
    free(xvfsp);
  }

  return rv;
}

static const char *
fmt_flags(int flags)
{
  /*
   * NB: if you add new flags, don't forget to add them here vvvvvv too.
   */
  static char buf[sizeof
    "static, network, read-only, synthetic, loopback, unicode, jail"];
  size_t len;

  buf[0] = '\0';

  if(flags & VFCF_STATIC)
    strlcat(buf, "static, ", sizeof(buf));
  if(flags & VFCF_NETWORK)
    strlcat(buf, "network, ", sizeof(buf));
  if(flags & VFCF_READONLY)
    strlcat(buf, "read-only, ", sizeof(buf));
  if(flags & VFCF_SYNTHETIC)
    strlcat(buf, "synthetic, ", sizeof(buf));
  if(flags & VFCF_LOOPBACK)
    strlcat(buf, "loopback, ", sizeof(buf));
  if(flags & VFCF_UNICODE)
    strlcat(buf, "unicode, ", sizeof(buf));
  if(flags & VFCF_JAIL)
    strlcat(buf, "jail, ", sizeof(buf));
  if(flags & VFCF_DELEGADMIN)
    strlcat(buf, "delegated-administration, ", sizeof(buf));
  len = strlen(buf);
  if (len > 2 && buf[len - 2] == ',')
    buf[len - 2] = '\0';

  return buf;
}
