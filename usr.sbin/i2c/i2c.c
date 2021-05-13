/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2008-2009 Semihalf, Michal Hajduk and Bartlomiej Sieka
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <sysexits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/endian.h>
#include <sys/ioctl.h>

#include <dev/iicbus/iic.h>

#define	I2C_DEV			"/dev/iic0"
#define	I2C_MODE_NOTSET		0
#define	I2C_MODE_NONE		1
#define	I2C_MODE_STOP_START	2
#define	I2C_MODE_REPEATED_START	3
#define	I2C_MODE_TRANSFER	4

struct options {
	const char	*width;
	int		count;
	int		verbose;
	int		binary;
	const char	*skip;
	int		mode;
	char		dir;
	uint32_t	addr;
	uint32_t	off;
	uint8_t		off_buf[2];
	size_t		off_len;
};

__dead2 static void
usage(const char *msg)
{

	if (msg != NULL)
		fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "usage: %s -a addr [-f device] [-d [r|w]] [-o offset] "
	    "[-w [0|8|16|16LE|16BE]] [-c count] [-m [tr|ss|rs|no]] [-b] [-v]\n",
	    getprogname());
	fprintf(stderr, "       %s -s [-f device] [-n skip_addr] -v\n",
	    getprogname());
	fprintf(stderr, "       %s -r [-f device] -v\n", getprogname());
	exit(EX_USAGE);
}

static void
parse_skip(const char *skip, char blacklist[128])
{
	const char *p;
	unsigned x, y;

	for (p = skip; *p != '\0';) {
		if (*p == '0' && p[1] == 'x')
			p += 2;
		if (!isxdigit(*p))
			usage("Bad -n argument, expected (first) hex-digit");
		x = digittoint(*p++) << 4;
		if (!isxdigit(*p))
			usage("Bad -n argument, expected (second) hex-digit");
		x |= digittoint(*p++);
		if (x == 0 || x > 0x7f)
			usage("Bad -n argument, (01..7f)");
		if (*p == ':' || *p == ',' || *p == '\0') {
			blacklist[x] = 1;
			if (*p != '\0')
				p++;
			continue;
		}
		if (*p == '-') {
			p++;
		} else if (*p == '.' && p[1] == '.') {
			p += 2;
		} else {
			usage("Bad -n argument, ([:|,|..|-])");
		}
		if (*p == '0' && p[1] == 'x')
			p += 2;
		if (!isxdigit(*p))
			usage("Bad -n argument, expected (first) hex-digit");
		y = digittoint(*p++) << 4;
		if (!isxdigit(*p))
			usage("Bad -n argument, expected (second) hex-digit");
		y |= digittoint(*p++);
		if (y == 0 || y > 0x7f)
			usage("Bad -n argument, (01..7f)");
		if (y < x)
			usage("Bad -n argument, (end < start)");
		for (;x <= y; x++)
			blacklist[x] = 1;
		if (*p == ':' || *p == ',')
			p++;
	}
}

static int
scan_bus(const char *dev, int fd, const char *skip)
{
	struct iiccmd cmd;
	struct iic_msg rdmsg;
	struct iic_rdwr_data rdwrdata;
	int error;
	unsigned u;
	int num_found = 0, use_read_xfer;
	uint8_t rdbyte;
	char blacklist[128];

	memset(blacklist, 0, sizeof blacklist);

	if (skip != NULL)
		parse_skip(skip, blacklist);

	printf("Scanning I2C devices on %s:", dev);

	for (use_read_xfer = 0; use_read_xfer < 2; use_read_xfer++) {
		for (u = 1; u < 127; u++) {
			if (blacklist[u])
				continue;

			cmd.slave = u << 1;
			cmd.last = 1;
			cmd.count = 0;
			error = ioctl(fd, I2CRSTCARD, &cmd);
			if (error) {
				fprintf(stderr, "Controller reset failed\n");
				fprintf(stderr,
				    "Error scanning I2C controller (%s): %s\n",
				    dev, strerror(errno));
				return (EX_NOINPUT);
			}
			if (use_read_xfer) {
				rdmsg.buf = &rdbyte;
				rdmsg.len = 1;
				rdmsg.flags = IIC_M_RD;
				rdmsg.slave = u << 1;
				rdwrdata.msgs = &rdmsg;
				rdwrdata.nmsgs = 1;
				error = ioctl(fd, I2CRDWR, &rdwrdata);
			} else {
				cmd.slave = u << 1;
				cmd.last = 1;
				error = ioctl(fd, I2CSTART, &cmd);
				if (errno == ENODEV || errno == EOPNOTSUPP)
					break; /* Try reads instead */
				(void)ioctl(fd, I2CSTOP);
			}
			if (error == 0) {
				++num_found;
				printf(" %02x", u);
			}
		}
		if (num_found > 0)
			break;
		fprintf(stderr,
		    "Hardware may not support START/STOP scanning; "
		    "trying less-reliable read method.\n");
	}
	if (num_found == 0)
		printf("<none found>");

	printf("\n");

	error = ioctl(fd, I2CRSTCARD, &cmd);
	if (error)
		fprintf(stderr, "Controller reset failed\n");
	return (EX_OK);
}

