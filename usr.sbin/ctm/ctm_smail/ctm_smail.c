/*
 * Send a compressed CTM delta to a recipient mailing list by encoding it
 * in safe ASCII characters, in mailer-friendly chunks, and passing them
 * to sendmail.  Optionally, the chunks can be queued to be sent later by
 * ctm_dequeue in controlled bursts.  The encoding is almost the same as
 * MIME BASE64, and is protected by a simple checksum.
 *
 * Author: Stephen McKay
 *
 * NOTICE: This is free software.  I hope you get some use from this program.
 * In return you should think about all the nice people who give away software.
 * Maybe you should write some free software too.
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <paths.h>
#include <limits.h>
#include "error.h"
#include "options.h"

#define DEF_MAX_MSG	64000	/* Default maximum mail msg minus headers. */

#define LINE_LENGTH	72	/* Chars per encoded line. Divisible by 4. */

int chop_and_send_or_queue(FILE *dfp, char *delta, off_t ctm_size,
	long max_msg_size, char *mail_alias, char *queue_dir);
int chop_and_send(FILE *dfp, char *delta, long msg_size, int npieces,
	char *mail_alias);
int chop_and_queue(FILE *dfp, char *delta, long msg_size, int npieces,
	char *mail_alias, char *queue_dir);
void clean_up_queue(char *queue_dir);
int encode_body(FILE *sm_fp, FILE *delta_fp, long msg_size, unsigned *sum);
void write_header(FILE *sfp, char *mail_alias, char *delta, int pce,
	int npieces);
void write_trailer(FILE *sfp, unsigned sum);
int apologise(char *delta, off_t ctm_size, long max_ctm_size,
	char *mail_alias, char *queue_dir);
FILE *open_sendmail(void);
int close_sendmail(FILE *fp);

int
main(int argc, char **argv)
    {
    int status = 0;
    char *delta_file;
    char *mail_alias;
    long max_msg_size = DEF_MAX_MSG;
    long max_ctm_size = 0;
    char *log_file = NULL;
    char *queue_dir = NULL;
    char *delta;
    FILE *dfp;
    struct stat sb;

    err_prog_name(argv[0]);

    OPTIONS("[-l log] [-m maxmsgsize] [-c maxctmsize] [-q queuedir] ctm-delta mail-alias")
	NUMBER('m', max_msg_size)
	NUMBER('c', max_ctm_size)
	STRING('l', log_file)
	STRING('q', queue_dir)
    ENDOPTS

    if (argc != 3)
	usage();

    if (log_file != NULL)
	err_set_log(log_file);

    delta_file = argv[1];
    mail_alias = argv[2];

    if ((delta = strrchr(delta_file, '/')) == NULL)
	delta = delta_file;
    else
	delta++;

    if ((dfp = fopen(delta_file, "r")) == NULL || fstat(fileno(dfp), &sb) < 0)
	{
	err("*%s", delta_file);
	exit(1);
	}

    if (max_ctm_size != 0 && sb.st_size > max_ctm_size)
	status = apologise(delta, sb.st_size, max_ctm_size, mail_alias,
		queue_dir);
    else
	status = chop_and_send_or_queue(dfp, delta, sb.st_size, max_msg_size,
		mail_alias, queue_dir);

    fclose(dfp);

    return status;
    }


/*
 * Carve our CTM delta into pieces, encode them, and send or queue them.
 * Returns 0 on success, and 1 on failure.
 */
int
chop_and_send_or_queue(FILE *dfp, char *delta, off_t ctm_size,
	long max_msg_size, char *mail_alias, char *queue_dir)
    {
    int npieces;
    long msg_size;
    long exp_size;
    int status;

#undef howmany
#define	howmany(x,y)	(((x)+((y)-1)) / (y))

    /*
     * Work out how many pieces we need, bearing in mind that each piece
     * grows by 4/3 when encoded.  We count the newlines too, but ignore
     * all mail headers and piece headers.  They are a "small" (almost
     * constant) per message overhead that we make the user worry about. :-)
     */
    exp_size = ctm_size * 4 / 3;
    exp_size += howmany(exp_size, LINE_LENGTH);
    npieces = howmany(exp_size, max_msg_size);
    msg_size = howmany(ctm_size, npieces);

#undef howmany

    if (queue_dir == NULL)
	status = chop_and_send(dfp, delta, msg_size, npieces, mail_alias);
    else
	{
	status = chop_and_queue(dfp, delta, msg_size, npieces, mail_alias,
		queue_dir);
	if (status)
	    clean_up_queue(queue_dir);
	}

    return status;
    }


/*
 * Carve our CTM delta into pieces, encode them, and send them.
 * Returns 0 on success, and 1 on failure.
 */
