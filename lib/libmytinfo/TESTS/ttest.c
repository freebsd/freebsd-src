/*
 * ttest.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:47
 *
 */
 
#define NOTLIB
#include "defs.h"
#include <term.h>

const char SCCSid[] = "@(#) mytinfo ttest.c 3.2 92/02/01 public domain, By Ross Ridge";

int
cup(x,y)
int x, y; {
	if (columns < 2 || lines < 2)
		return -1;
	if (cursor_address != NULL) 
		putp(tparm(cursor_address, y, x));
	else if (cursor_home != NULL && cursor_down != NULL
		 && cursor_right != NULL) {
		putp(cursor_home);
		if (parm_down_cursor != NULL)
			putp(tparm(parm_down_cursor, y));
		else
			for(; y > 0; y--)
				putp(cursor_down);
		if (parm_right_cursor != NULL)
			putp(tparm(parm_right_cursor, y));
		else
			for(; y > 0; y--)
				putp(cursor_right);
	} else if (cursor_to_ll != NULL && cursor_up != NULL
		   && cursor_right != NULL) {
		putp(cursor_to_ll);
		if (parm_up_cursor != NULL)
			putp(tparm(parm_up_cursor, y));
		else
			for(y++; y < columns ; y++)
				putp(cursor_up);
		if (parm_right_cursor != NULL)
			putp(tparm(parm_right_cursor, y));
		else
			for(; y > 0; y--)
				putp(cursor_right);
	} else
		return 1;
	return 0;
}

int
clear() {
	int r;
	if (clear_screen != NULL)
		putp(clear_screen);
	else if (clr_eos != NULL) {
		r = cup(0,0);
		if (r != 0)
			return r;
		putp(clr_eos);
	} else
		return -2;
	return 0;
}

void
nl() {
	if (newline != NULL) 
		putp(newline);
	else if (carriage_return != NULL && cursor_down != NULL) {
		putp(cursor_down);
		putp(carriage_return);
	} else 
		quit(-1, "can't do a newline");
	return;
}

void
putln(s)
char *s; {
	int i;

	if (columns < 2 || auto_right_margin)
		fputs(s, stdout);
	else {
		i = 0;
		while(*s) {
			putchar(*s);
			s++;
			if (++i == columns) {
				nl();
				i = 0;
			}
		}
	}
	nl();
}

void
anykey() {
	fputs("-- press any key --", stdout);
	fflush(stdout);
	getchar();
	nl();
}

void
do_cleanup(e)
int e; {
	fflush(stdout);
	fflush(stderr);
	reset_shell_mode();
	fprintf(stderr, "\n");
}

#ifdef USE_SGTTY
struct sgttyb new_tty;
#else
#ifdef USE_TERMIO
struct termio new_tty;
#else
struct termios new_tty;
#endif
#endif

int
main(argc, argv)
int argc;
char **argv; {
	register int i;

	prg_name = argv[0];
	cleanup = do_cleanup;

	if (argc == 1)
		setupterm(NULL, 1, (int *) 0);
	else if (argc == 2)
		setupterm(argv[1], 1, (int *) 0);
	else {
		fprintf(stderr, "usage: %s [terminal]\n", argv[0]);
		return 1;
	}
	fflush(stderr);
	fflush(stdout);
#ifdef USE_SGTTY
	ioctl(1, TIOCGETP, &new_tty);
	new_tty.sg_flags &= ~(CRMOD | ECHO | XTABS);
#ifdef CBREAK
	new_tty.sg_flags |= CBREAK;
#else
	new_tty.sg_flags |= RAW;
#endif
	ioctl(1, TIOCSETP, &new_tty);
#endif
#ifdef USE_TERMIO
	ioctl(1, TCGETA, &new_tty);
#else
#ifdef USE_TERMIOS
	tcgetattr(1, &new_tty);
#endif
#endif
#if defined(USE_TERMIO) || defined(USE_TERMIOS)
	new_tty.c_lflag &= ~(ICANON | ECHO);
	new_tty.c_oflag &= ~(OPOST);
	new_tty.c_cc[VMIN] = 1;
	new_tty.c_cc[VTIME] = 1;
#endif
#ifdef USE_TERMIO
	ioctl(1, TCSETA, &new_tty);
#else
#ifdef USE_TERMIOS
	tcsetattr(1, TCSADRAIN, &new_tty);
#endif
#endif
	def_prog_mode();
	
	clear();
	printf("columns = %d", columns);
	nl();
	printf("lines = %d", lines);
	if (columns < 2)
		quit(-1, "columns must be > 1");
	nl();
	anykey();
	nl();
	if (auto_right_margin) {
		putln("auto_right_margin = TRUE");
		nl();
		for(i = 0; i < columns; i++)
			putchar('1');
		for(i = 0; i < columns / 2; i++)
			putchar('2');
		nl();
	} else {
		putln("auto_right_margin = FALSE");
		nl();
		for(i = 0; i < columns + columns / 2; i++)
			putchar('1');
		nl();
		for(i = 0; i < columns / 2; i++)
			putchar('2');
		nl();
	}
	nl();
	putln("***a line of 1's followed by a line of 2's");
	nl();
	anykey();
	nl();

	if (over_strike) {
		putln("over_strike = TRUE");
		if (cursor_left != NULL) {
			for(i = 0; i < columns / 4 + 1; i++) {
				putchar('/');
				putp(cursor_left);
				putchar('\\');
			}
		} else if (carriage_return != NULL) {
			for(i = 0; i < columns / 4 + 1; i++) 
				putchar('/');
			putp(carriage_return);
			for(i = 0; i < columns / 4 + 1; i++) 
				putchar('\\');
		}
		nl();
		nl();
		putln("*** X's made from \\'s overstriking /'s");
		nl();
		anykey();
		nl();
	}

	if (cup(0,0) == 0) {
		clear();
		putln("cup test");
		for(i = 1; i < columns; i++)
			putp(tparm(cursor_address, 0, i));
		for(i = 0; i < lines; i++)
			putp(tparm(cursor_address, i, columns - 1));
		for(i = columns; i--;)
			putp(tparm(cursor_address, lines - 1, i));
		for(i = lines; i--;)
			putp(tparm(cursor_address, i, 0));
		nl();
		anykey();
	}
	clear();

	putln("Attribute test");
	nl();
	if (enter_blink_mode != NULL) {
		putp(enter_blink_mode);
		printf("blink");
		putp(exit_attribute_mode);
		nl();
	}
	if (enter_bold_mode != NULL) {
		putp(enter_bold_mode);
		printf("bold");
		putp(exit_attribute_mode);
		nl();
	}
	if (enter_dim_mode != NULL) {
		putp(enter_dim_mode);
		printf("dim");
		putp(exit_attribute_mode);
		nl();
	}
	if (enter_reverse_mode != NULL) {
		putp(enter_reverse_mode);
		printf("reverse");
		putp(exit_attribute_mode);
		nl();
	}
	if (enter_standout_mode != NULL) {
		putp(enter_standout_mode);
		printf("standout");
		putp(exit_standout_mode);
		nl();
	}
	if (enter_underline_mode != NULL) {
		putp(enter_underline_mode);
		printf("underline");
		putp(exit_underline_mode);
		nl();
	}
	nl();
	anykey();

	clear();
	reset_shell_mode();
	
	return (0);
}
