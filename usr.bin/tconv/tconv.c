/*
 * tconv.c
 *
 * Ross Ridge
 * Public Domain
 * 92/02/01 07:30:23
 *
 * tconv [-b] [-c [-OUGd]] [-i] [-B [-D dir]] [-I] [-k] [-V] [-t term] [file]
 *
 * -c		convert from termcap
 * -i		convert from terminfo source
 * -b		convert from terminfo binary
 * -B		convert to terminfo binary
 * -I		convert to terminfo source
 * -V		print version info
 *
 * The following switches are available when converting from termcap:
 * -d		don't supply any defaults for missing capabilities
 * -O		include obsolete termcap capabilities
 * -G		include GNU capabilities
 * -U		include UW capabilities
 *
 * -k		keep comments
 * -D dir	directory to put terminfo binaries in
 *
 * -t term	name of terminal to translate
 * file		filename of termcap/terminfo database to use
 *
 * If a file is specifed and no terminal is given the entire file we be
 * translated.
 * If no terminal and no file is specified then the terminal name will be
 * taken from the environment variable TERM.
 * Unless compiling to a terminfo binary, output is to stdout.
 *
 */

#define NOTLIB
#include "defs.h"
#define SINGLE
#include <term.h>

#include <ctype.h>
#include <fcntl.h>
#ifdef USE_STDDEF
#include <sys/types.h>
#endif
#include <sys/stat.h>
#ifdef __FreeBSD__
#include <unistd.h>
#endif

#ifndef __FreeBSD__
#include "strtok.c"
#include "mkdir.c"
#endif

#ifndef lint
static const char SCCSid[] =
	"@(#) mytinfo tconv.c 3.2 92/02/01 public domain, By Ross Ridge";
static const char rcsid[] =
  "$FreeBSD$";
#endif

/* the right margin of the output */
#define LINELEN 76

struct term_path *path;	/* returned from _buildpath */

TERMINAL _term_buf;
char buf[MAX_BUF+1];	/* buffer for the termcap entry */

int noOT = 1;		/* -O */
int noGNU = 1;		/* -G */
int noUW = 1;		/* -W */
int dodefault = 1;	/* -d */
int keepcomments = 0;	/* -k */
int compile = 0;	/* -B */
int from_tcap = 0;	/* -c */
int from_tinfo = 0;	/* -i */
int from_tbin = 0;	/* -b */
char *directory = NULL; /* -D */

int continued = 0;
int termcap = 1;

int lineno = 0;		/* current line number */

/* print the first part of a warning message */
void
warn() {
	if (lineno == 0)
		fprintf(stderr, "warning: ");
	else
		fprintf(stderr, "(%s)%d: warning: ",
			_term_buf.name_long, lineno);
}

/* output a string indenting at the beginning of a line, and wraping
 * at the right margin.
 */
void
putstr(s)
char *s; {
	static pos = 0;
	int l;

	if (s == NULL) {
		if (pos != 0) {
			pos = 0;
			putchar('\n');
		}
		return;
	}

	if (termcap && noOT && *s == 'O')
		return;
	if (termcap && noGNU && *s == 'G')
		return;
	if (termcap && noUW && *s == 'U')
		return;

	l = strlen(s) + 2;

	if (l + pos > LINELEN && pos != 0) {
		putchar('\n');
		pos = 0;
	}

	if (pos == 0) {
		putchar('\t');
		pos = 8;
	} else
		putchar(' ');

	printf("%s,", s);

	pos += l;
}

#ifndef MAX_PUSHED
/* maximum # of parameters that can be pushed onto the stack */
#define MAX_PUSHED 16
#endif

int stack[MAX_PUSHED];	/* the stack */
int stackptr;		/* the next empty place on the stack */
int onstack;		/* the top of stack */
int seenm;		/* seen a %m */
int seenn;		/* seen a %n */
int seenr;		/* seen a %r */
int param;		/* current parameter */
char *dp;		/* pointer to the end of the converted string */

/* push onstack on to the stack */
void
push() {
	if (stackptr > MAX_PUSHED) {
		warn();
		fprintf(stderr, "string to complex to convert\n");
	} else
		stack[stackptr++] = onstack;
}

/* pop the top of the stack into onstack */
void
pop() {
	if (stackptr == 0)
		if (onstack == 0) {
			warn();
			fprintf(stderr, "I'm confused\n");
		} else
			onstack = 0;
	else
		onstack = stack[--stackptr];
	param++;
}

