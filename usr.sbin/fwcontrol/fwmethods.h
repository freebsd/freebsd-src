/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/usr.sbin/fwcontrol/fwmethods.h,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $
 */

typedef void (fwmethod)(int dev_fd, const char *filename, char ich, int count);
extern fwmethod dvrecv;
extern fwmethod dvsend;
extern fwmethod mpegtsrecv;
