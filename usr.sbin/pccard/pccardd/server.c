/*
 *	pccardd UNIX-domain socket interface
 *	Copyright (C) 1996 by Tatsumi Hosokawa <hosokawa@mt.cs.keio.ac.jp>
 *
 * $Id: server.c,v 1.3 1999/02/07 08:02:44 kuriyama Exp $
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <setjmp.h>

#include "cardd.h"

static void
cardnum(char *buf)
{
	int     i = 0;
	struct slot *sp;

	for (sp = slots; sp; sp = sp->next)
		i++;
	if (i > MAXSLOT)
		i = MAXSLOT;
	sprintf(buf, "%2d", i);
}

static struct slot *
find_slot(int slot)
{
	struct slot *sp;

	/* Search the list until we find the slot or get to the end */
	for (sp = slots; sp && sp->slot != slot; sp = sp->next)
		continue;

	return ( sp );
}

static void
cardname(char *buf, int slot)
{
	struct slot *sp;
	char   *manuf, *vers, *drv, *stat;

	/* Look for the slot */
	if ( (sp = find_slot(slot)) == NULL)
		return;

	/* Fill in the information in the buff */
	if (sp->cis) {

		manuf = sp->cis->manuf;
		vers = sp->cis->vers;
		if (sp->config && sp->config->driver &&
		    sp->config->driver->name)
			drv = sp->config->driver->name;
		else
			drv = "";
	} else
		manuf = vers = drv = "";

	switch (sp->state) {
	case empty:
		stat = "0";
		break;
	case filled:
		stat = "1";
		break;
	case inactive:
		stat = "2";
		break;
	default:
		stat = "9";
	}
	sprintf(buf, "%d~%s~%s~%s~%s", slot, manuf, vers, drv, stat);
}

static void
cardpwr(int slot, int pwon)
{
	struct slot *sp;

	/* Look for the slot */
	if ( (sp = find_slot(slot)) == NULL)
		return;

	if (ioctl(sp->fd, PIOCSVIR, &pwon) < 0)
		logerr("invaild arguments for cardpwr");
}

static int sock = 0;
static int slen = 0;
static struct sockaddr_un sun;

void
set_socket(int s)
{
	sock = s;
}

void
stat_changed(struct slot *sp)
{
	int     len;
	char    buf[512];

	if (!slen)
		return;

	cardname(buf, sp->slot);
	len = strlen(buf);
	if (sendto(sock, buf, len, 0, (struct sockaddr *) & sun, slen) != len) {
		logerr("sendto failed");
		slen = 0;
	}
}

void
process_client(void)
{
	char    buf[512], obuf[512];
	int     len;
	int     snum;

	if (!sock)
		return;
	slen = sizeof(sun);
	len = recvfrom(sock, buf, sizeof(buf),
		       0, (struct sockaddr *)&sun, &slen);
	if (len < 0)
		logerr("recvfrom failed");
	buf[len] = '\0';
	obuf[0] = '\0';
	switch (buf[0]) {	/* Protocol implementation */
	case 'S':	/* How many slots? */
		cardnum(obuf);
		break;
	case 'N':	/* Card name request */
		sscanf(buf + 1, "%d", &snum);
		if (snum >= 0 && snum <= MAXSLOT)
			cardname(obuf, snum);
		else 
			logerr("Illegal slot requests for N command");
		break;
	case 'P':	/* Virtual insertion request */
		sscanf(buf + 1, "%d", &snum);
		if (snum >= 0 && snum <= MAXSLOT) {
			logmsg("slot %d: spring has come", snum);
			cardpwr(snum, 1);
		} else
			logerr("Illegal slot requests for P command");
		break;
	case 'Q':	/* Virtual removal request */
		sscanf(buf + 1, "%d", &snum);
		if (snum >= 0 && snum <= MAXSLOT) {
			logmsg("slot %d: hibernation", snum);
			cardpwr(snum, 0);
		} else
			logerr("Illegal slot requests for Q command");
		break;
	default:
		logerr("Unknown control message from socket");
		break;
	}
	len = strlen(obuf);
	if (len) {
		if (sendto(sock, obuf, len, 0, (struct sockaddr *)&sun, slen)
		    != len) {
			logerr("sendto failed");
			slen = 0;
		}
	} else if (sendto(sock, 0, 0, 0, (struct sockaddr *)&sun, slen)
		   != len) {
			logerr("sendto failed");
			slen = 0;
	}
}
