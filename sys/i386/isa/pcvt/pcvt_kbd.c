/*
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore and Holger Veit.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * This code is derived from software contributed to 386BSD by
 * Holger Veit.
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
 *	This product includes software developed by Hellmuth Michaelis,
 *	Brian Dunford-Shore and Joerg Wunsch.
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
 *
 * @(#)pcvt_kbd.c, 3.20, Last Edit-Date: [Sun Apr  2 18:59:04 1995]
 *
 */

/*---------------------------------------------------------------------------*
 *
 *	pcvt_kbd.c	VT220 Driver Keyboard Interface Code
 *	----------------------------------------------------
 *	-hm	------------ Release 3.00 --------------
 *	-hm	integrating NetBSD-current patches
 *	-jw	introduced kbd_emulate_pc() if scanset > 1
 *	-hm	patch from joerg for timeout in kbd_emulate_pc()
 *	-hm	starting to implement alt-shift/ctrl key mappings
 *	-hm	Gateway 2000 Keyboard fix from Brian Moore
 *	-hm	some #if adjusting for NetBSD 0.9
 *	-hm	split off pcvt_kbd.h
 *	-hm	applying Joerg's patches for FreeBSD 2.0
 *	-hm	patch from Martin, PCVT_NO_LED_UPDATE
 *	-hm	PCVT_VT220KEYB patches from Lon Willet
 *	-hm	PR #399, patch from Bill Sommerfeld: Return with PCVT_META_ESC
 *	-hm	allow keyboard-less kernel boot for serial consoles and such ..
 *	-hm	patch from Lon Willett for led-update and showkey()
 *	-hm	patch from Lon Willett to fix mapping of Control-R scancode
 *	-hm	delay patch from Martin Husemann after port-i386 ml-discussion
 *	-hm	added PCVT_NONRESP_KEYB_TRY definition to doreset()
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#include "opt_ddb.h"

#if NVT > 0

#include "pcvt_hdr.h"		/* global include */

extern int kbd_response __P((void));

static void fkey1(void), fkey2(void),  fkey3(void),  fkey4(void);
static void fkey5(void), fkey6(void),  fkey7(void),  fkey8(void);
static void fkey9(void), fkey10(void), fkey11(void), fkey12(void);

static void sfkey1(void), sfkey2(void),  sfkey3(void),  sfkey4(void);
static void sfkey5(void), sfkey6(void),  sfkey7(void),  sfkey8(void);
static void sfkey9(void), sfkey10(void), sfkey11(void), sfkey12(void);

static void cfkey1(void), cfkey2(void),  cfkey3(void),  cfkey4(void);
static void cfkey5(void), cfkey6(void),  cfkey7(void),  cfkey8(void);
static void cfkey9(void), cfkey10(void), cfkey11(void), cfkey12(void);

static void	doreset ( void );
static void	ovlinit ( int force );
static void 	settpmrate ( int rate );
static void	setlockkeys ( int snc );
static int	kbc_8042cmd ( int val );
static int	getokeydef ( unsigned key, struct kbd_ovlkey *thisdef );
static int 	getckeydef ( unsigned key, struct kbd_ovlkey *thisdef );
static int	rmkeydef ( int key );
static int	setkeydef ( struct kbd_ovlkey *data );
static u_char *	xlatkey2ascii( U_short key );

static int	ledstate  = 0;	/* keyboard led's */
static int	tpmrate   = KBD_TPD500|KBD_TPM100;
static u_char	altkpflag = 0;
static u_short	altkpval  = 0;

#if PCVT_SHOWKEYS
u_char rawkeybuf[80];
#endif

#include "pcvt_kbd.h"		/* tables etc */

#if PCVT_SHOWKEYS
/*---------------------------------------------------------------------------*
 *	keyboard debugging: put kbd communication char into some buffer
 *---------------------------------------------------------------------------*/
static void showkey (char delim, u_char val)
{
	int rki;

	for(rki = 3; rki < 80; rki++)		/* shift left buffer */
		rawkeybuf[rki-3] = rawkeybuf[rki];

	rawkeybuf[77] = delim;		/* delimiter */

	rki = (val & 0xf0) >> 4;	/* ms nibble */

	if(rki <= 9)
		rki = rki + '0';
	else
		rki = rki - 10 + 'A';

	rawkeybuf[78] = rki;

	rki = val & 0x0f;		/* ls nibble */

	if(rki <= 9)
		rki = rki + '0';
	else
		rki = rki - 10 + 'A';

	rawkeybuf[79] = rki;
}
#endif	/* PCVT_SHOWKEYS */

/*---------------------------------------------------------------------------*
 *	function to switch to another virtual screen
 *---------------------------------------------------------------------------*/
static void
do_vgapage(int page)
{
	if(critical_scroll)		/* executing critical region ? */
		switch_page = page;	/* yes, auto switch later */
	else
		vgapage(page);		/* no, switch now */
}


/*
 * This code from Lon Willett enclosed in #if PCVT_UPDLED_LOSES_INTR is
 * abled because it crashes FreeBSD 1.1.5.1 at boot time.
 * The cause is obviously that the timeout queue is not yet initialized
 * timeout is called from here the first time.
 * Anyway it is a pointer in the right direction so it is included for
 * reference here.
 */

#define PCVT_UPDLED_LOSES_INTR	0	/* disabled for now */

#if PCVT_UPDLED_LOSES_INTR

/*---------------------------------------------------------------------------*
 *	check for lost keyboard interrupts
 *---------------------------------------------------------------------------*/

/*
 * The two commands to change the LEDs generate two KEYB_R_ACK responses
 * from the keyboard, which aren't explicitly checked for (maybe they
 * should be?).  However, when a lot of other I/O is happening, one of
 * the interrupts sometimes gets lost (I'm not sure of the details of
 * how and why and what hardware this happens with).
 *
 * This is a real problem, because normally the keyboard is only polled
 * by pcrint(), and no more interrupts will be generated until the ACK
 * has been read.  So the keyboard is hung.  This code polls a little
 * while after changing the LEDs to make sure that this hasn't happened.
 *
 * XXX Quite possibly we should poll the kbd on a regular basis anyway,
 * in the interest of robustness.  It may be possible that interrupts
 * get lost other times as well.
 */

static int lost_intr_timeout_queued = 0;

static void
check_for_lost_intr (void *arg)
{
	lost_intr_timeout_queued = 0;
	if (inb(CONTROLLER_CTRL) & STATUS_OUTPBF)
	{
		int opri = spltty ();
		pcrint ();
		splx (opri);
	}
}

#endif /* PCVT_UPDLED_LOSES_INTR */

/*---------------------------------------------------------------------------*
 *	update keyboard led's
 *---------------------------------------------------------------------------*/
void
update_led(void)
{

#if !PCVT_NO_LED_UPDATE

	/* Don't update LED's unless necessary. */

	int new_ledstate = ((vsp->scroll_lock) |
			    (vsp->num_lock * 2) |
			    (vsp->caps_lock * 4));

	if (new_ledstate != ledstate)
	{
		if(kbd_cmd(KEYB_C_LEDS) != 0)
		{
			printf("Keyboard LED command timeout\n");
			return;
		}

		if(kbd_cmd(new_ledstate) != 0) {
			printf("Keyboard LED data timeout\n");
			return;
		}

		ledstate = new_ledstate;

#if PCVT_UPDLED_LOSES_INTR
		if (lost_intr_timeout_queued)
			untimeout (check_for_lost_intr, (void *)NULL);

		timeout (check_for_lost_intr, (void *)NULL, hz);
		lost_intr_timeout_queued = 1;
#endif /* PCVT_UPDLED_LOSES_INTR */

	}
#endif /* !PCVT_NO_LED_UPDATE */
}

/*---------------------------------------------------------------------------*
 *	set typematic rate
 *---------------------------------------------------------------------------*/
static void
settpmrate(int rate)
{
	tpmrate = rate & 0x7f;
	if(kbd_cmd(KEYB_C_TYPEM) != 0)
		printf("Keyboard TYPEMATIC command timeout\n");
	else if(kbd_cmd(tpmrate) != 0)
		printf("Keyboard TYPEMATIC data timeout\n");
}

/*---------------------------------------------------------------------------*
 *	Pass command to keyboard controller (8042)
 *---------------------------------------------------------------------------*/
static int
kbc_8042cmd(int val)
{
	unsigned timeo;

	timeo = 100000; 	/* > 100 msec */
	while (inb(CONTROLLER_CTRL) & STATUS_INPBF)
		if (--timeo == 0)
			return (-1);
	outb(CONTROLLER_CTRL, val);
	return (0);
}

/*---------------------------------------------------------------------------*
 *	Pass command to keyboard itself
 *---------------------------------------------------------------------------*/
