/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

#if defined(_WIN32) && !defined(__CYGWIN__)

#include "filter_fork.h"

pid_t
__archive_create_child(const char *path, int *child_stdin, int *child_stdout)
{
	HANDLE childStdout[2], childStdin[2], childStdinWr, childStdoutRd;
	SECURITY_ATTRIBUTES secAtts;
	STARTUPINFO staInfo;
	PROCESS_INFORMATION childInfo;
	char cmd[MAX_PATH];
	DWORD mode;

	secAtts.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAtts.bInheritHandle = TRUE;
	secAtts.lpSecurityDescriptor = NULL;
	if (CreatePipe(&childStdout[0], &childStdout[1], &secAtts, 0) == 0)
		goto fail;
	if (DuplicateHandle(GetCurrentProcess(), childStdout[0],
	    GetCurrentProcess(), &childStdoutRd, 0, FALSE,
	    DUPLICATE_SAME_ACCESS) == 0) {
		CloseHandle(childStdout[0]);
		CloseHandle(childStdout[1]);
		goto fail;
	}
	CloseHandle(childStdout[0]);

	if (CreatePipe(&childStdin[0], &childStdin[1], &secAtts, 0) == 0) {
		CloseHandle(childStdoutRd);
		CloseHandle(childStdout[1]);
		goto fail;
	}

	if (DuplicateHandle(GetCurrentProcess(), childStdin[1],
	    GetCurrentProcess(), &childStdinWr, 0, FALSE,
	    DUPLICATE_SAME_ACCESS) == 0) {
		CloseHandle(childStdoutRd);
		CloseHandle(childStdout[1]);
		CloseHandle(childStdin[0]);
		CloseHandle(childStdin[1]);
		goto fail;
	}
	CloseHandle(childStdin[1]);

	memset(&staInfo, 0, sizeof(staInfo));
	staInfo.cb = sizeof(staInfo);
	staInfo.hStdOutput = childStdout[1];
	staInfo.hStdInput = childStdin[0];
	staInfo.wShowWindow = SW_HIDE;
	staInfo.dwFlags = STARTF_USEFILLATTRIBUTE | STARTF_USECOUNTCHARS |
	    STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	strncpy(cmd, path, sizeof(cmd)-1);
	cmd[sizeof(cmd)-1] = '\0';
	if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL,
	    &staInfo, &childInfo) == 0) {
		CloseHandle(childStdoutRd);
		CloseHandle(childStdout[1]);
		CloseHandle(childStdin[0]);
		CloseHandle(childStdinWr);
		goto fail;
	}
	WaitForInputIdle(childInfo.hProcess, INFINITE);
	CloseHandle(childInfo.hProcess);
	CloseHandle(childInfo.hThread);

	mode = PIPE_NOWAIT;
	SetNamedPipeHandleState(childStdoutRd, &mode, NULL, NULL);
	*child_stdout = _open_osfhandle((intptr_t)childStdoutRd, _O_RDONLY);
	*child_stdin = _open_osfhandle((intptr_t)childStdinWr, _O_WRONLY);

	return (childInfo.dwProcessId);

fail:
	return (-1);
}

void
__archive_check_child(int in, int out)
{
	(void)in; /* UNSED */
	(void)out; /* UNSED */
	Sleep(100);
}

#endif /* _WIN32 && !__CYGWIN__ */
