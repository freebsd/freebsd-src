/*-
 * pccard noise interface.
 * Nate Williams, October 1997.
 * This file is in the public domain.
 */

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

static enum beepstate allow_beep = BEEP_OFF;

/*
 * timeout function to keep lots of noise from
 * happening with insertion/removals.
 */
static void enable_beep(void *dummy)
{
	/* Should never be needed */
	untimeout(enable_beep, (void *)NULL);

	allow_beep = 1;
}

void pccard_insert_beep(void)
{
	if (allow_beep == BEEP_ON) {
		sysbeep(PCCARD_BEEP_PITCH0, PCCARD_BEEP_DURATION0);
		allow_beep = 0;
		timeout(enable_beep, (void *)NULL, hz / 5);
	}
}

void pccard_remove_beep(void)
{
	if (allow_beep == BEEP_ON) {
		sysbeep(PCCARD_BEEP_PITCH0, PCCARD_BEEP_DURATION0);
		allow_beep = 0;
		timeout(enable_beep, (void *)NULL, hz / 5);
	}
}

void pccard_success_beep(void)
{
	sysbeep(PCCARD_BEEP_PITCH1, PCCARD_BEEP_DURATION1);
}

void pccard_failure_beep(void)
{
	sysbeep(PCCARD_BEEP_PITCH2, PCCARD_BEEP_DURATION2);
}

void pccard_beep_select(enum beepstate state)
{
	allow_beep = state;
}
