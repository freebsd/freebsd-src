/*
Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)

Permission to use, copy, modify, and distribute this material 
for any purpose and without fee is hereby granted, provided 
that the above copyright notice and this permission notice 
appear in all copies, and that the name of Bellcore not be 
used in advertising or publicity pertaining to this 
material without the specific, prior written permission 
of an authorized representative of Bellcore.  BELLCORE 
MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY 
OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", 
WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
*/
#include <stdio.h>
#include <ctype.h>
#include <config.h>

extern char *index();
static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

#define char64(c)  (((c) < 0 || (c) > 127) ? -1 : index_64[(c)])

/*
char64(c)
char c;
{
    char *s = (char *) index(basis_64, c);
    if (s) return(s-basis_64);
    return(-1);
}
*/

/* the following gets a character, but fakes it properly into two chars if there's a newline character */
static int InNewline=0;

int nextcharin(infile, PortableNewlines)
FILE *infile;
int PortableNewlines;
{
    int c;

#ifndef NEWLINE_CHAR
    return(getc(infile));
#else
    if (!PortableNewlines) return(getc(infile));
    if (InNewline) {
        InNewline = 0;
        return(10); /* LF */
    }
    c = getc(infile);
    if (c == NEWLINE_CHAR) {
        InNewline = 1;
        return(13); /* CR */
    }
    return(c);
#endif
}

to64(infile, outfile, PortableNewlines) 
FILE *infile, *outfile;
int PortableNewlines;
{
    int c1, c2, c3, ct=0;
    InNewline = 0; /* always reset it */
    while ((c1 = nextcharin(infile, PortableNewlines)) != EOF) {
        c2 = nextcharin(infile, PortableNewlines);
        if (c2 == EOF) {
            output64chunk(c1, 0, 0, 2, outfile);
        } else {
            c3 = nextcharin(infile, PortableNewlines);
            if (c3 == EOF) {
                output64chunk(c1, c2, 0, 1, outfile);
            } else {
                output64chunk(c1, c2, c3, 0, outfile);
            }
        }
        ct += 4;
        if (ct > 71) {
            putc('\n', outfile);
            ct = 0;
        }
    }
    if (ct) putc('\n', outfile);
    fflush(outfile);
}

output64chunk(c1, c2, c3, pads, outfile)
FILE *outfile;
{
    putc(basis_64[c1>>2], outfile);
    putc(basis_64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)], outfile);
    if (pads == 2) {
        putc('=', outfile);
        putc('=', outfile);
    } else if (pads) {
        putc(basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)], outfile);
        putc('=', outfile);
    } else {
        putc(basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)], outfile);
        putc(basis_64[c3 & 0x3F], outfile);
    }
}

PendingBoundary(s, Boundaries, BoundaryCt)
char *s;
char **Boundaries;
int *BoundaryCt;
{
    int i, len;

    if (s[0] != '-' || s[1] != '-') return(0);


    for (i=0; i < *BoundaryCt; ++i) {
	len = strlen(Boundaries[i]);
        if (!strncmp(s, Boundaries[i], len)) {
            if (s[len] == '-' && s[len+1] == '-') *BoundaryCt = i;
            return(1);
        }
    }
    return(0);
}

/* If we're in portable newline mode, we have to convert CRLF to the 
    local newline convention on output */

static int CRpending = 0;

#ifdef NEWLINE_CHAR
almostputc(c, outfile, PortableNewlines)
int c;
FILE *outfile;
int PortableNewlines;
{
    if (CRpending) {
        if (c == 10) {
            putc(NEWLINE_CHAR, outfile);
            CRpending = 0;
        } else {
            putc(13, outfile);
	    if (c != 13) {
            	putc(c, outfile);
		CRpending = 0;
	    }
        }
    } else {
        if (PortableNewlines && c == 13) {
            CRpending = 1;
        } else {
            putc(c, outfile);
        }
    }
}
#else
almostputc(c, outfile, PortableNewlines)
int c;
FILE *outfile;
int PortableNewlines;
{
    putc(c, outfile);
}
#endif