static int
reset_bus(const char *dev, int fd)
{
	struct iiccmd cmd;
	int error;

	printf("Resetting I2C controller on %s: ", dev);
	error = ioctl(fd, I2CRSTCARD, &cmd);

	if (error) {
		printf("error: %s\n", strerror(errno));
		return (EX_IOERR);
	} else {
		printf("OK\n");
		return (EX_OK);
	}
}

static const char *
encode_offset(const char *width, unsigned address, uint8_t *dst, size_t *len)
{

	if (!strcmp(width, "0")) {
		*len = 0;
		return (NULL);
	}
	if (!strcmp(width, "8")) {
		if (address > 0xff)
			return ("Invalid 8-bit address\n");
		*dst = address;
		*len = 1;
		return (NULL);
	}
	if (address > 0xffff)
		return ("Invalid 16-bit address\n");
	if (!strcmp(width, "16LE") || !strcmp(width, "16")) {
		le16enc(dst, address);
		*len = 2;
		return (NULL);
	}
	if (!strcmp(width, "16BE")) {
		be16enc(dst, address);
		*len = 2;
		return (NULL);
	}
	return ("Invalid offset width, must be: 0|8|16|16LE|16BE\n");
}

static const char *
write_offset(int fd, struct options i2c_opt, struct iiccmd *cmd)
{
	int error;

	if (i2c_opt.off_len > 0) {
		cmd->count = i2c_opt.off_len;
		cmd->buf = (void*)i2c_opt.off_buf;
		error = ioctl(fd, I2CWRITE, cmd);
		if (error == -1)
			return ("ioctl: error writing offset\n");
	}
	return (NULL);
}

static int
i2c_write(int fd, struct options i2c_opt, char *i2c_buf)
{
	struct iiccmd cmd;
	int error;
	char *buf;
	const char *err_msg;

	cmd.slave = i2c_opt.addr;
	error = ioctl(fd, I2CSTART, &cmd);
	if (error == -1) {
		err_msg = "ioctl: error sending start condition";
		goto err1;
	}

	switch(i2c_opt.mode) {
	case I2C_MODE_STOP_START:
		err_msg = write_offset(fd, i2c_opt, &cmd);
		if (err_msg != NULL)
			goto err1;

		error = ioctl(fd, I2CSTOP);
		if (error == -1) {
			err_msg = "ioctl: error sending stop condition";
			goto err2;
		}
		cmd.slave = i2c_opt.addr;
		error = ioctl(fd, I2CSTART, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error sending start condition";
			goto err1;
		}

		/*
		 * Write the data
		 */
		cmd.count = i2c_opt.count;
		cmd.buf = i2c_buf;
		cmd.last = 0;
		error = ioctl(fd, I2CWRITE, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error writing";
			goto err1;
		}
		break;

	case I2C_MODE_REPEATED_START:
		err_msg = write_offset(fd, i2c_opt, &cmd);
		if (err_msg != NULL)
			goto err1;

		cmd.slave = i2c_opt.addr;
		error = ioctl(fd, I2CRPTSTART, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error sending repeated start "
			    "condition";
			goto err1;
		}

		/*
		 * Write the data
		 */
		cmd.count = i2c_opt.count;
		cmd.buf = i2c_buf;
		cmd.last = 0;
		error = ioctl(fd, I2CWRITE, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error writing";
			goto err1;
		}
		break;

	case I2C_MODE_NONE: /* fall through */
	default:
		buf = malloc(i2c_opt.off_len + i2c_opt.count);
		if (buf == NULL) {
			err_msg = "error: data malloc";
			goto err1;
		}
		memcpy(buf, i2c_opt.off_buf, i2c_opt.off_len);

		memcpy(buf + i2c_opt.off_len, i2c_buf, i2c_opt.count);
		/*
		 * Write offset and data
		 */
		cmd.count = i2c_opt.off_len + i2c_opt.count;
		cmd.buf = buf;
		cmd.last = 0;
		error = ioctl(fd, I2CWRITE, &cmd);
		free(buf);
		if (error == -1) {
			err_msg = "ioctl: error writing";
			goto err1;
		}
		break;
	}
	error = ioctl(fd, I2CSTOP);
	if (error == -1) {
		err_msg = "ioctl: error sending stop condition";
		goto err2;
	}

	return (0);

err1:
	error = ioctl(fd, I2CSTOP);
	if (error == -1)
		fprintf(stderr, "error sending stop condition\n");
err2:
	if (err_msg)
		fprintf(stderr, "%s\n", err_msg);

	return (1);
}

