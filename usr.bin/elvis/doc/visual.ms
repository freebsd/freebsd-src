.Go 2 "VISUAL MODE COMMANDS"
.PP
Most visual mode commands are one keystroke long.
The following table lists the operation performed by each keystroke,
and also denotes any options or arguments that it accepts.
Notes at the end of the table describe the notation used in this table.
.PP
In addition to the keys listed here, your keyboard's "arrow" keys
will be interpretted as the appropriate cursor movement commands.
The same goes for <PgUp> and <PgDn>, if your keyboard has them.
The <Insert> key will toggle between insert mode and replace mode.
There is a colon mode command (":map", to be described later)
which will allow you to define other keys, such as function keys.
.PP
A tip: visual command mode looks a lot like text input mode.
If you forget which mode you're in, just hit the <Esc> key.
If \*E beeps, then you're in visual command mode.
If \*E does not beep, then you were in input mode,
but by hitting <Esc> you will have switched to visual command mode.
So, one way or another, after <Esc> \*E will be ready for a command.
.LD
.ta 0.7i 1.3i
\s'+2'COMMAND	DESCRIPTION\s'-2'
	^A	Search for next occurence of word at cursor (MOVE)(EXT)
	^B	Move toward the top of the file by 1 screenful
	^C	--- (usually sends SIGINT, to interupt a command)
count	^D	Scroll down <count> lines (default 1/2 screen)
count	^E	Scroll up <count> lines
	^F	Move toward the bottom of the file by 1 screenful
	^G	Show file status, and the current line #
count	^H	Move left, like h (MOVE)
	^I	---
count	^J	Move down (MOVE)
	^K	---
	^L	Redraw the screen
count	^M	Move to the front of the next line (MOVE)
count	^N	Move down (MOVE)
	^O	---
count	^P	Move up (MOVE)
	^Q	--- (typically XON, which restarts screen updates)
	^R	Redraw the screen
	^S	--- (typically XOFF, which stops screen updates)
	^T	---
count	^U	Scroll up <count> lines (default 1/2 screen)
	^V	---
	^W	---
count	^X	Move to a physical column number on the screen (MOVE) (EXT)
count	^Y	Scroll down <count> lines
	^Z	--- (sometimes sends SIGSUSP, to suspend execution)
	ESC	---
	^\\	--- (usually sends SIGQUIT, which is ignored)
	^]	If the cursor is on a tag name, go to that tag
	^^	Switch to the previous file, like ":e #"
	^_	---
count	SPC	Move right,like l (MOVE)
	! \s'-2'mv\s'+2'	Run the selected lines thru an external filter program
	" \s'-2'key\s'+2'	Select which cut buffer to use next
count	# \s'-2'+\s'+2'	Increment a number (EDIT) (EXT)
	$	Move to the rear of the current line (MOVE)
count	%	Move to matching (){}[] or to a given % of file (MOVE) (EXT)
count	&	Repeat the previous ":s//" command here (EDIT)
	' \s'-2'key\s'+2'	Move to a marked line (MOVE)
count	(	Move backward <count> sentences (MOVE)
count	)	Move forward <count> sentences (MOVE)
	*	Go to the next error in the errlist (EXT)
count	+	Move to the front of the next line (MOVE)
count	,	Repeat the previous [fFtT] but in the other direction (MOVE)
count	-	Move to the front of the preceding line (MOVE)
count	.	Repeat the previous "edit" command
	/ \s'-2'text\s'+2'	Search forward for a given regular expression (MOVE)
	0	If not part of count, move to 1st char of this line (MOVE)
	1	Part of count
	2	Part of count
	3	Part of count
	4	Part of count
	5	Part of count
	6	Part of count
	7	Part of count
	8	Part of count
	9	Part of count
	: \s'-2'text\s'+2'	Run single EX cmd
count	;	Repeat the previous [fFtT] cmd (MOVE)
	< \s'-2'mv\s'+2'	Shift text left (EDIT)
	= \s'-2'mv\s'+2'	Reformat
	> \s'-2'mv\s'+2'	Shift text right (EDIT)
	? \s'-2'text\s'+2'	Search backward for a given regular expression (MOVE)
	@ \s'-2'key\s'+2'	Execute the contents of a cut-buffer as VI commands
count	A \s'-2'inp\s'+2'	Append at end of the line (EDIT)
count	B	Move back Word (MOVE)
	C \s'-2'inp\s'+2'	Change text from the cursor through the end of the line (EDIT)
	D	Delete text from the cursor through the end of the line (EDIT)
