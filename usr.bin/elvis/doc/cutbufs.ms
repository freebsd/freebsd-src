.Go 6 "CUT BUFFERS"
.PP
When \*E deletes text, it stores that text in a cut buffer.
This happens in both visual mode and EX mode.
There is no practical limit to how much text a cut buffer can hold.
.PP
There are 36 cut buffers:
26 named buffers ("a through "z),
9 anonymous buffers ("1 through "9),
and 1 extra cut buffer (".).
.PP
In EX mode, the :move and :copy commands use a cut buffer to temporarily
hold the text to be moved/copied.
.NH 2
Putting text into a Cut Buffer
.PP
In visual mode, text is copied into a cut buffer when you use the
d, y, c, C, s, or x commands.
There are also a few others.
.PP
By default, the text goes into the "1 buffer.
The text that used to be in "1 gets shifted into "2,
"2 gets shifted into "3, and so on.
The text that used to be in "9 is lost.
This way, the last 9 things you deleted are still accessible.
.PP
You can also put the text into a named buffer -- "a through "z.
To do this, you should type the buffer's name
(two keystrokes: a double-quote and a lowercase letter)
before the command that will cut the text.
When you do this, "1 through "9 are not affected by the cut.
.PP
You can append text to one of the named buffers.
To do this, type the buffer's name in uppercase
(a double-quote and an uppercase letter)
before the d/y/c/C/s/x command.
.PP
The ". buffer is special.
It isn't affected by the d/y/c/C/s/x command.
Instead, it stores the text that you typed in
the last time you were in input mode.
It is used to implement the . visual command,
and ^A in input mode.
.PP
In EX mode (also known as colon mode),
the :delete, :change, and :yank commands all copy text into a cut buffer.
Like the visual commands, these EX commands normally use the "1 buffer,
but you can use one of the named buffers by giving its name after the command.
For example,
.sp 1
.ti +0.5i
:20,30y a
.sp
.LP
will copy lines 20 through 30 into cut buffer "a.
.PP
You can't directly put text into the ". buffer, or the "2 through "9 buffers.
.NH 2
Pasting from a Cut Buffer
.PP
There are two styles of pasting:
line-mode and character-mode.
If a cut buffer contains whole lines (from a command like "dd")
then line-mode pasting is used;
if it contains partial lines (from a command like "dw")
then character-mode pasting is used.
The EX commands always cut whole lines.
.PP
Character-mode pasting causes the text to be inserted into the line that
the cursor is on.
.PP
Line-mode pasting inserts the text on a new line above or below the line
that the cursor is on.
It doesn't affect the cursor's line at all.
.PP
In visual mode, the p and P commands insert text from a cut buffer.
Uppercase P will insert it before the cursor,
and lowercase p will insert it after the cursor.
Normally, these commands will paste from the "1 buffer, but you can
specify any other buffer to paste from.
Just type its name (a double-quote and another character)
before you type the P or p.
.PP
In EX mode, the (pu)t command pastes text after a given line.
To paste from a buffer other that "1,
enter its name after the command.
.NH 2
Macros
.PP
The contents of a named cut buffer can be executed as a series of
ex/vi commands.
.PP
To put the instructions into the cut buffer, you must first insert
them into the file, and then delete them into a named cut buffer.
.PP
To execute a cut buffer's contents as EX commands,
you should give the EX command "@" and the name of the buffer.
For example, :@z will execute "z as a series of EX commands.
.PP
To execute a cut buffer's contents as visual commands,
you should give the visual command "@" and the letter of the buffer's name.
The visual "@" command is different from the EX "@" command.
They interpret the cut buffer's contents differently.
.PP
The visual @ command can be rather finicky.
Each character in the buffer is interpretted as a keystroke.
If you load the instructions into the cut buffer via a "zdd command,
then the newline character at the end of the line will be executed just
like any other character, so the cursor would be moved down 1 line.
If you don't want the cursor to move down 1 line at the end of each
@z command, then you should load the cut buffer by saying 0"zD instead.
.PP
Although cut buffers can hold any amount of text,
\*E can only \fIexecute\fR small buffers.
The size limit is roughly 1000 characters, for either EX macros or VI macros.
If a buffer is too large to execute, an error message is displayed.
.PP
You can't nest :@ commands.
You can't run :@ commands from your .exrc file,
or any other :source file either.
Similarly, you can't run a :source command from within an @ command.
Hopefully, these restrictions will be lifted in a later version.
.NH 2
The Effect of Switching Files
.PP
When \*E first starts up, all cut buffers are empty.
When you switch to a different file
(via the :n or :e commands perhaps)
the 9 anonymous cut buffers are emptied again,
but the other 27 buffers ("a through "z, and ".) retain their text.