int
chop_and_send(FILE *dfp, char *delta, long msg_size, int npieces,
	char *mail_alias)
    {
    int pce;
    FILE *sfp;
    unsigned sum;

    /*
     * Send each chunk directly to sendmail as it is generated.
     * No temporary files necessary.  If things turn ugly, we just
     * have to live with the fact the we have sent only part of
     * the delta.
     */
    for (pce = 1; pce <= npieces; pce++)
	{
	int read_error;

	if ((sfp = open_sendmail()) == NULL)
	    return 1;

	write_header(sfp, mail_alias, delta, pce, npieces);
	read_error = encode_body(sfp, dfp, msg_size, &sum);
	if (!read_error)
	    write_trailer(sfp, sum);

	if (!close_sendmail(sfp) || read_error)
	    return 1;

	err("%s %d/%d sent to %s", delta, pce, npieces, mail_alias);
	}

    return 0;
    }


/*
 * Construct the tmp queue file name of a delta piece.
 */
#define mk_tmp_name(fn,qd,p) \
    sprintf((fn), "%s/.%08ld.%03d", (qd), (long)getpid(), (p))

/*
 * Construct the final queue file name of a delta piece.
 */
#define mk_queue_name(fn,qd,d,p,n) \
    sprintf((fn), "%s/%s+%03d-%03d", (qd), (d), (p), (n))

/*
 * Carve our CTM delta into pieces, encode them, and queue them.
 * Returns 0 on success, and 1 on failure.
 */
int
chop_and_queue(FILE *dfp, char *delta, long msg_size, int npieces,
	char *mail_alias, char *queue_dir)
    {
    int pce;
    FILE *qfp;
    unsigned sum;
    char tname[PATH_MAX];
    char qname[PATH_MAX];

    /*
     * Store each piece in the queue directory, but under temporary names,
     * so that they can be deleted without unpleasant consequences if
     * anything goes wrong.  We could easily fill up a disk, for example.
     */
    for (pce = 1; pce <= npieces; pce++)
	{
	int write_error;

	mk_tmp_name(tname, queue_dir, pce);
	if ((qfp = fopen(tname, "w")) == NULL)
	    {
	    err("cannot open '%s' for writing", tname);
	    return 1;
	    }

	write_header(qfp, mail_alias, delta, pce, npieces);
	if (encode_body(qfp, dfp, msg_size, &sum))
	    return 1;
	write_trailer(qfp, sum);

	fflush(qfp);
	write_error = ferror(qfp);
	fclose(qfp);
	if (write_error)
	    {
	    err("error writing '%s'", tname);
	    return 1;
	    }

	/*
	 * Give the warm success message now, instead of all in a rush
	 * during the rename phase.
	 */
	err("%s %d/%d queued for %s", delta, pce, npieces, mail_alias);
	}

    /*
     * Rename the pieces into place.  If an error occurs now, we are
     * stuffed, but there is no neat way to back out.  rename() should
     * only fail now under extreme circumstances.
     */
    for (pce = 1; pce <= npieces; pce++)
	{
	mk_tmp_name(tname, queue_dir, pce);
	mk_queue_name(qname, queue_dir, delta, pce, npieces);
	if (rename(tname, qname) < 0)
	    {
	    err("*rename: '%s' to '%s'", tname, qname);
	    unlink(tname);
	    }
	}

    return 0;
    }


/*
 * There may be temporary files cluttering up the queue directory.
 */
void
clean_up_queue(char *queue_dir)
    {
    int pce;
    char tname[PATH_MAX];

    err("discarding queued delta pieces");
    for (pce = 1; ; pce++)
	{
	mk_tmp_name(tname, queue_dir, pce);
	if (unlink(tname) < 0)
	    break;
	}
    }


/*
 * MIME BASE64 encode table.
 */
static char to_b64[0x40] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * This cheap plastic checksum effectively rotates our checksum-so-far
 * left one, then adds the character.  We only want 16 bits of it, and
 * don't care what happens to the rest.  It ain't much, but it's small.
 */
#define add_ck(sum,x)	\
    ((sum) += ((x)&0xff) + (sum) + (((sum)&0x8000) ? 1 : 0))

/*
 * Encode the body.  Use an encoding almost the same as MIME BASE64.
 *
 * Characters are read from delta_fp and encoded characters are written
 * to sm_fp.  At most 'msg_size' characters should be read from delta_fp.
 *
 * The body consists of lines of up to LINE_LENGTH characters.  Each group
 * of 4 characters encodes 3 input characters.  Each output character encodes
 * 6 bits.  Thus 64 different characters are needed in this representation.
 */