/* convert a character to a terminfo push */
static int
cvtchar(sp)
register char *sp; {
	char c;
	int l;

	switch(*sp) {
	case '\\':
		switch(*++sp) {
		case '\'':
		case '$':
		case '\\':
		case '%':
			c = *sp;
			l = 2;
			break;
		case '\0':
			c = '\\';
			l = 1;
			break;
		case '0':
			if (sp[1] == '0' && sp[2] == '0') {
				c = '\0';
				l = 4;
			} else {
				c = '\200'; /* '\0' ???? */
				l = 2;
			}
			break;
		default:
			c = *sp;
			l = 2;
			break;
		}
		break;
	default:
		c = *sp;
		l = 1;
	}
	c &= 0177;
	if (isgraph(c) && c != ',' && c != '\'' && c != '\\' && c != ':') {
		*dp++ = '%'; *dp++ = '\''; *dp++ = c; *dp++ = '\'';
	} else {
		*dp++ = '%'; *dp++ = '{';
		if (c > 99)
			*dp++ = c / 100 + '0';
		if (c > 9)
			*dp++ = (c / 10) % 10 + '0';
		*dp++ = c % 10 + '0';
		*dp++ = '}';
	}
	return l;
}

/* push n copies of param on the terminfo stack if not already there */
void
getparm(parm, n)
int parm;
int n; {
	if (seenr)  {
		if (parm == 1)
			parm = 2;
		else if (parm == 2)
			parm = 1;
	}
	if (onstack == parm) {
		if (n > 1) {
			warn();
			fprintf(stderr, "string may not be optimal");
			*dp++ = '%'; *dp++ = 'P'; *dp++ = 'a';
			while(n--) {
				*dp++ = '%'; *dp++ = 'g'; *dp++ = 'a';
			}
		}
		return;
	}
	if (onstack != 0)
		push();

	onstack = parm;

	while(n--) {		/* %p0 */
		*dp++ = '%'; *dp++ = 'p'; *dp++ = '0' + parm;
	}

	if (seenn && parm < 3) { /* %{96}%^ */
		*dp++ = '%'; *dp++ = '{'; *dp++ = '9'; *dp++ = '6'; *dp++ = '}';
		*dp++ = '%'; *dp++ = '^';
	}

	if (seenm && parm < 3) { /* %{127}%^ */
		*dp++ = '%'; *dp++ = '{'; *dp++ = '1'; *dp++ = '2'; *dp++ = '7';
		*dp++ = '}'; *dp++ = '%'; *dp++ = '^';
	}
}

