#include "out2con.h"

#include <windows.h>
#include <stdio.h>
#include <io.h>

class ConsoleEcho
{
public:
    ConsoleEcho();
    ~ConsoleEcho();

private:
    DWORD ThreadLoop();

    static DWORD WINAPI ThreadFunc(void* param);

    FILE m_originalStdout;
    int m_stdoutfd;
    int m_pipefd;
    HANDLE m_hReadPipe, m_hWritePipe;
    HANDLE m_hThread;

    static const int BUFSIZE=512;
};


ConsoleEcho *
CreateConsoleEcho()
{
    return new ConsoleEcho;
}

void
DestroyConsoleEcho(ConsoleEcho *echo)
{
    delete echo;
}


DWORD WINAPI ConsoleEcho::ThreadFunc(void* param)
{
    return ((ConsoleEcho*)(param))->ThreadLoop();
}


DWORD ConsoleEcho::ThreadLoop()
{
    DWORD dwRead, dwWritten;
    CHAR chBuf[BUFSIZE];
    BOOL bSuccess = FALSE;
    // Note that the following does not work when running in the msvc2010
    // debugger with redirected output; you still get the redirected file
    // handle, not the console:
    //HANDLE hConsoleStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    // This seems to be more reliable:
    HANDLE hConsoleStdOut = CreateFile("CONOUT$",
                                       GENERIC_WRITE,
                                       FILE_SHARE_WRITE,
                                       NULL, OPEN_EXISTING, 0, 0);
    for (;;) {
        // read from redirected stdout
        bSuccess = ReadFile(m_hReadPipe, chBuf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess || (dwRead == 0))
            break;

        // write to console
        WriteFile(hConsoleStdOut, chBuf, dwRead, &dwWritten, NULL);
        // also write to original stdout
        if (m_stdoutfd>=0) {
            _write(m_stdoutfd, chBuf, dwRead);
            // _commit() causes assert if m_stdoutfd is a device (e.g., console or NUL).
            if (!_isatty(m_stdoutfd))
                _commit(m_stdoutfd);
        }
    }
    CloseHandle(hConsoleStdOut);
    return 0;
}

ConsoleEcho::ConsoleEcho()
{
    // setup console
    AllocConsole();
    // create pipe
    CreatePipe(&m_hReadPipe, &m_hWritePipe, NULL, 0);
    // save original stdout to preserve commandline-specified redirection
    m_stdoutfd = _fileno(stdout);
    // and copy the whole damn FILE structure so we can restore it
    // when we're done.  I don't know any other way to restore the
    // crazy windows gui default '-2' filedesc stdout.
    m_originalStdout = *stdout;
    // hook up the write end of our pipe to stdout
    m_pipefd = _open_osfhandle((intptr_t)m_hWritePipe, 0);
    // take our os file handle and allocate a crt FILE for it
    FILE* fp = _fdopen(m_pipefd, "w");
    // copy to stdout
    *stdout = *fp;
    // now slam the allocated FILE's _flag to zero to mark it as free without
    // actually closing the os file handle and pipe
    fp->_flag = 0;

    // disable buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    // Create a thread to process our pipe, forwarding output
    // to both the console and the original stdout
    m_hThread = CreateThread(NULL, 0, &ThreadFunc, this, 0, NULL);
}

ConsoleEcho::~ConsoleEcho()
{
    // fclose() unfortunately immediately invalidates the read pipe before the
    // pipe thread has a chance to flush it, so don't do that.
    //fclose(stdout);

    // instead, just slam the original stdout
    *stdout = m_originalStdout;
    //printf("Safe to printf now and no longer echoed to console.\n");
    // Close write pipe
    _close(m_pipefd);
    // and wait here for pipe thread to exit
    WaitForSingleObject(m_hThread, 1000);
    // now close read pipe as well
    CloseHandle(m_hReadPipe);
}
