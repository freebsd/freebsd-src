/*  Copyright 1992 Simmule Turner and Rich Salz.  All rights reserved. 
 *
 *  This software is not subject to any license of the American Telephone 
 *  and Telegraph Company or of the Regents of the University of California. 
 *
 *  Permission is granted to anyone to use this software for any purpose on
 *  any computer system, and to alter it and redistribute it freely, subject
 *  to the following restrictions:
 *  1. The authors are not responsible for the consequences of use of this
 *     software, no matter how awful, even if they arise from flaws in it.
 *  2. The origin of this software must not be misrepresented, either by
 *     explicit claim or by omission.  Since few users ever read sources,
 *     credits must appear in the documentation.
 *  3. Altered versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.  Since few users
 *     ever read sources, credits must appear in the documentation.
 *  4. This notice may not be removed or altered.
 */

/*
**  Main editing routines for editline library.
*/
#include <config.h>
#include "editline.h"
#include <ctype.h>
#include <errno.h>

RCSID("$Id: editline.c,v 1.10 2001/09/13 01:19:54 assar Exp $");

/*
**  Manifest constants.
*/
#define SCREEN_WIDTH	80
#define SCREEN_ROWS	24
#define NO_ARG		(-1)
#define DEL		127
#define CTL(x)		((x) & 0x1F)
#define ISCTL(x)	((x) && (x) < ' ')
#define UNCTL(x)	((x) + 64)
#define META(x)		((x) | 0x80)
#define ISMETA(x)	((x) & 0x80)
#define UNMETA(x)	((x) & 0x7F)
#if	!defined(HIST_SIZE)
#define HIST_SIZE	20
#endif	/* !defined(HIST_SIZE) */

/*
**  Command status codes.
*/
typedef enum _el_STATUS {
    CSdone, CSeof, CSmove, CSdispatch, CSstay
} el_STATUS;

/*
**  The type of case-changing to perform.
*/
typedef enum _CASE {
    TOupper, TOlower
} CASE;

/*
**  Key to command mapping.
*/
typedef struct _KEYMAP {
    unsigned char	Key;
    el_STATUS	(*Function)();
} KEYMAP;

/*
**  Command history structure.
*/
typedef struct _HISTORY {
    int		Size;
    int		Pos;
    unsigned char	*Lines[HIST_SIZE];
} HISTORY;

/*
**  Globals.
*/
int		rl_eof;
int		rl_erase;
int		rl_intr;
int		rl_kill;

static unsigned char		NIL[] = "";
static const unsigned char	*Input = NIL;
static unsigned char		*Line;
static const char	*Prompt;
static unsigned char		*Yanked;
static char		*Screen;
static char		NEWLINE[]= CRLF;
static HISTORY		H;
int		rl_quit;
static int		Repeat;
static int		End;
static int		Mark;
static int		OldPoint;
static int		Point;
static int		PushBack;
static int		Pushed;
static KEYMAP		Map[33];
static KEYMAP		MetaMap[16];
static size_t		Length;
static size_t		ScreenCount;
static size_t		ScreenSize;
static char		*backspace;
static int		TTYwidth;
static int		TTYrows;

/* Display print 8-bit chars as `M-x' or as the actual 8-bit char? */
int		rl_meta_chars = 1;

/*
**  Declarations.
*/
static unsigned char	*editinput(void);
char	*tgetstr(const char*, char**);
int	tgetent(char*, const char*);
int	tgetnum(const char*);

/*
**  TTY input/output functions.
*/

static void
TTYflush()
{
    if (ScreenCount) {
	write(1, Screen, ScreenCount);
	ScreenCount = 0;
    }
}

static void
TTYput(unsigned char c)
{
    Screen[ScreenCount] = c;
    if (++ScreenCount >= ScreenSize - 1) {
	ScreenSize += SCREEN_INC;
	Screen = realloc(Screen, ScreenSize);
    }
}

static void
TTYputs(const char *p)
{
    while (*p)
	TTYput(*p++);
}

static void
TTYshow(unsigned char c)
{
    if (c == DEL) {
	TTYput('^');
	TTYput('?');
    }
    else if (ISCTL(c)) {
	TTYput('^');
	TTYput(UNCTL(c));
    }
    else if (rl_meta_chars && ISMETA(c)) {
	TTYput('M');
	TTYput('-');
	TTYput(UNMETA(c));
    }
    else
	TTYput(c);
}

