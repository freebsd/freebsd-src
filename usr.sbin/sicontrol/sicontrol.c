/*
 * Device driver for Specialix range (SLXOS) of serial line multiplexors.
 *	SLXOS configuration and debug interface
 *
 * Copyright (C) 1990, 1992 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 1995, Peter Wemm <peter@haywire.dialix.com>
 *
 * Derived from:	SunOS 4.x version
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Andy Rutter of
 *	Advanced Methods and Tools Ltd. based on original information
 *	from Specialix International.
 * 4. Neither the name of Advanced Methods and Tools, nor Specialix
 *    International may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 *	$Id: sicontrol.c,v 1.4 1995/10/01 03:13:33 peter Exp $
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include <machine/si.h>

struct lv {
	char	*lv_name;
	int 	lv_bit;
} lv[] = {
	"entry",	DBG_ENTRY,
	"open",		DBG_OPEN,
	"close",	DBG_CLOSE,
	"read",		DBG_READ,
	"write",	DBG_WRITE,
	"param",	DBG_PARAM,
	"modem",	DBG_MODEM,
	"select",	DBG_SELECT,
	"optim",	DBG_OPTIM,
	"intr",		DBG_INTR,
	"start",	DBG_START,
	"lstart",	DBG_LSTART,
	"ioctl",	DBG_IOCTL,
	"fail",		DBG_FAIL,
	"autoboot",	DBG_AUTOBOOT,
	"download",	DBG_DOWNLOAD,
	"drain",	DBG_DRAIN,
	"poll",		DBG_POLL,
	0,		0
};

static int alldev = 0;
int getnum(char *str);

int debug(), rxint(), txint();
int mstate(), nport();
int ccb_stat(), tty_stat();
struct opt {
	char	*o_name;
	int	(*o_func)();
} opt[] = {
	"debug",		debug,
	"rxint_throttle",	rxint,
	"int_throttle",		txint,
	"nport",		nport,
	"mstate",		mstate,
	"ccbstat",		ccb_stat,
	"ttystat",		tty_stat,
	0,			0,
};

struct stat_list {
	int (*st_func)();
} stat_list[] = {
	mstate,
	0
};

#define	U_DEBUG		0
#define	U_TXINT		1
#define	U_RXINT		2
#define	U_NPORT		3
#define	U_MSTATE	4
#define	U_STAT_CCB	5
#define	U_STAT_TTY	6

#define	U_MAX		7
#define	U_ALL		-1
char *usage[] = {
	"debug [[add|del|set debug_levels] | [off]]\n",
	"int_throttle [newvalue]\n",
	"rxint_throttle [newvalue]\n",
	"nport\n",
	"mstate\n",
	"ccbstat\n",
	"ttystat\n",
	0
};

int ctlfd;
char *Devname;
struct si_tcsi tc;

main(argc, argv)
	char **argv;
{
	struct opt *op;
	int (*func)() = NULL;

	if (argc < 2)
		prusage(U_ALL, 1);
	Devname = argv[1];
	if (strcmp(Devname, "-") == 0) {
		alldev = 1;
	} else {
		sidev_t dev;
		struct stat st;

		if (strchr(Devname, '/') == NULL) {
			char *acp = malloc(6 + strlen(Devname));
			strcpy(acp, "/dev/");
			strcat(acp, Devname);
			Devname = acp;
		}
		if (stat(Devname, &st) < 0) {
			fprintf(stderr, "can't stat %s\n", Devname);
			exit(1);
		}
		dev.sid_card = SI_CARD(minor(st.st_rdev));
		dev.sid_port = SI_PORT(minor(st.st_rdev));
		tc.tc_dev = dev;
	}
	ctlfd = opencontrol();
	if (argc == 2) {
		dostat();
		exit(0);
	}

	argc--; argv++;
	for (op = opt; op->o_name; op++) {
		if (strcmp(argv[1], op->o_name) == 0) {
			func = op->o_func;
			break;
		}
	}
	if (func == NULL)
		prusage(U_ALL, 1);

	argc -= 2;
	argv += 2;
	(*func)(argc, argv);
	exit(0);
}

opencontrol()
{
	int fd;

	fd = open(CONTROLDEV, O_RDWR|O_NDELAY);
	if (fd < 0) {
		fprintf(stderr, "open on %s - ", CONTROLDEV);
		perror("");
		exit(1);
	}
	return(fd);
}

/*
 * Print a usage message - this relies on U_DEBUG==0 and U_BOOT==1.
 * Don't print the DEBUG usage string unless explicity requested.
 */