/* convert a string to terminfo format */
char *
convstr(s, i)
register char *s;
int i; {
	static char line[MAX_LINE];
	register char *cap;
	int nocode = 0;

	stackptr = 0;
	onstack = 0;
	seenm = 0;
	seenn = 0;
	seenr = 0;
	param = 1;

	dp = line;
	cap = strnames[i];
#if 0
	if (cap[0] == 'k'
	    || ((cap[0] == 'i' || cap[0] == 'r') && cap[1] == 's'
		&& (cap[2] == '1' || cap[2] == '2' || cap[2] == '3')))
	/* if (k.* || [ir]s[123]) */
		nocode = 1;
#else
	if (_strflags[i] != 'G')
		nocode = 1;
#endif
	if (!nocode) {
		char *d = s;
		while(*s != '\0') {
			if (s[0] == '\\' && s[1] != '\0')
				s++;
			else if (s[0] == '%' && s[1] != '\0') {
				if (s[1] == 'p') {
					if (termcap) {
						warn();
						fprintf(stderr,
"string '%s' already in terminfo format\n", strcodes[i]);
						nocode = 1;
						break;
					} else
						nocode = 1;
				}
				s++;
			}
			s++;
		}
		if (!nocode && !termcap) {
			warn();
			fprintf(stderr,
"string '%s' not in terminfo format, converting...\n", cap);
		}
		s = d;
	}
	while(*s != '\0') {
		switch(*s) {
		case '%':
			s++;
			if (nocode) {
				*dp++ = '%';
				break;
			}
			switch(*s++) {
			case '%': *dp++ = '%'; break;
			case 'r':
				if (seenr++ == 1) {
					warn();
					fprintf(stderr, "seen %%r twice\n");
				}
				break;
			case 'm':
				if (seenm++ == 1) {
					warn();
					fprintf(stderr, "seen %%m twice\n");
				}
				break;
			case 'n':
				if (seenn++ == 1) {
					warn();
					fprintf(stderr, "seen %%n twice\n");
				}
				break;
			case 'i': *dp++ = '%'; *dp++ = 'i'; break;
			case '6':
			case 'B':
				getparm(param, 2);
				/* %{6}%*%+ */
				*dp++ = '%'; *dp++ = '{'; *dp++ = '6';
				*dp++ = '}'; *dp++ = '%'; *dp++ = '*';
				*dp++ = '%'; *dp++ = '+';
				break;
			case '8':
			case 'D':
				getparm(param, 2);
				/* %{2}%*%- */
				*dp++ = '%'; *dp++ = '{'; *dp++ = '2';
				*dp++ = '}'; *dp++ = '%'; *dp++ = '*';
				*dp++ = '%'; *dp++ = '-';
				break;
			case '>':
				getparm(param, 2);
				/* %?%{x}%>%t%{y}%+%; */
				*dp++ = '%'; *dp++ = '?';
				s += cvtchar(s);
				*dp++ = '%'; *dp++ = '>';
				*dp++ = '%'; *dp++ = 't';
				s += cvtchar(s);
				*dp++ = '%'; *dp++ = '+';
				*dp++ = '%'; *dp++ = ';';
				break;
			case 'a':
				if ((*s == '=' || *s == '+' || *s == '-'
				     || *s == '*' || *s == '/')
				    && (s[1] == 'p' || s[1] == 'c')
			            && s[2] != '\0') {
					int l;
					l = 2;
					if (*s != '=')
						getparm(param, 1);
					if (s[1] == 'p') {
						getparm(param + s[2] - '@', 1);
						if (param != onstack) {
							pop();
							param--;
						}
						l++;
					} else
						l += cvtchar(s + 2);
					switch(*s) {
					case '+':
						*dp++ = '%'; *dp++ = '+';
						break;
					case '-':
						*dp++ = '%'; *dp++ = '-';
						break;
					case '*':
						*dp++ = '%'; *dp++ = '*';
						break;
					case '/':
						*dp++ = '%'; *dp++ = '/';
						break;
					case '=':
						if (seenr)
							if (param == 1)
								onstack = 2;
							else if (param == 2)
								onstack = 1;
							else
								onstack = param;
						else
							onstack = param;
						break;
					}
					s += l;
					break;
				}
				getparm(param, 1);
				s += cvtchar(s);
				*dp++ = '%'; *dp++ = '+';
				break;
			case '+':
				getparm(param, 1);
				s += cvtchar(s);
				*dp++ = '%'; *dp++ = '+';
				*dp++ = '%'; *dp++ = 'c';
				pop();
				break;
			case 's':
				s += cvtchar(s);
				getparm(param, 1);
				*dp++ = '%'; *dp++ = '-';
				break;
			case '-':
				s += cvtchar(s);
				getparm(param, 1);
				*dp++ = '%'; *dp++ = '-';
				*dp++ = '%'; *dp++ = 'c';
				pop();
				break;
			case '.':
				getparm(param, 1);
				*dp++ = '%'; *dp++ = 'c';
				pop();
				break;
			case '2':
				getparm(param, 1);
				*dp++ = '%'; *dp++ = '0';
				*dp++ = '2'; *dp++ = 'd';
				pop();
				break;
			case '3':
				getparm(param, 1);
				*dp++ = '%'; *dp++ = '0';
				*dp++ = '3'; *dp++ = 'd';
				pop();
				break;
			case 'd':
				getparm(param, 1);
				*dp++ = '%'; *dp++ = 'd';
				pop();
				break;
			case 'f':
				param++;
				break;
			case 'b':
				param--;
				break;
			default:
				warn();
				*dp++ = '%';
				s--;
				fprintf(stderr, "'%s' unknown %% code %c",
					strcodes[i], *s);
				if (*s >= 0 && *s < 32)
					fprintf(stderr, "^%c\n", *s + '@');
				else if (*s < 0 || *s >= 127)
					fprintf(stderr, "\\%03o\n", *s & 0377);
				else
					fprintf(stderr, "%c\n", *s);
				break;
			}
			break;
		case '\\':
			if (!compile) {*dp++ = *s++; *dp++ = *s++; break;}
			/* FALLTHROUGH */
		case '\n':
			if (!compile) {*dp++ = '\\'; *dp++ = 'n'; s++; break;}
			/* FALLTHROUGH */
		case '\t':
			if (!compile) {*dp++ = '\\'; *dp++ = 't'; s++; break;}
			/* FALLTHROUGH */
		case '\r':
			if (!compile) {*dp++ = '\\'; *dp++ = 'r'; s++; break;}
			/* FALLTHROUGH */
		case '\200':
			if (!compile) {*dp++ = '\\'; *dp++ = '0'; s++; break;}
			/* FALLTHROUGH */
		case '\f':
			if (!compile) {*dp++ = '\\'; *dp++ = 'f'; s++; break;}
			/* FALLTHROUGH */
		case '\b':
			if (!compile) {*dp++ = '\\'; *dp++ = 'b'; s++; break;}
			/* FALLTHROUGH */
		case ' ':
			if (!compile) {*dp++ = '\\'; *dp++ = 's'; s++; break;}
			/* FALLTHROUGH */
		case '^':
			if (!compile) {*dp++ = '\\'; *dp++ = '^'; s++; break;}
			/* FALLTHROUGH */
		case ':':
			if (!compile) {*dp++ = '\\'; *dp++ = ':'; s++; break;}
			/* FALLTHROUGH */
		case ',':
			if (!compile) {*dp++ = '\\'; *dp++ = ','; s++; break;}
			/* FALLTHROUGH */
#if 0
		case '\'':
			if (!compile) {*dp++ = '\\'; *dp++ = '\''; s++; break;}
			/* FALLTHROUGH */
#endif
		default:
			if (compile)
				*dp++ = *s++;
			else if (*s > 0 && *s < 32) {
				*dp++ = '^';
				*dp++ = *s + '@';
				s++;
			} else if (*s <= 0 || *s >= 127) {
				*dp++ = '\\';
				*dp++ = ((*s & 0300) >> 6) + '0';
				*dp++ = ((*s & 0070) >> 3) + '0';
				*dp++ = (*s & 0007) + '0';
				s++;
			} else
				*dp++ = *s++;
			break;
		}
	}
	*dp = '\0';
	return line;
}

