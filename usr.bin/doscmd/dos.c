/*
 * Copyright (c) 1996
 *	Michael Smith.  All rights reserved.
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI int21.c,v 2.2 1996/04/08 19:32:51 bostic Exp
 *
 * $FreeBSD$
 */

#include "doscmd.h"
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>
#include <time.h>
#include <glob.h>
#include <errno.h>
#include <ctype.h>
#include <stddef.h>

#include "dispatch.h"

static u_long upcase_vector;

/* Country Info */
struct {
        ushort  ciDateFormat;
        char    ciCurrency[5];
        char    ciThousands[2];
        char    ciDecimal[2];
        char    ciDateSep[2];
        char    ciTimeSep[2];
        char    ciCurrencyFormat;
        char    ciCurrencyPlaces;
	char	ciTimeFormat;
	ushort	ciCaseMapOffset;
	ushort	ciCaseMapSegment;
        char    ciDataSep[2];
#if 0
        char    ciReserved[10];
#endif
} countryinfo = {
        0, "$", ",", ".", "-", ":", 0, 2, 0, 0xffff, 0xffff, "?"
};

/* DOS File Control Block */
struct fcb {
	u_char	fcbMagic;
	u_char	fcbResoived[5];
	u_char	fcbAttribute;
        u_char  fcbDriveID;
        u_char  fcbFileName[8];
        u_char  fcbExtent[3];
        u_short fcbCurBlockNo;
        u_short fcbRecSize;
        u_long  fcbFileSize;
        u_short fcbFileDate;     
        u_short fcbFileTime;
        int     fcbReserved;
        int     fcb_fd;                 /* hide UNIX FD here */
        u_char  fcbCurRecNo; 
        u_long  fcbRandomRecNo;
}/* __attribute__((__packed__))*/;

/* exports */
void 		encode_dos_file_time (time_t, u_short *, u_short *);
int		diskdrive = 2;	/* C: */
char		*InDOS;
unsigned long	disk_transfer_addr;

/* locals */
static int	ctrl_c_flag = 0;
static int	return_status = 0;
static int	doserrno = 0;
static int	memory_strategy = 0;	/* first fit (we ignore this) */


static u_char upc_table[0x80] = {
	0x80, 0x9a, 'E',  'A',  0x8e, 'A',  0x8f, 0x80,
	'E',  'E',  'E',  'I',  'I',  'I',  0x8e, 0x8f,
	0x90, 0x92, 0x92, 'O',  0x99, 'O',  'U',  'U', 
	'Y',  0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	'A',  'I',  'O',  'U',  0xa5, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};


/******************************************************************************
** utility functions
*/

static u_char
upcase(u_char c)
{

    if (islower(c))
	return (toupper(c));
    else if (c >= 0x80)
	return (upc_table[c - 0x80]);
    else
	return (c);
}

static void
upcase_entry(regcontext_t *REGS)
{

    R_AL = upcase(R_AL);
}


/*
** Handle the DOS drive info/free space/etc. calls.
*/
static int
int21_free(regcontext_t *REGS)
{
    fsstat_t		fs;
    int			error;
    int			drive;

    /* work out drive */
    switch (R_AH) {
    case 0x1c:
    case 0x36:
	drive = R_DL;
	if (drive)
	    break;
	/* FALLTHROUGH */
    case 0x1b:
	drive = diskdrive;
	break;
    default:
	fatal("int21_free called on unknown function %x\n",R_AH);
    }
    
    error = get_space(drive, &fs);
    if (error)
	return(error);

    R_AL = fs.sectors_cluster;			/* sectors per cluster */
    R_CX = fs.bytes_sector;			/* bytes per sector */
    R_DX = fs.total_clusters;			/* total clusters */

    switch (R_AH) {
    case 0x1b:
    case 0x1c:
	BIOSDATA[0xb4] = 0xf0;			/* "reserved" area, "other media" FAT ID */
	R_DX = 0x40;				/* BIOS data area */
	R_BX = 0xb4;
	break;
	
    case 0x36:
	R_BX = fs.avail_clusters;		/* number of available clusters */
	break;
    }
    return(0);
}

static void
pack_name(u_char *p, u_char *q)
{
    int i;

    for (i = 8; i > 0 && *p != ' '; i--)
	*q++ = *p++;
    p += i;
    if (*p != ' ') {
	*q++ = '.';
	for (i = 3; i > 0 && *p != ' '; i--)
	    *q++ = *p++;
	p += i;
    }
    *q = '\0';
}

static void
dosdir_to_dta(dosdir_t *dosdir, find_block_t *dta)
{

    dta->attr = dosdir->attr;
    dta->time = dosdir->time;
    dta->date = dosdir->date;
    dta->size = dosdir->size;
    pack_name(dosdir->name, dta->name);
}

/* exported */
void
encode_dos_file_time(time_t t, u_short *dosdatep, u_short *dostimep)
{
    struct tm tm;
    
    tm = *localtime(&t);
    *dostimep = (tm.tm_hour << 11) |
	(tm.tm_min << 5) |
	((tm.tm_sec / 2) << 0);
    *dosdatep = ((tm.tm_year - 80) << 9) |
	((tm.tm_mon + 1) << 5) |
	(tm.tm_mday << 0);
}

time_t
decode_dos_file_time(u_short dosdate, u_short dostime)
{
    struct tm tm;
    time_t then;

    tm.tm_hour = (dostime >> 11) & 0x1f;
    tm.tm_min  = (dostime >> 5) & 0x3f;
    tm.tm_sec  = ((dostime >> 0) & 0x1f) * 2;
    tm.tm_year = ((dosdate >> 9) & 0x7f) + 80;
    tm.tm_mon  = ((dosdate >> 5) & 0x0f) - 1;
    tm.tm_mday = (dosdate >> 0) & 0x1f;
    /* tm_wday and tm_yday are ignored. */
    tm.tm_isdst = 0;
    /* tm_gmtoff is ignored. */
    then = mktime(&tm);
    return (then);
}

int
translate_filename(u_char *dname, u_char *uname, int *drivep)
{
    u_char newpath[1024];
    int error;

    if (!strcasecmp(dname, "con")) {
	*drivep = -1;
	strcpy(uname, "/dev/tty");
	return (0);
    }

    /* XXX KLUDGE for EMS support w/o booting DOS */
    /* Really need a better way to handle devices */
    if (!strcasecmp(dname, "emmxxxx0")) {
	*drivep = -1;
	strcpy(uname, "/dev/null");
	return (0);
    }

    error = dos_makepath(dname, newpath);
    if (error)
	return (error);

    error = dos_to_real_path(newpath, uname, drivep);
    if (error)
	return (error);

    return (0);
}

static u_char magic[0x7e] = {
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,

	0x08, 0x0f, 0x06, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x04, 0x04, 0x0f, 0x0e, 0x06,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0f,

	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x06, 0x06, 0x06, 0x0f, 0x0f,

	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x04, 0x0f,
};

#define	isvalid(x)	((magic[x] & 0x01) != 0)
#define	issep(x)	((magic[x] & 0x02) == 0)
#define	iswhite(x)	((magic[x] & 0x04) == 0)

static char *
skipwhite(char *p)
{
	while (iswhite(*p))
		++p;
	return (p);
}

#define	get_drive_letter(x)	((x) - 0x40)

int
parse_filename(int flag, char *str, char *fcb, int *nb)
{
    char *p;
    int ret = 0;
    int i;

    p = str;

    p = skipwhite(p);
    if (flag & 1) {
	if (issep(*p)) {
	    ++p;
	    p = skipwhite(p);
	}
    }

    if (isvalid(*p) && p[1] == ':') {
	*fcb++ = get_drive_letter(upcase(*p));
	p += 2;
    } else if (flag & 2) {
	fcb++;
    } else {
	*fcb++ = 0;	/* default drive */
    }

    i = 8;
    if (isvalid(*p)) {
	for (;;) {
	    if (!isvalid(*p)) {
		for (; i > 0; i--)
		    *fcb++ = ' ';
		break;
	    }
	    if (i > 0) {
		switch (*p) {
		case '*':
		    ret = 1;
		    for (; i > 0; i--)
			*fcb++ = '?';
		    break;
		case '?':
		    ret = 1;
		default:
		    *fcb++ = upcase(*p);
		    i--;
		    break;
		}
	    }
	    ++p;
	}
    } else if (flag & 4) {
	fcb += i;
    } else {
	for (; i > 0; i--)
	    *fcb++ = ' ';
    }

    i = 3;
    if (*p == '.') {
	++p;
	for (;;) {
	    if (!isvalid(*p)) {
		for (; i > 0; i--)
		    *fcb++ = ' ';
		break;
	    }
	    if (i > 0) {
		switch (*p) {
		case '*':
		    ret = 1;
		    for (; i > 0; i--)
			*fcb++ = '?';
		    break;
		case '?':
		    ret = 1;
		default:
		    *fcb++ = upcase(*p);
		    i--;
		    break;
		}
	    }
	    ++p;
	}
    } else if (flag & 8) {
	fcb += i;
    } else {
	for (; i > 0; i--)
	    *fcb++ = ' ';
    }

    for (i = 4; i > 0; i--)
	*fcb++ = 0;	/* filler */

    *nb = p - str;
    return (ret);
}

