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
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include "error.h"
#include "options.h"

#define CTM_STATUS	".ctm_status"

char *piece_dir = NULL;		/* Where to store pieces of deltas. */
char *delta_dir = NULL;		/* Where to store completed deltas. */
char *base_dir = NULL;		/* The tree to apply deltas to. */
int delete_after = 0;		/* Delete deltas after ctm applies them. */
int apply_verbose = 0;		/* Run with '-v' */
int set_time = 0;		/* Set the time of the files that is changed. */

void apply_complete(void);
int read_piece(char *input_file);
int combine_if_complete(char *delta, int pce, int npieces);
int combine(char *delta, int npieces, char *dname, char *pname, char *tname);
int decode_line(char *line, char *out_buf);
int lock_file(char *name);

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
    int fork_ctm = 0;

    err_prog_name(argv[0]);

    OPTIONS("[-Dfuv] [-p piecedir] [-d deltadir] [-b basedir] [-l log] [file ...]")
	FLAG('D', delete_after)
	FLAG('f', fork_ctm)
	FLAG('u', set_time)
	FLAG('v', apply_verbose)
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

    /*
     * Digest each file in turn, or just stdin if no files were given.
     */
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

    /*
     * Maybe it's time to look for and apply completed deltas with ctm.
     *
     * Shall we report back to sendmail immediately, and let a child do
     * the work?  Sendmail will be waiting for us to complete, delaying
     * other mail, and possibly some intermediate process (like MH slocal)
     * will terminate us if we take too long!
     *
     * If fork() fails, it's unlikely we'll be able to run ctm, so give up.
     * Also, the child exit status is unimportant.
     */
    if (base_dir != NULL)
	if (!fork_ctm || fork() == 0)
	    apply_complete();

    return status;
    }


/*
 * Construct the file name of a piece of a delta.
 */
