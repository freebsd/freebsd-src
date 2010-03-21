/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/usr.sbin/fwcontrol/fwmethods.h,v 1.1.14.1 2010/02/10 00:26:20 kensmith Exp $
 */

typedef void (fwmethod)(int dev_fd, const char *filename, char ich, int count);
extern fwmethod dvrecv;
extern fwmethod dvsend;
extern fwmethod mpegtsrecv;