/******************************************************************************
** int21 functions
*/

/*
** 21:00
** 
** terminate
*/
static int
int21_00(regcontext_t *REGS)
{
    done(REGS,0);
    /* keep `gcc -Wall' happy */
    return(0);
}

/*
** 21:01
**
** read character with echo
*/
static int
int21_01(regcontext_t *REGS)
{
    int		n;
    
    if ((n = tty_read((regcontext_t *)&REGS->sc, TTYF_BLOCKALL)) >= 0)
	R_AL = n;
    return(0);
}

/*
** 21:02
**
** write char to stdout
*/
static int
int21_02(regcontext_t *REGS)
{
    tty_write(R_DL, TTYF_REDIRECT);
    return(0);
}

/*
** 21:06
**
** direct console I/O
**
** (dl) is output char unless 0xff, when we read instead
*/
static int
int21_06(regcontext_t *REGS)
{
    int		n;
    
    /* XXX - should be able to read a file */
    if (R_DL == 0xff) {
	n = tty_read((regcontext_t *)&REGS->sc, TTYF_ECHO|TTYF_REDIRECT);
	if (n < 0) {
	    R_FLAGS |= PSL_Z;		/* nothing available */
	    R_AL = 0;
	} else {
	    R_AL = n;			/* got character */
	    R_FLAGS &= ~PSL_Z;
	}
    } else {
	/* write and return char in %al */
	tty_write(R_DL, TTYF_REDIRECT);
	R_AL = R_DL;
    }
    return(0);
}

/*
** 21:07
**
** direct console input with no echo
*/
static int
int21_07(regcontext_t *REGS)
{
    R_AL = tty_read((regcontext_t *)&REGS->sc,
		    TTYF_BLOCK|TTYF_REDIRECT) & 0xff;
    return(0);
}

/*
** 21:08
**
** character input with no echo
*/
static int
int21_08(regcontext_t *REGS)
{
    int		n;
    
    if ((n = tty_read((regcontext_t *)&REGS->sc,
		      TTYF_BLOCK|TTYF_CTRL|TTYF_REDIRECT)) >= 0)
	R_AL = n;
    return(0);
}

/*
** 21:09
**
** write string to standard out.
**
** We're a little paranoid here; if the string is very long, truncate it.
*/
static int
int21_09(regcontext_t *REGS)
{
    char	*addr;
    int		len;
    
    /* pointer to string */
    addr = (char *)MAKEPTR(R_DS, R_DX);

    /* walk string looking for terminator or overlength */
    for (len = 0; len < 10000; len++, addr++) {
	if (*addr == '$')
	    break;
	tty_write(*addr, TTYF_REDIRECT);
    }
    R_AL = 0x24;
    return(0);
}

/*
** 21:0a
**
** buffered input
*/
static int
int21_0a(regcontext_t *REGS)
{
    unsigned char	*addr;
    int			nbytes,avail;
    int			n;
    
    /* pointer to buffer */
    addr = (unsigned char *)MAKEPTR(R_DS, R_DX);

    /* capacity of buffer */
    avail = addr[0];
    if (avail == 0)		/* no space */
	return(0);
    nbytes = 0;			/* read nothing yet */

    /* read loop */
    while (1) {
	n = tty_read((regcontext_t *)&REGS->sc,
		     TTYF_BLOCK|TTYF_CTRL|TTYF_REDIRECT);
	if (n < 0)			/* end of input */
	    n = '\r';			/* make like CR */
	
	switch (n) {
	case '\r':			/* done */
	    addr[1] = nbytes;
	    addr[nbytes+2] = '\r';
	    addr[nbytes+3] = '\0';	/* XXX is this necessary? */
	    return (0);
	case '\n':			/* ignore */
	case '\0':
	    break;
	case '\b':			/* backspace */
	    if (nbytes > 0) {
		--nbytes;
		tty_write('\b', TTYF_REDIRECT);
		if (addr[nbytes+2] < ' ')
		    tty_write('\b', TTYF_REDIRECT);
	    }	
	    break;
	default:
	    if (nbytes >= (avail-2)) {	/* buffer full */
		tty_write('\007', TTYF_REDIRECT);
	    } else {			/* add to end */
		addr[(nbytes++) +2] = n;
		if (n != '\t' && n < ' ') {
		    tty_write('^', TTYF_REDIRECT);
		    tty_write(n + '@', TTYF_REDIRECT);
		} else
		    tty_write(n, TTYF_REDIRECT);
	    }
	    break;
	}
    }
}

/*
** 21:0b
**
** get stdin status
**
** This is a favorite for camping on, so we do some poll-counting
** here as well.
*/
static int
int21_0b(regcontext_t *REGS)
{
    int		n;
    
    /* XXX this is pretty bogus, actually */
    if (!xmode) {
	R_AL = 0xff;		/* no X mode, always claim data available */
	return(0);
    }
    /* XXX tty_peek is broken */
    n = tty_peek(REGS, poll_cnt ? 0 : TTYF_POLL) ? 0xff : 0;
    if (n < 0)			/* control-break */
	return (0);
    R_AL = n;			/* will be 0 or 0xff */
    if (poll_cnt)
	--poll_cnt;
    return(0);
}

/*
** 21:0c
**
** flush stdin and read using other function
*/
static int
int21_0c(regcontext_t *REGS)
{
    if (xmode)			/* XXX should always flush! */
	tty_flush();
    
    switch(R_AL) {		/* which subfunction? */
    case 0x01:
	return(int21_01(REGS));
    case 0x06:
	return(int21_06(REGS));
    case 0x07:
	return(int21_07(REGS));
    case 0x08:
	return(int21_08(REGS));
    case 0x0a:
	return(int21_0a(REGS));
    }
    return(0);
}

/*
** 21:0e
**
** select default drive
*/
static int
int21_0e(regcontext_t *REGS)
{
    diskdrive = R_DL;		/* XXX rangecheck? */
    R_AL = ndisks + 2;		/* report actual limit */
    return(0);
}

/*
** 21:19
**
** get default drive
*/
static int
int21_19(regcontext_t *REGS)
{
    R_AL = diskdrive;
    return(0);
}

/*
** 21:1a
**
** set DTA
*/
static int
int21_1a(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "set dta to %x:%x\n", R_DS, R_DX);
    disk_transfer_addr = MAKEVEC(R_DS, R_DX);
    return(0);
}

/*
** 21:23
**
** Get file size for fcb
** DS:DX -> unopened FCB, no wildcards.
** pcb random record field filled in with number of records, rounded up.
*/
static int
int21_23(regcontext_t *REGS)
{
    debug(D_HALF, "Returning failure from get file size for fcb 21:23\n");
    R_AL = 0xff;
    return(0);
}

/*
** 21:25
**
** set interrupt vector
*/
static int
int21_25(regcontext_t *REGS)
{
    debug(D_MEMORY, "%02x -> %04x:%04x\n", R_AL, R_DS, R_DX);
    ivec[R_AL] = MAKEVEC(R_DS, R_DX);
    return(0);
}

/*
** 21:26
**
** Create PSP
*/
static int
int21_26(regcontext_t *REGS)
{
    unsigned char	*addr;

    /* address of new PSP */
    addr = (unsigned char *)MAKEPTR(R_DX, 0);

    /* copy this process' PSP - XXX needs some work 8( */
    memcpy (addr, dosmem, 256);
    return(0);
}

