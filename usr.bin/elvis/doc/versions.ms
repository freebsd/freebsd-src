.Go 13 "VERSIONS"
.PP
\*E currently works under BSD UNIX, AT&T System-V UNIX, SCO XENIX,
Minix, Coherent, MS-DOS, Atari TOS, OS9/68k, VAX/VMS, and AmigaDos.
This section of the manual provides special information that applies to each
particular version of \*E.
.PP
For all versions except MS-DOS and VMS,
the file "Makefile.mix" should be copied to "Makefile",
and then edited to select the correct set of options for your system.
There is more information about this embedded in the file itself.
.NH 2
BSD UNIX
.PP
Temporary files are stored in /tmp.
.PP
You should modify /etc/rc so that
the temp files are preserved when the system is rebooted.
Find a line in /etc/rc which reads
.sp
.ti +0.5i
ex4.3preserve /tmp
.LP
or something like that, and append the following line:
.sp
.ti +0.5i
elvprsv /tmp/elv*
.PP
If you do not have permission to modify /etc/rc, don't fret.
The above modification is only needed to allow you to recover your changes
after a system crash.
You can still run \*E without that modification,
and you can still recover your changes when \*E crashes
or when your dialup modem looses the carrier signal, or something like that.
Only a system crash or power failure could hurt you.
.PP
Both \*E and the real Vi
read initialization commands from a file called ".exrc",
but the commands in that file might work on one editor but not the other.
For example, "set keywordprg=man" will work for \*E,
but Vi will complain because it doesn't have a "keywordprg" option.
If the warning messages annoy you, then you can edit the CFLAGS setting
in the Makefile and add -DEXRC=\\".elvisrc\\".
.PP
If you use X windows, you may wish to add "-DCS_LATIN1" to CFLAGS.
This will cause the digraph table and the flipcase option to have default
values that are appropriate for the LATIN-1 character set.
That's the standard character set for X.
.PP
The default mailer used notify users when text is preserver is "mail".
You may wish to change this to "Mail" (with an uppercase 'M').
See the description of "MAILER" in the CFLAGS section of this manual.
.PP
The default keyboard macro time-out value is larger for BSD than it is for
some other systems, because I've had trouble running \*E via rlogin or Xterm.
I guess it takes a while for those keystokes to squirt through the net.
.NH 2
System-V UNIX
.PP
Most SysV UNIX systems use terminfo instead of termcap,
but  the  terminfo  library  doesn't seem to have a standard name.
As shipped, \*E' Makefile.mix  is  configured  with "LIBS=-ltermcap".
You may need to change it to "LIBS=-lterm" or "LIBS=-lterminfo"
or even "LIBS=-lcurses".
.PP
The /etc/rc file (or its equivelent) should be modified as described
for BSD systems, above.
There's a pretty good chance that "make install" will do this for you;
it knows how to create an editor recovery file in the /etc/rc2.d directory,
which is where most modern SysV systems store initialization commands.
You only need to do it manually for older SysV systems.
.PP
The potential trouble with ".exrc" described above for BSD UNIX applies
to System-V UNIX as well.
.PP
The default mailer used notify users when text is preserver is "mail".
You may wish to change this to "mailx".
See the description of "MAILER" in the CFLAGS section of this manual.
.PP
\*E uses control-C as the interrupt key, not Delete.
This was done so that the <Del> key could be used for character deletion.
.NH 2
SCO Xenix
.PP
For Xenix-386, you can use the generic System-V settings.
You may wish to add "-DCS_IBMPC" to CFLAGS, to have the digraph table and
flipcase option start up in a mode that is appropriate for the console.
Also, note that there is a separate group of settings for use with Xenix-286.
It already has "-DCS_IBMPC" in CFLAGS.
.PP
Because Xenix is so similar to System-V, everything I said earlier about
System-V applies to the Xenix version too, except that editor recovery
might belong in a directory called /etc/rc.d/8 instead.
.NH 2
Minix
.PP
There are separate settings in Makefile.mix for Minix-PC and Minix-68k.
The differences between these two are that
the 68k version uses ".o" for the object file extension where
the PC version uses ".s", and
the PC version has some extra flags in CFLAGS to reduce the size of \*E.
The PC version also uses tinytcap (instead of the full termcap)
to make it smaller.
.PP
Minix-PC users should read the CFLAGS section of this manual very carefully.
You have some choices to make...
.PP
The temporary files are stored in /usr/tmp.
The /usr/tmp directory must exist before you run \*E,
and it must be readable & writable by everybody.
We use /usr/tmp instead of /tmp because
after a system crash or power failure,
you can recover the altered version of a file from the temporary file
in /usr/tmp.
If it was stored in /tmp, though, then it would be lost because /tmp is
normally located on the RAM disk.
Also, you'll need a /usr/preserve directory which is readable & writable
by root;
this directory is used to store text files that have been preserved after a
crash.
The "make install" script will create it if necessary.
.PP
\*E uses control-C as the interrupt key, not Delete.
.NH 2
Coherent
.PP
\*E was ported to Coherent by Esa Ahola.
.PP
\*E is too large to run under Coherent unless you eliminate some
features via the CFLAGS setting.
The recommended settings, in Makefile.mix, produce a working version
of \*E which emulates Vi faithfully, but lacks most of the extensions.
You should read the CFLAGS section of this manual carefully.
.PP
You can probably reduce the size of \*E by using tinytcap.c instead of -lterm.
This would allow you to keep most features of \*E,
at the expense of terminal independence.
(Tinytcap.c has ANSI escape sequences hard-coded into it.)
To use tinytcap, just add "tinytcap.o" to the "EXTRA=" line in the Makefile,
and remove "-lterm" from the "LIBS=" line.
.PP
The temporary files are stored in /tmp.
Preserved files are stored in /usr/preserve.
You should modify your /etc/rc file to support file file preservation;
add the line...
.sp
	/usr/bin/elvprsv /tmp/*
.sp
... just before the first "/bin/rm" line.
.NH 2
MS-DOS
.PP
\*E was ported to MS-DOS by Guntram Blohm and Martin Patzel.
Willett Kempton added support for the DEC Rainbow.
.PP
Ideally, \*E should be compiled with Microsoft C 5.10 and the standard
Microsoft Make utility,
via the command "make elvis.mak".
This will compile \*E and all related utilities.
.PP
With Microsoft C 6.00, you may have trouble compiling regexp.c.
If so, try compiling it without optimization.
.PP
The "Makefile.mix" file contains a set of suggested settings for compiling
\*E with Turbo-C or Borland C.
(If you have Turbo-C, but not the Make utility,
then you can \fIalmost\fR use the "\*E.prj" file to compile \*E,
but you must explicitly force Turbo-C to compile it with the "medium" memory model.
Most of the related programs [ctags, ref, virec, refont, and wildcard] are
only one file long, so you should have no trouble compiling them.)
The "alias.c" file is meant to be compiled once into an executable named
"ex.exe".
You should then copy "ex.exe" to "vi.exe" and "view.exe".
.PP
\*E stores its temporary files in C:\\tmp.
If this is not satisfactory, then you should edit the CFLAGS line of
your Makefile to change TMPDIR to something else before compiling.
You can also control the name of the temp directory via an environment
variable named TMP or TEMP.
The directory must exist before you can run \*E.
.PP
The TERM environment variable determines how \*E will write to the screen.
It can be set to any one of the following values:
.LD
.ta 1.5i 2.5i
	pcbios	Use BIOS calls on an IBM-PC clone.
	rainbow	Use DEC Rainbow interface.
	ansi	Use ANSI.SYS driver.
	nansi	User faster NANSI.SYS driver.
.DE
.PP
If the TERM variable isn't set, then \*E will automatically select either
the "rainbow" interface (when run on a Rainbow) or "pcbios" (on an IBM clone).
.PP
You may prefer to use NANSI.SYS for speed;
or you may NEED to use ANSI.SYS for a non-clone, such as a lap-top.
If so, you should
install one of these drivers by adding "driver = nansi.sys" (or whatever)
to your CONFIG.SYS file,
and then you should define TERM to be "nansi" (or whatever) by adding
"set TERM=nansi" to your AUTOEXEC.BAT file.
You must then reboot for these changes to take effect.
After that, \*E will notice the "TERM" setting and use the driver.
.PP
Since ".exrc" is not a valid DOS filename,
the name of the initialization file has been changed to "elvis.rc".
\*E will look for an "elvis.rc" file first in your home directory.
If it exists, and contains ":set exrc", then \*E will check for
another "elvis.rc" in the current directory.
By default, the directory where ELVIS.EXE resides is taken to be your
home directory.
You can override this default by setting an environment variable named
"HOME" to the full pathname of your home directory.
To set "HOME", you would typically add the following line to your
AUTOEXEC.BAT file:
.br
.ti +0.5i
set HOME c:\\
.PP
An extra program, called "wildcard", is needed for MS-DOS.
It expands wildcard characters in file names.
If \*E flashes a "Bad command or filename" message when it starts,
then you've probably lost the WILDCARD.EXE program somehow.
.PP
\*E can run under Windows, but you may have trouble with TEMP.
Windows uses an environment variable called TEMP which interferes with
\*E' usage of TEMP;
to work around this, you can simply set an environment variable named
TMP (with no 'E') to the name of \*E' temporary directory.
When TEMP and TMP are both set, \*E uses TMP and ignored TEMP.
.NH 2
Atari TOS
.PP
\*E was ported to Atari TOS by Guntram Blohm and Martin Patzel.
It is very similar to the MS-DOS version.
It has been tested with the Mark Williams C compiler and also GNU-C.
.PP
The TERM environment variable is ignored;
the ST port always assumes that TERM=vt52.
The SHELL (not COMSPEC!) variable should be set to
the name of a line-oriented shell.
.PP
A simple shell in included with \*E.
Its source is in "shell.c", and the name of the executable is "shell.ttp".
The file "profile.sh" should contain a set of instructions to be executed
when the shell first starts up.
An example of this file is included, but you will almost certainly want to
edit it right away to match your configuration.
(If you already have a command-line shell,
then you'll probably want to continue using it.
The shell that comes with \*E is very limited.)
.PP
Currently, character attributes cannot be displayed on the screen.
.PP
\*E runs under MiNT (a free multi-tasking extension to TOS)
but it can be a CPU hog because of the way that \*E reads from the
keyboard with timeout.
Also, \*E doesn't use any of the special features of MiNT.
I have received a set of patches that optimize \*E for MiNT,
but they arrived too late to integrate into this release.
.NH 2
OS9/68k
.PP
\*E was ported to OS9/68k by Peter Reinig.
.PP
The Makefile is currently configured to install \*E and the related
programs in /dd/usr/cmds
If this this is unacceptable, then you should change the BIN setting
to some other directory.
Similarly, it expects the source code to reside in /dd/usr/src/elvis;
the ODIR setting is used to control this.
.PP
Temporary files are stored in the /dd/tmp directory.
Your /dd/startup file may need to be modified
to prevent it from deleting \*E' temporary files;
make /dd/startup run the \fIelvprsv\fR program before it wipes out /dd/tmp.
.PP
The program in alias.c is linked repeatedly to produce the
"vi", "view", and "input" aliases for \*E.
Sadly, the "ex" alias is impossible to implement under OS9
because the shell has a built-in command by that name.
.PP
For some purposes,
you must give `make' the "-b" option.
Specifically, you need this for "make -b clean" and "make -b install".
.NH 2
VAX/VMS
.PP
John Campbell ported \*E to VAX/VMS.
.PP
A heavily laden VAX can take half an hour to compile \*E.
This is normal.
Don't panic.
.PP
While running, \*E will create temporary files in SYS$SCRATCH.
Enter SHOW LOGICAL SYS$SCRATCH to see what actual directory you are using.
Many sites have SYS$SCRATCH equivalenced to SYS$LOGIN.
The \*E temporary files look like the following on VMS while \*E is running:
.br
.ti 0.75i
ELV_1123A.1;1       ELV_1123A.2;1       SO070202.;1
.PP
Also, filtering commands (like !!dir and !}fmt) should work on VMS.
This assumes, however, that you can create temporary mailboxes and that
your mailbox quota (a sysgen parameter) is at least 256 bytes for a
single write to the mailbox.
This is the default sysgen parameter,
so there should be few people who experience filter problems.
.PP
Additionally, an attempt was made to support the standard terminals on VMS:
"vt52", "vt100", "vt200", "vt300", "vt101", "vt102".
Non-standard terminals could be supported by setting your terminal type to
UNKNOWN (by entering SET TERM/UNKNOWN)
and defining the logical name ELVIS_TERM.
Whatever ELVIS_TERM translates to, however, will have to be included in
tinytcap.c.
Note that the upper/lowercase distinctions are significant,
and that DCL will upshift characters that are not quoted strings, so
enter DEFINE ELVIS_TERM "hp2621a".
As distributed, it would probably not be a good idea to have more than the
standard terminals in tinytcap.c (else it wouldn't be tiny, would it?).
Changes here, of course, would require a recompilation to take effect.
.PP
If you have a version of the "termcap" library and database on your system,
then you may wish to replace tinytcap with the real termcap.
.NH 2
AmigaDOS
.PP
Mike Rieser and Dale Rahn ported \*E to AmigaDOS.
.PP
The port was done using Manx Aztec C version 5.2b.
\*E uses about as much space as it can and still be small code and data.
\*E should also compile under DICE, though there may be a little trouble with
signed versus unsigned chars.
.PP
The port has been done so the same binary will run under both versions of AmigaDOS.
Under AmigaDOS 2.04, \*E supports all the documented features.
It also uses an external program ref to do tag lookup.
So, the accompanying programs: ref and ctags are recommended.
Under AmigaDOS 1.2/1.3 \*E works, buts lacks the more advanced features.
.PP
For the port to AmigaDOS 2.04, we tried to use as many Native AmigaDOS
calls as we could.
This should increase \*E's chances at being compiled with other compilers.
DICE seems to have a different default char type.
You may need to use the UCHAR() macro in tio.c.
To test it, try the :map command; if it looks right, things are cool.
.PP
For the port to AmigaDOS 1.3, we tried to make sure the program was at
least usable.
Many features are missing, most notably running commands in subshells.
Also, what we could get working, we used Aztec functions to support them,
so this part is little more compiler dependent.
.PP
Aztec is compatible with the SAS libcall #pragma.
I personally prefer using the includes that come from Commodore over the ones
supplied with Aztec, but for people with a straight Aztec installation,
I went with the default names for the Aztec pragmas.
.PP
One include you'll need is <sys/types.h>.
It's a common include when porting software just make yourself one.
It's a two line file that saves a lot of hassle especially in the \*E source.
So, make a directory where your includes are located called `sys'
and in a file below that type:
.br
.ti +0.8i
/* sys/types.h */
.br
.ti +0.8i
#include <exec/types.h>
.PP
When setting environment variables (either local or global) for
variables that specify a directory, make sure the variable ends in `:'
or `/'.
This saved from having to change much of the way \*E works.
The default temporary directory (if TEMP and TMP aren't specified) is "T:".
The default if HOME directory (if no HOME environment variable is set) is "S:".
.PP
To avoid conlict with other uses, \*E uses elvis.rc instead of .exrc or
where it looks for macros.
.NH 2
Other Systems
.PP
For SunOS and Solaris 1.x, use the BSD settings;
for Solaris 2.x, use the SysV settings.
Earlier versions of \*E didn't link correctly due to a quirk in Sun's
version of the "make" utility, but this version of \*E has a work-around
for that quirk so you should have no trouble at all.
.PP
For Linux, use the SysV settings.
You can probably just remove the "-lterm" from the "LIBS= -lterm" line,
since linux keeps the termcap functions in the standard C library.
.PP
For other UNIXoid systems, I suggest you start with the Minix-68k settings
and then grow from that.
Minix is a nice starting point because it is a clone of Version 7 UNIX,
which was the last common ancestor of BSD UNIX and SysV UNIX.
Any Operating System which claims any UNIX compatibility what so ever
will therefore support V7/Minix code.
You may need to fiddle with #include directives or something, though.
Minix-68k is a better starting point than Minix-PC because the PC compiler
has some severe quirks.
.PP
If you're thinking of porting \*E to some non-UNIX system,
I suggest you begin by studying the "INTERNALS" section of this manual.
