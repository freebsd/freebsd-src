.Go 8 "INTERNAL"
.PP
You don't need to know the material in this section to use \*E.
You only need it if you intend to modify \*E.
.PP
You should also check out the CFLAGS, TERMCAP, ENVIRONMENT VARIABLES,
VERSIONS, and QUIESTIONS & ANSWERS sections of this manual.
.NH 2
The temporary file
.PP
The temporary file is divided into blocks of 1024 bytes each.
The functions in "blk.c" maintain a cache of the five most recently used blocks,
to minimize file I/O.
.PP
When \*E starts up, the file is copied into the temporary file
by the function \fBtmpstart()\fR in "tmp.c".
Small amounts of extra space are inserted into the temporary file to
insure that no text lines cross block boundaries.
This speeds up processing and simplifies storage management.
The extra space is filled with NUL characters.
the input file must not contain any NULs, to avoid confusion.
This also limits lines to a length of 1023 characters or less.
.PP
The data blocks aren't necessarily stored in sequence.
For example, it is entirely possible that the data block containing
the first lines of text will be stored after the block containing the
last lines of text.
.PP
In RAM, \*E maintains two lists: one that describes the "proper"
order of the disk blocks, and another that records the line number of
the last line in each block.
When \*E needs to fetch a given line of text, it uses these tables
to locate the data block which contains that line.
.PP
Before each change is made to the file, these lists are copied.
The copies can be used to "undo" the change.
Also, the first list
-- the one that lists the data blocks in their proper order --
is written to the first data block of the temp file.
This list can be used during file recovery.
.PP
When blocks are altered, they are rewritten to a \fIdifferent\fR block in the file,
and the order list is updated accordingly.
The original block is left intact, so that "undo" can be performed easily.
\*E will eventually reclaim the original block, when it is no longer needed.
.NH 2
Implementation of Editing
.PP
There are three basic operations which affect text:
.ID
\(bu delete text	- delete(from, to)
\(bu add text	- add(at, text)
\(bu yank text	- cut(from, to)
.DE
.PP
To yank text, all text between two text positions is copied into a cut buffer.
The original text is not changed.
To copy the text into a cut buffer,
you need only remember which physical blocks that contain the cut text,
the offset into the first block of the start of the cut,
the offset into the last block of the end of the cut,
and what kind of cut it was.
(Cuts may be either character cuts or line cuts;
the kind of a cut affects the way it is later "put".)
Yanking is implemented in the function \fBcut()\fR,
and pasting is implemented in the function \fBpaste()\fR.
These functions are defined in "cut.c".
.PP
To delete text, you must modify the first and last blocks, and
remove any reference to the intervening blocks in the header's list.
The text to be deleted is specified by two marks.
This is implemented in the function \fBdelete()\fR.
.PP
To add text, you must specify
the text to insert (as a NUL-terminated string)
and the place to insert it (as a mark).
The block into which the text is to be inserted may need to be split into
as many as four blocks, with new intervening blocks needed as well...
or it could be as simple as modifying a single block.
This is implemented in the function \fBadd()\fR.
.PP
There is also a \fBchange()\fR function,
which generally just calls delete() and add().
For the special case where a single character is being replaced by another
single character, though, change() will optimize things somewhat.
The add(), delete(), and change() functions are all defined in "modify.c".
.PP
The \fBinput()\fR function reads text from a user and inserts it into the file.
It makes heavy use of the add(), delete(), and change() functions.
It inserts characters one at a time, as they are typed.
.PP
When text is modified, an internal file-revision counter, called \fBchanges\fR,
is incremented.
This counter is used to detect when certain caches are out of date.
(The "changes" counter is also incremented when we switch to a different file,
and also in one or two similar situations -- all related to invalidating caches.)
.NH 2
Marks and the Cursor
.PP
Marks are places within the text.
They are represented internally as 32-bit values which are split
into two bitfields:
a line number and a character index.
Line numbers start with 1, and character indexes start with 0.
Lines can be up to 1023 characters long, so the character index is 10 bits
wide and the line number fills the remaining 22 bits in the long int.
.PP
Since line numbers start with 1,
it is impossible for a valid mark to have a value of 0L.
0L is therefore used to represent unset marks.
.PP
When you do the "delete text" change, any marks that were part of
the deleted text are unset, and any marks that were set to points
after it are adjusted.
Marks are adjusted similarly after new text is inserted.
.PP
The cursor is represented as a mark.
.NH 2
Colon Command Interpretation
.PP
Colon commands are parsed, and the command name is looked up in an array
of structures which also contain a pointer to the function that implements
the command, and a description of the arguments that the command can take.
If the command is recognized and its arguments are legal,
then the function is called.
.PP
Each function performs its task; this may cause the cursor to be
moved to a different line, or whatever.
.NH 2
Screen Control
.PP
In input mode or visual command mode,
the screen is redrawn by a function called \fBredraw()\fR.
This function is called in the getkey() function before each keystroke is
read in, if necessary.
.PP
Redraw() writes to the screen via a package which looks like the "curses"
library, but isn't.
It is actually much simpler.
Most curses operations are implemented as macros which copy characters
into a large I/O buffer, which is then written with a single large
write() call as part of the refresh() operation.
.PP
(Note: Under MS-DOS, the pseudo-curses macros check to see whether you're
using the pcbios interface.  If you are, then the macros call functions
in "pc.c" to implement screen updates.)
.PP
The low-level functions which modify text (namely add(), delete(), and change())
supply redraw() with clues to help redraw() decide which parts of the
screen must be redrawn.
The clues are given via a function called \fBredrawrange()\fR.
.PP
Most EX commands use the pseudo-curses package to perform their output,
like redraw().
.PP
There is also a function called \fBmsg()\fR which uses the same syntax as printf().
In EX mode, msg() writes message to the screen and automatically adds a
newline.
In VI mode, msg() writes the message on the bottom line of the screen
with the "standout" character attribute turned on.
.NH 2
Options
.PP
For each option available through the ":set" command,
\*E contains a character array variable, named "o_\fIoption\fR".
For example, the "lines" option uses a variable called "o_lines".
.PP
For boolean options, the array has a dimension of 1.
The first (and only) character of the array will be NUL if the
variable's value is FALSE, and some other value if it is TRUE.
To check the value, just by dereference the array name,
as in "if (*o_autoindent)".
.PP
For number options, the array has a dimension of 3.
The array is treated as three unsigned one-byte integers.
The first byte is the current value of the option.
The second and third bytes are the lower and upper bounds of that
option.
.PP
For string options, the array usually has a dimension of about 60
but this may vary.
The option's value is stored as a normal NUL-terminated string.
.PP
All of the options are declared in "opts.c".
Most are initialized to their default values;
the \fBinitopts()\fR function is used to perform any environment-specific
initialization.
.NH 2
Portability
.PP
To improve portability, \*E collects as many of the system-dependent
definitions as possible into the "config.h" file.
This file begins with some preprocessor instructions which attempt to
determine which compiler and operating system you have.
After that, it conditionally defines some macros and constants for your system.
.PP
One of the more significant macros is \fBttyread()\fR.
This macro is used to read raw characters from the keyboard, possibly
with timeout.
For UNIX systems, this basically reads bytes from stdin.
For MSDOS, TOS, and OS9, ttyread() is a function defined in curses.c.
There is also a \fBttywrite()\fR macro.
.PP
The \fBtread()\fR and \fBtwrite()\fR macros are versions of read() and write() that are
used for text files.
On UNIX systems, these are equivelent to read() and write().
On MS-DOS, these are also equivelent to read() and write(),
since DOS libraries are generally clever enough to convert newline characters
automatically.
For Atari TOS, though, the MWC library is too stupid to do this,
so we had to do the conversion explicitly.
.PP
Other macros may substitute index() for strchr(), or bcopy() for memcpy(),
or map the "void" data type to "int", or whatever.
.PP
The file "tinytcap.c" contains a set of functions that emulate the termcap
library for a small set of terminal types.
The terminal-specific info is hard-coded into this file.
It is only used for systems that don't support real termcap.
Another alternative for screen control can be seen in
the "curses.h" and "pc.c" files.
Here, macros named VOIDBIOS and CHECKBIOS are used to indirectly call
functions which perform low-level screen manipulation via BIOS calls.
.PP
The stat() function must be able to come up with UNIX-style major/minor/inode
numbers that uniquely identify a file or directory.
.PP
Please try to keep you changes localized,
and wrap them in #if/#endif pairs,
so that \*E can still be compiled on other systems.
And PLEASE let me know about it, so I can incorporate your changes into
my latest-and-greatest version of \*E.