static int
i2c_read(int fd, struct options i2c_opt, char *i2c_buf)
{
	struct iiccmd cmd;
	int error;
	char data = 0;
	const char *err_msg;

	bzero(&cmd, sizeof(cmd));

	if (i2c_opt.off_len) {
		cmd.slave = i2c_opt.addr;
		cmd.count = 1;
		cmd.last = 0;
		cmd.buf = &data;
		error = ioctl(fd, I2CSTART, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error sending start condition";
			goto err1;
		}

		err_msg = write_offset(fd, i2c_opt, &cmd);
		if (err_msg != NULL)
			goto err1;

		if (i2c_opt.mode == I2C_MODE_STOP_START) {
			error = ioctl(fd, I2CSTOP);
			if (error == -1) {
				err_msg = "error sending stop condition";
				goto err2;
			}
		}
	}
	cmd.slave = i2c_opt.addr | 1;
	cmd.count = 1;
	cmd.last = 0;
	cmd.buf = &data;
	if (i2c_opt.mode == I2C_MODE_STOP_START || i2c_opt.off_len == 0) {
		error = ioctl(fd, I2CSTART, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error sending start condition";
			goto err2;
		}
	} else if (i2c_opt.mode == I2C_MODE_REPEATED_START) {
		error = ioctl(fd, I2CRPTSTART, &cmd);
		if (error == -1) {
			err_msg = "ioctl: error sending repeated start "
			    "condition";
			goto err1;
		}
	}

	cmd.count = i2c_opt.count;
	cmd.buf = i2c_buf;
	cmd.last = 1;
	error = ioctl(fd, I2CREAD, &cmd);
	if (error == -1) {
		err_msg = "ioctl: error while reading";
		goto err1;
	}

	error = ioctl(fd, I2CSTOP);
	if (error == -1) {
		err_msg = "error sending stop condtion\n";
		goto err2;
	}

	return (0);

err1:
	error = ioctl(fd, I2CSTOP);
	if (error == -1)
		fprintf(stderr, "error sending stop condition\n");
err2:
	if (err_msg)
		fprintf(stderr, "%s\n", err_msg);

	return (1);
}

/*
 * i2c_rdwr_transfer() - use I2CRDWR to conduct a complete i2c transfer.
 *
 * Some i2c hardware is unable to provide direct control over START, REPEAT-
 * START, and STOP operations.  Such hardware can only perform a complete
 * START-<data>-STOP or START-<data>-REPEAT-START-<data>-STOP sequence as a
 * single operation.  The driver framework refers to this sequence as a
 * "transfer" so we call it "transfer mode".  We assemble either one or two
 * iic_msg structures to describe the IO operations, and hand them off to the
 * driver to be handled as a single transfer.
 */
