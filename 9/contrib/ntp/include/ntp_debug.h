/*
 * $Header$
 *
 * $Created: Sat Aug 20 14:23:01 2005 $
 *
 * Copyright (C) 2005 by Frank Kardel
 */
#ifndef NTP_DEBUG_H
#define NTP_DEBUG_H

/*
 * macros for debugging output - cut down on #ifdef pollution in the code
 */

#ifdef DEBUG
#define DPRINTF(_lvl_, _arg_)				\
	do { 						\
		if (debug >= (_lvl_))			\
			printf _arg_;			\
	} while (0)
#else
#define DPRINTF(_lvl_, _arg_)	do {} while (0)
#endif

#endif
/*
 * $Log$
 */
