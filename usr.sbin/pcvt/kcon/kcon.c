/*
 * Copyright (c) 1992,1993,1994 Hellmuth Michaelis
 *
 * Copyright (c) 1992,1993 Holger Veit.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to 386BSD by
 * Holger Veit
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Hellmuth Michaelis and Holger Veit
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

static char *id =
	"@(#)kcon.c, 3.20, Last Edit-Date: [Wed Jan 25 16:33:08 1995]";

/*---------------------------------------------------------------------------*
 *
 *	kcon.c		Keyboard control and remapping
 *	----------------------------------------------
 *
 *	based on "keymap" which was written by
 *	Holger Veit (veit@du9ds3.uni-duisburg.de)
 *
 *	-hm	a first rewrite
 *	-hm	rewrite for pcvt 2.0 distribution
 *	-hm	adding show current typematic values
 *	-hm	hex/octal/esc output choices
 *	-hm	remapping debugging
 *	-hm	garbage output for remapped keys bugfix
 *	-hm	patch from Lon Willet, adding -R
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <machine/pcvt_ioctl.h>

#include "keycap.h"

int Rf = 0;
int df = 0;
int lf = 0;
int mf = 0;
int of = 0;
int pf = 0;
int rf = 0;
int tf = 0;
int xf = 0;
int sf = 0;

/*---------------------------------------------------------------------------*
 *	main entry
 *---------------------------------------------------------------------------*/
main(argc, argv)
int argc;
char *argv[];
{
	extern char *optarg;
	extern int optind;

	int c = 0;

	int errf = 0;

	int rate = -1;
	int delay = -1;
	char *map;
	int kbfd;

	while((c = getopt(argc, argv, "Rd:lm:opr:st:x")) != -1)
	{
		switch(c)
		{
			case 'R':
				Rf = 1;
				break;

			case 'd':
				df = 1;
				delay = atoi(optarg);
				break;

			case 'l':
				lf = 1;
				break;

			case 'm':
				mf = 1;
				map = optarg;
				break;

			case 'o':
				if(xf)
					errf = 1;
				else
					of = 1;
				break;

			case 'p':
				pf = 1;
				break;

			case 'r':
				rf = 1;
				rate = atoi(optarg);
				break;

			case 's':
				sf = 1;
				break;

			case 't':
				if(*optarg == '+')
					tf = 1;
				else if(*optarg == '-')
					tf = -1;
				else
					errf = 1;
				break;

			case 'x':
				if(of)
					errf = 1;
				else
					xf = 1;
				break;

			default:
				usage();
		}
	}

	if((Rf == 0 && df == 0 && lf == 0 && tf == 0 && sf == 0 &&
	    rf == 0 && mf == 0 ) || errf)
	{
		usage();
	}

	if((kbfd = open(KEYB_DEVICE, 0)) < 0)
	{
		perror("kcon: keyboard open failiure");
		exit(1);
	}

	if(sf)
	{
		showtypeamatic(kbfd);
		exit(0);
	}

	if(lf)
	{
		listcurrent(kbfd);
		exit(0);
	}

	if (Rf)
	{
		if (ioctl(kbfd, KBDRESET, 0) < 0) {
			perror ("kcon: ioctl KBDRESET failed");
			exit (1);
		}
	}

	if(tf)
	{
		setrepeat(kbfd, tf);
	}

	if(df || rf)
	{
		if(delay > 3)
		{
			fprintf(stderr,"Delay value (%d) out of range, possible values are 0..3!\n",delay);
			exit(1);
		}
		if(rate > 31)
		{
			fprintf(stderr,"Rate value (%d) out of range, possible values are 0..31!\n",rate);
			exit(1);
		}
		settypeam(kbfd, delay, rate);
	}

	if(mf)
	{
		remapkeys(kbfd, map);
	}

	close(kbfd);
	exit(0);
}

/*---------------------------------------------------------------------------*
 *	display usage info & exit
 *---------------------------------------------------------------------------*/
