/*
 * Cronyx-Sigma adapter configuration utility for Unix.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.9, Wed Oct  4 18:58:15 MSK 1995
 *
 * Usage:
 *      cxconfig [-a]
 *              -- print status of all channels
 *      cxconfig [-a] <channel>
 *              -- print status of the channel
 *      cxconfig <channel> <option>...
 *              -- set channel options
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <machine/cronyx.h>
#include <net/if.h>
#include <stdio.h>

#define NBRD    3
#define CXDEV   "/dev/cronyx"
#define atoi(a) strtol((a), (char**)0, 0)

cx_options_t o;
cx_stat_t st;
int aflag;
int sflag;

char *symbol (unsigned char sym)
{
	static char buf[40];

	if (sym < ' ')
		sprintf (buf, "^%c", sym+0100);
	else if (sym == '\\')
		strcat (buf, "\\\\");
	else if (sym < 127)
		sprintf (buf, "%c", sym);
	else
		sprintf (buf, "\\%03o", sym);
	return (buf);
}

unsigned char atosym (char *s)
{
	unsigned char c;

	if (*s == '^')
		return (*++s & 037);
	if (*s == '\\')
		return (strtol (++s, 0, 8));
	return (*s);
}

void usage ()
{
	printf ("Cronyx-Sigma Adapter Configuration Utility, Version 1.0\n");
	printf ("Copyright (C) 1994 Cronyx Ltd.\n");
	printf ("Usage:\n");
	printf ("\tcxconfig [-a]\n");
	printf ("\t\t-- print status of all channels\n");
	printf ("\tcxconfig [-a] <channel>\n");
	printf ("\t\t-- print status of the channel\n");
	printf ("\tcxconfig <channel> [async | hdlc | bisync | x.21] [ispeed #] [ospeed #]\n");
	printf ("\t\t[+cts | -cts]\n");
	printf ("\t\t-- set channel options\n");
	exit (1);
}

char *chantype (int type)
{
	switch (type) {
	case T_NONE:       return ("none");
	case T_ASYNC:      return ("RS-232");
	case T_UNIV_RS232: return ("RS-232");
	case T_UNIV_RS449: return ("RS-232/RS-449");
	case T_UNIV_V35:   return ("RS-232/V.35");
	case T_SYNC_RS232: return ("RS-232");
	case T_SYNC_V35:   return ("V.35");
	case T_SYNC_RS449: return ("RS-449");
	}
}

char *chanmode (int mode)
{
	switch (mode) {
	case M_ASYNC:   return ("Async");
	case M_HDLC:    return ("HDLC");
	case M_BISYNC:  return ("Bisync");
	case M_X21:     return ("X.21");
	default:        return ("???");
	}
}

void getchan (int channel)
{
	int s = open (CXDEV, 0);
	if (s < 0) {
		perror (CXDEV);
		exit (1);
	}
	o.board = channel/NCHAN;
	o.channel = channel%NCHAN;
	if (ioctl (s, CXIOCGETMODE, (caddr_t)&o) < 0) {
		perror ("cxconfig: CXIOCGETMODE");
		exit (1);
	}
	close (s);
	if (o.type == T_NONE) {
		fprintf (stderr, "cx%d: channel %d not configured\n", o.board,
			o.channel);
		exit (1);
	}
}

int printstats (int channel, int hflag)
{
	int s, res;

	s = open (CXDEV, 0);
	if (s < 0) {
		perror (CXDEV);
		exit (1);
	}
	st.board = channel/NCHAN;
	st.channel = channel%NCHAN;
	res = ioctl (s, CXIOCGETSTAT, (caddr_t)&st);
	close (s);
	if (res < 0)
		return (-1);

	if (hflag)
		printf ("Chan   Rintr   Tintr   Mintr   Ibytes   Ipkts   Ierrs   Obytes   Opkts   Oerrs\n");
	printf ("cx%-2d %7ld %7ld %7ld %8ld %7ld %7ld %8ld %7ld %7ld\n",
		channel, st.rintr, st.tintr, st.mintr, st.ibytes, st.ipkts,
		st.ierrs, st.obytes, st.opkts, st.oerrs);
	return (0);
}

void printallstats ()
{
	int b, c;

	printf ("Chan   Rintr   Tintr   Mintr   Ibytes   Ipkts   Ierrs   Obytes   Opkts   Oerrs\n");
	for (b=0; b<NBRD; ++b)
		for (c=0; c<NCHAN; ++c)
			printstats (b*NCHAN + c, 0);
}

void setchan (int channel)
{
	int s = open (CXDEV, 0);
	if (s < 0) {
		perror (CXDEV);
		exit (1);
	}
	o.board = channel/NCHAN;
	o.channel = channel%NCHAN;
	if (ioctl (s, CXIOCSETMODE, (caddr_t)&o) < 0) {
		perror ("cxconfig: CXIOCSETMODE");
		exit (1);
	}
	close (s);
}

void printopt ()
{
	/* Common channel options */
	/* channel option register 4 */
	printf ("\t");
	printf ("fifo=%d ", o.opt.cor4.thr);    /* FIFO threshold */
	printf ("%cctsdown ", o.opt.cor4.cts_zd ? '+' : '-');   /* detect 1 to 0 transition on the CTS */
	printf ("%ccddown ", o.opt.cor4.cd_zd ? '+' : '-');     /* detect 1 to 0 transition on the CD */
	printf ("%cdsrdown ", o.opt.cor4.dsr_zd ? '+' : '-');   /* detect 1 to 0 transition on the DSR */
	printf ("\n");

	/* channel option register 5 */
	printf ("\t");
	printf ("rfifo=%d ", o.opt.cor5.rx_thr);        /* receive flow control FIFO threshold */
	printf ("%cctsup ", o.opt.cor5.cts_od ? '+' : '-');     /* detect 0 to 1 transition on the CTS */
	printf ("%ccdup ", o.opt.cor5.cd_od ? '+' : '-');       /* detect 0 to 1 transition on the CD */
	printf ("%cdsrup ", o.opt.cor5.dsr_od ? '+' : '-');     /* detect 0 to 1 transition on the DSR */
	printf ("\n");

	/* receive clock option register */
	printf ("\t");
	printf ("%s ", o.opt.rcor.encod == ENCOD_NRZ ? "nrz" :  /* signal encoding */
		o.opt.rcor.encod == ENCOD_NRZI ? "nrzi" :
		o.opt.rcor.encod == ENCOD_MANCHESTER ? "manchester" : "???");
	printf ("%cdpll ", o.opt.rcor.dpll ? '+' : '-');        /* DPLL enable */

	/* transmit clock option register */
	printf ("%clloop ", o.opt.tcor.llm ? '+' : '-');        /* local loopback mode */
	printf ("%cextclock ", o.opt.tcor.ext1x ? '+' : '-');   /* external 1x clock mode */
	printf ("\n");

	switch (o.mode) {
	case M_ASYNC:                           /* async mode options */
		/* channel option register 1 */
		printf ("\t");
		printf ("cs%d ", o.aopt.cor1.charlen+1);        /* character length, 5..8 */
		printf ("par%s ", o.aopt.cor1.parity ? "odd" : "even"); /* parity */
		printf ("%cignpar ", o.aopt.cor1.ignpar ? '+' : '-');   /* ignore parity */
		if (o.aopt.cor1.parmode != PARM_NORMAL)                 /* parity mode */
			printf ("%s ", o.aopt.cor1.parmode == PARM_NOPAR ? "nopar" :
				o.aopt.cor1.parmode == PARM_FORCE ? "forcepar" : "???");
		printf ("\n");

		/* channel option register 2 */
		printf ("\t");
		printf ("%cdsr ", o.aopt.cor2.dsrae ? '+' : '-');  /* DSR automatic enable */
		printf ("%ccts ", o.aopt.cor2.ctsae ? '+' : '-');  /* CTS automatic enable */
		printf ("%crts ", o.aopt.cor2.rtsao ? '+' : '-');  /* RTS automatic output enable */
		printf ("%crloop ", o.aopt.cor2.rlm ? '+' : '-');  /* remote loopback mode enable */
		printf ("%cetc ", o.aopt.cor2.etc ? '+' : '-');    /* embedded transmitter cmd enable */
		printf ("%cxon ", o.aopt.cor2.ixon ? '+' : '-');   /* in-band XON/XOFF enable */
		printf ("%cxany ", o.aopt.cor2.ixany ? '+' : '-'); /* XON on any character */
		printf ("\n");

		/* option register 3 */
		printf ("\t");
		printf ("%s ", o.aopt.cor3.stopb == STOPB_1 ? "stopb1" : /* stop bit length */
			o.aopt.cor3.stopb == STOPB_15 ? "stopb1.5" :
			o.aopt.cor3.stopb == STOPB_2 ? "stopb2" : "???");
		printf ("%csdt ", o.aopt.cor3.scde ? '+' : '-');        /* special char detection enable */
		printf ("%cflowct ", o.aopt.cor3.flowct ? '+' : '-');   /* flow control transparency mode */
		printf ("%crdt ", o.aopt.cor3.rngde ? '+' : '-');       /* range detect enable */
		printf ("%cexdt ", o.aopt.cor3.escde ? '+' : '-');      /* extended spec. char detect enable */
		printf ("\n");

		/* channel option register 6 */
		printf ("\t");
		printf ("%s ", o.aopt.cor6.parerr == PERR_INTR ? "parintr" : /* parity/framing error actions */
			o.aopt.cor6.parerr == PERR_NULL ? "parnull" :
			o.aopt.cor6.parerr == PERR_IGNORE ? "parign" :
			o.aopt.cor6.parerr == PERR_DISCARD ? "pardisc" :
			o.aopt.cor6.parerr == PERR_FFNULL ? "parffnull" : "???");
		printf ("%s ", o.aopt.cor6.brk == BRK_INTR ? "brkintr" : /* action on break condition */
			o.aopt.cor6.brk == BRK_NULL ? "brknull" :
			o.aopt.cor6.brk == BRK_DISCARD ? "brkdisc" : "???");
		printf ("%cinlcr ", o.aopt.cor6.inlcr ? '+' : '-');     /* translate NL to CR on input */
		printf ("%cicrnl ", o.aopt.cor6.icrnl ? '+' : '-');     /* translate CR to NL on input */
		printf ("%cigncr ", o.aopt.cor6.igncr ? '+' : '-');     /* discard CR on input */
		printf ("\n");

		/* channel option register 7 */
		printf ("\t");
		printf ("%cocrnl ", o.aopt.cor7.ocrnl ? '+' : '-');     /* translate CR to NL on output */
		printf ("%conlcr ", o.aopt.cor7.onlcr ? '+' : '-');     /* translate NL to CR on output */
		printf ("%cfcerr ", o.aopt.cor7.fcerr ? '+' : '-');     /* process flow ctl err chars enable */
		printf ("%clnext ", o.aopt.cor7.lnext ? '+' : '-');     /* LNext option enable */
		printf ("%cistrip ", o.aopt.cor7.istrip ? '+' : '-');   /* strip 8-bit on input */
		printf ("\n");

		printf ("\t");
		printf ("schr1=%s ", symbol (o.aopt.schr1));    /* special character register 1 (XON) */
		printf ("schr2=%s ", symbol (o.aopt.schr2));    /* special character register 2 (XOFF) */
		printf ("schr3=%s ", symbol (o.aopt.schr3));    /* special character register 3 */
		printf ("schr4=%s ", symbol (o.aopt.schr4));    /* special character register 4 */
		printf ("scrl=%s ", symbol (o.aopt.scrl));      /* special character range low */
		printf ("scrh=%s ", symbol (o.aopt.scrh));      /* special character range high */
		printf ("lnext=%s ", symbol (o.aopt.lnxt));     /* LNext character */
		printf ("\n");
		break;

	case M_HDLC:                    /* hdlc mode options */
		/* hdlc channel option register 1 */
		printf ("\t");
		printf ("if%d ", o.hopt.cor1.ifflags);  /* number of inter-frame flags sent */
		printf ("%s ", o.hopt.cor1.admode == ADMODE_NOADDR ? "noaddr" : /* addressing mode */
			o.hopt.cor1.admode == ADMODE_4_1 ? "addr1" :
			o.hopt.cor1.admode == ADMODE_2_2 ? "addr2" : "???");
		printf ("%cclrdet ", o.hopt.cor1.clrdet ? '+' : '-'); /* clear detect for X.21 data transfer phase */
		printf ("addrlen%d ", o.hopt.cor1.aflo + 1);    /* address field length option */
		printf ("\n");

		/* hdlc channel option register 2 */
		printf ("\t");
		printf ("%cdsr ", o.hopt.cor2.dsrae ? '+' : '-'); /* DSR automatic enable */
		printf ("%ccts ", o.hopt.cor2.ctsae ? '+' : '-'); /* CTS automatic enable */
		printf ("%crts ", o.hopt.cor2.rtsao ? '+' : '-'); /* RTS automatic output enable */
		printf ("%ccrcinv ", o.hopt.cor2.crcninv ? '-' : '+');  /* CRC invertion option */
		printf ("%cfcsapd ", o.hopt.cor2.fcsapd ? '+' : '-');   /* FCS append */
		printf ("\n");

		/* hdlc channel option register 3 */
		printf ("\t");
		printf ("pad%d ", o.hopt.cor3.padcnt);  /* pad character count */
		printf ("idle%s ", o.hopt.cor3.idle ? "mark" : "flag"); /* idle mode */
		printf ("%cfcs ", o.hopt.cor3.nofcs ? '-' : '+');       /* FCS disable */
		printf ("fcs-%s ", o.hopt.cor3.fcspre ? "crc-16" : "v.41"); /* FCS preset */
		printf ("syn=%s ", o.hopt.cor3.syncpat ? "0xAA" : "0x00"); /* send sync pattern */
		printf ("%csyn ", o.hopt.cor3.sndpad ? '+' : '-');     /* send pad characters before flag enable */
		printf ("\n");

		printf ("\t");
		printf ("rfar1=0x%02x ", o.hopt.rfar1); /* receive frame address register 1 */
		printf ("rfar2=0x%02x ", o.hopt.rfar2); /* receive frame address register 2 */
		printf ("rfar3=0x%02x ", o.hopt.rfar3); /* receive frame address register 3 */
		printf ("rfar4=0x%02x ", o.hopt.rfar4); /* receive frame address register 4 */
		printf ("crc-%s ", o.hopt.cpsr ? "16" : "v.41"); /* CRC polynomial select */
		printf ("\n");
		break;

	case M_BISYNC:                  /* bisync mode options */
		/* channel option register 1 */
		printf ("\t");
		printf ("cs%d ", o.bopt.cor1.charlen+1);        /* character length, 5..8 */
		printf ("par%s ", o.bopt.cor1.parity ? "odd" : "even"); /* parity */
		printf ("%cignpar ", o.bopt.cor1.ignpar ? '+' : '-');   /* ignore parity */
		if (o.bopt.cor1.parmode != PARM_NORMAL)                 /* parity mode */
			printf ("%s ", o.bopt.cor1.parmode == PARM_NOPAR ? "nopar" :
				o.bopt.cor1.parmode == PARM_FORCE ? "forcepar" : "???");
		printf ("\n");

		/* channel option register 2 */
		printf ("\t");
		printf ("syn%d ", o.bopt.cor2.syns+2);                  /* number of extra SYN chars before a frame */
		printf ("%ccrcinv ", o.bopt.cor2.crcninv ? '-' : '+');  /* CRC invertion option */
		printf ("%s ", o.bopt.cor2.ebcdic ? "ebcdic" : "ascii"); /* use EBCDIC as char set (instead of ASCII) */
		printf ("%cbccapd ", o.bopt.cor2.bcc ? '+' : '-');      /* BCC append enable */
		printf ("%s ", o.bopt.cor2.lrc ? "lrc" : "crc-16");     /* longitudinal redundancy check */
		printf ("\n");

		/* channel option register 3 */
		printf ("\t");
		printf ("pad%d ", o.bopt.cor3.padcnt);  /* pad character count */
		printf ("idle%s ", o.bopt.cor3.idle ? "mark" : "syn"); /* idle mode */
		printf ("%cfcs ", o.bopt.cor3.nofcs ? '-' : '+');       /* FCS disable */
		printf ("fcs-%s ", o.bopt.cor3.fcspre ? "crc-16" : "v.41"); /* FCS preset */
		printf ("syn=%s ", o.bopt.cor3.padpat ? "0x55" : "0xAA"); /* send sync pattern */
		printf ("%csyn ", o.bopt.cor3.sndpad ? '+' : '-');     /* send pad characters before flag enable */
		printf ("\n");

		/* channel option register 6 */
		printf ("\t");
		printf ("specterm=%s ", symbol (o.bopt.cor6.specterm)); /* special termination character */

		printf ("crc-%s ", o.bopt.cpsr ? "16" : "v.41"); /* CRC polynomial select */
		printf ("\n");
		break;

	case M_X21:                     /* x.21 mode options */
		/* channel option register 1 */
		printf ("\t");
		printf ("cs%d ", o.xopt.cor1.charlen+1);        /* character length, 5..8 */
		printf ("par%s ", o.xopt.cor1.parity ? "odd" : "even"); /* parity */
		printf ("%cignpar ", o.xopt.cor1.ignpar ? '+' : '-');   /* ignore parity */
		if (o.xopt.cor1.parmode != PARM_NORMAL)                 /* parity mode */
			printf ("%s ", o.xopt.cor1.parmode == PARM_NOPAR ? "nopar" :
				o.xopt.cor1.parmode == PARM_FORCE ? "forcepar" : "???");
		printf ("\n");

		/* channel option register 2 */
		printf ("\t");
		printf ("%cetc ", o.xopt.cor2.etc ? '+' : '-'); /* embedded transmitter cmd enable */

		/* channel option register 3 */
		printf ("%csdt ", o.xopt.cor3.scde ? '+' : '-'); /* special char detection enable */
		printf ("%cstripsyn ", o.xopt.cor3.stripsyn ? '+' : '-'); /* treat SYN chars as special condition */
		printf ("%cssdt ", o.xopt.cor3.ssde ? '+' : '-'); /* steady state detect enable */
		printf ("syn%c ", o.xopt.cor3.syn ? '1' : '2'); /* the number of SYN chars on receive */
		printf ("\n");

		/* channel option register 6 */
		printf ("\t");
		printf ("syn=%s ", symbol (o.xopt.cor6.synchar)); /* syn character */

		printf ("schr1=%s ", symbol (o.xopt.schr1));    /* special character register 1 */
		printf ("schr2=%s ", symbol (o.xopt.schr2));    /* special character register 2 */
		printf ("schr3=%s ", symbol (o.xopt.schr3));    /* special character register 3 */
		printf ("\n");
		break;
	}
}