int
encode_body(FILE *sm_fp, FILE *delta_fp, long msg_size, unsigned *sum)
    {
    unsigned short cksum = 0xffff;
    unsigned char *ip;
    char *op;
    int want, n, i;
    unsigned char inbuf[LINE_LENGTH*3/4];
    char outbuf[LINE_LENGTH+1];

    /*
     * Round up to the nearest line boundary, for the tiniest of gains,
     * and lots of neatness. :-)
     */
    msg_size += (LINE_LENGTH*3/4) - 1;
    msg_size -= msg_size % (LINE_LENGTH*3/4);

    while (msg_size > 0)
	{
	want = (msg_size < sizeof(inbuf)) ? msg_size : sizeof(inbuf);
	if ((n = fread(inbuf, sizeof(char), want, delta_fp)) == 0)
	    break;
	msg_size -= n;

	for (i = 0; i < n; i++)
	    add_ck(cksum, inbuf[i]);

	/*
	 * Produce a line of encoded data.  Every line length will be a
	 * multiple of 4, except for, perhaps, the last line.
	 */
	ip = inbuf;
	op = outbuf;
	while (n >= 3)
	    {
	    *op++ = to_b64[ip[0] >> 2];
	    *op++ = to_b64[(ip[0] << 4 & 0x3f) | ip[1] >> 4];
	    *op++ = to_b64[(ip[1] << 2 & 0x3f) | ip[2] >> 6];
	    *op++ = to_b64[ip[2] & 0x3f];
	    ip += 3;
	    n -= 3;
	    }
	if (n > 0)
	    {
	    *op++ = to_b64[ip[0] >> 2];
	    *op++ = to_b64[(ip[0] << 4 & 0x3f) | ip[1] >> 4];
	    if (n >= 2)
		*op++ = to_b64[ip[1] << 2 & 0x3f];
	    }
	*op++ = '\n';
	fwrite(outbuf, sizeof(char), op - outbuf, sm_fp);
	}

    if (ferror(delta_fp))
	{
	err("error reading input file.");
	return 1;
	}

    *sum = cksum;

    return 0;
    }


/*
 * Write the mail header and data header.
 */
void
write_header(FILE *sfp, char *mail_alias, char *delta, int pce, int npieces)
    {
    fprintf(sfp, "From: owner-%s\n", mail_alias);
    fprintf(sfp, "To: %s\n", mail_alias);
    fprintf(sfp, "Subject: ctm-mail %s %d/%d\n\n", delta, pce, npieces);

    fprintf(sfp, "CTM_MAIL BEGIN %s %d %d\n", delta, pce, npieces);
    }


/*
 * Write the data trailer.
 */
void
write_trailer(FILE *sfp, unsigned sum)
    {
    fprintf(sfp, "CTM_MAIL END %ld\n", (long)sum);
    }


/*
 * We're terribly sorry, but the delta is too big to send.
 * Returns 0 on success, 1 on failure.
 */
int
apologise(char *delta, off_t ctm_size, long max_ctm_size, char *mail_alias,
	char *queue_dir)
    {
    FILE *sfp;
    char qname[PATH_MAX];

    if (queue_dir == NULL)
	{
	sfp = open_sendmail();
	if (sfp == NULL)
	    return 1;
	}
    else
	{
	mk_queue_name(qname, queue_dir, delta, 1, 1);
	sfp = fopen(qname, "w");
	if (sfp == NULL)
	    {
	    err("cannot open '%s' for writing", qname);
	    return 1;
	    }
	}


    fprintf(sfp, "From: owner-%s\n", mail_alias);
    fprintf(sfp, "To: %s\n", mail_alias);
    fprintf(sfp, "Subject: ctm-notice %s\n\n", delta);

    fprintf(sfp, "%s is %ld bytes.  The limit is %ld bytes.\n\n", delta,
	(long)ctm_size, max_ctm_size);
    fprintf(sfp, "You can retrieve this delta via ftp.\n");

    if (queue_dir == NULL)
	{
	if (!close_sendmail(sfp))
	    return 1;
	}
    else
	{
	if (fclose(sfp)!=0)
	    {
	    err("error writing '%s'", qname);
	    unlink(qname);
	    return 1;
            }
	}

    return 0;
    }


/*
 * Start a pipe to sendmail.  Sendmail will decode the destination
 * from the message contents.
 */
FILE *
open_sendmail()
    {
    FILE *fp;
    char buf[100];

    sprintf(buf, "%s -odq -t", _PATH_SENDMAIL);
    if ((fp = popen(buf, "w")) == NULL)
	err("cannot start sendmail");
    return fp;
    }


/*
 * Close a pipe to sendmail.  Sendmail will then do its bit.
 * Return 1 on success, 0 on failure.
 */
int
close_sendmail(FILE *fp)
    {
    int status;

    fflush(fp);
    if (ferror(fp))
	{
	err("error writing to sendmail");
	return 0;
	}

    if ((status = pclose(fp)) != 0)
	err("sendmail failed with status %d", status);

    return (status == 0);
    }
