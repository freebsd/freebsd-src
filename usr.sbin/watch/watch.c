/*
 * Copyright (c) 1995 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Snoop stuff.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <sys/snoop.h>


#define MSG_INIT	"Snoop started."
#define MSG_OFLOW	"Snoop stopped due to overflow.Reconnecting."
#define MSG_CLOSED	"Snoop stopped due to tty close.Reconnecting."
#define MSG_CHANGE	"Snoop device change by user request."


#define DEV_NAME_LEN	12	/* for /dev/ttyXX++ */
#define MIN_SIZE	256

#define CHR_SWITCH	24	/* Ctrl+X	 */
#define CHR_CLEAR	23	/* Ctrl+V	 */


int             opt_reconn_close = 0;
int             opt_reconn_oflow = 0;
int             opt_interactive = 1;
int             opt_timestamp = 0;

char            dev_name[DEV_NAME_LEN];
int             snp_io;
struct snptty   snp_tty;
int             std_in = 0, std_out = 1;


int             clear_ok = 0;
struct sgttyb   sgo;
char            tbuf[1024], buf[1024];


void
clear()
{
	if (clear_ok)
		tputs(buf, 1, putchar);
	fflush(stdout);
}

void
timestamp(buf)
	char           *buf;
{
	time_t          t;
	char            btmp[1024];
	clear();
	printf("\n---------------------------------------------\n");
	t = time(NULL);
	strftime(btmp, 1024, "Time: %d %b %H:%M", localtime(&t));
	printf("%s\n", btmp);
	printf("%s\n", buf);
	printf("---------------------------------------------\n");
	fflush(stdout);
}

void
set_tty()
{
	struct sgttyb   sgn;
	ioctl(std_in, TIOCGETP, &sgo);
	/* bcopy(&sgn, &sgo, sizeof(struct sgttyb)); */
	sgn = sgo;
	sgn.sg_flags |= CBREAK;
	sgn.sg_flags &= ~ECHO;
	ioctl(std_in, TIOCSETP, &sgn);
}

void
unset_tty()
{
	ioctl(std_in, TIOCSETP, &sgo);
}


void
fatal(buf)
	char           *buf;
{
	unset_tty();
	if (buf)
		fprintf(stderr, "Fatal: %s\n", buf);
	exit(1);
}

int
open_snp()
{
	char            snp[DEV_NAME_LEN] = "/dev/snpX";
	char            c;
	int             f;
	for (c = '0'; c <= '9'; c++) {
		snp[8] = c;
		if ((f = open(snp, O_RDONLY)) < 0)
			continue;
		return f;
	}
	fatal("Cannot open snoop device.");
}


void
cleanup()
{
	if (opt_timestamp)
		timestamp("Logging Exited.");
	close(snp_io);
	unset_tty();
	exit(0);
}


void
show_usage()
{
	printf("watch -[ciot] [tty name]\n");
	exit(1);
}

void
setup_scr()
{
	char           *cbuf = buf, *term;
	if (!opt_interactive)
		return;
	if ((term = getenv("TERM")))
		if (tgetent(tbuf, term) == 1)
			if (tgetstr("cl", &cbuf))
				clear_ok = 1;
	clear();
	set_tty();
}


int
ctoh(c)
	char            c;
{
	if (c >= '0' && c <= '9')
		return (int) (c - '0');

	if (c >= 'a' && c <= 'f')
		return (int) (c - 'a' + 10);

	fatal("Bad tty number.");
}


void
detach_snp()
{
	struct snptty   st;
	st.st_type = -1;
	st.st_unit = -1;
	ioctl(snp_io, SNPSTTY, &st);
}

void
attach_snp()
{
	if (ioctl(snp_io, SNPSTTY, &snp_tty) != 0)
		fatal("Cannot attach to tty.");
	if (opt_timestamp)
		timestamp("Logging Started.");
}