#define LSB(n) ((unsigned) (n) & 0377)
#define MSB(n) (((unsigned) (n) >> 8) & 0377)

void
writebin(fd, name)
int fd;
char *name; {
	static char bin[MAX_BUF + 1];
	register char *s;
	register char *d;
	register int i;
	register char *t;
	register int n;
	char *strtbl;
	int sz_name, n_bools, n_nums, n_offs, sz_strs;
	extern int _boolorder[], _numorder[], _strorder[];

	strncpy(bin + 12, name, 127);
	bin[12 + 128] = '\0';
	sz_name = strlen(name) + 1;
	if (sz_name > 128)
		sz_name = 128;

	s = bin + 12 + sz_name;
	for(i = 0; _boolorder[i] != -1; i++) {
		switch(_term_buf.bools[i]) {
		case -1: *s++ = 0; break;
		case  0: *s++ = 0377; break;
		default: *s++ = 1; break;
		}
	}
	n_bools = i;
	if ((sz_name + n_bools) & 1)
		n_bools++;

	s = bin + 12 + sz_name + n_bools;
	for(i = 0; _numorder[i] != -1; i++) {
		n = _term_buf.nums[_numorder[i]];
		switch(n) {
		case -2: *s++ = 0377; *s++ = 0377; break;
		case -1: *s++ = 0376; *s++ = 0377; break;
		default:
			*s++ = LSB(n);
			*s++ = MSB(n);
		}
	}
	n_nums = i;

	s = bin + 12 + sz_name + n_bools + n_nums * 2;
	for(i = 0; _strorder[i] != -1; i++) {
		if (_term_buf.strs[_strorder[i]] == (char *) 0) {
			*s++ = 0376; *s++ = 0377;
		} else {
			*s++ = 0377; *s++ = 0377;
		}
	}
	n_offs = i;

	s = bin + 12 + sz_name + n_bools + n_nums * 2;
	strtbl = d = s + n_offs * 2;
	for(i = 0; _strorder[i] != -1; i++) {
		t = _term_buf.strs[_strorder[i]];
		if (t == (char *) -1 || t == (char *)  0)
			s += 2;
		else {
			n = d - strtbl;
			*s++ = LSB(n);
			*s++ = MSB(n);
			t = convstr(t, _strorder[i]);
			while(*t != '\0') {
				*d++ = *t++;
				if (d >= bin + MAX_BUF - 1) {
					warn();
					fprintf(stderr,
					"compiled entry to big\n");
					*d++ = '\0';
					goto toobig;
				}
			}
			*d++ = '\0';
		}
	}

toobig:
	sz_strs = d - strtbl;

	bin[0] = 032;
	bin[1] = 01;
	bin[2] = LSB(sz_name);
	bin[3] = MSB(sz_name);
	bin[4] = LSB(n_bools);
	bin[5] = MSB(n_bools);
	bin[6] = LSB(n_nums);
	bin[7] = MSB(n_nums);
	bin[8] = LSB(n_offs);
	bin[9] = MSB(n_offs);
	bin[10] = LSB(sz_strs);
	bin[11] = MSB(sz_strs);

	if (write(fd, bin, d - bin) == -1)
		quit(errno, "can't write binary file");

	return;
}

