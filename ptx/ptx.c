/*	from Unix 7th Edition /usr/src/cmd/ptx.c	*/
/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, November 2005.
 */
/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   Redistributions of source code and documentation must retain the
 *    above copyright notice, this list of conditions and the following
 *    disclaimer.
 *   Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *   All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed or owned by Caldera
 *      International, Inc.
 *   Neither the name of Caldera International, Inc. nor the names of
 *    other contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucb/ptx.sl	1.5 (gritter) 11/6/05";

/*	permuted title index
	ptx [-t] [-i ignore] [-o only] [-w num] [-f] [input] [output]
	Ptx reads the input file and permutes on words in it.
	It excludes all words in the ignore file.
	Alternately it includes words in the only file.
	if neither is given it excludes the words in /usr/lib/eign.

	The width of the output line can be changed to num
	characters.  If omitted 72 is default unless troff than 100.
	the -f flag tells the program to fold the output
	the -t flag says the output is for troff and the
	output is then wider.

	make: cc ptx.c -lS
	*/

#include <stdio.h>
#include <ctype.h>
#ifdef	EUC
#include <wchar.h>
#include <wctype.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <locale.h>
#include <limits.h>
#define DEFLTX LIBDIR "/eign"
#define TILDE 0177
#define SORT "sort"
#define	N 30
#define	MAX	N*BUFSIZ
#define MAXT	2048
#define MASK	03777
#define SET	1

#define isabreak(c) (btable[c])

#ifdef	__GLIBC__
#ifdef	_IO_getc_unlocked
#undef	getc
#define	getc(f)		_IO_getc_unlocked(f)
#endif
#ifdef	_IO_putc_unlocked
#undef	putc
#define	putc(c, f)	_IO_putc_unlocked(c, f)
#endif
#endif

#define	getline		xxgetline

static int status;

#ifdef	EUC
#define	NCHARS	0x110000
#else	/* !EUC */
#define	NCHARS	256
#define	iswupper	isupper
#define	towlower	tolower
#define	iswspace	isspace
#define	wchar_t	unsigned char
#endif	/* !EUC */

static const wchar_t *hasht[MAXT];
static wchar_t *line;
static wchar_t btable[NCHARS];
static int ignore;
static int only;
static int llen = 72;
static int gap = 3;
static int gutter = 3;
static int mlen;
static int wlen;
static int rflag;
static int halflen;
static wchar_t *strtbufp, *endbufp;
static char *empty = "";

static char *infile;
static FILE *inptr /*= stdin*/;

static char *outfile;
static FILE *outptr /*= stdout*/;

static char *sortfile;	/* output of sort program */
static char nofold[] = {'-', 'd', 't', TILDE, 0};
static char fold[] = {'-', 'd', 'f', 't', TILDE, 0};
static char *sortopt = nofold;
static FILE *sortptr;

static char *bfile;	/*contains user supplied break chars */
static FILE *bptr;

static void msg(const char *, const char *);
static void diag(const char *, const char *);
static wchar_t *getline(void);
static void cmpline(const wchar_t *);
static int cmpword(const wchar_t *, const wchar_t *, const wchar_t *);
static void putline(const wchar_t *, const wchar_t *);
static void getsort(void);
static wchar_t *rtrim(const wchar_t *, const wchar_t *, int);
static wchar_t *ltrim(const wchar_t *, const wchar_t *, int);
static void putout(const wchar_t *, const wchar_t *);
static void onintr(int);
static int hash(const wchar_t *, const wchar_t *);
static int storeh(int, const wchar_t *);


#ifdef	EUC
static wint_t	peekc = WEOF;

static wint_t
GETC(FILE *fp)
{
	char	mb[MB_LEN_MAX+1];
	wchar_t	wc;
	int	c, i, n;
	mbstate_t	state;

	if (peekc != WEOF) {
		wc = peekc;
		peekc = WEOF;
		return wc;
	}
bad:	if ((c = getc(fp)) == EOF)
		return WEOF;
	if (c & 0200) {
		i = 0;
		for (;;) {
			mb[i++] = c;
			mb[i] = 0;
			memset(&state, 0, sizeof state);
			if ((n = mbrtowc(&wc, mb, i, &state)) == (size_t)-1)
				goto bad;
			if (n == (size_t)-2) {
				if ((c = getc(fp)) == EOF)
					return WEOF;
				continue;
			}
			if (wc >= NCHARS)
				goto bad;
			return wc;
		}
	}
	return c;
}