static void
TTYstring(unsigned char *p)
{
    while (*p)
	TTYshow(*p++);
}

static int
TTYget()
{
    char c;
    int e;

    TTYflush();
    if (Pushed) {
	Pushed = 0;
	return PushBack;
    }
    if (*Input)
	return *Input++;
    do {
	e = read(0, &c, 1);
    } while(e < 0 && errno == EINTR);
    if(e == 1)
	return c;
    return EOF;
}

static void
TTYback(void)
{
    if (backspace)
	TTYputs(backspace);
    else
	TTYput('\b');
}

static void
TTYbackn(int n)
{
    while (--n >= 0)
	TTYback();
}

static void
TTYinfo()
{
    static int		init;
    char		*term;
    char		buff[2048];
    char		*bp;
    char		*tmp;
#if	defined(TIOCGWINSZ)
    struct winsize	W;
#endif	/* defined(TIOCGWINSZ) */

    if (init) {
#if	defined(TIOCGWINSZ)
	/* Perhaps we got resized. */
	if (ioctl(0, TIOCGWINSZ, &W) >= 0
	 && W.ws_col > 0 && W.ws_row > 0) {
	    TTYwidth = (int)W.ws_col;
	    TTYrows = (int)W.ws_row;
	}
#endif	/* defined(TIOCGWINSZ) */
	return;
    }
    init++;

    TTYwidth = TTYrows = 0;
    bp = &buff[0];
    if ((term = getenv("TERM")) == NULL)
	term = "dumb";
    if (tgetent(buff, term) < 0) {
       TTYwidth = SCREEN_WIDTH;
       TTYrows = SCREEN_ROWS;
       return;
    }
    tmp = tgetstr("le", &bp);
    if (tmp != NULL)
	backspace = strdup(tmp);
    else
	backspace = "\b";
    TTYwidth = tgetnum("co");
    TTYrows = tgetnum("li");

#if	defined(TIOCGWINSZ)
    if (ioctl(0, TIOCGWINSZ, &W) >= 0) {
	TTYwidth = (int)W.ws_col;
	TTYrows = (int)W.ws_row;
    }
#endif	/* defined(TIOCGWINSZ) */

    if (TTYwidth <= 0 || TTYrows <= 0) {
	TTYwidth = SCREEN_WIDTH;
	TTYrows = SCREEN_ROWS;
    }
}


/*
**  Print an array of words in columns.
*/
static void
columns(int ac, unsigned char **av)
{
    unsigned char	*p;
    int		i;
    int		j;
    int		k;
    int		len;
    int		skip;
    int		longest;
    int		cols;

    /* Find longest name, determine column count from that. */
    for (longest = 0, i = 0; i < ac; i++)
	if ((j = strlen((char *)av[i])) > longest)
	    longest = j;
    cols = TTYwidth / (longest + 3);

    TTYputs(NEWLINE);
    for (skip = ac / cols + 1, i = 0; i < skip; i++) {
	for (j = i; j < ac; j += skip) {
	    for (p = av[j], len = strlen((char *)p), k = len; --k >= 0; p++)
		TTYput(*p);
	    if (j + skip < ac)
		while (++len < longest + 3)
		    TTYput(' ');
	}
	TTYputs(NEWLINE);
    }
}

static void
reposition()
{
    int		i;
    unsigned char	*p;

    TTYput('\r');
    TTYputs(Prompt);
    for (i = Point, p = Line; --i >= 0; p++)
	TTYshow(*p);
}

static void
left(el_STATUS Change)
{
    TTYback();
    if (Point) {
	if (ISCTL(Line[Point - 1]))
	    TTYback();
        else if (rl_meta_chars && ISMETA(Line[Point - 1])) {
	    TTYback();
	    TTYback();
	}
    }
    if (Change == CSmove)
	Point--;
}

static void
right(el_STATUS Change)
{
    TTYshow(Line[Point]);
    if (Change == CSmove)
	Point++;
}

static el_STATUS
ring_bell()
{
    TTYput('\07');
    TTYflush();
    return CSstay;
}

static el_STATUS
do_macro(unsigned char c)
{
    unsigned char		name[4];

    name[0] = '_';
    name[1] = c;
    name[2] = '_';
    name[3] = '\0';

    if ((Input = (unsigned char *)getenv((char *)name)) == NULL) {
	Input = NIL;
	return ring_bell();
    }
    return CSstay;
}