void
find_directory() {
	struct term_path *p;
	struct stat st;

	if (directory != NULL)
		return;

	p = path;
	while(p->type != -1 && p->file != NULL) {
		if (stat(p->file, &st) == 0) {
			if ((st.st_mode & 0170000) == 0040000) {
				directory = p->file;
				return;
			}
		}
		p++;
	}
	quit(-1, "can't find a terminfo directory");
}

/* convert a terminal name to a binary filename */
char *
binfile(name)
char *name; {
	static char line[MAX_LINE+1];

	sprintf(line, "%s/%c/%s", directory, *name, name);
	return line;
}

char *
bindir(name)
char *name; {
	static char line[MAX_LINE+1];

	sprintf(line, "%s/%c", directory, *name);
	return line;
}

int
badname(name)
char *name; {
	while(*name) {
		if (*name == '/' || !isgraph(*name))
			return 1;
		name++;
	}
	return 0;
}

/* output a terminfo binary */
void
outputbin(name)
char *name; {
	register char *s, *d, *last;
	char tmp[MAX_LINE+1];
	char line[MAX_LINE+1];
	int fd;

	find_directory();

	s = name;
	d = line;
	while(*s != '\0' && d < line + MAX_LINE) {
		*d++ = *s++;
	}

	while(d > line && d[-1] == '|') {
		d--;
	}

	*d = '\0';

	s = strtok(line, "|");
	last = NULL;

	while(s != NULL && last == NULL) {
		if (*s == '\0') {
			;
		} else if (badname(s)) {
			if (lineno)
				warn();
			fprintf(stderr, "bad terminal name '%s', ignored.\n",
				s);
		} else {
			if (access(bindir(s), 2) == -1) {
				if (errno != ENOENT)
					quit(errno,
					     "can't access directory '%s'",
					     bindir(s));
				if (mkdir(bindir(s), 0777) == -1)
					quit(errno, "can't make directory '%s'",
					     bindir(s));
			}
			fd = open(binfile(s), O_WRONLY | O_CREAT | O_EXCL,
				  0666);
			if (fd == -1) {
				if (errno != EEXIST)
					quit(errno, "can't open file '%s'",
					     binfile(s));
				if (unlink(binfile(s)) == -1)
					quit(errno, "can't unlink file '%s'",
					     binfile(s));
				fd = open(binfile(s),
					  O_WRONLY | O_CREAT | O_EXCL, 0666);
				if (fd == -1)
					quit(errno, "can't create file '%s'",
					     binfile(s));
			}
			writebin(fd, name);
			close(fd);
			last = s;
		}
		s = strtok(NULL, "|");
	}

	if (last == NULL) {
		if (lineno)
			warn();
		fprintf(stderr, "no terminal name, entry ignored.\n");
		return;
	}

	while(s != NULL && s + strlen(s) != d) {
		if (*s == '\0' || strcmp(s, last) == 0) {
			;
		} else if (badname(s)) {
			if (lineno)
				warn();
			fprintf(stderr, "bad terminal name '%s', ignored.\n",
				s);
		} else {
			if (access(bindir(s), 2) == -1) {
				if (errno != ENOENT)
					quit(errno,
					     "can't access directory '%s'",
					     bindir(s));
				if (mkdir(bindir(s), 0777) == -1)
					quit(errno, "can't make directory '%s'",
					     bindir(s));
			}
			if (access(binfile(s), 0) == -1) {
				if (errno != ENOENT)
					quit(errno, "can't access file '%s'",
					     binfile(s));
			} else if (unlink(binfile(s)) == -1) {
					quit(errno, "can't unlink file '%s'",
					     binfile(s));
			}
			strcpy(tmp, binfile(last));
			if (link(tmp, binfile(s)) == -1) {
				quit(errno, "can't link '%s' to '%s'",
				     last, binfile(s));
			}
		}
		s = strtok(NULL, "|");
	}
	return;
}