from64(infile, outfile, boundaries, boundaryct, PortableNewlines) 
FILE *infile, *outfile;
char **boundaries;
int *boundaryct;
int PortableNewlines;
{
    int c1, c2, c3, c4;
    int newline = 1, DataDone = 0;

    /* always reinitialize */
    CRpending = 0;
    while ((c1 = getc(infile)) != EOF) {
        if (isspace(c1)) {
            if (c1 == '\n') {
                newline = 1;
            } else {
                newline = 0;
            }
            continue;
        }
        if (newline && boundaries && c1 == '-') {
            char Buf[200];
            /* a dash is NOT base 64, so all bets are off if NOT a boundary */
            ungetc(c1, infile);
            fgets(Buf, sizeof(Buf), infile);
            if (boundaries
                 && (Buf[0] == '-')
                 && (Buf[1] == '-')
                 && PendingBoundary(Buf, boundaries, boundaryct)) {
                return;
            }
            fprintf(stderr, "Ignoring unrecognized boundary line: %s\n", Buf);
            continue;
        }
        if (DataDone) continue;
        newline = 0;
        do {
            c2 = getc(infile);
        } while (c2 != EOF && isspace(c2));
        do {
            c3 = getc(infile);
        } while (c3 != EOF && isspace(c3));
        do {
            c4 = getc(infile);
        } while (c4 != EOF && isspace(c4));
        if (c2 == EOF || c3 == EOF || c4 == EOF) {
            fprintf(stderr, "Warning: base64 decoder saw premature EOF!\n");
            return;
        }
        if (c1 == '=' || c2 == '=') {
            DataDone=1;
            continue;
        }
        c1 = char64(c1);
        c2 = char64(c2);
        almostputc(((c1<<2) | ((c2&0x30)>>4)), outfile, PortableNewlines);
        if (c3 == '=') {
            DataDone = 1;
        } else {
            c3 = char64(c3);
            almostputc((((c2&0XF) << 4) | ((c3&0x3C) >> 2)), outfile, PortableNewlines);
            if (c4 == '=') {
                DataDone = 1;
            } else {
                c4 = char64(c4);
                almostputc((((c3&0x03) <<6) | c4), outfile, PortableNewlines);
            }
        }
    }
    if (CRpending) putc(13, outfile); /* Don't drop a lone trailing char 13 */
}

