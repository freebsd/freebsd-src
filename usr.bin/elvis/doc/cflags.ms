.Go 10 "CFLAGS"
.PP
\*E uses many preprocessor symbols to control compilation.
Some of these control the sizes of buffers and such.
The "-DNO_XXXX" options remove small sets of related features.
.PP
Most \*E users will probably want to keep all features available.
Minix-PC users, though, will have to sacrifice some sets because otherwise
\*E would be too bulky to compile.
The "asld" phase of the compiler craps out.
.IP "-DM_SYSV, -Dbsd, -DTOS, -DCOHERENT, -Damiga"
These flags tell the compiler that \*E is being compiled for
System-V UNIX, BSD UNIX, Atari TOS, Coherent, or AmigaDos, respectively.
For other systems, the config.h file can generally figure it out automatically.
.IP -DRAINBOW
For MS-DOS systems, this causes support for the DEC Rainbow to be compiled
into \*E.
.IP -DNO_S5WINSIZE
Some versions of SysV UNIX don't support support the "winsize"
style of screen-size testing.
If you have a SysV system and can't compile "curses.c",
then try adding -DNO_S5WINSIZE to the CFLAGS.
.IP -DTERMIOS
POSIX is a SysV-derived specification which uses a terminal control
package called "termios", instead of "termio".
Some other SysV systems may also use termios.
You can make elvis uses termios instead of the more common termio
by adding -DTERMIOS to CFLAGS.
(Note: This hasn't been tested very well.)
.IP -DNBUFS=\fInumber\fP
\*E keeps most of your text in a temporary file;
only a small amount is actually stored in RAM.
This flag allows you to control how much of the file can be in RAM at any time.
The default is 5 blocks, and the minimum is 3 blocks.
(See the -DBLKSIZE flag, below.)
.IP
More RAM allows global changes to happen a little faster.
If you're just making many small changes in one section of a file, though,
extra RAM won't help much.
.IP -DBLKSIZE=\fInumber\fP
This controls the size of blocks that \*E uses internally.
The value of BLKSIZE must be a power of two.
Every time you double BLKSIZE, you quadruple the size of a text file that
\*E can handle, but you also cause the temporary file to grow faster.
For MS-DOS, Coherent, and Minix-PC, the default value is 1024, which allows
you to edit files up to almost 512K bytes long.
For all other systems, the default value is 2048, which allows you to edit
files that are nearly 2 megabytes long.
.IP
The BLKSIZE also determines the maximum line length, and a few other limits.
BLKSIZE should be either 256, 512, 1024, or 2048.
Values other than these can lead to strange behaviour.
.IP -DTMPDIR=\fIstring\fP
This sets the default value of the "directory" option, which specifies where
the temporary files should reside.
The value of TMPDIR must be a string, so be sure your value includes the
quote characters on each end.
.IP "-DEXRC=\fIstr\fP, -DHMEXRC=\fIstr\fP, -DSYSEXRC=\fIstr\fP, -DEXINIT=\fIstr\fP"
This lets you control the names of the initialization files.
Their values must be strings, so be careful about quoting.
.IP
EXRC is the name of the initialization file in the current directory.
Its default value is ".exrc" on UNIX systems -- the same as the real vi.
Since that isn't a legal DOS filename, under DOS the default is "elvis.rc".
For other systems, check the config.h file.
.IP
HMEXRC is the name of the initialization file in your home directory.
By default, it is the same as EXRC.
\*E will automatically prepend the name of your home directory to HMEXRC
at run time, so don't give a full path name.
.IP
SYSEXRC is the name of a system-wide initialization file.
It has no default value;
if you don't define a value for it, then
the code that supports SYSEXRC just isn't compiled.
The value of SYSEXRC should be a full pathname, in quotes.
.IP
EXINIT is the name of an environment variable that can contain initialization
commands.
Normally, its value is "EXINIT".
.IP -DKEYWORDPRG=\fIstring\fP
This flag determines the default value of the "keywordprg" option.
Its value must be a string, so be careful about quoting.
The default value of this flag is "ref", which is a C reference program.
.IP "-DCC_COMMAND=\fIstring\fP -DMAKE_COMMAND=\fIstring\fP -DERRLIST=\fIstring\fP"
These control the names of the C compiler, the "make" utility, and the
error output file, respectively.
They are only used if -DNO_ERRLIST is not given.
.IP
The default value of CC_COMMAND depends on the Operating System and compiler
that you use to compile elvis;
for UNIX, the default is "cc".
The default values of MAKE_COMMAND and ERRLIST are "make" and "errlist",
respectively.
.IP -DMAXRCLEN=\fInumber\fP
This determines how large a :@ macro command can be (measured in bytes).
The default is 1000 bytes.
If you increase this value significantly,
then you may need to allocate extra memory for the stack.
See the "CHMEM" setting in the Makefile.
.IP -DSHELL=\fIstring\fP
This is the default value of the "shell" option, and hence
the default shell used from within \*E.
This only controls the default;
the value you give here may be overridden at run-time by setting
an environment variable named SHELL (or COMSPEC for MS-DOS).
Its value must be a string constant, so be careful about quoting.
.IP -DMAILER=\fIstring\fP
This is the name of the program that \*E uses to send mail to a user whose
text has just been preserved.
(See the manual page for the \fIelvprsv\fR program.)
If your system doesn't use electronic mail, then this option is irrelevent.
For UNIX and OS-9 systems, though, the value should be a quoted string.
The default value is "mail", but
SysV users may prefer to use "mailx", and BSD users may prefer "Mail".
.IP -DTAGS=\fIstring\fP
This sets the name of the "tags" file,
which is used by the :tag command.
Its value must be a string constant, so be careful about quoting.
.IP "-DCS_IBMPC -DCS_LATIN1 -DCS_SPECIAL"
The digraph table and flipcase option will normally start out empty.
However, if you add -DCS_IBMPC or -DCS_LATIN1 to your CFLAGS,
then they will start out filled with values that are appropriate for the
IBM PC character set or the ISO Latin-1 character set, respectively.
.IP
You can also use -DCS_IBMPC and -DCS_SPECIAL together to get digraphs
that produce the PC's graphic characters.
.IP "-DDEBUG -DEBUG2"
-DDEBUG adds the ":debug" and ":validate" commands,
and also adds many internal consistency checks.
It increases the size of the ".text" segment by about 6K.
.IP
-DDEBUG2 causes a line to be appended to a file called "debug.out"
everytime any change is made to the edit buffer.
.IP -DCRUNCH
This flag removes some non-critical code, so that \*E is smaller.
For example, it removes a short-cut from the regexp package, so that
text searches are slower.
Also, screen updates are not as efficient.
A couple of obscure features are disabled by this, too.
.IP -DNO_MKEXRC
This removes the ":mkexrc" command,
so you have to create any .exrc files manually.
The size of the .text segment will be reduced by about 1500 bytes.
.IP -DNO_CHARATTR
Permanently disables the charattr option.
This reduces the size of your ".text" segment by about 850 bytes.
.IP -DNO_RECYCLE
Normally, \*E will recycle space (from the temporary file) which contains
totally obsolete text.
This flag disables this recycling.
Without recycling, the ".text" segment is about 1K smaller
than it would otherwise be,
but the tmp file grows much faster.
If you have a lot of free space on your hard disk,
but \*E is too bulky to run with recycling,
then try it without recycling.
.IP
When using a version of \*E that has been compiled with -DNO_RECYCLE,
you should be careful to avoid making many small changes to a file
because each individual change will cause the tmp file to grow by at least 1k.
Hitting "x" thirty times counts as thirty changes,
but typing "30x" counts as one change.
Also, you should occasionally do a ":w" followed by a ":e" to start with a
fresh tmp file.
.IP
Interestingly, the real vi never recycles space from its temporary file.
.IP -DNO_SENTENCE
Leaves out the "(" and ")" visual mode commands.
Also, the "[[", "]]", "{", and "}" commands will not recognize *roff macros.
The sections and paragraphs options go away.
This saves about 650 bytes in the ".text" segment.
.IP -DNO_CHARSEARCH
Leaves out the visual commands which locate a given character
in the current line:
"f", "t", "F", "T", "," and ";".
This saves about 900 bytes.
.IP -DNO_EXTENSIONS
Leaves out the "K" and "#" visual commands.
Also, the arrow keys will no longer work in input mode.
Regular expressions will no longer recognize the \\{\\} operator.
(Other extensions are either inherent in the design of \*E,
or are controlled by more specific flags,
or are too tiny to be worth removing.)
This saves about 250 bytes.
.IP -DNO_MAGIC
Permanently disables the "magic" option, so that most meta-characters
in a regular expression are *NOT* recognized.
This saves about 3k of space in the ".text" segment, because
the complex regular expression code can be replaced by much simpler code.
.IP -DNO_SHOWMODE
Permanently disables the "showmode" option, saving about 250 bytes.
.IP -DNO_CURSORSHAPE
Normally, \*E tries to adjust the shape of the cursor as a reminder
of which mode you're in.
The -DNO_CURSORSHAPE flag disables this, saving about 150 bytes.
.IP -DNO_DIGRAPH
To allow entry of non-ASCII characters, \*E supports digraphs.
A digraph is a single (non-ASCII) character which is entered as a
combination of two other (ASCII) characters.
If you don't need to input non-ASCII characters,
or if your keyboard supports a better way of entering non-ASCII characters,
then you can disable the digraph code and save about 450 bytes.
.IP -DNO_ERRLIST
\*E adds a ":errlist" command, which is useful to programmers.
If you don't need this feature, you can disable it via the -DNO_ERRLIST flag.
This will reduce the .text segment by about 900 bytes, and the .bss segment
by about 300 bytes.
.IP -DNO_ABBR
The -DNO_ABBR flag disables the ":abbr" command,
and reduces the size of \*E by about 250 bytes.
.IP -DNO_OPTCOLS
When \*E displays the current options settings via the ":set" command,
the options are normally sorted into columns.
The -DNO_OPTCOLS flag causes the options to be sorted across the rows,
which is much simpler for the computer.
The -DNO_OPTCOLS flag will reduce the size of your .text segment by about
500 bytes.
.IP -DNO_MODELINES
This removes all support for modelines.
.IP -DNO_TAG
This disables tag lookup.
It reduces the size of the .text segment by about 750 bytes.
.IP -DNO_TAGSTACK
This disables the tagstack.
The ^T and :pop commands will no longer be available.
.IP "-DNO_ALT_FKEY, -DNO_CTRL_FKEY, -DNO_SHIFT_FKEY, -DNO_FKEY"
These remove explicit support of function keys.
-DNO_ALT_FKEY removes support for the <alternate> versions function keys.
-DNO_CTRL_FKEY removes support for the <control> and <alternate> versions function keys.
-DNO_SHIFT_FKEY removes support for the <shift>, <control>, and <alternate> versions function keys.
-DNO_FKEY removes all support of function keys.
.IP
\*E's ":map" command normally allows you to use the special sequence "#<n>"
to map function key <n>.
For example, ":map #1 {!}fmt^M" will cause the <F1> key to reformat a paragraph.
\*E checks the :k1=: field in the termcap description of your terminal
to figure out what code is sent by the <F1> key.
This is handy because it allows you to create a .exrc file which maps function
keys the same way regardless of what type of terminal you use.
.IP
That behaviour is standard; most implementations of the real vi supports it too.
\*E extends this to allow you to use "#1s" to refer to <shift>+<F1>,
"#1c" to refer to <control>+<F1>, and
"#1a" to refer to <alt>+<F1>.
The termcap description for the terminal should have fields named
:s1=:c1=:a1=: respectively, to define the code sent by these key conbinations.
(You should also have :k2=:s2=:c2=:a2=: for the <F2> key, and so on.)
.IP
But there may be problems.
The terminfo database doesn't support :s1=:c1=:a1=:, so no terminfo terminal
description could ever support shift/control/alt function keys;
so you might as well add -DNO_SHIFT_FKEY to CFLAGS if you're using terminfo.
.IP
Note that, even if you have -DNO_FKEYS, you can still configure \*E to use
your function keys my mapping the literal character codes sent by the key.
You just couldn't do it in a terminal-independent way.
.IP "-DTERM_925, -DTERM_AMIGA, -DTERM_VT100, -DTERM_VT52, etc."
The tinytcap.c file contains descriptions of several terminal types.
For each system that uses tinytcap, a reasonable subset of the available
descriptions is actually compiled into \*E.
If you wish to enlarge this subset, then you can add the appropriate -DTERM_XXX
flag to your CFLAGS settings.
.IP
For a list of the available terminal types, check the tinytcap.c file.
.IP -DINTERNAL_TAGS
Normally, \*E uses the "ref" program to perform tag lookup.
This is more powerful than the real vi's tag lookup,
but it can be much slower.
.IP
If you add -DINTERNAL_TAGS to your CFLAGS setting,
then \*E will use its own internal tag lookup code, which is faster.
.IP -DPRSVDIR=\fIdirectory\fR
This controls where preserved files will be placed.
An appropriate default has been chosen for each Operating System,
so you probably don't need to worry about it.
.IP -DFILEPERMS=\fInumber\fR
This affects the attributes of files that are created by \*E;
it is used as the second argument to the creat() function.
The default is 0666 which (on UNIX systems at least) means that
anybody can read or write the new file, but nobody can execute it.
On UNIX systems, the creat() call modifies this via the umask setting.
.IP -DKEYBUFSIZE=\fInumber\fR
This determines the size of the type-ahead buffer that elvis uses.
It also limits the size of keymaps that it can handle.
The default is 1000 characters, which should be plenty.