void printchan (int channel)
{
	printf ("cx%d (%s) %s", channel, chantype (o.type), chanmode (o.mode));
	if (o.txbaud == o.rxbaud)
		printf (" %d", o.rxbaud);
	else
		printf (" ospeed=%d ispeed=%d", o.txbaud, o.rxbaud);
	if ((o.channel == 0 || o.channel == 8) &&
	    (o.type == T_UNIV_V35 || o.type == T_UNIV_RS449))
		printf (" port=%s", o.iftype ? (o.type == T_UNIV_V35 ?
			"v35" : "rs449") : "rs232");
	printf (o.sopt.ext ? " ext" : o.sopt.cisco ? " cisco" : " ppp");
	printf (" %ckeepalive", o.sopt.keepalive ? '+' : '-');
	printf (" %cautorts", o.sopt.norts ? '-' : '+');
	if (*o.master)
		printf (" master=%s", o.master);
	printf ("\n");
	if (aflag)
		printopt ();
}

void printall ()
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[BUFSIZ], *cp;
	int s, c;

	s = socket (AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror ("cxconfig: socket");
		exit (1);
	}
        ifc.ifc_len = sizeof (buf);
        ifc.ifc_buf = buf;
	if (ioctl (s, SIOCGIFCONF, (caddr_t)&ifc) < 0) {
		perror ("cxconfig: SIOCGIFCONF");
		exit (1);
	}
	close (s);
	s = open (CXDEV, 0);
	if (s < 0) {
		perror (CXDEV);
		exit (1);
	}

        ifr = ifc.ifc_req;