static el_STATUS
do_forward(el_STATUS move)
{
    int		i;
    unsigned char	*p;

    i = 0;
    do {
	p = &Line[Point];
	for ( ; Point < End && (*p == ' ' || !isalnum(*p)); Point++, p++)
	    if (move == CSmove)
		right(CSstay);

	for (; Point < End && isalnum(*p); Point++, p++)
	    if (move == CSmove)
		right(CSstay);

	if (Point == End)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

static el_STATUS
do_case(CASE type)
{
    int		i;
    int		end;
    int		count;
    unsigned char	*p;

    do_forward(CSstay);
    if (OldPoint != Point) {
	if ((count = Point - OldPoint) < 0)
	    count = -count;
	Point = OldPoint;
	if ((end = Point + count) > End)
	    end = End;
	for (i = Point, p = &Line[i]; i < end; i++, p++) {
	    if (type == TOupper) {
		if (islower(*p))
		    *p = toupper(*p);
	    }
	    else if (isupper(*p))
		*p = tolower(*p);
	    right(CSmove);
	}
    }
    return CSstay;
}

static el_STATUS
case_down_word()
{
    return do_case(TOlower);
}

static el_STATUS
case_up_word()
{
    return do_case(TOupper);
}

static void
ceol()
{
    int		extras;
    int		i;
    unsigned char	*p;

    for (extras = 0, i = Point, p = &Line[i]; i <= End; i++, p++) {
	TTYput(' ');
	if (ISCTL(*p)) {
	    TTYput(' ');
	    extras++;
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    TTYput(' ');
	    TTYput(' ');
	    extras += 2;
	}
    }

    for (i += extras; i > Point; i--)
	TTYback();
}

static void
clear_line()
{
    Point = -strlen(Prompt);
    TTYput('\r');
    ceol();
    Point = 0;
    End = 0;
    Line[0] = '\0';
}

static el_STATUS
insert_string(unsigned char *p)
{
    size_t	len;
    int		i;
    unsigned char	*new;
    unsigned char	*q;

    len = strlen((char *)p);
    if (End + len >= Length) {
	if ((new = malloc(sizeof(unsigned char) * (Length + len + MEM_INC))) == NULL)
	    return CSstay;
	if (Length) {
	    memcpy(new, Line, Length);
	    free(Line);
	}
	Line = new;
	Length += len + MEM_INC;
    }

    for (q = &Line[Point], i = End - Point; --i >= 0; )
	q[len + i] = q[i];
    memcpy(&Line[Point], p, len);
    End += len;
    Line[End] = '\0';
    TTYstring(&Line[Point]);
    Point += len;

    return Point == End ? CSstay : CSmove;
}


static unsigned char *
next_hist()
{
    return H.Pos >= H.Size - 1 ? NULL : H.Lines[++H.Pos];
}

static unsigned char *
prev_hist()
{
    return H.Pos == 0 ? NULL : H.Lines[--H.Pos];
}

static el_STATUS
do_insert_hist(unsigned char *p)
{
    if (p == NULL)
	return ring_bell();
    Point = 0;
    reposition();
    ceol();
    End = 0;
    return insert_string(p);
}

static el_STATUS
do_hist(unsigned char *(*move)())
{
    unsigned char	*p;
    int		i;

    i = 0;
    do {
	if ((p = (*move)()) == NULL)
	    return ring_bell();
    } while (++i < Repeat);
    return do_insert_hist(p);
}

static el_STATUS
h_next()
{
    return do_hist(next_hist);
}

static el_STATUS
h_prev()
{
    return do_hist(prev_hist);
}

static el_STATUS
h_first()
{
    return do_insert_hist(H.Lines[H.Pos = 0]);
}

static el_STATUS
h_last()
{
    return do_insert_hist(H.Lines[H.Pos = H.Size - 1]);
}

/*
**  Return zero if pat appears as a substring in text.
*/
static int
substrcmp(char *text, char *pat, int len)
{
    unsigned char	c;

    if ((c = *pat) == '\0')
        return *text == '\0';
    for ( ; *text; text++)
        if (*text == c && strncmp(text, pat, len) == 0)
            return 0;
    return 1;
}

static unsigned char *
search_hist(unsigned char *search, unsigned char *(*move)())
{
    static unsigned char	*old_search;
    int		len;
    int		pos;
    int		(*match)();
    char	*pat;

    /* Save or get remembered search pattern. */
    if (search && *search) {
	if (old_search)
	    free(old_search);
	old_search = (unsigned char *)strdup((char *)search);
    }
    else {
	if (old_search == NULL || *old_search == '\0')
            return NULL;
	search = old_search;
    }

    /* Set up pattern-finder. */
    if (*search == '^') {
	match = strncmp;
	pat = (char *)(search + 1);
    }
    else {
	match = substrcmp;
	pat = (char *)search;
    }
    len = strlen(pat);

    for (pos = H.Pos; (*move)() != NULL; )
	if ((*match)((char *)H.Lines[H.Pos], pat, len) == 0)
            return H.Lines[H.Pos];
    H.Pos = pos;
    return NULL;
}

static el_STATUS
h_search()
{
    static int	Searching;
    const char	*old_prompt;
    unsigned char	*(*move)();
    unsigned char	*p;

    if (Searching)
	return ring_bell();
    Searching = 1;

    clear_line();
    old_prompt = Prompt;
    Prompt = "Search: ";
    TTYputs(Prompt);
    move = Repeat == NO_ARG ? prev_hist : next_hist;
    p = search_hist(editinput(), move);
    clear_line();
    Prompt = old_prompt;
    TTYputs(Prompt);

    Searching = 0;
    return do_insert_hist(p);
}

static el_STATUS
fd_char()
{
    int		i;

    i = 0;
    do {
	if (Point >= End)
	    break;
	right(CSmove);
    } while (++i < Repeat);
    return CSstay;
}

static void
save_yank(int begin, int i)
{
    if (Yanked) {
	free(Yanked);
	Yanked = NULL;
    }

    if (i < 1)
	return;

    if ((Yanked = malloc(sizeof(unsigned char) * (i + 1))) != NULL) {
	memcpy(Yanked, &Line[begin], i);
	Yanked[i+1] = '\0';
    }
}

static el_STATUS
delete_string(int count)
{
    int		i;
    unsigned char	*p;

    if (count <= 0 || End == Point)
	return ring_bell();

    if (count == 1 && Point == End - 1) {
	/* Optimize common case of delete at end of line. */
	End--;
	p = &Line[Point];
	i = 1;
	TTYput(' ');
	if (ISCTL(*p)) {
	    i = 2;
	    TTYput(' ');
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    i = 3;
	    TTYput(' ');
	    TTYput(' ');
	}
	TTYbackn(i);
	*p = '\0';
	return CSmove;
    }
    if (Point + count > End && (count = End - Point) <= 0)
	return CSstay;

    if (count > 1)
	save_yank(Point, count);

    for (p = &Line[Point], i = End - (Point + count) + 1; --i >= 0; p++)
	p[0] = p[count];
    ceol();
    End -= count;
    TTYstring(&Line[Point]);
    return CSmove;
}

static el_STATUS
bk_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return CSstay;
}

static el_STATUS
bk_del_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return delete_string(i);
}