/*
** 21:2a
**
** Get date
*/
static int
int21_2a(regcontext_t *REGS)
{
    struct timeval	tv;
    struct timezone	tz;
    struct tm		tm;
    time_t		now;
    
    gettimeofday(&tv, &tz);	/* get time and apply DOS offset */
    now = tv.tv_sec + delta_clock;
    
    tm = *localtime(&now);	/* deconstruct and timezoneify */
    R_CX = tm.tm_year + 1900;
    R_DH =  tm.tm_mon + 1;
    R_DL = tm.tm_mday;
    R_AL = tm.tm_wday;
    return(0);
}

/*
** 21:2b
**
** set date
*/
static int
int21_2b(regcontext_t *REGS)
{
    struct timeval	tv;
    struct timezone	tz;
    struct tm		tm;
    time_t		now;
    
    gettimeofday(&tv, &tz);	/* get time and apply DOS offset */
    now = tv.tv_sec + delta_clock;
    tm = *localtime(&now);

    tm.tm_year = R_CX - 1900;
    tm.tm_mon = R_DH - 1;
    tm.tm_mday = R_DL;
    tm.tm_wday = R_AL;
    
    now = mktime(&tm);
    if (now == -1)
	return (DATA_INVALID);

    delta_clock = now - tv.tv_sec;	/* compute new offset? */
    R_AL = 0;
    return(0);
}

/*
** 21:2c
**
** Get time
*/
static int
int21_2c(regcontext_t *REGS)
{
    struct timeval	tv;
    struct timezone	tz;
    struct tm		tm;
    time_t		now;

    gettimeofday(&tv, &tz);
    now = tv.tv_sec + delta_clock;
    tm = *localtime(&now);

    R_CH = tm.tm_hour;
    R_CL = tm.tm_min;
    R_DH = tm.tm_sec;
    R_DL = tv.tv_usec / 10000;
    return(0);
}

/*
** 21:2d
**
** Set time
*/
static int
int21_2d(regcontext_t *REGS)
{
    struct timeval	tv;
    struct timezone	tz;
    struct tm		tm;
    time_t		now;

    gettimeofday(&tv, &tz);
    now = tv.tv_sec + delta_clock;
    tm = *localtime(&now);

    tm.tm_hour = R_CH;
    tm.tm_min = R_CL;
    tm.tm_sec = R_DH;
    tv.tv_usec = R_DL * 10000;

    now = mktime(&tm);
    if (now == -1)
	return (DATA_INVALID);
    
    delta_clock = now - tv.tv_sec;
    R_AL = 0;
    return(0);
}

/*
** 21:2f
**
** get DTA
*/
static int
int21_2f(regcontext_t *REGS)
{
    PUTVEC(R_ES, R_BX, disk_transfer_addr);
    debug(D_FILE_OPS, "get dta at %x:%x\n", R_ES, R_BX);
    return(0);
}

/*
** 21:30
**
** get DOS version number.
**
** XXX begging for a rewrite
*/
static int
int21_30(regcontext_t *REGS)
{
    int		v;
    
    char *cmd = (char *)MAKEPTR(get_env(), 0);

    /* 
     * retch.  I think this skips the environment and looks for the name
     * of the current command.
     */
    do {
	while (*cmd)
	    ++cmd;
    } while (*++cmd);
    ++cmd;
    cmd += *(short *)cmd + 1;
    while (cmd[-1] && cmd[-1] != '\\' && cmd[-1] != ':')
	--cmd;

    /* get the version we're pretending to be for this sucker */
    v = getver(cmd);
    R_AL = (v / 100) & 0xff;
    R_AH = v % 100;
    return(0);
}

/*
** 21:33:05
**
** Get boot drive
*/
static int
int21_33_5(regcontext_t *REGS)
{
    R_DL = 3;			/* always booted from C */
    return(0);
}

/*
** 21:33:06
**
** get true DOS version
*/
static int
int21_33_6(regcontext_t *REGS)
{
    int		v;
    
    v = getver(NULL);
    R_BL = (v / 100) & 0xff;
    R_BH = v % 100;
    R_DH = 0;
    R_DL = 0;
    return(0);
}

/*
** 21:33
**
** extended break checking
*/
static int
int21_33(regcontext_t *REGS)
{
    int		ftemp;
    
    switch (R_AL) {
    case 0x00:
	R_DL = ctrl_c_flag;
	break;
    case 0x01:
	ctrl_c_flag = R_DL;
	break;
    case 0x02:
	ftemp = ctrl_c_flag;
	ctrl_c_flag = R_DL;
	R_DL = ftemp;
	break;
	
    default:
	unknown_int3(0x21, 0x33, R_AL, REGS);
	return(FUNC_NUM_IVALID);
    }
    return(0);
}

/*
** 21:34
**
** Get address of InDos flag
**
** XXX check interrupt list WRT location of critical error flag too.
*/
static int
int21_34(regcontext_t *REGS)
{
    PUTVEC(R_ES, R_BX, (u_long)InDOS);
    return(0);
}

/*
** 21:35
**
** get interrupt vector
*/
static int
int21_35(regcontext_t *REGS)
{
    PUTVEC(R_ES, R_BX, ivec[R_AL]);
    debug(D_MEMORY, "%02x <- %04x:%04x\n", R_AL, R_ES, R_BX);
    return(0);
}

/*
** 21:37
**
** switch character manipulation
**
*/
static int
int21_37(regcontext_t *REGS)
{
    switch (R_AL) {
    case 0: /* get switch character */
	R_DL = '/';
	break;
	
    case 1: /* set switch character (normally /) */
	/* ignored by most versions of DOS */
	break;
    default:
	unknown_int3(0x21, 0x37, R_AL, REGS);
	return (FUNC_NUM_IVALID);
    }
    return(0);
    
}

/*
** 21:38
**
** country code information
**
** XXX internat guru?
*/
static int
int21_38(regcontext_t *REGS)
{
    char	*addr;
    
    if (R_DX == 0xffff) {
	debug(D_HALF, "warning: set country code ignored");
	return(0);
    }
    addr = (char *)MAKEPTR(R_DS, R_DX);
    PUTVEC(countryinfo.ciCaseMapSegment, countryinfo.ciCaseMapOffset,
	   upcase_vector);
    memcpy(addr, &countryinfo, sizeof(countryinfo));
    return(0);
}

/*
** 21:39
** 21:3a
** 21:41
** 21:56
**
** mkdir, rmdir, unlink, rename
*/
static int
int21_dirfn(regcontext_t *REGS)
{
    int		error;
    char	fname[PATH_MAX],tname[PATH_MAX];
    int		drive;

    error = translate_filename((u_char *)MAKEPTR(R_DS, R_DX), fname, &drive);
    if (error)
	return (error);

    if (dos_readonly(drive))
	return (WRITE_PROT_DISK);

    switch(R_AH) {
    case 0x39:
	debug(D_FILE_OPS, "mkdir(%s)\n", fname);
	error = mkdir(fname, 0777);
	break;
    case 0x3a:
	debug(D_FILE_OPS, "rmdir(%s)\n", fname);
	error = rmdir(fname);
	break;
    case 0x41:
	debug(D_FILE_OPS, "unlink(%s)\n", fname);
	error = unlink(fname);
	break;
    case 0x56:		/* rename - some extra work */
	error = translate_filename((u_char *)MAKEPTR(R_ES, R_DI), tname, &drive);
	if (error)
	    return (error);

	debug(D_FILE_OPS, "rename(%s, %s)\n", fname, tname);
	error = rename(fname, tname);
	break;

    default:
	fatal("call to int21_dirfn for unknown function %x\n",R_AH);
    }
    if (error < 0) {
	switch (errno) {
	case ENOTDIR:
	case ENOENT:
	    return (PATH_NOT_FOUND);
	case EXDEV:
	    return (NOT_SAME_DEV);
	default:
	    return (ACCESS_DENIED);
	}
    }
    return(0);
}

/*
** 21:3b
**
** chdir
*/
static int
int21_3b(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "chdir(%s)\n",(u_char *)MAKEPTR(R_DS, R_DX));
    return(dos_setcwd((u_char *)MAKEPTR(R_DS, R_DX)));
}

