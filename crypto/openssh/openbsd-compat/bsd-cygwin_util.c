/*
 * cygwin_util.c
 *
 * Copyright (c) 2000, 2001, Corinna Vinschen <vinschen@cygnus.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Created: Sat Sep 02 12:17:00 2000 cv
 *
 * This file contains functions for forcing opened file descriptors to
 * binary mode on Windows systems.
 */

#include "includes.h"

RCSID("$Id: bsd-cygwin_util.c,v 1.8 2002/04/15 22:00:52 stevesk Exp $");

#ifdef HAVE_CYGWIN

#include <fcntl.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <windows.h>
#define is_winnt       (GetVersion() < 0x80000000)

#define ntsec_on(c)	((c) && strstr((c),"ntsec") && !strstr((c),"nontsec"))
#define ntea_on(c)	((c) && strstr((c),"ntea") && !strstr((c),"nontea"))

#if defined(open) && open == binary_open
# undef open
#endif
#if defined(pipe) && open == binary_pipe
# undef pipe
#endif

int binary_open(const char *filename, int flags, ...)
{
	va_list ap;
	mode_t mode;
	
	va_start(ap, flags);
	mode = va_arg(ap, mode_t);
	va_end(ap);
	return open(filename, flags | O_BINARY, mode);
}

int binary_pipe(int fd[2])
{
	int ret = pipe(fd);

	if (!ret) {
		setmode (fd[0], O_BINARY);
		setmode (fd[1], O_BINARY);
	}
	return ret;
}

int check_nt_auth(int pwd_authenticated, struct passwd *pw)
{
	/*
	* The only authentication which is able to change the user
	* context on NT systems is the password authentication. So
	* we deny all requsts for changing the user context if another
	* authentication method is used.
	*
	* This doesn't apply to Cygwin versions >= 1.3.2 anymore which
	* uses the undocumented NtCreateToken() call to create a user
	* token if the process has the appropriate privileges and if
	* CYGWIN ntsec setting is on.
	*/
	static int has_create_token = -1;

	if (pw == NULL)
		return 0;
	if (is_winnt) {
		if (has_create_token < 0) {
			struct utsname uts;
		        int major_high = 0, major_low = 0, minor = 0;
			char *cygwin = getenv("CYGWIN");

			has_create_token = 0;
			if (ntsec_on(cygwin) && !uname(&uts)) {
				sscanf(uts.release, "%d.%d.%d",
				       &major_high, &major_low, &minor);
				if (major_high > 1 ||
				    (major_high == 1 && (major_low > 3 ||
				     (major_low == 3 && minor >= 2))))
					has_create_token = 1;
			}
		}
		if (has_create_token < 1 &&
		    !pwd_authenticated && geteuid() != pw->pw_uid)
			return 0;
	}
	return 1;
}

int check_ntsec(const char *filename)
{
	char *cygwin;
	int allow_ntea = 0;
	int allow_ntsec = 0;
	struct statfs fsstat;

	/* Windows 95/98/ME don't support file system security at all. */
	if (!is_winnt)
		return 0;

	/* Evaluate current CYGWIN settings. */
	cygwin = getenv("CYGWIN");
	allow_ntea = ntea_on(cygwin);
	allow_ntsec = ntsec_on(cygwin);

	/*
	 * `ntea' is an emulation of POSIX attributes. It doesn't support
	 * real file level security as ntsec on NTFS file systems does
	 * but it supports FAT filesystems. `ntea' is minimum requirement
	 * for security checks.
	 */
	if (allow_ntea)
		return 1;

	/*
	 * Retrieve file system flags. In Cygwin, file system flags are
	 * copied to f_type which has no meaning in Win32 itself.
	 */
	if (statfs(filename, &fsstat))
		return 1;

	/*
	 * Only file systems supporting ACLs are able to set permissions.
	 * `ntsec' is the setting in Cygwin which switches using of NTFS
	 * ACLs to support POSIX permissions on files.
	 */
	if (fsstat.f_type & FS_PERSISTENT_ACLS)
		return allow_ntsec;

	return 0;
}

void register_9x_service(void)
{
        HINSTANCE kerneldll;
        DWORD (*RegisterServiceProcess)(DWORD, DWORD);

	/* The service register mechanism in 9x/Me is pretty different from
	 * NT/2K/XP.  In NT/2K/XP we're using a special service starter
	 * application to register and control sshd as service.  This method
	 * doesn't play nicely with 9x/Me.  For that reason we register here
	 * as service when running under 9x/Me.  This function is only called
	 * by the child sshd when it's going to daemonize.
	 */
	if (is_winnt)
		return;
	if (! (kerneldll = LoadLibrary("KERNEL32.DLL")))
		return;
	if (! (RegisterServiceProcess = (DWORD (*)(DWORD, DWORD))
			  GetProcAddress(kerneldll, "RegisterServiceProcess")))
		return;
	RegisterServiceProcess(0, 1);
}

#endif /* HAVE_CYGWIN */