void
set_dev(name)
	char           *name;
{
	char            buf[DEV_NAME_LEN], num[DEV_NAME_LEN];
	int             unitbase = 0;

	if (strlen(name) > 5 && !strncmp(name, "/dev/", 5))
		strcpy(buf, &(name[5]));
	else
		strcpy(buf, name);

	if (strlen(buf) < 4)
		fatal("Bad tty name.");

	if (!strncmp(buf, "tty", 3))
		switch (buf[3]) {
		case 'v':
			snp_tty.st_unit = ctoh(buf[4]);
			snp_tty.st_type = ST_VTY;
			goto got_num;
		case 'r':
			unitbase += 16;
		case 'q':
			unitbase += 16;
		case 'p':
			snp_tty.st_unit = ctoh(buf[4]) + unitbase;
			snp_tty.st_type = ST_PTY;
			goto got_num;
		case '0':
		case 'd':
			snp_tty.st_unit = ctoh(buf[4]);
			snp_tty.st_type = ST_SIO;
			goto got_num;
		default:
			fatal("Bad tty name.");

		}


	if (!strncmp(buf, "vty", 3)) {
		strcpy(num, &(buf[3]));
		snp_tty.st_unit = atoi(num);
		snp_tty.st_type = ST_VTY;
		goto got_num;
	}
	if (!strncmp(buf, "pty", 3)) {
		strcpy(num, &(buf[3]));
		snp_tty.st_unit = atoi(num);
		snp_tty.st_type = ST_PTY;
		goto got_num;
	}
	if (!strncmp(buf, "sio", 3) || !strncmp(buf, "cua", 3)) {
		strcpy(num, &(buf[3]));
		snp_tty.st_unit = atoi(num);
		snp_tty.st_type = ST_SIO;
		goto got_num;
	}
	fatal("Bad tty name.");
got_num:
	attach_snp();
}

void
ask_dev(dev_name, msg)
	char           *dev_name, *msg;
{
	char            buf[DEV_NAME_LEN];
	int             len;

	clear();
	unset_tty();

	if (msg)
		printf("%s\n", msg);
	if (dev_name)
		printf("Enter device name [%s]:", dev_name);
	else
		printf("Enter device name:");

	if (fgets(buf, DEV_NAME_LEN - 1, stdin)) {
		len = strlen(buf);
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		if (buf[0] != '\0' && buf[0] != ' ')
			strcpy(dev_name, buf);
	}
	set_tty();
}


void
main(ac, av)
	int             ac;
	char          **av;
{
	int             res, nread, b_size = MIN_SIZE;
	extern int      optind;
	char            ch, *buf;
	fd_set          fd_s;

	if (getuid() != 0)
		fatal(NULL);

	if (isatty(std_out))
		opt_interactive = 1;
	else
		opt_interactive = 0;


	while ((ch = getopt(ac, av, "ciot")) != EOF)
		switch (ch) {
		case 'c':
			opt_reconn_close = 1;
			break;
		case 'i':
			opt_interactive = 1;
			break;
		case 'o':
			opt_reconn_oflow = 1;
			break;
		case 't':
			opt_timestamp = 1;
			break;
		case '?':
		default:
			show_usage();
			exit(1);
		}

	signal(SIGINT, cleanup);

	setup_scr();
	snp_io = open_snp();

	if (*(av += optind) == NULL) {
		if (opt_interactive)
			ask_dev(dev_name, MSG_INIT);
		else
			fatal("No device name given.");
	} else
		strncpy(dev_name, *av, DEV_NAME_LEN);

	set_dev(dev_name);

	if (!(buf = (char *) malloc(b_size)))
		fatal("Cannot malloc().");

	FD_ZERO(&fd_s);

	while (1) {
		if (opt_interactive)
			FD_SET(std_in, &fd_s);
		FD_SET(snp_io, &fd_s);
		res = select(snp_io + 1, &fd_s, NULL, NULL, NULL);
		if (opt_interactive && FD_ISSET(std_in, &fd_s)) {
			switch (ch = getchar()) {
			case CHR_CLEAR:
				clear();
				break;
			case CHR_SWITCH:
				/* detach_snp(); */
				ask_dev(dev_name, MSG_CHANGE);
				set_dev(dev_name);
				break;
			default:
			}
		}
		if (!FD_ISSET(snp_io, &fd_s))
			continue;

		if ((res = ioctl(snp_io, FIONREAD, &nread)) != 0)
			fatal("ioctl() failed.");

		switch (nread) {
		case SNP_OFLOW:
			if (opt_reconn_oflow)
				attach_snp();
			else if (opt_interactive) {
				ask_dev(dev_name, MSG_OFLOW);
				set_dev(dev_name);
			} else
				cleanup();
		case SNP_DETACH:
		case SNP_TTYCLOSE:
			if (opt_reconn_close)
				attach_snp();
			else if (opt_interactive) {
				ask_dev(dev_name, MSG_CLOSED);
				set_dev(dev_name);
			} else
				cleanup();
		default:
			if (nread < (b_size / 2) && (b_size / 2) > MIN_SIZE) {
				free(buf);
				if (!(buf = (char *) malloc(b_size / 2)))
					fatal("Cannot malloc()");
				b_size = b_size / 2;
			}
			if (nread > b_size) {
				b_size = (nread % 2) ? (nread + 1) : (nread);
				free(buf);
				if (!(buf = (char *) malloc(b_size)))
					fatal("Cannot malloc()");
			}
			if (read(snp_io, buf, nread) < nread)
				fatal("read failed.");
			if (write(std_out, buf, nread) < nread)
				fatal("write failed.");
		}
	}			/* While */
}