/*
** 21:3c
** 21:5b
** 21:6c
**
** open, creat, creat new, multipurpose creat
*/
static int
int21_open(regcontext_t *REGS)
{
    int		error;
    char	fname[PATH_MAX];
    struct stat	sb;
    int		mode,action,status;
    char	*pname;
    int		drive;
    int		fd;
    
    switch(R_AH) {
    case 0x3c:			/* creat */
	pname = (char *)MAKEPTR(R_DS, R_DX);
	action = 0x12;		/* create/truncate regardless */
	mode = O_RDWR;
	debug(D_FILE_OPS, "creat");
	break;

    case 0x3d:			/* open */
	pname = (char *)MAKEPTR(R_DS, R_DX);
	action = 0x01;		/* fail if not exist, open if exists */
	switch (R_AL & 3) {
	case 0:
	    mode = O_RDONLY;
	    break;
	case 1:
	    mode = O_WRONLY;
	    break;
	case 2:
	    mode = O_RDWR;
	    break;
	default:
	    return (FUNC_NUM_IVALID);
	}
	debug(D_FILE_OPS, "open");
	break;
	
    case 0x5b:			/* creat new */
    	pname = (char *)MAKEPTR(R_DS, R_DL);
	action = 0x10;		/* create if not exist, fail if exists */
	mode = O_RDWR;
	debug(D_FILE_OPS, "creat_new");
	break;

    case 0x6c:			/* multipurpose */
	pname = (char *)MAKEPTR(R_DS, R_SI);
	action = R_DX;
	switch (R_BL & 3) {
	case 0:
	    mode = O_RDONLY;
	    break;
	case 1:
	    mode = O_WRONLY;
	    break;
	case 2:
	    mode = O_RDWR;
	    break;
	default:
	    return (FUNC_NUM_IVALID);
	}
	debug(D_FILE_OPS, "mopen");
	break;

    default:
	fatal("called int21_creat for unknown function %x\n",R_AH);
    }
    if (action & 0x02)	/* replace/open mode */
	mode |= O_TRUNC;

    /* consider proposed name */
    error = translate_filename(pname, fname, &drive);
    if (error)
	return (error);

    debug(D_FILE_OPS, "(%s)\n", fname);
    
    if (dos_readonly(drive) && (mode != O_RDONLY))
	return (WRITE_PROT_DISK);

    if (ustat(fname, &sb) < 0) {		/* file does not exist */
	if (action & 0x10) {			/* create? */
	    sb.st_ino = 0;
	    mode |= O_CREAT;			/* have to create as we go */
	    status = 0x02;			/* file created */
	} else {
	    return(FILE_NOT_FOUND);
	}
    } else {
	if (S_ISDIR(sb.st_mode))
	    return(ACCESS_DENIED);
	if (action & 0x03) {			/* exists, work with it */
	    if (action & 0x02) {
		if (!S_ISREG(sb.st_mode)) {	/* only allowed for files */
		    debug(D_FILE_OPS,"attempt to truncate non-regular file\n");
		    return(ACCESS_DENIED);
		}
		status = 0x03;			/* we're going to truncate it */
	    } else {
		status = 0x01;			/* just open it */
	    }
	} else {
	    return(FILE_ALREADY_EXISTS);	/* exists, fail */
	}
    }
	
    if ((fd = open(fname, mode, from_dos_attr(R_CX))) < 0) {
	debug(D_FILE_OPS,"failed to open %s : %s\n",fname,strerror(errno));
	return (ACCESS_DENIED);
    }
    
    if (R_AH == 0x6c) 			/* need to return status too */
	R_CX = status;
    R_AX = fd;				/* return fd */
    return(0);
}

/*
** 21:3e
**
** close
*/
static int
int21_3e(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "close(%d)\n", R_BX);

    if (R_BX == fileno(debugf)) {
	printf("attempt to close debugging fd\n");
	return (HANDLE_INVALID);
    }

    if (close(R_BX) < 0)
	return (HANDLE_INVALID);
    return(0);
}

/*
** 21:3f
**
** read
*/
static int
int21_3f(regcontext_t *REGS)
{
    char	*addr;
    int		n;
    int		avail;
    
    addr = (char *)MAKEPTR(R_DS, R_DX);

    debug(D_FILE_OPS, "read(%d, %d)\n", R_BX, R_CX);
	
    if (R_BX == 0) {
	if (redirect0) {
	    n = read (R_BX, addr, R_CX);
	} else {
	    n = 0;
	    while (n < R_CX) {
		avail = tty_read(REGS, TTYF_BLOCK|TTYF_CTRL|TTYF_ECHONL);
		if (avail < 0)
		    return (0);
		if ((addr[n++] = avail) == '\r')
		    break;
	    }
	}
    } else {
	n = read (R_BX, addr, R_CX);
    }
    if (n < 0)
	return (READ_FAULT);

    R_AX = n;
    return(0);
}

/*
** 21:40
**
** write
*/
static int
write_or_truncate(int fd, char *addr, int len)
{
    off_t offset;

    if (len == 0) {
	offset = lseek(fd, 0, SEEK_CUR);
	if (offset < 0)
	    return -1;
	else
	    return ftruncate(fd, offset);
    } else {
	return write(fd, addr, len);
    }
}

static int
int21_40(regcontext_t *REGS)
{
    char	*addr;
    int		nbytes,n;
    
    addr = (char *)MAKEPTR(R_DS, R_DX);
    nbytes = R_CX;

    debug(D_FILE_OPS, "write(%d, %d)\n", R_BX, nbytes);

    switch (R_BX) {
    case 0:
	if (redirect0) {
	    n = write_or_truncate(R_BX, addr, nbytes);
	    break;
	}
	n = nbytes;
	while (nbytes-- > 0)
	    tty_write(*addr++, -1);
	break;
    case 1:
	if (redirect1) {
	    n = write_or_truncate(R_BX, addr, nbytes);
	    break;
	}
	n = nbytes;
	while (nbytes-- > 0)
	    tty_write(*addr++, -1);
	break;
    case 2:
	if (redirect2) {
	    n = write_or_truncate(R_BX, addr, nbytes);
	    break;
	}
	n = nbytes;
	while (nbytes-- > 0)
	    tty_write(*addr++, -1);
	break;
    default:
	n = write_or_truncate(R_BX, addr, nbytes);
	break;
    }
    if (n < 0)
	return (WRITE_FAULT);
    
    R_AX = n;
    return(0);
}

/*
** 21:42
**
** seek
*/
static int
int21_42(regcontext_t *REGS)
{
    int		whence;
    off_t	offset;
    
    offset = (off_t) ((int) (R_CX << 16) + R_DX);
    switch (R_AL) {
    case 0:
	whence = SEEK_SET;
	break;
    case 1:
	whence = SEEK_CUR;
	break;
    case 2:
	whence = SEEK_END;
	break;
    default:
	return (FUNC_NUM_IVALID);
    }

    debug(D_FILE_OPS, "seek(%d, 0x%qx, %d)\n", R_BX, offset, whence);

    if ((offset = lseek(R_BX, offset, whence)) < 0) {
	if (errno == EBADF)
	    return (HANDLE_INVALID);
	else
	    return (SEEK_ERROR);
    }	

    R_DX = (offset >> 16) & 0xffff;
    R_AX = offset & 0xffff;
    return(0);
}

/*
** 21:43
**
** get/set attributes
*/
static int
int21_43(regcontext_t *REGS)
{
    int		error;
    char	fname[PATH_MAX];
    struct stat	sb;
    int		mode;
    int		drive;
    
    error = translate_filename((u_char *)MAKEPTR(R_DS, R_DX), fname, &drive);
    if (error)
	return (error);

    debug(D_FILE_OPS, "get/set attributes: %s, cx=%x, al=%d\n",
	  fname, R_CX, R_AL);

    if (stat(fname, &sb) < 0) {
	debug(D_FILE_OPS, "stat failed for %s\n", fname);
	return (FILE_NOT_FOUND);
    }
    
    switch (R_AL) {
    case 0:			/* get attributes */
	mode = 0;
	if (dos_readonly(drive) || access(fname, W_OK))
	    mode |= 0x01;
	if (S_ISDIR(sb.st_mode))
	    mode |= 0x10;
	R_CX = mode;
	break;

    case 1:			/* set attributes - XXX ignored */
	if (R_CX & 0x18)
	    return (ACCESS_DENIED);
	break;

    default:
	return (FUNC_NUM_IVALID);
    }
    return(0);
}

/*
** 21:44:0
**
** ioctl - get device info
**
** XXX it would be nice to detect EOF.
*/
static int
int21_44_0(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "ioctl get %d\n", R_BX);

    switch (R_BX) {
    case 0:
	R_DX = 0x80 | 0x01;		/* is device, is standard output */
	break;
    case 1:
	R_DX =  0x80 | 0x02;		/* is device, is standard input */
	break;
    case 2:
	R_DX = 0x80;			/* is device */
	break;
    default:
	if (isatty (R_BX))
	    R_DX = 0x80;		/* is a device */
	else
	    R_DX = 0;			/* is a file */
	break;
    }
    return(0);
}

