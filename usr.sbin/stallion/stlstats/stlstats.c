/*****************************************************************************/

/*
 * stlstats.c  -- stallion intelligent multiport stats display.
 *
 * Copyright (c) 1994-1996 Greg Ungerer (gerg@stallion.oz.au).
 * All rights reserved.
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
 *	This product includes software developed by Greg Ungerer.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*****************************************************************************/

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <machine/cdk.h>
#include <machine/comstats.h>

/*****************************************************************************/

char	*version = "1.0.0";
char	*defdevice = "/dev/staliomem0";

char	*ctrldevice;
int	ctrlfd;
int	displaybrdnr = 0;
int	displaypanelnr = 0;
int	displayportnr = 0;
int	displayportbank = 0;

#define	MAXBRDS		8
#define	MAXPORTS	32

combrd_t	brdstats;
comstats_t	stats[MAXPORTS];

char	*line = "                                                                                ";

/*****************************************************************************/

/*
 *	Declare internal function prototypes here.
 */
static void	usage(void);
void	useportdevice(char *devname);
void	localexit(int nr);
void	menuport();
void	displayport();
void	menuallports();
void	displayallports();
void	getallstats();
void	getbrdstats();
void	clearportstats();
void	clearallstats();

/*****************************************************************************/

static void usage()
{
	fprintf(stderr, "%s\n%s\n",
	"usage: stlstats [-hVi] [-c control-device] [-b board-number]",
	"                [-p port-number] [-d port-device]");
	exit(0);
}

/*****************************************************************************/

void useportdevice(char *devname)
{
	struct stat	statinfo;
	int		portnr, portcnt;
	int		i;

	if (stat(devname, &statinfo) < 0)
		errx(1, "port device %s does not exist", devname);
	if ((statinfo.st_mode & S_IFMT) != S_IFCHR)
		errx(1, "port device %s is not a char device", devname);

	displaybrdnr = (statinfo.st_rdev & 0x00700000) >> 20;
	portnr = (statinfo.st_rdev & 0x1f) |
		((statinfo.st_rdev & 0x00010000) >> 11);
	getbrdstats();
	if (brdstats.ioaddr == 0)
		errx(1, "device %s does not exist", devname);

	for (portcnt = 0, i = 0; (i < brdstats.nrpanels); i++) {
		if ((portnr >= portcnt) &&
		    (portnr < (portcnt + brdstats.panels[i].nrports)))
			break;
		portcnt += brdstats.panels[i].nrports;
	}
	if (i >= brdstats.nrpanels)
		errx(1, "device %s does not exist", devname);
	displaypanelnr = i;
	displayportnr = portnr - portcnt;
	if (displayportnr >= 16)
		displayportbank = 16;
}

/*****************************************************************************/

/*
 *	Get the board stats for the current display board.
 */

void getbrdstats()
{
	brdstats.brd = displaybrdnr;
	if (ioctl(ctrlfd, COM_GETBRDSTATS, &brdstats) < 0)
		memset((combrd_t *) &brdstats, 0, sizeof(combrd_t));
}

/*****************************************************************************/

/*
 *	Zero out stats for the current display port.
 */

void clearportstats()
{
	stats[displayportnr].brd = displaybrdnr;
	stats[displayportnr].panel = displaypanelnr;
	stats[displayportnr].port = displayportnr;
	ioctl(ctrlfd, COM_CLRPORTSTATS, &stats[displayportnr]);
}

/*****************************************************************************/

/*
 *	Zero out all stats for all ports on all boards.
 */

void clearallstats()
{
	int	brdnr, panelnr, portnr;

	for (brdnr = 0; (brdnr < MAXBRDS); brdnr++) {
		for (panelnr = 0; (panelnr < COM_MAXPANELS); panelnr++) {
			for (portnr = 0; (portnr < MAXPORTS); portnr++) {
				stats[0].brd = brdnr;
				stats[0].panel = panelnr;
				stats[0].port = portnr;
				ioctl(ctrlfd, COM_CLRPORTSTATS, &stats[0]);
			}
		}
	}
}

/*****************************************************************************/

/*
 *	Get the stats for the current display board/panel.
 */

void getallstats()
{
	int	i;

	for (i = 0; (i < brdstats.panels[displaypanelnr].nrports); i++) {
		stats[i].brd = displaybrdnr;
		stats[i].panel = displaypanelnr;
		stats[i].port = i;
		if (ioctl(ctrlfd, COM_GETPORTSTATS, &stats[i]) < 0) {
			warn("ioctl(COM_GETPORTSTATS) failed");
			localexit(1);
		}
	}
}

/*****************************************************************************/

/*
 *	Display the per ports stats screen.
 */