count	E	Move end of Word (MOVE)
count	F \s'-2'key\s'+2'	Move leftward to a given character (MOVE)
count	G	Move to line #<count> (default is the bottom line) (MOVE)
count	H	Move to home row (the line at the top of the screen)
count	I \s'-2'inp\s'+2'	Insert at the front of the line (after indents) (EDIT)
count	J	Join lines, to form one big line (EDIT)
	K	Look up keyword (EXT)
count	L	Move to last row (the line at the bottom of the screen)
	M	Move to middle row
	N	Repeat previous search, but in the opposite direction (MOVE)
count	O \s'-2'inp\s'+2'	Open up a new line above the current line (EDIT)
	P	Paste text before the cursor (EDIT)
	Q	Quit to EX mode
	R \s'-2'inp\s'+2'	Overtype (EDIT)
count	S \s'-2'inp\s'+2'	Change lines, like <count>cc
count	T \s'-2'key\s'+2'	Move leftward *almost* to a given character (MOVE)
	U	Undo all recent changes to the current line
	V	Start marking lines for c/d/y/</>/!/\\ (EXT)
count	W	Move forward <count> Words (MOVE)
count	X	Delete the character(s) to the left of the cursor (EDIT)
count	Y	Yank text line(s) (copy them into a cut buffer)
	Z Z	Save the file & exit
	[ [	Move back 1 section (MOVE)
	\\ \s'-2'mv\s'+2'	Pop-up menu for modifying text (EXT)
	] ]	Move forward 1 section (MOVE)
	^	Move to the front of the current line (after indent) (MOVE)