/* output an entry in terminfo source format */
void
outputinfo(name)
char *name; {
	int i;
	char line[MAX_LINE];

	printf("%s,\n", name);

	for(i = 0; i < NUM_OF_BOOLS; i++)
		if (_term_buf.bools[i] == 0) {
			sprintf(line, "%s@", boolnames[i]);
			putstr(line);
		} else if (_term_buf.bools[i] != -1)
			putstr(boolnames[i]);

	for(i = 0; i < NUM_OF_NUMS; i++)
		if (_term_buf.nums[i] == -1) {
			sprintf(line, "%s@", numnames[i]);
			putstr(line);
		} else if (_term_buf.nums[i] != -2) {
			sprintf(line, "%s#%d", numnames[i], _term_buf.nums[i]);
			putstr(line);
		}

	for(i = 0; i < NUM_OF_STRS; i++)
		if (_term_buf.strs[i] == NULL) {
			sprintf(line, "%s@", strnames[i]);
			putstr(line);
		} else if (_term_buf.strs[i] != (char *) -1) {
			sprintf(line, "%s=%s", strnames[i],
				convstr(_term_buf.strs[i], i));
			putstr(line);
		}
	putstr(NULL);
}

/* convert a terminfo entry to binary format */
void
convtinfo() {
	int i, r;

	termcap = 0;

	for(i = 0; i < NUM_OF_BOOLS; i++)
		_term_buf.bools[i] = -1;
	for(i = 0; i < NUM_OF_NUMS; i++)
		_term_buf.nums[i] = -2;
	for(i = 0; i < NUM_OF_STRS; i++)
		_term_buf.strs[i] = (char *) -1;

	_term_buf.name_all = NULL;

	r = _gettinfo(buf, &_term_buf, path);
	if (r != 0) {
		if (lineno == 0)
			quit(-1, "problem reading entry");
		else {
			warn();
			fprintf(stderr, "problem reading entry\n");
		}
	}

	if (compile)
		outputbin(_term_buf.name_all);
	else
		outputinfo(_term_buf.name_all);
	return;
}

/* convert a terminfo binary to terminfo source */
void
convtbin() {
	int i, r;

	termcap = 0;

	for(i = 0; i < NUM_OF_BOOLS; i++)
		_term_buf.bools[i] = -1;
	for(i = 0; i < NUM_OF_NUMS; i++)
		_term_buf.nums[i] = -2;
	for(i = 0; i < NUM_OF_STRS; i++)
		_term_buf.strs[i] = (char *) -1;

	_term_buf.name_all = NULL;

	r = _gettbin(buf, &_term_buf);
	if (r != 0) {
		if (lineno == 0)
			quit(-1, "problem reading entry");
		else {
			warn();
			fprintf(stderr, "problem reading entry\n");
		}
	}

	outputinfo(_term_buf.name_all);

	return;
}

/* convert a termcap entry to terminfo format */
void
convtcap() {
	int i, r;
	char *name;

	termcap = 1;

	for(i = 0; i < NUM_OF_BOOLS; i++)
		_term_buf.bools[i] = -1;
	for(i = 0; i < NUM_OF_NUMS; i++)
		_term_buf.nums[i] = -2;
	for(i = 0; i < NUM_OF_STRS; i++)
		_term_buf.strs[i] = (char *) -1;

	_term_buf.name_all = NULL;


#if DEBUG
	printf("%s\n", buf);
#endif
	r = _gettcap(buf, &_term_buf, path);
	if (r != 0) {
		if (lineno == 0)
			quit(-1, "problem reading entry");
		else {
			warn();
			fprintf(stderr, "problem reading entry\n");
		}
	}

	if (dodefault && !continued)
		_tcapdefault();

	_tcapconv();

	name = _term_buf.name_all;
#if DEBUG
	printf("...%s\n", name);
#endif
	if (name[0] != '\0' && name[1] != '\0' && name[2] == '|')
		name += 3;	/* skip the 2 letter code */

	if (compile)
		outputbin(name);
	else
		outputinfo(name);
}

void
convbinfile(file)
char *file; {
	register FILE *f;
	int r;

	f = fopen(file, "r");

	if (f == NULL)
		quit(errno, "can't open '%s'", file);

	r = fread(buf, sizeof(char), MAX_BUF, f);
	if (r < 12 || buf[0] != 032 || buf[1] != 01)
		quit(-1, "file '%s' corrupted", file);

	convtbin();
}

