
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	beep.c
 *
 *	Routines beep() and flash()
 *
 */

#include "curses.priv.h"
#include "terminfo.h"

/*
 *	beep()
 *
 *	Sound the current terminal's audible bell if it has one.   If not,
 *	flash the screen if possible.
 *
 */

int beep()
{
	T(("beep() called"));

	/* should make sure that we are not in altchar mode */
	if (bell)
		return(putp(bell));
	else if (flash_screen)
		return(putp(flash_screen));
	else
		return(ERR);
}

/*
 *	flash()
 *
 *	Flash the current terminal's screen if possible.   If not,
 *	sound the audible bell if one exists.
 *
 */

int flash()
{
	T(("flash() called"));

	/* should make sure that we are not in altchar mode */
	if (flash_screen)
		return(putp(flash_screen));
	else if (bell)
		return(putp(bell));
	else
		return(ERR);
}