static int
i2c_rdwr_transfer(int fd, struct options i2c_opt, char *i2c_buf)
{
	struct iic_msg msgs[2], *msgp = msgs;
	struct iic_rdwr_data xfer;
	int flag = 0;

	if (i2c_opt.off_len) {
		msgp->flags = IIC_M_WR | IIC_M_NOSTOP;
		msgp->slave = i2c_opt.addr;
		msgp->buf   = i2c_opt.off_buf;
		msgp->len   = i2c_opt.off_len;
		msgp++;
		flag = IIC_M_NOSTART;
	}

	/*
	 * If the transfer direction is write and we did a write of the offset
	 * above, then we need to elide the start; this transfer is just more
	 * writing that follows the one started above.  For a read, we always do
	 * a start; if we did an offset write above it'll be a repeat-start
	 * because of the NOSTOP flag used above.
	 */
	if (i2c_opt.dir == 'w')
		msgp->flags = IIC_M_WR | flag;
	else
		msgp->flags = IIC_M_RD;
	msgp->slave = i2c_opt.addr;
	msgp->len   = i2c_opt.count;
	msgp->buf   = i2c_buf;
	msgp++;

	xfer.msgs = msgs;
	xfer.nmsgs = msgp - msgs;

	if (ioctl(fd, I2CRDWR, &xfer) == -1 )
		err(1, "ioctl(I2CRDWR) failed");

	return (0);
}

static int
access_bus(int fd, struct options i2c_opt)
{
	char *i2c_buf;
	int error, chunk_size = 16, i, ch;

	i2c_buf = malloc(i2c_opt.count);
	if (i2c_buf == NULL)
		err(1, "data malloc");

	/*
	 * For a write, read the data to be written to the chip from stdin.
	 */
	if (i2c_opt.dir == 'w') {
		if (i2c_opt.verbose && !i2c_opt.binary)
			fprintf(stderr, "Enter %d bytes of data: ",
			    i2c_opt.count);
		for (i = 0; i < i2c_opt.count; i++) {
			ch = getchar();
			if (ch == EOF) {
				free(i2c_buf);
				err(1, "not enough data, exiting\n");
			}
			i2c_buf[i] = ch;
		}
	}

	if (i2c_opt.mode == I2C_MODE_TRANSFER)
		error = i2c_rdwr_transfer(fd, i2c_opt, i2c_buf);
	else if (i2c_opt.dir == 'w')
		error = i2c_write(fd, i2c_opt, i2c_buf);
	else
		error = i2c_read(fd, i2c_opt, i2c_buf);

	if (error == 0) {
		if (i2c_opt.dir == 'r' && i2c_opt.binary) {
			(void)fwrite(i2c_buf, 1, i2c_opt.count, stdout);
		} else if (i2c_opt.verbose || i2c_opt.dir == 'r') {
			if (i2c_opt.verbose)
				fprintf(stderr, "\nData %s (hex):\n",
				    i2c_opt.dir == 'r' ?  "read" : "written");

			for (i = 0; i < i2c_opt.count; i++) {
				fprintf (stderr, "%02hhx ", i2c_buf[i]);
				if ((i % chunk_size) == chunk_size - 1)
					fprintf(stderr, "\n");
			}
			if ((i % chunk_size) != 0)
				fprintf(stderr, "\n");
		}
	}

	free(i2c_buf);
	return (error);
}

