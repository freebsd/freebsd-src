/*
 * Accept one (or more) ASCII encoded chunks that together make a compressed
 * CTM delta.  Decode them and reconstruct the deltas.  Any completed
 * deltas may be passed to ctm for unpacking.
 *
 * Author: Stephen McKay
 *
 * NOTICE: This is free software.  I hope you get some use from this program.
 * In return you should think about all the nice people who give away software.
 * Maybe you should write some free software too.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "error.h"
#include "options.h"

#define CTM_STATUS	".ctm_status"

char *piece_dir = NULL;		/* Where to store pieces of deltas. */
char *delta_dir = NULL;		/* Where to store completed deltas. */
char *base_dir = NULL;		/* The tree to apply deltas to. */
int delete_after = 0;		/* Delete deltas after ctm applies them. */

void apply_complete(void);
int read_piece(char *input_file);
int combine_if_complete(char *delta, int pce, int npieces);
int decode_line(char *line, char *out_buf);

/*
 * If given a '-p' flag, read encoded delta pieces from stdin or file
 * arguments, decode them and assemble any completed deltas.  If given
 * a '-b' flag, pass any completed deltas to 'ctm' for application to
 * the source tree.  The '-d' flag is mandatory, but either of '-p' or
 * '-b' can be omitted.  If given the '-l' flag, notes and errors will
 * be timestamped and written to the given file.
 *
 * Exit status is 0 for success or 1 for indigestible input.  That is,
 * 0 means the encode input pieces were decoded and stored, and 1 means
 * some input was discarded.  If a delta fails to apply, this won't be
 * reflected in the exit status.  In this case, the delta is left in
 * 'deltadir'.
 */
int
main(int argc, char **argv)
    {
    char *log_file = NULL;
    int status = 0;

    err_prog_name(argv[0]);

    OPTIONS("[-D] [-p piecedir] [-d deltadir] [-b basedir] [-l log] [file ...]")
	FLAG('D', delete_after)
	STRING('p', piece_dir)
	STRING('d', delta_dir)
	STRING('b', base_dir)
	STRING('l', log_file)
    ENDOPTS

    if (delta_dir == NULL)
	usage();

    if (piece_dir == NULL && (base_dir == NULL || argc > 1))
	usage();

    if (log_file != NULL)
	err_set_log(log_file);

    if (argc <= 1)
	{
	if (piece_dir != NULL)
	    status = read_piece(NULL);
	}
    else
	{
	while (*++argv != NULL)
	    status |= read_piece(*argv);
	}

    if (base_dir != NULL)
	apply_complete();

    return status;
    }


/*
 * Construct the file name of a piece of a delta.
 */
#define mk_piece_name(fn,d,p,n)	\
    sprintf((fn), "%s/%s+%d-%d", piece_dir, (d), (p), (n))

/*
 * Construct the file name of an assembled delta.
 */
#define mk_delta_name(fn,d)	\
    sprintf((fn), "%s/%s", delta_dir, (d))

/*
 * If the next required delta is now present, let ctm lunch on it and any
 * contiguous deltas.
 */
void
apply_complete()
    {
    int i, dn;
    FILE *fp, *ctm;
    struct stat sb;
    char class[20];
    char delta[30];
    char fname[1000];
    char buf[2000];
    char junk[2];
    char here[1000];

    sprintf(fname, "%s/%s", base_dir, CTM_STATUS);
    if ((fp = fopen(fname, "r")) == NULL)
	return;

    i = fscanf(fp, "%s %d %c", class, &dn, junk);
    fclose(fp);
    if (i != 2)
	return;

    /*
     * We might need to convert the delta filename to an absolute pathname.
     */
    here[0] = '\0';
    if (delta_dir[0] != '/')
	{
	getcwd(here, sizeof(here)-1);
	i = strlen(here) - 1;
	if (i >= 0 && here[i] != '/')
	    {
	    here[++i] = '/';
	    here[++i] = '\0';
	    }
	}

    /*
     * Keep applying deltas until we run out or something bad happens.
     */
    for (;;)
	{
	sprintf(delta, "%s.%04d.gz", class, ++dn);
	mk_delta_name(fname, delta);

	if (stat(fname, &sb) < 0)
	    return;

	sprintf(buf, "(cd %s && ctm %s%s) 2>&1", base_dir, here, fname);
	if ((ctm = popen(buf, "r")) == NULL)
	    {
	    err("ctm failed to apply %s", delta);
	    return;
	    }

	while (fgets(buf, sizeof(buf), ctm) != NULL)
	    {
	    i = strlen(buf) - 1;
	    if (i >= 0 && buf[i] == '\n')
		buf[i] = '\0';
	    err("ctm: %s", buf);
	    }

	if (pclose(ctm) != 0)
	    {
	    err("ctm failed to apply %s", delta);
	    return;
	    }

	if (delete_after)
	    unlink(fname);

	err("%s applied%s", delta, delete_after ? " and deleted" : "");
	}
    }


