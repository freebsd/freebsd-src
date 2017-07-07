/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* SIOCGIFCONF:

   The behavior of this ioctl varies across systems.

   The "largest gap" values are the largest number of bytes I've seen
   left unused at the end of the supplied buffer when there were more
   entries to return.  These values may of coures be dependent on the
   configurations of the particular systems I was testing with.

   NetBSD 1.5-alpha: The returned ifc_len is the desired amount of
   space, always.  The returned list may be truncated if there isn't
   enough room; no overrun.  Largest gap: 43.  (NetBSD now has
   getifaddrs.)

   BSD/OS 4.0.1 (courtesy djm): The returned ifc_len is equal to or
   less than the supplied ifc_len.  Sometimes the entire buffer is
   used; sometimes N-1 bytes; occasionally, the buffer must have quite
   a bit of extra room before the next structure will be added.
   Largest gap: 39.

   Solaris 7,8: Return EINVAL if the buffer space is too small for all
   the data to be returned, including ifc_len==0.  Solaris is the only
   system I've found so far that actually returns an error.  No gap.
   However, SIOCGIFNUM may be used to query the number of interfaces.

   Linux 2.2.12 (RH 6.1 dist, x86): The buffer is filled in with as
   many entries as will fit, and the size used is returned in ifc_len.
   The list is truncated if needed, with no indication.  Largest gap: 31.

   IRIX 6.5: The buffer is filled in with as many entries as will fit
   in N-1 bytes, and the size used is returned in ifc_len.  Providing
   exactly the desired number of bytes is inadequate; the buffer must
   be *bigger* than needed.  (E.g., 32->0, 33->32.)  The returned
   ifc_len is always less than the supplied one.  Largest gap: 32.

   AIX 4.3.3: Sometimes the returned ifc_len is bigger than the
   supplied one, but it may not be big enough for *all* the
   interfaces.  Sometimes it's smaller than the supplied value, even
   if the returned list is truncated.  The list is filled in with as
   many entries as will fit; no overrun.  Largest gap: 143.

   Older AIX: We're told by W. David Shambroom <DShambroom@gte.com> in
   PR krb5-kdc/919 that older versions of AIX have a bug in the
   SIOCGIFCONF ioctl which can cause them to overrun the supplied
   buffer.  However, we don't yet have details as to which version,
   whether the overrun amount was bounded (e.g., one ifreq's worth) or
   not, whether it's a real buffer overrun or someone assuming it was
   because ifc_len was increased, etc.  Once we've got details, we can
   try to work around the problem.

   Digital UNIX 4.0F: If input ifc_len is zero, return an ifc_len
   that's big enough to include all entries.  (Actually, on our
   system, it appears to be larger than that by 32.)  If input ifc_len
   is nonzero, fill in as many entries as will fit, and set ifc_len
   accordingly.  (Tested only with INIT of zero.)

   So... if the returned ifc_len is bigger than the supplied one,
   we'll need at least that much space -- but possibly more -- to hold
   all the results.  If the returned value is smaller or the same, we
   may still need more space.

   Using this ioctl is going to be messy.  Let's just hope that
   getifaddrs() catches on quickly....  */

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

#if (defined(sun) || defined(__sun__)) && !defined(SIOCGIFCONF)
/* Sun puts socket ioctls in another file.  */
#include <sys/sockio.h>
#endif

#define INIT 0xc3

int
main(void)
{
    char buffer[2048];
    int i, sock, t, olen = -9, omod = -9;
    struct ifconf ifc;
    int gap = -1, lastgap = -1;

    sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror ("socket");
        exit (1);
    }
    printf ("sizeof(struct if_req)=%d\n", sizeof (struct ifreq));
    for (t = 0; t < sizeof (buffer); t++) {
        ifc.ifc_len = t;
        ifc.ifc_buf = buffer;
        memset (buffer, INIT, sizeof (buffer));
        i = ioctl (sock, SIOCGIFCONF, (char *) &ifc);
        if (i < 0) {
            /* Solaris returns "Invalid argument" if the buffer is too
               small.  AIX and Linux return no error indication.  */
            int e = errno;
            snprintf (buffer, sizeof(buffer), "SIOCGIFCONF(%d)", t);
            errno = e;
            perror (buffer);
            if (e == EINVAL)
                continue;
            fprintf (stderr, "exiting on unexpected error\n");
            exit (1);
        }
        i = sizeof (buffer) - 1;
        while (buffer[i] == ((char)INIT) && i >= 0)
            i--;
        if (omod != i) {
            /* Okay... the gap computed on the *last* iteration is the
               largest for that particular size of returned data.
               Save it, and then start computing gaps for the next
               bigger size of returned data.  If we never get anything
               bigger back, we discard the newer value and only keep
               LASTGAP because all we care about is how much slop we
               need to "prove" that there really weren't any more
               entries to be returned.  */
            if (gap > lastgap)
                lastgap = gap;
        }
        gap = t - i - 1;
        if (olen != ifc.ifc_len || omod != i) {
            printf ("ifc_len in = %4d, ifc_len out = %4d, last mod = %4d\n",
                    t, ifc.ifc_len, i);
            olen = ifc.ifc_len;
            omod = i;
        }
    }
    printf ("finished at ifc_len %d\n", t);
    printf ("largest gap = %d\n", lastgap);
    exit (0);
}
