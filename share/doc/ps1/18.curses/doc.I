.\" Copyright (c) 1980 The Regents of the University of California.
.\" All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\" 3. All advertising materials mentioning features or use of this software
.\"    must display the following acknowledgement:
.\"	This product includes software developed by the University of
.\"	California, Berkeley and its contributors.
.\" 4. Neither the name of the University nor the names of its contributors
.\"    may be used to endorse or promote products derived from this software
.\"    without specific prior written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"	@(#)doc.I	6.3 (Berkeley) 4/17/91
.\"
.Ds
.Fd addch ch \*m
char	ch;
.Fd waddch win\*,ch
WINDOW	*win;
char	ch;
.De
Add the character
.Vn ch
on the window
at the current \*y.
If the character is a newline
(\'\en\')
the line will be cleared to the end,
and the current \*y will be changed to the
beginning off the next line
if newline mapping is on,
or to the next line at the same x co-ordinate
if it is off.
A return
(\'\er\')
will move to the beginning of the line on the window.
Tabs
(\'\et\')
will be expanded into spaces
in the normal tabstop positions of
every eight characters.
\*(Es
.Ds
.Fd addstr str \*m
char	*str;
.Fd waddstr win\*,str
WINDOW	*win;
char	*str;
.De
Add the string pointed to by
.Vn str
on the window at the current \*y.
\*(Es
In this case, it will put on as much as it can.
.Ds
.Fd box win\*,vert\*,hor
WINDOW	*win;
char	vert\*,hor;
.De
.Pp
Draws a box around the window using
.Vn vert
as the character for drawing the vertical sides, and
.Vn hor
for drawing the horizontal lines.
If scrolling is not allowed,
and the window encompasses the lower right-hand corner of the terminal,
the corners are left blank to avoid a scroll.
.Ds
.Fd clear "" \*m
.Fd wclear win
WINDOW	*win;
.De
Resets the entire window to blanks.
If
.Vn win
is a screen,
this sets the clear flag,
which will cause a clear-screen sequence to be sent
on the next
.Fn refresh
call.
This also moves the current \*y
to (0\*,0).
.Ds
.Fd clearok scr\*,boolf \*m
WINDOW	*scr;
bool	boolf;
.De
Sets the clear flag for the screen
.Vn scr .
If
.Vn boolf
is TRUE,
this will force a clear-screen to be printed on the next
.Fn refresh ,
or stop it from doing so if
.Vn boolf
is FALSE.
This only works on screens,
and,
unlike
.Fn clear ,
does not alter the contents of the screen.
If
.Vn scr
is
.Vn curscr ,
the next
.Fn refresh
call will cause a clear-screen,
even if the window passed to
.Fn refresh
is not a screen.
.Ds
.Fd clrtobot "" \*m
.Fd wclrtobot win
WINDOW	*win;
.De
Wipes the window clear from the current \*y to the bottom.
This does not force a clear-screen sequence on the next refresh
under any circumstances.
\*(Nm
.Ds
.Fd clrtoeol "" \*m
.Fd wclrtoeol win
WINDOW	*win;
.De
Wipes the window clear from the current \*y to the end of the line.
\*(Nm
.Ds
.Fd delch
.Fd wdelch win
WINDOW	*win;
.De
Delete the character at the current \*y.
Each character after it on the line shifts to the left,
and the last character becomes blank.
.Ds
.Fd deleteln
.Fd wdeleteln win
WINDOW	*win;
.De
Delete the current line.
Every line below the current one will move up,
and the bottom line will become blank.
The current \*y will remain unchanged.
.Ds
.Fd erase "" \*m
.Fd werase win
WINDOW	*win;
.De
Erases the window to blanks without setting the clear flag.
This is analagous to
.Fn clear ,
except that it never causes a clear-screen sequence to be generated
on a
.Fn refresh .
\*(Nm
.Ds
.Fd flushok win\*,boolf \*m
WINDOW	*win;
bool	boolf;
.De
Normally,
.Fn refresh
.Fn fflush 's
.Vn stdout
when it is finished.
.Fn flushok
allows you to control this.
if
.Vn boolf
is TRUE
(\c
.i i.e. ,
non-zero)
it will do the
.Fn fflush ;
if it is FALSE.
it will not.
.Ds
.Fd idlok win\*,boolf
WINDOW	*win;
bool	boolf;
.De
Reserved for future use.
This will eventually signal to
.Fn refresh
that it is all right to use the insert and delete line sequences
when updating the window.
.Ds
.Fd insch c
char	c;
.Fd winsch win\*,c
WINDOW	*win;
char	c;
.De
Insert
.Vn c
at the current \*y
Each character after it shifts to the right,
and the last character disappears.
\*(Es
.Ds
.Fd insertln
.Fd winsertln win
WINDOW	*win;
.De
Insert a line above the current one.
Every line below the current line
will be shifted down,
and the bottom line will disappear.
The current line will become blank,
and the current \*y will remain unchanged.
.Ds
.Fd move y\*,x \*m
int	y\*,x;
.Fd wmove win\*,y\*,x
WINDOW	*win;
int	y\*,x;
.De
Change the current \*y of the window to
.Vn y\*,x ). (
\*(Es
.Ds
.Fd overlay win1\*,win2
WINDOW	*win1\*,*win2;
.De
Overlay
.Vn win1
on
.Vn win2 .
The contents of
.Vn win1 ,
insofar as they fit,
are placed on
.Vn win2
at their starting \*y.
This is done non-destructively,
i.e., blanks on
.Vn win1
leave the contents of the space on
.Vn win2
untouched.
.Ds
.Fd overwrite win1\*,win2
WINDOW	*win1\*,*win2;
.De
Overwrite
.Vn win1
on
.Vn win2 .
The contents of
.Vn win1 ,
insofar as they fit,
are placed on
.Vn win2
at their starting \*y.
This is done destructively,
.i i.e. ,
blanks on
.Vn win1
become blank on
.Vn win2 .
.Ds
.Fd printw fmt\*,arg1\*,arg2\*,...
char	*fmt;
.Fd wprintw win\*,fmt\*,arg1\*,arg2\*,...
WINDOW	*win;
char	*fmt;
.De
Performs a
.Fn printf
on the window starting at the current \*y.
It uses
.Fn addstr
to add the string on the window.
It is often advisable to use the field width options of
.Fn printf
to avoid leaving things on the window from earlier calls.
\*(Es
.Ds
.Fd refresh "" \*m
.Fd wrefresh win
WINDOW	*win;
.De
Synchronize the terminal screen with the desired window.
If the window is not a screen,
only that part covered by it is updated.
\*(Es
In this case, it will update whatever it can
without causing the scroll.
.sp
As a special case,
if
.Fn wrefresh
is called with the window
.Vn curscr
the screen is cleared
and repainted as it is currently.
This is very useful for allowing the redrawing of the screen
when the user has garbage dumped on his terminal.
.Ds
.Fd standout "" \*m
.Fd wstandout win
WINDOW	*win;
.Fd standend "" \*m
.Fd wstandend win
WINDOW	*win;
.De
Start and stop putting characters onto
.i win
in standout mode.
.Fn standout
causes any characters added to the window
to be put in standout mode on the terminal
(if it has that capability).
.Fn standend
stops this.
The sequences
.Vn SO
and
.Vn SE
(or
.Vn US
and
.Vn UE
if they are not defined)
are used (see Appendix A).
