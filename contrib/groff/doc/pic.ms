.\" For tolerably obvious reason, this needs to be processed through PIC.
.\" It also needs to be processed through TBL and EQN.  Use "groff -p -e -t".
.\" There is no hope that this will ever look right under nroff.
.\"
.\" Comments beginning with %% are cut lines so portions of this
.\" document can be automatically extracted.  %%TUTORIAL%% begins the
.\" tutorial part; %%REFERENCE%% the reference part.  %%POSTLUDE%% the
.\" bibliography and end matter after the reference part.  
.\"
.\" This document was written for free use and redistribution by
.\" Eric S. Raymond <esr@thyrsus.com> in August 1995.
.\"
.\"	$Id: pic.ms,v 1.19 2002/09/20 12:14:56 wlemb Exp $	
.\"
.\" Set a proper TeX
.ie t .ds tx T\h'-.1667m'\v'.224m'E\v'-.224m'\h'-.125m'X
.el .ds tx TeX
.\"
.de CE\"	Centered caption for figure.  Assumes previous .KS
.ce 1
Figure \\n(H1-\\$1
.sp 1
.KE
..
.\" Definitions end here
.TL
Making Pictures With GNU PIC
.AU
Eric S. Raymond
.AI
\[la]\fIesr@snark.thyrsus.com\fP\[ra]
.AB
The \fBpic\fP language is a \fBtroff\fP extension that makes it easy
to create and alter box-and-arrow diagrams of the kind frequently used
in technical papers and textbooks.  This paper is both an introduction
to and reference for \fIgpic\/\fP(1), the implementation distributed by
the Free Software Foundation for use with \fIgroff\/\fP(1).
.AE
.\"%%TUTORIAL%%
.NH 1
Introduction to PIC
.NH 2
Why PIC?
.PP
The \fBpic\fP language provides an easy way to write procedural
box-and-arrow diagrams to be included in \fBtroff\fP documents.  The
language is sufficiently flexible to be quite useful for state charts,
Petri-net diagrams, flow charts, simple circuit schematics, jumper
layouts, and other kinds of illustration involving repetitive uses of
simple geometric forms and splines.  Because these descriptions are
procedural and object-based, they are both compact and easy to modify.
.PP
The \fIgpic\/\fP(1) implementation of \fBpic\fP is distributed by the
Free Software Foundation for use with their \fIgroff\/\fP(1)
implementation of \fBtroff\fP.  Because both implementations are
widely available in source form for free, they are good bets for
writing very portable documentation.
.NH 2
PIC Versions
.PP
The original 1984 pre-\fIditroff\/\fP(1) version of \fBpic\fP is long
obsolete.  The rewritten 1991 version is still available as part of
the Documenter's Work Bench module of System V.
.PP
Where differences between Documenter's Work Bench (1991) \fBpic\fP and GNU
\fBpic\fP need to be described, original \fBpic\fP is referred to as
\[lq]DWB pic\[rq].  Details on the history of the program are given at the
end of this document.
.PP
In this document, the \fIgpic\/\fP(1) extensions will be marked as such.
.NH 1
Invoking PIC
.PP
Every \fBpic\fP description is a little program, which gets compiled
by \fIpic\/\fP(1) into \fIgtroff\/\fP(1) macros.  Programs that process or
display \fIgtroff\/\fP(1) output need not know or care that parts of the
image began life as \fBpic\fP descriptions.
.PP
The \fIpic\/\fP(1) program tries to translate anything between \fB.PS\fP
and \fB.PE\fP markers, and passes through everything else.  The normal
definitions of \fB.PS\fP and \fB.PE\fP in the \fIms\fP macro package
and elsewhere have also the side-effect of centering the \fBpic\fP output
on the page.
.NH 2
PIC Error Messages
.PP
If you make a \fBpic\fP syntax error, \fIgpic\/\fP(1) will issue an
error message in the standard \fIgcc\/\fP(1)-like syntax.  A typical
error message looks like this,
.KS
.DS
.CW
pic:pic.ms:<nnn>: parse error before `<token>'
pic:pic.ms:<nnn>: giving up on this picture
.DE
.R
.KE
.LP
where \[la]nnn\[ra] is a line number, and \[la]token\[ra] is a token near (usually
just after) the error location.
.NH 1
Basic PIC Concepts
.PP
Pictures are described procedurally, as collections of objects
connected by motions.  Normally, \fBpic\fP tries to string together
objects left-to-right in the sequence they are described, joining them
at visually natural points.  Here is an example illustrating the
flow of data in \fBpic\fP processing:
.KS
.PS
ellipse "document";
arrow;
box width 0.6 "\fIgpic\/\fP(1)"
arrow;
box width 1.1 "\fIgtbl\/\fP(1) or \fIgeqn\/\fP(1)" "(optional)" dashed;
arrow;
box width 0.6 "\fIgtroff\/\fP(1)";
arrow;
ellipse "PostScript"
.PE
.CE "1: Flow of \fBpic\fP data"
.PP
This was produced from the following \fBpic\fP program:
.KS
.DS
.ps -1
.vs -1
.CW
\&.PS
ellipse "document";
arrow;
box width 0.6 "\efIpic\e/\efP(1)"
arrow;
box width 1.1 "\efIgtbl\e/\efP(1) or \efIgeqn\e/\efP(1)" "(optional)" dashed;
arrow;
box width 0.6 "\efIgtroff\e/\efP(1)";
arrow;
ellipse "PostScript"
\&.PE
.DE
.R
.KE
.LP
This little program illustrates several \fBpic\fP basics.  Firstly, we
see how to invoke three object types; ellipses, arrows, and boxes.  We
see how to declare text lines to go within an object (and that text
can have font changes in it).  We see how to change the line style of
an object from solid to dashed.  And we see that a box can be made
wider than its default size to accommodate more text (we'll discuss
this facility in detail in the next section).
.PP
We also get to see \fBpic\fP's simple syntax.  Statements are ended by
newlines or semicolons.  String quotes are required around all text
arguments, whether or not they contain spaces.  In general, the order
of command arguments and modifiers like \[lq]width 1.2\[rq] or
\[lq]dashed\[rq] doesn't matter, except that the order of text arguments
is significant.
.PP
Here are all but one of the basic \fBpic\fP objects at their default sizes:
.KS
.PS
box "box";
move;
line "line" "";
move;
arrow "arrow" "";
move;
circle "circle";
move;
ellipse "ellipse";
move;
arc; down; move; "arc"
.PE
.CE "2: Basic \fBpic\fP objects"
.PP
The missing simple object type is a \fIspline\fP.  There is also a way
to collect objects into \fIblock composites\fP which allows you to
treat the whole group as a single object (resembling a box) for many
purposes.  We'll describe both of these later on.
.PP
The box, ellipse, circle, and block composite objects are \fIclosed\/\fR;
lines, arrows, arcs and splines are \fIopen\fP.  This distinction
will often be important in explaining command modifiers.
.PP
Figure \n[H1]-2 was produced by the following \fBpic\fP program,
which introduces some more basic concepts:
.KS
.DS
.CW
\&.PS
box "box";
move;
line "line" "";
move;
arrow "arrow" "";
move;
circle "circle";
move;
ellipse "ellipse";
move;
arc; down; move; "arc"
\&.PE
.DE
.ft R
.KE
.PP
The first thing to notice is the \fImove\fP command, which moves a
default distance (1/2 inch) in the current movement direction.
.PP
Secondly, see how we can also decorate lines and arrows with text.
The line and arrow commands each take two arguments here, specifying
text to go above and below the object.  If you wonder why one argument
would not do, contemplate the output of \fBarrow "ow!"\fP:
.KS
.PS
arrow "ow!"
.PE
.CE "3: Text centered on an arrow"
.PP
When a command takes one text string, \fBpic\fP tries to place it at
the object's geometric center.  As you add more strings, \fBpic\fP
treats them as a vertical block to be centered.  The program
.KS
.DS
.CW
line "1";
line "1" "2";
line "1" "2" "3";
line "1" "2" "3" "4";
line "1" "2" "3" "4" "5";
.DE
.ft R
.KE
.LP
for example, gives you this:
.KS
.sp 2
.PS
line "1";
line "1" "2";
line "1" "2" "3";
line "1" "2" "3" "4";
line "1" "2" "3" "4" "5";
.PE
.sp 2
.CE "4: Effects of multiple text arguments"
.PP
The last line of Figure 3.2's program, `\fBarc; down; move;
"arc"\fP', describing the captioned arc, introduces several new ideas.
Firstly, we see how to change the direction in which objects are
joined.  Had we written \fBarc; move; "arc"\fP,
omitting \fBdown\fP the caption would have been joined to the top
of the arc, like this:
.KS
.PS
arc; move; "arc";
.PE
.CE "5: Result of \fBarc; move; \"arc\"\fP"
.PP
This is because drawing an arc changes the default direction to the
one its exit end points at.  To reinforce this point, consider:
.KS
.PS
arc cw; move; "arc";
.PE
.CE "6: Result of \fBarc cw; move; \"arc\"\fP"
.PP
All we've done differently here is specify \[lq]cw\[rq] for a clockwise arc
(\[lq]ccw\[rq] specifies counter-clockwise direction).
Observe how it changes the default direction to down, rather than up.
.PP
Another good way to see this via with the following program:
.KS
.DS
.CW
line; arc; arc cw; line
.DE
.ft R
.KE
.LP
which yields:
.KS
.PS
line; arc; arc cw; line;
.PE
.CE "7: Result of \fBline; arc; arc cw; line\fP"
.LP
Notice that we did not have to specify \[lq]up\[rq] for the second arc to be
joined to the end of the first.
.PP
Finally, observe that a string, alone, is treated as text to be
surrounded by an invisible box of a size either specified by width
and height attributes or by the defaults \fBtextwid\fR and
\fBtextht\fR.  Both are initially zero (because we don't know the
default font size).
.NH 1 
Sizes and Spacing
.PP
Sizes are specified in inches.  If you don't like inches, it's
possible to set a global style variable \fBscale\fP that changes the
unit.  Setting \fBscale = 2.54\fP will effectively change the internal
unit to centimeters (all other size variable values will be scaled
correspondingly).
.NH 2
Default Sizes of Objects
.PP
Here are the default sizes for \fBpic\fP objects:
.TS H
center, tab(@), linesize(2);
lb | lb
l | l.
.sp 2p
Object@Default Size
.sp 2p
_
.sp 2p
.TH
box@0.75" wide by 0.5" high
circle@0.5" diameter
ellipse@0.75" wide by 0.5" high
arc@0.5" radius
line@0.5" long
arrow@0.5" long
.sp 5p
_
.TE
.PP
The simplest way to think about these defaults is that they make the
other basic objects fit snugly into a default-sized box.
.NH 2
Objects Do Not Stretch!
.PP
Text is rendered in the current font with normal troff line spacing.
Boxes, circles, and ellipses do \fInot\fP automatically resize to fit
enclosed text.  Thus, if you say \fBbox "this text far too long for a
default box"\fP you'll get this:
.KS
.PS
box "this text is far too long for a default box"
.PE
.CE "1: Boxes do not automatically resize"
.LP
which is probably not the effect you want.
.NH 2
Resizing Boxes
.PP
To change the box size, you can specify a box width with the \[lq]width\[rq]
modifier:
.KS
.PS
box width 3 "this text is far too long for a default box"
.PE
.CE "2: Result of \fBbox width 3 \"text far too long\"\fP"
.PP
This modifier takes a dimension in inches.  There is also a \[lq]height\[rq]
modifier that will change a box's height.  The \fBwidth\fP keyword may
be abbreviated to \fBwid\fP; the \fBheight\fP keyword to \fBht\fP.
.NH 2
Resizing Other Object Types
.PP
To change the size of a circle, give it a \fBrad[ius]\fP or
\fBdiam[eter]\fP modifier; this changes the radius or diameter of the
circle, according to the numeric argument that follows.
.KS
.PS
{circle rad 0.1; move down 0.2 from last circle .s; "0.1"};
move; circle rad 0.2 "0.2"; move; circle rad 0.3 "0.3";
.PE
.CE "3: Circles with increasing radii"
.PP
The \fBmove\fP command can also take a dimension, which just tells
it how many inches to move in the current direction.
.PP
Ellipses are sized to fit in the rectangular box defined by their
axes, and can be resized with \fBwidth\fP and \fBheight\fP like boxes.
.PP
You can also change the radius of curvature of an arc with \fBrad[ius]\fP
(which specifies the radius of the circle of which the arc is a segment).
Larger values yield flatter arcs.
.KS
.PS
{arc rad 0.1; move down 0.3 from last arc .center; "0.1"};
move;
{arc rad 0.2; move down 0.4 from last arc .center; "0.2"};
move;
{arc rad 0.3; move down 0.5 from last arc .center; "0.3"};
.PE
.CE "4: \fBarc rad\fP with increasing radii"
.PP
Observe that because an arc is defined as a quarter circle, increasing
the radius also increases the size of the arc's bounding box.
.NH 2
The `same' Keyword
.PP
In place of a dimension specification, you can use the keyword
\fBsame\fR.  This gives the object the same size as the previous one
of its type.  As an example, the program
.KS
.DS
.CW
\&.PS
box; box wid 1 ht 1; box same; box
\&.PE
.R
.DE
.KE
.LP
gives you
.KS
.PS
box; box wid 1 ht 1; box same; box
.PE
.CE "5: The \fBsame\fP keyword"

.NH 1
Generalized Lines and Splines
.NH 2
Diagonal Lines
.PP
It is possible to specify diagonal lines or arrows by adding multiple \fBup\fP,
\fBdown\fP, \fBleft\fP, and \fBright\fP modifiers to the line object.
Any of these can have a multiplier.  To understand the effects, think
of the drawing area as being gridded with standard-sized boxes.
.KS
.PS
# Draw a demonstration up left arrow with grid box overlay
define gridarrow
{
	move right 0.1
	[
		{arrow up left $1;}
		box wid 0.5 ht 0.5 dotted with .nw at last arrow .end;
		for i = 2 to ($1 / 0.5) do {
			box wid 0.5 ht 0.5 dotted with .sw at last box .se;
		}
		move down from last arrow .center;
		[
			if ( $1 == boxht ) then {
				"\fBline up left\fP"
			} else {
				sprintf("\fBarrow up left %g\fP", $1)
			}
		]
	] 
	move right 0.1 from last [] .e;
}
gridarrow(0.5);
gridarrow(1);
gridarrow(1.5);
gridarrow(2);
undef gridarrow
.PE
.CE "1: Diagonal arrows (dotted boxes show the implied 0.5-inch grid)"
.NH 2
Multi-Segment Line Objects
.PP
A \[lq]line\[rq] or \[lq]arrow\[rq] object may actually be a path
consisting of any number of segments of varying lengths and directions.
To describe a path, connect several line or arrow commands with the
keyword \fBthen\fP.
.KS
.PS
define zigzag { $1 right 1 then down .5 left 1 then right 1 }
zigzag(line); 
.PE
.CE "2: \fBline right 1 then down .5 left 1 then right 1\fP"
.NH 2
Spline Objects
.PP
If you start a path with the \fBspline\fP keyword, the path vertices
are treated as control points for a spline curve fit.
.KS
.PS
[zigzag(spline);]
move down 0.2 from last [] .s;
"The spline curve..."
move right from last [] .e;
[
	zigzag(line dashed);
	spline from start of last line right 1 then down .5 left 1 then right 1;
	"1" at last spline .start + (-0.1, 0);
	"2" at last spline .start + (1.1, 0);
	"3" at last spline .end + (-1.1, 0);
	"4" at last spline .end + (0.1, 0);
]
move down 0.2 from last [] .s;
"...with tangents displayed"
undef zigzag;
.PE
.CE "3: \fBspline right 1 then down .5 left 1 then right 1\fP"
.PP
You can describe many natural-looking but irregular curves this
way.  For example: 
.KS
.PS
[spline right then up then left then down ->;]
move down 0.2 from last [] .s;
["\fBspline right then up then left then down ->;\fP"]
move right 3 from last [] .se;
"\fBspline left then up right then down right ->;\fP"
move up 0.2;
[spline left then up right then down right ->;]
.PE
.CE "4: Two more spline examples"
.LP
Note the arrow decorations.  Arrowheads can be applied naturally to
any path-based object, line or spline.  We'll see how in the next
section.
.NH 1
Decorating Objects
.NH 2
Dashed Objects
.PP
We've already seen that the modifier \fBdashed\fP can change the line
style of an object from solid to dashed.  GNU \fBgpic\fP permits you to
dot or dash ellipses, circles, and arcs (and splines in \*[tx] mode
only); some versions of DWB may only permit dashing of lines and
boxes.  It's possible to change the dash interval by specifying a
number after the modifier.
.PP
.KS
.PS
box dashed "default";
move;
box dashed 0.05 "0.05";
move;
box dashed 0.1 "0.1";
move;
box dashed 0.15 "0.15";
move;
box dashed 0.2 "0.2";
.PE
.CE "1: Dashed objects"
.NH 2
Dotted Objects
.PP
Another available qualifier is \fBdotted\fP.  GNU \fBgpic\fP permits
you to dot or dash ellipses, circles, and arcs (and splines in \*[tx]
mode only); some versions of DWB may only permit dashing of lines and
boxes.  It too can be suffixed with a number to specify the interval
between dots:
.KS
.PS
box dotted "default";
move;
box dotted 0.05 "0.05";
move;
box dotted 0.1 "0.1";
move;
box dotted 0.15 "0.15";
move;
box dotted 0.2 "0.2";
.PE
.CE "2: Dotted objects"
.NH 2
Rounding Box Corners
.PP
It is also possible, in GNU \fBgpic\fP only, to modify a box so it has
rounded corners:
.KS
.PS
box rad 0.05 "rad 0.05";
move;
box rad 0.1 "rad 0.1";
move;
box rad 0.15 "rad 0.15"; 
move;
box rad 0.2 "rad 0.2"; 
move;
box rad 0.25 "rad 0.25"; 
.PE
.CE "3: \fBbox rad\fP with increasing radius values"
.PP
Radius values higher than half the minimum box dimension are silently
truncated to that value.
.NH 2
Arrowheads
.PP
Lines and arcs can be decorated as well.  Any line or arc (and any
spline as well) can be decorated with arrowheads by adding one or more
as modifiers:
.KS
.PS
line <- ->
.PE
.CE "4: Double-headed line made with  \fBline <- ->\fP"
.PP
In fact, the \fBarrow\fP command is just shorthand for \fBline ->\fP.  And
there is a double-head modifier <->, so the figure above could have been made
with \fBline <->\fP.
.PP
Arrowheads have a \fBwidth\fP attribute, the distance across the rear;
and a \fBheight\fP attribute, the length of the arrowhead along the shaft.
.PP
Arrowhead style is controlled by the style variable \fBarrowhead\fP.
The DWB and GNU versions interpret it differently.  DWB defaults to
open arrowheads and an \fBarrowhead\fP value of\~2; the Kernighan
paper says a value of\~7 will make solid arrowheads.  GNU \fBgpic\fP
defaults to solid arrowheads and an \fBarrowhead\fP value of\~1; a
value of\~0 will produce open arrowheads.  Note that solid arrowheads are
always filled with the current outline color.
.NH 2
Line Thickness
.PP
It's also possible to change the line thickness of an object (this is
a GNU extension, DWB \fBpic\fP doesn't support it).  
The default thickness of the lines used to draw objects is controlled by the
.B linethick
variable.
This gives the thickness of lines in points.
A negative value means use the default thickness:
in \*[tx] output mode, this means use a thickness of 8 milliinches;
in \*[tx] output mode with the
.B -c
option, this means use the line thickness specified by
.B .ps
lines; in troff output mode, this means use a thickness proportional
to the pointsize.  A zero value means draw the thinnest possible line
supported by the output device.  Initially it has a value of -1.
There is also a \fBthickness\fP attribute (which can be abbreviated to
\fBthick\fP).  For example, \fBcircle thickness 1.5\fP would draw a
circle using a line with a thickness of 1.5 points.  The thickness of
lines is not affected by the value of the
.B scale
variable, nor by any width or height given in the
.B .PS
line.
.NH 2
Invisible Objects
.PP
The modifier \fBinvis[ible]\fP makes an object entirely invisible.  This
used to be useful for positioning text in an invisible object that is
properly joined to neighboring ones.  Newer DWB versions and GNU
\fBpic\fP treat stand-alone text in exactly this way.
.NH 2
Filled Objects
.PP
It is possible to fill boxes, circles, and ellipses.  The
modifier \fBfill[ed]\fP accomplishes this.  You can suffix it with a fill
value; the default is given by the stule variable \fBfillval\fP.
.PP
DWB \fBpic\fP and \fBgpic\fP have opposite conventions for fill values
and different defaults.  DWB \fBfillval\fP defaults to 0.3 and smaller
values are darker; GNU \fBfillval\fP uses 0 for white and 1 for black.
.KS
.PS
circle fill; move; circle fill 0.4; move; circle fill 0.9;
.PE
.CE "5: \fBcircle fill; move; circle fill 0.4; move; circle fill 0.9;\fR"
.PP
GNU \fBgpic\fP makes some additional guarantees.  A fill value greater
than 1 can also be used: this means fill with the shade of gray that
is currently being used for text and lines.  Normally this will be
black, but output devices may provide a mechanism for changing this.
The invisible attribute does not affect the filling of objects.  Any
text associated with a filled object will be added after the object
has been filled, so that the text will not be obscured by the filling.
.PP
The closed-object modifier \fBsolid\fP is equivalent to \fBfill\fP
with the darkest fill value (DWB \fBpic\fP had this capability but
mentioned it only in a reference section).  
.NH 2
Colored Objects
.PP
As a GNU extension, three additional modifiers are available to specify
colored objects.  \fBoutline\fP sets the color of the outline, \fBshaded\fP
the fill color, and \fBcolor\fP sets both.  All three keywords expect a
suffix specifying the color.  Example:
.KS
.PS
box color "yellow"; arrow color "cyan"; circle shaded "green" outline "black";
.PE
.CE "6: \fBbox color ""yellow""; arrow color ""cyan""; \
circle shaded ""green"" outline ""black"";\fR"
.PP
Alternative spellings are \fBcolour\fP, \fBcolored\fP, \fBcoloured\fP,
and \fBoutlined\fP.
.PP
Currently, color support is not available in \*[tx] mode.  Predefined color
names for \fIgroff\/\fP(1) are in the device macro files, for example
\f(CWps.tmac\fP; additional colors can be defined with the \fB.defcolor\fP
request (see the manual page of GNU \fItroff\/\fP(1) for more details).
.PP
\fBpic\fP assumes that at the beginning of a picture both glyph and fill
color are set to the default value.
.NH 1
More About Text Placement
.PP
By default, text is centered at the geometric center of the object it is
associated with.  The modifier \fBljust\fP causes the left end to be
at the specified point (which means that the text lies to the right of
the specified place!), the modifier \fBrjust\fP puts the right end at
the place.  The modifiers \fBabove\fP and \fBbelow\fP center the text
one half line space in the given direction.
.PP
Text attributes can be combined:
.KS
.PS
[line up "ljust text" ljust;] 
move 1.5;
[line up "rjust text" rjust;] 
move;
[arrow 1 "ljust above" ljust above;]
move;
[arrow 1 "rjust below" rjust below;]
.PE
.CE "1: Text attributes"
.PP
What actually happens is that \fIn\fP text strings are centered in a box
that is \fBtextwid\fP wide by \fBtextht\fP high.  Both these variables
are initially zero (that is \fBpic\fR's way of not making assumptions
about \fI[tg]roff\/\fP(1)'s default point size).
.PP
In GNU \fBgpic\fR, objects can have an
.B aligned
attribute.
This will only work when the postprocessor is
\fBgrops\fP.
Any text associated with an object having the
.B aligned
attribute will be rotated about the center of the object
so that it is aligned in the direction from the start point
to the end point of the object.
Note that this attribute will have no effect for objects whose start and
end points are coincident.
.NH 1
More About Direction Changes
.PP
We've already seen how to change the direction in which objects are
composed from rightwards to downwards.  Here are some more
illustrative examples:
.KS
.PS
down;
[
	"\fBright; box; arrow; circle; arrow; ellipse\fP";
	move 0.2;
	[right; box; arrow; circle; arrow; ellipse;]
]
move down 0.3 from last [] .s;
[
	"\fBleft; box; arrow; circle; arrow; ellipse\fP"
	move 0.2;
	[left; box; arrow; circle; arrow; ellipse;]
]
# move down 0.3 from last [] .sw;
# To re-join this illustrations, delete everything from here down to
# the next #-comment, and uncomment the move line above
.PE
.CE "1: Effects of different motion directions (right and left)"
.KS
.PS
# To re-join this illustrations, delete everything down to here, then
# comment out the next `down' line.
# Don't forget to re-number the figures following!
down;
[
	"\fBdown; box; arrow; circle; arrow; ellipse;\fP"
	move 0.2;
	box; arrow; circle; arrow; ellipse;
]
move right 2 from last [] .e;
[
	up; box; arrow; circle; arrow; ellipse;
	move 0.2;
	"\fBup; box; arrow; circle; arrow; ellipse;\fP"
]
.PE
.CE "2: Effects of different motion directions (up and down)"
.PP
Something that may appear surprising happens if you change directions 
in the obvious way:
.KS
.PS
box; arrow; circle; down; arrow; ellipse
.PE
.CE "3: \fBbox; arrow; circle; down; arrow; ellipse\fP"
.LP
You might have expected that program to yield this:
.KS
.PS
box; arrow; circle; move to last circle .s; down; arrow; ellipse
.PE
.CE "4: More intuitive?"
.LP
But, in fact, to get Figure \*[SN]3 you have to do this:
.KS
.DS
.CW
\&.PS
box;
arrow;
circle;
move to last circle .s;
down;
arrow;
ellipse
\&.PE
.R
.DE
.KE
.LP
Why is this?  Because the exit point for the current direction is
already set when you draw the object.  The second arrow in Figure
\*[SN]2 dropped downwards from the circle's attachment point for an
object to be joined to the right.
.PP
The meaning of the command \fBmove to last circle .s\fP should be obvious.
In order to see how it generalizes, we'll need to go into detail on two
important topics; locations and object names.
.NH 1
Naming Objects
.PP
The most natural way to name locations in \fBpic\fP is relative to
objects.  In order to do this, you have to be able you have to be able
to name objects.  The \fBpic\fP language has rich facilities for this
that try to emulate the syntax of English.
.NH 2
Naming Objects By Order Of Drawing
.PP
The simplest (and generally the most useful) way to name an object is
with a \fBlast\fP clause.  It needs to be followed by an object type
name; \fBbox\fP, \fBcircle\fP, \fBellipse\fP, \fBline\fP, \fBarrow\fP,
\fBspline\fP, \fB""\fP, or \fB[]\fP (the last type refers to a \fIcomposite
object\fP which we'll discuss later).  So, for example, the \fBlast
circle\fP clause in the program attached to Figure \*[SN]3 refers to the
last circle drawn.
.PP
More generally, objects of a given type are implicitly numbered
(starting from\~1).  You can refer to (say) the third ellipse in the
current picture with \fB3rd ellipse\fP, or to the first box as \fB1st
box\fP, or to the fifth text string (which isn't an attribute to another
object) as \fB5th ""\fP.
.PP
Objects are also numbered backwards by type from the last one.
You can say \fB2nd last box\fP to get the second-to-last box, or
\fB3rd last ellipse\fP to get the third-to-last ellipse.
.PP
In places where \fIn\/\fBth\fR is allowed, \fB`\fIexpr\/\fB'th\fR is
also allowed.  Note that
.B 'th
is a single token: no space is allowed between the
.B '
and the \fBth\fP.
For example,
.IP
.KS
.DS
.CW
for i = 1 to 4 do {
   line from `i'th box.nw to `i+1'th box.se
}
.DE
.R
.KE
.NH 2
Naming Objects With Labels
.PP
You can also specify an object by referring to a label.  A label is a
word (which must begin with a capital letter) followed by a colon;
you declare it by placing it immediately before the object drawing command.
For example, the program
.KS
.DS
.CW
\&.PS
A: box "first" "object"
move;
B: ellipse "second" "object"
move;
arrow right at A .r;
\&.PE
.R
.DE
.KE
.LP
declares labels \fBA\fP and \fBB\fP for its first and second objects.
Here's what that looks like:
.KS
.PS
A: box "first" "object"
move;
B: ellipse "second" "object"
move;
arrow right at A .r;
.PE
.CE "1: Example of label use"
The \fBat\fP statement in the fourth line uses the label \fBA\fP (the
behavior of \fBat\fP will be explained in the next section).  We'll
see later on that labels are most useful for referring to block composite
objects.  
.PP
Labels are not constants but variables (you can view colon as a sort
of assignment).  You can say something like \fBA: A + (1,0);\fP
and the effect will be to reassign the label \fBA\fR to designate a
position one inch to the right of its old value. 
.NH 1
Describing locations
.PP
The location of points can be described in many different ways.  All these
forms are interchangeable as for as the \fBpic\fP language syntax is
concerned; where you can use one, any of the others that would make 
semantic sense are allowed.
.PP
The special label \fBHere\fR always refers to the current position.
.NH 2
Absolute Coordinates
.PP
The simplest is absolute coordinates in inches; \fBpic\fP uses a
Cartesian system with (0, 0) at the lower left corner of the virtual
drawing surface for each picture (that is, X increases to the right
and Y increases upwards).  An absolute location may always be written in the
conventional form as two comma-separated numbers surrounded by
parentheses (and this is recommended for clarity).  In contexts where
it creates no ambiguity, the pair of X and Y coordinates suffices
without parentheses.
.PP
It is a good idea to avoid absolute coordinates, however.  They tend
to make picture descriptions difficult to understand and modify.
Instead, there are quite a number of ways to specify locations
relative to \fBpic\fP objects and previous locations.
.PP
.NH 2
Locations Relative to Objects
.PP
The symbol \fBHere\fP always refers to the position of the last object
drawn or the destination of the last \fBmove\fP.
.PP
Alone and unqualified, a \fBlast circle\fP or any other way of
specifying a closed-object or arc location refers as a position to the
geometric center of the object.  Unqualified, the name of a line or
spline object refers to the position of the object start.
.PP
Also, \fBpic\fP objects have quite a few named locations
associated with them.  One of these is the object center, which can be
indicated (redundantly) with the suffix \fB.center\fP (or just \fB.c\fP).
Thus, \fBlast circle \&.center\fP is equivalent to \fBlast
circle\fP.
.NH 3
Locations Relative to Closed Objects
.PP
Every closed object (box, circle, ellipse, or block composite) also
has eight compass points associated with it;
.KS
.PS
define dot {circle fill rad 0.02 at $1}

define compass { [
	ME: $1;
	dot(ME.c);  "\fB .c\fP"  at ME .c ljust;
	dot(ME.n);  "\fB.n\fP"   at ME .n above
	dot(ME.ne); "\fB .ne\fP" at ME .ne above
	dot(ME.e);  "\fB .e\fP"  at ME .e ljust
	dot(ME.se); "\fB .se\fP" at ME .se below
	dot(ME.s);  "\fB.s\fP"   at ME .s below
	dot(ME.sw); "\fB.sw \fP" at ME .sw below
	dot(ME.w);  "\fB.w \fP"  at ME .w rjust
	dot(ME.nw); "\fB.nw \fP" at ME .nw above
] }
compass(box wid 1.5 ht 1);
move right from last [] .e;
compass(circle diam 1);
move right from last [] .e;
compass(ellipse wid 1.5 ht 1);
.PE
.CE "1: Compass points"
.LP
these are the locations where eight compass rays from the geometric center
would intersect the figure.  So when we say \fBlast circle .s\fP we are
referring to the south compass point of the last circle drawn.  The
explanation of Figure 7.3's program is now complete.
.PP
(In case you dislike compass points, the names \fB.top\fP,
\&\fB.bottom\fP, \fB.left\fP and \fB.right\fP are synonyms for \fB.n\fP,
\&\fB.s\fP, \fB.e\fP, and \fB.w\fP respectively; they can even be
abbreviated to \fB.t\fP, \fB.b\fP, \fB.l\fP and \fB.r\fP).
.PP
The names \fBcenter\fP, \fBtop\fP, \fBbottom\fP, \fBleft\fP, \fBright\fP,
\fBnorth\fP, \fBsouth\fP, \fBeast\fP, and \fBwest\fP can also be used
(without the leading dot) in a prefix form marked by \fBof\fP; thus,
\fBcenter of last circle\fP and \fBtop of 2nd last ellipse\fP are both
valid object references.  Finally, the names \fBleft\fP and \fBright\fP
can be prefixed with \fBupper\fP and \fBlower\fP which both have the
obvious meaning.
.PP
Arc objects also have compass point; they are the compass points of
the implied circle.
.NH 3
Locations Relative to Open Objects
.PP
Every open object (line, arrow, arc, or spline) has three named
points: \fB.start\fP, \fB.center\fP (or \fB.c\fP), and \fB.end\fP.  They
can also be used without leading dots in the \fBof\fP prefix form.
The center of an arc is the center of its circle, but the center of
a line, path, or spline is halfway between its endpoints.
.KS
.PS
define critical {
	[ ME: $1;
		dot(ME.c);     "\fB.center\fP" rjust at ME.center + (-0.1, 0.1)
		dot(ME.start); "\fB.start\fP"  rjust at ME.start  + (-0.1, 0.1)
		dot(ME.end);   "\fB.end\fP"    rjust at ME.end    + (-0.1, 0.1)
	]
}
critical(line up right 1);
move right 1 from last [] .e;
critical(arc rad 0.5 cw);
move down 0.5 from 2nd last [] .s;
critical(line right 1 then down .5 left 1 then right 1);
move right 1 from last [] .e;
critical(spline right 1 then up right then left then left 1);
.PE
.CE "2: Special points on open objects"
.PP
.NH 2
Ways of Composing Positions
.PP
Once you have two positions to work with, there are several ways to
combine them to specify new positions.
.NH 3
Vector Sums and Displacements
.PP
Positions may be added or subtracted to yield a new position (to be
more precise, you can only add a position and an expression pair; the
latter must be on the right side of the addition or subtraction sign).
The result is the conventional vector sum or difference of coordinates.
For example, \fBlast box .ne + (0.1, 0)\fP is a valid position.  This 
example illustrates a common use, to define a position slightly offset
from a named one (say, for captioning purposes).
.NH 3
Interpolation Between Positions
.PP
A position may be interpolated between any two positions.  The syntax
is `\fIfraction\fP \fBof the way between\fP \fIposition1\fP \fBand\fP
\fIposition2\fP'.  For example, you can say \fB1/3 of the way between
here and last ellipse .ne\fP.  The fraction may be in
numerator/denominator form or may be an ordinary number (values are
\fInot\fP restricted to [0,1]).  As an alternative to this verbose
syntax, you can say `\fIfraction\fP  \fB<\,\fP\fIposition1\fP \fB,\fP
\fIposition2\/\fP\fB>\fP'; thus, the example could also be written as
\fB1/3 <here, last ellipse>\fP.
.KS
.PS
arrow up right;
P: 1/3 of the way between last arrow .start and last arrow .end;
dot(P); move right 0.1; "P";
.PE
.CE "3: \fBP: 1/3 of the way between last arrow .start and last arrow .end\fP"
.PP
This facility can be used, for example, to draw double connections.
.KS
.PS
A: box "yin"; move;
B: box "yang";
arrow right at 1/4 <A.e,A.ne>;
arrow left  at 1/4 <B.w,B.sw>;
.PE
.CE "4: Doubled arrows"
.LP
You can get Figure \n[H1]-4 from the following program:
.KS
.DS
.CW
\&.PS
A: box "yin"; move;
B: box "yang";
arrow right at 1/4 <A.e,A.ne>;
arrow left  at 1/4 <B.w,B.sw>;
\&.PE
.R
.DE
.KE
.LP
Note the use of the short form for interpolating points.
.NH 3
Projections of Points
.PP
Given two positions \fIp\fP and \fIq\fP, the position
\fB(\,\fP\fIp\fP\fB,\fP \fIq\fP\fB)\fP has the X coordinate of \fIp\fP
and the Y coordinate of \fIq\fP.  This can be helpful in placing an
object at one of the corners of the virtual box defined by two other
objects.
.KS
.PS
box invis wid 2 height 1;
dot(last box .ne); "\fB(B,A)\fP is here" ljust at last circle + (0.1, 0.1);
dot(last box .se); "B" ljust at last circle + (0.1, -0.1)
dot(last box .sw); "\fB(A,B)\fP is here" rjust at last circle + (-0.1, -0.1);
dot(last box .nw); "A" ljust at last circle + (-0.1, 0.1)
.PE
.CE "5: Using (\fIx\fP, \fIy\fP) composition"
.NH 2
Using Locations
.PP
There are four ways to use locations; \fBat\fP, \fBfrom\fP, \fBto\fP,
and \fBwith\fP.  All three are object modifiers; that is, you use them
as suffixes to a drawing command.
.PP
The \fBat\fP modifier says to draw a closed object or arc with its
center at the following location, or to draw a line/spline/arrow
starting at the following location.
.PP
The \fBto\fP modifier can be used alone to specify a move destination.
The \fBfrom\fP modifier can be used alone in the same way as \fBat\fP.
.PP
The \fBfrom\fP and \fBto\fP modifiers can be used with a \fBline\fR or
\fBarc\fR command to specify start and end points of the object.  In
conjunction with named locations, this offers a very flexible
mechanism for connecting objects.  For example, the following program
.KS
.DS
.CW
\&.PS
box "from"
move 0.75;
ellipse "to"
arc cw from 1/3 of the way \e
    between last box .n and last box .ne to last ellipse .n;
\&.PE
.R
.DE
.KE
.LP
yields:
.KS
.PS
box "from"
move 0.75;
ellipse "to"
arc cw from 1/3 of the way \
    between last box .n and last box .ne to last ellipse .n;
.PE
.CE "6: A tricky connection specified with English-like syntax"
.PP
The \fBwith\fP modifier allows you to identify a named attachment
point of an object (or a position within the object) with another point.
This is very useful for connecting objects in a natural way.  For an
example, consider these two programs:
.KS
.PS
[
	[
		box wid 0.5 ht 0.5;
		box wid 0.75 ht 0.75;
	]
	move down 0.3 from last [] .s 0.1;
	"\fBbox wid 0.5 ht 0.5; box wid 0.75 ht 0.75\fP"
]
move from last [].e 1.5
[
	[
		box wid 0.5 ht 0.5;
		box wid 0.75 ht 0.75 with .sw at last box .se;
	]
	move down 0.3 from last [] .s 0.1;
	box invisible "\fBbox wid 0.5 ht 0.5;\fP" \
	  "\fBbox wid 0.75 ht 0.75 with .sw at last box .se;\fP"
]
.PE
.CE "7: Using the \fBwith\fP modifier for attachments"
.NH 2
The `chop' Modifier
.PP
When drawing lines between circles that don't intersect them at a
compass point, it is useful to be able to shorten a line by the radius 
of the circle at either or both ends.  Consider the following program:
.KS
.DS
.CW
\&.PS
circle "x"
circle "y" at 1st circle - (0.4, 0.6) 
circle "z" at 1st circle + (0.4, -0.6) 
arrow from 1st circle to 2nd circle chop 
arrow from 2nd circle to 3rd circle chop 
arrow from 3rd circle to 1st circle chop 
\&.PE
.DE
.R
.KE
.LP
It yields the following:
.KS
.PS
circle "x"
circle "y" at 1st circle - (0.4, 0.6) 
circle "z" at 1st circle + (0.4, -0.6) 
arrow from 1st circle to 2nd circle chop 
arrow from 2nd circle to 3rd circle chop 
arrow from 3rd circle to 1st circle chop 
.PE
.CE "8: The \fBchop\fR modifier"
.LP
Notice that the \fBchop\fR attribute moves arrowheads rather than
stepping on them.  By default, the \fBchop\fR modifier shortens both
ends of the line by \fBcirclerad\fR.  By suffixing it with a number
you can change the amount of chopping.
.PP
If you say \fBline .\|.\|.\& chop \fIr1\fP chop \fIr2\fP\fR with \fIr1\fP
and \fIr2\fP both numbers, you can vary the amount of chopping at both
ends.  You can use this in combination with trigonometric functions
to write code that will deal with more complex intersections.
.NH 1
Object Groups
.PP
There are two different ways to group objects in \fBpic\fP; \fIbrace
grouping\fP and \fIblock composites\fP.
.NH 2
Brace Grouping
.PP
The simpler method is simply to group a set of objects within curly
bracket or brace characters.  On exit from this grouping, the current
position and direction are restored to their value when the opening
brace was encountered.
.NH 2
Block Composites
.PP
A block composite object is created a series of commands enclosed by
square brackets.  The composite can be treated for most purposes like
a single closed object, with the size and shape of its bounding box.
Here is an example.  The program fragment
.KS
.DS
.CW
A: [
    circle;
    line up 1 at last circle .n;
    line down 1 at last circle .s;
    line right 1 at last circle .e;
    line left 1 at last circle .w;
    box dashed with .nw at last circle .se + (0.2, -0.2); 
    Caption: center of last box;
]
.R
.DE
.KE
.LP
yields the block in figure \n[H1]-1, which we show both with and
without its attachment points.  The block's location becomes the
value of \fBA\fP.
.KS
.PS
define junction {
	circle;
	line up 1 at last circle .n;
	line down 1 at last circle .s;
	line right 1 at last circle .e;
	line left 1 at last circle .w;
	box dashed with .nw at last circle .se + (0.2, -0.2); 
	Caption: center of last box;
}
[junction();]
move;
compass([junction()]);
.PE
.CE "1: A sample composite object"
.LP
To refer to one of the composite's attachment points, you can say
(for example) \fBA .s\fP.  For purposes of object naming, composites
are a class.  You could write \fBlast [] .s\fP as an equivalent
reference, usable anywhere a location is needed.  This construction is
very important for putting together large, multi-part diagrams.
.PP 
Blocks are also a variable-scoping mechanism, like a \fIgroff\/\fP(1)
environment.  All variable assignments done inside a block are undone
at the end of it.  To get at values within a block, write a name of
the block followed by a dot, followed by the label you
want.  For example, we could refer the the center of the box in the
above composite as \fBlast [] .Caption\fP or \fBA.Caption\fP.
.PP
This kind of reference to a label can be used in any way any other
location can be.  For example, if we added \fB"Hi!" at A.Caption\fP
the result would look like this:
.KS
.PS
A: [junction();]
"Hi!" at A.Caption;
.PE
.CE "2: Adding a caption using interior labeling"
.PP
You can also use interior labels in either part of a \fBwith\fR
modifier.  This means that the example composite could be placed
relative to its caption box by a command containing \fBwith A.Caption
at\fP.
.PP
Note that both width and height of the block composite object are always
positive:
.KS
.PS
[
	[
		box wid -0.5 ht 0.5
		box wid 0.75 ht 0.75
	]
	move down 0.3 from last [].s 0.1
	"\fBbox wid -0.5 ht 0.5; box wid 0.75 ht 0.75\fP"
]
move from last [].e 2
[
	[
		[ box wid -0.5 ht 0.5 ]
		box wid 0.75 ht 0.75
	]
	move down 0.3 from last [].s 0.1
	"\fB[box wid -0.5 ht 0.5]; box wid 0.75 ht 0.75\fP"
]
.PE
.CE "3: Composite block objects always have positive width and height
.PP
Blocks may be nested.  This means you can use block attachment points
to build up complex diagrams hierarchically, from the inside out.
Note that \fBlast\fP and the other sequential naming mechanisms
don't look inside blocks, so if you have a program that looks 
like 
.KS
.DS
.CW
\&.PS
P: [box "foo"; ellipse "bar"];
Q: [
	[box "baz"; ellipse "quxx"]
	"random text";
   ]
arrow from 2nd last [];
\&.PE
.R
.DE
.KE
.LP
the arrow in the last line will be attached to object \fBP\fP, not 
object \fBQ\fP.
.PP
In DWB \fBpic\fP, only references one level deep into enclosed blocks
were permitted.  GNU \fBgpic\fP removes this restriction.
.PP
The combination of block variable scoping, assignability of labels and
the macro facility that we'll describe later on can be used to
simulate functions with local variables (just wrap the macro body in
block braces).
.NH 1
Style Variables
.PP
There are a number of global style variables in \fBpic\fR that can be used to
change its overall behavior.  We've mentioned several of them in
previous sections.  They're all described here.  For each variable,
the default is given.
.TS H
center, tab(@), linesize(2);
lb | lb | lb
l | n | l.
.sp 2p
Style Variable@Default@What It Does
.sp 2p
_
.sp 2p
.TH
boxht@0.5@Default height of a box
boxwid@0.75@Default width of a box
lineht@0.5@Default length of vertical line
linewid@0.75@Default length of horizontal line
linethick@-1@Default line thickness
arcrad @0.25@Default radius of an arc
circlerad@0.25@Default radius of a circle
ellipseht@0.5@Default height of an ellipse
ellipsewid@0.75@Default width of an ellipse
moveht@0.5@Default length of vertical move
movewid@0.75@Default length of horizontal move
textht@0@Default height of box enclosing a text object
textwid@0@Default width of box enclosing a text object
arrowht@0.1@Length of arrowhead along shaft
arrowwid@0.05@Width of rear of arrowhead
arrowhead@1@Enable/disable arrowhead filling
dashwid@0.05@Interval for dashed lines
maxpswid@11@Maximum width of picture
maxpsht@8.5@Maximum height of picture
scale@1@Unit scale factor
fillval@0.5@Default fill value
.sp 5p
_
.TE
Any of these variables can be set with a simple assignment statement.
For example:
.KS
.PS
[boxht=1; boxwid=0.3; movewid=0.2; box; move; box; move; box; move; box;]
.PE
.CE "1: \fBboxht=1; boxwid=0.3; movewid=0.2; box; move; box; move; box; move; box;\fP"
.PP
In GNU \fBpic\fR, setting the \fBscale\fR variable re-scales all
size-related state variables so that their values remain equivalent in
the new units.
.PP
The command \fBreset\fP resets all style variables to their defaults.
You can give it a list of variable names as arguments (optionally
separated by commas), in which case it resets only those.
.PP
State variables retain their values across pictures until reset.
.NH 1
Expressions, Variables, and Assignment
.PP
A number is a valid expression, of course (all numbers are stored
internally as floating-point).  Decimal-point notation is acceptable;
in GNU \fBgpic\fR, scientific notation in C's `e' format (like
\f(CW5e-2\fP) is accepted.
.PP
Anywhere a number is expected, the language will also accept a
variable.  Variables may be the built-in style variable described in
the last section, or new variables created by assignment.
.PP
DWB \fBpic\fP supports only the ordinary assignment via \fB=\fP, which
defines the variable (on the left side of the equal sign) in the current
block if it is not already defined there, and then changes the value (on
the right side) in the current block.  The variable is not visible outside
of the block.  This is similar to the C\~programming language where a
variable within a block shadows a variable with the same name outside of
the block.
.PP
GNU \fBgpic\fP supports an alternate form of assignment using \fB:=\fP.
The variable must already be defined, and the value will be assigned to
that variable without creating a variable local to the current block.
For example, this
.KS
.DS
.CW
x=5
y=5
[
  x:=3
  y=3
]
print x " " y
.DE
.KE
.LP
prints \fB3 5\fP.
.PP
You can use the height, width, radius, and x and y coordinates of any
object or corner in expressions.  If \fBA\fP is an object label or name,
all the following are valid:
.KS
.DS
.CW
A.x                  # x coordinate of the center of A
A.ne.y               # y coordinate of the northeast corner of A
A.wid                # the width of A
A.ht                 # and its height
2nd last circle.rad  # the radius of the 2nd last circle
.R
.DE
.KE
.LP
Note the second expression, showing how to extract a corner coordinate.
.PP
Basic arithmetic resembling those of C operators are available; \fB+\fP,
\fB*\fP, \fB-\fP, \fB/\fP, and \fB%\fP.  So is \fB^\fP for exponentiation.
Grouping is permitted in the usual way using parentheses.  GNU \fBgpic\fP
allows logical operators to appear in expressions; \fB!\&\fP (logical
negation, not factorial), \fB&&\fP, \fB|\||\fP, \fB==\fP, \fB!=\fP,
\fB>=\fP, \fB<=\fP, \fB<\fP, \fB>\fP.
.PP
Various built-in functions are supported: \fBsin(\fIx\fB)\fR,
\fBcos(\fIx\fB)\fR, \fBlog(\fIx\fB)\fR, \fBexp(\fIx\fB)\fR,
\fBsqrt(\fIx\fB)\fR, \fBmax(\fIx\fB,\fIy\fB)\fR,
\fBatan2(\fIx\fB,\fIy\fB)\fR, \fBmin(\fIx\fB,\fIy\fB)\fR,
\fBint(\fIx\fB)\fR, \fBrand()\fP, and \fBsrand()\fP.
Both \fBexp\fP and \fBlog\fP are
base\~10; \fBint\fP does integer truncation; \fBrand()\fP returns a
random number in [0-1), and \fBsrand()\fP sets the seed for 
a new sequence of pseudo-random numbers to be returned by \fBrand()\fP
(\fBsrand()\fP is a GNU extension).
.PP
GNU \fBgpic\fP also documents a one-argument form or rand,
\fBrand(\fIx\fB)\fR, which returns a random number between 1 and
\fIx\fP, but this is deprecated and may be removed in a future
version.
.PP
The function \fBsprintf()\fP behaves like a C \fIsprintf\/\fP(3)
function that only takes %, %e, %f, and %g format strings.
.NH 1
Macros
.PP
You can define macros in \fBpic\fP.  This is useful for diagrams with
repetitive parts.  In conjunction with the scope rules for block
composites, it effectively gives you the ability to write functions.
.PP
The syntax is
.DS
.CW
\fBdefine\fP \fIname\fP \fB{\fP \fIreplacement text \fB}\fP
.R
.DE
.LP
This defines \fIname\fR as a macro to be replaced by the replacement
text (not including the braces).  The macro may be called as 
.DS
.CW
\fIname\fB(\fIarg1, arg2, \|.\|.\|.\& argn\fB)\fR
.R
.DE
.LP
The arguments (if any) will be substituted for tokens \fB$1\fP, \fB$2\fP
\&.\|.\|.\& \fB$n\fP
appearing in the replacement text.
.PP
As an example of macro use, consider this:
.KS
.DS
.CW
.ps -1
.vs -1
\&.PS
# Plot a single jumper in a box, $1 is the on-off state.
define jumper { [
    shrinkfactor = 0.8;
    Outer: box invis wid 0.45 ht 1;

    # Count on end ] to reset these
    boxwid = Outer.wid * shrinkfactor / 2;
    boxht  = Outer.ht  * shrinkfactor / 2;

    box fill (!$1) with .s at center of Outer;
    box fill ($1)  with .n at center of Outer;
] }

# Plot a block of six jumpers.
define jumperblock {
    jumper($1);
    jumper($2);
    jumper($3);
    jumper($4);
    jumper($5);
    jumper($6);

    jwidth  = last [].Outer.wid;
    jheight = last [].Outer.ht;

    box with .nw at 6th last [].nw wid 6*jwidth ht jheight;

    # Use {} to avoid changing position from last box draw.
    # This is necessary so move in any direction will work as expected
    {"Jumpers in state $1$2$3$4$5$6" at last box .s + (0, -0.2);}
}

# Sample macro invocations.
jumperblock(1,1,0,0,1,0);
move;
jumperblock(1,0,1,0,1,1);
\&.PE
.ps
.vs
.R
.DE
.KE
.LP
It yields the following:
.KS
.PS
# Plot a single jumper in a box, $1 is the on-off state.
define jumper { [
    shrinkfactor = 0.8;
    Outer: box invis wid 0.45 ht 1;

    # Count on end ] to reset these
    boxwid = Outer.wid * shrinkfactor / 2;
    boxht  = Outer.ht  * shrinkfactor / 2;

    box fill (!$1) with .s at center of Outer;
    box fill ($1)  with .n at center of Outer;
] }

# Plot a block of six jumpers
define jumperblock {
    jumper($1);
    jumper($2);
    jumper($3);
    jumper($4);
    jumper($5);
    jumper($6);

    jwidth  = last [].Outer.wid;
    jheight = last [].Outer.ht;

    box with .nw at 6th last [].nw wid 6*jwidth ht jheight;

    # Use {} to avoid changing position from last box draw.
    # This is necessary so move in any direction will work as expected
    {"Jumpers in state $1$2$3$4$5$6" at last box .s + (0, -0.2);}
}

# Sample macro invocations
jumperblock(1,1,0,0,1,0);
move 0.25;
jumperblock(1,0,1,0,1,1);
.PE
.CE "1: Sample use of a macro"
.LP
This macro example illustrates how you can combine [], brace grouping,
and variable assignment to write true functions.
.PP
One detail the example above does not illustrate is the fact that
macro argument parsing is not token-oriented.  If you call
\fBjumper(\ 1\ )\fP, the value of $1 will be \fB"\ 1\ "\fP.  You could
even call \fBjumper(big\ string)\fP to give $1 the value
\fB"big\ string"\fP.
.PP
If you want to pass in a coordinate pair, you can avoid getting
tripped up by the comma by wrapping the pair in parentheses.
.PP
Macros persist through pictures.  To undefine a macro, say \fBundef\fP
\fIname\fR; for example,
.DS
\f(CWundef jumper\fP
\f(CWundef jumperblock\fP
.DE
.LP
would undefine the two macros in the jumper block example.
.NH 1
Import/Export Commands
.PP
Commands that import or export data between \fBpic\fR and its
environment are described here.
.NH 2
File and Table Insertion
.PP
The statement
.DS
\f(CWcopy\fP \fIfilename\fR
.DE
.LP
inserts the contents of \fIfilename\fR in the \fBpic\fP input stream.
Any \fB.PS\fP/\fB.PE\fP pair in the file will be ignored.  This, you
can use this to include pre-generated images.
.PP
A variant of this statement replicates the \fBcopy thru\fP feature of
\fIgrap\fP(1).  The call
.DS
\f(CWcopy\fP \fIfilename\fR \f(CWthru\fP \fImacro\fP
.DE
.LP
calls \fImacro\fP (which may be either a name or replacement text)
on the arguments obtained by breaking each line of the file into
blank-separated fields.  The macro may have up to 9\~arguments.  The
replacement text may be delimited by braces or by a pair of instances
of any character not appearing in the rest of the text.
.PP
If you write
.DS
\f(CWcopy\fP \f(CWthru\fP \fImacro\fP
.DE
.LP
omitting the filename, lines to be parsed are taken from the input
source up to the next \fB.PE\fP.
.PP
In either of the last two \fBcopy\fP commands, GNU \fBgpic\fP permits a
trailing `\fBuntil\fP \fIword\/\fP' clause to be added which terminates
the copy when the first word matches the argument (the default
behavior is therefore equivalent to \fBuntil .PE\fP).
.PP
Accordingly, the command
.RS
.KS
.IP
.ft CW
.nf
\&.PS
copy thru % circle at ($1,$2) % until "END"
1 2
3 4
5 6
END
box
\&.PE
.R
.fi
.KE
.RE
.LP
is equivalent to
.RS
.KS
.IP
.ft CW
.nf
\&.PS
circle at (1,2)
circle at (3,4)
circle at (5,6)
box
\&.PE
.R
.fi
.KE
.RE
.NH 2
Debug Messages
.PP
The command \fBprint\fR accepts any number of arguments, concatenates
their output forms, and writes the result to standard error.  Each
argument must be an expression, a position, or a text string.
.NH 2
Escape to Post-Processor
.PP
If you write
.DS
\fBcommand\fR \fIarg\fR\|.\|.\|.
.DE
.LP
\fBpic\fP concatenates the arguments and pass them through as a line
to troff or \*[tx].  Each
.I arg
must be an expression, a position, or text.
This has a similar effect to a line beginning with
.B .
or
\fB\e\fR\|,
but allows the values of variables to be passed through.
.NH 2
Executing Shell Commands
.PP
The command 
.DS
\f(CWsh\fP \f(CW{\fP \fIanything.\|.\|.\fP \f(CW}\fP
.DE
.LP
macro-expands the text in braces, then executes it as a shell command.
This could be used to generate images or data tables for later
inclusion.  The delimiters shown as {} here may also be two copies of
any one character not present in the shell command text.  In either
case, the body may contain balanced {} pairs.  Strings in the body
may contain balanced or unbalanced braces in any case.
.NH 1
Control-flow constructs
.PP
The \fBpic\fP language provides conditionals and looping.  For
example,
.KS
.DS
.CW
pi = atan2(0, -1);
for i = 0 to 2 * pi by 0.1 do {
    "-" at (i/2, 0);
    "." at (i/2, sin(i)/2);
    ":" at (i/2, cos(i)/2);
}
.R
.DE
.KE
.LP
which yields this:
.KS
.PS
pi = atan2(0, -1);
for i = 0 to 2 * pi by 0.1 do {
    "-" at (i/2, 0);
    "." at (i/2, sin(i)/2);
    ":" at (i/2, cos(i)/2);
}
.PE
.CE "1: Plotting with a \fBfor\fP loop"
.LP
The syntax of the \fBfor\fP statement is:
.DS
\fBfor\fR \fIvariable\fR \fB=\fR \fIexpr1\/\fR \fBto\fR \fIexpr2\/\fR \
[\fBby\fR [\fB*\fR]\fIexpr3\/\fR] \fBdo\fR \fIX\fR \fIbody\fR \fIX\fR
.DE
The semantics are as follows:  Set
.I variable
to \fIexpr1\fR.
While the value of
.I variable
is less than or equal to
\fIexpr2\fR,
do
.I body
and increment
.I variable
by
\fIexpr3\fR;
if
.B by
is not given, increment
.I variable
by\~1.
If
.I expr3
is prefixed by\~\c
.B *
then
.I variable
will instead be multiplied by
\fIexpr3\fR.
.I X
can be any character not occurring in
\fIbody\fR; or the two \fIX\/\fPs may be paired braces (as in the
\fBsh\fR command).
.PP
The syntax of the \fBif\fP statement is as follows:
.DS
\fBif\fR \fIexpr\fR \fBthen\fR \fIX\fR \fIif-true\fR \fIX\fR \
[\fBelse\fR \fIY\fR \fIif-false\fR \fIY\/\fR]
.DE
Its semantics are as follows: Evaluate
\fIexpr\fR;
if it is non-zero then do
\fIif-true\fR,
otherwise do
\fIif-false\fR.
.I X
can be any character not occurring in
\fIif-true\fR.
.I Y
can be any character not occurring in
\fIif-false\fR.
.PP
Eithe or both of the
.I X
or
.I Y
pairs may instead be balanced pairs of
braces ({ and\~}) as in the \fBsh\fR command.  In either case, the
\fIif-true\fR may contain balanced pairs of braces.  None of these
delimiters will be seen inside strings.
.PP
All the usual relational operators my be used in conditional expressions;
\fB!\&\fP (logical negation, not factorial), \fB&&\fP, \fB|\||\fP, \fB==\fP,
\fB!=\fP, \fB>=\fP, \fB<=\fP, \fB<\fP, \fB>\fP.
.PP
String comparison is also supported using \fB==\fP and \fB!=\fP.  String
comparisons may need to be parenthesized to avoid syntactic
ambiguities.
.NH 1
Interface To [gt]roff
.PP
The output of \fBpic\fP is \fB[gt]roff\fP drawing commands.  The GNU
\fIgpic\/\fP(1) command warns that it relies on drawing extensions
present in \fIgroff\/\fP(1) that are not present in \fItroff\/\fP(1).
.NH 2
Scaling Arguments
.PP
The DWB \fIpic\/\fP(1) program will accept one or two arguments to
\&\fB.PS\fP, which is interpreted as a width and height in inches to
which the results of \fIpic\/\fP(1) should be scaled (width and height
scale independently).  If there is only one argument, it is
interpreted as a width to scale the picture to, and height will be
scaled by the same proportion.
.PP
GNU \fBgpic\fP is less general; it will accept a single width to scale
to, or a zero width and a maximum height to scale to.  With
two non-zero arguments, it will scale to the maximum height.
.NH 2
How Scaling is Handled
.PP
When \fBpic\fP processes a picture description on input, it passes
\fB.PS\fP and \fB.PE\fP through to the postprocessor.  The \fB.PS\fP
gets decorated with two numeric arguments which are the X and
Y\~dimensions of the picture in inches.  The post-processor can use
these to reserve space for the picture and center it.
.PP
The GNU incarnation of the \fBms\fP macro package, for example, includes
the following definitions:
.KS
.DS
.ps -1
.vs -1
.CW
\&.de PS
\&.br
\&.sp \e\en[DD]u
\&.ie \e\en[.$]<2 .@error bad arguments to PS (not preprocessed with pic?)
\&.el \e{\e
\&.	ds@need (u;\e\e$1)+1v
\&.	in +(u;\e\en[.l]-\e\en[.i]-\e\e$2/2>?0)
\&.\e}
\&..
\&.de PE
\&.par@reset
\&.sp \e\en[DD]u+.5m
\&..
.R
.DE
.ps
.vs
.KE
.LP
Equivalent definition will be supplied by GNU \fIpic\/\fP(1) if you use
the \-mpic option; this should make it usable with macro pages other
than \fIms\/\fR(1).
.PP
If \fB.PF\fP is used instead of \fB.PE\fP, the \fBtroff\fP position is
restored to what it was at the picture start (Kernighan notes that
the\~F stands for \[lq]flyback\[rq]).
.PP
The invocation
.DS
\&\fB.PS <\,\fP\fIfile\fP
.DE
.LP
causes the contents of \fIfile\fP to replace the \fB.PS\fP line.  This
feature is deprecated; use `\fBcopy\fP \fIfile\fR' instead).
.PP
By default, input lines that begin with a period are passed to the
postprocessor, embedded at the corresponding point in the output.
Messing with horizontal or vertical spacing is an obvious recipe for
bugs, but point size and font changes will usually be safe.
.PP
Point sizes and font changes are also safe within text strings, as
long as they are undone before the end of string.
.PP
The state of \fB[gt]roff\fP's fill mode is preserved across pictures.
.PP
The Kernighan paper notes that there is a subtle problem with
complicated equations inside \fBpic\fR pictures; they come out wrong if
\fIeqn\/\fP(1) has to leave extra vertical space for the equation.
If your equation involves more than subscripts and superscripts, you
must add to the beginning of each equation the extra information
\fBspace\~0\fP.  He gives the following example:
.KS
.DS
.CW
arrow
box "$space 0 {H( omega )} over {1 - H( omega )}$"
arrow
.R
.DE
.KE
.EQ
delim @@
.EN
.KS
.PS
arrow
box "@space 0 {H( omega )} over {1 - H( omega )}@"
arrow
.PE
.CE "1: Equations within pictures"
.NH 1
Interface to TeX
.PP
.PP
\*[tx] mode is enabled by the
.B \-t
option.
In \*[tx] mode, pic will define a vbox called
.B \egraph
for each picture.
You must yourself print that vbox using, for example, the command
.RS
.LP
.CW
\ecenterline{\ebox\egraph}
.RE
.LP
Actually, since the vbox has a height of zero this will produce
slightly more vertical space above the picture than below it;
.RS
.LP
.CW
\ecenterline{\eraise 1em\ebox\egraph}
.RE
.LP
would avoid this.
.PP
You must use a \*[tx] driver that supports the
.B tpic
specials, version\~2.
.PP
Lines beginning with
.B \e
are passed through transparently; a
.B %
is added to the end of the line to avoid unwanted spaces.
You can safely use this feature to change fonts or to
change the value of \fB\ebaselineskip\fR.
Anything else may well produce undesirable results; use at your own risk.
Lines beginning with a period are not given any special treatment.
.PP
The \*[tx] mode of \fIpic\/\fP(1) will \fInot\fP translate \fBtroff\fP
font and size changes included in text strings!
.NH 1
Obsolete Commands
.PP
GNU \fIgpic\/\fP(1)  has a command
.DS
\fBplot\fR \fIexpr\fR [\fB"\fItext\fB"\fR]
.DE
This is a text object which is constructed by using
.I text
as a format string for sprintf
with an argument of
\fIexpr\fP.
If
.I text
is omitted a format string of \fB"%g"\fP is used.
Attributes can be specified in the same way as for a normal text
object.
Be very careful that you specify an appropriate format string;
\fBpic\fP does only very limited checking of the string.
This is deprecated in favour of
\fBsprintf\fP.
.NH 1
Some Larger Examples
.PP
Here are a few larger examples, with complete source code.
One of our earlier examples is generated in an instructive way using a
for loop:
.KS
.DS
.ps -1
.vs -1
.CW
\&.PS
# Draw a demonstration up left arrow with grid box overlay
define gridarrow
{
    move right 0.1
    [
        {arrow up left $1;}
        box wid 0.5 ht 0.5 dotted with .nw at last arrow .end;
        for i = 2 to ($1 / 0.5) do
        {
            box wid 0.5 ht 0.5 dotted with .sw at last box .se;
        }
        move down from last arrow .center;
        [
            if ( $1 == boxht ) \e
            then { "\efBline up left\efP" } \e
            else { sprintf("\efBarrow up left %g\efP", $1) };
        ]
    ] 
    move right 0.1 from last [] .e;
}
gridarrow(0.5);
gridarrow(1);
gridarrow(1.5);
gridarrow(2);
undef gridarrow
\&.PE
.R
.DE
.ps
.vs
.KE
.KS
.PS
# Draw a demonstration up left arrow with grid box overlay
define gridarrow
{
	move right 0.1
	[
		{arrow up left $1;}
		box wid 0.5 ht 0.5 dotted with .nw at last arrow .end;
		for i = 2 to ($1 / 0.5) do
		{
			box wid 0.5 ht 0.5 dotted with .sw at last box .se;
		}
		move down from last arrow .center;
		[
			if ( $1 == boxht ) \
			then { "\fBline up left\fP" } \
			else { sprintf("\fBarrow up left %g\fP", $1) };
		]
	] 
	move right 0.1 from last [] .e;
}
gridarrow(0.5);
gridarrow(1);
gridarrow(1.5);
gridarrow(2);
undef gridarrow
.PE
.CE "1: Diagonal arrows (dotted boxes show the implied 0.5-inch grid)"
.PP
Here's an example concocted to demonstrate layout of a large,
multiple-part pattern:
.KS
.DS
.ps -1
.vs -1
.CW
\&.PS
define filter {box ht 0.25 rad 0.125}
lineht = 0.25;
Top: [
    right;
    box "\efBms\efR" "sources";
    move;
    box "\efBHTML\efR" "sources";
    move;
    box "\efBlinuxdoc-sgml\efP" "sources" wid 1.5;
    move;
    box "\efBTexinfo\efP" "sources";

    line down from 1st box .s lineht;
    A: line down;
    line down from 2nd box .s; filter "\efBhtml2ms\efP";
    B: line down;
    line down from 3rd box .s; filter "\efBformat\efP";
    C: line down;
    line down from 4th box .s; filter "\efBtexi2roff\efP";
    D: line down;
]
move down 1 from last [] .s;
Anchor: box wid 1 ht 0.75 "\efBms\efR" "intermediate" "form";
arrow from Top.A.end to Anchor.nw;
arrow from Top.B.end to 1/3 of the way between Anchor.nw and Anchor.ne;
arrow from Top.C.end to 2/3 of the way between Anchor.nw and Anchor.ne;
arrow from Top.D.end to Anchor.ne
{
    # PostScript column
    move to Anchor .sw;
    line down left then down ->;
    filter "\efBpic\efP";
    arrow;
    filter "\efBeqn\efP";
    arrow;
    filter "\efBtbl\efP";
    arrow;
    filter "\efBgroff\efP";
    arrow;
    box "PostScript";

    # HTML column
    move to Anchor .se;
    line down right then down ->;
    A: filter dotted "\efBpic2img\efP";
    arrow;
    B: filter dotted "\efBeqn2html\efP";
    arrow;
    C: filter dotted "\efBtbl2html\efP";
    arrow;
    filter "\efBms2html\efP";
    arrow;
    box "HTML";

    # Nonexistence caption
    box dashed wid 1 at B + (2, 0) "These tools" "don't yet exist";
    line chop 0 chop 0.1 dashed from last box .nw to A.e ->;
    line chop 0 chop 0.1 dashed from last box .w  to B.e ->;
    line chop 0 chop 0.1 dashed from last box .sw to C.e ->;
}
\&.PE
.R
.DE
.ps
.vs
.KE
.KS
.PS
define filter {box ht 0.25 rad 0.125}
lineht = 0.25;
Top: [
    right;
    box "\fBms\fR" "sources";
    move;
    box "\fBHTML\fR" "sources";
    move;
    box "\fBlinuxdoc-sgml\fP" "sources" wid 1.5;
    move;
    box "\fBTexinfo\fP" "sources";

    line down from 1st box .s lineht;
    A: line down;
    line down from 2nd box .s; filter "\fBhtml2ms\fP";
    B: line down;
    line down from 3rd box .s; filter "\fBformat\fP";
    C: line down;
    line down from 4th box .s; filter "\fBtexi2roff\fP";
    D: line down;
]
move down 1 from last [] .s;
Anchor: box wid 1 ht 0.75 "\fBms\fR" "intermediate" "form";
arrow from Top.A.end to Anchor.nw;
arrow from Top.B.end to 1/3 of the way between Anchor.nw and Anchor.ne;
arrow from Top.C.end to 2/3 of the way between Anchor.nw and Anchor.ne;
arrow from Top.D.end to Anchor.ne
{
    # PostScript column
    move to Anchor .sw;
    line down left then down ->;
    filter "\fBpic\fP";
    arrow;
    filter "\fBeqn\fP";
    arrow;
    filter "\fBtbl\fP";
    arrow;
    filter "\fBgroff\fP";
    arrow;
    box "PostScript";

    # HTML column
    move to Anchor .se;
    line down right then down ->;
    A: filter dotted "\fBpic2img\fP";
    arrow;
    B: filter dotted "\fBeqn2html\fP";
    arrow;
    C: filter dotted "\fBtbl2html\fP";
    arrow;
    filter "\fBms2html\fP";
    arrow;
    box "HTML";

    # Nonexistence caption
    box dashed wid 1 at B + (2, 0) "These tools" "don't yet exist";
    line chop 0 chop 0.1 dashed from last box .nw to A.e ->;
    line chop 0 chop 0.1 dashed from last box .w  to B.e ->;
    line chop 0 chop 0.1 dashed from last box .sw to C.e ->;
}
.PE
.CE "2: Hypothetical production flow for dual-mode publishing"
.LP
.\"%%REFERENCE%%
.NH 1
PIC Reference
.PP
This is an annotated grammar of \fBpic\fP.
.NH 2
Lexical Items
.PP
In general, \fBpic\fP is a free-format, token-oriented language that
ignores whitespace outside strings.  But certain lines and contructs
are specially interpreted at the lexical level:
.PP
A comment begins with \fB#\fP and continues to \fB\en\fP (comments may
also follow text in a line).  A line beginning with a period or
backslash may be interpreted as text to be passed through to the
post-processor, depending on command-line options.  An end-of-line
backslash is interpreted as a request to continue the line; the
backslash and following newline are ignored.
.PP
.RS
Here are the grammar terminals:
.IP \s[-1]INT\s[0]
A positive integer.
.IP \s[-1]NUMBER\s[0]
A floating point numeric constant.  May contain a decimal point or be
expressed in scientific notation in the style of \fIprintf\/\fP(3)'s %e
escape.  A trailing `i' or `I' (indicating the unit `inch') is ignored.
.IP \s[-1]TEXT\s[0]
A string enclosed in double quotes.  A double quote within \s[-1]TEXT\s[0]
must be preceded by a backslash.  Instead of \s[-1]TEXT\s[0] you can use
.DS
.CW
sprintf ( TEXT [, <expr> ...] )
.R
.DE
.IP
except after the `until' and `last' keywords, and after all ordinal
keywords (`th' and friends).
.IP \s[-1]VARIABLE\s[0]
A string starting with a character from the set [a-z], optionally
followed by one or more characters of the set [a-zA-Z0-9_].
(Values of variables are preserved across pictures.)
.IP \s[-1]LABEL\s[0]
A string starting with a character from the set [A-Z], optionally
followed by one or more characters of the set [a-zA-Z0-9_].
.IP \s[-1]COMMAND-LINE\s[0]
A line starting with a command character (`.' in groff mode, `\e' in
\*[tx] mode).
.IP \s[-1]BALANCED-TEXT\s[0]
A string either enclosed by `{' and `}' or with \fIX\fP and \fIX\fP,
where \fIX\fP doesn't occur in the string.
.IP \s[-1]BALANCED-BODY\s[0]
Delimiters as in \s[-1]BALANCED-TEXT\s[0]; the body will be interpreted as
`\fB\[la]command\[ra].\|.\|.\fP'.
.IP \s[-1]FILENAME\s[0]
The name of a file.  This has the same semantics as \s[-1]TEXT\s[0].
.IP \s[-1]MACRONAME\s[0]
Either \s[-1]VARIABLE\s[0] or \s[-1]LABEL\s[0].
.RE
.NH 2
Semi-Formal Grammar
.PP
Tokens not enclosed in \[la]\|\[ra] are literals, except:
.IP 1.
\fB\en\fP is a newline.
.IP 2.
Three dots is a suffix meaning `replace with 0 or more repetitions
of the preceding element(s).
.IP 3.
An enclosure in square brackets has its usual meaning of `this clause is
optional'.
.IP 4.
Square-bracket-enclosed portions within tokens are optional.  Thus,
`h\^[eigh]\^t' matches either `height' or `ht'.
.LP
If one of these special tokens has to be referred to literally, it is
surrounded with single quotes.
.PP
The top-level \fBpic\fP object is a picture.
.DS
.CW
<picture> ::=
  .PS [NUMBER [NUMBER]]\en
  <statement> ...
  .PE \en
.R
.DE
.PP
The arguments, if present, represent the width and height of the picture,
causing \fBpic\fR to attempt to scale it to the given dimensions in
inches.  In no case, however, will the X and Y\~dimensions of the
picture exceed the values of the style variables \fBmaxpswid\fP and
\fBmaxpsheight\fP (which default to the normal 8.5\^i by 11\^i page size).
.PP
If the ending `.PE' is replaced by `.PF', the page vertical position is
restored to its value at the time `.PS' was encountered.  Another
alternate form of invocation is `.PS\ <\s[-1]FILENAME\s[0]', which
replaces the `.PS' line with a file to be interpreted by \fBpic\fR (but
this feature is deprecated).
.PP
The `.PS', `.PE', and `.PF' macros to perform centering and scaling are
normally supplied by the post-processor.
.PP
In the following, either `|' or a new line starts an alternative.
.DS
.CW
<statement> ::=
  <command> ;
  <command> \en
.R
.DE
.DS
.CW
<command> ::=
  <primitive> [<attribute>]
  LABEL : [;] <command>
  LABEL : [;] <command> [<position>]
  { <command> ... }
  VARIABLE [:] = <any-expr>
  up | down | left | right
  COMMAND-LINE
  command <print-arg> ...
  print <print-arg> ...
  sh BALANCED-TEXT
  copy FILENAME
  copy [FILENAME] thru MACRONAME [until TEXT]
  copy [FILENAME] thru BALANCED-BODY [until TEXT]
  for VARIABLE = <expr> to <expr> [by [*] <expr>] do BALANCED-BODY
  if <any-expr> then BALANCED-BODY [else BALANCED-BODY]
  reset [VARIABLE [[,] VARIABLE ...]]
.R
.DE
.DS
.CW
<print-arg> ::=
  TEXT
  <expr>
  <position>
.R
.DE
.PP
The current position and direction are saved on entry to a `{\ .\|.\|.\ }'
construction and restored on exit from it.
.PP
Note that in `if' constructions, newlines can only occur in
\s[-1]BALANCED-BODY\s[0].  This means that
.DS
.CW
if
{ ... }
else
{ ... }
.R
.DE
.PP
will fail.  You have to use the braces on the same line as the keywords:
.DS
.CW
if {
\&...
} else {
\&...
}
.R
.DE
.PP
This restriction doesn't hold for the body after the `do' in a `for'
construction.
.DS
.CW
<any-expr> ::=
  <expr>
  <text-expr>
  <any-expr> <logical-op> <any-expr>
  ! <any-expr>
.R
.DE
.DS
.CW
<logical-op> ::=
  == | != | && | '||'
.R
.DE
.DS
.CW
<text-expr> ::=
  TEXT == TEXT
  TEXT != TEXT
.R
.DE
.PP
Logical operators are handled specially by \fBpic\fP since they can
deal with text strings also.  \fBpic\fP uses \%\fIstrcmp\/\fP(3) to test
for equality of strings; an empty string is considered as `false' for
`&&' and `|\||'.
.DS
.CW
<primitive> ::=
  box                   \fR# closed object \[em] rectangle\fP
  circle                \fR# closed object \[em] circle\fP
  ellipse               \fR# closed object \[em] ellipse\fP
  arc                   \fR# open object \[em] quarter-circle\fP
  line                  \fR# open object \[em] line\fP
  arrow                 \fR# open object \[em] line with arrowhead\fP
  spline                \fR# open object \[em] spline curve\fP
  move
  TEXT TEXT ...         \fR# text within invisible box\fP
  plot <expr> TEXT      \fR# formatted text\fP
  '[' <command> ... ']'
.R
.DE
.PP
Drawn objects within `[\ .\|.\|.\ ]' are treated as a single composite
object with a rectangular shape (that of the bounding box of all the
elements).  Variable and label assignments within a block are local to
the block.  Current direction of motion is restored to the value at start
of block upon exit.  Position is \fInot\fR restored (unlike `{\ }');
instead, the current position becomes the exit position for the current
direction on the block's bounding box.
.DS
.CW
<attribute> ::=
  h[eigh]t <expr>       \fR# set height of closed figure \fP
  wid[th] <expr>        \fR# set width of closed figure \fP
  rad[ius] <expr>       \fR# set radius of circle/arc \fP
  diam[eter] <expr>     \fR# set diameter of circle/arc \fP
  up [<expr>]           \fR# move up \fP
  down [<expr>]         \fR# move down \fP
  left [<expr>]         \fR# move left \fP
  right [<expr>]        \fR# move right \fP
  from <position>       \fR# set from position of open figure\fP
  to <position>         \fR# set to position of open figure\fP
  at <position>         \fR# set center of open figure\fP
  with <path>           \fR# fix corner/named point at specified location\fP
  with <position>       \fR# fix position of object at specified location\fP
  by <expr-pair>        \fR# set object's attachment point\fP
  then                  \fR# sequential segment composition\fP
  dotted [<expr>]       \fR# set dotted line style\fP
  dashed [<expr>]       \fR# set dashed line style\fP
  thick[ness] <expr>    \fR# set thickness of lines\fP
  chop [<expr>]         \fR# chop end(s) of segment\fP
  '->' | '<-' | '<->'   \fR# decorate with arrows\fP
  invis[ible]           \fR# make primitive invisible\fP
  solid                 \fR# make closed figure solid\fP
  fill[ed] [<expr>]     \fR# set fill density for figure\fP
  colo[u]r[ed] TEXT     \fR# set fill and outline color for figure\fP
  outline[d] TEXT       \fR# set outline color for figure\fP
  shaded TEXT           \fR# set fill color for figure\fP
  same                  \fR# copy size of previous object\fP
  cw | ccw              \fR# set orientation of curves\fP
  ljust | rjust         \fR# adjust text horizontally\fP
  above | below         \fR# adjust text vertically\fP
  aligned               \fR# align parallel to object\fP
  TEXT TEXT ...         \fR# text within object\fP
  <expr>                \fR# motion in the current direction\fR
.R
.DE
.PP
Missing attributes are supplied from defaults; inappropriate ones are
silently ignored.  For lines, splines, and arcs, height and width
refer to arrowhead size.
.PP
The `at' primitive sets the center of the current object.  The
`with' attribute fixes the specified feature of the given object
to a specified location.  (Note that `with' is incorrectly described
in the Kernighan paper.)
.PP
The `by' primitive is not documented in the tutorial portion of
the Kernighan paper, and should probably be considered unreliable.
.PP
The primitive `arrow' is a synonym for `line\ ->'.
.PP
Text is normally an attribute of some object, in which case successive
strings are vertically stacked and centered on the object's center by
default.  Standalone text is treated as though placed in an invisible
box.
.PP
A text item consists of a string or sprintf-expression, optionally
followed by positioning information.  Text (or strings specified with
`sprintf' may contain [gtn]roff font changes, size changes, and local
motions, provided those changes are undone before the end of the current
item.
.PP
A position is an (x,y) coordinate pair.  There are lots of different
ways to specify positions:
.DS
.CW
<position> ::=
  <position-not-place>
  <place>
.R
.DE
.DS
.CW
<position-not-place> ::=
  <expr-pair>
  <position> + <expr-pair>
  <position> - <expr-pair>
  ( <position> , <position> )
  <expr> [of the way] between <position> and <position>
  <expr> '<' <position> , <position> '>'
.R
.DE
.DS
.CW
<expr-pair> ::=
  <expr> , <expr>
  ( expr-pair )
.R
.DE
.DS
.CW
<place> ::=
  <label>
  <label> <corner>
  <corner> [of] <label>
  Here
.R
.DE
.DS
.CW
<label> ::=
  LABEL [. LABEL ...]
  <nth-primitive>
.R
.DE
.DS
.CW
<corner> ::=
  .n | .e | .w | .s
  .ne | .se | .nw | .sw
  .c[enter] | .start | .end
  .t[op] | .b[ot[tom]] | .l[eft] | .r[ight]
  left | right | <top-of> | <bottom-of>
  <north-of> | <south-of> | <east-of> | <west-of>
  <center-of> | <start-of> | <end-of>
  upper left | lower left | upper right | lower right
.R
.DE
.DS
.CW
<xxx-of> ::=
  xxx                   \fR# followed by `of'\fP
.R
.DE
.DS
.CW
<nth-primitive> ::=
  <ordinal> <object-type>
  [<ordinal>] last <object-type>
.R
.DE
.DS
.CW
<ordinal> ::=
  INT th
  INT st | INT nd | INT rd
  ` <any-expr> 'th
.R
.DE
.DS
.CW
<object-type> ::=
  box
  circle
  ellipse
  arc
  line
  arrow
  spline
  '[]'
  TEXT
.R
.DE
.PP
As Kernighan notes, \[lq]since barbarisms like \fB1th\fP and \fB3th\fP are
barbaric, synonyms like \fB1st\fP and \fB3rd\fP are accepted as well.\[rq]
Objects of a given type are numbered from 1 upwards in order of
declaration; the \fBlast\fP modifier counts backwards.
.PP
The \[lq]'th\[rq] form (which allows you to select a previous object with
an expression, as opposed to a numeric literal) is not documented in DWB's
\fIpic\/\fR(1).
.PP
The \[la]\|\fIxxx\fP-of\|\[ra] rule is special: The lexical parser checks whether
\fIxxx\fP is followed by the token `of' without eliminating it so that
the grammar parser can still see `of'.  Valid examples of specifying a
place with corner and label are thus
.DS
.CW
A .n
\&.n of A
\&.n A
north of A
.R
.DE
.LP
while
.DS
.CW
north A
A north
.R
.DE
both cause a syntax error.  (DWB \fBpic\fP also allows the weird form
`A\ north\ of'.)
.PP
Here the special rules for the `with' keyword using a path:
.DS
.CW
<path> ::=
  <relative-path>
  ( <relative-path> , <relative-path> )
.R
.DE
.DS
.CW
<relative-path> ::=
  <corner>
  . LABEL [. LABEL ...] [<corner>]
.R
.DE
.PP
The following style variables control output:
.TS H
center tab(@), linesize(2);
lb | lb | lb
l | n | l.
.sp 2p
Style Variable@Default@What It Does
.sp 2p
_
.sp 2p
.TH
boxht@0.5@Default height of a box
boxwid@0.75@Default height of a box
lineht@0.5@Default length of vertical line
linewid@0.75@Default length of horizontal line
arcrad @0.25@Default radius of an arc
circlerad@0.25@Default radius of a circle
ellipseht@0.5@Default height of an ellipse
ellipsewid@0.75@Default width of an ellipse
moveht@0.5@Default length of vertical move
movewid@0.75@Default length of horizontal move
textht@0@Default height of box enclosing a text object
textwid@0@Default width of box enclosing a text object
arrowht@0.1@Length of arrowhead along shaft
arrowwid@0.05@Width of rear of arrowhead
arrowhead@1@Enable/disable arrowhead filling
dashwid@0.05@Interval for dashed lines
maxpswid@11@Maximum width of picture
maxpsht@8.5@Maximum height of picture
scale@1@Unit scale factor
fillval@0.5@Default fill value
.sp 5p
_
.TE
Any of these can be set by assignment, or reset using the \fBreset\fP
statement.  Style variables assigned within `[\ ]' blocks are restored to
their beginning-of-block value on exit; top-level assignments persist
across pictures.  Dimensions are divided by \fBscale\fR on output.
.PP
All \fBpic\fP expressions are evaluated in floating point; units
are always inches (a trailing `i' or `I' is ignored).  Expressions have
the following simple grammar, with semantics very similar to
C\~expressions:
.DS
.CW
<expr> ::=
  VARIABLE
  NUMBER
  <place> <place-attribute>
  <expr> <op> <expr>
  - <expr>
  ( <any-expr> )
  ! <expr>
  <func1> ( <any-expr> )
  <func2> ( <any-expr> , <any-expr> )
  rand ( )
.R
.DE
.DS
.CW
<place-attribute>
 .x | .y | .h[eigh]t | .wid[th] | .rad
.R
.DE
.DS
.CW
<op> ::=
  + | - | * | / | % | ^ | '<' | '>' | '<=' | '>='
.R
.DE
.DS
.CW
<func1> ::=
  sin | cos | log | exp | sqrt | int | rand | srand
.R
.DE
.DS
.CW
<func2> ::=
  atan2 | max | min
.R
.DE
.LP
Both \fBexp\fP and \fBlog\fP are base 10; \fBint\fP does integer
truncation; and \fBrand()\fP returns a random number in [0-1).
.PP
There are \fBdefine\fP and \fBundef\fR statements which are not part
of the grammar (they behave as pre-processor macros to the language).
These may be used to define pseudo-functions.
.DS
.CW
\fBdefine\fP \fIname\fP \fB{\fP \fIreplacement-text\fP \fB}\fP
.R
.DE
.LP
This defines \fIname\fR as a macro to be replaced by the replacement
text (not including the braces).  The macro may be called as 
.DS
.CW
\fIname\/\fB(\,\fIarg1, arg2, .\|.\|., argn\fB\/)\fR
.R
.DE
.LP
The arguments (if any) will be substituted for tokens $1, $2 .\|.\|.\& $n
appearing in the replacement text.  To undefine a macro, say \fBundef\fP
\fIname\fR, specifying the name to be undefined.
.\"%%POSTLUDE%%
.NH 1
History and Acknowledgements
.PP
Original \fBpic\fP was written to go with Joseph Ossanna's original
\fItroff\/\fP(1) by Brian Kernighan, and later re-written by Kernighan
with substantial enhancements (apparently as part of the evolution of
\fItroff\/\fP(1) into \fIditroff\/\fP(1) to generate
device-independent output).
.PP
The language had been inspired by some earlier graphics languages
including \fBideal\fP and \fBgrap\fP.  Kernighan credits Chris van Wyk
(the designer of \fBideal\fP) with many of the ideas that went into
\fBpic\fP.
.PP
.\" the original definitions of EQ and EN cause insertion of vertical
.\" space which is not appropriate here
.de EQ
..
.de EN
..
.EQ
delim $$
.EN
The \fBpic\fP language was originally described by Brian Kernighan in
Bell Labs Computing Science Technical Report #116 (you can obtain a
PostScript copy of the revised version, [1], by sending a mail message to
\fInetlib@research.att.com\fP with a body of `send 116 from
research/cstr').  There have been two revisions, in 1984 and 1991.
.PP
The document you are reading effectively subsumes Kernighan's
description; it was written to fill in lacun\[ae] in the exposition and
integrate in descriptions of the GNU \fIgpic\/\fP(1) features.
.PP
The GNU \fBgpic\fR implementation was written by James Clark
\[la]\,\fIjjc@jclark.com\/\fP\[ra].  It is currently maintained by Werner
Lemberg \[la]\,\fIwl@gnu.org\/\fP\[ra].
.NH 1
Bibliography
.IP 1.
Kernighan, B. W.  \fBPIC \[em] A Graphics Language for Typesetting
(Revised User Manual)\fP.  Bell Labs Computing Science Technical Report
#116, December 1991.
.IP 2.
Van Wyk, C. J.  \fBA high-level language for specifying pictures\fP.
\fIACM Transactions On Graphics\fP 1,2 (1982) 163-182.
.
.\" end of pic.ms