int
kbd_cmd(int val)
{
	unsigned timeo;

	timeo = 100000; 	/* > 100 msec */
	while (inb(CONTROLLER_CTRL) & STATUS_INPBF)
		if (--timeo == 0)
			return (-1);
	outb(CONTROLLER_DATA, val);

#if PCVT_SHOWKEYS
	showkey ('>', val);
#endif	/* PCVT_SHOWKEYS */

	return (0);
}

/*---------------------------------------------------------------------------*
 *	Read response from keyboard
 *	NB: make sure to call spltty() before kbd_cmd(), kbd_response().
 *---------------------------------------------------------------------------*/
int
kbd_response(void)
{
	u_char ch;
	unsigned timeo;

	timeo = 500000; 	/* > 500 msec (KEYB_R_SELFOK requires 87) */
	while (!(inb(CONTROLLER_CTRL) & STATUS_OUTPBF))
		if (--timeo == 0)
			return (-1);

	PCVT_KBD_DELAY();		/* 7 us delay */
	ch = inb(CONTROLLER_DATA);

#if PCVT_SHOWKEYS
	showkey ('<', ch);
#endif	/* PCVT_SHOWKEYS */

	return ch;
}

#if PCVT_SCANSET > 1
/*---------------------------------------------------------------------------*
 *	switch PC scan code emulation mode
 *---------------------------------------------------------------------------*/
void
kbd_emulate_pc(int do_emulation)
{
	int cmd, timeo = 10000;

	cmd = COMMAND_SYSFLG|COMMAND_IRQEN; /* common base cmd */

#if !PCVT_USEKBDSEC
	cmd |= COMMAND_INHOVR;
#endif

	if(do_emulation)
		cmd |= COMMAND_PCSCAN;

	kbc_8042cmd(CONTR_WRITE);
	while (inb(CONTROLLER_CTRL) & STATUS_INPBF)
		if (--timeo == 0)
			break;
	outb(CONTROLLER_DATA, cmd);
}

#endif /* PCVT_SCANSET > 1 */


#ifndef PCVT_NONRESP_KEYB_TRY
#define PCVT_NONRESP_KEYB_TRY	25	/* no of times to try to detect	*/
#endif					/* a nonresponding keyboard	*/

/*---------------------------------------------------------------------------*
 *	try to force keyboard into a known state ..
 *---------------------------------------------------------------------------*/
static
void doreset(void)
{
	int again = 0;
	int once = 0;
	int response, opri;

	/* Enable interrupts and keyboard, etc. */
	if (kbc_8042cmd(CONTR_WRITE) != 0)
		printf("pcvt: doreset() - timeout controller write command\n");

#if PCVT_USEKBDSEC		/* security enabled */

#  if PCVT_SCANSET == 2
#    define KBDINITCMD COMMAND_SYSFLG|COMMAND_IRQEN
#  else /* PCVT_SCANSET != 2 */
#    define KBDINITCMD COMMAND_PCSCAN|COMMAND_SYSFLG|COMMAND_IRQEN
#  endif /* PCVT_SCANSET == 2 */

#else /* ! PCVT_USEKBDSEC */	/* security disabled */

#  if PCVT_SCANSET == 2
#    define KBDINITCMD COMMAND_INHOVR|COMMAND_SYSFLG|COMMAND_IRQEN
#  else /* PCVT_SCANSET != 2 */
#    define KBDINITCMD COMMAND_PCSCAN|COMMAND_INHOVR|COMMAND_SYSFLG\
	|COMMAND_IRQEN
#  endif /* PCVT_SCANSET == 2 */

#endif /* PCVT_USEKBDSEC */

	if (kbd_cmd(KBDINITCMD) != 0)
		printf("pcvt: doreset() - timeout writing keyboard init command\n");

	/*
	 * Discard any stale keyboard activity.  The 0.1 boot code isn't
	 * very careful and sometimes leaves a KEYB_R_RESEND.
	 */
	while (inb(CONTROLLER_CTRL) & STATUS_OUTPBF)
		kbd_response();

	/* Start keyboard reset */

	opri = spltty ();

	if (kbd_cmd(KEYB_C_RESET) != 0)
		printf("pcvt: doreset() - timeout for keyboard reset command\n");

	/* Wait for the first response to reset and handle retries */
	while ((response = kbd_response()) != KEYB_R_ACK)
	{
		if (response < 0)
		{
			if(!again)	/* print message only once ! */
				printf("pcvt: doreset() - response != ack and response < 0 [one time only msg]\n");
			response = KEYB_R_RESEND;
		}
		if (response == KEYB_R_RESEND)
		{
			if(!again)	/* print message only once ! */
				printf("pcvt: doreset() - got KEYB_R_RESEND response ... [one time only msg]\n");

			if(++again > PCVT_NONRESP_KEYB_TRY)
			{
				printf("pcvt: doreset() - Caution - no PC keyboard detected!\n");
				keyboard_type = KB_UNKNOWN;
				splx(opri);
				return;
			}

			if((kbd_cmd(KEYB_C_RESET) != 0) && (once == 0))
			{
				once++;		/* print message only once ! */
				printf("pcvt: doreset() - timeout for loop keyboard reset command [one time only msg]\n");
			}
		}
	}

	/* Wait for the second response to reset */

	while ((response = kbd_response()) != KEYB_R_SELFOK)
	{
		if (response < 0)
		{
			printf("pcvt: doreset() - response != OK and resonse < 0\n");
			/*
			 * If KEYB_R_SELFOK never arrives, the loop will
			 * finish here unless the keyboard babbles or
			 * STATUS_OUTPBF gets stuck.
			 */
			break;
		}
	}

	splx (opri);

#if PCVT_KEYBDID

	opri = spltty ();

	if(kbd_cmd(KEYB_C_ID) != 0)
	{
		printf("pcvt: doreset() - timeout for keyboard ID command\n");
		keyboard_type = KB_UNKNOWN;
	}
	else
	{

r_entry:

		if((response = kbd_response()) == KEYB_R_MF2ID1)
		{
			if((response = kbd_response()) == KEYB_R_MF2ID2)
			{
				keyboard_type = KB_MFII;
			}
			else if(response == KEYB_R_MF2ID2HP)
			{
				keyboard_type = KB_MFII;
			}
			else
			{
				printf("\npcvt: doreset() - kbdid, response 2 = [%d]\n",
				       response);
				keyboard_type = KB_UNKNOWN;
			}
		}
		else if (response == KEYB_R_ACK)
		{
			goto r_entry;
		}
		else if (response == -1)
		{
			keyboard_type = KB_AT;
		}
		else
		{
			printf("\npcvt: doreset() - kbdid, response 1 = [%d]\n", response);
		}
	}

	splx (opri);

#else /* PCVT_KEYBDID */

	keyboard_type = KB_MFII;	/* force it .. */

#endif /* PCVT_KEYBDID */

}

/*---------------------------------------------------------------------------*
 *	init keyboard code
 *---------------------------------------------------------------------------*/
void
kbd_code_init(void)
{
	doreset();
	ovlinit(0);
	keyboard_is_initialized = 1;
}

/*---------------------------------------------------------------------------*
 *	init keyboard code, this initializes the keyboard subsystem
 *	just "a bit" so the very very first ddb session is able to
 *	get proper keystrokes - in other words, it's a hack ....
 *---------------------------------------------------------------------------*/
void
kbd_code_init1(void)
{
	doreset();
	keyboard_is_initialized = 1;
}

/*---------------------------------------------------------------------------*
 *	init keyboard overlay table
 *---------------------------------------------------------------------------*/
static
void ovlinit(int force)
{
	register i;

	if(force || ovlinitflag==0)
	{
		if(ovlinitflag == 0 &&
		   (ovltbl = (Ovl_tbl *)malloc(sizeof(Ovl_tbl) * OVLTBL_SIZE,
					       M_DEVBUF, M_WAITOK)) == NULL)
			panic("pcvt_kbd: malloc of Ovl_tbl failed");

		for(i=0; i<OVLTBL_SIZE; i++)
		{
			ovltbl[i].keynum =
			ovltbl[i].type = 0;
			ovltbl[i].unshift[0] =
			ovltbl[i].shift[0] =
			ovltbl[i].ctrl[0] =
			ovltbl[i].altgr[0] = 0;
			ovltbl[i].subu =
			ovltbl[i].subs =
			ovltbl[i].subc =
			ovltbl[i].suba = KBD_SUBT_STR;	/* just strings .. */
		}
		for(i=0; i<=MAXKEYNUM; i++)
			key2ascii[i].type &= KBD_MASK;
		ovlinitflag = 1;
	}
}

/*---------------------------------------------------------------------------*
 *	get original key definition
 *---------------------------------------------------------------------------*/
