/*
 * Copyright (c) 1994 Eric P. Allman
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

# include "sendmail.h"
# include <string.h>

#ifndef lint
static char sccsid[] = "@(#)mime.c	8.30 (Berkeley) 10/31/95";
#endif /* not lint */

/*
**  MIME support.
**
**	I am indebted to John Beck of Hewlett-Packard, who contributed
**	his code to me for inclusion.  As it turns out, I did not use
**	his code since he used a "minimum change" approach that used
**	several temp files, and I wanted a "minimum impact" approach
**	that would avoid copying.  However, looking over his code
**	helped me cement my understanding of the problem.
**
**	I also looked at, but did not directly use, Nathaniel
**	Borenstein's "code.c" module.  Again, it functioned as
**	a file-to-file translator, which did not fit within my
**	design bounds, but it was a useful base for understanding
**	the problem.
*/

#if MIME8TO7

/* character set for hex and base64 encoding */
char	Base16Code[] =	"0123456789ABCDEF";
char	Base64Code[] =	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* types of MIME boundaries */
#define MBT_SYNTAX	0	/* syntax error */
#define MBT_NOTSEP	1	/* not a boundary */
#define MBT_INTERMED	2	/* intermediate boundary (no trailing --) */
#define MBT_FINAL	3	/* final boundary (trailing -- included) */

static char	*MimeBoundaryNames[] =
{
	"SYNTAX",	"NOTSEP",	"INTERMED",	"FINAL"
};
/*
**  MIME8TO7 -- output 8 bit body in 7 bit format
**
**	The header has already been output -- this has to do the
**	8 to 7 bit conversion.  It would be easy if we didn't have
**	to deal with nested formats (multipart/xxx and message/rfc822).
**
**	We won't be called if we don't have to do a conversion, and
**	appropriate MIME-Version: and Content-Type: fields have been
**	output.  Any Content-Transfer-Encoding: field has not been
**	output, and we can add it here.
**
**	Parameters:
**		mci -- mailer connection information.
**		header -- the header for this body part.
**		e -- envelope.
**		boundaries -- the currently pending message boundaries.
**			NULL if we are processing the outer portion.
**		flags -- to tweak processing.
**
**	Returns:
**		An indicator of what terminated the message part:
**		  MBT_FINAL -- the final boundary
**		  MBT_INTERMED -- an intermediate boundary
**		  MBT_NOTSEP -- an end of file
*/

struct args
{
	char	*field;		/* name of field */
	char	*value;		/* value of that field */
};

