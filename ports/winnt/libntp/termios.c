/* This file implements system calls that are not compatible with UNIX */
/* Moved to libntp/termios.c */

#include <config.h>
#include <io.h>
#include <stdio.h>

#include "ntp.h"
#include "ntp_tty.h"
#include "lib_strbuf.h"
#include "ntp_assert.h"
#include "win32_io.h"

#include "ntp_iocplmem.h"
#include "ntp_iocpltypes.h"

/* -------------------------------------------------------------------
 * COM port management
 *
 * com port handling needs some special functionality, especially for
 * PPS support. There are things that are shared by the Windows Kernel
 * on device level, not handle level. These include IOCPL membership,
 * event wait slot, ... It's also no so simple to open a device a
 * second time, and so we must manage the handles on open com ports
 * in userland. Well, partially.
 */
#define MAX_SERIAL 255	/* COM1: - COM255: */
#define MAX_COMDUP 8	/* max. allowed number of dupes per device */

typedef struct comhandles_tag {
	uint16_t	unit;	/* COMPORT number		*/
	uint16_t	nhnd;	/* number of open handles	*/
	char *		comName;/* windows device name		*/
	DevCtx_t *	devCtx;	/* shared device context	*/
	HANDLE		htab[MAX_COMDUP];	/* OS handles	*/
} comhandles;

comhandles **	tab_comh;	/* device data table		*/
size_t		num_comh;	/* current used array size	*/
size_t		max_comh;	/* current allocated array size	*/

/* lookup a COM unit by a handle
 * Scans all used units for a matching handle. Returns the slot
 * or NULL on failure.
 *
 * If 'phidx' is given, the index in the slots handle table that
 * holds the handle is also returned.
 *
 * This a simple 2d table scan. But since we don't expect to have
 * hundreds of com ports open, this should be no problem.
 */
static comhandles*
lookup_com_handle(
	HANDLE		h,
	size_t *	phidx
	)
{
	size_t		tidx, hidx;
	comhandles *	slot;
	for (tidx = 0; tidx < num_comh; ++tidx) {
		slot = tab_comh[tidx];
		for (hidx = 0; hidx < slot->nhnd; ++hidx) {
			if (slot->htab[hidx] == h) {
				if (phidx != NULL)
					*phidx = hidx;
				return slot;
			}
		}
	}
	return NULL;
}

/* lookup the list of COM units by unit number. This will always return
 * a valid location -- eventually the table gets expanded, and a new
 * entry is returned. In that case, the structure is set up with all
 * entries valid and *no* file handles yet.
 */
static comhandles*
insert_com_unit(
	uint16_t unit
)
{
	size_t		tidx;
	comhandles *	slot;

	/* search for matching entry and return if found */
	for (tidx = 0; tidx < num_comh; ++tidx)
		if (tab_comh[tidx]->unit == unit)
			return tab_comh[tidx];

	/* search failed. make sure we can add a new slot */
	if (num_comh >= max_comh) {
		/* round up to next multiple of 4 */
		max_comh = (num_comh + 4) & ~(size_t)3;
		tab_comh = erealloc(tab_comh, max_comh * sizeof(tab_comh[0]));
	}

	/* create a new slot and populate it. */
	slot = emalloc_zero(sizeof(comhandles));
	LIB_GETBUF(slot->comName);
	snprintf(slot->comName, LIB_BUFLENGTH, "\\\\.\\COM%d", unit);
	slot->comName = estrdup(slot->comName);
	slot->devCtx  = DevCtxAlloc();
	slot->unit    = unit;

	/* plug it into table and return it */
	tab_comh[num_comh++] = slot;
	return slot;
}

/* remove a COM slot from the table and destroy it. */
static void
remove_com_slot(
	comhandles *	slot	/* must be valid! */
	)
{
	size_t	tidx;
	for (tidx = 0; tidx < num_comh; ++tidx)
		if (tab_comh[tidx] == slot) {
			tab_comh[tidx] = tab_comh[--num_comh];
			break;
		}

	DevCtxDetach(slot->devCtx);
	free(slot->comName);
	free(slot);
}

/* fetch the stored device context block.
 * This does NOT step the reference counter!
 */