static int
getokeydef(unsigned key, Ovl_tbl *thisdef)
{
	if(key == 0 || key > MAXKEYNUM)
		return EINVAL;

	thisdef->keynum = key;
	thisdef->type = key2ascii[key].type;

	if(key2ascii[key].unshift.subtype == STR)
	{
		bcopy((u_char *)(key2ascii[key].unshift.what.string),
		       thisdef->unshift, CODE_SIZE);
		thisdef->subu = KBD_SUBT_STR;
	}
	else
	{
		bcopy("", thisdef->unshift, CODE_SIZE);
		thisdef->subu = KBD_SUBT_FNC;
	}

	if(key2ascii[key].shift.subtype == STR)
	{
		bcopy((u_char *)(key2ascii[key].shift.what.string),
		       thisdef->shift, CODE_SIZE);
		thisdef->subs = KBD_SUBT_STR;
	}
	else
	{
		bcopy("",thisdef->shift,CODE_SIZE);
		thisdef->subs = KBD_SUBT_FNC;
	}

	if(key2ascii[key].ctrl.subtype == STR)
	{
		bcopy((u_char *)(key2ascii[key].ctrl.what.string),
		       thisdef->ctrl, CODE_SIZE);
		thisdef->subc = KBD_SUBT_STR;
	}
	else
	{
		bcopy("",thisdef->ctrl,CODE_SIZE);
		thisdef->subc = KBD_SUBT_FNC;
	}

	/* deliver at least anything for ALTGR settings ... */

	if(key2ascii[key].unshift.subtype == STR)
	{
		bcopy((u_char *)(key2ascii[key].unshift.what.string),
		       thisdef->altgr, CODE_SIZE);
		thisdef->suba = KBD_SUBT_STR;
	}
	else
	{
		bcopy("",thisdef->altgr, CODE_SIZE);
		thisdef->suba = KBD_SUBT_FNC;
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	get current key definition
 *---------------------------------------------------------------------------*/
static int
getckeydef(unsigned key, Ovl_tbl *thisdef)
{
	u_short type = key2ascii[key].type;

	if(key>MAXKEYNUM)
		return EINVAL;

	if(type & KBD_OVERLOAD)
		*thisdef = ovltbl[key2ascii[key].ovlindex];
	else
		getokeydef(key,thisdef);

	return 0;
}

/*---------------------------------------------------------------------------*
 *	translate keynumber and returns ptr to associated ascii string
 *	if key is bound to a function, executes it, and ret empty ptr
 *---------------------------------------------------------------------------*/
static u_char *
xlatkey2ascii(U_short key)
{
	static u_char	capchar[2] = {0, 0};
#if PCVT_META_ESC
	static u_char	metachar[3] = {0x1b, 0, 0};
#else
	static u_char	metachar[2] = {0, 0};
#endif
	static Ovl_tbl	thisdef;
	int		n;
	void		(*fnc)(void);

	if(key==0)			/* ignore the NON-KEY */
		return 0;

	getckeydef(key&0x7F, &thisdef);	/* get the current ASCII value */

	thisdef.type &= KBD_MASK;

	if(key&0x80)			/* special handling of ALT-KEYPAD */
	{
		/* is the ALT Key released? */
		if(thisdef.type==KBD_META || thisdef.type==KBD_ALTGR)
		{
			if(altkpflag)	/* have we been in altkp mode? */
			{
				capchar[0] = altkpval;
				altkpflag = 0;
				altkpval = 0;
				return capchar;
			}
		}
		return 0;
	}

	switch(thisdef.type)		/* convert the keys */
	{
		case KBD_BREAK:
		case KBD_ASCII:
		case KBD_FUNC:
			fnc = NULL;
			more_chars = NULL;

			if(altgr_down)
			{
				more_chars = (u_char *)thisdef.altgr;
			}
			else if(!ctrl_down && (shift_down || vsp->shift_lock))
			{
				if(key2ascii[key].shift.subtype == STR)
					more_chars = (u_char *)thisdef.shift;
				else
					fnc = key2ascii[key].shift.what.func;
			}

			else if(ctrl_down)
			{
				if(key2ascii[key].ctrl.subtype == STR)
					more_chars = (u_char *)thisdef.ctrl;
				else
					fnc = key2ascii[key].ctrl.what.func;
			}

			else
			{
				if(key2ascii[key].unshift.subtype == STR)
					more_chars = (u_char *)thisdef.unshift;
				else
					fnc = key2ascii[key].unshift.what.func;
			}

			if(fnc)
				(*fnc)();	/* execute function */

			if((more_chars != NULL) && (more_chars[1] == 0))
			{
				if(vsp->caps_lock && more_chars[0] >= 'a'
				   && more_chars[0] <= 'z')
				{
					capchar[0] = *more_chars - ('a'-'A');
					more_chars = capchar;
				}
				if(meta_down)
				{
#if PCVT_META_ESC
					metachar[1] = *more_chars;
#else
					metachar[0] = *more_chars | 0x80;
#endif
					more_chars = metachar;
				}
			}
			return(more_chars);

		case KBD_KP:
			fnc = NULL;
			more_chars = NULL;

			if(meta_down)
			{
				switch(key)
				{
					case 95:	/* / */
						altkpflag = 0;
						more_chars =
						 (u_char *)"\033OQ";
						return(more_chars);

					case 100:	/* * */
						altkpflag = 0;
						more_chars =
						 (u_char *)"\033OR";
						return(more_chars);

					case 105:	/* - */
						altkpflag = 0;
						more_chars =
						 (u_char *)"\033OS";
						return(more_chars);
				}
			}

			if(meta_down || altgr_down)
			{
				if((n = keypad2num[key-91]) >= 0)
				{
					if(!altkpflag)
					{
						/* start ALT-KP mode */
						altkpflag = 1;
						altkpval = 0;
					}
					altkpval *= 10;
					altkpval += n;
				}
				else
					altkpflag = 0;
				return 0;
			}

			if(!(vsp->num_lock))
			{
				if(key2ascii[key].shift.subtype == STR)
					more_chars = (u_char *)thisdef.shift;
				else
					fnc = key2ascii[key].shift.what.func;
			}
			else
			{
				if(key2ascii[key].unshift.subtype == STR)
					more_chars = (u_char *)thisdef.unshift;
				else
					fnc = key2ascii[key].unshift.what.func;
			}

			if(fnc)
				(*fnc)();	/* execute function */
			return(more_chars);

		case KBD_CURSOR:
			fnc = NULL;
			more_chars = NULL;

			if(vsp->ckm)
			{
				if(key2ascii[key].shift.subtype == STR)
					more_chars = (u_char *)thisdef.shift;
				else
					fnc = key2ascii[key].shift.what.func;
			}
			else
			{
				if(key2ascii[key].unshift.subtype == STR)
					more_chars = (u_char *)thisdef.unshift;
				else
					fnc = key2ascii[key].unshift.what.func;
			}

			if(fnc)
				(*fnc)();	/* execute function */
			return(more_chars);

		case KBD_NUM:		/*  special kp-num handling */
			more_chars = NULL;

			if(meta_down)
			{
				more_chars = (u_char *)"\033OP"; /* PF1 */
			}
			else
			{
				vsp->num_lock ^= 1;
				update_led();
			}
			return(more_chars);

		case KBD_RETURN:
			more_chars = NULL;

			if(!(vsp->num_lock))
			{
				more_chars = (u_char *)thisdef.shift;
			}
			else
			{
				more_chars = (u_char *)thisdef.unshift;
			}
			if(vsp->lnm && (*more_chars == '\r'))
			{
				more_chars = (u_char *)"\r\n"; /* CR LF */
			}
			if(meta_down)
			{
#if PCVT_META_ESC
				metachar[1] = *more_chars;
#else
				metachar[0] = *more_chars | 0x80;
#endif
				more_chars = metachar;
			}
			return(more_chars);

		case KBD_META:		/* these keys are	*/
		case KBD_ALTGR:		/*  handled directly	*/
		case KBD_SCROLL:	/*  by the keyboard	*/
		case KBD_CAPS:		/*  handler - they are	*/
		case KBD_SHFTLOCK:	/*  ignored here	*/
		case KBD_CTL:
		case KBD_NONE:
		default:
			return 0;
	}
}

/*---------------------------------------------------------------------------*
 *	get keystrokes from the keyboard.
 *	if noblock = 0, wait until a key is pressed.
 *	else return NULL if no characters present.
 *---------------------------------------------------------------------------*/

#if PCVT_KBD_FIFO
extern	u_char	pcvt_kbd_fifo[];
extern	int	pcvt_kbd_rptr;
extern	short	pcvt_kbd_count;
#endif

u_char *
sgetc(int noblock)
{
	u_char		*cp;
	u_char		dt;
	u_char		key;
	u_short		type;

#if PCVT_KBD_FIFO && PCVT_SLOW_INTERRUPT
	int		s;
#endif

	static u_char	kbd_lastkey = 0; /* last keystroke */

	static struct
	{
		u_char extended: 1;	/* extended prefix seen */
		u_char ext1: 1;		/* extended prefix 1 seen */
		u_char breakseen: 1;	/* break code seen */
		u_char vshift: 1;	/* virtual shift pending */
		u_char vcontrol: 1;	/* virtual control pending */
		u_char sysrq: 1;	/* sysrq pressed */
	} kbd_status = {0};

#ifdef XSERVER
	static char	keybuf[2] = {0}; /* the second 0 is a delimiter! */
#endif /* XSERVER */

loop:

#ifdef XSERVER

#if PCVT_KBD_FIFO

	/* see if there is data from the keyboard available either from */
	/* the keyboard fifo or from the 8042 keyboard controller	*/

	if ((noblock && pcvt_kbd_count) ||
	    ((!noblock || kbd_polling) && (inb(CONTROLLER_CTRL) & STATUS_OUTPBF)))
	{
		if (!noblock || kbd_polling)	/* source = 8042 */
		{
			PCVT_KBD_DELAY();	/* 7 us delay */
			dt = inb(CONTROLLER_DATA);	/* get from obuf */
		}
		else			/* source = keyboard fifo */
		{
			dt = pcvt_kbd_fifo[pcvt_kbd_rptr++];
			PCVT_DISABLE_INTR();
			pcvt_kbd_count--;
			PCVT_ENABLE_INTR();
			if (pcvt_kbd_rptr >= PCVT_KBD_FIFO_SZ)
				pcvt_kbd_rptr = 0;
		}

#else /* !PCVT_KB_FIFO */

	/* see if there is data from the keyboard available from the 8042 */

	if (inb(CONTROLLER_CTRL) & STATUS_OUTPBF)
	{
		PCVT_KBD_DELAY();		/* 7 us delay */
		dt = inb(CONTROLLER_DATA);	/* yes, get data */

#endif /* !PCVT_KBD_FIFO */

		/*
		 * If x mode is active, only care for locking keys, then
		 * return the scan code instead of any key translation.
		 * Additionally, this prevents us from any attempts to
		 * execute pcvt internal functions caused by keys (such
		 * as screen flipping).
		 * XXX For now, only the default locking key definitions
		 * are recognized (i.e. if you have overloaded you "A" key
		 * as NUMLOCK, that wont effect X mode:-)
		 * Changing this would be nice, but would require modifi-
		 * cations to the X server. After having this, X will
		 * deal with the LEDs itself, so we are committed.
		 */
		/*
		 * Iff PCVT_USL_VT_COMPAT is defined, the behaviour has
		 * been fixed. We need not care about any keys here, since
		 * there are ioctls that deal with the lock key / LED stuff.
		 */
		if (pcvt_kbd_raw)
		{
			keybuf[0] = dt;

#if PCVT_FREEBSD > 210
			add_keyboard_randomness(dt);
#endif	/* PCVT_FREEBSD > 210 */

#if !PCVT_USL_VT_COMPAT
			if ((dt & 0x80) == 0)
				/* key make */
				switch(dt)
				{
				case 0x45:
					/* XXX on which virt screen? */					vsp->num_lock ^= 1;
					update_led();
					break;

				case 0x3a:
					vsp->caps_lock ^= 1;
					update_led();
					break;

				case 0x46:
					vsp->scroll_lock ^= 1;
					update_led();
					break;
				}
#endif /* !PCVT_USL_VT_COMPAT */

#if PCVT_EMU_MOUSE
			/*
			 * The (mouse systems) mouse emulator. The mouse
			 * device allocates the first device node that is
			 * not used by a virtual terminal. (E.g., you have
			 * eight vtys, /dev/ttyv0 thru /dev/ttyv7, so the
			 * mouse emulator were /dev/ttyv8.)
			 * Currently the emulator only works if the keyboard
			 * is in raw (PC scan code) mode. This is the typic-
			 * al case when running the X server.
			 * It is activated if the num locks LED is active
			 * for the current vty, and if the mouse device
			 * has been opened by at least one process. It
			 * grabs the numerical keypad events (but only
			 * the "non-extended", so the separate arrow keys
			 * continue to work), and three keys for the "mouse
			 * buttons", preferrably F1 thru F3. Any of the
			 * eight directions (N, NE, E, SE, S, SW, W, NW)
			 * is supported, and frequent key presses (less
			 * than e.g. half a second between key presses)
			 * cause the emulator to accelerate the pointer
			 * movement by 6, while single presses result in
			 * single moves, so each point can be reached.
			 */
			/*
			 * NB: the following code is spagghetti.
			 * Only eat it with lotta tomato ketchup and
			 * Parmesan cheese:-)
			 */
			/*
			 * look whether we will have to steal the keys
			 * and cook them into mouse events
			 */
			if(vsp->num_lock && mouse.opened)
			{
				int button, accel, i;
				enum mouse_dir
				{
					MOUSE_NW, MOUSE_N, MOUSE_NE,
					MOUSE_W,  MOUSE_0, MOUSE_E,
					MOUSE_SW, MOUSE_S, MOUSE_SE
				}
				move;
				struct timeval now;
				dev_t dummy = makedev(0, mouse.minor);
				struct tty *mousetty = get_pccons(dummy);
				/*
				 * strings to send for each mouse event,
				 * indexed by the movement direction and
				 * the "accelerator" value (TRUE for frequent
				 * key presses); note that the first byte
				 * of each string is actually overwritten
				 * by the current button value before sending
				 * the string
				 */
				static u_char mousestrings[2][MOUSE_SE+1][5] =
				{
				{
					/* first, the non-accelerated strings*/
					{0x87,  -1,   1,   0,   0}, /* NW */
					{0x87,   0,   1,   0,   0}, /* N */
					{0x87,   1,   1,   0,   0}, /* NE */
					{0x87,  -1,   0,   0,   0}, /* W */
					{0x87,   0,   0,   0,   0}, /* 0 */
					{0x87,   1,   0,   0,   0}, /* E */
					{0x87,  -1,  -1,   0,   0}, /* SW */
					{0x87,   0,  -1,   0,   0}, /* S */
					{0x87,   1,  -1,   0,   0}  /* SE */
				},
				{
					/* now, 6 steps at once */
					{0x87,  -4,   4,   0,   0}, /* NW */
					{0x87,   0,   6,   0,   0}, /* N */
					{0x87,   4,   4,   0,   0}, /* NE */
					{0x87,  -6,   0,   0,   0}, /* W */
					{0x87,   0,   0,   0,   0}, /* 0 */
					{0x87,   6,   0,   0,   0}, /* E */
					{0x87,  -4,  -4,   0,   0}, /* SW */
					{0x87,   0,  -6,   0,   0}, /* S */
					{0x87,   4,  -4,   0,   0}  /* SE */
				}
				};

				if(dt == 0xe0)
				{
					/* ignore extended scan codes */
					mouse.extendedseen = 1;
					goto no_mouse_event;
				}
				if(mouse.extendedseen)
				{
					mouse.extendedseen = 0;
					goto no_mouse_event;
				}
				mouse.extendedseen = 0;

				/*
				 * Note that we cannot use a switch here
				 * since we want to have the keycodes in
				 * a variable
				 */
				if((dt & 0x7f) == mousedef.leftbutton) {
					button = 4;
					goto do_button;
				}
				else if((dt & 0x7f) == mousedef.middlebutton) {
					button = 2;
					goto do_button;
				}
				else if((dt & 0x7f) == mousedef.rightbutton) {
					button = 1;
				do_button:

					/*
					 * i would really like to give
					 * some acustical support
					 * (pling/plong); i am not sure
					 * whether it is safe to call
					 * sysbeep from within an intr
					 * service, since it calls
					 * timeout in turn which mani-
					 * pulates the spl mask - jw
					 */

# define PLING sysbeep(PCVT_SYSBEEPF / 1500, 2)
# define PLONG sysbeep(PCVT_SYSBEEPF / 1200, 2)

					if(mousedef.stickybuttons)
					{
						if(dt & 0x80) {
							mouse.breakseen = 1;
							return (u_char *)0;
						}
						else if(mouse.buttons == button
							&& !mouse.breakseen) {
							/* ignore repeats */
							return (u_char *)0;
						}
						else
							mouse.breakseen = 0;
						if(mouse.buttons == button) {
							/* release it */
							mouse.buttons = 0;
							PLONG;
						} else {
							/*
							 * eventually, release
							 * any other button,
							 * and stick this one
							 */
							mouse.buttons = button;
							PLING;
						}
					}
					else
					{
						if(dt & 0x80) {
							mouse.buttons &=
								~button;
							PLONG;
						}
						else if((mouse.buttons
							& button) == 0) {
							mouse.buttons |=
								button;
							PLING;
						}
						/*else: ignore same btn press*/
					}
					move = MOUSE_0;
					accel = 0;
				}
# undef PLING
# undef PLONG
				else switch(dt & 0x7f)
				{
				/* the arrow keys - KP 1 thru KP 9 */
				case 0x47:	move = MOUSE_NW; goto do_move;
				case 0x48:	move = MOUSE_N; goto do_move;
				case 0x49:	move = MOUSE_NE; goto do_move;
				case 0x4b:	move = MOUSE_W; goto do_move;
				case 0x4c:	move = MOUSE_0; goto do_move;
				case 0x4d:	move = MOUSE_E; goto do_move;
				case 0x4f:	move = MOUSE_SW; goto do_move;
				case 0x50:	move = MOUSE_S; goto do_move;
				case 0x51:	move = MOUSE_SE;
				do_move:
					if(dt & 0x80)
						/*
						 * arrow key break events are
						 * of no importance for us
						 */
						return (u_char *)0;
					/*
					 * see whether the last move did
					 * happen "recently", i.e. before
					 * less than half a second
					 */
					now = time;
					timevalsub(&now, &mouse.lastmove);
					mouse.lastmove = time;
					accel = (now.tv_sec == 0
						 && now.tv_usec
						 < mousedef.acceltime);
					break;

				default: /* not a mouse-emulating key */
					goto no_mouse_event;
				}
				mousestrings[accel][move][0] =
					0x80 + (~mouse.buttons & 7);
				/* finally, send the string */
				for(i = 0; i < 5; i++)
					(*linesw[mousetty->t_line].l_rint)
						(mousestrings[accel][move][i],
						 mousetty);
				return (u_char *)0; /* not a kbd event */
			}
no_mouse_event:

#endif /* PCVT_EMU_MOUSE */

			return ((u_char *)keybuf);
		}
	}

#else /* !XSERVER */

#  if PCVT_KBD_FIFO

	/* see if there is data from the keyboard available either from */
	/* the keyboard fifo or from the 8042 keyboard controller	*/

	if ((noblock && pcvt_kbd_count) ||
	    ((!noblock || kbd_polling) && (inb(CONTROLLER_CTRL) & STATUS_OUTPBF)))
	{
		if (!noblock || kbd_polling)	/* source = 8042 */
		{
			PCVT_KBD_DELAY();	/* 7 us delay */
			dt = inb(CONTROLLER_DATA);
		}
		else			/* source = keyboard fifo */
		{
			dt = pcvt_kbd_fifo[pcvt_kbd_rptr++]; /* yes, get it ! */
			PCVT_DISABLE_INTR();
			pcvt_kbd_count--;
			PCVT_ENABLE_INTR();
			if (pcvt_kbd_rptr >= PCVT_KBD_FIFO_SZ)
				pcvt_kbd_rptr = 0;
		}
	}

#else /* !PCVT_KBD_FIFO */

	/* see if there is data from the keyboard available from the 8042 */

	if(inb(CONTROLLER_CTRL) & STATUS_OUTPBF)
	{
		PCVT_KBD_DELAY();		/* 7 us delay */
		dt = inb(CONTROLLER_DATA);	/* yes, get data ! */
	}

#endif /* !PCVT_KBD_FIFO */

#endif /* !XSERVER */

	else
	{
		if(noblock)
			return NULL;
		else
			goto loop;
	}

#if PCVT_SHOWKEYS
	showkey (' ', dt);
#endif	/* PCVT_SHOWKEYS */

	/* lets look what we got */
	switch(dt)
	{
		case KEYB_R_OVERRUN0:	/* keyboard buffer overflow */

#if PCVT_SCANSET == 2
		case KEYB_R_SELFOK:	/* keyboard selftest ok */
#endif /* PCVT_SCANSET == 2 */

		case KEYB_R_ECHO:	/* keyboard response to KEYB_C_ECHO */
		case KEYB_R_ACK:	/* acknowledge after command has rx'd*/
		case KEYB_R_SELFBAD:	/* keyboard selftest FAILED */
		case KEYB_R_DIAGBAD:	/* keyboard self diagnostic failure */
		case KEYB_R_RESEND:	/* keyboard wants us to resend cmnd */
		case KEYB_R_OVERRUN1:	/* keyboard buffer overflow */
			break;

		case KEYB_R_EXT1:	/* keyboard extended scancode pfx 2 */
			kbd_status.ext1 = 1;
			/* FALLTHROUGH */
		case KEYB_R_EXT0:	/* keyboard extended scancode pfx 1 */
			kbd_status.extended = 1;
			break;

#if PCVT_SCANSET == 2
		case KEYB_R_BREAKPFX:	/* break code prefix for set 2 and 3 */
			kbd_status.breakseen = 1;
			break;
#endif /* PCVT_SCANSET == 2 */

		default:
			goto regular;	/* regular key */
	}

	if(noblock)
		return NULL;
	else
		goto loop;

	/* got a normal scan key */
regular:

#if PCVT_FREEBSD > 210
	add_keyboard_randomness(dt);
#endif	/* PCVT_FREEBSD > 210 */

#if PCVT_SCANSET == 1
	kbd_status.breakseen = dt & 0x80 ? 1 : 0;
	dt &= 0x7f;
#endif	/* PCVT_SCANSET == 1 */

	/*   make a keycode from scan code	*/
	if(dt >= sizeof scantokey / sizeof(u_char))
		key = 0;
	else
		key = kbd_status.extended ? extscantokey[dt] : scantokey[dt];

	if(kbd_status.ext1 && key == 64)
		/* virtual control key */
		key = 129;

	kbd_status.extended = kbd_status.ext1 = 0;

#if PCVT_CTRL_ALT_DEL		/*   Check for cntl-alt-del	*/
	if((key == 76) && ctrl_down && (meta_down||altgr_down))
		shutdown_nice();
#endif /* PCVT_CTRL_ALT_DEL */

#if !(PCVT_NETBSD || PCVT_FREEBSD >= 200)
#include "ddb.h"
#endif /* !(PCVT_NETBSD || PCVT_FREEBSD >= 200) */

#if NDDB > 0 || defined(DDB)		 /*   Check for cntl-alt-esc	*/

  	if((key == 110) && ctrl_down && (meta_down || altgr_down))
 	{
 		static u_char in_Debugger;

 		if(!in_Debugger)
 		{
 			in_Debugger = 1;
#if PCVT_FREEBSD
			/* the string is actually not used... */
			Debugger("kbd");
#else
 			Debugger();
#endif
 			in_Debugger = 0;
 			if(noblock)
 				return NULL;
 			else
 				goto loop;
 		}
 	}
#endif /* NDDB > 0 || defined(DDB) */

	/* look for keys with special handling */
	if(key == 128)
	{
		/*
		 * virtual shift; sent around PrtScr, and around the arrow
		 * keys if the NumLck LED is on
		 */
		kbd_status.vshift = !kbd_status.breakseen;
		key = 0;	/* no key */
	}
	else if(key == 129)
	{
		/*
		 * virtual control - the most ugly thingie at all
		 * the Pause key sends:
		 * <virtual control make> <numlock make> <virtual control
		 * break> <numlock break>
		 */
		if(!kbd_status.breakseen)
			kbd_status.vcontrol = 1;
		/* else: let the numlock hook clear this */
		key = 0;	/* no key */
	}
	else if(key == 90)
	{
		/* NumLock, look whether this is rather a Pause */
		if(kbd_status.vcontrol)
			key = 126;
		/*
		 * if this is the final break code of a Pause key,
		 * clear the virtual control status, too
		 */
		if(kbd_status.vcontrol && kbd_status.breakseen)
			kbd_status.vcontrol = 0;
	}
	else if(key == 127)
	{
		/*
		 * a SysRq; some keyboards are brain-dead enough to
		 * repeat the SysRq key make code by sending PrtScr
		 * make codes; other keyboards do not repeat SysRq
		 * at all. We keep track of the SysRq state here.
		 */
		kbd_status.sysrq = !kbd_status.breakseen;
	}
	else if(key == 124)
	{
		/*
		 * PrtScr; look whether this is really PrtScr or rather
		 * a silly repeat of a SysRq key
		 */
		if(kbd_status.sysrq)
			/* ignore the garbage */
			key = 0;
	}

	/* in NOREPEAT MODE ignore the key if it was the same as before */

	if(!kbrepflag && key == kbd_lastkey && !kbd_status.breakseen)
	{
		if(noblock)
			return NULL;
		else
			goto loop;
	}

	type = key2ascii[key].type;

	if(type & KBD_OVERLOAD)
		type = ovltbl[key2ascii[key].ovlindex].type;

	type &= KBD_MASK;

	switch(type)
	{
		case KBD_SHFTLOCK:
			if(!kbd_status.breakseen && key != kbd_lastkey)
			{
				vsp->shift_lock ^= 1;
			}
			break;

		case KBD_CAPS:
			if(!kbd_status.breakseen && key != kbd_lastkey)
			{
				vsp->caps_lock ^= 1;
				update_led();
			}
			break;

		case KBD_SCROLL:
			if(!kbd_status.breakseen && key != kbd_lastkey)
			{
				vsp->scroll_lock ^= 1;
				update_led();

				if(!(vsp->scroll_lock))
				{
					/* someone may be sleeping */
					wakeup((caddr_t)&(vsp->scroll_lock));
				}
			}
			break;

		case KBD_SHIFT:
			shift_down = kbd_status.breakseen ? 0 : 1;
			break;

		case KBD_META:
			meta_down = kbd_status.breakseen ? 0 : 0x80;
			break;

		case KBD_ALTGR:
			altgr_down = kbd_status.breakseen ? 0 : 1;
			break;

		case KBD_CTL:
			ctrl_down = kbd_status.breakseen ? 0 : 1;
			break;

		case KBD_NONE:
		default:
			break;			/* deliver a key */
	}

	if(kbd_status.breakseen)
	{
		key |= 0x80;
		kbd_status.breakseen = 0;
		kbd_lastkey = 0; /* -hv- I know this is a bug with */
	}			 /* N-Key-Rollover, but I ignore that */
	else			 /* because avoidance is too complicated */
		kbd_lastkey = key;

	cp = xlatkey2ascii(key);	/* have a key */

	if(cp == NULL && !noblock)
		goto loop;

	return cp;
}

/*---------------------------------------------------------------------------*
 *	reflect status of locking keys & set led's
 *---------------------------------------------------------------------------*/
static void
setlockkeys(int snc)
{
	vsp->scroll_lock = snc & 1;
	vsp->num_lock	 = (snc & 2) ? 1 : 0;
	vsp->caps_lock	 = (snc & 4) ? 1 : 0;
	update_led();
}

/*---------------------------------------------------------------------------*
 *	remove a key definition
 *---------------------------------------------------------------------------*/
static int
rmkeydef(int key)
{
	register Ovl_tbl *ref;

	if(key==0 || key > MAXKEYNUM)
		return EINVAL;

	if(key2ascii[key].type & KBD_OVERLOAD)
	{
		ref = &ovltbl[key2ascii[key].ovlindex];
		ref->keynum = 0;
		ref->type = 0;
		ref->unshift[0] =
		ref->shift[0] =
		ref->ctrl[0] =
		ref->altgr[0] = 0;
		key2ascii[key].type &= KBD_MASK;
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	overlay a key
 *---------------------------------------------------------------------------*/
static int
setkeydef(Ovl_tbl *data)
{
	register i;

	if( data->keynum > MAXKEYNUM		 ||
	    (data->type & KBD_MASK) == KBD_BREAK ||
	    (data->type & KBD_MASK) > KBD_SHFTLOCK)
		return EINVAL;

	data->unshift[KBDMAXOVLKEYSIZE] =
	data->shift[KBDMAXOVLKEYSIZE] =
	data->ctrl[KBDMAXOVLKEYSIZE] =
	data->altgr[KBDMAXOVLKEYSIZE] = 0;

	data->subu =
	data->subs =
	data->subc =
	data->suba = KBD_SUBT_STR;		/* just strings .. */

	data->type |= KBD_OVERLOAD;		/* mark overloaded */

	/* if key already overloaded, use that slot else find free slot */

	if(key2ascii[data->keynum].type & KBD_OVERLOAD)
	{
		i = key2ascii[data->keynum].ovlindex;
	}
	else
	{
		for(i=0; i<OVLTBL_SIZE; i++)
			if(ovltbl[i].keynum==0)
				break;

		if(i==OVLTBL_SIZE)
			return ENOSPC;	/* no space, abuse of ENOSPC(!) */
	}

	ovltbl[i] = *data;		/* copy new data string */

	key2ascii[data->keynum].type |= KBD_OVERLOAD; 	/* mark key */
	key2ascii[data->keynum].ovlindex = i;

	return 0;
}

/*---------------------------------------------------------------------------*
 *	keyboard ioctl's entry
 *---------------------------------------------------------------------------*/
int
kbdioctl(Dev_t dev, int cmd, caddr_t data, int flag)
{
	int key;

	switch(cmd)
	{
		case KBDRESET:
			doreset();
			ovlinit(1);
			settpmrate(KBD_TPD500|KBD_TPM100);
			setlockkeys(0);
			break;

		case KBDGTPMAT:
			*(int *)data = tpmrate;
			break;

		case KBDSTPMAT:
			settpmrate(*(int *)data);
			break;

		case KBDGREPSW:
			*(int *)data = kbrepflag;
			break;

		case KBDSREPSW:
			kbrepflag = (*(int *)data) & 1;
			break;

		case KBDGLEDS:
			*(int *)data = ledstate;
			break;

		case KBDSLEDS:
			update_led();	/* ??? */
			break;

		case KBDGLOCK:
			*(int *)data = ( (vsp->scroll_lock) |
					 (vsp->num_lock * 2) |
					 (vsp->caps_lock * 4));
			break;

		case KBDSLOCK:
			setlockkeys(*(int *)data);
			break;

		case KBDGCKEY:
			key = ((Ovl_tbl *)data)->keynum;
			return getckeydef(key,(Ovl_tbl *)data);

		case KBDSCKEY:
			key = ((Ovl_tbl *)data)->keynum;
			return setkeydef((Ovl_tbl *)data);

		case KBDGOKEY:
			key = ((Ovl_tbl *)data)->keynum;
			return getokeydef(key,(Ovl_tbl *)data);

		case KBDRMKEY:
			key = *(int *)data;
			return rmkeydef(key);

		case KBDDEFAULT:
			ovlinit(1);
			break;

		default:
			/* proceed with vga ioctls */
			return -1;
	}
	return 0;
}

#if PCVT_EMU_MOUSE
/*--------------------------------------------------------------------------*
 *	mouse emulator ioctl
 *--------------------------------------------------------------------------*/
int
mouse_ioctl(Dev_t dev, int cmd, caddr_t data)
{
	struct mousedefs *def = (struct mousedefs *)data;

	switch(cmd)
	{
		case KBDMOUSEGET:
			*def = mousedef;
			break;

		case KBDMOUSESET:
			mousedef = *def;
			break;

		default:
			return -1;
	}
	return 0;
}
#endif	/* PCVT_EMU_MOUSE */

#if PCVT_USL_VT_COMPAT
/*---------------------------------------------------------------------------*
 *	convert ISO-8859 style keycode into IBM 437
 *---------------------------------------------------------------------------*/
static inline u_char
iso2ibm(u_char c)
{
	if(c < 0x80)
		return c;
	return iso2ibm437[c - 0x80];
}

/*---------------------------------------------------------------------------*
 *	build up a USL style keyboard map
 *---------------------------------------------------------------------------*/
void
get_usl_keymap(keymap_t *map)
{
	int i;

	bzero((caddr_t)map, sizeof(keymap_t));

	map->n_keys = 0x59;	/* that many keys we know about */

	for(i = 1; i < N_KEYNUMS; i++)
	{
		Ovl_tbl kdef;
		u_char c;
		int j;
		int idx = key2scan1[i];

		if(idx == 0 || idx >= map->n_keys)
			continue;

		getckeydef(i, &kdef);
		kdef.type &= KBD_MASK;
		switch(kdef.type)
		{
		case KBD_ASCII:
		case KBD_RETURN:
			map->key[idx].map[0] = iso2ibm(kdef.unshift[0]);
			map->key[idx].map[1] = iso2ibm(kdef.shift[0]);
			map->key[idx].map[2] = map->key[idx].map[3] =
				iso2ibm(kdef.ctrl[0]);
			map->key[idx].map[4] = map->key[idx].map[5] =
				iso2ibm(c = kdef.altgr[0]);
			/*
			 * XXX this is a hack
			 * since we currently do not map strings to AltGr +
			 * shift, we attempt to use the unshifted AltGr
			 * definition here and try to toggle the case
			 * this should at least work for ISO8859 letters,
			 * but also for (e.g.) russian KOI-8 style
			 */
			if((c & 0x7f) >= 0x40)
				map->key[idx].map[5] = iso2ibm(c ^ 0x20);
			break;

		case KBD_FUNC:
			/* we are only interested in F1 thru F12 here */
			if(i >= 112 && i <= 123) {
				map->key[idx].map[0] = i - 112 + 27;
				map->key[idx].spcl = 0x80;
			}
			break;

		case KBD_SHIFT:
			c = i == 44? 2 /* lSh */: 3 /* rSh */; goto special;

		case KBD_CAPS:
			c = 4; goto special;

		case KBD_NUM:
			c = 5; goto special;

		case KBD_SCROLL:
			c = 6; goto special;

		case KBD_META:
			c = 7; goto special;

		case KBD_CTL:
			c = 9; goto special;
		special:
			for(j = 0; j < NUM_STATES; j++)
				map->key[idx].map[j] = c;
			map->key[idx].spcl = 0xff;
			break;

		default:
			break;
		}
	}
}

#endif /* PCVT_USL_VT_COMPAT */

/*---------------------------------------------------------------------------*
 *	switch keypad to numeric mode
 *---------------------------------------------------------------------------*/
void
vt_keynum(struct video_state *svsp)
{
	svsp->num_lock = 1;
	update_led();
}

/*---------------------------------------------------------------------------*
 *	switch keypad to application mode
 *---------------------------------------------------------------------------*/
void
vt_keyappl(struct video_state *svsp)
{
	svsp->num_lock = 0;
	update_led();
}

#if !PCVT_VT220KEYB	/* !PCVT_VT220KEYB, HP-like Keyboard layout */

/*---------------------------------------------------------------------------*
 *	function bound to function key 1
 *---------------------------------------------------------------------------*/
static void
fkey1(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_columns(vsp);
		else
			more_chars = (u_char *)"\033[17~";	/* F6 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[26~";	/* F14 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 2
 *---------------------------------------------------------------------------*/
static void
fkey2(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			vt_ris(vsp);
		else
			more_chars = (u_char *)"\033[18~";	/* F7 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[28~";	/* HELP */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 3
 *---------------------------------------------------------------------------*/
static void
fkey3(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_24l(vsp);
		else
			more_chars = (u_char *)"\033[19~";	/* F8 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[29~";	/* DO */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 4
 *---------------------------------------------------------------------------*/
static void
fkey4(void)
{
	if(!meta_down)
	{

#if PCVT_SHOWKEYS
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_kbddbg(vsp);
		else
			more_chars = (u_char *)"\033[20~";	/* F9 */
#else
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[20~";	/* F9 */
#endif /* PCVT_SHOWKEYS */

	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[31~";	/* F17 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 5
 *---------------------------------------------------------------------------*/
static void
fkey5(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_bell(vsp);
		else
			more_chars = (u_char *)"\033[21~";	/* F10 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[32~";	/* F18 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 6
 *---------------------------------------------------------------------------*/
static void
fkey6(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_sevenbit(vsp);
		else
			more_chars = (u_char *)"\033[23~";	/* F11 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[33~";	/* F19 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 7
 *---------------------------------------------------------------------------*/
static void
fkey7(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_dspf(vsp);
		else
			more_chars = (u_char *)"\033[24~";	/* F12 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[34~";	/* F20 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 8
 *---------------------------------------------------------------------------*/
static void
fkey8(void)
{
	if(!meta_down)
	{
		if((vsp->vt_pure_mode == M_HPVT)
		   && (vsp->which_fkl == SYS_FKL))
			toggl_awm(vsp);
		else
			more_chars = (u_char *)"\033[25~";	/* F13 */
	}
	else
	{
		if(vsp->vt_pure_mode == M_PUREVT
		   || (vsp->which_fkl == USR_FKL))
			more_chars = (u_char *)"\033[35~";	/* F21 ??!! */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 9
 *---------------------------------------------------------------------------*/
static void
fkey9(void)
{
	if(meta_down)
	{
		if(vsp->vt_pure_mode == M_PUREVT)
			return;

		if(vsp->labels_on)	/* toggle label display on/off */
			fkl_off(vsp);
		else
			fkl_on(vsp);
	}
	else
	{
		do_vgapage(0);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 10
 *---------------------------------------------------------------------------*/
static void
fkey10(void)
{
	if(meta_down)
	{
		if(vsp->vt_pure_mode != M_PUREVT && vsp->labels_on)
		{
			if(vsp->which_fkl == USR_FKL)
				sw_sfkl(vsp);
			else if(vsp->which_fkl == SYS_FKL)
				sw_ufkl(vsp);
		}
	}
	else
	{
		do_vgapage(1);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 11
 *---------------------------------------------------------------------------*/
static void
fkey11(void)
{
	if(meta_down)
	{
		if(vsp->vt_pure_mode == M_PUREVT)
			set_emulation_mode(vsp, M_HPVT);
		else if(vsp->vt_pure_mode == M_HPVT)
			set_emulation_mode(vsp, M_PUREVT);
	}
	else
	{
		do_vgapage(2);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 12
 *---------------------------------------------------------------------------*/
static void
fkey12(void)
{
	if(meta_down)
	{
		if(current_video_screen + 1 > totalscreens-1)
			do_vgapage(0);
		else
			do_vgapage(current_video_screen + 1);
	}
	else
	{
		do_vgapage(3);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 1
 *---------------------------------------------------------------------------*/
static void
sfkey1(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[0])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[0]]);
	}
	else
	{
		if(vsp->ukt.length[9])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[9]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 2
 *---------------------------------------------------------------------------*/
static void
sfkey2(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[1])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[1]]);
	}
	else
	{
		if(vsp->ukt.length[11])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[11]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 3
 *---------------------------------------------------------------------------*/
static void
sfkey3(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[2])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[2]]);
	}
	else
	{
		if(vsp->ukt.length[12])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[12]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 4
 *---------------------------------------------------------------------------*/
static void
sfkey4(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[3])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[3]]);
	}
	else
	{
		if(vsp->ukt.length[13])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[13]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 5
 *---------------------------------------------------------------------------*/
static void
sfkey5(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[4])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[4]]);
	}
	else
	{
		if(vsp->ukt.length[14])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[14]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 6
 *---------------------------------------------------------------------------*/
static void
sfkey6(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[6])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[6]]);
	}
	else
	{
		if(vsp->ukt.length[15])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[15]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 7
 *---------------------------------------------------------------------------*/
static void
sfkey7(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[7])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[7]]);
	}
	else
	{
		if(vsp->ukt.length[16])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[16]]);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 8
 *---------------------------------------------------------------------------*/
static void
sfkey8(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[8])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[8]]);
	}
	else
	{
		if(vsp->ukt.length[17])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[17]]);
	}
}
/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 9
 *---------------------------------------------------------------------------*/
static void
sfkey9(void)
{
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 10
 *---------------------------------------------------------------------------*/
static void
sfkey10(void)
{
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 11
 *---------------------------------------------------------------------------*/
static void
sfkey11(void)
{
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 12
 *---------------------------------------------------------------------------*/
static void
sfkey12(void)
{
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 1
 *---------------------------------------------------------------------------*/
static void
cfkey1(void)
{
	if(meta_down)
		do_vgapage(0);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 2
 *---------------------------------------------------------------------------*/
static void
cfkey2(void)
{
	if(meta_down)
		do_vgapage(1);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 3
 *---------------------------------------------------------------------------*/
static void
cfkey3(void)
{
	if(meta_down)
		do_vgapage(2);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 4
 *---------------------------------------------------------------------------*/
static void
cfkey4(void)
{
	if(meta_down)
		do_vgapage(3);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 5
 *---------------------------------------------------------------------------*/
static void
cfkey5(void)
{
	if(meta_down)
		do_vgapage(4);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 6
 *---------------------------------------------------------------------------*/
static void
cfkey6(void)
{
	if(meta_down)
		do_vgapage(5);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 7
 *---------------------------------------------------------------------------*/
static void
cfkey7(void)
{
	if(meta_down)
		do_vgapage(6);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 8
 *---------------------------------------------------------------------------*/
static void
cfkey8(void)
{
	if(meta_down)
		do_vgapage(7);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 9
 *---------------------------------------------------------------------------*/
static void
cfkey9(void)
{
	if(meta_down)
		do_vgapage(8);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 10
 *---------------------------------------------------------------------------*/
static void
cfkey10(void)
{
	if(meta_down)
		do_vgapage(9);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 11
 *---------------------------------------------------------------------------*/
static void
cfkey11(void)
{
	if(meta_down)
		do_vgapage(10);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 12
 *---------------------------------------------------------------------------*/
static void
cfkey12(void)
{
	if(meta_down)
		do_vgapage(11);
}

#else	/* PCVT_VT220  -  VT220-like Keyboard layout */

/*---------------------------------------------------------------------------*
 *	function bound to function key 1
 *---------------------------------------------------------------------------*/
static void
fkey1(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[23~"; /* F11 */
	else
		do_vgapage(0);
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 2
 *---------------------------------------------------------------------------*/
static void
fkey2(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[24~"; /* F12 */
	else
		do_vgapage(1);
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 3
 *---------------------------------------------------------------------------*/
static void
fkey3(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[25~"; /* F13 */
	else
		do_vgapage(2);
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 4
 *---------------------------------------------------------------------------*/
static void
fkey4(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[26~"; /* F14 */
	else
		do_vgapage(3);
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 5
 *---------------------------------------------------------------------------*/
static void
fkey5(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[28~"; /* Help */
	else
	{
		if((current_video_screen + 1) > totalscreens-1)
			do_vgapage(0);
		else
			do_vgapage(current_video_screen + 1);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 6
 *---------------------------------------------------------------------------*/
static void
fkey6(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[29~"; /* DO */
	else
		more_chars = (u_char *)"\033[17~"; /* F6 */
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 7
 *---------------------------------------------------------------------------*/
static void
fkey7(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[31~"; /* F17 */
	else
		more_chars = (u_char *)"\033[18~"; /* F7 */
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 8
 *---------------------------------------------------------------------------*/
static void
fkey8(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[32~"; /* F18 */
	else
		more_chars = (u_char *)"\033[19~"; /* F8 */
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 9
 *---------------------------------------------------------------------------*/
static void
fkey9(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[33~"; /* F19 */
	else
		more_chars = (u_char *)"\033[20~"; /* F9 */
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 10
 *---------------------------------------------------------------------------*/
static void
fkey10(void)
{
	if(meta_down)
		more_chars = (u_char *)"\033[34~"; /* F20 */
	else
		more_chars = (u_char *)"\033[21~"; /* F10 */
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 11
 *---------------------------------------------------------------------------*/
static void
fkey11(void)
{
	if(meta_down)
		more_chars = (u_char *)"\0x8FP"; /* PF1 */
	else
		more_chars = (u_char *)"\033[23~"; /* F11 */
}

/*---------------------------------------------------------------------------*
 *	function bound to function key 12
 *---------------------------------------------------------------------------*/
static void
fkey12(void)
{
	if(meta_down)
		more_chars = (u_char *)"\0x8FQ"; /* PF2 */
	else
		more_chars = (u_char *)"\033[24~"; /* F12 */
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 1
 *---------------------------------------------------------------------------*/
static void
sfkey1(void)
{
	if(meta_down)
	{
		if(vsp->ukt.length[6])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[6]]);
		else
			more_chars = (u_char *)"\033[23~"; /* F11 */
	}
	else
	{
		do_vgapage(4);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 2
 *---------------------------------------------------------------------------*/
static void
sfkey2(void)
{
	if(meta_down)
	{
		if(vsp->ukt.length[7])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[7]]);
		else
			more_chars = (u_char *)"\033[24~"; /* F12 */
	}
	else
	{
		do_vgapage(5);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 3
 *---------------------------------------------------------------------------*/
static void
sfkey3(void)
{
	if(meta_down)
	{
		if(vsp->ukt.length[8])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[8]]);
		else
			more_chars = (u_char *)"\033[25~"; /* F13 */
	}
	else
	{
		do_vgapage(6);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 4
 *---------------------------------------------------------------------------*/
static void
sfkey4(void)
{
	if(meta_down)
	{
		if(vsp->ukt.length[9])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[9]]);
		else
			more_chars = (u_char *)"\033[26~"; /* F14 */
	}
	else
	{
		do_vgapage(7);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 5
 *---------------------------------------------------------------------------*/
static void
sfkey5(void)
{
	if(meta_down)
	{
		if(vsp->ukt.length[11])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[11]]);
		else
			more_chars = (u_char *)"\033[28~"; /* Help */
	}
	else
	{
		if(current_video_screen <= 0)
			do_vgapage(totalscreens-1);
		else
			do_vgapage(current_video_screen - 1);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 6
 *---------------------------------------------------------------------------*/
static void
sfkey6(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[0])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[0]]);
		else
			more_chars = (u_char *)"\033[17~"; /* F6 */
	}
	else if(vsp->ukt.length[12])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[12]]);
	     else
			more_chars = (u_char *)"\033[29~"; /* DO */
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 7
 *---------------------------------------------------------------------------*/
static void
sfkey7(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[1])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[1]]);
		else
			more_chars = (u_char *)"\033[18~"; /* F7 */
	}
	else if(vsp->ukt.length[14])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[14]]);
	     else
			more_chars = (u_char *)"\033[31~"; /* F17 */
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 8
 *---------------------------------------------------------------------------*/
static void
sfkey8(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[2])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[2]]);
		else
			more_chars = (u_char *)"\033[19~"; /* F8 */
	}
	else if(vsp->ukt.length[14])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[15]]);
	     else
			more_chars = (u_char *)"\033[32~"; /* F18 */
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 9
 *---------------------------------------------------------------------------*/
static void
sfkey9(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[3])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[3]]);
		else
			more_chars = (u_char *)"\033[20~"; /* F9 */
	}
	else if(vsp->ukt.length[16])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[16]]);
	     else
			more_chars = (u_char *)"\033[33~"; /* F19 */
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 10
 *---------------------------------------------------------------------------*/
static void
sfkey10(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[4])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[4]]);
		else
			more_chars = (u_char *)"\033[21~"; /* F10 */
	}
	else if(vsp->ukt.length[17])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[17]]);
	     else
			more_chars = (u_char *)"\033[34~"; /* F20 */
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 11
 *---------------------------------------------------------------------------*/
static void
sfkey11(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[6])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[6]]);
		else
			more_chars = (u_char *)"\033[23~"; /* F11 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to SHIFTED function key 12
 *---------------------------------------------------------------------------*/
static void
sfkey12(void)
{
	if(!meta_down)
	{
		if(vsp->ukt.length[7])	/* entry available ? */
			more_chars = (u_char *)
				&(vsp->udkbuf[vsp->ukt.first[7]]);
		else
			more_chars = (u_char *)"\033[24~"; /* F12 */
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 1
 *---------------------------------------------------------------------------*/
static void
cfkey1(void)
{
	if(vsp->which_fkl == SYS_FKL)
		toggl_columns(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 2
 *---------------------------------------------------------------------------*/
static void
cfkey2(void)
{
	if(vsp->which_fkl == SYS_FKL)
		vt_ris(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 3
 *---------------------------------------------------------------------------*/
static void
cfkey3(void)
{
	if(vsp->which_fkl == SYS_FKL)
		toggl_24l(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 4
 *---------------------------------------------------------------------------*/
static void
cfkey4(void)
{

#if PCVT_SHOWKEYS
	if(vsp->which_fkl == SYS_FKL)
		toggl_kbddbg(vsp);
#endif /* PCVT_SHOWKEYS */

}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 5
 *---------------------------------------------------------------------------*/
static void
cfkey5(void)
{
	if(vsp->which_fkl == SYS_FKL)
		toggl_bell(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 6
 *---------------------------------------------------------------------------*/
static void
cfkey6(void)
{
	if(vsp->which_fkl == SYS_FKL)
		toggl_sevenbit(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 7
 *---------------------------------------------------------------------------*/
static void
cfkey7(void)
{
	if(vsp->which_fkl == SYS_FKL)
		toggl_dspf(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 8
 *---------------------------------------------------------------------------*/
static void
cfkey8(void)
{
	if(vsp->which_fkl == SYS_FKL)
		toggl_awm(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 9
 *---------------------------------------------------------------------------*/
static void
cfkey9(void)
{
	if(vsp->labels_on)	/* toggle label display on/off */
	        fkl_off(vsp);
	else
	        fkl_on(vsp);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 10
 *---------------------------------------------------------------------------*/
static void
cfkey10(void)
{
	if(vsp->labels_on)	/* toggle user/system fkey labels */
	{
		if(vsp->which_fkl == USR_FKL)
			sw_sfkl(vsp);
		else if(vsp->which_fkl == SYS_FKL)
			sw_ufkl(vsp);
	}
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 11
 *---------------------------------------------------------------------------*/
static void
cfkey11(void)
{
	if(vsp->vt_pure_mode == M_PUREVT)
	        set_emulation_mode(vsp, M_HPVT);
	else if(vsp->vt_pure_mode == M_HPVT)
	        set_emulation_mode(vsp, M_PUREVT);
}

/*---------------------------------------------------------------------------*
 *	function bound to control function key 12
 *---------------------------------------------------------------------------*/
static void
cfkey12(void)
{
}

#endif	/* PCVT_VT220KEYB */

#endif	/* NVT > 0 */

/* ------------------------------- EOF -------------------------------------*/