void displayport()
{
	mvprintw(0, 0, "STALLION SERIAL PORT STATISTICS");
	mvprintw(2, 0,
		"Board=%d  Type=%d  HwID=%02x  State=%06x  TotalPorts=%d",
		displaybrdnr, brdstats.type, brdstats.hwid, brdstats.state,
		brdstats.nrports);
	mvprintw(3, 0, "Panel=%d  HwID=%02x  Ports=%d", displaypanelnr,
		brdstats.panels[displaypanelnr].hwid,
		brdstats.panels[displaypanelnr].nrports);

	attron(A_REVERSE);
	mvprintw(5, 0, line);
	mvprintw(5, 0, "Port=%d ", displayportnr);
	attroff(A_REVERSE);

	mvprintw(7,  0, "STATE:      State=%08x", stats[displayportnr].state);
  	mvprintw(7, 29, "Tty=%08x", stats[displayportnr].ttystate);
	mvprintw(7, 47, "Flags=%08x", stats[displayportnr].flags);
	mvprintw(7, 65, "HwID=%02x", stats[displayportnr].hwid);

	mvprintw(8,  0, "CONFIG:     Cflag=%08x", stats[displayportnr].cflags);
	mvprintw(8, 29, "Iflag=%08x", stats[displayportnr].iflags);
	mvprintw(8, 47, "Oflag=%08x", stats[displayportnr].oflags);
	mvprintw(8, 65, "Lflag=%08x", stats[displayportnr].lflags);

	mvprintw(10,  0, "TX DATA:    Total=%d", stats[displayportnr].txtotal);
	mvprintw(10, 29, "Buffered=%d      ", stats[displayportnr].txbuffered);
	mvprintw(11,  0, "RX DATA:    Total=%d", stats[displayportnr].rxtotal);
	mvprintw(11, 29, "Buffered=%d      ", stats[displayportnr].rxbuffered);
	mvprintw(12,  0, "RX ERRORS:  Parity=%d", stats[displayportnr].rxparity);
	mvprintw(12, 29, "Framing=%d", stats[displayportnr].rxframing);
	mvprintw(12, 47, "Overrun=%d", stats[displayportnr].rxoverrun);
	mvprintw(12, 65, "Lost=%d", stats[displayportnr].rxlost);

	mvprintw(14,  0, "FLOW TX:    Xoff=%d", stats[displayportnr].txxoff);
	mvprintw(14, 29, "Xon=%d", stats[displayportnr].txxon);
#if 0
	mvprintw(14, 47, "CTSoff=%d", stats[displayportnr].txctsoff);
	mvprintw(14, 65, "CTSon=%d", stats[displayportnr].txctson);
#endif
	mvprintw(15,  0, "FLOW RX:    Xoff=%d", stats[displayportnr].rxxoff);
	mvprintw(15, 29, "Xon=%d", stats[displayportnr].rxxon);
	mvprintw(15, 47, "RTSoff=%d", stats[displayportnr].rxrtsoff);
	mvprintw(15, 65, "RTSon=%d", stats[displayportnr].rxrtson);

	mvprintw(17,  0, "OTHER:      TXbreaks=%d",
		stats[displayportnr].txbreaks);
	mvprintw(17, 29, "RXbreaks=%d", stats[displayportnr].rxbreaks);
	mvprintw(17, 47, "Modem=%d", stats[displayportnr].modem);

	mvprintw(19, 0, "SIGNALS:    DCD=%d    DTR=%d    CTS=%d    RTS=%d    "
		"DSR=%d    RI=%d",
		(stats[displayportnr].signals & TIOCM_CD) ? 1 : 0,
		(stats[displayportnr].signals & TIOCM_DTR) ? 1 : 0,
		(stats[displayportnr].signals & TIOCM_CTS) ? 1 : 0,
		(stats[displayportnr].signals & TIOCM_RTS) ? 1 : 0,
		(stats[displayportnr].signals & TIOCM_DSR) ? 1 : 0,
		(stats[displayportnr].signals & TIOCM_RI) ? 1 : 0);

	attron(A_REVERSE);
	mvprintw(22, 0, line);
	attroff(A_REVERSE);

	mvprintw(24, 19, "(q=Quit,0123456789abcdef=Port,Z=ZeroStats)");
	refresh();
}

/*****************************************************************************/

/*
 *	Continuously update and display the per ports stats screen.
 *	Also checks for keyboard input, and processes it as appropriate.
 */

void menuport()
{
	int	ch, done;

	clear();
	done = 0;

	while ((ch = getch()) != 27) {
		switch (ch) {
		case ERR:
			break;
		case '':
			refresh();
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			ch = (ch - 'a' + '0' + 10);
			/* FALLTHROUGH */
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			ch -= '0';
			if (ch >= brdstats.panels[displaypanelnr].nrports) {
				beep();
			} else {
				displayportnr = displayportbank + ch;
				clear();
			}
			break;
		case 'Z':
			clearportstats();
			clear();
			break;
		case 'q':
			done = 1;
			break;
		default:
			beep();
			break;
		}

		if (done)
			break;

		getallstats();
		displayport();
	}
}

/*****************************************************************************/

/*
 *	Display the all ports stats screen.
 */

