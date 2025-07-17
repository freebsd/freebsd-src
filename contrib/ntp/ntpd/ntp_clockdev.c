/* ntp_clockdev.c - map clock instances to devices
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 * ---------------------------------------------------------------------
 * The runtime support for the 'device' configuration statement.  Just a
 * simple list to map refclock source addresses to the device(s) to use
 * instead of the builtin names.
 * ---------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_NETINFO
# include <netinfo/ni.h>
#endif

#include <stdio.h>
#include <isc/net.h>

#include "ntp.h"
#include "ntpd.h"
#include "ntp_clockdev.h"

/* In the windows port 'refclock_open' is in 'libntp' (windows specific
 * 'termios.c' source) and calling a function located in NTPD from the
 * library is not something we should do.  Therefore 'termios.c' now
 * provides a hook to set a callback function used for the lookup, and
 * we have to populate that when we have indeed device name
 * redirections...
 */
#ifdef SYS_WINNT
extern const char * (*termios_device_lookup_func)(const sockaddr_u*, int);
#endif

/* What we remember for a device redirection */
typedef struct DeviceInfoS DeviceInfoT;
struct DeviceInfoS {
	DeviceInfoT *next;	/* link to next record		*/
	int	     ident;	/* type (byte1) and unit (byte0)*/
	char	    *ttyName;	/* time data IO device		*/
	char	    *ppsName;	/* PPS device			*/
};

/* Our list of device redirections: */
static DeviceInfoT * InfoList = NULL;

/* Free a single record: */
static void freeDeviceInfo(
	DeviceInfoT *item
	)
{
	if (NULL != item) {
		free(item->ttyName);
		free(item->ppsName);
		free(item);
	}
}

/* Get clock ID from pseudo network address. Returns -1 on error. */
static int
getClockIdent(
	const sockaddr_u *srcadr
	)
{
	int clkType, clkUnit;

	/*
	 * Check for valid address and running peer
	 */
	if (!ISREFCLOCKADR(srcadr))
		return -1;

	clkType = REFCLOCKTYPE(srcadr);
	clkUnit = REFCLOCKUNIT(srcadr);
	return (clkType << 8) + clkUnit;
}

/* Purge the complete redirection list. */
void
clockdev_clear(void)
{
	DeviceInfoT * item;
	while (NULL != (item = InfoList)) {
		InfoList = item->next;
		freeDeviceInfo(item);
	}
}

/* Remove record(s) for a clock.
 * returns number of removed records (maybe zero) or -1 on error
 */
int
clockdev_remove(
	const sockaddr_u *addr_sock
	)
{
	DeviceInfoT *item, **ppl;
	int	     rcnt  = 0;
	const int    ident = getClockIdent(addr_sock);

	if (ident < 0)
		return -1;

	ppl = &InfoList;
	while (NULL != (item = *ppl)) {
		if (ident == item->ident) {
			*ppl = item->next;
			freeDeviceInfo(item);
			++rcnt;
		} else {
			ppl = &item->next;
		}
	}
	return rcnt;
}

/* Update or create a redirection record for a clock instace */
int /*error*/
clockdev_update(
	const sockaddr_u *addr_sock,
	const char	 *ttyName,
	const char	 *ppsName
	)
{
	DeviceInfoT *item;
	const int   ident = getClockIdent(addr_sock);
	if (ident < 0)
		return EINVAL;

	/* make sure Windows can use device redirections, too: */
#   ifdef SYS_WINNT
	termios_device_lookup_func = clockdev_lookup;
#   endif

	/* try to update an existing record */
	for (item = InfoList;  NULL != item;  item = item->next)
		if (ident == item->ident) {
			msyslog(LOG_INFO, "Update IO devices for %s: timedata='%s' ppsdata='%s'",
				refnumtoa(addr_sock),
				ttyName ? ttyName : "(null)",
				ppsName ? ppsName : "(null)");
			free(item->ttyName);
			free(item->ppsName);
			item->ttyName = ttyName ? estrdup(ttyName) : NULL;
			item->ppsName = ppsName ? estrdup(ppsName) : NULL;
			return 0;
		}

	/* seems we have to create a new entry... */
	msyslog(LOG_INFO, "Add IO devices for %s: timedata='%s' ppsdata='%s'",
		refnumtoa(addr_sock),
		ttyName ? ttyName : "(null)",
		ppsName ? ppsName : "(null)");

	item = emalloc(sizeof(*item));
	item->next    = InfoList;
	item->ident   = ident;
	item->ttyName = ttyName ? estrdup(ttyName) : NULL;
	item->ppsName = ppsName ? estrdup(ppsName) : NULL;
	InfoList = item;
	return 0;
}

/* Lookup a redirection for a clock instance. Returns either the name
 * registered for the device or NULL if no redirection is found.
 */
const char*
clockdev_lookup(
	const sockaddr_u *addr_sock,
	int		  getPPS
	)
{
	const DeviceInfoT *item;
	const int	  ident = getClockIdent(addr_sock);

	if (ident < 0)
		return NULL;

	for (item = InfoList;  NULL != item;  item = item->next)
		if (ident == item->ident)
			return getPPS ? item->ppsName : item->ttyName;

	return NULL;
}
