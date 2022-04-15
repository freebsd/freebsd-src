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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int PendingBoundary(s, Boundaries, BoundaryCt)
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

void toqp(infile, outfile) 
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

void fromqp(infile, outfile, boundaries, boundaryct) 
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

#define QP 2 /* quoted-printable */

int main(argc, argv)
int argc;
char **argv;
{
    int encode = 1, i;
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
		default:
                    fprintf(stderr,
                       "Usage: mmencode [-u] [-o outputfile] [file name]\n");
                    exit(-1);
            }
        } else {
            fp = fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                exit(-1);
            }
        }
    }
    if (encode) toqp(fp, fpo); else fromqp(fp, fpo, NULL, 0);
    return(0);
}

