/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

DMenuItem documentation_items[] = {
{ "README", "Read this for a general description of FreeBSD",
      MENU_DISPLAY_FILE, (void *)"help/readme.hlp", 0 },
{ "Hardware", "The FreeBSD survival guide for PC hardware.",
      MENU_DISPLAY_FILE, (void *)"help/hardware.hlp", 0 },
{ "Install", "A step-by-step guide to installing FreeBSD.",
      MENU_DISPLAY_FILE, (void *)"help/install.hlp", 0 },
{ "FAQ", "Frequently Asked Questions about FreeBSD.",
      MENU_DISPLAY_FILE, (void *)"help/faq.hlp", 0 },
{ NULL },
};

DMenu MenuDocumentation = {
"Documentation for FreeBSD 2.0.5",	/* Title */
"Blah blah",				/* Prompt */
NULL,	/* No help line */
NULL,	/* No help file */
documentation_items,
};

DMenuItem language_items[] = {
{ "English", "The system default.",
      MENU_SET_VARIABLE, (void *)"LANG=en", 0 },
{ "French", "French language and character set (ISO-8859-1)",
      MENU_SET_VARIABLE, (void *)"LANG=fr", 0 },
{ "German", "German language and character set (ISO-8859-1)",
      MENU_SET_VARIABLE, (void *)"LANG=de", 0 },
{ "Japanese", "Japanese language and character set (JIS?)",
      MENU_SET_VARIABLE, (void *)"LANG=jp", 0 },
{ "Russian", "Russian language and character set (cp866-8x14)",
      MENU_SET_VARIABLE, (void *)"LANG=ru", 0 },
{ NULL },
};

DMenu MenuLanguage = {
"Set your preferred language",
"Blah blah",
NULL,
NULL,
language_items,
};

DMenuItem initial_items[] = {
{ "Usage", "Quick start - How to use this menu system.",	/* U */
      MENU_DISPLAY_FILE, (void *)"help/initial.hlp", 0 },
{ "Doc", "More detailed documentation on FreeBSD.",		/* D */
      MENU_SUBMENU, (void *)&MenuDocumentation, 0 },
{ "Lang", "Select natural language options.",			/* L */
      MENU_SUBMENU, (void *)&MenuLanguage, 0 },
{ "Express", "Express installation (don't ask)",		/* E */
      MENU_CALL, (void *)installExpress, 0 },
{ "Custom", "Custom installation (please ask)",			/* C */
      MENU_CALL, (void *)installCustom, 0 },
{ NULL },
};

/* The first menu */
DMenu MenuInitial = {
"Welcome to FreeBSD 2.0.5!",	/* title */
"This is the main menu of the FreeBSD installation system.  Please\n\
select one of the options below by using the arrow keys or typing the\n\
first character of the option name you're interested in.  Invoke an\n\
option by pressing enter.",	/* prompt */
"Press F1 for further help",	/* help line */
"help/initial.hlp",		/* help file */
initial_items,			/* items */
};
