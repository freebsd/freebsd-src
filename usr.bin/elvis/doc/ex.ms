.Go 3 "COLON MODE COMMANDS"
.ID
.ps
.in 0.8i
.ta 2i 3.i
.\" NOTE: The following macro is used to output a single line of the
.\" command chart.  Its usage is:
.\"
.\"		.Cm <linespecs> <name> <arguments>...
.\"
.de Cm
.if "\\$1"0" \t\\$2\t\\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.if "\\$1"1" \s'-2'[line]\s'+2'\t\\$2\t\\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.if "\\$1"2" \s'-2'[line][,line]\s'+2'\t\\$2\t\\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
..
.if t .ds Q ``
.if t .ds U ''
.if n .ds Q "
.if n .ds U "
\s'+2'LINES	COMMAND	ARGUMENTS\s'-2'
.Cm 0 ab[br] [short] [expanded form]
.Cm 1 a[ppend][!]
.Cm 0 ar[gs] [files]
.Cm 0 cc [files]
.Cm 0 cd[!] [directory]
.Cm 2 c[hange]
.Cm 0 chd[ir][!] [directory]
.Cm 2 co[py] line
.Cm 0 col[or] [when] [[\*Qlight\*U] color] [\*Qon\*U color]
.Cm 2 d[elete] [\*Ux]
.Cm 0 dig[raph][!] [XX [Y]]
.Cm 0 e[dit][!] [file]
.Cm 0 er[rlist][!] [errlist]
.Cm 0 f[ile] [file]
.Cm 2 g[lobal] /regexp/ command
.Cm 1 i[nsert]
.Cm 2 j[oin][!]
.Cm 2 l[ist]
.Cm 0 mak[e] [target]
.Cm 0 map[!] key mapped_to
.Cm 1 ma[rk]  \*Ux
.Cm 0 mk[exrc]
.Cm 2 m[ove] line
.Cm 0 n[ext][!] [files]
.Cm 0 N[ext][!]
.Cm 2 nu[mber]
.Cm 2 p[rint]
.Cm 1 pu[t] [\*Ux]
.Cm 0 q[uit][!]
.Cm 1 r[ead] file
.Cm 0 rew[ind][!]
.Cm 0 se[t] [options]
.Cm 0 so[urce] file
.Cm 2 s[ubstitute] /regexp/replacement/[p][g][c]
.Cm 0 ta[g][!] tagname
.Cm 0 una[bbr] [short]
.Cm 0 u[ndo]
.Cm 0 unm[ap][!] key
.Cm 0 ve[rsion]
.Cm 2 v[global] /regexp/ command
.Cm 0 vi[sual] [filename]
.Cm 0 wq 
.Cm 2 w[rite][!] [[>>]file]
.Cm 0 x[it][!]
.Cm 2 y[ank] [\*Ux]
.Cm 2 ! command
.Cm 2 < 
.Cm 2 = 
.Cm 2 > 
.Cm 2 & 
.Cm 0 @ "" \*Ux
.DE
.TA
.PP
To use colon mode commands, you must switch from visual command
mode to colon command mode.
The visual mode commands to do this are ":" for a single colon command,
or "Q" for many colon mode commands.
.NH 2
Line Specifiers
.PP
Line specifiers are always optional.
The first line specifier of most commands usually defaults to the current line.
The second line specifier usually defaults to be the same
as the first line specifier.
Exceptions are :write, :global, and :vglobal, which act on all lines of the
file by default, and :!, which acts on no lines by default.
.PP
Line specifiers consist of an absolute part and a relative part.
The absolute part of a line specifier may be either an explicit line number,
a mark, a dot to denote the current line, a dollar sign to denote the last
line of the file, or a forward or backward search.
.PP
An explicit line number is simply a decimal number, expressed as a
string of digits.
.PP
A mark is typed in as an apostrophe followed by a letter.
Marks must be set before they can be used.
You can set a mark in visual command mode by typing "m" and a letter,
or you can set it in colon command mode via the "mark" command.
.PP
A forward search is typed in as a regular expression surrounded by
slash characters; searching begins at the default line.
A backward search is typed in as a regular expression surrounded by
question marks; searching begins at the line before the default line.
.PP
If you omit the absolute part, then the default line is used.
.PP
The relative part of a line specifier is typed as a "+" or "-" character
followed by a decimal number.
The number is added to or subtracted from the absolute part
of the line specifier to produce the final line number.
.PP
As a special case, the % character may be used to specify all lines of the file.
It is roughly equivelent to saying 1,$.
This can be a handy shortcut.
.PP
Some examples:
.LD
.ps
.ta 0.5i 1.8i
	:p	print the current line
	:37p	print line 37
	:'gp	print the line which contains mark g
	:/foo/p	print the next line that contains "foo"
	:$p	print the last line of the file
	:20,30p	print lines 20 through 30
	:1,$p	print all lines of the file
	:%p	print all lines of the file
	:/foo/-2,+4p	print 5 lines around the next "foo"
