/*
 * Copyright 1997 Sean Eric Fagan
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
 *	This product includes software developed by Sean Eric Fagan
 * 4. Neither the name of the author may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

extern int setup_and_wait(char **);
extern int start_tracing(pid_t);
extern void restore_proc(int);
extern void waitevent(struct trussinfo *);
extern const char *ioctlname(unsigned long val);
extern char *strsig(int sig);
#ifdef __arm__
extern void arm_syscall_entry(struct trussinfo *, int);
extern long arm_syscall_exit(struct trussinfo *, int);
#endif
#ifdef __amd64__
extern void amd64_syscall_entry(struct trussinfo *, int);
extern long amd64_syscall_exit(struct trussinfo *, int);
extern void amd64_linux32_syscall_entry(struct trussinfo *, int);
extern long amd64_linux32_syscall_exit(struct trussinfo *, int);
extern void amd64_fbsd32_syscall_entry(struct trussinfo *, int);
extern long amd64_fbsd32_syscall_exit(struct trussinfo *, int);
#endif
#ifdef __i386__
extern void i386_syscall_entry(struct trussinfo *, int);
extern long i386_syscall_exit(struct trussinfo *, int);
extern void i386_linux_syscall_entry(struct trussinfo *, int);
extern long i386_linux_syscall_exit(struct trussinfo *, int);
#endif
#ifdef __ia64__
extern void ia64_syscall_entry(struct trussinfo *, int);
extern long ia64_syscall_exit(struct trussinfo *, int);
#endif
#ifdef __powerpc__
extern void powerpc_syscall_entry(struct trussinfo *, int);
extern long powerpc_syscall_exit(struct trussinfo *, int);
extern void powerpc64_syscall_entry(struct trussinfo *, int);
extern long powerpc64_syscall_exit(struct trussinfo *, int);
#endif
#ifdef __sparc64__
extern void sparc64_syscall_entry(struct trussinfo *, int);
extern long sparc64_syscall_exit(struct trussinfo *, int);
#endif
#ifdef __mips__
extern void mips_syscall_entry(struct trussinfo *, int);
extern long mips_syscall_exit(struct trussinfo *, int);
#endif

