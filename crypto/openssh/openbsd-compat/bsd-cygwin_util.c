/*
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

#ifdef HAVE_CYGWIN

#if defined(open) && open == binary_open
# undef open
#endif
#if defined(pipe) && open == binary_pipe
# undef pipe
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/vfs.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <windows.h>

#include "xmalloc.h"
#define is_winnt       (GetVersion() < 0x80000000)

#define ntsec_on(c)	((c) && strstr((c),"ntsec") && !strstr((c),"nontsec"))
#define ntsec_off(c)	((c) && strstr((c),"nontsec"))
#define ntea_on(c)	((c) && strstr((c),"ntea") && !strstr((c),"nontea"))

int 
binary_open(const char *filename, int flags, ...)
{
	va_list ap;
	mode_t mode;
	
	va_start(ap, flags);
	mode = va_arg(ap, mode_t);
	va_end(ap);
	return (open(filename, flags | O_BINARY, mode));
}

int 
binary_pipe(int fd[2])
{
	int ret = pipe(fd);

	if (!ret) {
		setmode(fd[0], O_BINARY);
		setmode(fd[1], O_BINARY);
	}
	return (ret);
}

#define HAS_CREATE_TOKEN 1
#define HAS_NTSEC_BY_DEFAULT 2
#define HAS_CREATE_TOKEN_WO_NTSEC 3

static int 
has_capability(int what)
{
	static int inited;
	static int has_create_token;
	static int has_ntsec_by_default;
	static int has_create_token_wo_ntsec;

	/* 
	 * has_capability() basically calls uname() and checks if
	 * specific capabilities of Cygwin can be evaluated from that.
	 * This simplifies the calling functions which only have to ask
	 * for a capability using has_capability() instead of having
	 * to figure that out by themselves.
	 */
	if (!inited) {
		struct utsname uts;
		
		if (!uname(&uts)) {
			int major_high = 0, major_low = 0, minor = 0;
			int api_major_version = 0, api_minor_version = 0;
			char *c;

			sscanf(uts.release, "%d.%d.%d", &major_high,
			    &major_low, &minor);
			if ((c = strchr(uts.release, '(')) != NULL) {
				sscanf(c + 1, "%d.%d", &api_major_version,
				    &api_minor_version);
			}
			if (major_high > 1 ||
			    (major_high == 1 && (major_low > 3 ||
			    (major_low == 3 && minor >= 2))))
				has_create_token = 1;
			if (api_major_version > 0 || api_minor_version >= 56)
				has_ntsec_by_default = 1;
			if (major_high > 1 ||
			    (major_high == 1 && major_low >= 5))
				has_create_token_wo_ntsec = 1;
			inited = 1;
		}
	}
	switch (what) {
	case HAS_CREATE_TOKEN:
		return (has_create_token);
	case HAS_NTSEC_BY_DEFAULT:
		return (has_ntsec_by_default);
	case HAS_CREATE_TOKEN_WO_NTSEC:
		return (has_create_token_wo_ntsec);
	}
	return (0);
}

int
check_nt_auth(int pwd_authenticated, struct passwd *pw)
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
			char *cygwin = getenv("CYGWIN");

			has_create_token = 0;
			if (has_capability(HAS_CREATE_TOKEN) &&
			    (ntsec_on(cygwin) ||
			    (has_capability(HAS_NTSEC_BY_DEFAULT) &&
			     !ntsec_off(cygwin)) ||
			     has_capability(HAS_CREATE_TOKEN_WO_NTSEC)))
				has_create_token = 1;
		}
		if (has_create_token < 1 &&
		    !pwd_authenticated && geteuid() != pw->pw_uid)
			return (0);
	}
	return (1);
}

int
check_ntsec(const char *filename)
{
	return (pathconf(filename, _PC_POSIX_PERMISSIONS));
}

void
register_9x_service(void)
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
	if (!(kerneldll = LoadLibrary("KERNEL32.DLL")))
		return;
	if (!(RegisterServiceProcess = (DWORD (*)(DWORD, DWORD))
		GetProcAddress(kerneldll, "RegisterServiceProcess")))
		return;
	RegisterServiceProcess(0, 1);
}

#define NL(x) x, (sizeof (x) - 1)
#define WENV_SIZ (sizeof (wenv_arr) / sizeof (wenv_arr[0]))

static struct wenv {
	const char *name;
	size_t namelen;
} wenv_arr[] = {
	{ NL("ALLUSERSPROFILE=") },
	{ NL("COMMONPROGRAMFILES=") },
	{ NL("COMPUTERNAME=") },
	{ NL("COMSPEC=") },
	{ NL("CYGWIN=") },
	{ NL("NUMBER_OF_PROCESSORS=") },
	{ NL("OS=") },
	{ NL("PATH=") },
	{ NL("PATHEXT=") },
	{ NL("PROCESSOR_ARCHITECTURE=") },
	{ NL("PROCESSOR_IDENTIFIER=") },
	{ NL("PROCESSOR_LEVEL=") },
	{ NL("PROCESSOR_REVISION=") },
	{ NL("PROGRAMFILES=") },
	{ NL("SYSTEMDRIVE=") },
	{ NL("SYSTEMROOT=") },
	{ NL("TMP=") },
	{ NL("TEMP=") },
	{ NL("WINDIR=") }
};

char **
fetch_windows_environment(void)
{
	char **e, **p;
	unsigned int i, idx = 0;

	p = xcalloc(WENV_SIZ + 1, sizeof(char *));
	for (e = environ; *e != NULL; ++e) {
		for (i = 0; i < WENV_SIZ; ++i) {
			if (!strncmp(*e, wenv_arr[i].name, wenv_arr[i].namelen))
				p[idx++] = *e;
		}
	}
	p[idx] = NULL;
	return p;
}

void
free_windows_environment(char **p)
{
	xfree(p);
}

#endif /* HAVE_CYGWIN */