#define max(a,b) ((a)>(b) ? (a) : (b))
#define size(p)	max((p).sa_len, sizeof(p))
	for (cp=buf; cp<buf+ifc.ifc_len; cp+=sizeof(ifr->ifr_name)+size(ifr->ifr_addr)) {
		ifr = (struct ifreq*) cp;
		if (ifr->ifr_addr.sa_family != AF_LINK)
			continue;
		if (strncmp (ifr->ifr_name, "cx", 2) != 0)
			continue;
		c = atoi (ifr->ifr_name + 2);
		o.board = c/NCHAN;
		o.channel = c%NCHAN;
		if (ioctl (s, CXIOCGETMODE, (caddr_t)&o) < 0) {
			perror ("cxconfig: CXIOCGETMODE");
			exit (1);
		}
		printchan (c);
	}
	close (s);
}

void set_interface_type (char *type)
{
	if (o.channel != 0 && o.channel != 8) {
		printf ("interface option is applicable only for channels 0 and 8\n");
		exit (1);
	}
	if (o.type != T_UNIV_V35 && o.type != T_UNIV_RS449) {
		printf ("interface option is applicable only for universal channels\n");
		exit (1);
	}
	if (! strcasecmp (type, "port=rs232"))
		o.iftype = 0;
	else
		o.iftype = 1;
}