static void
UNGETC(int c, FILE *fp)
{
	peekc = c;
}

static int
PUTC(int c, FILE *fp)
{
	char	mb[MB_LEN_MAX];
	int	i, n;

	if ((n = wctomb(mb, c)) > 0) {
		for (i = 0; i < n; i++)
			putc(mb[i]&0377, fp);
		return c;
	} else if (n == 0) {
		putc(0, fp);
		return 0;
	} else
		return EOF;
}

#define	L	"l"

#else	/* !EUC */
#define	GETC(f)		getc(f)
#define	UNGETC(c, f)	ungetc(c, f)
#define	PUTC(c, f)	putc(c, f)
#define	L
#endif	/* !EUC */
int
main(int argc,char **argv)
{
	char template[] = "/tmp/ptxsXXXXXX";
	register int c;
	register wchar_t *bufp;
	int pid;
	wchar_t *pend;

	char *xfile;
	FILE *xptr;

	setlocale(LC_CTYPE, "");
	inptr = stdin;
	outptr = stdout;
	if(signal(SIGHUP,onintr)==SIG_IGN)
		signal(SIGHUP,SIG_IGN);
	if(signal(SIGINT,onintr)==SIG_IGN)
		signal(SIGINT,SIG_IGN);
	signal(SIGPIPE,onintr);
	signal(SIGTERM,onintr);

/*	argument decoding	*/

	xfile = DEFLTX;
	argv++;
	while(argc>1 && **argv == '-') {
		switch (*++*argv){

		case 'r':
			rflag++;
			break;
		case 'f':
			sortopt = fold;
			break;

		case 'w':
			if(argc >= 2) {
				argc--;
				wlen++;
				llen = atoi(*++argv);
				if(llen == 0)
					diag("Wrong width:",*argv);
				break;
			}

		case 't':
			if(wlen == 0)
				llen = 100;
			break;
		case 'g':
			if(argc >=2) {
				argc--;
				gap = gutter = atoi(*++argv);
			}
			break;

		case 'i':
			if(only) 
				diag("Only file already given.",empty);
			if (argc>=2){
				argc--;
				ignore++;
				xfile = *++argv;
			}
			break;

		case 'o':
			if(ignore)
				diag("Ignore file already given",empty);
			if (argc>=2){
				only++;
				argc--;
				xfile = *++argv;
			}
			break;

		case 'b':
			if(argc>=2) {
				argc--;
				bfile = *++argv;
			}
			break;

		default:
			msg("Illegal argument:",*argv);
		}
		argc--;
		argv++;
	}

	if(argc>3)
		diag("Too many filenames",empty);
	else if(argc==3){
		infile = *argv++;
		outfile = *argv;
		if((outptr = fopen(outfile,"w")) == NULL)
			diag("Cannot open output file:",outfile);
	} else if(argc==2) {
		infile = *argv;
		outfile = 0;
	}


	/* Default breaks of blank, tab and newline */
	btable[' '] = SET;
	btable['\t'] = SET;
	btable['\n'] = SET;
	if(bfile) {
		if((bptr = fopen(bfile,"r")) == NULL)
			diag("Cannot open break char file",bfile);

		while((c = GETC(bptr)) != EOF)
			btable[c] = SET;
	}

/*	Allocate space for a buffer.  If only or ignore file present
	read it into buffer. Else read in default ignore file
	and put resulting words in buffer.
	*/


	if((strtbufp = calloc(N,BUFSIZ)) == NULL)
		diag("Out of memory space",empty);
	bufp = strtbufp;
	endbufp = strtbufp+MAX;

	if((xptr = fopen(xfile,"r")) == NULL)
		diag("Cannot open  file",xfile);

	while(bufp < endbufp && (c = GETC(xptr)) != EOF) {
		if(isabreak(c)) {
			if(storeh(hash(strtbufp,bufp),strtbufp))
				diag("Too many words",xfile);
			*bufp++ = '\0';
			strtbufp = bufp;
		}
		else {
			*bufp++ = (iswupper(c)?towlower(c):c);
		}
	}
	if (bufp >= endbufp)
		diag("Too many words in file",xfile);
	endbufp = --bufp;

	/* open output file for sorting */

	close(mkstemp(template));
	sortfile = template;
	if((sortptr = fopen(sortfile, "w")) == NULL)
		diag("Cannot open output for sorting:",sortfile);

/*	get a line of data and compare each word for
	inclusion or exclusion in the sort phase
*/

	if (infile!=0 && (inptr = fopen(infile,"r")) == NULL)
		diag("Cannot open data: ",infile);
	while((pend=getline()))
		cmpline(pend);
	fclose(sortptr);

	switch (pid = fork()){

	case -1:	/* cannot fork */
		diag("Cannot fork",empty);

	case 0:		/* child */
		execlp(SORT, SORT, sortopt, "+0", "-1", "+1",
			sortfile, "-o", sortfile, NULL);

	default:	/* parent */
		while(wait(&status) != pid);
	}


	getsort();
	onintr(0);
	/*NOTREACHED*/
	return 0;
}