DevCtx_t*
serial_devctx(
	HANDLE	h
	)
{
	comhandles * slot = NULL;
	if (INVALID_HANDLE_VALUE != h && NULL != h)
		slot = lookup_com_handle(h, NULL);
	return (NULL != slot) ? slot->devCtx : NULL;
}


/*
 * common_serial_open ensures duplicate opens of the same port
 * work by duplicating the handle for the 2nd open, allowing
 * refclock_atom to share a GPS refclock's comm port.
 */
HANDLE
common_serial_open(
	const char *	dev,
	const char **	pwindev
	)
{
	HANDLE		handle;
	size_t		unit;
	const char *	pch;
	comhandles *	slot;

	/*
	 * This is odd, but we'll take any unix device path
	 * by looking for the initial '/' and strip off everything
	 * before the final digits, then translate that to COM__:
	 * maintaining backward compatibility with NTP practice of
	 * mapping unit 0 to the nonfunctional COM0:
	 *
	 * To ease the job of taking the windows COMx: device names
	 * out of reference clocks, we'll also work with those
	 * equanimously.
	 */

	TRACE(1, ("common_serial_open given %s\n", dev));

	handle = INVALID_HANDLE_VALUE;

	pch = NULL;
	if ('/' == dev[0]) {
		pch = dev + strlen(dev);
		while (isdigit((u_char)pch[-1]))
			--pch;
		TRACE(1, ("common_serial_open skipped to ending digits leaving %s\n", pch));
	} else if (0 == _strnicmp("COM", dev, 3)) {
		pch = dev + 3;
		TRACE(1, ("common_serial_open skipped COM leaving %s\n", pch));
	}

	if (!pch || !isdigit((u_char)pch[0])) {
		TRACE(1, ("not a digit: %s\n", pch ? pch : "[NULL]"));
		return INVALID_HANDLE_VALUE;
	}

	unit = strtoul(pch, (char**)&pch, 10);
	if (*pch || unit > MAX_SERIAL) {
		TRACE(1, ("conversion failure: unit=%u at '%s'\n", pch));
		return INVALID_HANDLE_VALUE;
	}

	/* Now.... find the COM slot, and either create a new file
	 * (if there is no handle yet) or duplicate one of the existing
	 * handles. Unless the dup table for one com port would overflow,
	 * but that's an indication of a programming error somewhere.
	 */
	slot = insert_com_unit(unit);
	if (slot->nhnd == 0) {
		TRACE(1, ("windows device %s\n", slot->comName));
		slot->htab[0] = CreateFileA(
				slot->comName,
			GENERIC_READ | GENERIC_WRITE,
			0, /* sharing prohibited */
			NULL, /* default security */
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);
		if (INVALID_HANDLE_VALUE != slot->htab[0]) {
			slot->nhnd     = 1;
			handle         = slot->htab[0];
			*pwindev       = slot->comName;
		}
	} else if (slot->nhnd >= MAX_COMDUP) {
		SetLastError(ERROR_TOO_MANY_OPEN_FILES);
	} else if (DuplicateHandle(GetCurrentProcess(), slot->htab[0],
				   GetCurrentProcess(), &slot->htab[slot->nhnd],
				   0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		handle = slot->htab[slot->nhnd++];
		*pwindev = slot->comName;
	}

	return handle;
}

/*
 * closeserial() is used in place of close by ntpd refclock I/O for ttys
 */
int
closeserial(
	int	fd
	)
{
	HANDLE	h;
	size_t	hidx;
	comhandles *	slot;

	h = (HANDLE)_get_osfhandle(fd);
	if (INVALID_HANDLE_VALUE == h)
		goto onerror;

	slot = lookup_com_handle(h, &hidx);
	if (NULL == slot)
		goto onerror;

	slot->htab[hidx] = slot->htab[--slot->nhnd];
	if (slot->nhnd == 0)
		remove_com_slot(slot);

	return close(fd); /* closes system handle, too! */

onerror:
	errno = EBADF;
	return -1;
}

/*
 * isserialhandle() -- check if a handle is a COM port handle
 */
int/*BOOL*/
isserialhandle(
	HANDLE h
	)
{
	if (INVALID_HANDLE_VALUE != h && NULL != h)
		return lookup_com_handle(h, NULL) != NULL;
	return FALSE;
}


/*
 * tty_open - open serial port for refclock special uses
 *
 * This routine opens a serial port for and returns the 
 * file descriptor if success and -1 if failure.
 */
int
tty_open(
	const char *dev,	/* device name pointer */
	int access,		/* O_RDWR */
	int mode		/* unused */
	)
{
	HANDLE		Handle;
	const char *	windev;

	/*
	 * open communication port handle
	 */
	windev = dev;
	Handle = common_serial_open(dev, &windev);

	if (Handle == INVALID_HANDLE_VALUE) {  
		msyslog(LOG_ERR, "tty_open: device %s CreateFile error: %m", windev);
		errno = EMFILE; /* lie, lacking conversion from GetLastError() */
		return -1;
	}

	return _open_osfhandle((intptr_t)Handle, _O_TEXT);
}


/*
 * refclock_open - open serial port for reference clock
 *
 * This routine opens a serial port for I/O and sets default options. It
 * returns the file descriptor or -1 indicating failure.
 */
int
refclock_open(
	const char *	dev,	/* device name pointer */
	u_int		speed,	/* serial port speed (code) */
	u_int		flags	/* line discipline flags */
	)
{
	const char *	windev;
	HANDLE		h;
	COMMTIMEOUTS	timeouts;
	DCB		dcb;
	DWORD		dwEvtMask;
	int		fd;
	int		translate;

	/*
	 * open communication port handle
	 */
	windev = dev;
	h = common_serial_open(dev, &windev);

	if (INVALID_HANDLE_VALUE == h) {
		SAVE_ERRNO(
			msyslog(LOG_ERR, "CreateFile(%s) error: %m",
				windev);
		)
		return -1;
	}

	/* Change the input/output buffers to be large. */
	if (!SetupComm(h, 1024, 1024)) {
		SAVE_ERRNO(
			msyslog(LOG_ERR, "SetupComm(%s) error: %m",
				windev);
		)
		return -1;
	}

	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(h, &dcb)) {
		SAVE_ERRNO(
			msyslog(LOG_ERR,
				"GetCommState(%s) error: %m",
				windev);
		)
		return -1;
	}

	switch (speed) {

	case B300:
		dcb.BaudRate = 300;
		break;

	case B1200:  
		dcb.BaudRate = 1200;
		break;

	case B2400:
		dcb.BaudRate = 2400;
		break;

	case B4800:
		dcb.BaudRate = 4800;
		break;

	case B9600:
		dcb.BaudRate = 9600;
		break;

	case B19200:
		dcb.BaudRate = 19200;
		break;

	case B38400:
		dcb.BaudRate = 38400;
		break;

	case B57600:
		dcb.BaudRate = 57600;
		break;

	case B115200:
		dcb.BaudRate = 115200;
		break;

	default:
		msyslog(LOG_ERR, "%s unsupported bps code %u", windev,
			speed);
		SetLastError(ERROR_INVALID_PARAMETER);
		return -1;
	}

	dcb.fBinary = TRUE;
	dcb.fParity = TRUE;
	dcb.fOutxCtsFlow = 0;
	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fDsrSensitivity = 0;
	dcb.fTXContinueOnXoff = TRUE;
	dcb.fOutX = 0; 
	dcb.fInX = 0;
	dcb.fErrorChar = 0;
	dcb.fNull = 0;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fAbortOnError = 0;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = NOPARITY;
	dcb.ErrorChar = 0;
	dcb.EofChar = 0;
	if (LDISC_RAW & flags)
		dcb.EvtChar = 0;
	else
		dcb.EvtChar = '\r';

	if (!SetCommState(h, &dcb)) {
		SAVE_ERRNO(
			msyslog(LOG_ERR, "SetCommState(%s) error: %m",
				windev);
		)
		return -1;
	}

	/* watch out for CR (dcb.EvtChar) as well as the CD line */
	dwEvtMask = EV_RLSD;
	if (LDISC_RAW & flags)
		dwEvtMask |= EV_RXCHAR;
	else
		dwEvtMask |= EV_RXFLAG;
	if (!SetCommMask(h, dwEvtMask)) {
		SAVE_ERRNO(
			msyslog(LOG_ERR, "SetCommMask(%s) error: %m",
				windev);
		)
		return -1;
	}

	/* configure the handle to never block */
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;

	if (!SetCommTimeouts(h, &timeouts)) {
		SAVE_ERRNO(
			msyslog(LOG_ERR,
				"Device %s SetCommTimeouts error: %m",
				windev);
		)
		return -1;
	}

	translate = (LDISC_RAW & flags)
			? 0
			: _O_TEXT;
	fd = _open_osfhandle((intptr_t)h, translate);
	/* refclock_open() long returned 0 on failure, avoid it. */
	if (0 == fd) {
		fd = _dup(0);
		_close(0);
	}

	return fd;
}


