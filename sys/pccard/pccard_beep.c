/*-
 * pccard noise interface.
 * Nate Williams, October 1997.
 * This file is in the public domain.
 */
/* $FreeBSD: src/sys/pccard/pccard_beep.c,v 1.3 1999/12/02 19:46:41 imp Exp $ */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/clock.h>

#include <pccard/driver.h>

#define	PCCARD_BEEP_PITCH0	1600
#define	PCCARD_BEEP_DURATION0	20
#define	PCCARD_BEEP_PITCH1	1200
#define	PCCARD_BEEP_DURATION1	40
#define	PCCARD_BEEP_PITCH2	3200
#define	PCCARD_BEEP_DURATION2	40

static struct callout_handle beeptimeout_ch
    = CALLOUT_HANDLE_INITIALIZER(&beeptimeout_ch);

static enum beepstate allow_beep = BEEP_OFF;

/*
 * timeout function to keep lots of noise from
 * happening with insertion/removals.
 */
static void enable_beep(void *dummy)
{
	/* Should never be needed */
	untimeout(enable_beep, (void *)NULL, beeptimeout_ch);

	allow_beep = BEEP_ON;
}

void pccard_insert_beep(void)
{
	if (allow_beep == BEEP_ON) {
		sysbeep(PCCARD_BEEP_PITCH0, PCCARD_BEEP_DURATION0);
		allow_beep = BEEP_OFF;
		beeptimeout_ch = timeout(enable_beep, (void *)NULL, hz / 5);
	}
}

void pccard_remove_beep(void)
{
	if (allow_beep == BEEP_ON) {
		sysbeep(PCCARD_BEEP_PITCH0, PCCARD_BEEP_DURATION0);
		allow_beep = BEEP_OFF;
		beeptimeout_ch = timeout(enable_beep, (void *)NULL, hz / 5);
	}
}

void pccard_success_beep(void)
{
	if (allow_beep == BEEP_ON) {
		sysbeep(PCCARD_BEEP_PITCH1, PCCARD_BEEP_DURATION1);
	}
}

void pccard_failure_beep(void)
{
	if (allow_beep == BEEP_ON) {
		sysbeep(PCCARD_BEEP_PITCH2, PCCARD_BEEP_DURATION2);
	}
}

int pccard_beep_select(enum beepstate state)
{
	if (state == BEEP_ON || state == BEEP_OFF) {
		allow_beep = state;
		return 0;
	}
	return 1;
}