void set_master (char *ifname)
{
	if (o.type == T_ASYNC) {
		printf ("master option is not applicable for async channels\n");
		exit (1);
	}
	strcpy (o.master, ifname);
}

void set_async_opt (char *opt)
{
	/* channel option register 1 */
	if (! strncasecmp (opt, "cs", 2))        o.aopt.cor1.charlen = atoi (opt + 2) - 1;
	else if (! strcasecmp (opt, "parodd"))   o.aopt.cor1.parity = 1;
	else if (! strcasecmp (opt, "pareven"))  o.aopt.cor1.parity = 0;
	else if (! strcasecmp (opt, "-ignpar"))  o.aopt.cor1.ignpar = 0;
	else if (! strcasecmp (opt, "+ignpar"))  o.aopt.cor1.ignpar = 1;
	else if (! strcasecmp (opt, "nopar"))    o.aopt.cor1.parmode = PARM_NOPAR;
	else if (! strcasecmp (opt, "forcepar")) o.aopt.cor1.parmode = PARM_FORCE;

	/* channel option register 2 */
	else if (! strcasecmp (opt, "-dsr"))   o.aopt.cor2.dsrae = 0;
	else if (! strcasecmp (opt, "+dsr"))   o.aopt.cor2.dsrae = 1;
	else if (! strcasecmp (opt, "-cts"))   o.aopt.cor2.ctsae = 0;
	else if (! strcasecmp (opt, "+cts"))   o.aopt.cor2.ctsae = 1;
	else if (! strcasecmp (opt, "-rts"))   o.aopt.cor2.rtsao = 0;
	else if (! strcasecmp (opt, "+rts"))   o.aopt.cor2.rtsao = 1;
	else if (! strcasecmp (opt, "-rloop")) o.aopt.cor2.rlm = 0;
	else if (! strcasecmp (opt, "+rloop")) o.aopt.cor2.rlm = 1;
	else if (! strcasecmp (opt, "-etc"))   o.aopt.cor2.etc = 0;
	else if (! strcasecmp (opt, "+etc"))   o.aopt.cor2.etc = 1;
	else if (! strcasecmp (opt, "-ixon"))  o.aopt.cor2.ixon = 0;
	else if (! strcasecmp (opt, "+ixon"))  o.aopt.cor2.ixon = 1;
	else if (! strcasecmp (opt, "-ixany")) o.aopt.cor2.ixany = 0;
	else if (! strcasecmp (opt, "+ixany")) o.aopt.cor2.ixany = 1;

	/* option register 3 */
	else if (! strcasecmp (opt, "stopb1"))   o.aopt.cor3.stopb = STOPB_1;
	else if (! strcasecmp (opt, "stopb1.5")) o.aopt.cor3.stopb = STOPB_15;
	else if (! strcasecmp (opt, "stopb2"))   o.aopt.cor3.stopb = STOPB_2;
	else if (! strcasecmp (opt, "-sdt"))     o.aopt.cor3.scde = 0;
	else if (! strcasecmp (opt, "+sdt"))     o.aopt.cor3.scde = 1;
	else if (! strcasecmp (opt, "-flowct"))  o.aopt.cor3.flowct = 0;
	else if (! strcasecmp (opt, "+flowct"))  o.aopt.cor3.flowct = 1;
	else if (! strcasecmp (opt, "-rdt"))     o.aopt.cor3.rngde = 0;
	else if (! strcasecmp (opt, "+rdt"))     o.aopt.cor3.rngde = 1;
	else if (! strcasecmp (opt, "-exdt"))    o.aopt.cor3.escde = 0;
	else if (! strcasecmp (opt, "+exdt"))    o.aopt.cor3.escde = 1;

	/* channel option register 6 */
	else if (! strcasecmp (opt, "parintr"))   o.aopt.cor6.parerr = PERR_INTR;
	else if (! strcasecmp (opt, "parnull"))   o.aopt.cor6.parerr = PERR_NULL;
	else if (! strcasecmp (opt, "parign"))    o.aopt.cor6.parerr = PERR_IGNORE;
	else if (! strcasecmp (opt, "pardisc"))   o.aopt.cor6.parerr = PERR_DISCARD;
	else if (! strcasecmp (opt, "parffnull")) o.aopt.cor6.parerr = PERR_FFNULL;
	else if (! strcasecmp (opt, "brkintr"))   o.aopt.cor6.brk = BRK_INTR;
	else if (! strcasecmp (opt, "brknull"))   o.aopt.cor6.brk = BRK_NULL;
	else if (! strcasecmp (opt, "brkdisc"))   o.aopt.cor6.brk = BRK_DISCARD;
	else if (! strcasecmp (opt, "-inlcr"))    o.aopt.cor6.inlcr = 0;
	else if (! strcasecmp (opt, "+inlcr"))    o.aopt.cor6.inlcr = 1;
	else if (! strcasecmp (opt, "-icrnl"))    o.aopt.cor6.icrnl = 0;
	else if (! strcasecmp (opt, "+icrnl"))    o.aopt.cor6.icrnl = 1;
	else if (! strcasecmp (opt, "-igncr"))    o.aopt.cor6.igncr = 0;
	else if (! strcasecmp (opt, "+igncr"))    o.aopt.cor6.igncr = 1;

	/* channel option register 7 */
	else if (! strcasecmp (opt, "-ocrnl"))    o.aopt.cor7.ocrnl = 0;
	else if (! strcasecmp (opt, "+ocrnl"))    o.aopt.cor7.ocrnl = 1;
	else if (! strcasecmp (opt, "-onlcr"))    o.aopt.cor7.onlcr = 0;
	else if (! strcasecmp (opt, "+onlcr"))    o.aopt.cor7.onlcr = 1;
	else if (! strcasecmp (opt, "-fcerr"))    o.aopt.cor7.fcerr = 0;
	else if (! strcasecmp (opt, "+fcerr"))    o.aopt.cor7.fcerr = 1;
	else if (! strcasecmp (opt, "-lnext"))    o.aopt.cor7.lnext = 0;
	else if (! strcasecmp (opt, "+lnext"))    o.aopt.cor7.lnext = 1;
	else if (! strcasecmp (opt, "-istrip"))   o.aopt.cor7.istrip = 0;
	else if (! strcasecmp (opt, "+istrip"))   o.aopt.cor7.istrip = 1;

	else if (! strncasecmp (opt, "schr1=", 6)) o.aopt.schr1 = atosym (opt+6);
	else if (! strncasecmp (opt, "schr2=", 6)) o.aopt.schr2 = atosym (opt+6);
	else if (! strncasecmp (opt, "schr3=", 6)) o.aopt.schr3 = atosym (opt+6);
	else if (! strncasecmp (opt, "schr4=", 6)) o.aopt.schr4 = atosym (opt+6);
	else if (! strncasecmp (opt, "scrl=", 5))  o.aopt.scrl = atosym (opt+5);
	else if (! strncasecmp (opt, "scrh=", 5))  o.aopt.scrh = atosym (opt+5);
	else if (! strncasecmp (opt, "lnext=", 6)) o.aopt.lnxt = atosym (opt+6);
	else
		usage ();
}