prusage(strn, eflag)
	int strn, eflag;
{
	char **cp;

	if (strn == U_ALL) {
		fprintf(stderr, "Usage: sicontrol - %s", usage[1]);
		fprintf(stderr, "       sicontrol - %s", usage[2]);
		fprintf(stderr, "       sicontrol - %s", usage[3]);
		fprintf(stderr, "       sicontrol devname %s", usage[4]);
		for (cp = &usage[5]; *cp; cp++)
			fprintf(stderr, "       sicontrol devname %s", *cp);
	}
	else if (strn >= 0 && strn <= U_MAX)
		fprintf(stderr, "Usage: sicontrol devname %s", usage[strn]);
	else
		fprintf(stderr, "sicontrol: usage ???\n");
	exit(eflag);
}

/* print port status */
dostat()
{
	char *av[1], *acp;
	struct stat_list *stp;
	struct si_tcsi stc;
	int (*func)();
	int donefirst = 0;

	printf("%s: ", alldev ? "ALL" : Devname);
	acp = malloc(strlen(Devname) + 3);
	memset(acp, ' ', strlen(Devname));
	strcat(acp, "  ");
	stc = tc;
	for (stp = stat_list; stp->st_func != NULL; stp++) {
		if (donefirst)
			fputs(acp, stdout);
		else
			donefirst++;
		av[0] = NULL;
		tc = stc;
		(*stp->st_func)(-1, av);
	}
}

/*
 * debug
 * debug [[set|add|del debug_lvls] | [off]]
 */
debug(ac, av)
	char **av;
{
	int level, cmd;
	char *str;

	if (ac > 2)
		prusage(U_DEBUG, 1);
	if (alldev) {
		if (ioctl(ctlfd, TCSIGDBG_ALL, &tc.tc_dbglvl) < 0)
			Perror("TCSIGDBG_ALL on %s", Devname);
	} else {
		if (ioctl(ctlfd, TCSIGDBG_LEVEL, &tc) < 0)
			Perror("TCSIGDBG_LEVEL on %s", Devname);
	}

	switch (ac) {
	case 0:
		printf("%s: debug levels - ", Devname);
		prlevels(tc.tc_dbglvl);
		return;
	case 1:
		if (strcmp(av[0], "off") == 0) {
			tc.tc_dbglvl = 0;
			break;
		}
		prusage(U_DEBUG, 1);
		/* no return */
	case 2:
		level = lvls2bits(av[1]);
		if (strcmp(av[0], "add") == 0)
			tc.tc_dbglvl |= level;
		else if (strcmp(av[0], "del") == 0)
			tc.tc_dbglvl &= ~level;
		else if (strcmp(av[0], "set") == 0)
			tc.tc_dbglvl = level;
		else
			prusage(U_DEBUG, 1);
	}
	if (alldev) {
		if (ioctl(ctlfd, TCSISDBG_ALL, &tc.tc_dbglvl) < 0)
			Perror("TCSISDBG_ALL on %s", Devname);
	} else {
		if (ioctl(ctlfd, TCSISDBG_LEVEL, &tc) < 0)
			Perror("TCSISDBG_LEVEL on %s", Devname);
	}
}

rxint(ac, av)
	char **av;
{
	tc.tc_port = 0;
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		if (ioctl(ctlfd, TCSIGRXIT, &tc) < 0)
			Perror("TCSIGRXIT");
		printf("RX interrupt throttle: %d msec\n", tc.tc_int*10);
		break;
	case 1:
		tc.tc_int = getnum(av[0]) / 10;
		if (tc.tc_int == 0)
			tc.tc_int = 1;
		if (ioctl(ctlfd, TCSIRXIT, &tc) < 0)
			Perror("TCSIRXIT on %s at %d msec", Devname, tc.tc_int*10);
		break;
	default:
		prusage(U_RXINT, 1);
	}
}

txint(ac, av)
	char **av;
{

	tc.tc_port = 0;
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		if (ioctl(ctlfd, TCSIGIT, &tc) < 0)
			Perror("TCSIGIT");
		printf("aggregate interrupt throttle: %d\n", tc.tc_int);
		break;
	case 1:
		tc.tc_int = getnum(av[0]);
		if (ioctl(ctlfd, TCSIIT, &tc) < 0)
			Perror("TCSIIT on %s at %d", Devname, tc.tc_int);
		break;
	default:
		prusage(U_TXINT, 1);
	}
}

onoff(ac, av, cmd, cmdstr, prstr, usage)
	char **av, *cmdstr, *prstr;
	int ac, cmd, usage;
{
	if (ac > 1)
		prusage(usage, 1);
	if (ac == 1) {
		if (strcmp(av[0], "on") == 0)
			tc.tc_int = 1;
		else if (strcmp(av[0], "off") == 0)
			tc.tc_int = 0;
		else
			prusage(usage, 1);
	} else
		tc.tc_int = -1;
	if (ioctl(ctlfd, cmd, &tc) < 0)
		Perror("%s on %s", cmdstr, Devname);
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		printf("%s ", prstr);
		if (tc.tc_int)
			printf("on\n");
		else
			printf("off\n");
	}
}