int
mime8to7(mci, header, e, boundaries, flags)
	register MCI *mci;
	HDR *header;
	register ENVELOPE *e;
	char **boundaries;
	int flags;
{
	register char *p;
	int linelen;
	int bt;
	off_t offset;
	size_t sectionsize, sectionhighbits;
	int i;
	char *type;
	char *subtype;
	char *cte;
	char **pvp;
	int argc = 0;
	char *bp;
	struct args argv[MAXMIMEARGS];
	char bbuf[128];
	char buf[MAXLINE];
	char pvpbuf[MAXLINE];
	extern u_char MimeTokenTab[256];

	if (tTd(43, 1))
	{
		printf("mime8to7: flags = %x, boundaries =", flags);
		if (boundaries[0] == NULL)
			printf(" <none>");
		else
		{
			for (i = 0; boundaries[i] != NULL; i++)
				printf(" %s", boundaries[i]);
		}
		printf("\n");
	}
	p = hvalue("Content-Transfer-Encoding", header);
	if (p == NULL ||
	    (pvp = prescan(p, '\0', pvpbuf, sizeof pvpbuf, NULL,
			   MimeTokenTab)) == NULL ||
	    pvp[0] == NULL)
	{
		cte = NULL;
	}
	else
	{
		cataddr(pvp, NULL, buf, sizeof buf, '\0');
		cte = newstr(buf);
	}

	type = subtype = NULL;
	p = hvalue("Content-Type", header);
	if (p == NULL)
	{
		if (bitset(M87F_DIGEST, flags))
			p = "message/rfc822";
		else
			p = "text/plain";
	}
	if (p != NULL &&
	    (pvp = prescan(p, '\0', pvpbuf, sizeof pvpbuf, NULL,
			   MimeTokenTab)) != NULL &&
	    pvp[0] != NULL)
	{
		if (tTd(43, 40))
		{
			for (i = 0; pvp[i] != NULL; i++)
				printf("pvp[%d] = \"%s\"\n", i, pvp[i]);
		}
		type = *pvp++;
		if (*pvp != NULL && strcmp(*pvp, "/") == 0 &&
		    *++pvp != NULL)
		{
			subtype = *pvp++;
		}

		/* break out parameters */
		while (*pvp != NULL && argc < MAXMIMEARGS)
		{
			/* skip to semicolon separator */
			while (*pvp != NULL && strcmp(*pvp, ";") != 0)
				pvp++;
			if (*pvp++ == NULL || *pvp == NULL)
				break;

			/* extract field name */
			argv[argc].field = *pvp++;

			/* see if there is a value */
			if (*pvp != NULL && strcmp(*pvp, "=") == 0 &&
			    (*++pvp == NULL || strcmp(*pvp, ";") != 0))
			{
				argv[argc].value = *pvp;
				argc++;
			}
		}
	}

	/* check for disaster cases */
	if (type == NULL)
		type = "-none-";
	if (subtype == NULL)
		subtype = "-none-";

	/* don't propogate some flags more than one level into the message */
	flags &= ~M87F_DIGEST;

	/*
	**  Check for cases that can not be encoded.
	**
	**	For example, you can't encode certain kinds of types
	**	or already-encoded messages.  If we find this case,
	**	just copy it through.
	*/

	sprintf(buf, "%.100s/%.100s", type, subtype);
	if (wordinclass(buf, 'n') || (cte != NULL && !wordinclass(cte, 'e')))
		flags |= M87F_NO8BIT;

	/*
	**  Multipart requires special processing.
	**
	**	Do a recursive descent into the message.
	*/

	if (strcasecmp(type, "multipart") == 0 && !bitset(M87F_NO8BIT, flags))
	{
		int blen;

		if (strcasecmp(subtype, "digest") == 0)
			flags |= M87F_DIGEST;

		for (i = 0; i < argc; i++)
		{
			if (strcasecmp(argv[i].field, "boundary") == 0)
				break;
		}
		if (i >= argc)
		{
			syserr("mime8to7: Content-Type: %s missing boundary", p);
			p = "---";
		}
		else
		{
			p = argv[i].value;
			stripquotes(p);
		}
		blen = strlen(p);
		if (blen > sizeof bbuf - 1)
		{
			syserr("mime8to7: multipart boundary \"%s\" too long",
				p);
			blen = sizeof bbuf - 1;
		}
		strncpy(bbuf, p, blen);
		bbuf[blen] = '\0';
		if (tTd(43, 1))
			printf("mime8to7: multipart boundary \"%s\"\n", bbuf);
		for (i = 0; i < MAXMIMENESTING; i++)
			if (boundaries[i] == NULL)
				break;
		if (i >= MAXMIMENESTING)
			syserr("mime8to7: multipart nesting boundary too deep");
		else
		{
			boundaries[i] = bbuf;
			boundaries[i + 1] = NULL;
		}
		mci->mci_flags |= MCIF_INMIME;

		/* skip the early "comment" prologue */
		putline("", mci);
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			bt = mimeboundary(buf, boundaries);
			if (bt != MBT_NOTSEP)
				break;
			putxline(buf, mci, PXLF_MAPFROM|PXLF_STRIP8BIT);
			if (tTd(43, 99))
				printf("  ...%s", buf);
		}
		if (feof(e->e_dfp))
			bt = MBT_FINAL;
		while (bt != MBT_FINAL)
		{
			auto HDR *hdr = NULL;

			sprintf(buf, "--%s", bbuf);
			putline(buf, mci);
			if (tTd(43, 35))
				printf("  ...%s\n", buf);
			collect(e->e_dfp, FALSE, FALSE, &hdr, e);
			if (tTd(43, 101))
				putline("+++after collect", mci);
			putheader(mci, hdr, e);
			if (tTd(43, 101))
				putline("+++after putheader", mci);
			bt = mime8to7(mci, hdr, e, boundaries, flags);
		}
		sprintf(buf, "--%s--", bbuf);
		putline(buf, mci);
		if (tTd(43, 35))
			printf("  ...%s\n", buf);
		boundaries[i] = NULL;
		mci->mci_flags &= ~MCIF_INMIME;

		/* skip the late "comment" epilogue */
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			bt = mimeboundary(buf, boundaries);
			if (bt != MBT_NOTSEP)
				break;
			putxline(buf, mci, PXLF_MAPFROM|PXLF_STRIP8BIT);
			if (tTd(43, 99))
				printf("  ...%s", buf);
		}
		if (feof(e->e_dfp))
			bt = MBT_FINAL;
		if (tTd(43, 3))
			printf("\t\t\tmime8to7=>%s (multipart)\n",
				MimeBoundaryNames[bt]);
		return bt;
	}

	/*
	**  Message/* types -- recurse exactly once.
	**
	**	Class 's' is predefined to have "rfc822" only.
	*/

	if (strcasecmp(type, "message") == 0)
	{
		if (!wordinclass(subtype, 's'))
		{
			flags |= M87F_NO8BIT;
		}
		else
		{
			auto HDR *hdr = NULL;

			putline("", mci);

			mci->mci_flags |= MCIF_INMIME;
			collect(e->e_dfp, FALSE, FALSE, &hdr, e);
			if (tTd(43, 101))
				putline("+++after collect", mci);
			putheader(mci, hdr, e);
			if (tTd(43, 101))
				putline("+++after putheader", mci);
			if (hvalue("MIME-Version", hdr) == NULL)
				putline("MIME-Version: 1.0", mci);
			bt = mime8to7(mci, hdr, e, boundaries, flags);
			mci->mci_flags &= ~MCIF_INMIME;
			return bt;
		}
	}

	/*
	**  Non-compound body type
	**
	**	Compute the ratio of seven to eight bit characters;
	**	use that as a heuristic to decide how to do the
	**	encoding.
	*/

	sectionsize = sectionhighbits = 0;
	if (!bitset(M87F_NO8BIT, flags))
	{
		/* remember where we were */
		offset = ftell(e->e_dfp);
		if (offset == -1)
			syserr("mime8to7: cannot ftell on df%s", e->e_id);

		/* do a scan of this body type to count character types */
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			if (mimeboundary(buf, boundaries) != MBT_NOTSEP)
				break;
			for (p = buf; *p != '\0'; p++)
			{
				/* count bytes with the high bit set */
				sectionsize++;
				if (bitset(0200, *p))
					sectionhighbits++;
			}

			/*
			**  Heuristic: if 1/4 of the first 4K bytes are 8-bit,
			**  assume base64.  This heuristic avoids double-reading
			**  large graphics or video files.
			*/

			if (sectionsize >= 4096 &&
			    sectionhighbits > sectionsize / 4)
				break;
		}

		/* return to the original offset for processing */
		/* XXX use relative seeks to handle >31 bit file sizes? */
		if (fseek(e->e_dfp, offset, SEEK_SET) < 0)
			syserr("mime8to7: cannot fseek on df%s", e->e_id);
		else
			clearerr(e->e_dfp);
	}

	/*
	**  Heuristically determine encoding method.
	**	If more than 1/8 of the total characters have the
	**	eighth bit set, use base64; else use quoted-printable.
	**	However, only encode binary encoded data as base64,
	**	since otherwise the NL=>CRLF mapping will be a problem.
	*/

	if (tTd(43, 8))
	{
		printf("mime8to7: %ld high bit(s) in %ld byte(s), cte=%s\n",
			sectionhighbits, sectionsize,
			cte == NULL ? "[none]" : cte);
	}
	if (cte != NULL && strcasecmp(cte, "binary") == 0)
		sectionsize = sectionhighbits;
	linelen = 0;
	bp = buf;
	if (sectionhighbits == 0)
	{
		/* no encoding necessary */
		if (cte != NULL)
		{
			sprintf(buf, "Content-Transfer-Encoding: %.200s", cte);
			putline(buf, mci);
			if (tTd(43, 36))
				printf("  ...%s\n", buf);
		}
		putline("", mci);
		mci->mci_flags &= ~MCIF_INHEADER;
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			bt = mimeboundary(buf, boundaries);
			if (bt != MBT_NOTSEP)
				break;
			putline(buf, mci);
		}
		if (feof(e->e_dfp))
			bt = MBT_FINAL;
	}
	else if (sectionsize / 8 < sectionhighbits)
	{
		/* use base64 encoding */
		int c1, c2;

		putline("Content-Transfer-Encoding: base64", mci);
		if (tTd(43, 36))
			printf("  ...Content-Transfer-Encoding: base64\n");
		putline("", mci);
		mci->mci_flags &= ~MCIF_INHEADER;
		while ((c1 = mime_getchar_crlf(e->e_dfp, boundaries, &bt)) != EOF)
		{
			if (linelen > 71)
			{
				*bp = '\0';
				putline(buf, mci);
				linelen = 0;
				bp = buf;
			}
			linelen += 4;
			*bp++ = Base64Code[(c1 >> 2)];
			c1 = (c1 & 0x03) << 4;
			c2 = mime_getchar_crlf(e->e_dfp, boundaries, &bt);
			if (c2 == EOF)
			{
				*bp++ = Base64Code[c1];
				*bp++ = '=';
				*bp++ = '=';
				break;
			}
			c1 |= (c2 >> 4) & 0x0f;
			*bp++ = Base64Code[c1];
			c1 = (c2 & 0x0f) << 2;
			c2 = mime_getchar_crlf(e->e_dfp, boundaries, &bt);
			if (c2 == EOF)
			{
				*bp++ = Base64Code[c1];
				*bp++ = '=';
				break;
			}
			c1 |= (c2 >> 6) & 0x03;
			*bp++ = Base64Code[c1];
			*bp++ = Base64Code[c2 & 0x3f];
		}
		*bp = '\0';
		putline(buf, mci);
	}
	else
	{
		/* use quoted-printable encoding */
		int c1, c2;
		int fromstate;
		BITMAP badchars;

		/* set up map of characters that must be mapped */
		clrbitmap(badchars);
		for (c1 = 0x00; c1 < 0x20; c1++)
			setbitn(c1, badchars);
		clrbitn('\t', badchars);
		for (c1 = 0x7f; c1 < 0x100; c1++)
			setbitn(c1, badchars);
		setbitn('=', badchars);
		if (bitnset(M_EBCDIC, mci->mci_mailer->m_flags))
			for (p = "!\"#$@[\\]^`{|}~"; *p != '\0'; p++)
				setbitn(*p, badchars);

		putline("Content-Transfer-Encoding: quoted-printable", mci);
		if (tTd(43, 36))
			printf("  ...Content-Transfer-Encoding: quoted-printable\n");
		putline("", mci);
		mci->mci_flags &= ~MCIF_INHEADER;
		fromstate = 0;
		c2 = '\n';
		while ((c1 = mime_getchar(e->e_dfp, boundaries, &bt)) != EOF)
		{
			if (c1 == '\n')
			{
				if (c2 == ' ' || c2 == '\t')
				{
					*bp++ = '=';
					*bp++ = Base16Code[(c2 >> 4) & 0x0f];
					*bp++ = Base16Code[c2 & 0x0f];
				}
				if (buf[0] == '.' && bp == &buf[1])
				{
					buf[0] = '=';
					*bp++ = Base16Code[('.' >> 4) & 0x0f];
					*bp++ = Base16Code['.' & 0x0f];
				}
				*bp = '\0';
				putline(buf, mci);
				linelen = fromstate = 0;
				bp = buf;
				c2 = c1;
				continue;
			}
			if (c2 == ' ' && linelen == 4 && fromstate == 4 &&
			    bitnset(M_ESCFROM, mci->mci_mailer->m_flags))
			{
				*bp++ = '=';
				*bp++ = '2';
				*bp++ = '0';
				linelen += 3;
			}
			else if (c2 == ' ' || c2 == '\t')
			{
				*bp++ = c2;
				linelen++;
			}
			if (linelen > 72 &&
			    (linelen > 75 || c1 != '.' ||
			     (linelen > 73 && c2 == '.')))
			{
				if (linelen > 73 && c2 == '.')
					bp--;
				else
					c2 = '\n';
				*bp++ = '=';
				*bp = '\0';
				putline(buf, mci);
				linelen = fromstate = 0;
				bp = buf;
				if (c2 == '.')
				{
					*bp++ = '.';
					linelen++;
				}
			}
			if (bitnset(c1 & 0xff, badchars))
			{
				*bp++ = '=';
				*bp++ = Base16Code[(c1 >> 4) & 0x0f];
				*bp++ = Base16Code[c1 & 0x0f];
				linelen += 3;
			}
			else if (c1 != ' ' && c1 != '\t')
			{
				if (linelen < 4 && c1 == "From"[linelen])
					fromstate++;
				*bp++ = c1;
				linelen++;
			}
			c2 = c1;
		}

		/* output any saved character */
		if (c2 == ' ' || c2 == '\t')
		{
			*bp++ = '=';
			*bp++ = Base16Code[(c2 >> 4) & 0x0f];
			*bp++ = Base16Code[c2 & 0x0f];
			linelen += 3;
		}

		if (linelen > 0 || boundaries[0] != NULL)
		{
			*bp = '\0';
			putline(buf, mci);
		}

	}
	if (tTd(43, 3))
		printf("\t\t\tmime8to7=>%s (basic)\n", MimeBoundaryNames[bt]);
	return bt;
}
/*
**  MIME_GETCHAR -- get a character for MIME processing
**
**	Treats boundaries as EOF.
**
**	Parameters:
**		fp -- the input file.
**		boundaries -- the current MIME boundaries.
**		btp -- if the return value is EOF, *btp is set to
**			the type of the boundary.
**
**	Returns:
**		The next character in the input stream.
*/