usage()
{
	fprintf(stderr, "\nkcon: keyboard control and remapping utility for pcvt video driver\n");
	fprintf(stderr, "usage: [-R] [-d delay] [-l] [-m map] [-o] [-p] [-r rate] [-t +/-] [-x]\n");
	fprintf(stderr, "       -R   full reset of keyboard\n");
	fprintf(stderr, "       -d   delay until a key is repeated (range: 0...3 => 250...1000ms)\n");
	fprintf(stderr, "       -l   produce listing of current keyboard mapping\n");
	fprintf(stderr, "       -m   set keyboard remapping from a keycap entry\n");
	fprintf(stderr, "       -o   set octal output for listing\n");
	fprintf(stderr, "       -p   pure, don't display escape as 'ESC' for listing\n");
	fprintf(stderr, "       -r   chars/second repeat value (range: 0...31 => 30...2 chars/sec)\n");
	fprintf(stderr, "       -s   show, display the current keyboard typematic values\n");
	fprintf(stderr, "       -t   switch repeat on(+) or off(-)\n");
	fprintf(stderr, "       -x   set hexadecimal output for listing\n\n");
	exit(1);
}

/*---------------------------------------------------------------------------*
 *	convert control char in string to printable values
 *---------------------------------------------------------------------------*/
char *showcntrl(s)
u_char *s;
{
	static char res_str[80];
	static char conv_buf[80];
	int i;

	res_str[0] = '\0';

	for(i = 0; s[i]; i++)
	{
		if(((s[i] > 0x20) && (s[i] <= 0x7e)) || ((s[i] >= 0xa0) && (s[i] <= 0xff)))
		{
			conv_buf[0] = s[i];
			conv_buf[1] = '\0';
		}
		else if((s[i] == 0x1b) && (pf == 0))
		{
			strcpy(conv_buf,"ESC ");
		}
		else if(of)
		{
			sprintf(conv_buf,"\\%03.3o ", s[i]);
		}
		else
		{
			sprintf(conv_buf,"0x%02.2X ", s[i]);
		}
		strcat(res_str, conv_buf);
	}
	return(res_str);
}

/*---------------------------------------------------------------------------*
 *	list the current keyboard mapping
 *---------------------------------------------------------------------------*/
listcurrent(kbfd)
int kbfd;
{
	static char *keytypetab[] = {
		"NONE     ",
		"SHIFT    ",
		"ALT/META ",
		"NUMLOCK  ",
		"CONTROL  ",
		"CAPSLOCK ",
		"ASCII    ",
		"SCROLL   ",
		"FUNCTION ",
		"KEYPAD   ",
		"BREAK    ",
		"ALTGR    ",
		"SHIFTLOCK",
		"CURSOR   ",
		"RETURN   "
	};

	struct kbd_ovlkey keyboardmap[KBDMAXKEYS];
	struct kbd_ovlkey *kbmapp;
	int keytype;
	int altgr_defined;
	int i;

	altgr_defined = 0;
	kbmapp = keyboardmap;

	for (i = 0; i < KBDMAXKEYS; i++)
	{
		kbmapp->keynum = i;

		if(ioctl(kbfd, KBDGCKEY, kbmapp) < 0)
		{
			perror("kcon: ioctl KBDGCKEY failed");
			exit(1);
		}

		if((kbmapp->type & KBD_MASK) == KBD_ALTGR)
			altgr_defined = i;

		kbmapp++;
	}

	if(altgr_defined)
	{
		printf("S Key KeyType   Normal          Shift           Control         Altgr          \n");
		printf("- --- --------- --------------- --------------- --------------- ---------------\n");
	}
	else
	{
		printf("S Key KeyType   Normal          Shift           Control        \n");
		printf("- --- --------- --------------- --------------- ---------------\n");
	}

	kbmapp = &keyboardmap[1];

	for(i = 1; i < KBDMAXKEYS; i++)
	{
		keytype = kbmapp->type;

		if(keytype)
		{
			if(keytype & KBD_OVERLOAD)
				printf("! %3.3d %9.9s ", i, keytypetab[keytype & KBD_MASK]);
			else
				printf("- %3.3d %9.9s ", i, keytypetab[keytype & KBD_MASK]);

			switch(keytype & KBD_MASK)
			{

				case KBD_NUM:
				case KBD_ASCII:
				case KBD_FUNC:
				case KBD_KP:
				case KBD_CURSOR:
				case KBD_RETURN: /* ??? */

					if(kbmapp->subu == KBD_SUBT_STR)
						printf("%-15s ",showcntrl(kbmapp->unshift));
					else
						printf("Function()      ");

					if(kbmapp->subs == KBD_SUBT_STR)
						printf("%-15s ",showcntrl(kbmapp->shift));
					else
						printf("Function()      ");

					if(kbmapp->subc == KBD_SUBT_STR)
						printf("%-15s ",showcntrl(kbmapp->ctrl));
					else
						printf("Function()      ");

					if(altgr_defined)
					{
						if(kbmapp->suba == KBD_SUBT_STR)
							printf("%-15s ",showcntrl(kbmapp->altgr));
						else
							printf("Function()      ");
					}
					break;
			}
			putchar('\n');
		}
		kbmapp++;
	}
}