static el_STATUS
redisplay()
{
    TTYputs(NEWLINE);
    TTYputs(Prompt);
    TTYstring(Line);
    return CSmove;
}

static el_STATUS
kill_line()
{
    int		i;

    if (Repeat != NO_ARG) {
	if (Repeat < Point) {
	    i = Point;
	    Point = Repeat;
	    reposition();
	    delete_string(i - Point);
	}
	else if (Repeat > Point) {
	    right(CSmove);
	    delete_string(Repeat - Point - 1);
	}
	return CSmove;
    }

    save_yank(Point, End - Point);
    Line[Point] = '\0';
    ceol();
    End = Point;
    return CSstay;
}

static el_STATUS
insert_char(int c)
{
    el_STATUS	s;
    unsigned char	buff[2];
    unsigned char	*p;
    unsigned char	*q;
    int		i;

    if (Repeat == NO_ARG || Repeat < 2) {
	buff[0] = c;
	buff[1] = '\0';
	return insert_string(buff);
    }

    if ((p = malloc(Repeat + 1)) == NULL)
	return CSstay;
    for (i = Repeat, q = p; --i >= 0; )
	*q++ = c;
    *q = '\0';
    Repeat = 0;
    s = insert_string(p);
    free(p);
    return s;
}

static el_STATUS
meta()
{
    unsigned int	c;
    KEYMAP		*kp;

    if ((c = TTYget()) == EOF)
	return CSeof;
    /* Also include VT-100 arrows. */
    if (c == '[' || c == 'O')
	switch (c = TTYget()) {
	default:	return ring_bell();
	case EOF:	return CSeof;
	case 'A':	return h_prev();
	case 'B':	return h_next();
	case 'C':	return fd_char();
	case 'D':	return bk_char();
	}

    if (isdigit(c)) {
	for (Repeat = c - '0'; (c = TTYget()) != EOF && isdigit(c); )
	    Repeat = Repeat * 10 + c - '0';
	Pushed = 1;
	PushBack = c;
	return CSstay;
    }

    if (isupper(c))
	return do_macro(c);
    for (OldPoint = Point, kp = MetaMap; kp->Function; kp++)
	if (kp->Key == c)
	    return (*kp->Function)();

    return ring_bell();
}

