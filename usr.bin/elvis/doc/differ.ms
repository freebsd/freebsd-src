.Go 7 "DIFFERENCES BETWEEN \*E & BSD VI/EX"
.PP
\*E is not 100% compatible with the real vi/ex.
\*E has many small extensions, some omissions, and a few features which
are implemented in a slightly different manner.
.NH 2
Extensions
.IP "Save Configuration" 1i
The :mkexrc command saves the current :set, :map, :ab, :color, and :digraph
configurations in the ".exrc" file in your current directory.
.IP "Previous File" 1i
The :N or :prev command moves backwards through the args list.
.IP "Center Current Row" 1i
In visual command mode, the (lowercase) "zz" command will center the current
line on the screen, like "z=".
.IP "Changing Repeat Count" 1i
The default count value for . is the same as the previous command
which . is meant to repeat.
However, you can supply a new count if you wish.
For example, after "3dw", "." will delete 3 words,
but "5." will delete 5 words.
.IP "Previous Text" 1i
The text which was most recently input
(via a "cw" command, or something similar)
is saved in a cut buffer called ". (which
is a pretty hard name to write in an English sentence).
.IP "Keyword Lookup" 1i
In visual command mode, you can move the cursor onto a word and press
shift-K to have \*E run a reference program to look that word up.
This command alone is worth the price of admission!
See the ctags and ref programs.
.IP "Increment/Decrement" 1i
In visual command mode, you can move the cursor onto a number and
then hit ## or #+ to increment that number by 1.
To increment it by a larger amount,
type in the increment value before hitting the initial #.
The number can also be decremented or set by hitting #- or #=, respectively.
.IP "Input Mode" 1i
You can backspace past the beginning of the line.
.IP "" 1i
The arrow keys work in input mode.
.IP "" 1i
If you type control-A, then the text that you input last time is inserted.
You will remain in input mode, so you can backspace over part of it,
or add more to it.
(This is sort of like control-@ on the real vi,
except that control-A really works.)
.IP "" 1i
Control-P will insert the contents of the cut buffer.
.IP "" 1i
Real vi can only remember up to 128 characters of input,
but \*E can remember any amount.
.IP "" 1i
The ^T and ^D keys can adjust the indent of a line no matter where
the cursor happens to be in that line.
.IP "" 1i
You can save your file and exit \*E directly from input mode by hitting
control-Z twice.
.IP "" 1i
\*E supports digraphs as a way to enter non-ASCII characters.
.IP "Start in Input Mode" 1i
If you ":set inputmode" in your .exrc file, then \*E will start up in
input mode instead of visual command mode.
.IP "Visible Fonts" 1i
With ":set charattr", \*E can display "backslash-f" style character attributes on the
screen as you edit.
The following example shows the recognized atributes:
.sp
.ti +0.5i
normal \\fBboldface\\fR \\fIitalics\\fR \\fUunderlined\\fR normal
.sp
NOTE: you must compile \*E without the -DNO_CHARATTR flag for
this to work.
.IP "File Syncing" 1i
After a crash, you can usually recover the altered form of the file
from the temporary file that \*E uses -- unless the temporary file was
corrupted.
.IP "" 1i
UNIX systems use a delayed-write cache, which means that when \*E tries to
write to the temporary file, the information might still be in RAM instead
of on the disk.
A power failure at that time would cause the in-RAM information to be lost.
UNIX's sync() call will force all such information to disk.
.IP "" 1i
MS-DOS and Atari TOS don't write a file's length to disk until that file
is closed.
Consequently, the temporary file would appear to be 0 bytes long if power
failed when we were editing.
To avoid this problem, a sync() function has been written which will close
the temporary file and then immediately reopen it.
.IP "Cursor Shape" 1i
\*E changes the shape of the cursor to indicate which mode you're in,
if your terminal's termcap entry includes the necessary capabilities.
.IP "Hide nroff Lines" 1i
The ":set hideformat" option hides nroff format control lines.
(They are displayed on the screen as blank lines.)
.ne 7
.IP "Compiler Interface" 1i
\*E is clever enough to parse the error messages emitted by many compilers.
To use this feature,
you should collect your compiler's error messages into a file called "errlist";
\*E will read this file,
determine which source file caused the error messages,
start editing that file,
move the cursor to the line where the error was detected,
and display the error message on the status line.
Nifty!
.IP "Visible Text Selection" 1i
In visual command mode, 'v' starts visibly selecting characters and
\&'V' starts visibly selecting whole lines.
The character or line where the cursor is located becomes one
endpoint of the selection.
You can then use the standard cursor movement commands to move the cursor
to the other endpoint, and then press one of the operator commands
(c/d/y/</>/!/=/\\).
The operator will then immediately be applied to the selected text.
.IP "Pop-up Menu Operator" 1i
The '\\' key is a new operator,
similar in operation to the c/d/y/</>/! operators.
It conjures up a menu, from which you can select any of the other
operators plus a few other common commands.
.IP "Preset Filter Operator" 1i
The '=' key is another new operator.
It is similar to the '!' operator, except that while
\&'!' asks you to type in a filter command each time,
\&'=' assumes it should always run the command stored in the \fIequalprg\fR option.
.IP "Move to a Given Percentage" 1i
The '%' movement key can now accept an optional count.
Without a count, the '%' key still moves to a matching parenthesis
like it always did.
With a count somewhere between 1 and 100, though, it moves the cursor to
approximately a given percentage of the way through the file.
For example, typing "50%" will move the cursor to the middle of the file.
.IP "Regular Expressions"
In regular expressions, several new forms of closure operators are supported:
\\{\fIn\fR}, \\{\fIn\fR,\fIm\fR}, \\+, and \\?.
.NH 2
Omissions
.PP
The replace mode is a hack.
It doesn't save the text that it overwrites.
.PP
Long lines are displayed differently -- where the real vi would
wrap a long line onto several rows of the screen, \*E simply
displays part of the line, and allows you to scroll the screen
sideways to see the rest of it.
.PP
The ":preserve" and ":recover" commands are missing.
So is the -r flag.
I've never had a good reason to use ":preserve",
and since ":recover" is used so rarely
I decided to implement it as a separate program.
There's no need to load the recovery code into memory every
time you edit a file, I figured.
.PP
LISP support is missing.
However, the = key is still an operator that reformats lines of text.
By default, it reformats lines by sending them through the \fIfmt\fP filter,
but you could write your own LISP beautifier and configure elvis to use it.
Key mappings could take care of most other differences.
Auto-indent is the only thing that is irrecoverably lost.
.PP
Autoindent mode acts a little different from the real vi, anyway.
It doesn't handle ^^D or 0^D correctly.
On the other hand, it \fIdoes\fP allow ^D and ^T to be used anywhere in the
line, to adjust the indentation for the whole line.
