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
	HANDLE childStdout[2], childStdin[2],childStderr;
	SECURITY_ATTRIBUTES secAtts;
	STARTUPINFO staInfo;
	PROCESS_INFORMATION childInfo;
	char cmd[MAX_PATH];

	secAtts.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAtts.bInheritHandle = TRUE;
	secAtts.lpSecurityDescriptor = NULL;
	if (CreatePipe(&childStdout[0], &childStdout[1], &secAtts, 0) == 0)
		goto fail;
	if (!SetHandleInformation(childStdout[0], HANDLE_FLAG_INHERIT, 0))
	{
		CloseHandle(childStdout[0]);
		CloseHandle(childStdout[1]);
		goto fail;
	}
	if (CreatePipe(&childStdin[0], &childStdin[1], &secAtts, 0) == 0) {
		CloseHandle(childStdout[0]);
		CloseHandle(childStdout[1]);
		goto fail;
	}
	if (!SetHandleInformation(childStdin[1], HANDLE_FLAG_INHERIT, 0))
	{
		CloseHandle(childStdout[0]);
		CloseHandle(childStdout[1]);
		CloseHandle(childStdin[0]);
		CloseHandle(childStdin[1]);
		goto fail;
	}
	if (DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE),
	    GetCurrentProcess(), &childStderr, 0, TRUE,
	    DUPLICATE_SAME_ACCESS) == 0) {
		CloseHandle(childStdout[0]);
		CloseHandle(childStdout[1]);
		CloseHandle(childStdin[0]);
		CloseHandle(childStdin[1]);
		goto fail;
	}

	memset(&staInfo, 0, sizeof(staInfo));
	staInfo.cb = sizeof(staInfo);
	staInfo.hStdError = childStderr;
	staInfo.hStdOutput = childStdout[1];
	staInfo.hStdInput = childStdin[0];
	staInfo.wShowWindow = SW_HIDE;
	staInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	strncpy(cmd, path, sizeof(cmd)-1);
	cmd[sizeof(cmd)-1] = '\0';
	if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL,
	    &staInfo, &childInfo) == 0) {
		CloseHandle(childStdout[0]);
		CloseHandle(childStdout[1]);
		CloseHandle(childStdin[0]);
		CloseHandle(childStdin[1]);
		CloseHandle(childStderr);
		goto fail;
	}
	WaitForInputIdle(childInfo.hProcess, INFINITE);
	CloseHandle(childInfo.hProcess);
	CloseHandle(childInfo.hThread);

	*child_stdout = _open_osfhandle((intptr_t)childStdout[0], _O_RDONLY);
	*child_stdin = _open_osfhandle((intptr_t)childStdin[1], _O_WRONLY);
	
	CloseHandle(childStdout[1]);
	CloseHandle(childStdin[0]);

	return (childInfo.dwProcessId);

fail:
	return (-1);
}

void
__archive_check_child(int in, int out)
{
	(void)in; /* UNUSED */
	(void)out; /* UNUSED */
	Sleep(100);
}

#endif /* _WIN32 && !__CYGWIN__ */
