/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/usr.sbin/fwcontrol/fwmethods.h,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $
 */

typedef void (fwmethod)(int dev_fd, const char *filename, char ich, int count);
extern fwmethod dvrecv;
extern fwmethod dvsend;
extern fwmethod mpegtsrecv;