static void
msg(const char *s,const char *arg)
{
	fprintf(stderr,"%s %s\n",s,arg);
	return;
}
static void
diag(const char *s,const char *arg)
{

	msg(s,arg);
	exit(1);
}


static wchar_t *
getline(void)
{

	register int c;
	register int i = 0;


	if (line == NULL)
		line = calloc(1, mlen = 1);
	/* Throw away leading white space */

	while(iswspace(c=GETC(inptr)))
		;
	if(c==EOF)
		return(0);
	UNGETC(c,inptr);
	while(( c=GETC(inptr)) != EOF) {
		switch (c) {

			case '\n':
				while(iswspace(line[--i]));
				line[++i] = '\n';
				return(&line[i]);
			case '\t':
				c = ' ';
				/*FALLTHRU*/
				break;
			default:
				if (i+1 >= mlen)
					line = realloc(line, mlen += 200);
				line[i++] = c;
		}
	}
	return(0);
}

static void
cmpline(const wchar_t *pend)
{

	const wchar_t *pstrt, *pchar, *cp;
	const wchar_t **hp;
	int flag;

	pchar = line;
	if(rflag)
		while(pchar<pend&&!iswspace(*pchar))
			pchar++;
	while(pchar<pend){
	/* eliminate white space */
		if(isabreak(*pchar++))
			continue;
		pstrt = --pchar;

		flag = 1;
		while(flag){
			if(isabreak(*pchar)) {
				hp = &hasht[hash(pstrt,pchar)];
				pchar--;
				while((cp = *hp++)){
					if(hp == &hasht[MAXT])
						hp = hasht;
	/* possible match */
					if(cmpword(pstrt,pchar,cp)){
	/* exact match */
						if(!ignore && only)
							putline(pstrt,pend);
						flag = 0;
						break;
					}
				}
	/* no match */
				if(flag){
					if(ignore || !only)
						putline(pstrt,pend);
					flag = 0;
				}
			}
		pchar++;
		}
	}
}

static int
cmpword(const wchar_t *cpp,const wchar_t *pend,const wchar_t *hpp)
{
	int c;

	while(*hpp != '\0'){
		c = *cpp++;
		if((iswupper(c)?towlower(c):c) != *hpp++)
			return(0);
	}
	if(--cpp == pend) return(1);
	return(0);
}

static void
putline(const wchar_t *strt, const wchar_t *end)
{
	const wchar_t *cp;

	for(cp=strt; cp<end; cp++)
		PUTC(*cp, sortptr);
	/* Add extra blank before TILDE to sort correctly
	   with -fd option */
	putc(' ',sortptr);
	putc(TILDE,sortptr);
	for (cp=line; cp<strt; cp++)
		PUTC(*cp,sortptr);
	putc('\n',sortptr);
}

