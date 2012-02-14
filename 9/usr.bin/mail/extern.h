/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.2 (Berkeley) 4/20/95 
 *
 * $FreeBSD$
 */

struct name *cat(struct name *, struct name *);
struct name *delname(struct name *, char []);
struct name *elide(struct name *);
struct name *extract(char [], int);
struct name *gexpand(struct name *, struct grouphead *, int, int);
struct name *nalloc(char [], int);
struct name *outof(struct name *, FILE *, struct header *);
struct name *put(struct name *, struct name *);
struct name *tailof(struct name *);
struct name *usermap(struct name *);
FILE	*Fdopen(int, const char *);
FILE	*Fopen(const char *, const char *);
FILE	*Popen(char *, const char *);
FILE	*collect(struct header *, int);
char	*copyin(char *, char **);
char	*detract(struct name *, int);
char	*expand(char *);
char	*getdeadletter(void);
char	*getname(int);
char	*hfield(const char *, struct message *);
FILE	*infix(struct header *, FILE *);
char	*ishfield(char [], char *, const char *);
char	*name1(struct message *, int);
char	*nameof(struct message *, int);
char	*nextword(char *, char *);
char	*readtty(const char *, char []);
char 	*reedit(char *);
FILE	*run_editor(FILE *, off_t, int, int);
char	*salloc(int);
char	*savestr(char *);
FILE	*setinput(struct message *);
char	*skin(char *);
char	*skip_comment(char *);
char	*snarf(char [], int *);
char	*username(void);
char	*value(const char *);
char	*vcopy(const char *);
char	*yankword(char *, char []);
char	*yanklogin(char *, char []);
int	 Fclose(FILE *);
int	 More(int *);
int	 Pclose(FILE *);
int	 Respond(int *);
int	 Type(int *);
int	 doRespond(int []);
int	 dorespond(int *);
void	 alter(char *);
int	 alternates(char **);
void	 announce(void);
int	 append(struct message *, FILE *);
int	 argcount(char **);
void	 assign(const char *, const char *);
int	 bangexp(char *, size_t);
void	 brokpipe(int);
int	 charcount(char *, int);
int	 check(int, int);
void	 clob1(int);
int	 clobber(char **);
void	 close_all_files(void);
int	 cmatch(char *, char *);
void	 collhup(int);
void	 collint(int);
void	 collstop(int);
void	 commands(void);
int	 copycmd(char []);
int	 core(void);
int	 count(struct name *);
int	 delete(int []);
int	 delm(int []);
int	 deltype(int []);
void	 demail(void);
int	 diction(const void *, const void *);
int	 dosh(char *);
int	 echo(char **);
int	 edit1(int *, int);
int	 editor(int *);
void	 edstop(void);
int	 elsecmd(void);
int	 endifcmd(void);
int	 evalcol(int);
int	 execute(char [], int);
int	 exwrite(char [], FILE *, int);
void	 fail(const char *, const char *);
int	 file(char **);
struct grouphead *
	 findgroup(char []);
void	 findmail(char *, char *, int);
int	 first(int, int);
void	 fixhead(struct header *, struct name *);
void	 fmt(const char *, struct name *, FILE *, int);
int	 folders(void);
int	 forward(char [], FILE *, char *, int);
void	 free_child(int);
int	 from(int *);
off_t	 fsize(FILE *);
int	 getfold(char *, int);
int	 gethfield(FILE *, char [], int, char **);
int	 getmsglist(char *, int *, int);
int	 getrawlist(char [], char **, int);
int	 getuserid(char []);
int	 grabh(struct header *, int);
int	 group(char **);
void	 hangup(int);
int	 hash(const char *);
void	 hdrstop(int);
int	 headers(int *);
int	 help(void);
void	 holdsigs(void);
int	 ifcmd(char **);
int	 igcomp(const void *, const void *);
int	 igfield(char *[]);
int	 ignore1(char *[], struct ignoretab *, const char *);
int	 igshow(struct ignoretab *, const char *);
int	 inc(void *);
int	 incfile(void);
void	 intr(int);
int	 isdate(char []);
int	 isdir(char []);
int	 isfileaddr(char *);
int	 ishead(char []);
int	 isign(const char *, struct ignoretab []);
int	 isprefix(const char *, const char *);
void	 istrncpy(char *, const char *, size_t);
__const struct cmd *
	 lex(char []);
void	 load(char *);
struct var *
	 lookup(const char *);
int	 mail(struct name *,
	    struct name *, struct name *, struct name *, char *, char *);
void	 mail1(struct header *, int);
void	 makemessage(FILE *, int);
void	 mark(int);
int	 markall(char [], int);
int	 matchsender(char *, int);
int	 matchfield(char *, int);
int	 mboxit(int []);
int	 member(char *, struct ignoretab *);
void	 mesedit(FILE *, int);
void	 mespipe(FILE *, char []);
int	 messize(int *);
int	 metamess(int, int);
int	 more(int *);
int	 newfileinfo(int);
int	 next(int *);
int	 null(int);
void	 parse(char [], struct headline *, char []);
int	 pcmdlist(void);
int	 pdot(void);
void	 prepare_child(sigset_t *, int, int);
int	 preserve(int *);
void	 prettyprint(struct name *);
void	 printgroup(char []);
void	 printhead(int);
int	 puthead(struct header *, FILE *, int);
int	 putline(FILE *, char *, int);
int	 pversion(int);
void	 quit(void);
int	 quitcmd(void);
int	 readline(FILE *, char *, int);
void	 register_file(FILE *, int, int);
void	 regret(int);
void	 relsesigs(void);
int	 respond(int *);
int	 retfield(char *[]);
int	 rexit(int);
int	 rm(char *);
int	 run_command(char *, sigset_t *, int, int, char *, char *, char *);
int	 save(char []);
int	 save1(char [], int, const char *, struct ignoretab *);
void	 savedeadletter(FILE *);
int	 saveigfield(char *[]);
int	 savemail(char [], FILE *);
int	 saveretfield(char *[]);
int	 scan(char **);
void	 scaninit(void);
int	 schdir(char **);
int	 screensize(void);
int	 scroll(char []);
int	 sendmessage(struct message *, FILE *, struct ignoretab *, char *);
int	 sendmail(char *);
int	 set(char **);
int	 setfile(char *);
void	 setmsize(int);
void	 setptr(FILE *, off_t);
void	 setscreensize(void);
int	 shell(char *);
void	 sigchild(int);
void	 sort(char **);
int	 source(char **);
void	 spreserve(void);
void	 sreset(void);
int	 start_command(char *, sigset_t *, int, int, char *, char *, char *);
void	 statusput(struct message *, FILE *, char *);
void	 stop(int);
int	 stouch(int []);
int	 swrite(char []);
void	 tinit(void);
int	 top(int *);
void	 touch(struct message *);
void	 ttyint(int);
void	 ttystop(int);
int	 type(int *);
int	 type1(int *, int, int);
int	 undelete_messages(int *);
void	 unmark(int);
char	**unpack(struct name *);
int	 unread(int []);
void	 unregister_file(FILE *);
int	 unset(char **);
int	 unstack(void);
void	 vfree(char *);
int	 visual(int *);
int	 wait_child(int);
int	 wait_command(int);
int	 writeback(FILE *);

extern char *__progname;
extern char *tmpdir;
