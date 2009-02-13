2004-11-04  Per Bothner  <per@bothner.com>

	* pty.c:  Import from screen-4.0.2.
	* configure.in, Makefile.in, config.h.in:  Set up autoconf handling,
	copying a bunk of stuff over from screen.
	* rlfe.c:  Use OpenPTY from pty.c instead of get_master_pty.

2004-11-03  Per Bothner  <per@bothner.com>

	* rlfe.c:  Get input emphasis (boldening) more robust.

	* rlfe.c:  Various cleanups on comments and names.

2003-11-07 Wolfgang Taeuber <wolfgang_taeuber@agilent.com>

	* Specify a history file and the size of the history file with command
	* line options; use EDITOR/VISUAL to set vi/emacs preference.

1999-09-03  Chet Ramey <chet@nike.ins.cwru.edu>

	* fep.c: Memmove is not universally available.  This patch assumes
 	that an autoconf test has been performed, and that memcpy is
 	available without checking.

	* fep.c: VDISCARD is not universally available, even when termios is.

	* fep.c: If a system doesn't have TIOCSCTTY, the first `open'
 	performed after setsid allocates a controlling terminal.  The
 	original code would leave the child process running on the slave pty
 	without a controlling tty if TIOCSCTTY was not available.

	* fep.c: Most versions of SVR4, including solaris, don't allow
	terminal ioctl calls on the master side of the pty.

1999-08-28  Per Bothner  <per@bothner.com>

	* fep.c:  Initial release.