int
ioctl_tiocmget(
	HANDLE h,
	int *pi
	)
{
	DWORD	dw;

	if (!GetCommModemStatus(h, &dw)) {
		errno = ENOTTY;
		return -1;
	}

	*pi = ((dw & MS_CTS_ON)  ? TIOCM_CTS : 0)
	    | ((dw & MS_DSR_ON)  ? TIOCM_DSR : 0)
	    | ((dw & MS_RLSD_ON) ? TIOCM_CAR : 0)
	    | ((dw & MS_RING_ON) ? TIOCM_RI  : 0);

	return 0;
}


int
ioctl_tiocmset(
	HANDLE h,
	int *pi
	)
{
	BOOL	failed;
	int	result;
	
	failed = !EscapeCommFunction(
			h, 
			(*pi & TIOCM_RTS) 
			    ? SETRTS
			    : CLRRTS
			);

	if (!failed)
		failed = !EscapeCommFunction(
				h, 
				(*pi & TIOCM_DTR) 
				    ? SETDTR
				    : CLRDTR
				);

	if (failed) {
		errno = ENOTTY;
		result = -1;
	} else
		result = 0;

	return result;
}


int 
ioctl(
	int fd,
	int op,
	void *pv
	)
{
	HANDLE	h;
	int	result;
	int	modctl;
	int *pi = (int *) pv;
	
	h = (HANDLE)_get_osfhandle(fd);

	if (INVALID_HANDLE_VALUE == h) {
		/* errno already set */
		return -1;
	}

	switch (op) {

	case TIOCMGET:
		result = ioctl_tiocmget(h, pi);
		break;

	case TIOCMSET:
		result = ioctl_tiocmset(h, pi);
		break;

	case TIOCMBIC:
		result = ioctl_tiocmget(h, &modctl);
		if (result < 0)
			return result;
		modctl &= ~(*pi);
		result = ioctl_tiocmset(h, &modctl);
		break;

	case TIOCMBIS:
		result = ioctl_tiocmget(h, &modctl);
		if (result < 0)
			return result;
		modctl |= *pi;
		result = ioctl_tiocmset(h, &modctl);
		break;

	default:
		errno = EINVAL;
		result = -1;
	}

	return result;
}