static char basis_hex[] = "0123456789ABCDEF";
static char index_hex[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
    -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

/* The following version generated complaints on Solaris. */
/* #define hexchar(c)  (((c) < 0 || (c) > 127) ? -1 : index_hex[(c)])  */
/*  Since we're no longer ever calling it with anything signed, this should work: */
#define hexchar(c)  (((c) > 127) ? -1 : index_hex[(c)])

/*
hexchar(c)
char c;
{
    char *s;
    if (islower(c)) c = toupper(c);
    s = (char *) index(basis_hex, c);
    if (s) return(s-basis_hex);
    return(-1);
}
*/

toqp(infile, outfile) 
FILE *infile, *outfile;
{
    int c, ct=0, prevc=255;
    while ((c = getc(infile)) != EOF) {
        if ((c < 32 && (c != '\n' && c != '\t'))
             || (c == '=')
             || (c >= 127)
             /* Following line is to avoid single periods alone on lines,
               which messes up some dumb smtp implementations, sigh... */
             || (ct == 0 && c == '.')) {
            putc('=', outfile);
            putc(basis_hex[c>>4], outfile);
            putc(basis_hex[c&0xF], outfile);
            ct += 3;
            prevc = 'A'; /* close enough */
        } else if (c == '\n') {
            if (prevc == ' ' || prevc == '\t') {
                putc('=', outfile); /* soft & hard lines */
                putc(c, outfile);
            }
            putc(c, outfile);
            ct = 0;
            prevc = c;
        } else {
            if (c == 'F' && prevc == '\n') {
                /* HORRIBLE but clever hack suggested by MTR for sendmail-avoidance */
                c = getc(infile);
                if (c == 'r') {
                    c = getc(infile);
                    if (c == 'o') {
                        c = getc(infile);
                        if (c == 'm') {
                            c = getc(infile);
                            if (c == ' ') {
                                /* This is the case we are looking for */
                                fputs("=46rom", outfile);
                                ct += 6;
                            } else {
                                fputs("From", outfile);
                                ct += 4;
                            }
                        } else {
                            fputs("Fro", outfile);
                            ct += 3;
                        }
                    } else {
                        fputs("Fr", outfile);
                        ct += 2;
                    }
                } else {
                    putc('F', outfile);
                    ++ct;
                }
                ungetc(c, infile);
                prevc = 'x'; /* close enough -- printable */
            } else { /* END horrible hack */
                putc(c, outfile);
                ++ct;
                prevc = c;
            }
        }
        if (ct > 72) {
            putc('=', outfile);
            putc('\n', outfile);
            ct = 0;
            prevc = '\n';
        }
    }
    if (ct) {
        putc('=', outfile);
        putc('\n', outfile);
    }
}

fromqp(infile, outfile, boundaries, boundaryct) 
FILE *infile, *outfile;
char **boundaries;
int *boundaryct;
{
    unsigned int c1, c2;
    int sawnewline = 1, neednewline = 0;
    /* The neednewline hack is necessary because the newline leading into 
      a multipart boundary is part of the boundary, not the data */

    while ((c1 = getc(infile)) != EOF) {
        if (sawnewline && boundaries && (c1 == '-')) {
            char Buf[200];
            unsigned char *s;

            ungetc(c1, infile);
            fgets(Buf, sizeof(Buf), infile);
            if (boundaries
                 && (Buf[0] == '-')
                 && (Buf[1] == '-')
                 && PendingBoundary(Buf, boundaries, boundaryct)) {
                return;
            }
            /* Not a boundary, now we must treat THIS line as q-p, sigh */
            if (neednewline) {
                putc('\n', outfile);
                neednewline = 0;
            }
            for (s=(unsigned char *) Buf; *s; ++s) {
                if (*s == '=') {
                    if (!*++s) break;
                    if (*s == '\n') {
                        /* ignore it */
                        sawnewline = 1;
                    } else {
                        c1 = hexchar(*s);
                        if (!*++s) break;
                        c2 = hexchar(*s);
                        putc(c1<<4 | c2, outfile);
                    }
                } else {
#ifdef MSDOS
                    if (*s == '\n')
                        putc('\r', outfile);	/* insert CR for binary-mode write */
#endif
                    putc(*s, outfile);
                }
            }
        } else {
            if (neednewline) {
                putc('\n', outfile);
                neednewline = 0;
            }
            if (c1 == '=') {
                sawnewline = 0;
                c1 = getc(infile);
                if (c1 == '\n') {
                    /* ignore it */
                    sawnewline = 1;
                } else {
                    c2 = getc(infile);
                    c1 = hexchar(c1);
                    c2 = hexchar(c2);
                    putc(c1<<4 | c2, outfile);
                    if (c2 == '\n') sawnewline = 1;
                }
            } else {
                if (c1 == '\n') {
                    sawnewline = 1;
                    neednewline = 1;
                } else {
                    sawnewline = 0;
                    putc(c1, outfile);
                }
            }
        }
    }
    if (neednewline) {
        putc('\n', outfile);
        neednewline = 0;
    }    
}

/*
Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)

Permission to use, copy, modify, and distribute this material 
for any purpose and without fee is hereby granted, provided 
that the above copyright notice and this permission notice 
appear in all copies, and that the name of Bellcore not be 
used in advertising or publicity pertaining to this 
material without the specific, prior written permission 
of an authorized representative of Bellcore.  BELLCORE 
MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY 
OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", 
WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
*/
#include <stdio.h>
#include <config.h>
#ifdef MSDOS
#include <fcntl.h>
#endif

#define BASE64 1
#define QP 2 /* quoted-printable */

main(argc, argv)
int argc;
char **argv;
{
    int encode = 1, which = BASE64, i, portablenewlines = 0;
    FILE *fp = stdin;
    FILE *fpo = stdout;

    for (i=1; i<argc; ++i) {
        if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
		case 'o':
		    if (++i >= argc) {
			fprintf(stderr, "mimencode: -o requires a file name.\n");
			exit(-1);
		    }
		    fpo = fopen(argv[i], "w");
		    if (!fpo) {
			perror(argv[i]);
			exit(-1);
		    }
		    break;
                case 'u':
                    encode = 0;
                    break;
                case 'q':
                    which = QP;
                    break;
                case 'p':
                    portablenewlines = 1;
                    break;
                case 'b':
                    which = BASE64;
                    break;
		default:
                    fprintf(stderr,
                       "Usage: mmencode [-u] [-q] [-b] [-p] [-o outputfile] [file name]\n");
                    exit(-1);
            }
        } else {
#ifdef MSDOS
            if (encode)
                fp = fopen(argv[i], "rb");
            else
            {
                fp = fopen(argv[i], "rt");
                setmode(fileno(fpo), O_BINARY);
            } /* else */
#else
            fp = fopen(argv[i], "r");
#endif /* MSDOS */
            if (!fp) {
                perror(argv[i]);
                exit(-1);
            }
        }
    }
#ifdef MSDOS
    if (fp == stdin) setmode(fileno(fp), O_BINARY);
#endif /* MSDOS */
    if (which == BASE64) {
        if (encode) {
            to64(fp, fpo, portablenewlines);
        } else {
            from64(fp,fpo, (char **) NULL, (int *) 0, portablenewlines);
        }
    } else {
        if (encode) toqp(fp, fpo); else fromqp(fp, fpo, NULL, 0);
    }
    return(0);
}