/*
 * This cheap plastic checksum effectively rotates our checksum-so-far
 * left one, then adds the character.  We only want 16 bits of it, and
 * don't care what happens to the rest.  It ain't much, but it's small.
 */
#define add_ck(sum,x)	\
    ((sum) += ((x)&0xff) + (sum) + (((sum)&0x8000) ? 1 : 0))


/*
 * Decode the data between BEGIN and END, and stash it in the staging area.
 * Multiple pieces can be present in a single file, bracketed by BEGIN/END.
 * If we have all pieces of a delta, combine them.  Returns 0 on success,
 * and 1 for any sort of failure.
 */
int
read_piece(char *input_file)
    {
    int status = 0;
    FILE *ifp, *ofp = 0;
    int decoding = 0;
    int line_no = 0;
    int i, n;
    int pce, npieces;
    unsigned claimed_cksum;
    unsigned short cksum = 0;
    char out_buf[200];
    char line[200];
    char delta[30];
    char pname[1000];
    char junk[2];

    ifp = stdin;
    if (input_file != NULL && (ifp = fopen(input_file, "r")) == NULL)
	{
	err("cannot open '%s' for reading", input_file);
	return 1;
	}

    while (fgets(line, sizeof(line), ifp) != NULL)
	{
	line_no++;

	/*
	 * Look for the beginning of an encoded piece.
	 */
	if (!decoding)
	    {
	    char *s;

	    if (sscanf(line, "CTM_MAIL BEGIN %s %d %d %c",
		    delta, &pce, &npieces, junk) != 3)
		continue;

	    while ((s = strchr(delta, '/')) != NULL)
		*s = '_';

	    mk_piece_name(pname, delta, pce, npieces);
	    if ((ofp = fopen(pname, "w")) == NULL)
		{
		err("cannot open '%s' for writing", pname);
		status++;
		continue;
		}

	    cksum = 0xffff;
	    decoding++;
	    continue;
	    }

	/*
	 * We are decoding.  Stop if we see the end flag.
	 */
	if (sscanf(line, "CTM_MAIL END %d %c", &claimed_cksum, junk) == 1)
	    {
	    int e;

	    decoding = 0;

	    fflush(ofp);
	    e = ferror(ofp);
	    fclose(ofp);

	    if (e)
		err("error writing %s", pname);

	    if (cksum != claimed_cksum)
		err("checksum: read %d, calculated %d", claimed_cksum, cksum);

	    if (e || cksum != claimed_cksum)
		{
		err("%s %d/%d discarded", delta, pce, npieces);
		unlink(pname);
		status++;
		continue;
		}

	    err("%s %d/%d stored", delta, pce, npieces);

	    if (!combine_if_complete(delta, pce, npieces))
		status++;
	    continue;
	    }

	/*
	 * Must be a line of encoded data.  Decode it, sum it, and save it.
	 */
	n = decode_line(line, out_buf);
	if (n < 0)
	    {
	    err("line %d: illegal character: '%c'", line_no, line[-n]);
	    err("%s %d/%d discarded", delta, pce, npieces);

	    fclose(ofp);
	    unlink(pname);

	    status++;
	    decoding = 0;
	    continue;
	    }

	for (i = 0; i < n; i++)
	    add_ck(cksum, out_buf[i]);

	fwrite(out_buf, sizeof(char), n, ofp);
	}

    if (decoding)
	{
	err("truncated file");
	err("%s %d/%d discarded", delta, pce, npieces);

	fclose(ofp);
	unlink(pname);

	status++;
	}

    if (ferror(ifp))
	{
	err("error reading %s", input_file == NULL ? "stdin" : input_file);
	status++;
	}

    if (input_file != NULL)
	fclose(ifp);

    return (status != 0);
    }