/*---------------------------------------------------------------------------*
 *	show delay and rate values for keyboard
 *---------------------------------------------------------------------------*/
showtypeamatic(kbfd)
int kbfd;
{
	static char *delaytab[] = {
		"250",
		"500",
		"750",
		"1000"
	};

	static char *ratetab[] = {
		"30.0",
		"26.7",
		"24.0",
		"21.8",
		"20.0",
		"18.5",
		"17.1",
		"16.0",
		"15.0",
		"13.3",
		"12.0",
		"10.9",
		"10.0",
		"9.2",
		"8.6",
		"8.0",
		"7.5",
		"6.7",
		"6.0",
		"5.5",
		"5.0",
		"4.6",
		"4.3",
		"4.0",
		"3.7",
		"3.3",
		"3.0",
		"2.7",
		"2.5",
		"2.3",
		"2.1",
		"2.0"
	};

	int cur_typemat_val;
	int delay, rate;

	if((ioctl(kbfd, KBDGTPMAT, &cur_typemat_val)) < 0)
	{
		perror("kcon: ioctl KBDGTPMAT failed");
		exit(1);
	}

	delay = ((cur_typemat_val & 0x60) >> 5);
	rate = cur_typemat_val & 0x1f;

	printf("\nDisplaying the current keyboard typematic values:\n\n");
	printf("The delay-until-repeat time is [ %s ] milliseconds\n",delaytab[delay]);
	printf("The repeat-rate is [ %s ] characters per second\n\n",ratetab[rate]);
}

/*---------------------------------------------------------------------------*
 *	set repeat feature on/off
 *---------------------------------------------------------------------------*/
setrepeat(kbfd, tf)
int kbfd;
int tf;
{
	int	srepsw_val;

	if(tf == 1)
		srepsw_val = KBD_REPEATON;
	else
		srepsw_val = KBD_REPEATOFF;

	if(ioctl(kbfd, KBDSREPSW, &srepsw_val) < 0)
	{
		perror("kcon: ioctl KBDREPSW failed");
		exit(1);
	}
}

/*---------------------------------------------------------------------------*
 *	set delay and rate values for keyboard
 *---------------------------------------------------------------------------*/
settypeam(kbfd, delay, rate)
int kbfd;
int delay;
int rate;
{
	int cur_typemat_val;
	int new_typemat_val;

	if((ioctl(kbfd, KBDGTPMAT, &cur_typemat_val)) < 0)
	{
		perror("kcon: ioctl KBDGTPMAT failed");
		exit(1);
	}

	if(delay == -1)
		delay = (cur_typemat_val & 0x60);
	else
		delay = ((delay << 5) & 0x60);

	if(rate == -1)
		rate = (cur_typemat_val & 0x1f);
	else
		rate &= 0x1f;

	new_typemat_val = delay | rate;

	if((ioctl(kbfd, KBDSTPMAT, &new_typemat_val)) < 0)
	{
		perror("kcon: ioctl KBDSTPMAT failed");
		exit(1);
	}
}

/*---------------------------------------------------------------------------*
 *	remap keyboard from keycap entry
 *---------------------------------------------------------------------------*/
remapkeys(kbfd, map)
int kbfd;
char *map;
{
	char cap_entry[1024];
	int ret;
	char keyflag[128];
	int i;

	/* try to find the entry */

	ret = kgetent(cap_entry, map);

	if(ret == -1)
	{
		fprintf(stderr, "kcon: keycap database not found or not accessible!\n");
		exit(1);
	}
	else if(ret == 0)
	{
		fprintf(stderr, "kcon: keycap entry [%s] not found in database!\n", map);
		exit(1);
	}

	/* set default mapping */

	if((ioctl(kbfd, KBDDEFAULT)) < 0)
	{
		perror("kcon: ioctl KBDDEFAULT failed");
		exit(1);
	}

	/* DE flag present? */

	if(kgetflag("de"))
		return;

	for(i = 0; i < KBDMAXKEYS; i++)
		keyflag[i] = 0;

	set_lock(keyflag, kbfd);

	set_shift(keyflag, kbfd);

	set_char(keyflag, kbfd);
}