count	_	Move to the current line
	` \s'-2'key\s'+2'	Move to a marked character (MOVE)
count	a \s'-2'inp\s'+2'	Insert text after the cursor (EDIT)
count	b	Move back <count> words (MOVE)
	c \s'-2'mv\s'+2'	Change text (EDIT)
	d \s'-2'mv\s'+2'	Delete text (EDIT)
count	e	Move forward to the end of the current word (MOVE)
count	f \s'-2'key\s'+2'	Move rightward to a given character (MOVE)
	g	---
count	h	Move left (MOVE)
count	i \s'-2'inp\s'+2'	Insert text at the cursor (EDIT)
count	j	Move down (MOVE)
count	k	Move up (MOVE)
count	l	Move right (MOVE)
	m \s'-2'key\s'+2'	Mark a line or character
	n	Repeat the previous search (MOVE)
count	o \s'-2'inp\s'+2'	Open a new line below the current line (EDIT)
	p	Paste text after the cursor (EDIT)
	q	---
count	r \s'-2'key\s'+2'	Replace <count> chars by a given character (EDIT)
count	s \s'-2'inp\s'+2'	Replace <count> chars with text from the user (EDIT)
count	t \s'-2'key\s'+2'	Move rightward *almost* to a given character (MOVE)
	u	Undo the previous edit command
	v	Start marking characters for c/d/y/</>/!/\\ (EXT)
count	w	Move forward <count> words (MOVE)
count	x	Delete the character that the cursor's on (EDIT)
	y \s'-2'mv\s'+2'	Yank text (copy it into a cut buffer)
	z \s'-2'key\s'+2'	Scroll current line to the screen's +=top -=bottom .=middle
count	{	Move back <count> paragraphs (MOVE)
count	|	Move to column <count> (the leftmost column is 1)
count	}	Move forward <count> paragraphs (MOVE)
count	~	Switch a character between uppercase & lowercase (EDIT)
	DEL	--- (usually mapped to shift-X, so it deletes one character)
.DE
.IP count 0.7i
Many commands may be preceded by a count.  This is a sequence of digits
representing a decimal number.  For most commands that use a count,
the command is repeated <count> times.  The count is always optional,
and usually defaults to 1.
.IP key 0.7i
Some commands require two keystrokes.  The first key always determines
which command is to be executed.  The second key is used as a parameter
to the command.
.IP mv 0.7i
Some commands (! < > c d y \\ =) operate on text between the cursor and some
other position.
There are three ways that you can specifify that other position.
.IP "" 0.7i
The first way is to follow the command keystroke with a movement command.
For example, "dw" deletes a single word.
"d3w" and "3dw" both delete three words.
.IP "" 0.7i
The second way is to type the command keystroke twice.
This causes whole lines to be acted upon.
For example, ">>" indents the current line.
"3>>" indents the current line and the following two lines.
.IP "" 0.7i
The last way is to move the cursor to one end of the text,
type 'v' or 'V' to start marking,
move the cursor to the other end,
and then type the desired command key.
.IP inp 0.7i
Many commands allow the user to interactively enter text.
See the discussion of "input mode" in the following section.
.IP (EXT) 0.7i
These commands are extensions -- the real vi doesn't have them.
.IP (EDIT) 0.7i
These commands affect text, and may be repeated by the "." command.
.IP (MOVE) 0.7i
These commands move the cursor, and may be used to specify the extent
of a member of the "mv" class of commands.
.NH 2
Input Mode
.PP
You can't type text into your file directly from visual command mode.
Instead, you must first give a command which will put you into input mode.
The commands to do this are A/C/I/O/R/S/a/i/o/s.
.PP
The S/s/C/c commands temporarily place a $ at the end of the text that
they are going to change.
.PP
In input mode, all keystrokes are inserted into the text at the
cursor's position, except for the following:
.ID
^A	insert a copy of the last input text
^D	delete one indent character
^H	(backspace) erase the character before the cursor
^L	redraw the screen
^M	(carriage return) insert a newline (^J, linefeed)
^O	execute next key as a visual command (limited!)
^P	insert the contents of the cut buffer
^R	redraw the screen, like ^L
^T	insert an indent character
^U	backspace to the beginning of the line
^V	insert the following keystroke, even if special
^W	backspace to the beginning of the current word
^Z^Z	write the file & exit \*E
^[	(ESCape) exit from input mode, back to command mode
.DE
.PP
Also, on some systems, ^S may stop output, ^Q may restart output,
and ^C may interupt execution.
^@ (the NUL character) cannot be inserted.
.PP
The R visual command puts you in overtype mode,
which is a slightly different form of input mode.
In overtype mode, each time you insert a character,
one of the old characters is deleted from the file.
.NH 2
Arrow keys in Input Mode
.PP
The arrow keys can be used to move the cursor in input mode.
(This is an extension; the real Vi doesn't support arrow keys in input mode.)
The <PgUp>, <PgDn>, <Home>, and <End> keys work in input mode, too.
The <Delete> key deletes a single character in input mode.
The <Insert> key toggles between input mode and replace mode.
.PP
The best thing about allowing arrow keys to work in input mode is that
as long as you're in input mode,
\*E seems to have a fairly ordinary user interface.
With most other text editors, you are always in either insert mode or
replace mode, and you can use the arrow keys at any time to move the cursor.
Now, \*E can act like that, too.
In fact, with the new "inputmode" option and the "control-Z control-Z" input
command, you may never have to go into visual command mode for simple edit
sessions.
.NH 2
Digraphs
.PP
\*E supports digraphs as a way to enter non-ASCII characters.
A digraph is a character which is composed of two other characters.
For example, an apostrophe and the letter i could be defined as a digraph
which is to be stored & displayed as an accented i.
.PP
There is no single standard for extended ASCII character sets.
\*E can be compiled to fill the digraph with values appropriate for
either the IBM PC character set, or the LATIN-1 character set used by
X windows, or neither.
(See the discussions of -DCS_IBMPC and -DCS_LATIN1 in the CFLAGS section
of this manual.)
You can view or edit the digraph table via the ":digraph" colon command.
.PP
Digraphs will not be recognized until you've entered ":set digraph".
.PP
To actually use a digraph
type the first character, then hit <Backspace>, and then type the
second character.
\*E will then substitute the non-ASCII character in their place.
.NH 2
Abbreviations
.PP
\*E can expand abbreviations for you.
You define an abbreviation with the :abbr command,
and then whenever you type in the abbreviated form while in input mode,
\*E will immediately replace it with the long form.
COBOL programmers should find this useful. :-)
.PP
\*E doesn't perform the substitution until you type a non-alphanumeric
character to mark the end of the word.
If you type a control-V before that non-alphanumeric character, then
\*E will not perform the substitution.
.NH 2
Auto-Indent
.PP
With the ":set autoindent" option turned on,
\*E will automatically insert leading whitespace at the beginning of each
new line that you type in.
The leading whitespace is copied from the preceding line.
.PP
To add more leading whitespace, type control-T.
To remove some whitespace, type control-D.
.PP
If you ":set noautotab", then the whitespace generated by control-T will
always consist of spaces -- never tabs.
Some people seem to prefer this.
.PP
\*E' autoindent mode isn't 100% compatible with vi's.
In \*E, 0^D and ^^D don't work,
^U can wipeout all indentation, 
and sometimes \*E will use a different amount of indentation than vi would.