int	
tcsetattr(
	int			fd, 
	int			optional_actions, 
	const struct termios *	tios
	)
{
	DCB dcb;
	HANDLE h;

	UNUSED_ARG(optional_actions);

	h = (HANDLE)_get_osfhandle(fd);

	if (INVALID_HANDLE_VALUE == h) {
		/* errno already set */
		return -1;
	}

	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(h, &dcb)) {
		errno = ENOTTY;
		return -1;
	}

	switch (max(tios->c_ospeed, tios->c_ispeed)) {

	case B300:
		dcb.BaudRate = 300;
		break;

	case B1200:
		dcb.BaudRate = 1200;
		break;

	case B2400:
		dcb.BaudRate = 2400;
		break;

	case B4800:
		dcb.BaudRate = 4800;
		break;

	case B9600:
		dcb.BaudRate = 9600;
		break;

	case B19200:
		dcb.BaudRate = 19200;
		break;

	case B38400:
		dcb.BaudRate = 38400;
		break;

	case B57600:
		dcb.BaudRate = 57600;
		break;

	case B115200:
		dcb.BaudRate = 115200;
		break;

	default:
		msyslog(LOG_ERR, "unsupported serial baud rate");
		errno = EINVAL;
		return -1;
	}

	switch (tios->c_cflag & CSIZE) {

	case CS5:
		dcb.ByteSize = 5;
		break;

	case CS6:
		dcb.ByteSize = 6;
		break;

	case CS7:
		dcb.ByteSize = 7;
		break;

	case CS8:
		dcb.ByteSize = 8;
		break;

	default:
		msyslog(LOG_ERR, "unsupported serial word size");
		errno = EINVAL;
		return FALSE;
	}

	if (PARENB & tios->c_cflag) {
		dcb.fParity = TRUE;
		dcb.Parity = (tios->c_cflag & PARODD)
				? ODDPARITY
				: EVENPARITY;
	} else {
		dcb.fParity = FALSE;
		dcb.Parity = NOPARITY;
	}

	dcb.StopBits = (CSTOPB & tios->c_cflag)
			? TWOSTOPBITS
			: ONESTOPBIT;

	if (!SetCommState(h, &dcb)) {
		errno = ENOTTY;
		return -1;
	}

	return 0;
}