int
mime_getchar(fp, boundaries, btp)
	register FILE *fp;
	char **boundaries;
	int *btp;
{
	int c;
	static u_char *bp = NULL;
	static int buflen = 0;
	static bool atbol = TRUE;	/* at beginning of line */
	static int bt = MBT_SYNTAX;	/* boundary type of next EOF */
	static u_char buf[128];		/* need not be a full line */

	if (buflen > 0)
	{
		buflen--;
		return *bp++;
	}
	bp = buf;
	buflen = 0;
	c = getc(fp);
	if (c == '\n')
	{
		/* might be part of a MIME boundary */
		*bp++ = c;
		atbol = TRUE;
		c = getc(fp);
		if (c == '\n')
		{
			ungetc(c, fp);
			return c;
		}
	}
	if (c != EOF)
		*bp++ = c;
	else
		bt = MBT_FINAL;
	if (atbol && c == '-')
	{
		/* check for a message boundary */
		c = getc(fp);
		if (c != '-')
		{
			if (c != EOF)
				*bp++ = c;
			else
				bt = MBT_FINAL;
			buflen = bp - buf - 1;
			bp = buf;
			return *bp++;
		}

		/* got "--", now check for rest of separator */
		*bp++ = '-';
		while (bp < &buf[sizeof buf - 2] &&
		       (c = getc(fp)) != EOF && c != '\n')
		{
			*bp++ = c;
		}
		*bp = '\0';
		bt = mimeboundary(&buf[1], boundaries);
		switch (bt)
		{
		  case MBT_FINAL:
		  case MBT_INTERMED:
			/* we have a message boundary */
			buflen = 0;
			*btp = bt;
			return EOF;
		}

		atbol = c == '\n';
		if (c != EOF)
			*bp++ = c;
	}

	buflen = bp - buf - 1;
	if (buflen < 0)
	{
		*btp = bt;
		return EOF;
	}
	bp = buf;
	return *bp++;
}
/*
**  MIME_GETCHAR_CRLF -- do mime_getchar, but translate NL => CRLF
**
**	Parameters:
**		fp -- the input file.
**		boundaries -- the current MIME boundaries.
**		btp -- if the return value is EOF, *btp is set to
**			the type of the boundary.
**
**	Returns:
**		The next character in the input stream.
*/

