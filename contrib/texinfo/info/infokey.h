/* infokey.h -- Custom keystroke definition support.
   $Id: infokey.h,v 1.1 2002/03/20 16:03:22 karl Exp $

   Copyright (C) 1999, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Andrew Bettison <andrewb@zip.com.au>.

   This design was derived from the "lesskey" system in less 3.4.0. by
   Mark Nudelman.

   The following terminology is confusing:
	source file = $HOME/.infokey
	infokey file = $HOME/.info
   Oh, well.
 */


/* Default source file, where user writes text definitions to be
   compiled to the infokey file.  MS-DOS doesn't allow leading
   dots in file names.  */
#ifdef __MSDOS__
#define INFOKEY_SRCFILE		"_infokey"
#else
#define INFOKEY_SRCFILE		".infokey"
#endif

/* Default "infokey file", where compiled user defs are kept and
   read by Info.  MS-DOS doesn't allow leading dots in file names.  */
#ifdef __MSDOS__
#define INFOKEY_FILE		"_info"
#else
#define INFOKEY_FILE		".info"
#endif

/*
Format of entire infokey file:

	4 bytes		magic number S
	X bytes		version string
	1 byte '\0'	terminator

	any number of sections:
		1 byte		section id
		2 bytes		section length (N)
		N bytes		section definitions: format depends on section

	4 bytes		magic number E

Format of INFO and EA sections:
	
	1 byte		flag: 1 == suppress default key bindings
	Repeat:
		X bytes		key sequence
		1 byte '\0'	terminator
		1 byte		action code (A_xxx)

Format of VAR section:
	
	Repeat:
		X bytes		variable name
		1 byte '\0'	terminator
		Y bytes		value
		1 byte '\0'	terminator

*/

#define	INFOKEY_NMAGIC		8

#define	INFOKEY_MAGIC_S0	'\001'
#define	INFOKEY_MAGIC_S1	'I'
#define	INFOKEY_MAGIC_S2	'n'
#define	INFOKEY_MAGIC_S3	'f'

#define	INFOKEY_SECTION_INFO	'i'
#define	INFOKEY_SECTION_EA	'e'
#define	INFOKEY_SECTION_VAR	'v'

#define	INFOKEY_MAGIC_E0	'A'
#define	INFOKEY_MAGIC_E1	'l'
#define	INFOKEY_MAGIC_E2	'f'
#define	INFOKEY_MAGIC_E3	'n'

#define	INFOKEY_RADIX		64
#define	INFOKEY_MAX_SECTIONLEN	500
#define	INFOKEY_MAX_DEFLEN	16

#define	A_MAX_COMMAND		120
#define	A_INVALID		121

/* Character transformations (independent of info's own) */

#define	CONTROL(c)		((c) & 0x1f)
#define	ISCONTROL(c)		(((c) & ~0x1f) == 0)
#define	META(c)			((c) | 0x80)
#define	UNMETA(c)		((c) & ~0x80)
#define	ISMETA(c)		(((c) & 0x80) != 0)

/* Special keys (keys which output different strings on different terminals) */

#define SK_ESCAPE		CONTROL('k')
#define SK_RIGHT_ARROW		1
#define SK_LEFT_ARROW		2
#define SK_UP_ARROW		3
#define SK_DOWN_ARROW		4
#define SK_PAGE_UP		5
#define SK_PAGE_DOWN		6
#define SK_HOME			7
#define SK_END			8
#define SK_DELETE		9
#define SK_INSERT		10
#define SK_CTL_LEFT_ARROW	11
#define SK_CTL_RIGHT_ARROW	12
#define SK_CTL_DELETE		13
#define SK_LITERAL		40