/* convert a termcap file to terminfo format */
void
convtcfile(file)
char *file; {
	int nocolon;
	register int c;
	register char *d;
	register FILE *f;

	f = fopen(file, "r");

	if (f == NULL)
		quit(errno, "can't open '%s'", file);

	d = buf;
	c = getc(f);
	while(c != EOF) {
		lineno++;
		if (c == '#') {
			if (keepcomments) {
				do {
					putchar(c);
					c = getc(f);
				} while(c != '\n' && c != EOF);
				putchar('\n');
			} else
				do
					c = getc(f);
				while(c != '\n' && c != EOF);
			if (c != EOF)
				c = getc(f);
			continue;
		}
		while(isspace(c) && c != '\n')
			c = getc(f);
		if (c == '\n' && buf == d) {
			c = getc(f);
			continue;
		}

		while(c != EOF) {
			if (c == '\\') {
				c = getc(f);
				if (c == EOF)
					break;
				if (c == '\n') {
					c = getc(f);
					break;
				}
				*d++ = '\\';
				*d++ = c;
			} else if (c == '\n') {
				*d = '\0';
				if (*--d == ':') {
					nocolon = 0;
					*d-- = '\0';
				} else {
					nocolon = 1;
				}
				while(d > buf && *d != ':')
					d--;
				if (d[1] == 't' && d[2] == 'c' && d[3] == '=') {
					continued = 1;
					d[1] = '\0';
				} else
					continued = 0;
				convtcap();
				if (nocolon) {
					warn();
					fprintf(stderr,
						"entry doesn't end with :\n");
				}
				_term_buf.strbuf = _endstr();
				_del_strs(&_term_buf);
				if (continued) {
					printf("\tuse=%s,\n", d + 4);
				}
				d = buf;
				c = getc(f);
				break;
			} else
				*d++ = c;
			c = getc(f);
		}
	}
}

static int
getln(f, buf, len)
FILE *f;
register char *buf;
int len; {
	register int c, i = 0;

	while((c = getc(f)) == '#') {
		lineno++;
		if (keepcomments) {
			putchar('#');
			while((c = getc(f)) != '\n') {
				if (c == EOF)
					return -1;
				putchar(c);
			}
			putchar('\n');
		} else {
			while((c = getc(f)) != '\n')
				if (c == EOF)
					return -1;
		}
	}

	lineno++;
	while(c != '\n') {
		if (c == EOF)
			return -1;
		if (i < len) {
			i++;
			*buf++ = c;
		}
		c = getc(f);
	}

	while(isspace(*(buf-1))) {
		buf--;
		i--;
	}

	*buf = '\0';
	return i;
}

void
convtifile(file)
char *file; {
	static char line[MAX_LINE+1];
	int l;
	int n;
	register FILE *f;

	f = fopen(file, "r");

	if (f == NULL)
		quit(errno, "can't open '%s'", file);

	lineno = 0;

	l = getln(f, line, MAX_LINE);
	while(l != -1) {
		if (line[l-1] == ':') {
			strncpy(buf, line, MAX_BUF);
			convtcap();
		} else if (line[l-1] == '\\') {
			n = MAX_BUF;
			do {
				line[--l] = '\0';
				if (n > 0)
					strncpy(buf + MAX_BUF - n, line, n);
				n -= l;
				l = getln(f, line, MAX_LINE);
			} while(l != -1 && line[l-1] == '\\');
			if (n > 0 && l != -1)
				strncpy(buf + MAX_BUF - n, line, n);
			convtcap();
		} else if (line[l-1] == ',') {
			n = MAX_BUF;
			do {
				if (n > 0)
					strncpy(buf + MAX_BUF - n, line, n);
				n -= l;
				l = getln(f, line, MAX_LINE);
			} while(l != -1 && isspace(line[0]));
#if 0
			printf("buf = '%s'\n", buf);
#endif
			convtinfo();
			continue;
		} else if (line[0] != '\0') {
			warn();
			fprintf(stderr, "malformed line\n");
			if (keepcomments) {
				printf("%s\n", line);
			}
		}
		l = getln(f, line, MAX_LINE);
	}
	return;
}

/* dummy routine for quit */
/* ARGSUSED */
void
do_cleanup(e)
int e; {
	return;
}

/* print out usage, called by quit */
/* ARGSUSED */  
void
usage(e)
int e; {
	fprintf(stderr, "%s\n%s\n%s\n%s\n", 
		"usage: tconv [-b] [-c [-OUGd]] [-i] [-B [-D dir]] [-I] [-k] [-V]",
		"             [-t term] [file]",
		"       tic [file]",
		"       captoinfo [-t term] [-OUGdk] [file]");
	return;
}

