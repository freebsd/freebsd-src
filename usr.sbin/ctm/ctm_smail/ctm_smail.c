/*
 * Send a compressed CTM delta to a recipient mailing list by encoding it
 * in safe ASCII characters, in mailer-friendly chunks, and passing it
 * to sendmail.  The encoding is almost the same as MIME BASE64, and is
 * protected by a simple checksum.
 *
 * Author: Stephen McKay
 *
 * NOTICE: This is free software.  I hope you get some use from this program.
 * In return you should think about all the nice people who give away software.
 * Maybe you should write some free software too.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <paths.h>
#include "error.h"
#include "options.h"

#define DEF_MAX_MSG	64000	/* Default maximum mail msg minus headers. */

#define LINE_LENGTH	76	/* Chars per encode line. Divisible by 4. */

void chop_and_send(char *delta, off_t ctm_size, long max_msg_size,
	char *mail_alias);
unsigned encode_body(FILE *sm_fp, FILE *delta_fp, long msg_size);
void write_header(FILE *sfp, char *mail_alias, char *delta, int pce,
	int npieces);
void write_trailer(FILE *sfp, unsigned sum);
void apologise(char *delta, off_t ctm_size, long max_ctm_size,
	char *mail_alias);
FILE *open_sendmail(void);
int close_sendmail(FILE *fp);


int
main(int argc, char **argv)
    {
    char *delta_file;
    char *mail_alias;
    long max_msg_size = DEF_MAX_MSG;
    long max_ctm_size = 0;
    char *log_file = NULL;
    struct stat sb;

    err_prog_name(argv[0]);

    OPTIONS("[-l log] [-m maxmsgsize] [-c maxctmsize] ctm-delta mail-alias")
	NUMBER('m', max_msg_size)
	NUMBER('c', max_ctm_size)
	STRING('l', log_file)
    ENDOPTS

    if (argc != 3)
	usage();

    if (log_file != NULL)
	err_set_log(log_file);

    delta_file = argv[1];
    mail_alias = argv[2];

    if (stat(delta_file, &sb) < 0)
	{
	err("%s: %s", delta_file, strerror(errno));
	exit(1);
	}

    if (max_ctm_size != 0 && sb.st_size > max_ctm_size)
	apologise(delta_file, sb.st_size, max_ctm_size, mail_alias);
    else
	chop_and_send(delta_file, sb.st_size, max_msg_size, mail_alias);

    return 0;
    }


/*
 * Carve our CTM delta into pieces, encode them, and send them.
 */
void
chop_and_send(char *delta, off_t ctm_size, long max_msg_size, char *mail_alias)
    {
    int npieces;
    long msg_size;
    long exp_size;
    int pce;
    FILE *sfp;
    FILE *dfp;
    unsigned sum;

#define	howmany(x, y)	(((x) + ((y) - 1)) / (y))

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

    if ((dfp = fopen(delta, "r")) == NULL)
	{
	err("cannot open '%s' for reading.", delta);
	exit(1);
	}

    for (pce = 1; pce <= npieces; pce++)
	{
	sfp = open_sendmail();
	if (sfp == NULL)
	    exit(1);
	write_header(sfp, mail_alias, delta, pce, npieces);
	sum = encode_body(sfp, dfp, msg_size);
	write_trailer(sfp, sum);
	if (!close_sendmail(sfp))
	    exit(1);
	err("%s %d/%d sent to %s", delta, pce, npieces, mail_alias);
	}

    fclose(dfp);
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
unsigned
encode_body(FILE *sm_fp, FILE *delta_fp, long msg_size)
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
	exit(1);
	}

    if (ferror(sm_fp))
	{
	err("error writing to sendmail");
	exit(1);
	}

    return cksum;
    }


/*
 * Write the mail header and data header.
 */
void
write_header(FILE *sfp, char *mail_alias, char *delta, int pce, int npieces)
    {
    char *sn;

    if ((sn = strrchr(delta, '/')) == NULL)
	sn = delta;
    else
	sn++;

    fprintf(sfp, "From: owner-%s\n", mail_alias);
    fprintf(sfp, "To: %s\n", mail_alias);
    fprintf(sfp, "Subject: ctm-mail %s %d/%d\n\n", sn, pce, npieces);

    fprintf(sfp, "CTM_MAIL BEGIN %s %d %d\n", sn, pce, npieces);
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
 */
void
apologise(char *delta, off_t ctm_size, long max_ctm_size, char *mail_alias)
    {
    FILE *sfp;
    char *sn;

    sfp = open_sendmail();
    if (sfp == NULL)
	exit(1);

    if ((sn = strrchr(delta, '/')) == NULL)
	sn = delta;
    else
	sn++;

    fprintf(sfp, "From: %s-owner\n", mail_alias);
    fprintf(sfp, "To: %s\n", mail_alias);
    fprintf(sfp, "Subject: ctm-notice %s\n\n", sn);

    fprintf(sfp, "%s is %ld bytes.  The limit is %ld bytes.\n\n", sn,
	(long)ctm_size, max_ctm_size);
    fprintf(sfp, "You can retrieve this delta via ftpmail, or your good mate at the university.\n");

    if (!close_sendmail(sfp))
	exit(1);
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

    sprintf(buf, "%s -t", _PATH_SENDMAIL);
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