/*
** 21:44:01
**
** ioctl - set device info
*/
static int
int21_44_1(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "ioctl set device info %d flags %x (ignored)\n",
	  R_BX, R_DX);
    return(0);
}

/*
** 21:44:7
**
** Get output status
*/
static int
int21_44_7(regcontext_t *REGS)
{
    /* XXX Should really check to see if BX is open or not */
    R_AX = 0xFF;
    return(0);
}

/*
** 21:44:8
**
** test for removable block device
*/
static int
int21_44_8(regcontext_t *REGS)
{
    R_AX = 1;			/* fixed */
    return(0);
}

/*
** 21:44:0
**
** test for remote device (disallow direct I/O)
*/
static int
int21_44_9(regcontext_t *REGS)
{
    R_DX = 0x1200;		/* disk is remote, direct I/O not allowed */
    return(0);
}

/*
** 21:45
**
** dup
*/
static int
int21_45(regcontext_t *REGS)
{
    int		nfd;
    
    debug(D_FILE_OPS, "dup(%d)\n", R_BX);

    if ((nfd = dup(R_BX)) < 0) {
	if (errno == EBADF)
	    return (HANDLE_INVALID);
	else
	    return (TOO_MANY_OPEN_FILES);
    }
    R_AX = nfd;
    return(0);
}

/*
** 21:46
**
** dup2
*/
static int
int21_46(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "dup2(%d, %d)\n", R_BX, R_CX);

    if (dup2(R_BX, R_CX) < 0) {
	if (errno == EMFILE)
	    return (TOO_MANY_OPEN_FILES);
	else
	    return (HANDLE_INVALID);
    }
    return(0);
}

/*
** 21:47
**
** getcwd
*/
static int
int21_47(regcontext_t *REGS)
{
    int		n,nbytes;
    char	*p,*addr;

    n = R_DL;
    if (!n--)
	n = diskdrive;

    p = (char *)dos_getcwd(n) + 1;
    addr = (char *)MAKEPTR(R_DS, R_SI);

    nbytes = strlen(p);
    if (nbytes > 63)
	nbytes = 63;
    
    memcpy(addr, p, nbytes);
    addr[nbytes] = 0;
    return(0);
}

/*
** 21:48
**
** allocate memory
*/
static int
int21_48(regcontext_t *REGS)
{
    int		memseg,avail;
    
    memseg = mem_alloc(R_BX, pspseg, &avail);

    if (memseg == 0L) {
	R_BX = avail;
	return (INSUF_MEM);
    }

    R_AX = memseg;
    return(0);
}

/*
** 21:49
**
** free memory
*/
static int
int21_49(regcontext_t *REGS)
{
    if (mem_adjust(R_ES, 0, NULL) < 0)
	return (MEM_BLK_ADDR_IVALID);
    return(0);
}

/*
** 21:4a
**
** resize memory block
*/
static int
int21_4a(regcontext_t *REGS)
{
    int		n,avail;
    
    if ((n = mem_adjust(R_ES, R_BX, &avail)) < 0) {
	R_BX = avail;
	if (n == -1)
	    return (INSUF_MEM);
	else
	    return (MEM_BLK_ADDR_IVALID);
    }
    return(0);
}

/*
** 21:4b
**
** exec
**
** XXX verify!
*/
static int
int21_4b(regcontext_t *REGS)
{
    int		fd;
    u_short	*param;
    
    debug(D_EXEC, "exec(%s)\n",(u_char *)MAKEPTR(R_DS, R_DX));

    if ((fd = open_prog((u_char *)MAKEPTR(R_DS, R_DX))) < 0) {
	debug(D_EXEC, "%s: command not found\n",
	      (u_char *)MAKEPTR(R_DS, R_DX));
	return (FILE_NOT_FOUND);
    }

    /* child */
    param = (u_short *)MAKEPTR(R_ES, R_BX);

    switch (R_AL) {
    case 0x00: /* load and execute */
	exec_command(REGS, 1, fd, cmdname, param);
	close(fd);
	break;

    case 0x01: /* just load */
	exec_command(REGS, 0, fd, cmdname, param);
	close(fd);
	break;

    case 0x03: /* load overlay */
	load_overlay(fd, param[0], param[1]);
	close(fd);
	break;

    default:
	unknown_int3(0x21, 0x4b, R_AL, REGS);
	return (FUNC_NUM_IVALID);
    }
    return(0);
}

/*
** 21:4c
**
** return with code
*/
static int
int21_4c(regcontext_t *REGS)
{
    return_status = R_AL;
    done(REGS, R_AL);
    return 0;
}

/*
** 21:4d
**
** get return code of child
*/
static int
int21_4d(regcontext_t *REGS)
{
    R_AX = return_status;
    return(0);
}

/*
** 21:4e
** 21:4f
**
** find first, find next
*/
static int
int21_find(regcontext_t *REGS)
{
    find_block_t	*dta;
    dosdir_t		dosdir;
    int			error;
    
    dta = (find_block_t *)VECPTR(disk_transfer_addr);
	
    switch (R_AH) {
    case 0x4e:		/* find first */
	error = find_first((u_char *)MAKEPTR(R_DS, R_DX), R_CX, &dosdir, dta);
	break;
    case 0x4f:
	error = find_next(&dosdir, dta);
	break;
    default:
	fatal("called int21_find for unknown function %x\n",R_AH);
    }
    if (!error) {
	dosdir_to_dta(&dosdir, dta);
	R_AX = 0;
    }
    return(error);
}

/*
** 21:50
**
** set PSP
*/
static int
int21_50(regcontext_t *REGS)
{
    pspseg = R_BX;
    return(0);
}

/*
** 21:57:00
**
** get mtime for handle
*/
static int
int21_57_0(regcontext_t *REGS)
{
    struct stat	sb;
    u_short	date, mtime;
    
    if (fstat(R_BX, &sb) < 0)
	return (HANDLE_INVALID);
    encode_dos_file_time(sb.st_mtime, &date, &mtime);
    R_CX = mtime;
    R_DX = date;
    return(0);
}

/*
** 21:57:01
**
** set mtime for handle
*/
static int
int21_57_1(regcontext_t *REGS)
{
#ifdef __NetBSD__	/* XXX need futimes() */
	struct stat sb;
	struct timeval tv[2];
	u_short date, time;

	time = R_CX;
	date = R_DX;
	tv[0].tv_sec = tv[1].tv_sec = decode_dos_file_time(date, time);
	tv[0].tv_usec = tv[1].tv_usec = 0;
	if (futimes(R_BX, tv) < 0)
	    return (HANDLE_INVALID);
	break;
#endif
	return(0);
}

/*
** 21:58
**
** get/set memory strategy
** get/set UMB link state
*/
static int
int21_58(regcontext_t *REGS)
{
    switch (R_AL) {
    case 0x00:				/* get memory strategy */
	R_AX = memory_strategy;
	break;
    case 0x01:				/* set memory strategy */
	memory_strategy = R_BL;
	if (memory_strategy > 2)	/* higher make no sense without UMBs */
	    memory_strategy = 2;	
	break;
    case 0x02:			/* get UMB link state */
	R_AL = 0;		/* UMBs not in link chain */
	break;
    default:			/* includes set, which is invalid */
	unknown_int3(0x21, 0x58, R_AL, REGS);
	return (FUNC_NUM_IVALID);
    }
    return(0);
}

/*
** 21:59
**
** get extended error information
*/
static int
int21_59(regcontext_t *REGS)
{
    R_AX = doserrno;
    switch (doserrno) {
    case 1:
    case 6:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 15:
	R_BH = 7;		/* application error */
	break;

    case 2:
    case 3:
    case 4:
    case 5:
	R_BH = 8;		/* not found */
	break;

    case 7:
    case 8:
	R_BH = 1;		/* out of resource */
	break;
	
    default:
	R_BH = 12;		/* already exists */
	break;
    }
    R_BL = 6;			/* always ignore! */
    R_CH = 1;			/* unknown/inappropriate */
    return(0);
}

