/*-
 * pccard noise interface.
 * Nate Williams, October 1997.
 * This file is in the public domain.
 */
/* $FreeBSD$ */

#define OBSOLETE_IN_6

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/clock.h>

#include <pccard/driver.h>

static enum beepstate allow_beep = BEEP_OFF;
static int melody_type = 0;

#define MAX_TONE_MODE	3
#define MAX_STATE	4 

struct tone {
        int pitch;
        int duration;
};

static struct tone silent_beep[] = {
	{0, 0}
};

static struct tone success_beep[] = {
	{1200,   40}, {0, 0}
};
static struct tone failure_beep[] = {
	{3200,   40}, {0, 0}
};
static struct tone insert_remove_beep[] = {
	{1600,   20}, {0, 0}
};

static struct tone success_melody_beep[] = {
	{1200,    7}, {1000,    7}, { 800,   15}, {0, 0}
};
static struct tone failure_melody_beep[] = {
	{2000,    7}, {2400,    7}, {2800,   15}, {0, 0}
};
static struct tone insert_melody_beep[] = {
	{1600,   10}, {1200,    5}, {0, 0}
};
static struct tone remove_melody_beep[] = {
	{1200,   10}, {1600,    5}, {0, 0}
};

static struct tone *melody_table[MAX_TONE_MODE][MAX_STATE] = {
	{ /* silent mode */
		silent_beep, silent_beep, silent_beep, silent_beep,
	},
	{ /* simple beep mode */
		success_beep, failure_beep,
		insert_remove_beep, insert_remove_beep,
	},
	{ /* melody beep mode */
		success_melody_beep, failure_melody_beep,
		insert_melody_beep, remove_melody_beep,
	},
};


static void
pccard_beep_sub(void *arg)
{
	struct tone *melody;
	melody = (struct tone *)arg;

	if (melody->pitch != 0) {
		sysbeep(melody->pitch, (melody->duration * hz + 99) / 100);
		timeout(pccard_beep_sub, melody + 1,
		    (melody->duration * hz + 99) / 100);
	} else 
		allow_beep = BEEP_ON;
}

static void
pccard_beep_start(void *arg)
{
	struct tone *melody;
	melody = (struct tone *)arg;

	if (allow_beep == BEEP_ON && melody->pitch != 0) {
		allow_beep = BEEP_OFF;
		sysbeep(melody->pitch, (melody->duration * hz + 99) / 100);
		timeout(pccard_beep_sub, melody + 1,
		    (melody->duration * hz + 99) / 100);
	}
}

void
pccard_success_beep(void)
{
	pccard_beep_start(melody_table[melody_type][0]);
}

void
pccard_failure_beep(void)
{
	pccard_beep_start(melody_table[melody_type][1]);
}

void
pccard_insert_beep(void)
{
	pccard_beep_start(melody_table[melody_type][2]);
}

void
pccard_remove_beep(void)
{
	pccard_beep_start(melody_table[melody_type][3]);
}

int
pccard_beep_select(int type)
{
	int errcode = 0;

	if (type == 0)  {
		allow_beep = BEEP_OFF;
		melody_type = 0;
	} else if (type < 0 || MAX_TONE_MODE - 1 < type) {
		errcode = 1;
	} else {
		allow_beep = BEEP_ON;
		melody_type = type;
	}
	return (errcode);
}