/*
 * Put the pieces together to form a delta, if they are all present.
 * Returns 1 on success (even if we didn't do anything), and 0 on failure.
 */
int
combine_if_complete(char *delta, int pce, int npieces)
    {
    int i;
    FILE *dfp, *pfp;
    int c;
    struct stat sb;
    char pname[1000];
    char dname[1000];

    /*
     * All here?
     */
    for (i = 1; i <= npieces; i++)
	{
	if (i == pce)
	    continue;
	mk_piece_name(pname, delta, i, npieces);
	if (stat(pname, &sb) < 0)
	    return 1;
	}

    mk_delta_name(dname, delta);

    /*
     * We can probably just rename() it in to place if it is a small delta.
     */
    if (npieces == 1)
	{
	mk_piece_name(pname, delta, 1, 1);
	if (rename(pname, dname) == 0)
	    {
	    err("%s complete", delta);
	    return 1;
	    }
	}

    if ((dfp = fopen(dname, "w")) == NULL)
	{
	err("cannot open '%s' for writing", dname);
	return 0;
	}

    /*
     * Ok, the hard way.  Reconstruct the delta by reading each piece in order.
     */
    for (i = 1; i <= npieces; i++)
	{
	mk_piece_name(pname, delta, i, npieces);
	if ((pfp = fopen(pname, "r")) == NULL)
	    {
	    err("cannot open '%s' for reading", pname);
	    fclose(dfp);
	    unlink(dname);
	    return 0;
	    }
	while ((c = getc(pfp)) != EOF)
	    putc(c, dfp);
	fclose(pfp);
	}
    fflush(dfp);
    if (ferror(dfp))
	{
	err("error writing '%s'", dname);
	fclose(dfp);
	unlink(dname);
	return 0;
	}
    fclose(dfp);

    /*
     * Throw the pieces away.
     */
    for (i = 1; i <= npieces; i++)
	{
	mk_piece_name(pname, delta, i, npieces);
	unlink(pname);
	}

    err("%s complete", delta);
    return 1;
    }


/*
 * MIME BASE64 decode table.
 */
static unsigned char from_b64[0x80] =
    { 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
    };


/*
 * Decode a line of ASCII into binary.  Returns the number of bytes in
 * the output buffer, or < 0 on indigestable input.  Error output is
 * the negative of the index of the inedible character.
 */
int
decode_line(char *line, char *out_buf)
    {
    unsigned char *ip = (unsigned char *)line;
    unsigned char *op = (unsigned char *)out_buf;
    unsigned long bits;
    unsigned x;

    for (;;)
	{
	if (*ip >= 0x80 || (x = from_b64[*ip]) >= 0x40)
	    break;
	bits = x << 18;
	ip++;
	if (*ip < 0x80 && (x = from_b64[*ip]) < 0x40)
	    {
	    bits |= x << 12;
	    *op++ = bits >> 16;
	    ip++;
	    if (*ip < 0x80 && (x = from_b64[*ip]) < 0x40)
		{
		bits |= x << 6;
		*op++ = bits >> 8;
		ip++;
		if (*ip < 0x80 && (x = from_b64[*ip]) < 0x40)
		    {
		    bits |= x;
		    *op++ = bits;
		    ip++;
		    }
		}
	    }
	}

    if (*ip == '\0' || *ip == '\n')
	return op - (unsigned char *)out_buf;
    else
	return -(ip - (unsigned char *)line);
    }
