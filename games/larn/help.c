/*	help.c		Larn is copyrighted 1986 by Noah Morgan. */
/* $FreeBSD$ */
#include "header.h"
/*
 *	help function to display the help info
 *
 *	format of the .larn.help file
 *
 *	1st character of file:	# of pages of help available (ascii digit)
 *	page (23 lines) for the introductory message (not counted in above)
 *	pages of help text (23 lines per page)
 */
extern char helpfile[];
help()
	{
	int i,j;
#ifndef VT100
	char tmbuf[128];	/* intermediate translation buffer when not a VT100 */
#endif VT100
	if ((j=openhelp()) < 0)  return;	/* open the help file and get # pages */
	for (i=0; i<23; i++) lgetl();	/* skip over intro message */
	for (;  j>0; j--)
		{
		clear();
		for (i=0; i<23; i++)
#ifdef VT100
			lprcat(lgetl());	/* print out each line that we read in */
#else VT100
			{ tmcapcnv(tmbuf,lgetl());  lprcat(tmbuf); } /* intercept \33's */
#endif VT100
		if (j>1)
			{
			lprcat("    ---- Press ");  standout("return");
			lprcat(" to exit, ");  standout("space");
			lprcat(" for more help ---- ");
			i=0; while ((i!=' ') && (i!='\n') && (i!='\33')) i=getchar();
			if ((i=='\n') || (i=='\33'))
				{
				lrclose();  setscroll();  drawscreen();  return;
				}
			}
		}
	lrclose();  retcont();  drawscreen();
	}

/*
 *	function to display the welcome message and background
 */
welcome()
	{
	int i;
#ifndef VT100
	char tmbuf[128];	/* intermediate translation buffer when not a VT100 */
#endif VT100
	if (openhelp() < 0)  return;   	/* open the help file */
	clear();
	for(i=0; i<23; i++)
#ifdef VT100
			lprcat(lgetl());	/* print out each line that we read in */
#else VT100
			{ tmcapcnv(tmbuf,lgetl());  lprcat(tmbuf); } /* intercept \33's */
#endif VT100
	lrclose();  retcont();	/* press return to continue */
	}

/*
 *	function to say press return to continue and reset scroll when done
 */
retcont()
	{
	cursor(1,24); lprcat("Press "); standout("return");
	lprcat(" to continue: ");   while (getchar() != '\n');
	setscroll();
	}

/*
 *	routine to open the help file and return the first character - '0'
 */
openhelp()
	{
	if (lopen(helpfile)<0)
		{
		lprintf("Can't open help file \"%s\" ",helpfile);
		lflush(); sleep(4);	drawscreen();	setscroll(); return(-1);
		}
	resetscroll();  return(lgetc() - '0');
	}