void set_hdlc_opt (char *opt)
{
	/* hdlc channel option register 1 */
	if (! strncasecmp (opt, "if", 2))        o.hopt.cor1.ifflags = atoi (opt + 2);
	else if (! strcasecmp (opt, "noaddr"))   o.hopt.cor1.admode = ADMODE_NOADDR;
	else if (! strcasecmp (opt, "addr1"))    o.hopt.cor1.admode = ADMODE_4_1;
	else if (! strcasecmp (opt, "addr2"))    o.hopt.cor1.admode = ADMODE_2_2;
	else if (! strcasecmp (opt, "-clrdet"))  o.hopt.cor1.clrdet = 0;
	else if (! strcasecmp (opt, "+clrdet"))  o.hopt.cor1.clrdet = 1;
	else if (! strcasecmp (opt, "addrlen1")) o.hopt.cor1.aflo = 0;
	else if (! strcasecmp (opt, "addrlen2")) o.hopt.cor1.aflo = 1;

	/* hdlc channel option register 2 */
	else if (! strcasecmp (opt, "-dsr"))    o.hopt.cor2.dsrae = 0;
	else if (! strcasecmp (opt, "+dsr"))    o.hopt.cor2.dsrae = 1;
	else if (! strcasecmp (opt, "-cts"))    o.hopt.cor2.ctsae = 0;
	else if (! strcasecmp (opt, "+cts"))    o.hopt.cor2.ctsae = 1;
	else if (! strcasecmp (opt, "-rts"))    o.hopt.cor2.rtsao = 0;
	else if (! strcasecmp (opt, "+rts"))    o.hopt.cor2.rtsao = 1;
	else if (! strcasecmp (opt, "-fcsapd")) o.hopt.cor2.fcsapd = 0;
	else if (! strcasecmp (opt, "+fcsapd")) o.hopt.cor2.fcsapd = 1;
	else if (! strcasecmp (opt, "-crcinv")) o.hopt.cor2.crcninv = 1;
	else if (! strcasecmp (opt, "+crcinv")) o.hopt.cor2.crcninv = 0;

	/* hdlc channel option register 3 */
	else if (! strncasecmp (opt, "pad", 3))    o.hopt.cor3.padcnt = atoi (opt + 3);
	else if (! strcasecmp (opt, "idlemark"))   o.hopt.cor3.idle = 1;
	else if (! strcasecmp (opt, "idleflag"))   o.hopt.cor3.idle = 0;
	else if (! strcasecmp (opt, "-fcs"))       o.hopt.cor3.nofcs = 1;
	else if (! strcasecmp (opt, "+fcs"))       o.hopt.cor3.nofcs = 0;
	else if (! strcasecmp (opt, "fcs-crc-16")) o.hopt.cor3.fcspre = 1;
	else if (! strcasecmp (opt, "fcs-v.41"))   o.hopt.cor3.fcspre = 0;
	else if (! strcasecmp (opt, "syn=0xaa"))   o.hopt.cor3.syncpat = 1;
	else if (! strcasecmp (opt, "syn=0x00"))   o.hopt.cor3.syncpat = 0;
	else if (! strcasecmp (opt, "-syn"))       o.hopt.cor3.sndpad = 0;
	else if (! strcasecmp (opt, "+syn"))       o.hopt.cor3.sndpad = 1;

	else if (! strncasecmp (opt, "rfar1=", 6)) o.hopt.rfar1 = atoi (opt + 6);
	else if (! strncasecmp (opt, "rfar2=", 6)) o.hopt.rfar2 = atoi (opt + 6);
	else if (! strncasecmp (opt, "rfar3=", 6)) o.hopt.rfar3 = atoi (opt + 6);
	else if (! strncasecmp (opt, "rfar4=", 6)) o.hopt.rfar4 = atoi (opt + 6);
	else if (! strcasecmp (opt, "crc-16"))     o.hopt.cpsr = 1;
	else if (! strcasecmp (opt, "crc-v.41"))   o.hopt.cpsr = 0;
	else usage ();
}