static el_STATUS
emacs(unsigned int c)
{
    el_STATUS		s;
    KEYMAP		*kp;

    if (ISMETA(c)) {
	Pushed = 1;
	PushBack = UNMETA(c);
	return meta();
    }
    for (kp = Map; kp->Function; kp++)
	if (kp->Key == c)
	    break;
    s = kp->Function ? (*kp->Function)() : insert_char((int)c);
    if (!Pushed)
	/* No pushback means no repeat count; hacky, but true. */
	Repeat = NO_ARG;
    return s;
}

static el_STATUS
TTYspecial(unsigned int c)
{
    if (ISMETA(c))
	return CSdispatch;

    if (c == rl_erase || c == DEL)
	return bk_del_char();
    if (c == rl_kill) {
	if (Point != 0) {
	    Point = 0;
	    reposition();
	}
	Repeat = NO_ARG;
	return kill_line();
    }
    if (c == rl_intr || c == rl_quit) {
	Point = End = 0;
	Line[0] = '\0';
	return redisplay();
    }
    if (c == rl_eof && Point == 0 && End == 0)
	return CSeof;

    return CSdispatch;
}

static unsigned char *
editinput()
{
    unsigned int	c;

    Repeat = NO_ARG;
    OldPoint = Point = Mark = End = 0;
    Line[0] = '\0';

    while ((c = TTYget()) != EOF)
	switch (TTYspecial(c)) {
	case CSdone:
	    return Line;
	case CSeof:
	    return NULL;
	case CSmove:
	    reposition();
	    break;
	case CSdispatch:
	    switch (emacs(c)) {
	    case CSdone:
		return Line;
	    case CSeof:
		return NULL;
	    case CSmove:
		reposition();
		break;
	    case CSdispatch:
	    case CSstay:
		break;
	    }
	    break;
	case CSstay:
	    break;
	}
    return NULL;
}

static void
hist_add(unsigned char *p)
{
    int		i;

    if ((p = (unsigned char *)strdup((char *)p)) == NULL)
	return;
    if (H.Size < HIST_SIZE)
	H.Lines[H.Size++] = p;
    else {
	free(H.Lines[0]);
	for (i = 0; i < HIST_SIZE - 1; i++)
	    H.Lines[i] = H.Lines[i + 1];
	H.Lines[i] = p;
    }
    H.Pos = H.Size - 1;
}

/*
**  For compatibility with FSF readline.
*/
/* ARGSUSED0 */
void
rl_reset_terminal(char *p)
{
}

void
rl_initialize(void)
{
}

char *
readline(const char* prompt)
{
    unsigned char	*line;

    if (Line == NULL) {
	Length = MEM_INC;
	if ((Line = malloc(Length)) == NULL)
	    return NULL;
    }

    TTYinfo();
    rl_ttyset(0);
    hist_add(NIL);
    ScreenSize = SCREEN_INC;
    Screen = malloc(ScreenSize);
    Prompt = prompt ? prompt : (char *)NIL;
    TTYputs(Prompt);
    if ((line = editinput()) != NULL) {
	line = (unsigned char *)strdup((char *)line);
	TTYputs(NEWLINE);
	TTYflush();
    }
    rl_ttyset(1);
    free(Screen);
    free(H.Lines[--H.Size]);
    return (char *)line;
}

