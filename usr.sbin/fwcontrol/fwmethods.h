/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/usr.sbin/fwcontrol/fwmethods.h,v 1.1.12.1.6.1 2010/12/21 17:09:25 kensmith Exp $
 */

typedef void (fwmethod)(int dev_fd, const char *filename, char ich, int count);
extern fwmethod dvrecv;
extern fwmethod dvsend;
extern fwmethod mpegtsrecv;