int
main(argc, argv)
int argc;
char **argv; {
	char *term = NULL;
	char *file = NULL;
	int r;
	char c;
	int pversion = 0;

	prg_name = strrchr(argv[0], '/');
	if (prg_name == NULL)
		prg_name = argv[0];
	else
		prg_name++;

	cleanup = usage;

	opterr = 0;

	if (strcmp(prg_name, "tic") == 0)
		compile = 1;

	while ((c = getopt(argc, argv, "bciBIOGUdkVD:t:")) != -1) {
		switch(c) {
		case 'O':
			noOT = 0;
			break;
		case 'G':
			noGNU = 0;
			break;
		case 'U':
			noUW = 0;
			break;
		case 'D':
			if (directory != NULL)
				quit(-1, "more than one directory specified");
			directory = optarg;
			break;
		case 't':
			if (term != NULL)
				quit(-1, "more than one terminal specified");
			term = optarg;
			break;
		case 'd': dodefault = 0; break;
		case 'k': keepcomments = 1; break;
		case 'b': from_tbin = 1; break;
		case 'c': from_tcap = 1; break;
		case 'i': from_tinfo = 1; break;
		case 'B': compile = 1; break;
		case 'I': compile = 0; break;
		case 'V': pversion = 1; break;
		case '?':
		default:
			quit(-1, "bad or missing command line argument");
		}
	}

	if (pversion) {
		quit(0, "%s\n%s", _mytinfo_version, SCCSid);
	}

	if (optind == argc - 1)
		file = argv[optind];
	else if (optind != argc)
		quit(-1, "wrong number of arguments");

	if (from_tbin + from_tcap + from_tinfo > 1)
		quit(-1, "more than one input file type specified");

	if (!from_tcap && !from_tinfo && !from_tbin && file != NULL) {
		if (strcmp(prg_name, "cap2info") == 0
		    || strcmp(prg_name, "captoinfo") == 0)
			from_tcap = 1;
		else if (strcmp(prg_name, "tic") == 0)
			from_tinfo = 1;
		else
			quit(-1, "no input file type specified");
	}

	if (from_tbin && compile)
		quit(-1, "can't convert from binary to binary");

	if (file != NULL) {
		if (from_tbin) {
			cleanup = do_cleanup;
			convbinfile(file);
			exit(0);
		}
		if (!compile)
			path = _buildpath(file, 0, NULL, -1);
		else {
			path = _buildpath(file, 0,
					  "$TERMINFO", 2,
					  "$MYTERMINFO", 2,
#ifdef TERMINFODIR
					  TERMINFODIR, 0,
#endif
					  NULL, -1);
		}
		if (path == NULL)
			quit(-1, "can't build path");
		if (term == NULL) {
			cleanup = do_cleanup;
			if (from_tcap && !compile)
				convtcfile(file);
			else
				convtifile(file);
			exit(0);
		}
	} else if (from_tcap && !compile)
		path = _buildpath("$TERMCAP", 1,
#ifdef TERMCAPFILE
				  TERMCAPFILE, 0,
#endif
				  NULL, -1);
	else if (from_tinfo || from_tbin)
		path = _buildpath("$TERMINFO", 2,
				  "$MYTERMINFO", 2,
#ifdef TERMINFODIR
				  TERMINFODIR, 0,
#endif
#ifdef TERMINFOSRC
				  TERMINFOSRC, 0,
#endif
				  NULL, -1);
	else if (from_tcap)
		path = _buildpath("$TERMCAP", 1,
#ifdef TERMCAPFILE
				  TERMCAPFILE, 0,
#endif
				  "$TERMINFO", 2,
				  "$MYTERMINFO", 2,
#ifdef TERMINFODIR
				  TERMINFODIR, 0,
#endif
				  NULL, -1);
	else
		path = _buildpath(
#ifdef USE_TERMINFO
				  "$MYTERMINFO", 2,
				  "$TERMINFO", 2,
#ifdef TERMINFODIR
				  TERMINFODIR, 0,
#endif
#ifdef TERMINFOSRC
				  TERMINFOSRC, 0,
#endif
#endif
#ifdef USE_TERMCAP
				  "$TERMCAP", 1,
#ifdef TERMCAPFILE
				  TERMCAPFILE, 0,
#endif
#endif
				  NULL, -1);
	if (term == NULL) {
		term = getenv("TERM");
		if (term == NULL)
			quit(-1, "no terminal type given");
	}

	cleanup = do_cleanup;

	r = _findterm(term, path, buf);
	switch(r) {
	case 1:
		convtcap();
		break;
	case 2:
		convtinfo();
		break;
	case 3:
		if (compile)
			quit(-1, "entry is already compiled");
		convtbin();
		break;
	default:
		quit(-1, "can't find a terminal entry for '%s'", term);
	}

	exit(0);
	return 0;
}