int
main(int argc, char** argv)
{
	struct options i2c_opt;
	const char *dev, *err_msg;
	int fd, error = 0, ch;
	const char *optflags = "a:f:d:o:w:c:m:n:sbvrh";
	char do_what = 0;

	dev = I2C_DEV;

	/* Default values */
	i2c_opt.off = 0;
	i2c_opt.verbose = 0;
	i2c_opt.dir = 'r';	/* direction = read */
	i2c_opt.width = "8";
	i2c_opt.count = 1;
	i2c_opt.binary = 0;	/* ASCII text output */
	i2c_opt.skip = NULL;	/* scan all addresses */
	i2c_opt.mode = I2C_MODE_NOTSET;

	/* Find out what we are going to do */

	while ((ch = getopt(argc, argv, "a:f:d:o:w:c:m:n:sbvrh")) != -1) {
		switch(ch) {
		case 'a':
		case 'r':
		case 's':
			if (do_what)
				usage("Only one of [-a|-h|-r|-s]");
			do_what = ch;
			break;
		case 'h':
			usage("Help:");
			break;
		default:
			break;
		}
	}

	/* Then handle the legal subset of arguments */

	switch (do_what) {
	case 0: usage("Pick one of [-a|-h|-r|-s]"); break;
	case 'a': optflags = "a:f:d:w:o:c:m:bv"; break;
	case 'r': optflags = "rf:v"; break;
	case 's': optflags = "sf:n:v"; break;
	default: assert("Bad do_what");
	}

	optreset = 1;
	optind = 1;

	while ((ch = getopt(argc, argv, optflags)) != -1) {
		switch(ch) {
		case 'a':
			i2c_opt.addr = strtoul(optarg, 0, 16);
			if (i2c_opt.addr == 0 && errno == EINVAL)
				usage("Bad -a argument (hex)");
			if (i2c_opt.addr == 0 || i2c_opt.addr > 0x7f)
				usage("Bad -a argument (01..7f)");
			i2c_opt.addr <<= 1;
			break;
		case 'f':
			dev = optarg;
			break;
		case 'd':
			if (strcmp(optarg, "r") && strcmp(optarg, "w"))
				usage("Bad -d argument ([r|w])");
			i2c_opt.dir = optarg[0];
			break;
		case 'o':
			i2c_opt.off = strtoul(optarg, 0, 16);
			if (i2c_opt.off == 0 && errno == EINVAL)
				usage("Bad -o argument (hex)");
			break;
		case 'w':
			i2c_opt.width = optarg;		// checked later.
			break;
		case 'c':
			i2c_opt.count = (strtoul(optarg, 0, 10));
			if (i2c_opt.count == 0 && errno == EINVAL)
				usage("Bad -c argument (decimal)");
			break;
		case 'm':
			if (!strcmp(optarg, "no"))
				i2c_opt.mode = I2C_MODE_NONE;
			else if (!strcmp(optarg, "ss"))
				i2c_opt.mode = I2C_MODE_STOP_START;
			else if (!strcmp(optarg, "rs"))
				i2c_opt.mode = I2C_MODE_REPEATED_START;
			else if (!strcmp(optarg, "tr"))
				i2c_opt.mode = I2C_MODE_TRANSFER;
			else
				usage("Bad -m argument ([no|ss|rs|tr])");
			break;
		case 'n':
			i2c_opt.skip = optarg;
			break;
		case 's': break;
		case 'b':
			i2c_opt.binary = 1;
			break;
		case 'v':
			i2c_opt.verbose = 1;
			break;
		case 'r': break;
		default:
			fprintf(stderr, "Illegal -%c option", ch);
			usage(NULL);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage("Too many arguments");

	/* Set default mode if option -m is not specified */
	if (i2c_opt.mode == I2C_MODE_NOTSET) {
		if (i2c_opt.dir == 'r')
			i2c_opt.mode = I2C_MODE_STOP_START;
		else if (i2c_opt.dir == 'w')
			i2c_opt.mode = I2C_MODE_NONE;
	}

	err_msg = encode_offset(i2c_opt.width, i2c_opt.off,
	    i2c_opt.off_buf, &i2c_opt.off_len);
	if (err_msg != NULL) {
		fprintf(stderr, "%s", err_msg);
		exit(EX_USAGE);
	}

	if (i2c_opt.verbose)
		fprintf(stderr, "dev: %s, addr: 0x%x, r/w: %c, "
		    "offset: 0x%02x, width: %s, count: %d\n", dev,
		    i2c_opt.addr >> 1, i2c_opt.dir, i2c_opt.off,
		    i2c_opt.width, i2c_opt.count);

	fd = open(dev, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Error opening I2C controller (%s): %s\n",
		    dev, strerror(errno));
		return (EX_NOINPUT);
	}

	switch (do_what) {
	case 's':
		error = scan_bus(dev, fd, i2c_opt.skip);
		break;
	case 'r':
		error = reset_bus(dev, fd);
		break;
	case 'a':
		error = access_bus(fd, i2c_opt);
		break;
	default:
		assert("Bad do_what");
	}

	ch = close(fd);
	assert(ch == 0);
	return (error);
}