void displayallports()
{
	int	i, nrports, portnr;;

	nrports = brdstats.panels[displaypanelnr].nrports;

	mvprintw(0, 0, "STALLION SERIAL PORT STATISTICS");
	mvprintw(2, 0, "Board=%d  Type=%d  HwID=%02x  State=%06x  TotalPorts=%d",
		displaybrdnr, brdstats.type, brdstats.hwid, brdstats.state,
		brdstats.nrports);
	mvprintw(3, 0, "Panel=%d  HwID=%02x  Ports=%d", displaypanelnr,
		brdstats.panels[displaypanelnr].hwid, nrports);

	attron(A_REVERSE);
	mvprintw(5, 0, "Port  State   Tty    Flags  Cflag Iflag Oflag Lflag "
		"Sigs    TX Total   RX Total ");
	attroff(A_REVERSE);

	if (nrports > 0) {
		if (nrports > 16)
			nrports = 16;
		portnr = displayportbank;
		for (i = 0; (i < nrports); i++, portnr++) {
			mvprintw((6 + i), 1, "%2d", portnr);
			mvprintw((6 + i), 5, "%06x", stats[portnr].state);
			mvprintw((6 + i), 12, "%06x", stats[portnr].ttystate);
			mvprintw((6 + i), 19, "%08x", stats[portnr].flags);
			mvprintw((6 + i), 28, "%05x", stats[portnr].cflags);
			mvprintw((6 + i), 34, "%05x", stats[portnr].iflags);
			mvprintw((6 + i), 40, "%05x", stats[portnr].oflags);
			mvprintw((6 + i), 46, "%05x", stats[portnr].lflags);
			mvprintw((6 + i), 52, "%04x", stats[portnr].signals);
			mvprintw((6 + i), 58, "%10d", stats[portnr].txtotal);
			mvprintw((6 + i), 69, "%10d", stats[portnr].rxtotal);
		}
	} else {
		mvprintw(12, 32, "NO BOARD %d FOUND", displaybrdnr);
		i = 16;
	}

	attron(A_REVERSE);
	mvprintw((6 + i), 0, line);
	attroff(A_REVERSE);

	mvprintw(24, 14,
		"(q=Quit,01234567=Board,n=Panels,p=Ports,Z=ZeroStats)");
	refresh();
}

/*****************************************************************************/

/*
 *	Continuously update and display the all ports stats screen.
 *	Also checks for keyboard input, and processes it as appropriate.
 */

void menuallports()
{
	int	ch, done;

	clear();
	getbrdstats();

	done = 0;
	while ((ch = getch()) != 27) {
		switch (ch) {
		case ERR:
			break;
		case '':
			refresh();
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			displaybrdnr = ch - '0';
			displaypanelnr = 0;
			getbrdstats();
			if (brdstats.state == 0)
				beep();
			clear();
			break;
		case 'n':
			if (brdstats.panels[displaypanelnr].nrports > 16) {
				if (displayportbank == 0) {
					displayportbank = 16;
					clear();
					break;
				}
			}
			displayportbank = 0;
			displaypanelnr++;
			if (displaypanelnr >= brdstats.nrpanels)
				displaypanelnr = 0;
			clear();
			break;
		case 'p':
			if (brdstats.panels[displaypanelnr].nrports > 0) {
				displayportnr = displayportbank;
				menuport();
				clear();
			} else {
				beep();
			}
			break;
		case 'Z':
			clearallstats();
			clear();
			break;
		case 'q':
			done = 1;
			break;
		default:
			beep();
			break;
		}

		if (done)
			break;

		getallstats();
		displayallports();
	}
}

/*****************************************************************************/

/* 
 *	A local exit routine - shuts down curses before exiting.
 */

void localexit(int nr)
{
	refresh();
	endwin();
	exit(nr);
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	struct stat	statinfo;
	int		c, useport;
	char		*portdev;

	ctrldevice = defdevice;
	useport = 0;

	while ((c = getopt(argc, argv, "hvVb:p:d:c:")) != -1) {
		switch (c) {
		case 'V':
			printf("stlstats version %s\n", version);
			exit(0);
			break;
		case 'h':
			usage();
			break;
		case 'b':
			displaybrdnr = atoi(optarg);
			break;
		case 'p':
			displaypanelnr = atoi(optarg);
			break;
		case 'd':
			useport++;
			portdev = optarg;
			break;
		case 'c':
			ctrldevice = optarg;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

/*
 *	Check that the control device exits and is a character device.
 */
	if (stat(ctrldevice, &statinfo) < 0)
		errx(1, "control device %s does not exist", ctrldevice);
	if ((statinfo.st_mode & S_IFMT) != S_IFCHR)
		errx(1, "control device %s is not a char device", ctrldevice);
	if ((ctrlfd = open(ctrldevice, O_RDWR)) < 0)
		errx(1, "open of %s failed", ctrldevice);

/*
 *	Validate the panel number supplied by user. We do this now since we
 *	need to have parsed the entire command line first.
 */
	getbrdstats();
	if (displaypanelnr >= brdstats.nrpanels)
		displaypanelnr = 0;

	if (useport)
		useportdevice(portdev);

/*
 *	Everything is now ready, lets go!
 */
	initscr();
	cbreak();
	halfdelay(5);
	noecho();
	clear();
	if (useport) {
		menuport();
		clear();
	}
	menuallports();
	refresh();
	endwin();

	close(ctrlfd);
	printf("\n");
	exit(0);
}

/*****************************************************************************/
