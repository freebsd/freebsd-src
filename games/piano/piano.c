/*
 * piano.c - a piano emulator
 */
static const char rcsid[] =
  "$FreeBSD$";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include <unistd.h>
#include <sys/file.h>

char *myname;
int verbose;
static const char *initcmd = "t160 o1 l16 ml";

static const char usage_msg[] =
	"simple keyboard player V0.8086\n"
	"usage: %s [-v][-i str]\n"
	"\t-i str defaults 't160 o1 l16 ml'\n"
	"function: play by console keyboard\n"
	"\tESC to exit. Note keys are ...\n"
	"\t1 2   4 5   7 8 9   - = \\\n"
	"\t Q W E R T Y U I O P [ ]\n"
	;

struct kdef_t {
	int ch;
	const char *str;
};

static const char *kstr[256];

static struct kdef_t kdef[] = {
	/* white key */
	{ '\t', "<g>" },
	{ 'q', "<a>" },
	{ 'w', "<b>" },
	{ 'e', "c" },
	{ 'r', "d" },
	{ 't', "e" },
	{ 'y', "f" },
	{ 'u', "g" },
	{ 'i', "a" },
	{ 'o', "b" },
	{ 'p', ">c<" },
	{ '[', ">d<" },
	{ ']', ">e<" },
	{ '\n', ">f<" },
	{ '\r', ">f<" },
	/* black key */
	{ '`', "<f#>" },
	{ '1', "<g#>" },
	{ '2', "<a#>" },
	/*{ '3', "<b#>" },*/
	{ '4', "c#" },
	{ '5', "d#" },
	/*{ '6', "e#" },*/
	{ '7', "f#" },
	{ '8', "g#" },
	{ '9', "a#" },
	/*{ '0', "b#" },*/
	{ '-', ">c#<" },
	{ '=', ">d#<" },
	/*{ '\', ">e#<" },*/
	{ '\177', ">f#<" },
	{ '\0', NULL }
};

static int
init_kstr(void)
{
	struct kdef_t *mv = kdef;
	while (mv->str != NULL) {
		kstr[mv->ch] = mv->str;
		mv++;
	}/* while */
	return 0;
}/* init_kstr */

static int
fdputs(const char *s, int fd, int p_echo)
{
	int err;
	size_t len;
	len = strlen(s);
	write(fd, s, len);
	err = write(fd, "\n", 1);
	if (p_echo) {
		fputs(s, stdout);
	}
	return err;
}/* fdputs */

static int
outspkr(const char *s)
{
	int err = -1, fd = open("/dev/speaker", O_WRONLY);
	if (fd >= 0) {
		fdputs(initcmd, fd, 0);
		err = fdputs(s, fd, verbose);
		close(fd);
	}
	return err;
}/* outspkr */

static int
nain(void)
{
	int ch;
	initscr();
	noecho();
	nonl();
	raw();
	init_kstr();
	while ((ch = getch()) != '\033') {
		if (kstr[ch] != NULL) {
			outspkr(kstr[ch]);
		}
		else {
			if (verbose) {
				switch (ch) {
				case ' ':
					fputs(" ", stdout);
					break;
				case '\b':
					fputs("\b", stdout);
					break;
				}/* switch */
			}
		}
	}/* while */
	endwin();
	return 0;
}/* nain */

int
main(int argc, char *argv[])
{
	int ch, ex, show_usage = 0;
	myname = argv[0];
	while ((ch = getopt(argc, argv, "-vi:")) != -1) {
		switch (ch) {
		default:
		case 'V':
			show_usage++;
			break;
		case 'v':
			verbose++;
			break;
		case 'i':
			initcmd = optarg;
			break;
		}/* switch */
	}/* while */
	ex = 1;
	if (show_usage) {
		fprintf(stderr, usage_msg, myname);
	}
	else {
		printf("Type ESC to exit.\n");
		ex = 0;
		nain();
	}
	return ex;
}/* main */