void
add_history(char *p)
{
    if (p == NULL || *p == '\0')
	return;

#if	defined(UNIQUE_HISTORY)
    if (H.Pos && strcmp(p, H.Lines[H.Pos - 1]) == 0)
        return;
#endif	/* defined(UNIQUE_HISTORY) */
    hist_add((unsigned char *)p);
}


static el_STATUS
beg_line()
{
    if (Point) {
	Point = 0;
	return CSmove;
    }
    return CSstay;
}

static el_STATUS
del_char()
{
    return delete_string(Repeat == NO_ARG ? 1 : Repeat);
}

static el_STATUS
end_line()
{
    if (Point != End) {
	Point = End;
	return CSmove;
    }
    return CSstay;
}

/*
**  Move back to the beginning of the current word and return an
**  allocated copy of it.
*/
static unsigned char *
find_word()
{
    static char	SEPS[] = "#;&|^$=`'{}()<>\n\t ";
    unsigned char	*p;
    unsigned char	*new;
    size_t	len;

    for (p = &Line[Point]; p > Line && strchr(SEPS, (char)p[-1]) == NULL; p--)
	continue;
    len = Point - (p - Line) + 1;
    if ((new = malloc(len)) == NULL)
	return NULL;
    memcpy(new, p, len);
    new[len - 1] = '\0';
    return new;
}

static el_STATUS
c_complete()
{
    unsigned char	*p;
    unsigned char	*word;
    int		unique;
    el_STATUS	s;

    word = find_word();
    p = (unsigned char *)rl_complete((char *)word, &unique);
    if (word)
	free(word);
    if (p && *p) {
	s = insert_string(p);
	if (!unique)
	    ring_bell();
	free(p);
	return s;
    }
    return ring_bell();
}

static el_STATUS
c_possible()
{
    unsigned char	**av;
    unsigned char	*word;
    int		ac;

    word = find_word();
    ac = rl_list_possib((char *)word, (char ***)&av);
    if (word)
	free(word);
    if (ac) {
	columns(ac, av);
	while (--ac >= 0)
	    free(av[ac]);
	free(av);
	return CSmove;
    }
    return ring_bell();
}

static el_STATUS
accept_line()
{
    Line[End] = '\0';
    return CSdone;
}

static el_STATUS
transpose()
{
    unsigned char	c;

    if (Point) {
	if (Point == End)
	    left(CSmove);
	c = Line[Point - 1];
	left(CSstay);
	Line[Point - 1] = Line[Point];
	TTYshow(Line[Point - 1]);
	Line[Point++] = c;
	TTYshow(c);
    }
    return CSstay;
}

static el_STATUS
quote()
{
    unsigned int	c;

    return (c = TTYget()) == EOF ? CSeof : insert_char((int)c);
}

static el_STATUS
wipe()
{
    int		i;

    if (Mark > End)
	return ring_bell();

    if (Point > Mark) {
	i = Point;
	Point = Mark;
	Mark = i;
	reposition();
    }

    return delete_string(Mark - Point);
}

static el_STATUS
mk_set()
{
    Mark = Point;
    return CSstay;
}

static el_STATUS
exchange()
{
    unsigned int	c;

    if ((c = TTYget()) != CTL('X'))
	return c == EOF ? CSeof : ring_bell();

    if ((c = Mark) <= End) {
	Mark = Point;
	Point = c;
	return CSmove;
    }
    return CSstay;
}

static el_STATUS
yank()
{
    if (Yanked && *Yanked)
	return insert_string(Yanked);
    return CSstay;
}

static el_STATUS
copy_region()
{
    if (Mark > End)
	return ring_bell();

    if (Point > Mark)
	save_yank(Mark, Point - Mark);
    else
	save_yank(Point, Mark - Point);

    return CSstay;
}

static el_STATUS
move_to_char()
{
    unsigned int	c;
    int			i;
    unsigned char		*p;

    if ((c = TTYget()) == EOF)
	return CSeof;
    for (i = Point + 1, p = &Line[i]; i < End; i++, p++)
	if (*p == c) {
	    Point = i;
	    return CSmove;
	}
    return CSstay;
}

static el_STATUS
fd_word()
{
    return do_forward(CSmove);
}

static el_STATUS
fd_kill_word()
{
    int		i;

    do_forward(CSstay);
    if (OldPoint != Point) {
	i = Point - OldPoint;
	Point = OldPoint;
	return delete_string(i);
    }
    return CSstay;
}

