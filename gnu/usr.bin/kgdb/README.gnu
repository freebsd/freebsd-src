This is GDB, the GNU source-level debugger, presently running under un*x.

Before compiling GDB, you must tell GDB what kind of machine you are
running on.  To do this, type `config.gdb machine', where machine is
something like `vax' or `sun2'.  For a list of valid machine types,
type `config.gdb'.

Normally config.gdb edits the makefile as necessary.  If you have to
edit the makefile on a standard machine listed in config.gdb this
should be considered a bug and reported as such.

Once these files are set up, just `make' will do everything,
producing an executable `gdb' in this directory.

If you want a new (current to this release) version of the manual, you
will have to use the gdb.texinfo file provided with this distribution.
The gdb.texinfo file requires the texinfo-format-buffer command from
emacs 18.55 or later.

About languages other than C...

C++ support has been integrated into gdb.  GDB should work with
FORTRAN programs (if you have problem, please send a bug report), but
I am not aware of anyone who is working on getting it to use the
syntax of any language other than C or C++.  Pascal programs which use
sets, subranges, file variables, or nested functions will not
currently work.

About -gg format...

Currently GDB version 3.x does *not* support GCC's -gg format.  This
is because it (in theory) has fast enough startup on dbx debugging
format object files that -gg format is unnecessary (and hence
undesirable, since it wastes space and processing power in gcc).  I
would like to hear people's opinions on the amount of time currently
spent in startup; is it fast enough?

About remote debugging...

The two files remote-multi.shar and remote-sa.m68k.shar contain two
examples of a remote stub to be used with remote.c.  The the -multi
file is a general stub that can probably be running on various
different flavors of unix to allow debugging over a serial line from
one machine to another.  The remote-sa.m68k.shar is designed to run
standalone on a 68k type cpu and communicate properley with the
remote.c stub over a serial line.

About reporting bugs...

The correct address for reporting bugs found with gdb is
"bug-gdb@prep.ai.mit.edu".  Please send all bugs to that address.

About xgdb...

xgdb.c was provided to us by the user community; it is not an integral
part of the gdb distribution.  The problem of providing visual
debugging support on top of gdb is peripheral to the GNU project and
(at least right now) we can't afford to put time into it.  So while we
will be happy to incorporate user fixes to xgdb.c, we do not guarantee
that it will work and we will not fix bugs reported in it.  Someone is
working on writing a new XGDB, so improving (e.g. by fixing it so that
it will work, if it doesn't currently) the current one is not worth it.

For those intersted in auto display of source and the availability of
an editor while debugging I suggest trying gdb-mode in gnu-emacs.
Comments on this mode are welcome.

About the machine-dependent files...

m-<machine>.h (param.h is a link to this file).
This file contains macro definitions that express information
about the machine's registers, stack frame format and instructions.

<machine>-opcode.h (opcode.h is a link to this file).
<machine>-pinsn.c (pinsn.c is a link to this file).
These files contain the information necessary to print instructions
for your cpu type.

<machine>-dep.c (dep.c is a link to this file).
Those routines which provide a low level interface to ptrace and which
tend to be machine-dependent.  (The machine-independent routines are in
`infrun.c' and `inflow.c')

About writing code for GDB...

We appreciate having users contribute code that is of general use, but
for it to be included in future GDB releases it must be cleanly
written.  We do not want to include changes that will needlessly make future
maintainance difficult.  It is not much harder to do things right, and
in the long term it is worth it to the GNU project, and probably to
you individually as well.

Please code according to the GNU coding standards.  If you do not have
a copy, you can request one by sending mail to gnu@prep.ai.mit.edu.

Please try to avoid making machine-specific changes to
machine-independent files (i.e. all files except "param.h" and
"dep.c".  "pinsn.c" and "opcode.h" are processor-specific but not
operating system-dependent).  If this is unavoidable, put a hook in
the machine-independent file which calls a (possibly)
machine-dependent macro (for example, the IGNORE_SYMBOL macro can be
used for any symbols which need to be ignored on a specific machine.
Calling IGNORE_SYMBOL in dbxread.c is a lot cleaner than a maze of #if
defined's).  The machine-independent code should do whatever "most"
machines want if the macro is not defined in param.h.  Using #if
defined can sometimes be OK (e.g.  SET_STACK_LIMIT_HUGE) but should be
conditionalized on a specific feature of an operating system (set in
param.h) rather than something like #if defined(vax) or #if
defined(SYSV).

It is better to replace entire routines which may be system-specific,
rather than put in a whole bunch of hooks which are probably not going
to be helpful for any purpose other than your changes.  For example,
if you want to modify dbxread.c to deal with DBX debugging symbols
which are in COFF files rather than BSD a.out files, do something
along the lines of a macro GET_NEXT_SYMBOL, which could have
different definitions for COFF and a.out, rather than trying to put
the necessary changes throughout all the code in dbxread.c that
currently assumes BSD format.

Please avoid duplicating code.  For example, if something needs to be
changed in read_inferior_memory, it is very painful because there is a
copy in every dep.c file.  The correct way to do this is to put (in
this case) the standard ptrace interfaces in a separate file ptrace.c,
which is used by all systems which have ptrace.  ptrace.c would deal
with variations between systems the same way any system-independent
file would (hooks, #if defined, etc.).

About debugging gdb with itself...

You probably want to do a "make TAGS" after you configure your
distribution; this will put the machine dependent routines for your
local machine where they will be accessed first by a M-period .

Also, make sure that you've compiled gdb with your local cc or taken
appropriate precautions regarding ansification of include files.  See
the Makefile for more information.

The "info" command, when executed without a subcommand in a gdb being
debugged by gdb, will pop you back up to the top level gdb.  See
.gdbinit for more details.