/*
** 21:5a
**
** create temporary file
*/
static int
int21_5a(regcontext_t *REGS)
{
    char	fname[PATH_MAX];
    char	*pname;
    int		error;
    int		n;
    int		drive;
    int		fd;
    
    /* get and check proposed path */
    pname = (char *)MAKEPTR(R_DS, R_DX);
    error = translate_filename(pname, fname, &drive);
    if (error)
	return (error);

    debug(D_FILE_OPS, "tempname(%s)\n", fname);

    if (dos_readonly(drive))
	return (WRITE_PROT_DISK);

    n = strlen(fname);
    strcat(fname,"__dostmp.XXX");
    fd = mkstemp(fname);
    if (fd < 0)
	return (ACCESS_DENIED);

    strcat(pname, fname + n);	/* give back the full name */
    R_AX = fd;
    return(0);
}

/*
** 21:60
**
** canonicalise name
*/
static int
int21_60(regcontext_t *REGS)
{
    return(dos_makepath((char *)MAKEPTR(R_DS, R_SI),
			(char *)MAKEPTR(R_ES, R_DI)));
}

/*
** 21:62
**
** get current PSP
*/
static int
int21_62(regcontext_t *REGS)
{
    R_BX = pspseg;
    return(0);
}

/*
** 21:65:23
**
** determine yes/no
** (mostly for humour value 8)
*/
static int
int21_65_23(regcontext_t *REGS)
{
    switch (R_DL) {
    case 'n':	/* no, nein, non, nyet */
    case 'N':
	R_AX = 0;
	break;
    case 'y':	/* yes */
    case 'Y':
    case 'j':	/* ja */
    case 'J':
    case 'o':	/* oui */
    case 'O':
    case 'd':	/* da */
    case 'D':
	R_AX = 1;
	break;
    default:	/* maybe */
	R_AX = 2;
	break;
    }
    return(0);
}

/*
** 21:68
** 21:6a
**
** fflush/commit file
*/
static int
int21_fflush(regcontext_t *REGS)
{
    debug(D_FILE_OPS, "fsync(%d)\n", R_BX);
	
    if (fsync(R_BX) < 0)
	return (HANDLE_INVALID);
    return(0);
}

/******************************************************************************
** 21:0f 21:10 21:11 21:12 21:16 21:27 21:28:21:29
**
** FCB functions
*/
static void
openfcb(struct fcb *fcbp)
{
	struct stat statb;

	fcbp->fcbDriveID = 3;		/* drive C */
	fcbp->fcbCurBlockNo = 0;
	fcbp->fcbRecSize = 128;
	if (fstat(fcbp->fcb_fd, &statb) < 0) {
		debug(D_FILE_OPS, "open not complete with errno %d\n", errno);
		return;
	}
	encode_dos_file_time(statb.st_mtime,
				&fcbp->fcbFileDate, &fcbp->fcbFileTime);
	fcbp->fcbFileSize = statb.st_size;
}

static int
getfcb_rec(struct fcb *fcbp, int nrec)
{
	int n;

	n = fcbp->fcbRandomRecNo;
	if (fcbp->fcbRecSize >= 64)
		n &= 0xffffff;
	fcbp->fcbCurRecNo = n % 128;
	fcbp->fcbCurBlockNo = n / 128;
	if (lseek(fcbp->fcb_fd, n * fcbp->fcbRecSize, SEEK_SET) < 0)
		return (-1);
	return (nrec * fcbp->fcbRecSize);
}


static int
setfcb_rec(struct fcb *fcbp, int n)
{
	int recs, total;

	total = fcbp->fcbRandomRecNo;
	if (fcbp->fcbRecSize >= 64)
		total &= 0xffffff;
	recs = (n+fcbp->fcbRecSize-1) / fcbp->fcbRecSize;
	total += recs;

	fcbp->fcbRandomRecNo = total;
	fcbp->fcbCurRecNo = total % 128;
	fcbp->fcbCurBlockNo = total / 128;
}

void
fcb_to_string(fcbp, buf)
	struct fcb *fcbp;
	u_char *buf;
{

	if (fcbp->fcbDriveID != 0x00) {
		*buf++ = drntol(fcbp->fcbDriveID - 1);
		*buf++ = ':';
	}
	pack_name(fcbp->fcbFileName, buf);
}


static int
int21_fcb(regcontext_t *REGS)
{	
    char		buf[PATH_MAX];
    char		fname[PATH_MAX];
    struct stat		sb;
    dosdir_t		dosdir;
    struct fcb		*fcbp;
    find_block_t	*dta;
    u_char		*addr;
    int			error;
    int			drive;
    int			fd;
    int			nbytes,n;


    fcbp = (struct fcb *)MAKEPTR(R_DS, R_DX);

    switch (R_AH) {
	
    case 0x0f: /* open file with FCB */
	fcb_to_string(fcbp, buf);
	error = translate_filename(buf, fname, &drive); 
	if (error)
	    return (error);

	debug(D_FILE_OPS, "open FCB(%s)\n", fname);

	if (ustat(fname, &sb) < 0) 
	    sb.st_ino = 0;

	if (dos_readonly(drive))
	    return (WRITE_PROT_DISK);

	if (sb.st_ino == 0 || S_ISDIR(sb.st_mode))
	    return (FILE_NOT_FOUND);

	if ((fd = open(fname, O_RDWR)) < 0) {
	    if (errno == ENOENT)
		return (FILE_NOT_FOUND);
	    else
		return (ACCESS_DENIED);
	}

	fcbp->fcb_fd = fd;
	openfcb(fcbp);
	R_AL = 0;
	break;

    case 0x10: /* close file with FCB */
	debug(D_FILE_OPS, "close FCB(%d)\n", fcbp->fcb_fd);

	if (close(fcbp->fcb_fd) < 0)
	    return (HANDLE_INVALID);

	fcbp->fcb_fd = -1;
	R_AL = 0;
	break;

    case 0x11: /* find_first with FCB */
	dta = (find_block_t *)VECPTR(disk_transfer_addr);

	fcb_to_string(fcbp, buf);
	error = find_first(buf, fcbp->fcbAttribute, &dosdir, dta);
	if (error)
	    return (error);
	
	dosdir_to_dta(&dosdir, dta);
	R_AL = 0;
	break;

    case 0x12: /* find_next with FCB */
	dta = (find_block_t *)VECPTR(disk_transfer_addr);

	error = find_next(&dosdir, dta);
	if (error)
	    return (error);

	dosdir_to_dta(&dosdir, dta);
	R_AL = 0;
	break;

    case 0x16: /* create file with FCB */
	fcb_to_string(fcbp, buf);
	error = translate_filename(buf, fname, &drive); 
	if (error)
	    return (error);

	debug(D_FILE_OPS, "creat FCB(%s)\n", fname);
	
	if (ustat(fname, &sb) < 0)
	    sb.st_ino = 0;

	if (dos_readonly(drive))
	    return (WRITE_PROT_DISK);

	if (sb.st_ino && !S_ISREG(sb.st_mode))
	    return (ACCESS_DENIED);

	if ((fd = open(fname, O_CREAT|O_TRUNC|O_RDWR, 0666)) < 0)
	    return (ACCESS_DENIED);
	
	fcbp->fcb_fd = fd;
	openfcb(fcbp);
	R_AL = 0;
	break;

    case 0x27: /* random block read */
	addr = (u_char *)VECPTR(disk_transfer_addr);
	nbytes = getfcb_rec(fcbp, R_CX);

	if (nbytes < 0)
	    return (READ_FAULT);
	n = read(fcbp->fcb_fd, addr, nbytes);
	if (n < 0)
	    return (READ_FAULT);
	R_CX = setfcb_rec(fcbp, n);
	if (n < nbytes) {
	    nbytes = n % fcbp->fcbRecSize;
	    if (0 == nbytes) {
		R_AL = 0x01;
	    } else {
		bzero(addr + n, fcbp->fcbRecSize - nbytes);
		R_AL = 0x03;
	    }
	} else {
	    R_AL = 0;
	}
	break;

    case 0x28: /* random block write */
	addr = (u_char *)VECPTR(disk_transfer_addr);
	nbytes = getfcb_rec(fcbp, R_CX);

	if (nbytes < 0)
	    return (WRITE_FAULT);
	n = write(fcbp->fcb_fd, addr, nbytes);
	if (n < 0)
	    return (WRITE_FAULT);
	R_CX = setfcb_rec(fcbp, n);
	if (n < nbytes) {
	    R_AL = 0x01;
	} else {
	    R_AL = 0;
	}
	break;

    case 0x29: /* parse filename */
	debug(D_FILE_OPS,"parse filename: flag=%d, ", R_AL);

	R_AX = parse_filename(R_AL,
			      (char *)MAKEPTR(R_DS, R_SI),
			      (char *)MAKEPTR(R_ES, R_DI),
			      &nbytes);
	debug(D_FILE_OPS, "%d %s, FCB: %d, %.11s\n",
	      nbytes,
	      (char *)MAKEPTR(R_DS, R_SI),
	      *(int *)MAKEPTR(R_ES, R_DI),
	      (char *)MAKEPTR(R_ES, R_DI) + 1);
	
	R_SI += nbytes;
	break;

    default:
	fatal("called int21_fcb with unknown function %x\n",R_AH);
    }
    return(0);
}