/*---------------------------------------------------------------------------*
 *	care for lock keys
 *---------------------------------------------------------------------------*/
set_lock(keyflag, kbfd)
char keyflag[];
int kbfd;
{
	int i, j;
	char cap[16];
	struct kbd_ovlkey entry;

	struct	{
		char	*ch;
		u_short	typ;
	} lock[] =
	{
		"ca",	KBD_CAPS,
		"sh",	KBD_SHFTLOCK,
		"nl",	KBD_NUMLOCK,
		"sc",	KBD_SCROLL
	};


	for(i = 0; i < 4; i++)
	{
		int n;

		sprintf(cap, "%s", lock[i].ch);

		n = kgetnum(cap);

		if(n > 0)
		{
			if (keyflag[n])
			{
				fprintf(stderr,"kcon: duplicate key definition for key [%d]!\n",n);
				exit(1);
			}
			keyflag[n] = 1;

			entry.keynum = n;
			entry.type = lock[i].typ;

			if((ioctl(kbfd, KBDSCKEY, &entry)) < 0)
			{
				perror("kcon: ioctl KBDSCKEY failed");
				exit(1);
			}
		}
	}
}

/*---------------------------------------------------------------------------*
 *	care for shifting keys
 *---------------------------------------------------------------------------*/
set_shift(keyflag, kbfd)
char keyflag[];
int kbfd;
{
	int i, j;
	char cap[16];
	struct kbd_ovlkey entry;

	struct {
		char	ch;
		u_short	typ;
	} shift[] =
	{
		'm',	KBD_META,
		'l',	KBD_ALTGR,
		'h',	KBD_SHIFT,
		't',	KBD_CTL
	};

	for(i = 0; i < 4; i++)
	{
		for(j = 1; j < 10; j++)
		{
			int n;

			sprintf(cap, "%c%d", shift[i].ch,j);

			n = kgetnum(cap);

			if (n >= 0)
			{
				if (keyflag[n])
				{
					fprintf(stderr,"kcon: duplicate key definition for key [%d]!\n",n);
					exit(1);
				}
				keyflag[n] = 1;

				entry.keynum = n;
				entry.type = shift[i].typ;
				if((ioctl(kbfd, KBDSCKEY, &entry)) < 0)
				{
					perror("kcon: ioctl KBDSCKEY failed");
					exit(1);
				}
			}
		}
	}
}

/*---------------------------------------------------------------------------*
 *	care for normal keys
 *---------------------------------------------------------------------------*/
set_char(keyflag, kbfd)
char keyflag[];
int kbfd;
{
	int i, j;
	char cap[16];
	int setflag;
	char *addr_str;
	char *new_str;
	struct kbd_ovlkey entry;

	struct {
		char	*addr;
		char	ch;
	} standard[] = {
		0,			'D',
		&entry.unshift[0],	'K',
		&entry.shift[0],	'S',
		&entry.ctrl[0],		'C',
		&entry.altgr[0],	'A'
	};

	for(i = 1; i < KBDMAXKEYS; i++)
	{
		setflag = 0;

		entry.keynum = i;

		if((ioctl(kbfd, KBDGOKEY, &entry)) < 0)
		{
			perror("kcon: ioctl KBDGOKEY failed");
			exit(1);
		}

		entry.type = KBD_ASCII;

		for(j = 0; j < 5; j++)
		{
			sprintf(cap, "%c%d", standard[j].ch,i);

			if((j == 0) && (kgetflag(cap)))
			{
				/* delete a key */

				entry.type = KBD_NONE;
				setflag = 1;
				goto setit;

			}
			else
			{
				addr_str = standard[j].addr;
				if(new_str = kgetstr(cap, &addr_str))
				{
					if(strlen(new_str) > KBDMAXOVLKEYSIZE)
					{
						fprintf(stderr, "kcon: database entry string [%s] longer than max [%d]!\n",new_str,KBDMAXOVLKEYSIZE);
						exit(1);
					}
					setflag = 1;
				}
			}
		}

setit:		if (setflag)
		{
			if (keyflag[i])
			{
				fprintf(stderr,"kcon: duplicate key definition for key [%d]!\n",i);
				exit(1);
			}
			keyflag[i] = 1;

			if((ioctl(kbfd, KBDSCKEY, &entry)) < 0)
			{
				perror("kcon: ioctl KBDSCKEY failed");
				exit(1);
			}
		}
	}
}

/*------------------- EOF ------------------------------------------------*/