static el_STATUS
bk_word()
{
    int		i;
    unsigned char	*p;

    i = 0;
    do {
	for (p = &Line[Point]; p > Line && !isalnum(p[-1]); p--)
	    left(CSmove);

	for (; p > Line && p[-1] != ' ' && isalnum(p[-1]); p--)
	    left(CSmove);

	if (Point == 0)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

static el_STATUS
bk_kill_word()
{
    bk_word();
    if (OldPoint != Point)
	return delete_string(OldPoint - Point);
    return CSstay;
}

static int
argify(unsigned char *line, unsigned char ***avp)
{
    unsigned char	*c;
    unsigned char	**p;
    unsigned char	**new;
    int		ac;
    int		i;

    i = MEM_INC;
    if ((*avp = p = malloc(sizeof(unsigned char*) * i))== NULL)
	 return 0;

    for (c = line; isspace(*c); c++)
	continue;
    if (*c == '\n' || *c == '\0')
	return 0;

    for (ac = 0, p[ac++] = c; *c && *c != '\n'; ) {
	if (isspace(*c)) {
	    *c++ = '\0';
	    if (*c && *c != '\n') {
		if (ac + 1 == i) {
		    new = malloc(sizeof(unsigned char*) * (i + MEM_INC));
		    if (new == NULL) {
			p[ac] = NULL;
			return ac;
		    }
		    memcpy(new, p, i * sizeof (char **));
		    i += MEM_INC;
		    free(p);
		    *avp = p = new;
		}
		p[ac++] = c;
	    }
	}
	else
	    c++;
    }
    *c = '\0';
    p[ac] = NULL;
    return ac;
}

static el_STATUS
last_argument()
{
    unsigned char	**av;
    unsigned char	*p;
    el_STATUS	s;
    int		ac;

    if (H.Size == 1 || (p = H.Lines[H.Size - 2]) == NULL)
	return ring_bell();

    if ((p = (unsigned char *)strdup((char *)p)) == NULL)
	return CSstay;
    ac = argify(p, &av);

    if (Repeat != NO_ARG)
	s = Repeat < ac ? insert_string(av[Repeat]) : ring_bell();
    else
	s = ac ? insert_string(av[ac - 1]) : CSstay;

    if (ac)
	free(av);
    free(p);
    return s;
}

static KEYMAP	Map[33] = {
    {	CTL('@'),	ring_bell	},
    {	CTL('A'),	beg_line	},
    {	CTL('B'),	bk_char		},
    {	CTL('D'),	del_char	},
    {	CTL('E'),	end_line	},
    {	CTL('F'),	fd_char		},
    {	CTL('G'),	ring_bell	},
    {	CTL('H'),	bk_del_char	},
    {	CTL('I'),	c_complete	},
    {	CTL('J'),	accept_line	},
    {	CTL('K'),	kill_line	},
    {	CTL('L'),	redisplay	},
    {	CTL('M'),	accept_line	},
    {	CTL('N'),	h_next		},
    {	CTL('O'),	ring_bell	},
    {	CTL('P'),	h_prev		},
    {	CTL('Q'),	ring_bell	},
    {	CTL('R'),	h_search	},
    {	CTL('S'),	ring_bell	},
    {	CTL('T'),	transpose	},
    {	CTL('U'),	ring_bell	},
    {	CTL('V'),	quote		},
    {	CTL('W'),	wipe		},
    {	CTL('X'),	exchange	},
    {	CTL('Y'),	yank		},
    {	CTL('Z'),	ring_bell	},
    {	CTL('['),	meta		},
    {	CTL(']'),	move_to_char	},
    {	CTL('^'),	ring_bell	},
    {	CTL('_'),	ring_bell	},
    {	0,		NULL		}
};

static KEYMAP	MetaMap[16]= {
    {	CTL('H'),	bk_kill_word	},
    {	DEL,		bk_kill_word	},
    {	' ',		mk_set	},
    {	'.',		last_argument	},
    {	'<',		h_first		},
    {	'>',		h_last		},
    {	'?',		c_possible	},
    {	'b',		bk_word		},
    {	'd',		fd_kill_word	},
    {	'f',		fd_word		},
    {	'l',		case_down_word	},
    {	'u',		case_up_word	},
    {	'y',		yank		},
    {	'w',		copy_region	},
    {	0,		NULL		}
};
