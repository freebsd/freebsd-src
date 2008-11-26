/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/usr.sbin/fwcontrol/fwmethods.h,v 1.1.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $
 */

typedef void (fwmethod)(int dev_fd, const char *filename, char ich, int count);
extern fwmethod dvrecv;
extern fwmethod dvsend;
extern fwmethod mpegtsrecv;