#define mk_piece_name(fn,d,p,n)	\
    sprintf((fn), "%s/%s+%03d-%03d", piece_dir, (d), (p), (n))

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
    int lfd;
    FILE *fp, *ctm;
    struct stat sb;
    char class[20];
    char delta[30];
    char junk[2];
    char fname[PATH_MAX];
    char here[PATH_MAX];
    char buf[PATH_MAX*2];

    /*
     * Grab a lock on the ctm mutex file so that we can be sure we are
     * working alone, not fighting another ctm_rmail!
     */
    strcpy(fname, delta_dir);
    strcat(fname, "/.mutex_apply");
    if ((lfd = lock_file(fname)) < 0)
	return;

    /*
     * Find out which delta ctm needs next.
     */
    sprintf(fname, "%s/%s", base_dir, CTM_STATUS);
    if ((fp = fopen(fname, "r")) == NULL)
	{
	close(lfd);
	return;
	}

    i = fscanf(fp, "%s %d %c", class, &dn, junk);
    fclose(fp);
    if (i != 2)
	{
	close(lfd);
	return;
	}

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
	    break;

	sprintf(buf, "(cd %s && ctm %s%s%s%s) 2>&1", base_dir,
				set_time ? "-u " : "",
				apply_verbose ? "-v " : "", here, fname);
	if ((ctm = popen(buf, "r")) == NULL)
	    {
	    err("ctm failed to apply %s", delta);
	    break;
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
	    break;
	    }

	if (delete_after)
	    unlink(fname);

	err("%s applied%s", delta, delete_after ? " and deleted" : "");
	}

    /*
     * Closing the lock file clears the lock.
     */
    close(lfd);
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
    int got_one = 0;
    int line_no = 0;
    int i, n;
    int pce, npieces;
    unsigned claimed_cksum;
    unsigned short cksum = 0;
    char out_buf[200];
    char line[200];
    char delta[30];
    char pname[PATH_MAX];
    char tname[PATH_MAX];
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
	 * Remove all trailing white space.
	 */
	i = strlen(line) - 1;
	while (i > 0 && isspace(line[i]))
		line[i--] = '\0';

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

	    got_one++;
	    strcpy(tname, piece_dir);
	    strcat(tname, "/p.XXXXXX");
	    if (mktemp(tname) == NULL)
		{
		err("*mktemp: '%s'", tname);
		status++;
		continue;
		}
	    if ((ofp = fopen(tname, "w")) == NULL)
		{
		err("cannot open '%s' for writing", tname);
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
		err("error writing %s", tname);

	    if (cksum != claimed_cksum)
		err("checksum: read %d, calculated %d", claimed_cksum, cksum);

	    if (e || cksum != claimed_cksum)
		{
		err("%s %d/%d discarded", delta, pce, npieces);
		unlink(tname);
		status++;
		continue;
		}

	    mk_piece_name(pname, delta, pce, npieces);
	    if (rename(tname, pname) < 0)
		{
		err("*rename: '%s' to '%s'", tname, pname);
		err("%s %d/%d lost!", delta, pce, npieces);
		unlink(tname);
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
	if (n <= 0)
	    {
	    err("line %d: illegal character: '%c'", line_no, line[-n]);
	    err("%s %d/%d discarded", delta, pce, npieces);

	    fclose(ofp);
	    unlink(tname);

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
	unlink(tname);

	status++;
	}

    if (ferror(ifp))
	{
	err("error reading %s", input_file == NULL ? "stdin" : input_file);
	status++;
	}

    if (input_file != NULL)
	fclose(ifp);

    if (!got_one)
	{
	err("message contains no delta");
	status++;
	}

    return (status != 0);
    }


/*
 * Put the pieces together to form a delta, if they are all present.
 * Returns 1 on success (even if we didn't do anything), and 0 on failure.
 */
int
combine_if_complete(char *delta, int pce, int npieces)
    {
    int i, e;
    int lfd;
    struct stat sb;
    char pname[PATH_MAX];
    char dname[PATH_MAX];
    char tname[PATH_MAX];

    /*
     * We can probably just rename() it into place if it is a small delta.
     */
    if (npieces == 1)
	{
	mk_delta_name(dname, delta);
	mk_piece_name(pname, delta, 1, 1);
	if (rename(pname, dname) == 0)
	    {
	    err("%s complete", delta);
	    return 1;
	    }
	}

    /*
     * Grab a lock on the reassembly mutex file so that we can be sure we are
     * working alone, not fighting another ctm_rmail!
     */
    strcpy(tname, delta_dir);
    strcat(tname, "/.mutex_build");
    if ((lfd = lock_file(tname)) < 0)
	return 0;

    /*
     * Are all of the pieces present?  Of course the current one is,
     * unless all pieces are missing because another ctm_rmail has
     * processed them already.
     */
    for (i = 1; i <= npieces; i++)
	{
	if (i == pce)
	    continue;
	mk_piece_name(pname, delta, i, npieces);
	if (stat(pname, &sb) < 0)
	    {
	    close(lfd);
	    return 1;
	    }
	}

    /*
     * Stick them together.  Let combine() use our file name buffers, since
     * we're such good buddies. :-)
     */
    e = combine(delta, npieces, dname, pname, tname);
    close(lfd);
    return e;
    }


/*
 * Put the pieces together to form a delta.
 * Returns 1 on success, and 0 on failure.
 * Note: dname, pname, and tname are room for some file names that just
 * happened to by lying around in the calling routine.  Waste not, want not!
 */
int
combine(char *delta, int npieces, char *dname, char *pname, char *tname)
    {
    FILE *dfp, *pfp;
    int i, n, e;
    char buf[BUFSIZ];

    strcpy(tname, delta_dir);
    strcat(tname, "/d.XXXXXX");
    if (mktemp(tname) == NULL)
	{
	err("*mktemp: '%s'", tname);
	return 0;
	}
    if ((dfp = fopen(tname, "w")) == NULL)
	{
	err("cannot open '%s' for writing", tname);
	return 0;
	}

    /*
     * Reconstruct the delta by reading each piece in order.
     */
    for (i = 1; i <= npieces; i++)
	{
	mk_piece_name(pname, delta, i, npieces);
	if ((pfp = fopen(pname, "r")) == NULL)
	    {
	    err("cannot open '%s' for reading", pname);
	    fclose(dfp);
	    unlink(tname);
	    return 0;
	    }
	while ((n = fread(buf, sizeof(char), sizeof(buf), pfp)) != 0)
	    fwrite(buf, sizeof(char), n, dfp);
	e = ferror(pfp);
	fclose(pfp);
	if (e)
	    {
	    err("error reading '%s'", pname);
	    fclose(dfp);
	    unlink(tname);
	    return 0;
	    }
	}
    fflush(dfp);
    e = ferror(dfp);
    fclose(dfp);
    if (e)
	{
	err("error writing '%s'", tname);
	unlink(tname);
	return 0;
	}

    mk_delta_name(dname, delta);
    if (rename(tname, dname) < 0)
	{
	err("*rename: '%s' to '%s'", tname, dname);
	unlink(tname);
	return 0;
	}

    /*
     * Throw the pieces away.
     */
    for (i = 1; i <= npieces; i++)
	{
	mk_piece_name(pname, delta, i, npieces);
	if (unlink(pname) < 0)
	    err("*unlink: '%s'", pname);
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


/*
 * Create and lock the given file.
 *
 * Clearing the lock is as simple as closing the file descriptor we return.
 */
int
lock_file(char *name)
    {
    int lfd;

    if ((lfd = open(name, O_WRONLY|O_CREAT, 0600)) < 0)
	{
	err("*open: '%s'", name);
	return -1;
	}
    if (flock(lfd, LOCK_EX) < 0)
	{
	close(lfd);
	err("*flock: '%s'", name);
	return -1;
	}
    return lfd;
    }