int
mime_getchar_crlf(fp, boundaries, btp)
	register FILE *fp;
	char **boundaries;
	int *btp;
{
	static bool sendlf = FALSE;
	int c;

	if (sendlf)
	{
		sendlf = FALSE;
		return '\n';
	}
	c = mime_getchar(fp, boundaries, btp);
	if (c == '\n')
	{
		sendlf = TRUE;
		return '\r';
	}
	return c;
}
/*
**  MIMEBOUNDARY -- determine if this line is a MIME boundary & its type
**
**	Parameters:
**		line -- the input line.
**		boundaries -- the set of currently pending boundaries.
**
**	Returns:
**		MBT_NOTSEP -- if this is not a separator line
**		MBT_INTERMED -- if this is an intermediate separator
**		MBT_FINAL -- if this is a final boundary
**		MBT_SYNTAX -- if this is a boundary for the wrong
**			enclosure -- i.e., a syntax error.
*/

int
mimeboundary(line, boundaries)
	register char *line;
	char **boundaries;
{
	int type = MBT_NOTSEP;
	int i;
	int savec;

	if (line[0] != '-' || line[1] != '-' || boundaries == NULL)
		return MBT_NOTSEP;
	i = strlen(line);
	if (line[i - 1] == '\n')
		i--;

	/* strip off trailing whitespace */
	while (line[i - 1] == ' ' || line[i - 1] == '\t')
		i--;
	savec = line[i];
	line[i] = '\0';

	if (tTd(43, 5))
		printf("mimeboundary: line=\"%s\"... ", line);

	/* check for this as an intermediate boundary */
	if (isboundary(&line[2], boundaries) >= 0)
		type = MBT_INTERMED;
	else if (i > 2 && strncmp(&line[i - 2], "--", 2) == 0)
	{
		/* check for a final boundary */
		line[i - 2] = '\0';
		if (isboundary(&line[2], boundaries) >= 0)
			type = MBT_FINAL;
		line[i - 2] = '-';
	}

	line[i] = savec;
	if (tTd(43, 5))
		printf("%s\n", MimeBoundaryNames[type]);
	return type;
}
/*
**  DEFCHARSET -- return default character set for message
**
**	The first choice for character set is for the mailer
**	corresponding to the envelope sender.  If neither that
**	nor the global configuration file has a default character
**	set defined, return "unknown-8bit" as recommended by
**	RFC 1428 section 3.
**
**	Parameters:
**		e -- the envelope for this message.
**
**	Returns:
**		The default character set for that mailer.
*/

char *
defcharset(e)
	register ENVELOPE *e;
{
	if (e != NULL && e->e_from.q_mailer != NULL &&
	    e->e_from.q_mailer->m_defcharset != NULL)
		return e->e_from.q_mailer->m_defcharset;
	if (DefaultCharSet != NULL)
		return DefaultCharSet;
	return "unknown-8bit";
}
/*
**  ISBOUNDARY -- is a given string a currently valid boundary?
**
**	Parameters:
**		line -- the current input line.
**		boundaries -- the list of valid boundaries.
**
**	Returns:
**		The index number in boundaries if the line is found.
**		-1 -- otherwise.
**
*/

int
isboundary(line, boundaries)
	char *line;
	char **boundaries;
{
	register int i;

	for (i = 0; boundaries[i] != NULL; i++)
	{
		if (strcmp(line, boundaries[i]) == 0)
			return i;
	}
	return -1;
}

#endif /* MIME */
