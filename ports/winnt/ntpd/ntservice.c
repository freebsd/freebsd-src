/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#include <ntp_stdlib.h>
#include "syslog.h"
#include "ntpd.h"
#include "ntservice.h"
#include "clockstuff.h"
#include "ntp_iocompletionport.h"
#include "ntpd-opts.h"
#include "isc/win32os.h"
#include <ssl_applink.c>


/*
 * Globals
 */
static SERVICE_STATUS_HANDLE hServiceStatus = 0;
static BOOL foreground = FALSE;
static BOOL computer_shutting_down = FALSE;
static int glb_argc;
static char **glb_argv;
HANDLE hServDoneEvent = NULL;
extern int accept_wildcard_if_for_winnt;

/*
 * Forward declarations
 */
extern void uninit_io_completion_port();
extern int ntpdmain(int argc, char *argv[]);
extern void WINAPI ServiceControl(DWORD dwCtrlCode);
extern void ntservice_exit(void);

#ifdef WRAP_DBG_MALLOC
void *wrap_dbg_malloc(size_t s, const char *f, int l);
void *wrap_dbg_realloc(void *p, size_t s, const char *f, int l);
void wrap_dbg_free(void *p);
void wrap_dbg_free_ex(void *p, const char *f, int l);
#endif

void WINAPI
service_main(
	DWORD argc,
	LPTSTR *argv
	)
{
	if (argc > 1) {
		/*
		 * Let command line parameters from the Windows SCM GUI
		 * override the standard parameters from the ImagePath registry key.
		 */
		glb_argc = argc;
		glb_argv = argv;
	}

	ntpdmain(glb_argc, glb_argv);
}


/*
 * This is the entry point for the executable 
 * We can call ntpdmain() explicitly or via StartServiceCtrlDispatcher()
 * as we need to.
 */
int main(
	int	argc,
	char **	argv
	)
{
	int	rc;
	int	argc_after_opts;
	char **	argv_after_opts;

	ssl_applink();

	/* Save the command line parameters */
	glb_argc = argc;
	glb_argv = argv;

	/* Under original Windows NT we must not discard the wildcard */
	/* socket to workaround a bug in NT's getsockname(). */
	if (isc_win32os_majorversion() <= 4)
		accept_wildcard_if_for_winnt = TRUE;

	argc_after_opts = argc;
	argv_after_opts = argv;
	parse_cmdline_opts(&argc_after_opts, &argv_after_opts);

	if (HAVE_OPT(QUIT)
	    || HAVE_OPT(SAVECONFIGQUIT)
	    || HAVE_OPT(HELP)
#ifdef DEBUG
	    || OPT_VALUE_SET_DEBUG_LEVEL != 0
#endif
	    || HAVE_OPT(NOFORK))
		foreground = TRUE;

	if (foreground)			/* run in console window */
		rc = ntpdmain(argc, argv);
	else {
		/* Start up as service */

		SERVICE_TABLE_ENTRY dispatchTable[] = {
			{ TEXT(NTP_DISPLAY_NAME), service_main },
			{ NULL, NULL }
		};

		rc = StartServiceCtrlDispatcher(dispatchTable);
		if (rc)
			rc = 0; 
		else {
			rc = GetLastError();
			fprintf(stderr,
				"%s: unable to start as service:\n"
				"%s\n"
				"Use -d, -q, -n, -?, --help or "
				"--saveconfigquit to run "
				"interactive.\n",
				argv[0], ntp_strerror(rc));
		}
	}
	return rc;
}


/*
 * Initialize the Service by registering it.
 */
void
ntservice_init(void)
{
	char ConsoleTitle[256];

	if (!foreground) {
		/* Register handler with the SCM */
		hServiceStatus = RegisterServiceCtrlHandler(NTP_DISPLAY_NAME,
					ServiceControl);
		if (!hServiceStatus) {
			NTReportError(NTP_SERVICE_NAME,
				"could not register with SCM");
			exit(1);
		}
		UpdateSCM(SERVICE_START_PENDING);
	} else {
		snprintf(ConsoleTitle, sizeof(ConsoleTitle),
			 "NTP Version %s", Version);
		ConsoleTitle[sizeof(ConsoleTitle) - 1] = '\0';
		SetConsoleTitle(ConsoleTitle);
	}

#ifdef _CRTDBG_MAP_ALLOC
		/* ask the runtime to dump memory leaks at exit */
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF
			       | _CRTDBG_LEAK_CHECK_DF		/* report on leaks at exit */
			       | _CRTDBG_CHECK_ALWAYS_DF	/* Check heap every alloc/dealloc */
#ifdef MALLOC_LINT
			       | _CRTDBG_DELAY_FREE_MEM_DF	/* Don't actually free memory */
#endif
			       );