mstate(ac, av)
	char **av;
{
	u_char state;

	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		break;
	default:
		prusage(U_MSTATE, 1);
	}
	if (ioctl(ctlfd, TCSISTATE, &tc) < 0)
		Perror("TCSISTATE on %s", Devname);
	printf("modem bits state - (0x%x)", tc.tc_int);
	if (tc.tc_int & IP_DCD)	printf(" DCD");
	if (tc.tc_int & IP_DTR)	printf(" DTR");
	if (tc.tc_int & IP_RTS)	printf(" RTS");
	printf("\n");
}

nport(ac, av)
	char **av;
{
	int ports;

	if (ac != 0)
		prusage(U_NPORT, 1);
	if (ioctl(ctlfd, TCSIPORTS, &ports) < 0)
		Perror("TCSIPORTS on %s", Devname);
	printf("SLXOS: total of %d ports\n", ports);
}

ccb_stat(ac, av)
char **av;
{
	struct si_pstat sip;
#define	CCB	sip.tc_ccb

	if (ac != 0)
		prusage(U_STAT_CCB);
	sip.tc_dev = tc.tc_dev;
	if (ioctl(ctlfd, TCSI_CCB, &sip) < 0)
		Perror("TCSI_CCB on %s", Devname);
	printf("%s: ", Devname);

							/* WORD	next - Next Channel */
							/* WORD	addr_uart - Uart address */
							/* WORD	module - address of module struct */
	printf("\tuart_type 0x%x\n", CCB.type);		/* BYTE type - Uart type */
							/* BYTE	fill - */
	printf("\tx_status 0x%x\n", CCB.x_status);	/* BYTE	x_status - XON / XOFF status */
	printf("\tc_status 0x%x\n", CCB.c_status);	/* BYTE	c_status - cooking status */
	printf("\thi_rxipos 0x%x\n", CCB.hi_rxipos);	/* BYTE	hi_rxipos - stuff into rx buff */
	printf("\thi_rxopos 0x%x\n", CCB.hi_rxopos);	/* BYTE	hi_rxopos - stuff out of rx buffer */
	printf("\thi_txopos 0x%x\n", CCB.hi_txopos);	/* BYTE	hi_txopos - Stuff into tx ptr */
	printf("\thi_txipos 0x%x\n", CCB.hi_txipos);	/* BYTE	hi_txipos - ditto out */
	printf("\thi_stat 0x%x\n", CCB.hi_stat);		/* BYTE	hi_stat - Command register */
	printf("\tdsr_bit 0x%x\n", CCB.dsr_bit);		/* BYTE	dsr_bit - Magic bit for DSR */
	printf("\ttxon 0x%x\n", CCB.txon);		/* BYTE	txon - TX XON char */
	printf("\ttxoff 0x%x\n", CCB.txoff);		/* BYTE	txoff - ditto XOFF */
	printf("\trxon 0x%x\n", CCB.rxon);		/* BYTE	rxon - RX XON char */
	printf("\trxoff 0x%x\n", CCB.rxoff);		/* BYTE	rxoff - ditto XOFF */
	printf("\thi_mr1 0x%x\n", CCB.hi_mr1);		/* BYTE	hi_mr1 - mode 1 image */
	printf("\thi_mr2 0x%x\n", CCB.hi_mr2);		/* BYTE	hi_mr2 - mode 2 image */
        printf("\thi_csr 0x%x\n", CCB.hi_csr);		/* BYTE	hi_csr - clock register */
	printf("\thi_op 0x%x\n", CCB.hi_op);		/* BYTE	hi_op - Op control */
	printf("\thi_ip 0x%x\n", CCB.hi_ip);		/* BYTE	hi_ip - Input pins */
	printf("\thi_state 0x%x\n", CCB.hi_state);	/* BYTE	hi_state - status */
	printf("\thi_prtcl 0x%x\n", CCB.hi_prtcl);	/* BYTE	hi_prtcl - Protocol */
	printf("\thi_txon 0x%x\n", CCB.hi_txon);		/* BYTE	hi_txon - host copy tx xon stuff */
	printf("\thi_txoff 0x%x\n", CCB.hi_txoff);	/* BYTE	hi_txoff - */
	printf("\thi_rxon 0x%x\n", CCB.hi_rxon);		/* BYTE	hi_rxon - */
	printf("\thi_rxoff 0x%x\n", CCB.hi_rxoff);	/* BYTE	hi_rxoff - */
	printf("\tclose_prev 0x%x\n", CCB.close_prev);	/* BYTE	close_prev - Was channel previously closed */
	printf("\thi_break 0x%x\n", CCB.hi_break);	/* BYTE	hi_break - host copy break process */
	printf("\tbreak_state 0x%x\n", CCB.break_state);	/* BYTE	break_state - local copy ditto */
	printf("\thi_mask 0x%x\n", CCB.hi_mask);		/* BYTE	hi_mask - Mask for CS7 etc. */
	printf("\tmask_z280 0x%x\n", CCB.mask_z280);	/* BYTE	mask_z280 - Z280's copy */
							/* BYTE	res[0x60 - 36] - */
							/* BYTE	hi_txbuf[SLXOS_BUFFERSIZE] - */
							/* BYTE	hi_rxbuf[SLXOS_BUFFERSIZE] - */
							/* BYTE	res1[0xA0] - */

}

