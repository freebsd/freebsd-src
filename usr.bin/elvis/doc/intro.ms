.Go 1 "INTRODUCTION"
.PP
\*E is a clone of vi/ex, the standard UNIX editor.
\*E supports nearly all of the vi/ex commands,
in both visual mode and colon mode.
.PP
Like vi/ex, \*E stores most of the text in a temporary file, instead of RAM.
This allows it to edit files that are too large to fit
in a single process' data space.
Also, the edit buffer can survive a power failure or crash.
.PP
\*E runs under BSD UNIX, AT&T SysV UNIX, Minix, MS-DOS, Atari TOS,
Coherent, OS9/68000, VMS and AmigaDos.
The next version is also expected to add MS-Windows, OS/2 and MacOS.
Contact me before you start porting it to some other OS,
because somebody else may have already done it for you.
.PP
\*E is freely redistributable, in either source form or executable form.
There are no restrictions on how you may use it.
.NH 2
Compiling
.PP
See the "Versions" section of this manual for instructions on how to compile
\*E.
.PP
If you want to port \*E to another O.S. or compiler, then
you should start be reading the "Portability" part of the "Internal" section.
.NH 2
Overview of \*E
.PP
The user interface of \*E/vi/ex is weird.
There are two major command modes in \*E, and a few text input modes as well.
Each command mode has a command which allows you to switch to the other mode.
.PP
You will probably use the \fIvisual command mode\fR
most of the time.
This is the mode that \*E normally starts up in.
.PP
In visual command mode, the entire screen is filled with lines of text
from your file.
Each keystroke is interpretted as part of a visual command.
If you start typing text, it will \fInot\fR be inserted,
it will be treated as part of a command.
To insert text, you must first give an "insert text" command.
This will take some getting used to.
(An alternative exists.
Lookup the "inputmode" option.)
.PP
The \fIcolon mode\fR is quite different.
\*E displays a ":" character on the bottom line of the screen, as a prompt.
You are then expected to type in a command line and hit the <Return> key.
The set of commands recognized in the colon mode is different
from visual mode's.