#ifdef DOES_NOT_WORK
			/*
			 * hart: I haven't seen this work, running ntpd.exe -n from a shell
			 * to both a file and the debugger output window.  Docs indicate it
			 * should cause leak report to go to stderr, but it's only seen if
			 * ntpd runs under a debugger (in the debugger's output), even with
			 * this block of code enabled.
			 */
			_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
			_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
#endif
#endif /* using MS debug C runtime heap, _CRTDBG_MAP_ALLOC */

	atexit( ntservice_exit );
}

void
ntservice_isup(void)
{
	if (!foreground) {
		/* Register handler with the SCM */
		if (!hServiceStatus) {
			NTReportError(NTP_SERVICE_NAME,
				"could not report to SCM");
			exit(1);
		}
		UpdateSCM(SERVICE_RUNNING);
	}
}

/*
 * Routine to check if the service is stopping
 * because the computer is shutting down
 */
BOOL
ntservice_systemisshuttingdown(void)
{
	return computer_shutting_down;
}

void
ntservice_exit(void)
{
	uninit_io_completion_port();
	Sleep( 200 );  	//##++ 

	reset_winnt_time();

	msyslog(LOG_INFO, "ntservice: The Network Time Protocol Service is stopping.");

	if (!foreground) {
		/* service mode, need to have the service_main routine
		 * register with the service control manager that the 
		 * service has stopped running, before exiting
		 */
		UpdateSCM(SERVICE_STOPPED);
	}
}

/* 
 * ServiceControl(): Handles requests from the SCM and passes them on
 * to the service.
 */
void WINAPI
ServiceControl(
	DWORD dwCtrlCode
	) 
{
	static const char * const msg_tab[2] = {
		"explicit stop",
		"system shutdown"
	};

	switch (dwCtrlCode) {

	case SERVICE_CONTROL_SHUTDOWN:
		computer_shutting_down = TRUE;
		/* fall through to stop case */

	case SERVICE_CONTROL_STOP:
		if (WaitableExitEventHandle != NULL) {
			msyslog(LOG_INFO, "SCM requests stop (%s)",
				msg_tab[!!computer_shutting_down]);
			UpdateSCM(SERVICE_STOP_PENDING);
			SetEvent(WaitableExitEventHandle);
			Sleep(100);  //##++
			break;
		}
		msyslog(LOG_ERR, "SCM requests stop (%s), but have no exit event!",
			msg_tab[!!computer_shutting_down]);
		/* FALLTHROUGH */

	case SERVICE_CONTROL_PAUSE:
	case SERVICE_CONTROL_CONTINUE:
	case SERVICE_CONTROL_INTERROGATE:
	default:
		UpdateSCM(SERVICE_RUNNING);
		break;
	}
}

/*
 * Tell the Service Control Manager the state of the service.
 */
void
UpdateSCM(
	DWORD state
	)
{
	static DWORD dwState = SERVICE_STOPPED;
	static DWORD dwCheck = 0;

	SERVICE_STATUS ss;

	if (hServiceStatus) {
		if (state)
			dwState = state;

		ZERO(ss);
		ss.dwServiceType |= SERVICE_WIN32_OWN_PROCESS;
		ss.dwCurrentState = dwState;
		/* ss.dwServiceSpecificExitCode = 0; default by ZERO(ss) */
		/* ss.dwWin32ExitCode = NO_ERROR;    default by ZERO(ss) */

		switch (dwState) {
		case SERVICE_START_PENDING:
			ss.dwControlsAccepted = 0;
			ss.dwWaitHint = 15000;
			ss.dwCheckPoint = ++dwCheck;
			break;
		case SERVICE_STOP_PENDING:
			ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
			ss.dwWaitHint = 3000;
			ss.dwCheckPoint = ++dwCheck;
			break;
		default:
			ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
			ss.dwWaitHint = 1000;
			ss.dwCheckPoint = dwCheck = 0;
			break;
		}

		SetServiceStatus(hServiceStatus, &ss);
	}
}

BOOL WINAPI 
OnConsoleEvent(  
	DWORD dwCtrlType
	)
{
	switch (dwCtrlType) {
#ifdef DEBUG
		case CTRL_BREAK_EVENT:
			if (debug > 0) {
				debug <<= 1;
			}
			else {
				debug = 1;
			}
			if (debug > 8) {
				debug = 0;
			}
			msyslog(LOG_DEBUG, "debug level %d", debug);
			break;
#else
		case CTRL_BREAK_EVENT:
			break;
#endif

		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			if (WaitableExitEventHandle != NULL) {
				SetEvent(WaitableExitEventHandle);
				Sleep(100);  //##++
			}
			break;

		default :
			/* pass to next handler */
			return FALSE; 
	}

	/* we've handled it, no more handlers should be called */
	return TRUE;
}

