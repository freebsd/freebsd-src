.Go 5 "OPTIONS"
.PP
Options may be set or examined via the colon command "set".
The values of options will affect the operation of later commands.
.PP
For convenience, options have both a long descriptive name and a short name
which is easy to type.
You may use either name interchangably.
I like the short names, myself.
.PP
There are three types of options: Boolean, string, and numeric.
Boolean options are made TRUE by giving the name of the option as an
argument to the "set" command;
they are made FALSE by prefixing the name with "no".
For example, "set autoindent" makes the autoindent option TRUE,
and "set noautoindent" makes it FALSE.
\*E also allows boolean options to be toggled by prefixing the name with "neg".
So, ":map g :set neglist^M" will cause the <g> key to alternately toggle the
"list" option on and off.
(The "neg" prefix is an extension; the real vi doesn't support it.)
.PP
To change the value of a string or numeric option, pass the "set" command
the name of the option, followed by an "=" sign and the option's new value.
For example, "set tabstop=8" will give the tabstop option a value of 8.
For string options, you may enclose the new value in quotes.
.LD
.ta 1.9i 2.4i 3.8i
.ps +2
\fBNAMES	TYPE	DEFAULT	MEANING\fP
.ps -2
autoindent, ai	Bool	noai	auto-indent during input
autoprint, ap	Bool	ap	in EX, print the current line
autotab, at	Bool	at	auto-indent allowed to use tabs?
autowrite, aw	Bool	noaw	auto-write when switching files
beautify,  bf	Bool	nobf	strip control chars from file?
charattr, ca	Bool	noca	interpret \\fX sequences?
cc, cc	Str	cc="cc -c"	name of the C compiler
columns, co	Num	co=80	width of the screen
digraph, dig	Bool	nodig	recognize digraphs?
directory, dir	Str	dir="/usr/tmp"	where tmp files are kept
edcompatible, ed	Bool	noed	remember ":s//" options
equalprg, ep	Bool	ep="fmt"	program to run for = operator
errorbells, eb	Bool	eb	ring bell on error
exrc, exrc	Bool	noexrc	read "./.exrc" file?
exrefresh, er	Bool	er	write lines indiviually in EX
flash, vbell	Bool	flash	use visible alternative to bell
flipcase, fc	Str	fc=""	non-ASCII chars flipped by ~
hideformat, hf	Bool	hf	hide text formatter commands
ignorecase, ic	Bool	noic	upper/lowercase match in search
inputmode, im	Bool	noim	start vi in insert mode?
keytime, kt	Num	kt=2	timeout for mapped key entry
keywordprg, kp	Str	kp="ref"	full pathname of shift-K prog
lines, ln	Num	ln=25	number of lines on the screen
list, li	Bool	noli	display lines in "list" mode
magic, ma	Bool	ma	use regular expression in search
make, mk	Str	mk="make"	name of the "make" program
mesg, ms	Bool	ms	allow messages from other users?
modelines, ml	Bool	noml	are modelines processed?
more, more	Bool	more	pause between messages?
nearscroll, ns	Num	ns=15	when to scroll vs. redraw
novice, nov	Bool	nonovice	set options for ease of use
number, nu	Bool	nonumber	show line numbers
paragraphs, para	Str	para="PPppIPLPQP"	names of "paragraph" nroff cmd
prompt, pr	Bool	pr	show ':' prompt in \fIex\fR mode
readonly, ro	Bool	noro	prevent overwriting of orig file
remap, rem	Bool	remap	allow key maps to call key maps
report, re	Num	re=5	report when 5 or more changes
ruler, ru	Bool	noru	display line/column numbers
scroll, sc	Num	sc=12	scroll amount for ^U and ^D
sections, sect	Str	sect="NHSHSSSEse"	names of "section" nroff cmd
shell, sh	Str	sh="/bin/sh"	full pathname of the shell
showmatch, sm	Bool	nosm	show matching ()[]{}
showmode, smd	Bool	nosmd	say when we're in input mode
shiftwidth, sw	Num	sw=8	shift amount for < and >
sidescroll, ss	Num	ss=8	amount of sideways scrolling
sync, sy	Bool	nosy	call sync() often
tabstop, ts	Num	ts=8	width of tab characters
taglength, tl	Num	tl=0	significant chars in tag name
tags, tag	Str	tags="tags" list of tags files
tagstack, tgs	Bool	tgs	enable tagstack?
term, te	Str	te="$TERM"	name of the termcap entry
terse, tr	Bool	notr	give shorter error messages
timeout, to	Bool	to	distinguish <esc> from <arrow>?
warn, wa	Bool	wa	warn for ! if file modified
window, wi	Num	wi=24	lines to redraw after long move
wrapmargin, wm	Num	wm=0	wrap long lines in input mode
wrapscan, ws	Bool	ws	at EOF, searches wrap to line 1
writeany, wr	Bool	nowr	allow :w to clobber files
.DE
.TA
.ne 6
.IP "autoindent, ai"
During input mode, the autoindent option will cause each added line
to begin with the same amount of leading whitespace as the line above it.
Without autoindent, added lines are initially empty.
.IP "autoprint, ap"
This option only affects EX mode.
If the autoprint option on,
and either the cursor has moved to a different line
or the previous command modified the file,
then \*E will print the current line.
.IP "autotab, at"
This option affects the behaviour of the autoindent mode.
If autoindent is turned off, then autotab has no effect.
.IP
When autotab is turned on, elvis will use a mixture of spaces and tabs
to create the proper amount of indentation.
This is the default.
.IP
When autotab is turned off, elvis will only use spaces for auto-indent.
\*E will still insert a real tab character when you hit the <Tab> key, though;
the autotab option only affects \fIautomatic\fR indentation.
.IP "autowrite, aw"
When you're editing one file and decide to switch to another
\- via the :tag command, or :next command, perhaps \-
if your current file has been modified,
then \*E will normally print an error message and refuse to switch.
.IP
However, if the autowrite option is on,
then \*E will write the modified version of the current file
and successfully switch to the new file.
.IP "beautify, bf"
This option causes all control characters to be deleted from the text file,
at the time when you start editing it.
If you're already editing a file when you turn on the beautify option,
then that file won't be affected.
.IP cc
The :cc command runs the C compiler.
This option should be set to the name of your compiler.
.IP "charattr, ca"
Many text formatting programs allow you to designate portions of
your text to be underlined, italicized, or boldface by embedding
the special strings \\fU, \\fI, and \\fB in your text.
The special string \\fP marks the end of underlined or boldface text.
.IP
\*E normally treats those special strings just like any other text.
.IP
However, if the charattr option is on, then \*E will interpret
those special strings correctly,
to display underlined or boldface text on the screen.
(This only works, of course, if your terminal can display
underlined and boldface, and if the TERMCAP entry says how to do it.)
.IP "columns, co"
This option shows how wide your screen is.
.IP "digraph, dig"
This option is used to enable/disable recognition of digraphs.
The default value is nodigraph, which means that digraphs will not be
recognized.
.IP "directory, dir"
\*E stores text in temporary files.
This option allows you to control which directory those temporary files will
appear in.
The default is /usr/tmp.
.IP
This option can only be set in a .exrc file;
after that, \*E will have already started making temporary files
in some other directory, so it would be too late.
.IP "edcompatible, ed"
This option affects the behaviour of the ":s/regexp/text/options" command.
It is normally off (:se noed) which causes all of the substitution options
to be off unless explicitly given.
.IP
However, with edcompatible on (:se ed), the substitution command remembers
which options you used last time.
Those same options will continue to be used until you change them.
In edcompatible mode, when you explicitly give the name of a
substitution option, you will toggle the state of that option.
.IP
This all seems very strange to me, but its implementation was almost free
when I added the ":&" command to repeat the previous substitution,
so there it is.
.IP "equalprg, ep"
This holds the name & arguments of the external filter program
used the the visual = operator.
The defualt value is "fmt",
so the = operator will adjust line breaks in text.
.IP "errorbells, eb"
\*E normally rings a bell when you do something wrong.
This option lets you disable the bell.
.IP exrc
This option specifies whether a .exrc file in the current directory
should be executed.
By default, this option is off (":set noexrc") which prevents elvis from
executing .exrc in the current directory.
If the .exrc file in your home directory turns this option on (":set exrc")
then the \*E will attempt to execute the .exrc file in the current directory.
.IP
This option exist mainly for security reasons.
A mean-spirited person could do something like
.br
	echo >/tmp/.exrc '!rm -rf $HOME'
.br
and then anybody who attempted to edit or view a file in the /tmp directory
would lose most of their files.
With the exrc option turned off, this couldn't happen to you.
.IP "exrefresh, er"
The EX mode of \*E writes many lines to the screen.
You can make \*E either write each line to the screen separately,
or save up many lines and write them all at once.
.IP
The exrefresh option is normally on, so each line is written to the
screen separately.
.IP
You may wish to turn the exrefresh option off (:se noer) if the
"write" system call is costly on your machine, or if you're using a
windowing environment.
(Windowing environments scroll text a lot faster when you write
many lines at once.)
.IP
This option has no effect in visual command mode or input mode.
.IP "flash, vbell"
If your termcap entry describes a visible alternative to ringing
your terminal's bell, then this option will say whether the visible
version gets used or not.
Normally it will be.
.IP
If your termcap does NOT include a visible bell capability,
then the flash option will be off, and you can't turn it on.
.IP "flipcase, fc"
The flipcase option allows you to control how the non-ASCII characters are
altered by the "~" command.
.IP
The string is divided into pairs of characters.
When "~" is applied to a non-ASCII character,
\*E looks up the character in the flipcase string to see which pair it's in,
and replaces it by the other character of the pair.
.IP "hideformat, hf"
Many text formatters require you to embed format commands in your text,
on lines that start with a "." character.
\*E normally displays these lines like any other text,
but if the hideformat option is on,
then format lines are displayed as blank lines.
.IP "ignorecase, ic"
Normally, when \*E searches for text, it treats uppercase letters
as being different for lowercase letters.
.IP
When the ignorecase option is on, uppercase and lowercase are treated as equal.
.IP "inputmode, im"
This option allows you to have \*E start up in insert mode.
You can still exit insert mode at any time by hitting the ESC key, as usual.
Usually, this option would be set in your ".exrc" file.
.IP "keytime, kt"
The arrow keys of most terminals send a multi-character sequence.
It takes a measurable amount of time for these sequences to be transmitted.
The keytime option allows you to control the maximum amount of time
to allow for an arrow key (or other mapped key) to be received in full.
.IP
On most systems, the setting is the number of tenths of a second to allow
between characters.
On some other systems, the setting is in whole seconds.
.IP
Try to avoid setting keytime=1.
Most systems just count clock beats, so if you tried to read a character
shortly before a clock beat, you could allow almost no time at all for
reading the characters.
For higher keytime settings, the difference is less critical.
.IP
If your system's response time is poor, you might want to increase the keytime.
In particular, I've found that when keystrokes must be sent through a network
(via X windows, rlogin, or telnet, for example) the keytime should be set to
at least 1 second.
.IP
As a special case,
you can set keytime to 0 to disable this time limit stuff altogether.
The big problem here is:
If your arrow keys' sequences start with an ESC,
then every time you hit your ESC key \*E will wait... and wait...
to see if maybe that ESC was part of an arrow key's sequence.
.IP
NOTE: this option is a generalization of the timeout option of the real vi.
.IP "keywordprg, kp"
\*E has a special keyword lookup feature.
You move the cursor onto a word, and hit shift-K,
and \*E uses another program to look up the word
and display information about it.
.IP
This option says which program gets run.
.IP
The default value of this option is "ref",
which is a program that looks up the definition of a function in C.
It looks up the function name in a file called "refs" which is created by ctags.
.IP
You can subtitute other programs, such as an English dictionary program
or the online manual.
\*E runs the program, using the keyword as its only argument.
The program should write information to stdout.
The program's exit status should be 0, unless you want \*E to print
"<<< failed >>>".
.IP "lines, ln"
This option says how many lines you screen has.
.IP "list, li"
In nolist mode (the default), \*E displays text in a "normal" manner
-- with tabs expanded to an appropriate number of spaces, etc.
.IP
However, sometimes it is useful to have tab characters displayed differently.
In list mode, tabs are displayed as "^I",
and a "$" is displayed at the end of each line.
.IP "magic, ma"
The search mechanism in \*E can accept "regular expressions"
-- strings in which certain characters have special meaning.
.IP
The magic option is normally on, which causes these characters to be treated
specially.
.IP
If you turn the magic option off (:se noma),
then all characters except ^ and $ are treated literally.
^ and $ retain their special meanings regardless of the setting of magic.
.IP "make, mk"
The :make command runs your "make" program.
This option defines the name of your "make" program.
.IP mesg
With the real vi, running under real UNIX,
":set nomesg" would prevent other users from sending you messages.
\*E ignores it, though.
.IP "modelines, ml"
\*E supports modelines.
Modelines are lines near the beginning or end of your text file which
contain "ex:yowza:",
where "yowza" is any EX command.
A typical "yowza" would be something like "set ts=5 ca kp=spell wm=15".
Other text may also appear on a modeline,
so you can place the "ex:yowza:" in a comment:
.br
.ID
/* ex:set sw=4 ai: */
.DE
.IP
Normally these lines are ignored, for security reasons,
but if you have "set modelines" in your .exrc file
then "yowza" is executed.
.IP "nearscroll, ns"
The line that contains the cursor will always be on the screen.
If you move the cursor to a line that isn't on the screen,
then elvis will either scroll
(if the cursor's line is nearly on the screen already)
or redraw the screen completely with the cursor's line centered
(if the cursor line is \fInot\fR near the screen already).
.IP
This option allows you to control elvis' idea of "near".
A value of 15 is typical.
A value of 1 would cause elvis to scroll no more that one line.
A value of 0 disables scrolling.
.IP "novice, nov"
The command ":set novice" is equivelent to ":set nomagic report=1 showmode".
.IP "number, nu"
The "number" option causes \*E to display line numbers at the start of
each line.
The numbers are not actually part of the text; when the file is written out,
it will be written without line numbers.
.IP "paragraphs, pa"
The { and } commands move the cursor forward or backward in increments
of one paragraph.
Paragraphs may be separated by blank lines, or by a "dot" command of
a text formatter.
Different text formatters use different "dot" commands.
This option allows you to configure \*E to work with your text formatter.
.IP
It is assumed that your formatter uses commands that start with a
"." character at the front of a line,
and then have a one- or two-character command name.
.IP
The value of the paragraphs option is a string in which each pair
of characters is one possible form of your text formatter's paragraph
command.
.IP "more"
When \*E must display a sequence of messages at the bottom line of the screen
in visual mode, it normally pauses after all but the last one, so you have
time to read them all.
.IP
If you turn off the "more" option, then \*E will not pause.
This means you can only read the last message, but it is usually the most
important one anyway.
.IP "prompt, pr"
If you ":set noprompt", then \*E will no longer emit a ':' when it
expects you to type in an \fIex\fR command.
This is slightly useful if you're using an astonishingly slow UNIX machine,
but the rest of us can just ignore this one.
.IP "readonly, ro"
Normally, \*E will let you write back any file to which you have
write permission.
If you don't have write permission, then you can only write the changed
version of the file to a \fIdifferent\fP file.
.IP
If you set the readonly option,
then \*E will pretend you don't have write permission to \fIany\fP file you edit.
It is useful when you really only mean to use \*E to look at a file,
not to change it.
This way you can't change it accidentally.
.IP
This option is normally off, unless you use the "view" alias of \*E.
"View" is like "vi" except that the readonly option is on.
.IP "remap"
The ":map" command allows you to convert one key sequence into another.
The remap option allows you to specify what should happen if portions of
that other sequence are also in the map table.
If remap is on, then those portions will also be mapped, just as if they
had been typed on the keyboard.
If remap is off, then the matching portions will not be mapped.
.IP
For example, if you enter the commands ":map A B" and ":map B C",
then when remap is on, A will be converted to C.
But when remap is off, A will be converted only to B.
.IP "report, re"
Commands in \*E may affect many lines.
For commands that affect a lot of lines, \*E will output a message saying
what was done and how many lines were affected.
This option allows you to define what "a lot of lines" means.
The default is 5, so any command which affects 5 or more lines will cause
a message to be shown.
.IP "ruler, ru"
This option is normally off.
If you turn it on, then \*E will constantly display the line/column numbers
of the cursor, at the bottom of the screen.
.IP "scroll, sc"
The ^U and ^D keys normally scroll backward or forward by half a screenful,
but this is adjustable.
The value of this option says how many lines those keys should scroll by.
If you invoke ^U or ^D with a count argument (for example, "33^D") then
this option's value is set to the count.
.IP "sections, se"
The [[ and ]] commands move the cursor backward or forward in increments of
1 section.
Sections may be delimited by a { character in column 1
(which is useful for C source code)
or by means of a text formatter's "dot" commands.
.IP
This option allows you to configure \*E to work with your text formatter's
"section" command, in exectly the same way that the paragraphs option makes
it work with the formatter's "paragraphs" command.
.IP "shell, sh"
When \*E forks a shell
(perhaps for the :! or :shell commands)
this is the program that is uses as a shell.
This is "/bin/sh" by default,
unless you have set the SHELL (or COMSPEC, for MS-DOS) environment variable,
it which case the default value is copied from the environment.
.IP "shiftwidth, sw"
The < and > commands shift text left or right by some uniform number of columns.
The shiftwidth option defines that "uniform number".
The default is 8.
.IP "showmatch, sm"
With showmatch set,
in input mode every time you hit one of )}],
\*E will momentarily move the cursor to the matching ({[.
.IP "showmode, smd"
In visual mode, it is easy to forget whether you're in the visual command mode
or input/replace mode.
Normally, the showmode option is off, and you haven't a clue as to which mode
you're in.
If you turn the showmode option on, though, a little message will appear in the
lower right-hand corner of your screen, telling you which mode you're in.
.IP "sidescroll, ss"
For long lines, \*E scrolls sideways.
(This is different from the real vi,
which wraps a single long line onto several rows of the screen.)
.IP
To minimize the number of scrolls needed,
\*E moves the screen sideways by several characters at a time.
The value of this option says how many characters' widths to scroll at a time.
.IP
Generally, the faster your screen can be redrawn,
the lower the value you will want in this option.
.IP "sync, sy"
If the system crashes during an edit session, then most of your work
can be recovered from the temporary file that \*E uses to store
changes.
However, sometimes the OS will not copy changes to the
hard disk immediately, so recovery might not be possible.
The [no]sync option lets you control this.
.IP
In nosync mode (which is the default, for UNIX), \*E lets the operating system
control when data is written to the disk.
This is generally faster.
.IP
In sync mode (which is the default for MS-DOS, AmigaDos, and Atari TOS),
\*E forces all changes out
to disk every time you make a change.
This is generally safer, but slower.
It can also be a rather rude thing to do on a multi-user system.
.IP "tabstop, ts"
Tab characters are normally 8 characters wide,
but you can change their widths by means of this option.
.IP "taglength, tl"
This option allows you to specify how many characters of a tag's name
must match when performing tag lookup.
As a special case, ":set taglength=0" means that all characters of a tag's
name must match.
.IP
Note: some configurations of \*E don't support this option.
.IP "tags, tag"
If your version of elvis is compiled with -DINTERNAL_TAGS,
then this is a space-delimited list of tags files.
When you tell elvis to look up a tag,
it searches though each file in turn until it finds the tag.
.IP
If your version of elvis is compiled without -DINTERNAL_TAGS,
then you can achieve the same effect via an environment variable called
TAGPATH.
TAGPATH's value is a colon-delimited list of file or directory names.
(For some operating systems, including MS-DOS, the list is delimited by
semicolons instead of colons.)
.IP "tagstack"
This option allows you to disable the tagstack.
I can't think of any reason why you would want to do that.
.IP "term, te"
This read-only option shows the name of the termcap entry that
\*E is using for your terminal.
.IP "terse, tr"
The real vi uses this option to select longer vs. shorter error messages.
\*E has only one set of error messages, though, so this option has no effect.
.IP "timeout, to"
The command ":set notimeout" is equivelent to ":set keytime=0",
and ":set timeout" is equivelent to ":set keytime=1".
This affects the behaviour of the <Esc> key.
See the discussion of the "keytime" option for more information.
.IP "warn, wa"
If you have modified a file but not yet written it back to disk, then
\*E will normally print a warning before executing a ":!cmd" command.
However, in nowarn mode, this warning is not given.
.IP
\*E also normally prints a message after a successful search that
wrapped at EOF.
The [no]warn option can also disable this warning.
.IP "window, wi"
This option controls how many lines are redrawn after a long move.
.IP
On fast terminals, this is usually set to the number of rows that the
terminal can display, minus one.
This causes the entire screen to be filled with text around the cursor.
.IP
On slow terminals, you may wish to reduce this value to about 7 or so.
That way, if you're doing something like repeatedly hitting 'n' to search
for each occurrence of some string and trying to find a particular occurrence,
then you don't need to wait as long for \*E to redraw the screen after each
search.
.IP "wrapmargin, wm"
Normally (with wrapmargin=0) \*E will let you type in extremely long
lines, if you wish.
.IP
However, with warpmargin set to something other that 0 (wrapmargin=10
is nice), \*E will automatically cause long lines to be "wrapped"
on a word break for lines come too close to the right-hand margin.
For example: On an 80-column screen, ":set wm=10" will cause lines to
wrap when their length exceeds 70 columns.
.IP "wrapscan, ws"
Normally, when you search for something, \*E will find it no matter
where it is in the file.
\*E starts at the cursor position, and searches forward.
If \*E hits EOF without finding what you're looking for,
then it wraps around to continue searching from line 1.
If you turn off the wrapscan option (:se nows),
then when \*E hits EOF during a search, it will stop and say so.
.IP "writeany, wr"
With "writeany" turned off, elvis will prevent you from accidentally
overwriting a file.
For example, if "foo" exists then ":w foo" will fail.
If you turn on the "writeany" option, then ":w foo" will work.
.IP
Regardless of the setting of "writeany", though, ":w! foo" will work.
The '!' forces the ":w" command to write the file unless the operating system
won't allow it.