void set_bisync_opt (char *opt)
{
	usage ();
}

void set_x21_opt (char *opt)
{
	usage ();
}

int main (int argc, char **argv)
{
	int channel;

	for (--argc, ++argv; argc>0 && **argv=='-'; --argc, ++argv)
		if (! strcasecmp (*argv, "-a"))
			++aflag;
		else if (! strcasecmp (*argv, "-s"))
			++sflag;
		else
			usage ();

	if (argc <= 0) {
		if (sflag)
			printallstats ();
		else
			printall ();
		return (0);
	}

	if (argv[0][0]=='c' && argv[0][1]=='x')
		*argv += 2;
	if (**argv<'0' || **argv>'9')
		usage ();
	channel = atoi (*argv);
	--argc, ++argv;

	if (sflag) {
		if (printstats (channel, 1) < 0)
			printf ("channel cx%d not available\n", channel);
		return (0);
	}

	getchan (channel);

	if (argc <= 0) {
		printchan (channel);
		return (0);
	}

	for (; argc>0; --argc, ++argv)
		if (**argv == '(')
			continue;
		else if (! strncasecmp (*argv, "ispeed=", 7))
			o.rxbaud = atoi (*argv+7);
		else if (! strncasecmp (*argv, "ospeed=", 7))
			o.txbaud = atoi (*argv+7);
		else if (! strcasecmp (*argv, "async"))
			o.mode = M_ASYNC;
		else if (! strcasecmp (*argv, "hdlc"))
			o.mode = M_HDLC;
		else if (! strcasecmp (*argv, "bisync") ||
		    ! strcasecmp (*argv, "bsc"))
			o.mode = M_BISYNC;
		else if (! strcasecmp (*argv, "x.21") ||
		    ! strcasecmp (*argv, "x21"))
			o.mode = M_X21;
		else if (**argv>='0' && **argv<='9')
			o.txbaud = o.rxbaud = atoi (*argv);
		else if (! strcasecmp (*argv, "cisco")) {
			o.sopt.cisco = 1;
			o.sopt.ext = 0;
		} else if (! strcasecmp (*argv, "ppp")) {
			o.sopt.cisco = 0;
			o.sopt.ext = 0;
		} else if (! strcasecmp (*argv, "ext"))
			o.sopt.ext = 1;
		else if (! strcasecmp (*argv, "+keepalive"))
			o.sopt.keepalive = 1;
		else if (! strcasecmp (*argv, "-keepalive"))
			o.sopt.keepalive = 0;
		else if (! strcasecmp (*argv, "+autorts"))
			o.sopt.norts = 0;
		else if (! strcasecmp (*argv, "-autorts"))
			o.sopt.norts = 1;
		else if (! strcasecmp (*argv, "port=rs232") ||
		    ! strcasecmp (*argv, "port=rs449") ||
		    ! strcasecmp (*argv, "port=v35"))
			set_interface_type (*argv);
		else if (! strncasecmp (*argv, "master=",7))
			set_master (*argv+7);

		/*
		 * Common channel options
		 */
		/* channel option register 4 */
		else if (! strcasecmp (*argv, "-ctsdown"))
			o.opt.cor4.cts_zd = 0;
		else if (! strcasecmp (*argv, "+ctsdown"))
			o.opt.cor4.cts_zd = 1;
		else if (! strcasecmp (*argv, "-cddown"))
			o.opt.cor4.cd_zd = 0;
		else if (! strcasecmp (*argv, "+cddown"))
			o.opt.cor4.cd_zd = 1;
		else if (! strcasecmp (*argv, "-dsrdown"))
			o.opt.cor4.dsr_zd = 0;
		else if (! strcasecmp (*argv, "+dsrdown"))
			o.opt.cor4.dsr_zd = 1;
		else if (! strncasecmp (*argv, "fifo=", 5))
			o.opt.cor4.thr = atoi (*argv + 5);

		/* channel option register 5 */
		else if (! strcasecmp (*argv, "-ctsup"))
			o.opt.cor5.cts_od = 0;
		else if (! strcasecmp (*argv, "+ctsup"))
			o.opt.cor5.cts_od = 1;
		else if (! strcasecmp (*argv, "-cdup"))
			o.opt.cor5.cd_od = 0;
		else if (! strcasecmp (*argv, "+cdup"))
			o.opt.cor5.cd_od = 1;
		else if (! strcasecmp (*argv, "-dsrup"))
			o.opt.cor5.dsr_od = 0;
		else if (! strcasecmp (*argv, "+dsrup"))
			o.opt.cor5.dsr_od = 1;
		else if (! strncasecmp (*argv, "rfifo=", 6))
			o.opt.cor5.rx_thr = atoi (*argv + 6);

		/* receive clock option register */
		else if (! strcasecmp (*argv, "nrz"))
			o.opt.rcor.encod = ENCOD_NRZ;
		else if (! strcasecmp (*argv, "nrzi"))
			o.opt.rcor.encod = ENCOD_NRZI;
		else if (! strcasecmp (*argv, "manchester"))
			o.opt.rcor.encod = ENCOD_MANCHESTER;
		else if (! strcasecmp (*argv, "-dpll"))
			o.opt.rcor.dpll = 0;
		else if (! strcasecmp (*argv, "+dpll"))
			o.opt.rcor.dpll = 1;

		/* transmit clock option register */
		else if (! strcasecmp (*argv, "-lloop"))
			o.opt.tcor.llm = 0;
		else if (! strcasecmp (*argv, "+lloop"))
			o.opt.tcor.llm = 1;
		else if (! strcasecmp (*argv, "-extclock"))
			o.opt.tcor.ext1x = 0;
		else if (! strcasecmp (*argv, "+extclock"))
			o.opt.tcor.ext1x = 1;

		/*
		 * Mode dependent channel options
		 */
		else switch (o.mode) {
		case M_ASYNC:  set_async_opt (*argv); break;
		case M_HDLC:   set_hdlc_opt (*argv); break;
		case M_BISYNC: set_bisync_opt (*argv); break;
		case M_X21:    set_x21_opt (*argv); break;
		}

	setchan (channel);
	return (0);
}
