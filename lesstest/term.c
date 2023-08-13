#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <termcap.h>
#include <sys/ioctl.h>
#include "lesstest.h"

TermInfo terminfo;

static void set_termio_flags(struct termios* s) {
	s->c_lflag &= ~(0
#ifdef ICANON
		| ICANON
#endif
#ifdef ECHO
		| ECHO
#endif
#ifdef ECHOE
		| ECHOE
#endif
#ifdef ECHOK
		| ECHOK
#endif
#if ECHONL
		| ECHONL
#endif
	);

	s->c_oflag |= (0
#ifdef OXTABS
		| OXTABS
#else
#ifdef TAB3
		| TAB3
#else
#ifdef XTABS
		| XTABS
#endif
#endif
#endif
#ifdef OPOST
		| OPOST
#endif
#ifdef ONLCR
		| ONLCR
#endif
	);

	s->c_oflag &= ~(0
#ifdef ONOEOT
		| ONOEOT
#endif
#ifdef OCRNL
		| OCRNL
#endif
#ifdef ONOCR
		| ONOCR
#endif
#ifdef ONLRET
		| ONLRET
#endif
	);
}

// Enable or disable raw mode on the given tty.
void raw_mode(int tty, int on) {
	struct termios s;
	static struct termios save_term;
	if (!on) {
		s = save_term;
	} else {
		tcgetattr(tty, &s);
		save_term = s;
		set_termio_flags(&s);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
	}
	tcsetattr(tty, TCSADRAIN, &s);
}

// Initialize the enter & exit capabilities for a given terminal mode.
static void setup_mode(char* enter_cap, char* exit_cap, char** enter_str, char** exit_str, char** spp) {
	*enter_str = tgetstr(enter_cap, spp);
	if (*enter_str == NULL) *enter_str = "";
	*exit_str = tgetstr(exit_cap, spp);
	if (*exit_str == NULL) *exit_str = tgetstr("me", spp);
	if (*exit_str == NULL) *exit_str = "";
}

static char* ltgetstr(char* id, char** area) {
	char* str = tgetstr(id, area);
	if (str == NULL) str = "";
	return str;
}

// Initialize the terminfo struct with info about the terminal $TERM.
int setup_term(void) {
	static char termbuf[4096];
	static char sbuf[4096];
	char* term = getenv("TERM");
	if (term == NULL) term = "dumb";
	if (tgetent(termbuf, term) <= 0) {
		fprintf(stderr, "cannot setup terminal %s\n", term);
		return 0;
	}
	char* sp = sbuf;
	setup_mode("so", "se", &terminfo.enter_standout, &terminfo.exit_standout, &sp);
	setup_mode("us", "ue", &terminfo.enter_underline, &terminfo.exit_underline, &sp);
	setup_mode("md", "me", &terminfo.enter_bold, &terminfo.exit_bold, &sp);
	setup_mode("mb", "me", &terminfo.enter_blink, &terminfo.exit_blink, &sp);

	char* bs = ltgetstr("kb", &sp);
	terminfo.backspace_key = (strlen(bs) == 1) ? *bs : '\b';
	terminfo.cursor_move = ltgetstr("cm", &sp);
	terminfo.clear_screen = ltgetstr("cl", &sp);
	terminfo.init_term = ltgetstr("ti", &sp);
	terminfo.deinit_term = ltgetstr("te", &sp);
	terminfo.enter_keypad = ltgetstr("ks", &sp);
	terminfo.exit_keypad = ltgetstr("ke", &sp);
	terminfo.key_right = ltgetstr("kr", &sp);
	terminfo.key_left = ltgetstr("kl", &sp);
	terminfo.key_up = ltgetstr("ku", &sp);
	terminfo.key_down = ltgetstr("kd", &sp);
	terminfo.key_home = ltgetstr("kh", &sp);
	terminfo.key_end = ltgetstr("@7", &sp);
	return 1;
}
