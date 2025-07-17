/* ntp_clockdev.h - map clock instances to devices
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 * ---------------------------------------------------------------------
 * The runtime support for the 'device' configuration statement.  Just a
 * simple list to map refclock source addresses to the device(s) to use
 * instead of the builtin names.
 * ---------------------------------------------------------------------
 */
#ifndef NTP_CLOCKDEV_H
#define NTP_CLOCKDEV_H

extern void clockdev_clear(void);

extern int clockdev_remove(
	const sockaddr_u *addr_sock);

extern int clockdev_update(
	const sockaddr_u *addr_sock, const char *ttyName, const char *ppsName);

extern const char *clockdev_lookup(
	const sockaddr_u * addr_sock, int getPps);

#endif /*!defined(NTP_CLOCKDEV_H)*/