static void
getsort(void)
{
	register int c;
	register wchar_t *tilde = NULL, *linep, *ref;
	wchar_t *p1a,*p1b,*p2a,*p2b,*p3a,*p3b,*p4a,*p4b;
	int w;

	if((sortptr = fopen(sortfile,"r")) == NULL)
		diag("Cannot open sorted data:",sortfile);

	halflen = (llen-gutter)/2;
	linep = line;
	while((c = GETC(sortptr)) != EOF) {
		switch(c) {

		case TILDE:
			tilde = linep;
			break;

		case '\n':
			while(iswspace(linep[-1]))
				linep--;
			ref = tilde;
			if(rflag) {
				while(ref<linep&&!iswspace(*ref))
					ref++;
				*ref++ = 0;
			}
		/* the -1 is an overly conservative test to leave
		   space for the / that signifies truncation*/
			p3b = rtrim(p3a=line,tilde,halflen-1);
			if(p3b-p3a>halflen-1)
				p3b = p3a+halflen-1;
			p2a = ltrim(ref,p2b=linep,halflen-1);
			if(p2b-p2a>halflen-1)
				p2a = p2b-halflen-1;
			p1b = rtrim(p1a=p3b+(iswspace(p3b[0])!=0),tilde,
				w=halflen-(p2b-p2a)-gap);
			if(p1b-p1a>w)
				p1b = p1a;
			p4a = ltrim(ref,p4b=p2a-(iswspace(p2a[-1])!=0),
				w=halflen-(p3b-p3a)-gap);
			if(p4b-p4a>w)
				p4a = p4b;
			fprintf(outptr,".xx \"");
			putout(p1a,p1b);
	/* tilde-1 to account for extra space before TILDE */
			if(p1b!=(tilde-1) && p1a!=p1b)
				fprintf(outptr,"/");
			fprintf(outptr,"\" \"");
			if(p4a==p4b && p2a!=ref && p2a!=p2b)
				fprintf(outptr,"/");
			putout(p2a,p2b);
			fprintf(outptr,"\" \"");
			putout(p3a,p3b);
	/* ++p3b to account for extra blank after TILDE */
	/* ++p3b to account for extra space before TILDE */
			if(p1a==p1b && ++p3b!=tilde)
				fprintf(outptr,"/");
			fprintf(outptr,"\" \"");
			if(p1a==p1b && p4a!=ref && p4a!=p4b)
				fprintf(outptr,"/");
			putout(p4a,p4b);
			if(rflag)
				fprintf(outptr,"\" %" L "s\n",tilde);
			else
				fprintf(outptr,"\"\n");
			linep = line;
			break;

		case '"':
	/* put double " for "  */
			*linep++ = c;
		default:
			*linep++ = c;
		}
	}
}

static wchar_t *
rtrim(const wchar_t *a,const wchar_t *c,int d)
{
	const wchar_t *b,*x;
	b = c;
	for(x=a+1; x<=c&&x-a<=d; x++)
		if((x==c||iswspace(x[0]))&&!isspace(x[-1]))
			b = x;
	if(b<c&&!iswspace(b[0]))
		b++;
	return((wchar_t *)b);
}

static wchar_t *
ltrim(const wchar_t *c,const wchar_t *b,int d)
{
	const wchar_t *a,*x;
	a = c;
	for(x=b-1; x>=c&&b-x<=d; x--)
		if(!iswspace(x[0])&&(x==c||isspace(x[-1])))
			a = x;
	if(a>c&&!iswspace(a[-1]))
		a--;
	return((wchar_t *)a);
}

static void
putout(const wchar_t *strt,const wchar_t *end)
{
	const wchar_t *cp;

	cp = strt;

	for(cp=strt; cp<end; cp++) {
		PUTC(*cp,outptr);
	}
}

static void
onintr(int st)
{

	if(*sortfile)
		unlink(sortfile);
	exit(st);
}

static int
hash(const wchar_t *strtp,const wchar_t *endp)
{
	const wchar_t *cp;
	int c, i, j, k;

	/* Return zero hash number for single letter words */
	if((endp - strtp) == 1)
		return(0);

	cp = strtp;
	c = *cp++;
	i = (iswupper(c)?towlower(c):c);
	c = *cp;
	j = (iswupper(c)?towlower(c):c);
	i = i*j;
	cp = --endp;
	c = *cp--;
	k = (iswupper(c)?towlower(c):c);
	c = *cp;
	j = (iswupper(c)?towlower(c):c);
	j = k*j;

	k = (i ^ (j>>2)) & MASK;
	return(k);
}

static int
storeh(int num,const wchar_t *strtp)
{
	int i;

	for(i=num; i<MAXT; i++) {
		if(hasht[i] == 0) {
			hasht[i] = strtp;
			return(0);
		}
	}
	for(i=0; i<num; i++) {
		if(hasht[i] == 0) {
			hasht[i] = strtp;
			return(0);
		}
	}
	return(1);
}