/*
** 21:5d
** 21:5e
** 21:5f
**
** network functions
** XXX relevant?
*/
static int
int21_net(regcontext_t *REGS)
{
    switch(R_AH) {
    case 0x5d:
	switch(R_AL) {
	case 0x06:
	    debug(D_HALF, "Get Swapable Area\n");
	    return (ACCESS_DENIED);
	case 0x08: /* Set redirected printer mode */
	    debug(D_HALF, "Redirection is %s\n",
		  R_DL ? "separate jobs" : "combined");
	    break;
	case 0x09: /* Flush redirected printer output */
	    break;
	default:
	    unknown_int3(0x21, 0x5d, R_AL, REGS);
	    return (FUNC_NUM_IVALID);
	}
	break;
	
    case 0x5e:
    case 0x5f:
	unknown_int2(0x21, R_AH, REGS);
	return (FUNC_NUM_IVALID);
    default:
	fatal("called int21_net with unknown function %x\n",R_AH);
    }
    return(0);
}

/*
** 21:??
**
** Unknown/unsupported
*/
static int
int21_NOFUNC(regcontext_t *REGS)
{
    unknown_int2(0x21, R_AH, REGS);
    return (FUNC_NUM_IVALID);
}

/*
** 21:??
**
** Null function; no error, no action
*/
static int
int21_NULLFUNC(regcontext_t *REGS)
{
    R_AL = 0;
    return(0);
}