tty_stat(ac, av)
char **av;
{
	struct si_pstat sip;
#define	TTY	sip.tc_tty

	if (ac != 0)
		prusage(U_STAT_TTY);
	sip.tc_dev = tc.tc_dev;
	if (ioctl(ctlfd, TCSI_TTY, &sip) < 0)
		Perror("TCSI_TTY on %s", Devname);
	printf("%s: ", Devname);

	printf("\tt_outq.c_cc %d.\n", TTY.t_outq.c_cc);	/* struct clist t_outq */
	printf("\tt_dev 0x%x\n", TTY.t_dev);		/* dev_t t_dev */
	printf("\tt_flags 0x%x\n", TTY.t_flags);	/* int	t_flags */
	printf("\tt_state 0x%x\n", TTY.t_state);	/* int	t_state */
	printf("\tt_hiwat %d.\n", TTY.t_hiwat);		/* short t_hiwat */
	printf("\tt_lowat %d.\n", TTY.t_lowat);		/* short t_lowat */
	printf("\tt_iflag 0x%x\n", TTY.t_iflag);	/* t_iflag */
	printf("\tt_oflag 0x%x\n", TTY.t_oflag);	/* t_oflag */
	printf("\tt_cflag 0x%x\n", TTY.t_cflag);	/* t_cflag */
	printf("\tt_lflag 0x%x\n", TTY.t_lflag);	/* t_lflag */
	printf("\tt_cc 0x%x\n", TTY.t_cc);		/* t_cc */
	printf("\tt_termios.c_ispeed 0x%x\n", TTY.t_termios.c_ispeed);	/* t_termios.c_ispeed */
	printf("\tt_termios.c_ospeed 0x%x\n", TTY.t_termios.c_ospeed);	/* t_termios.c_ospeed */

}

islevel(tk)
	char *tk;
{
	register struct lv *lvp;
	register char *acp;

	for (acp = tk; *acp; acp++)
		if (isupper(*acp))
			*acp = tolower(*acp);
	for (lvp = lv; lvp->lv_name; lvp++)
		if (strcmp(lvp->lv_name, tk) == 0)
			return(lvp->lv_bit);
	return(0);
}

/*
 * Convert a string consisting of tokens separated by white space, commas
 * or `|' into a bitfield - flag any unrecognised tokens.
 */
lvls2bits(str)
	char *str;
{
	int i, bits = 0;
	int errflag = 0;
	char token[20];

	while (sscanf(str, "%[^,| \t]", token) == 1) {
		str += strlen(token);
		while (isspace(*str) || *str==',' || *str=='|')
			str++;
		if (strcmp(token, "all") == 0)
			return(0xffffffff);
		if ((i = islevel(token)) == 0) {
			fprintf(stderr, "sicontrol: unknown token '%s'\n", token);
			errflag++;
		} else
			bits |= i;
	}
	if (errflag)
		exit(1);

	return(bits);
}

int
getnum(char *str)
{
	int x;
	register char *acp = str;

	x = 0;
	while (*acp) {
		if (!isdigit(*acp)) {
			error("%s is not a number\n", str);
			exit(1);
		}
		x *= 10;
		x += (*acp - '0');
		acp++;
	}
	return(x);
}

prlevels(x)
	int x;
{
	struct lv *lvp;

	switch (x) {
	case 0:
		printf("(none)\n");
		break;
	case 0xffffffff:
		printf("all\n");
		break;
	default:
		for (lvp = lv; lvp->lv_name; lvp++)
			if (x & lvp->lv_bit)
				printf(" %s", lvp->lv_name);
		printf("\n");
	}
}

/* VARARGS */
Perror(str, a1, a2, a3, a4, a5)
	char *str;
	int a1, a2, a3, a4, a5;
{
	fprintf(stderr, str, a1, a2, a3, a4, a5);
	perror(" ");
	exit(1);
}

/* VARARGS */
error(str, a1, a2, a3, a4, a5)
	char *str;
	int a1, a2, a3, a4, a5;
{
	fprintf(stderr, str, a1, a2, a3, a4, a5);
	exit(1);
}
