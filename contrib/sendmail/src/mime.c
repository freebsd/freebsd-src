/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1994, 1996-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sendmail.h>
#include <string.h>

#ifndef lint
static char id[] = "@(#)$Id: mime.c,v 8.94.16.3 2000/10/09 02:46:10 gshapiro Exp $";
#endif /* ! lint */

static int	isboundary __P((char *, char **));
static int	mimeboundary __P((char *, char **));
static int	mime_fromqp __P((u_char *, u_char **, int, int));
static int	mime_getchar __P((FILE *, char **, int *));
static int	mime_getchar_crlf __P((FILE *, char **, int *));

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
static char	Base16Code[] =	"0123456789ABCDEF";
static char	Base64Code[] =	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* types of MIME boundaries */
# define MBT_SYNTAX	0	/* syntax error */
# define MBT_NOTSEP	1	/* not a boundary */
# define MBT_INTERMED	2	/* intermediate boundary (no trailing --) */
# define MBT_FINAL	3	/* final boundary (trailing -- included) */

static char	*MimeBoundaryNames[] =
{
	"SYNTAX",	"NOTSEP",	"INTERMED",	"FINAL"
};

static bool	MapNLtoCRLF;

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
	char	*a_field;	/* name of field */
	char	*a_value;	/* value of that field */
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
	bool use_qp = FALSE;
	struct args argv[MAXMIMEARGS];
	char bbuf[128];
	char buf[MAXLINE];
	char pvpbuf[MAXLINE];
	extern u_char MimeTokenTab[256];

	if (tTd(43, 1))
	{
		dprintf("mime8to7: flags = %x, boundaries =", flags);
		if (boundaries[0] == NULL)
			dprintf(" <none>");
		else
		{
			for (i = 0; boundaries[i] != NULL; i++)
				dprintf(" %s", boundaries[i]);
		}
		dprintf("\n");
	}
	MapNLtoCRLF = TRUE;
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
				dprintf("pvp[%d] = \"%s\"\n", i, pvp[i]);
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

			/* complain about empty values */
			if (strcmp(*pvp, ";") == 0)
			{
				usrerr("mime8to7: Empty parameter in Content-Type header");

				/* avoid bounce loops */
				e->e_flags |= EF_DONT_MIME;
				continue;
			}

			/* extract field name */
			argv[argc].a_field = *pvp++;

			/* see if there is a value */
			if (*pvp != NULL && strcmp(*pvp, "=") == 0 &&
			    (*++pvp == NULL || strcmp(*pvp, ";") != 0))
			{
				argv[argc].a_value = *pvp;
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

	snprintf(buf, sizeof buf, "%.100s/%.100s", type, subtype);
	if (wordinclass(buf, 'n') || (cte != NULL && !wordinclass(cte, 'e')))
		flags |= M87F_NO8BIT;

# ifdef USE_B_CLASS
	if (wordinclass(buf, 'b') || wordinclass(type, 'b'))
		MapNLtoCRLF = FALSE;
# endif /* USE_B_CLASS */
	if (wordinclass(buf, 'q') || wordinclass(type, 'q'))
		use_qp = TRUE;

	/*
	**  Multipart requires special processing.
	**
	**	Do a recursive descent into the message.
	*/

	if (strcasecmp(type, "multipart") == 0 &&
	    (!bitset(M87F_NO8BIT, flags) || bitset(M87F_NO8TO7, flags)))
	{

		if (strcasecmp(subtype, "digest") == 0)
			flags |= M87F_DIGEST;

		for (i = 0; i < argc; i++)
		{
			if (strcasecmp(argv[i].a_field, "boundary") == 0)
				break;
		}
		if (i >= argc || argv[i].a_value == NULL)
		{
			usrerr("mime8to7: Content-Type: \"%s\": %s boundary",
				i >= argc ? "missing" : "bogus", p);
			p = "---";

			/* avoid bounce loops */
			e->e_flags |= EF_DONT_MIME;
		}
		else
		{
			p = argv[i].a_value;
			stripquotes(p);
		}
		if (strlcpy(bbuf, p, sizeof bbuf) >= sizeof bbuf)
		{
			usrerr("mime8to7: multipart boundary \"%s\" too long",
				p);

			/* avoid bounce loops */
			e->e_flags |= EF_DONT_MIME;
		}

		if (tTd(43, 1))
			dprintf("mime8to7: multipart boundary \"%s\"\n", bbuf);
		for (i = 0; i < MAXMIMENESTING; i++)
		{
			if (boundaries[i] == NULL)
				break;
		}
		if (i >= MAXMIMENESTING)
		{
			usrerr("mime8to7: multipart nesting boundary too deep");

			/* avoid bounce loops */
			e->e_flags |= EF_DONT_MIME;
		}
		else
		{
			boundaries[i] = bbuf;
			boundaries[i + 1] = NULL;
		}
		mci->mci_flags |= MCIF_INMIME;

		/* skip the early "comment" prologue */
		putline("", mci);
		mci->mci_flags &= ~MCIF_INHEADER;
		bt = MBT_FINAL;
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			bt = mimeboundary(buf, boundaries);
			if (bt != MBT_NOTSEP)
				break;
			putxline(buf, strlen(buf), mci, PXLF_MAPFROM|PXLF_STRIP8BIT);
			if (tTd(43, 99))
				dprintf("  ...%s", buf);
		}
		if (feof(e->e_dfp))
			bt = MBT_FINAL;
		while (bt != MBT_FINAL)
		{
			auto HDR *hdr = NULL;

			snprintf(buf, sizeof buf, "--%s", bbuf);
			putline(buf, mci);
			if (tTd(43, 35))
				dprintf("  ...%s\n", buf);
			collect(e->e_dfp, FALSE, &hdr, e);
			if (tTd(43, 101))
				putline("+++after collect", mci);
			putheader(mci, hdr, e, flags);
			if (tTd(43, 101))
				putline("+++after putheader", mci);
			bt = mime8to7(mci, hdr, e, boundaries, flags);
		}
		snprintf(buf, sizeof buf, "--%s--", bbuf);
		putline(buf, mci);
		if (tTd(43, 35))
			dprintf("  ...%s\n", buf);
		boundaries[i] = NULL;
		mci->mci_flags &= ~MCIF_INMIME;

		/* skip the late "comment" epilogue */
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			bt = mimeboundary(buf, boundaries);
			if (bt != MBT_NOTSEP)
				break;
			putxline(buf, strlen(buf), mci, PXLF_MAPFROM|PXLF_STRIP8BIT);
			if (tTd(43, 99))
				dprintf("  ...%s", buf);
		}
		if (feof(e->e_dfp))
			bt = MBT_FINAL;
		if (tTd(43, 3))
			dprintf("\t\t\tmime8to7=>%s (multipart)\n",
				MimeBoundaryNames[bt]);
		return bt;
	}

	/*
	**  Message/xxx types -- recurse exactly once.
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
			collect(e->e_dfp, FALSE, &hdr, e);
			if (tTd(43, 101))
				putline("+++after collect", mci);
			putheader(mci, hdr, e, flags);
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
	if (!bitset(M87F_NO8BIT|M87F_NO8TO7, flags))
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
		dprintf("mime8to7: %ld high bit(s) in %ld byte(s), cte=%s, type=%s/%s\n",
			(long) sectionhighbits, (long) sectionsize,
			cte == NULL ? "[none]" : cte,
			type == NULL ? "[none]" : type,
			subtype == NULL ? "[none]" : subtype);
	}
	if (cte != NULL && strcasecmp(cte, "binary") == 0)
		sectionsize = sectionhighbits;
	linelen = 0;
	bp = buf;
	if (sectionhighbits == 0)
	{
		/* no encoding necessary */
		if (cte != NULL &&
		    bitset(MCIF_CVT8TO7|MCIF_CVT7TO8|MCIF_INMIME,
			   mci->mci_flags) &&
		    !bitset(M87F_NO8TO7, flags))
		{
			/*
			**  Skip _unless_ in MIME mode and potentially
			**  converting from 8 bit to 7 bit MIME.  See
			**  putheader() for the counterpart where the
			**  CTE header is skipped in the opposite
			**  situation.
			*/

			snprintf(buf, sizeof buf,
				"Content-Transfer-Encoding: %.200s", cte);
			putline(buf, mci);
			if (tTd(43, 36))
				dprintf("  ...%s\n", buf);
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
	else if (!MapNLtoCRLF ||
		 (sectionsize / 8 < sectionhighbits && !use_qp))
	{
		/* use base64 encoding */
		int c1, c2;

		if (tTd(43, 36))
			dprintf("  ...Content-Transfer-Encoding: base64\n");
		putline("Content-Transfer-Encoding: base64", mci);
		snprintf(buf, sizeof buf,
			"X-MIME-Autoconverted: from 8bit to base64 by %s id %s",
			MyHostName, e->e_id);
		putline(buf, mci);
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
		BITMAP256 badchars;

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

		if (tTd(43, 36))
			dprintf("  ...Content-Transfer-Encoding: quoted-printable\n");
		putline("Content-Transfer-Encoding: quoted-printable", mci);
		snprintf(buf, sizeof buf,
			"X-MIME-Autoconverted: from 8bit to quoted-printable by %s id %s",
			MyHostName, e->e_id);
		putline(buf, mci);
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
			if (bitnset(bitidx(c1), badchars))
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
		dprintf("\t\t\tmime8to7=>%s (basic)\n", MimeBoundaryNames[bt]);
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

static int
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
	int start = 0;			/* indicates position of - in buffer */

	if (buflen == 1 && *bp == '\n')
	{
		/* last \n in buffer may be part of next MIME boundary */
		c = *bp;
	}
	else if (buflen > 0)
	{
		buflen--;
		return *bp++;
	}
	else
		c = getc(fp);
	bp = buf;
	buflen = 0;
	if (c == '\n')
	{
		/* might be part of a MIME boundary */
		*bp++ = c;
		atbol = TRUE;
		c = getc(fp);
		if (c == '\n')
		{
			(void) ungetc(c, fp);
			return c;
		}
		start = 1;
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
		bt = mimeboundary((char *) &buf[start], boundaries);
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

static int
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
	if (c == '\n' && MapNLtoCRLF)
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

static int
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
	if (i > 0 && line[i - 1] == '\n')
		i--;

	/* strip off trailing whitespace */
	while (i > 0 && (line[i - 1] == ' ' || line[i - 1] == '\t'))
		i--;
	savec = line[i];
	line[i] = '\0';

	if (tTd(43, 5))
		dprintf("mimeboundary: line=\"%s\"... ", line);

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
		dprintf("%s\n", MimeBoundaryNames[type]);
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

static int
isboundary(line, boundaries)
	char *line;
	char **boundaries;
{
	register int i;

	for (i = 0; i <= MAXMIMENESTING && boundaries[i] != NULL; i++)
	{
		if (strcmp(line, boundaries[i]) == 0)
			return i;
	}
	return -1;
}
#endif /* MIME8TO7 */

#if MIME7TO8

/*
**  MIME7TO8 -- output 7 bit encoded MIME body in 8 bit format
**
**  This is a hack. Supports translating the two 7-bit body-encodings
**  (quoted-printable and base64) to 8-bit coded bodies.
**
**  There is not much point in supporting multipart here, as the UA
**  will be able to deal with encoded MIME bodies if it can parse MIME
**  multipart messages.
**
**  Note also that we wont be called unless it is a text/plain MIME
**  message, encoded base64 or QP and mailer flag '9' has been defined
**  on mailer.
**
**  Contributed by Marius Olaffson <marius@rhi.hi.is>.
**
**	Parameters:
**		mci -- mailer connection information.
**		header -- the header for this body part.
**		e -- envelope.
**
**	Returns:
**		none.
*/

static char index_64[128] =
{
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
	52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
	-1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
	15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
	-1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
	41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

# define CHAR64(c)  (((c) < 0 || (c) > 127) ? -1 : index_64[(c)])

void
mime7to8(mci, header, e)
	register MCI *mci;
	HDR *header;
	register ENVELOPE *e;
{
	register char *p;
	char *cte;
	char **pvp;
	u_char *fbufp;
	char buf[MAXLINE];
	u_char fbuf[MAXLINE + 1];
	char pvpbuf[MAXLINE];
	extern u_char MimeTokenTab[256];

	p = hvalue("Content-Transfer-Encoding", header);
	if (p == NULL ||
	    (pvp = prescan(p, '\0', pvpbuf, sizeof pvpbuf, NULL,
			   MimeTokenTab)) == NULL ||
	    pvp[0] == NULL)
	{
		/* "can't happen" -- upper level should have caught this */
		syserr("mime7to8: unparsable CTE %s", p == NULL ? "<NULL>" : p);

		/* avoid bounce loops */
		e->e_flags |= EF_DONT_MIME;

		/* cheap failsafe algorithm -- should work on text/plain */
		if (p != NULL)
		{
			snprintf(buf, sizeof buf,
				"Content-Transfer-Encoding: %s", p);
			putline(buf, mci);
		}
		putline("", mci);
		mci->mci_flags &= ~MCIF_INHEADER;
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
			putline(buf, mci);
		return;
	}
	cataddr(pvp, NULL, buf, sizeof buf, '\0');
	cte = newstr(buf);

	mci->mci_flags |= MCIF_INHEADER;
	putline("Content-Transfer-Encoding: 8bit", mci);
	snprintf(buf, sizeof buf,
		"X-MIME-Autoconverted: from %.200s to 8bit by %s id %s",
		cte, MyHostName, e->e_id);
	putline(buf, mci);
	putline("", mci);
	mci->mci_flags &= ~MCIF_INHEADER;

	/*
	**  Translate body encoding to 8-bit.  Supports two types of
	**  encodings; "base64" and "quoted-printable". Assume qp if
	**  it is not base64.
	*/

	if (strcasecmp(cte, "base64") == 0)
	{
		int c1, c2, c3, c4;

		fbufp = fbuf;
		while ((c1 = fgetc(e->e_dfp)) != EOF)
		{
			if (isascii(c1) && isspace(c1))
				continue;

			do
			{
				c2 = fgetc(e->e_dfp);
			} while (isascii(c2) && isspace(c2));
			if (c2 == EOF)
				break;

			do
			{
				c3 = fgetc(e->e_dfp);
			} while (isascii(c3) && isspace(c3));
			if (c3 == EOF)
				break;

			do
			{
				c4 = fgetc(e->e_dfp);
			} while (isascii(c4) && isspace(c4));
			if (c4 == EOF)
				break;

			if (c1 == '=' || c2 == '=')
				continue;
			c1 = CHAR64(c1);
			c2 = CHAR64(c2);

			*fbufp = (c1 << 2) | ((c2 & 0x30) >> 4);
			if (*fbufp++ == '\n' || fbufp >= &fbuf[MAXLINE])
			{
				if (*--fbufp != '\n' ||
				    (fbufp > fbuf && *--fbufp != '\r'))
					fbufp++;
				putxline((char *) fbuf, fbufp - fbuf,
					 mci, PXLF_MAPFROM);
				fbufp = fbuf;
			}
			if (c3 == '=')
				continue;
			c3 = CHAR64(c3);
			*fbufp = ((c2 & 0x0f) << 4) | ((c3 & 0x3c) >> 2);
			if (*fbufp++ == '\n' || fbufp >= &fbuf[MAXLINE])
			{
				if (*--fbufp != '\n' ||
				    (fbufp > fbuf && *--fbufp != '\r'))
					fbufp++;
				putxline((char *) fbuf, fbufp - fbuf,
					 mci, PXLF_MAPFROM);
				fbufp = fbuf;
			}
			if (c4 == '=')
				continue;
			c4 = CHAR64(c4);
			*fbufp = ((c3 & 0x03) << 6) | c4;
			if (*fbufp++ == '\n' || fbufp >= &fbuf[MAXLINE])
			{
				if (*--fbufp != '\n' ||
				    (fbufp > fbuf && *--fbufp != '\r'))
					fbufp++;
				putxline((char *) fbuf, fbufp - fbuf,
					 mci, PXLF_MAPFROM);
				fbufp = fbuf;
			}
		}
	}
	else
	{
		/* quoted-printable */
		fbufp = fbuf;
		while (fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			if (mime_fromqp((u_char *) buf, &fbufp, 0,
					&fbuf[MAXLINE] - fbufp) == 0)
				continue;

			if (fbufp - fbuf > 0)
				putxline((char *) fbuf, fbufp - fbuf - 1, mci,
					 PXLF_MAPFROM);
			fbufp = fbuf;
		}
	}

	/* force out partial last line */
	if (fbufp > fbuf)
	{
		*fbufp = '\0';
		putxline((char *) fbuf, fbufp - fbuf, mci, PXLF_MAPFROM);
	}
	if (tTd(43, 3))
		dprintf("\t\t\tmime7to8 => %s to 8bit done\n", cte);
}
/*
**  The following is based on Borenstein's "codes.c" module, with simplifying
**  changes as we do not deal with multipart, and to do the translation in-core,
**  with an attempt to prevent overrun of output buffers.
**
**  What is needed here are changes to defned this code better against
**  bad encodings. Questionable to always return 0xFF for bad mappings.
*/

static char index_hex[128] =
{
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
	-1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

# define HEXCHAR(c)  (((c) < 0 || (c) > 127) ? -1 : index_hex[(c)])

static int
mime_fromqp(infile, outfile, state, maxlen)
	u_char *infile;
	u_char **outfile;
	int state;		/* Decoding body (0) or header (1) */
	int maxlen;		/* Max # of chars allowed in outfile */
{
	int c1, c2;
	int nchar = 0;

	while ((c1 = *infile++) != '\0')
	{
		if (c1 == '=')
		{
			if ((c1 = *infile++) == 0)
				break;

			if (c1 == '\n' || (c1 = HEXCHAR(c1)) == -1)
			{
				/* ignore it */
				if (state == 0)
					return 0;
			}
			else
			{
				do
				{
					if ((c2 = *infile++) == '\0')
					{
						c2 = -1;
						break;
					}
				} while ((c2 = HEXCHAR(c2)) == -1);

				if (c2 == -1 || ++nchar > maxlen)
					break;

				*(*outfile)++ = c1 << 4 | c2;
			}
		}
		else
		{
			if (state == 1 && c1 == '_')
				c1 = ' ';

			if (++nchar > maxlen)
				break;

			*(*outfile)++ = c1;

			if (c1 == '\n')
				break;
		}
	}
	*(*outfile)++ = '\0';
	return 1;
}
#endif /* MIME7TO8 */