static struct intfunc_table int21_table [] = {
    { 0x00,	IFT_NOSUBFUNC,	int21_00,	"terminate"},
    { 0x01,	IFT_NOSUBFUNC,	int21_01,	"read character with echo"},
    { 0x02,	IFT_NOSUBFUNC,	int21_02,	"write char to stdout"},
    { 0x03,	IFT_NOSUBFUNC,	int21_NOFUNC,	"read char from stdaux"},
    { 0x04,	IFT_NOSUBFUNC,	int21_NOFUNC,	"write char to stdaux"},
    { 0x05,	IFT_NOSUBFUNC,	int21_NOFUNC,	"write char to printer"},
    { 0x06,	IFT_NOSUBFUNC,	int21_06,	"direct console I/O"},
    { 0x07,	IFT_NOSUBFUNC,	int21_07,	"direct console in without echo"},
    { 0x08,	IFT_NOSUBFUNC,	int21_08,	"read character, no echo"},
    { 0x09,	IFT_NOSUBFUNC,	int21_09,	"write string to standard out"},
    { 0x0a,	IFT_NOSUBFUNC,	int21_0a,	"buffered input"},
    { 0x0b,	IFT_NOSUBFUNC,	int21_0b,	"get stdin status"},
    { 0x0c,	IFT_NOSUBFUNC,	int21_0c,	"flush stdin and read"},
    { 0x0d,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"disk reset"},
    { 0x0e,	IFT_NOSUBFUNC,	int21_0e,	"select default drive"},
    { 0x19,	IFT_NOSUBFUNC,	int21_19,	"get default drive"},
    { 0x1a,	IFT_NOSUBFUNC,	int21_1a,	"set DTA"},
    { 0x1b,	IFT_NOSUBFUNC,	int21_free,	"get allocation for default drive"},
    { 0x1c,	IFT_NOSUBFUNC,	int21_free,	"get allocation for specific drive"},
    { 0x1f,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get DPB for current drive"},
    { 0x23,	IFT_NOSUBFUNC,	int21_23,	"Get file size (old)"},
    { 0x25,	IFT_NOSUBFUNC,	int21_25,	"set interrupt vector"},
    { 0x26,	IFT_NOSUBFUNC,	int21_26,	"create new PSP"},
    { 0x2a,	IFT_NOSUBFUNC,	int21_2a,	"get date"},
    { 0x2b,	IFT_NOSUBFUNC,	int21_2b,	"set date"},
    { 0x2c,	IFT_NOSUBFUNC,	int21_2c,	"get time"},
    { 0x2d,	IFT_NOSUBFUNC,	int21_2d,	"set time"},
    { 0x2e,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"set verify flag"},
    { 0x2f,	IFT_NOSUBFUNC,	int21_2f,	"get DTA"},
    { 0x30,	IFT_NOSUBFUNC,	int21_30,	"get DOS version"},
    { 0x31,	IFT_NOSUBFUNC,	int21_NOFUNC,	"terminate and stay resident"},
    { 0x32,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get DPB for specific drive"},
    { 0x33,	0x05,		int21_33_5,	"get boot drive"},
    { 0x33,	0x06,		int21_33_6,	"get true version number"},
    { 0x33,	IFT_NOSUBFUNC,	int21_33,	"extended break checking"},
    { 0x34,	IFT_NOSUBFUNC,	int21_34,	"get address of InDos flag"},
    { 0x35,	IFT_NOSUBFUNC,	int21_35,	"get interrupt vector"},
    { 0x36,	IFT_NOSUBFUNC,	int21_free,	"get disk free space"},
    { 0x37,	IFT_NOSUBFUNC,	int21_37,	"switch character"},
    { 0x38,	IFT_NOSUBFUNC,	int21_38,	"country code/information"},
    { 0x39,	IFT_NOSUBFUNC,	int21_dirfn,	"mkdir"},
    { 0x3a,	IFT_NOSUBFUNC,	int21_dirfn,	"rmdir"},
    { 0x3b,	IFT_NOSUBFUNC,	int21_3b,	"chdir"},
    { 0x3c,	IFT_NOSUBFUNC,	int21_open,	"creat"},
    { 0x3d,	IFT_NOSUBFUNC,	int21_open,	"open"},
    { 0x3e,	IFT_NOSUBFUNC,	int21_3e,	"close"},
    { 0x3f,	IFT_NOSUBFUNC,	int21_3f,	"read"},
    { 0x40,	IFT_NOSUBFUNC,	int21_40,	"write"},
    { 0x41,	IFT_NOSUBFUNC,	int21_dirfn,	"unlink"},
    { 0x42,	IFT_NOSUBFUNC,	int21_42,	"lseek"},
    { 0x43,	IFT_NOSUBFUNC,	int21_43,	"get/set file attributes"},
    { 0x44,	0x00,		int21_44_0,	"ioctl(get)"},
    { 0x44,	0x01,		int21_44_1,	"ioctl(set)"},
    { 0x44,	0x07,		int21_44_7,	"ioctl(Check output status)"},
    { 0x44,	0x08,		int21_44_8,	"ioctl(test removable)"},
    { 0x44,	0x09,		int21_44_9,	"ioctl(test remote)"},
    { 0x45,	IFT_NOSUBFUNC,	int21_45,	"dup"},
    { 0x46,	IFT_NOSUBFUNC,	int21_46,	"dup2"},
    { 0x47,	IFT_NOSUBFUNC,	int21_47,	"getwd"},
    { 0x48,	IFT_NOSUBFUNC,	int21_48,	"allocate memory"},
    { 0x49,	IFT_NOSUBFUNC,	int21_49,	"free memory"},
    { 0x4a,	IFT_NOSUBFUNC,	int21_4a,	"resize memory block"},
    { 0x4b,	IFT_NOSUBFUNC,	int21_4b,	"exec"},
    { 0x4c,	IFT_NOSUBFUNC,	int21_4c,	"exit with return code"},
    { 0x4d,	IFT_NOSUBFUNC,	int21_4d,	"get return code from child"},
    { 0x4e,	IFT_NOSUBFUNC,	int21_find,	"findfirst"},
    { 0x4f,	IFT_NOSUBFUNC,	int21_find,	"findnext"},
    { 0x50,	IFT_NOSUBFUNC,	int21_50,	"set psp"},
    { 0x51,	IFT_NOSUBFUNC,	int21_62,	"get psp"},
    { 0x52,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get LoL"},
    { 0x53,	IFT_NOSUBFUNC,	int21_NOFUNC,	"translate BPB to DPB"},
    { 0x54,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"get verify flag"},
    { 0x55,	IFT_NOSUBFUNC,	int21_NOFUNC,	"create PSP"},
    { 0x56,	IFT_NOSUBFUNC,	int21_dirfn,	"rename"},
    { 0x57,	0x00,		int21_57_0,	"get mtime"},
    { 0x57,	0x01,		int21_57_1,	"set mtime"},
    { 0x58,	IFT_NOSUBFUNC,	int21_58,	"get/set memory strategy"},
    { 0x59,	IFT_NOSUBFUNC,	int21_59,	"get extended error information"},
    { 0x5a,	IFT_NOSUBFUNC,	int21_5a,	"create temporary file"},
    { 0x5b,	IFT_NOSUBFUNC,	int21_open,	"create new file"},
    { 0x5c,	IFT_NOSUBFUNC,	int21_NOFUNC,	"flock"},
    { 0x5d,	IFT_NOSUBFUNC,	int21_net,	"network functions"},
    { 0x5e,	IFT_NOSUBFUNC,	int21_net,	"network functions"},
    { 0x5f,	IFT_NOSUBFUNC,	int21_net,	"network functions"},
    { 0x60,	IFT_NOSUBFUNC,	int21_60,	"canonicalise name/path"},
    { 0x61,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"network functions (reserved)"},
    { 0x62,	IFT_NOSUBFUNC,	int21_62,	"get current PSP"},
    { 0x63,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get DBCS lead-byte table"},
    { 0x64,	IFT_NOSUBFUNC,	int21_NOFUNC,	"set device-driver lookahead"},
    { 0x65,	0x23,		int21_65_23,	"determine yes/no"},
    { 0x65,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get extended country information"},
    { 0x66,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get/set codepage table"},
    { 0x67,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"set handle count"},
    { 0x68,	IFT_NOSUBFUNC,	int21_fflush,	"fflush"},
    { 0x69,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get/set disk serial number"},
    { 0x6a,	IFT_NOSUBFUNC,	int21_fflush,	"commit file"},
    { 0x6b,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"IFS ioctl"},
    { 0x6c,	IFT_NOSUBFUNC,	int21_open,	"extended open/create"},

/* FCB functions */
    { 0x0f,	IFT_NOSUBFUNC,	int21_fcb,	"open file"},
    { 0x10,	IFT_NOSUBFUNC,	int21_fcb,	"close file"},
    { 0x11,	IFT_NOSUBFUNC,	int21_fcb,	"find first"},
    { 0x12,	IFT_NOSUBFUNC,	int21_fcb,	"find next"},
    { 0x13,	IFT_NOSUBFUNC,	int21_NOFUNC,	"delete"},
    { 0x14,	IFT_NOSUBFUNC,	int21_NOFUNC,	"sequential read"},
    { 0x15,	IFT_NOSUBFUNC,	int21_NOFUNC,	"sequential write"},
    { 0x16,	IFT_NOSUBFUNC,	int21_fcb,	"create/truncate"},
    { 0x17,	IFT_NOSUBFUNC,	int21_NOFUNC,	"rename"},
    { 0x21,	IFT_NOSUBFUNC,	int21_NOFUNC,	"read random"},
    { 0x22,	IFT_NOSUBFUNC,	int21_NOFUNC,	"write random"},
    { 0x23,	IFT_NOSUBFUNC,	int21_NOFUNC,	"get file size"},
    { 0x24,	IFT_NOSUBFUNC,	int21_NOFUNC,	"set random record number"},
    { 0x27,	IFT_NOSUBFUNC,	int21_fcb,	"random block read"},
    { 0x28,	IFT_NOSUBFUNC,	int21_fcb,	"random block write"},
    { 0x29,	IFT_NOSUBFUNC,	int21_fcb,	"parse filename into FCB"},

/* CPM compactability */
    { 0x18,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"CPM"},
    { 0x1d,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"CPM"},
    { 0x1e,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"CPM"},
    { 0x20,	IFT_NOSUBFUNC,	int21_NULLFUNC,	"CPM"},

    { -1,	IFT_NOSUBFUNC,	NULL,		NULL}	/* terminator */

};

static int int21_fastlookup[256];

char *dos_return[] = {
    "OK",
    "FUNC_NUM_IVALID",
    "FILE_NOT_FOUND",
    "PATH_NOT_FOUND",
    "TOO_MANY_OPEN_FILES",
    "ACCESS_DENIED",
    "HANDLE_INVALID",
    "MEM_CB_DEST",
    "INSUF_MEM",
    "MEM_BLK_ADDR_IVALID",
    "ENV_INVALID",
    "FORMAT_INVALID",
    "ACCESS_CODE_INVALID",
    "DATA_INVALID",
    "UNKNOWN_UNIT",
    "DISK_DRIVE_INVALID",
    "ATT_REM_CUR_DIR",
    "NOT_SAME_DEV",
    "NO_MORE_FILES",
    "WRITE_PROT_DISK",
    "UNKNOWN_UNIT_CERR",
    "DRIVE_NOT_READY",
    "UNKNOWN_COMMAND",
    "DATA_ERROR_CRC",
    "BAD_REQ_STRUCT_LEN",
    "SEEK_ERROR",
    "UNKNOWN_MEDIA_TYPE",
    "SECTOR_NOT_FOUND",
    "PRINTER_OUT_OF_PAPER",
    "WRITE_FAULT",
    "READ_FAULT",
    "GENERAL_FAILURE"
};

const int dos_ret_size = (sizeof(dos_return) / sizeof(char *));

/*
** for want of anywhere better to go
*/
static void
int20(regcontext_t *REGS)
{
    /* int 20 = exit(0) */
    done(REGS, 0);
}

static void
int29(regcontext_t *REGS)
{
    tty_write(R_AL, TTYF_REDIRECT);
}

/******************************************************************************
** entrypoint for MS-DOS functions
*/
static void
int21(regcontext_t *REGS)
{
    int error;
    int idx;
    
    /* look for a handler */
    idx = intfunc_find(int21_table, int21_fastlookup, R_AH, R_AL);

    if (idx == -1) {			/* no matching functions */
	unknown_int3(0x21, R_AH, R_AL, REGS);
	R_FLAGS |= PSL_C;               /* Flag an error */
        R_AX = 0xff;
	return;
    }

    /* call the handler */
    error = int21_table[idx].handler(REGS);
    debug(D_DOSCALL, "msdos call %02x (%s) returns %d (%s)\n", 
	  int21_table[idx].func, int21_table[idx].desc,  error,
	  ((error >= 0) && (error <= dos_ret_size)) ? dos_return[error] : "unknown");

    if (error) {
	doserrno = error;
	R_FLAGS |= PSL_C;
	
	/* XXX is this entirely legitimate? */
	if (R_AH >= 0x2f)
	    R_AX = error;
	else
	    R_AX = 0xff;
    } else {
	R_FLAGS &= ~PSL_C;
    }
    return;
}

static void
int67(regcontext_t *REGS)
{
    ems_entry(REGS);
}

static u_char upcase_trampoline[] = {
	0xf4,	/* HLT */
	0xcb,	/* RETF */
};

/*
** initialise thyself
*/
void
dos_init(void)
{
    u_long	vec;

    /* hook vectors */
    vec = insert_softint_trampoline();
    ivec[0x20] = vec;
    register_callback(vec, int20, "int 20");

    vec = insert_softint_trampoline();
    ivec[0x21] = vec;
    register_callback(vec, int21, "int 21");

    vec = insert_softint_trampoline();
    ivec[0x29] = vec;
    register_callback(vec, int29, "int 29");

    vec = insert_softint_trampoline();
    ivec[0x67] = vec;
    register_callback(vec, int67, "int 67 (EMS)");

    vec = insert_null_trampoline();
    ivec[0x28] = vec;	/* dos idle */
    ivec[0x2b] = vec;	/* reserved */
    ivec[0x2c] = vec;	/* reserved */
    ivec[0x2d] = vec;	/* reserved */

    upcase_vector = insert_generic_trampoline(
	sizeof(upcase_trampoline), upcase_trampoline);
    register_callback(upcase_vector, upcase_entry, "upcase");

    /* build fastlookup idx into the monster table of interrupts */
    intfunc_init(int21_table, int21_fastlookup);

    ems_init();
}	