.TA
.DE
.NH 2
Text Entry Commands
.if n .ul 0
.ID
.ps
[line] append
[line][,line] change ["x]
[line] insert
.DE
.PP
The \fBa\fRppend command inserts text after the specified line.
.PP
The \fBi\fRnsert command inserts text before the specified line.
.PP
The \fBc\fRhange command copies the range of lines into a cut buffer,
deletes them, and inserts new text where the old text used to be.
.PP
For all of these commands, you indicate the end of the text you're
inserting by hitting ^D or by entering a line which contains only a
period.
.NH 2
Cut & Paste Commands
.if n .ul 0
.ID
.ps
[line][,line] delete ["x]
[line][,line] yank ["x]
[line] put ["x]
[line][,line] copy line
[line][,line] to line
[line][,line] move line
.DE
.PP
The \fBd\fRelete command copies the specified range of lines into a
cut buffer, and then deletes them.
.PP
The \fBy\fRank command copies the specified range of lines into a cut
buffer, but does *not* delete them.
.PP
The \fBpu\fRt command inserts text from a cut buffer after the
specified line.
.PP
The \fBco\fRpy and \fBt\fRo commands yank the specified range of lines and
then immediately paste them after some other line.
.PP
The \fBm\fRove command deletes the specified range of lines and then
immediately pastes them after some other line.
If the destination line comes after the deleted text,
then it will be adjusted automatically to account for the deleted lines.
.NH 2
Display Text Commands
.if n .ul 0
.ID
.ps
[line][,line] print
[line][,line] list
[line][,line] number
.DE
.PP
The \fBp\fRrint command displays the specified range of lines.
.PP
The \fBnu\fRmber command displays the lines, with line numbers.
.PP
The \fBl\fRist command also displays them, but it is careful to make
control characters visible.
.NH 2
Global Operations Commands
.if n .ul 0
.ID
.ps
[line][,line] global /regexp/ command
[line][,line] vglobal /regexp/ command
.DE
.PP
The \fBg\fRlobal command searches through the lines of the specified range
(or through the whole file if no range is specified)
for lines that contain a given regular expression.
It then moves the cursor to each of these lines and
runs some other command on them.
.PP
The \fBv\fRglobal command is similar, but it searches for lines that \fIdon't\fR
contain the regular expression.
.NH 2
Line Editing Commands
.if n .ul 0
.ID
.ps
[line][,line] join[!]
[line][,line] ! program
[line][,line] <
[line][,line] >
[line][,line] substitute /regexp/replacement/[p][g][c]
[line][,line] &
.DE
.PP
The \fBj\fRoin command catenates all lines in the specified range together
to form one big line.
If only a single line is specified, then the following line is catenated
onto it.
The normal ":join" inserts one or two spaces between the lines;
the ":join!" variation (with a '!') doesn't insert spaces.
.PP
The \fB!\fR command runs an external filter program,
and feeds the specified range of lines to it's stdin.
The lines are then replaced by the output of the filter.
A typical example would be ":'a,'z!sort" to sort the lines 'a,'z.
.PP
The \fB<\fR and \fB>\fR commands shift the specified range of lines left or right,
normally by the width of 1 tab character.
The "shiftwidth" option determines the shifting amount.
.PP
The \fBs\fRubstitute command finds the regular expression in each line,
and replaces it with the replacement text.
The "p" option causes the altered lines to be printed.
The "g" option permits all instances of the regular expression
to be found & replaced.
(Without "g", only the first occurrence in each line is replaced.)
The "c" option asks for confirmation before each substitution.
.PP
The \fB&\fR command repeats the previous substitution command.
Actually, "&" is equivelent to "s//~/" with the same options as last time.
It searches for the last regular expression that you specified for any purpose,
and replaces it with the the same text
that was used in the previous substitution.
.NH 2
Undo Command
.if n .ul 0
.ID
.ps
undo
.DE
.PP
The \fBu\fRndo command restores the file to the state it was in before
your most recent command which changed text.
.NH 2
Configuration & Status Commands
.if n .ul 0
.ID
.ps
map[!] [key mapped_to]
unmap[!] key
abbr [word expanded_form_of_word]
unabbr word
digraph[!] [XX [Y]]
set [options]
mkexrc
[line] mark "x
visual
version
[line][,line] =
file [file]
source file
@ "x
color [when] [["light"] color] ["on" color]
.DE
.PP
The \fBma\fRp command allows you to configure \*E to recognize your function keys,
and treat them as though they transmitted some other sequence of characters.
Normally this mapping is done only when in the visual command mode,
but with the [!] present it will map keys under input and replace modes as well.
When this command is given with no arguments,
it prints a table showing all mappings currently in effect.
When called with two arguments, the first is the sequence that your
function key really sends, and the second is the sequence that you want
\*E to treat it as having sent.
As a special case, if the first argument is a number then \*E will map the
corresponding function key;
for example, ":map 7 dd" will cause the <F7> key to delete a line.
.PP
The \fBunm\fRap command removes key definitions that were made via the map command.
.PP
The \fBab\fRbr command is used to define/list a table of abbreviations.
The table contains both the abbreviated form and the fully spelled-out form.
When you're in visual input mode, and you type in the abbreviated form,
\*E will replace the abbreviated form with the fully spelled-out form.
When this command is called without arguments, it lists the table;
with two or more arguments, the first argument is taken as the abbreviated
form, and the rest of the command line is the fully-spelled out form.
.PP
The \fBuna\fRbbr command deletes entries from the abbr table.
.PP
The \fBdi\fRgraph command allows you to display the set of digraphs that \*E is
using, or add/remove a digraph.
To list the set of digraphs, use the digraph command with no arguments.
To add a digraph, you should give the digraph command two arguments.
The first argument is the two ASCII characters that are to be combined;
the second is the non-ASCII character that they represent.
The non-ASCII character's most significant bit is automatically set by the
digraph command, unless to append a ! to the command name.
Removal of a digraph is similar to adding a digraph, except that you should
leave off the second argument.
.PP
The \fBse\fRt command allows you examine or set various options.
With no arguments, it displays the values of options that have been changed.
With the single argument "all" it displays the values of all options,
regardless of whether they've been explicitly set or not.
Otherwise, the arguments are treated as options to be set.
.PP
The \fBmk\fRexrc command saves the current configuration to a file
called ".exrc" in the current directory.
.PP
The mar\fBk\fR command defines a named mark to refer to a specific place
in the file.
This mark may be used later to specify lines for other commands.
.PP
The \fBvi\fRsual command puts the editor into visual mode.
Instead of emulating ex, \*E will start emulating vi.
.PP
The \fBve\fRrsion command tells you that what version of \*E this is.
.PP
The \fB=\fR command tells you what line you specified, or,
if you specified a range of lines, it will tell you both endpoints and
the number of lines included in the range.
.PP
The \fBf\fRile command tells you the name of the file,
whether it has been modified,
the number of lines in the file,
and the current line number.
You can also use it to change the name of the current file.
.PP
The \fBso\fRurce command reads a sequence of colon mode commands from a file,
and interprets them.
.PP
The \fB@\fR command executes the contents of a cut-buffer as EX commands.
.PP
The \fBcol\fRor command only works under MS-DOS, or if you have an ANSI-compatible
color terminal.
It allows you to set the foreground and background colors
for different types of text:
normal, bold, italic, underlined, standout, pop-up menu, and visible selection.
By default, it changes the "normal" colors;
to change other colors, the first argument to the :color command should be
the first letter of the type of text you want.
The syntax for the colors themselves is fairly intuitive.
For example, ":color light cyan on blue" causes normal text to be displayed
in light cyan on a blue background, and
":color b bright white" causes bold text to be displayed in bright white on
a blue background.
The background color always defaults to the current background color of
normal text.
Your first :color command \fImust\fP specify both the foreground and background
for normal text.
.NH 2
Multiple File Commands
.if n .ul 0
.ID
.ps
args [files]
next[!] [files]
Next[!]
previous[!]
rewind[!]
.DE
.PP
When you invoke \*E from your shell's command line,
any filenames that you give to \*E as arguments are stored in the args list.
The \fBar\fRgs command will display this list, or define a new one.
.PP
The \fBn\fRext command switches from the current file to the next one
in the args list.
You may specify a new args list here, too.
.PP
The \fBN\fRext and \fBpre\fRvious commands
(they're really aliases for the same command)
switch from the current file to the preceding file in the args list.
.PP
The \fBrew\fRind command switches from the current file to the first file
in the args list.
.NH 2
Switching Files
.if n .ul 0
.ID
.ps
edit[!] [file]
tag[!] tagname
.DE
.PP
The \fBe\fRdit command allows to switch from the current file to some other file.
This has nothing to do with the args list, by the way.
.PP
The \fBta\fRg command looks up a given tagname in a file called "tags".
This tells it which file the tag is in, and how to find it in that file.
\*E then switches to the tag's file and finds the tag.
.NH 2
Working with a Compiler
.if n .ul 0
.ID
.ps
cc [files]
make [target]
errlist[!] [errlist]
.DE
.PP
The \fBcc\fR and \fBmak\fRe commands execute your compiler or "make" utility
and redirect any error messages into a file called "errlist".
By default, cc is run on the current file.
(You should write it before running cc.)
The contents of the "errlist" file are then scanned for error messages.
If an error message is found, then the cursor is moved to the line where
the error was detected,
and the description of the error is displayed on the status line.
.PP
After you've fixed one error, the \fBer\fRrlist command will move
the cursor to the next error.
In visual command mode,
hitting `*' will do this, too.
.PP
You can also create an "errlist" file from outside of \*E,
and use "elvis -m" to start \*E and have the cursor moved to the
first error.
Note that you don't need to supply a filename with "elvis -m" because
the error messages always say which source file an error is in.
.PP
Note:
When you use errlist repeatedly to fix several errors in a single file,
it will attempt to adjust the reported line numbers to allow for lines
that you have inserted or deleted.
These adjustments are made with the assumption that you will work though
the file from the beginning to the end.
.NH 2
Exit Commands
.if n .ul 0
.ID
.ps
quit[!]
wq
xit
.DE
.PP
The \fBq\fRuit command exits from the editor without saving your file.
.PP
The \fBwq\fR command writes your file out, then then exits.
.PP
The \fBx\fRit command is similar to the \fBwq\fR command, except that
\fBx\fRit won't bother to write your file if you haven't modified it.
.NH 2
File I/O Commands
.if n .ul 0
.ID
.ps
[line] read file
[line][,line] write[!] [[>>]file]
.DE
.PP
The \fBr\fRead command gets text from another file and inserts it
after the specified line.
It can also read the output of a program;
simply precede the program name by a '!' and use it in place of the file name.
.PP
The \fBw\fRrite command writes the whole file, or just part of it,
to some other file.
The !, if present, will permit the lines to be written even if you've set
the readonly option.
If you precede the filename by >> then the lines will be appended to the file.
You can send the lines to the standard input of a program by replacing the
filename with a '!' followed by the command and its arguments.
.PP
Note: Be careful not to confuse ":w!filename" and ":w !command".
To write to a program, you must have at least one blank before the '!'.
.NH 2
Directory Commands
.if n .ul 0
.ID
.ps
cd [directory]
chdir [directory]
shell
.DE
.PP
The \fBcd\fR and \fBchd\fRir commands
(really two names for one command)
switch the current working directory.
.PP
The \fBsh\fRell command starts an interactive shell.
.NH 2
Debugging Commands
.if n .ul 0
.ID
.ps
[line][,line] debug[!]
validate[!]
.DE
.PP
These commands are only available if you compile \*E with the -DDEBUG flag.
.PP
The de\fBb\fRug command lists statistics for the blocks which contain
the specified range of lines.
If the ! is present, then the contents of those blocks is displayed, too.
.PP
The \fBva\fRlidate command checks certain variables for internal consistency.
Normally it doesn't output anything unless it detects a problem.
With the !, though, it will always produce *some* output.