int
tcgetattr(
	int		fd,
	struct termios *tios
	)
{
	DCB	dcb;
	HANDLE	h;

	h = (HANDLE)_get_osfhandle(fd);

	if (INVALID_HANDLE_VALUE == h) {
		/* errno already set */
		return -1;
	}

	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(h, &dcb)) {
		errno = ENOTTY;
		return -1;
	}

	/*  Set c_ispeed & c_ospeed */

	switch (dcb.BaudRate) {

	case 300:
		tios->c_ispeed = tios->c_ospeed = B300;
		break;

	case 1200: 
		tios->c_ispeed = tios->c_ospeed = B1200;
		break;

	case 2400:
		tios->c_ispeed = tios->c_ospeed = B2400;
		break;

	case 4800: 
		tios->c_ispeed = tios->c_ospeed = B4800;
		break;

	case 9600:
		tios->c_ispeed = tios->c_ospeed = B9600;
		break;

	case 19200:
		tios->c_ispeed = tios->c_ospeed = B19200;
		break;

	case 38400:
		tios->c_ispeed = tios->c_ospeed = B38400;
		break;

	case 57600:
		tios->c_ispeed = tios->c_ospeed = B57600;
		break;

	case 115200:
		tios->c_ispeed = tios->c_ospeed = B115200;
		break;

	default:
		tios->c_ispeed = tios->c_ospeed = B9600;
	}
	

	switch (dcb.ByteSize) {
		case 5:
			tios->c_cflag = CS5;
			break;

		case 6:
			tios->c_cflag = CS6;
			break;

		case 7: 
			tios->c_cflag = CS7; 
			break;

		case 8:
		default:
			tios->c_cflag = CS8;
	}

	if (dcb.fParity) {
		tios->c_cflag |= PARENB;

		if (ODDPARITY == dcb.Parity)
			tios->c_cflag |= PARODD;
	}

	if (TWOSTOPBITS == dcb.StopBits)
		tios->c_cflag |= CSTOPB;

	tios->c_iflag = 0;
	tios->c_lflag = 0;
	tios->c_line = 0;
	tios->c_oflag = 0;

	return 0;
}


int
tcflush(
	int fd,
	int mode
	)
{
	HANDLE	h;
	BOOL	success;
	DWORD	flags;
	int	result;

	h = (HANDLE)_get_osfhandle(fd);

	if (INVALID_HANDLE_VALUE == h) {
		/* errno already set */
		return -1;
	}

	switch (mode) {

	case TCIFLUSH:
		flags = PURGE_RXCLEAR;
		break;

	case TCOFLUSH:
		flags = PURGE_TXABORT;
		break;

	case TCIOFLUSH:
		flags = PURGE_RXCLEAR | PURGE_TXABORT;
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	success = PurgeComm(h, flags);

	if (success)
		result = 0;
	else {
		errno = ENOTTY;
		result = -1;
	}

	return result;
}

