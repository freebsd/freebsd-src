.\" This file gives a small example how a web page could look like if created
.\" with groff.
.\"
.\"
.\" To make it work with other output devices also, we include www.tmac
.\" directly.
.\"
.nr PS 10
.nr VS 12
.if '\*[.T]'html' .nr LL 10i
.if r ps4html .nr LL 10i
.mso www.tmac
.
.de blm-macro
.  nr tmp \\n[.i]
.  LP
.  in \\n[tmp]u
..
.blm blm-macro
.
.HX 0
.
.defcolor mydarkred rgb 0.65f 0.1f 0.2f
.defcolor mydarkgreen rgb 0.1f 0.5f 0.2f
.defcolor mydarkblue rgb 0.1f 0.2f 0.6f
.
.ds GNU \m[mydarkred]G\m[]\m[mydarkgreen]N\m[]\m[mydarkblue]U\m[]
.
.HTL
.NHR
.
Home of Groff (GNU Troff).
.
.HTML-IMAGE-LEFT
.PSPIC -L gnu.eps 2i 2i
.ie r ps4html \
.  nop \v'-0.66i'\h'2.0i'\s[100]\*[GNU] Troff\s0
.el \
.  nop \v'-0.66i'\h'2.0i'\s[60]\*[GNU] Troff\s0
.HTML-IMAGE-END
.
.HnS 1
.HR
GNU Troff
.URL http://\:groff.ffii.org/ (Groff) 
\[em] a
.URL http://\:www.gnu.org/ GNU 
project.
Hosted by
.URL http://\:ffii.org/ FFII .
.HR
.HnE
.LK

.DC T HE mydarkred
groff (GNU Troff) software
is a typesetting package which reads plain text mixed with
formatting commands and produces formatted output.
Groff now supports HTML.
.
.
.SH
Download

.URL "ftp://\:groff.ffii.org/\:pub/\:groff/" Germany 
|
.URL "ftp://\:ftp.gnu.org/\:gnu/\:groff/" USA 

.URL http://\:www.gnu.org/\:copyleft/\:gpl.html License 
|
.URL http://\:ffii.org/\:mailman/\:listinfo/\:groff/ "Mailing list" 
|
.URL "http://\:gnuwin32.sf.net/\:packages/\:groffl.htm" "Groff for Windows" 

User issues lead:
.MTO Ted.Harding@\:nessie.mcc.ac.uk "Ted Harding"
.br
Technical issues lead:
.MTO wl@\:gnu.org "Werner Lemberg"
.
.
.SH
README

This is the GNU groff document formatting system.
The version number is given in the file VERSION.

Included in this release are implementations of troff, pic, eqn, tbl, refer,
the \-man macros and the \-ms macros, and drivers for PostScript, TeX dvi
format, HP LaserJet 4 printers, HTML format (still alpha), and
typewriter-like devices.
Also included is a modified version of the Berkeley \-me macros, an enhanced
version of the X11 xditview previewer, and an implementation of the \-mm
macros contributed by
.MTO jh@\:axis.se "J\[:o]rgen H\[:a]gg" .

See the file INSTALL for installation instructions.
You will require a C++ compiler.

The file NEWS describes recent user-visible changes to groff.

Groff is free software.
See the file COPYING for copying permission.

The file PROBLEMS describes various problems that have been encountered in
compiling, installing, and running groff.

For the moment, the documentation assumes that you are already familiar with
the Unix versions of troff, \-man, \-ms and the preprocessors.

The most recent released version of groff is always available by anonymous
ftp from ftp.gnu.org in the directory pub/\:gnu/\:groff.

A CVS repository is now available, containing the current development
version of groff.
You can access it with the commands

.RS
.nf
.ft C
export CVSROOT=:pserver:anoncvs@anoncvs.ffii.org:/var/cvs
cvs login
cvs -z9 co groff
.ft P
.fi
.RE

(if the prompt for the password appears, just press the enter key).
After a successful login you no longer need the first two commands; an
update of a checked out repository should be done with

.RS
.ft C
cvs -z9 update -dP
.ft P
.RE

Please read the info pages of cvs for further details.

Alternatively, you can download snapshots (which are updated twice a day)
from
.URL ftp://\:ftp.ffii.org/\:pub/\:groff/\:devel/\:groff-current.tar.gz here
or a diff file relative to the latest official groff release as:

.RS
.ft C
ftp://ftp.ffii.org/pub/groff/devel/groff-\[la]version\[ra]-current.diff.gz
.ft P
.RE

Assuming that groff-\[la]version\[ra].tar.gz and
groff-\[la]version\[ra]-current.diff.gz are in the same directory, do the
following to apply the diff file:

.RS
.nf
.ft C
tar xzvf groff-\[la]version\[ra].tar.gz
cd groff-\[la]version\[ra]
gunzip -c ../groff-\[la]version\[ra]-current.diff.gz | patch -p1
.ft P
.fi
.RE

Please report bugs using the form in the file BUG-REPORT; the idea of this
is to make sure that FSF has all the information it needs to fix the bug.
At the very least, read the BUG-REPORT form and make sure that you supply
all the information that it asks for.
Even if you are not sure that something is a bug, report it using
BUG-REPORT: this will enable us to determine whether it really is a bug or
not.

Three mailing lists are available:

.ULS
.LI
.MTO bug-groff@gnu.org bug-groff@gnu.org
for reporting bugs

.LI
.MTO groff@gnu.org groff@gnu.org
for general discussion of groff

.LI
.MTO groff-commit@ffii.org groff-commit@ffii.org
a read-only list showing logs of commitments to the CVS repository
.ULE

Note that groff@gnu.org is an alias for
.MTO groff@\:ffii.org groff@\:ffii.org ;
you must be subscribed to the `groff' list to send mails.

To subscribe, send e-mail to \[la]list\[ra]-request@\[la]domain\[ra]
(example:
.MTO groff-request@\:ffii.org groff-request@\:ffii.org )
with the word `subscribe' in either the
subject or body of the e-mail (don't include the quotes).

GNU groff was written by
.MTO jjc@\:jclark.com "James Clark" .
It is now maintained by
.MTO Ted.Harding@\:nessie.mcc.ac.uk "Ted Harding"
and
.MTO wl@\:gnu.org "Werner Lemberg" .
.
.
.SH
NEWS VERSION 1.18.1
.
.SH 2
troff

.ULS
.LI
The non-slanted PostScript font definition files have been regenerated to
include left and right italic correction values.
Applying those to a glyph (this is, prepending the glyph with `\e,' and
appending `\e/' to the glyph) sets the glyph width to the real value given
by the horizontal bounding box values.
Without those escapes, the advance width for the particular glyph is used
(which can differ considerably).

Most users will neither need this feature nor notice a difference in
existing documents (provided \e, and \e/ is used as advertised, namely for
italic fonts only); its main goal is to improve image generation with
grohtml.

This is an experimental change, and feedback is welcome.
.ULE
.
.SH 2
tbl

.ULS
.LI
Added global option `nospaces' to ignore leading and trailing spaces in data
items.
.ULE
.
.SH 2
grolbp

.ULS
.LI
The option \-w (\-\-linewidth) has been added (similar to other device
drivers) to set the default line width.
.ULE
.
.SH 2
grn

.ULS
.LI
Support for b-spline and Bezier curves has been added.
.ULE
.
.SH 2
groffer

.ULS
.LI
New option \-\-shell to select the shell under wich groffer shall run.
.ULE
.
.SH 2
Macro Packages

.ULS
.LI
The string `Am' (producing an ampersand) has been added to mdoc for
compatibility with NetBSD.

.LI
`.IX' is now deprecated for mom; you should use `.IQ' (Indent Quit)
instead.

.LI
In mom, new inlines `FWD', `BCK', `UP', and `DOWN' deal with horizontal
and vertical movements; please refer to contrib/mom/NEWS for more
details.

.LI
New macro ENDNOTES_HDRFTR_CENTER for mom to better control headers.
.ULE
.
.SH 2
Miscellaneous

.ULS
.LI
The `papersize' keyword in the DESC file now accepts multiple arguments.
It is scanned from left to the right, and the first valid argument is used.
This makes it possible to provide a fallback paper size.

Example:
.RS
.ft C
papersize /etc/papersize a4
.ft P
.RE

.LI
A local font directory has been prepended to the default font path; it
defaults to /usr/local/share/groff/site-font.  Similar to the normal
font searching process, files must be placed into a dev\fIXXX\fP
subdirectory, e.g.

.RS
.ft C
/usr/local/share/groff/site-font/devps/FOO
.ft P
.RE

for a PostScript font definition file FOO.
.ULE
.
.
.SH
NEWS VERSION 1.18

This section describes recent user-visible changes in groff.
Bug fixes are not described.
There are more details in the man pages.

.ad c
\s[+5]\m[red]\
Please read the changes below regarding
.ie '\*[.T]'html' \
.  URL #grotty grotty ,
.el \
.  nop grotty,
groff's tty frontend.\
\m[]\s[0]
.br
.ad n
.
.SH 2
Troff

.ULS
.LI
Color support has been added to troff and pic (and to the device drivers
grops, grodvi, grotty, and grohtml -- other preprocessors and drivers will
follow).
A new function `defcolor' defines colors; the escape sequence `\em' sets the
drawing color, the escape sequence `\eM' specifies the background color for
closed objects created with \eD'.\|.\|.' commands.
`\em[]' and `\eM[]' switch back to the previous color.
`\em' and `\eM' correspond to the new troff output command sets starting
with `m' and `DF'.
The device-specific default color is called `default' and can't be
redefined.

Use the `color' request to toggle the usage of colors (default is on); the
read-only register `.color' is\~0 if colors are not active, and non-zero
otherwise.

The old `Df' output command is mapped onto `DFg'; all color output
commands don't change the current font position (consequently, `Df'
doesn't either).

Outputting color can be disabled in troff and groff with the option\~\-c
(it is always disabled in compatibility mode).
See the section on grotty for the
.ie '\*[.T]'html' \
.  URL #GROFF_NO_SGR GROFF_NO_SGR
.el \
. nop GROFF_NO_SGR
environment variable also.

For defining color components as fractions between 0 and\~1, a new scaling
indicator `f' has been defined: 1f\~=\~65536u.
For testing whether a color is defined (with .if and .ie), a new
conditional operator `m' is available.

More details can be found in the groff_diff.7 manual page and in
groff.texinfo.

.LI
Similar to \em and \eM, \ef[] switches back to the previous font.
\efP (and \ef[P]) is still valid for backwards compatibility.

.LI
The new escape \eF is the same as `.fam'; \eF[] switches back to previous
family -- \eF[P] selects family `P'.

.LI
Two new glyph symbols are available: `eu' is the official Euro symbol;
`Eu' is a font-specific glyph variant.

.LI
The new glyph symbols `t+\-', `tdi', and `tmu' are textual variants of
`+\-', `di', and `mu', respectively.

.LI
Latin-1 character 181 (PS name `mu', Unicode name U+00B5 MICRO SIGN) has
got the troff glyph name `mc'.

.LI
\-Tutf8 is now available on EBCDIC hosts.

.LI
Strings can take arguments, using this syntax: \e*[foo\~arg1\~arg2\~.\|.\|.].
Example:
.RS
.nf
.ft C
\&.ds xxx This is a \e\e$1 test.
\e*[xxx nice]
.ft P
.fi
.RE

.LI
It is now possible to have whitespace between the first and second dot (or
the name of the ending macro) to end a macro definition.
Example:
.RS
.nf
.ft C
\&.de !
\&..
\&.
\&.de foo
\&.  nop Hello, I'm `foo'.
\&.  nop I will now define `bar'.
\&.  de bar !
\&.    nop Hello, I'm `bar'.
\&.  !
\&..
.ft P
.fi
.RE

.LI
`.fn' is a new string-valued register which returns the (internal) real
font name; styles and families are properly concatenated.

.LI
Three new read/write registers `seconds', `minutes', and `hours' contain
the current time, set at start-up of troff.
Use the `af' request to control their output format.

.LI
The new request `fchar' can be used to provide fallback characters.
It has the same syntax as the `char' request; the only difference is that
a character defined with `.char' hides the glyph with the same name in the
current font, whereas a character defined with `.fchar' is checked only if
the particular glyph isn't found in the current font.
This test happens before checking special fonts.

.LI
In analogy to the `tmc' request, `.writec' is the same as `.write' but
doesn't emit a final newline.

.LI
The new request `itc' is a variant of `.it' for which a line interrupted
with \ec counts as one input line.

.LI
Two new requests `ds1' and `as1' which are similar to `ds' and `as' but
with compatibility mode disabled during expansion of strings defined by
them.

.LI
The syntax of the `substring' request has been changed:
The first character in a string now has index\~0, the last character
has index\~\-1.
Note that this is an incompatible change.

.LI
To emit strings directly to the intermediate output, a new `output'
request has been added; it is similar to `\e!' used at the top level.

.LI
`.hpf' has been extended.
It can now handle most TeX hyphenation pattern files without
modification.
To do that, the commands \epatterns, \ehyphenation, and \eendinput are
recognized.
Please refer to groff_diff.7 for more information.

.LI
`hpfcode' is a new request to provide an input encoding mapping for the
`hpf' request.

.LI
The new request `hpfa' appends hyphenation patterns (`hpf' replaces
already existing patterns).

.LI
A new request `ami' (append macro indirect) has been added.
The first and second parameter of `ami' are taken from string registers
rather than directly; this very special request is needed to make
`trace.tmac' independent from the escape character (which might even
be disabled).

.LI
The new request `sizes' is similar to the `sizes' command in DESC files.
It expects the same syntax; the data must be on a single line, and the
final `0' can be omitted.

.LI
`trin' (translate input) is a new request which is similar to `tr' with
the exception that the `asciify' request will use the character code (if
any) before the character translation.
Example:
.RS
.nf
.ft C
\&.trin ax
\&.di xxx
a
\&.br
\&.di
\&.xxx
\&.trin aa
\&.asciify xxx
\&.xxx
.ft P
.fi
.RE

The result is `x\~a'.  Using `tr', the result would be `x\~x'.

.LI
The request `pvs' isn't new, but hasn't been documented before.
It adds vertical space after a line has been output.
This makes it an alternative to the `ls' request to produce
double-spaced documents.
The read-only register `.pvs' holds the current amount of the
post-vertical line space.

.LI
For compatibility with plan 9's troff, multiple `pi' requests are
supported:

.RS
.nf
.ft C
\&.pi foo
\&.pi bar
.ft P
.fi
.RE

is now equivalent to
.RS
.ft C
\&.pi foo | bar
.ft P
.RE

.LI
A new escape sequence `\eO' is available to disable and enable glyph
output.
Please see groff_diff.man and groff.texinfo for more details.

.LI
The escapes `\e%', `\e&', `\e)', and `\e:' no longer cause an error in \eX;
they are ignored now.
Additionally `\e\ ' and `\e~' are converted to single space characters.

.LI
The default tab distance in nroff mode is now 0.8i to be compatible
with UNIX troff.

.LI
Using the latin-1 input character 0xAD (soft hyphen) for the `shc'
request was a bad idea.
Instead, it is now translated to `\e%', and the default hyphenation
character is again \e[hy].
Note that the glyph \e[shc] is not useful for typographic purposes;
it only exists to have glyph names for all latin-1 characters.
.ULE
.
.SH 2
Macro Packages

.ULS
.LI
.MTO df191@\:ncf.ca "Peter Schaffter"
has contributed a new major macro package called `mom', mainly for
non-scientific writers, which takes care of many typographic issues.
It comes with a complete reference (in HTML format) and some examples.
`mom' has been designed to format documents for PostScript output only.

.LI
Two macros `AT' (AT&T) and `UC' (Univ. of California) have been added to
the man macros for compatibility with older BSD releases.

.LI
Both the man and mdoc macro packages now use the LL and LT registers for
setting the line and title length, respectively (similar to those
registers in the ms macro package).
If not set on the command line or in a macro file loaded before the macro
package itself, they default to 78n in nroff mode and 6.5i in troff mode.

.LI
The `\-xwidth' specifier in the mdoc macro package has been removed.
Its functionality is now integrated directly into `\-width'.
Similarly, `\-column' has been extended to has this functionality also.

.LI
A new macro `Ex' has been added to the mdoc macro package to document an
exit status.

.LI
`troff.man' has been split.  Differences to UNIX troff are now documented
in the new man page `groff_diff.man'.

.LI
The PSPIC macro has been extended to work with DVI output (`pspic.tmac' is
now automatically loaded for \-Tdvi), using a dvips special to load the EPS
file.

.LI
The trace.tmac package now traces calls to `am' also.
Additionally, it works in compatibility mode.

.LI
`troff.1' has been split.
Differences to UNIX troff are now documented in the new man page
`groff_diff.7'.

.LI
`groff_mwww.7' has been renamed to `groff_www.7'.
The file mwww.tmac has been removed.

.LI
`groff_ms.7' has been completely rewritten.
It now contains a complete reference to the ms macros.

.LI
`groff_trace.7' documents the trace macro package.

.LI
Changes in www.tmac

Note that HTML support is still in alpha change, so it is rather likely
that both macro names and macro syntax will change.
Some of the macros mentioned below aren't really new but haven't been
documented properly before.

.ULS
.LI
The following macros have been renamed:
.RS
.nf
MAILTO -> MTO
IMAGE  -> IMG
LINE   -> HR
.fi
.RE

.LI
For consistency, the macros `URL', `FTL', and `MTO' now all have the
address as the first parameter followed by the description.

.LI
By default, grohtml generates links to all section headings at the top
of the document.
Use the new `LK' macro to specify a different place.

.LI
For specifying the background color and a background image, use the
new macros `BCL' and `BGIMG', respectively.

.LI
The macro `NHR' has been added; it suppresses the generation of top and
bottom rules which grohtml emits by default.

.LI
The new macro `HX' determines the cut-off point for automatic link
generation to headings.

.LI
The image position parameter names in `IMG' have been changed to `\-L',
`\-R', and `\-C'.

.LI
New macro `PIMG' for inclusion of a PNG image (it will automatically
convert it into an EPS file if not \-Thtml is used).

.LI
New macro `MPIMG' for putting a PNG image into the left or right margin
(it will automatically convert it into an EPS file if not \-Thtml is used).

.LI
New macros `HnS', `HnE' to start and end a header line block.

.LI
New macro `DC' to produce dropcap characters.

.LI
New macro `HTL' to generate an HTML title line only but no H1 heading.

.LI
New macros `ULS' and `ULE' to start and end an unordered list.
The new macro `LI' inserts a list item.
.ULE
.ULE
.
.
.SH 2
groff

.ULS
.LI
The new command line `\-c' disables color output (which is always disabled
in compatibility mode).
.ULE
.
.
.SH 2
Nroff

.ULS
.LI
Two new command line options `\-c' and `\-C'; the former passes `\-c' to
grotty (switching to the old output scheme); the latter passes `\-C' to
groff (enabling compatibility mode).
.ULE
.
.
.SH 2
pic

.ULS
.LI
New keywords `color' (or `colour', `colored', `coloured'), `outline' (or
`outlined'), and `shaded' are available.
`outline' sets the color of the outline, `shaded' the fill color, and
`color' sets both.
Example:
.RS
.ft C
circle shaded "green" outline "black" ;
.ft P
.RE

Filled arrows always use the outline color for filling.

Color support for TeX output is not implemented yet.
.ULE
.
.
.SH 2
Pic2graph

.ULS
.LI
A new script contributed by
.MTO esr@\:thyrsus.com "Eric S.\~Raymond" .
It converts a PIC diagram into a cropped image.
Since it uses gs and the PNM library, virtually all graphics formats
are available for output.
.ULE
.
.
.SH 2
Eqn2graph

.ULS
.LI
A new script contributed by
.MTO esr@\:thyrsus.com "Eric S.\~Raymond" .
It converts an EQN diagram into a cropped image.
Since it uses gs and the PNM library, virtually all graphics formats
are available for output.
.ULE
.
.
.SH 2
Groffer

.ULS
.LI
A new script contributed by
.MTO bwarken@mayn.de "Bernd Warken" .
It displays groff files and man pages on X and tty, taking care of most
parameters automatically.
.ULE
.
.
.SH 2
Grog

.ULS
.LI
Documents using the mom macro package are recognized.
.ULE
.
.
.SH 2
grops

.ULS
.LI
Color support has been added.

.LI
A new option `\-p' is available to select the output paper size.
It has the same syntax as the new `papersize' keyword in the DESC file.
.ULE
.
.
.SH 2
Grodvi

.ULS
.LI
By default, font sizes are now available in the range 5\-10000pt, similar
to PS fonts.
If you want the old behaviour (i.e., font sizes at discrete values only),
insert the following at the start of your document:
.RS
.nf
.ft C
\&.if '\e*[.T]'dvi' \e
\&.  sizes 500 600 700 800 900 1000 1095 1200 1400 1440 1600 \e
\&         1728 1800 2000 2074 2200 2400 2488 2800 3600
.ft P
.fi
.RE

.LI
A new font file HBI (using cmssbxo10; this is slanted sans serif bold
extended) has been added.

.LI
Two font families are now available: `T' and `H'.

.LI
EC and TC fonts have been integrated.
Use `\-mec' (calling the file ec.tmac) to switch to them.
Those fonts give a much better coverage of the symbols defined by groff
than the CM fonts.

Note that ec.tmac must be called before any language-specific files; it
doesn't take care of hcode values.

.LI
Color support has been added.
For drawing commands, colors are translated to gray values currently.
.ULE
.
.
.TAG grotty
.SH 2
Grotty

.ULS
.LI
Color support has been added, using the SGR (ISO\~6429, sometimes called
ANSI color) escape sequences.

.LI
SGR escape sequences are now used by default for underlining and bold
printing also, no longer using the backspace character trick.
To revert to the old behaviour, use the `\-c' switch.

Note that you have to use the `\-R' option of `less' to make SGR escapes
display correctly.
On the other hand, terminal programs and consoles like `xterm' which
support SGR sequences natively can directly display the output of grotty.
Consequently, the options `\-b', `\-B', `\-u', and `\-U' work only in
combination with `\-c' and are ignored silently otherwise.

For the `man' program, it may be necessary to add the `\-R' option of
`less' to the $PAGER environment variable; alternatively, you can use
`man's `\-P' option (or adapt its configuration file accordingly).
See man(1) for more details.

.TAG GROFF_NO_SGR
.LI
If the environment variable GROFF_NO_SGR is set, SGR output is disabled,
reverting to the old behaviour.

.LI
A new special \eX'tty:\~sgr\~n' has been added; if n is non-zero or missing,
enable SGR output (the default).

.LI
If the new option `\-i' is used (only in SGR mode), grotty sends escape
sequences to set the italic font attribute instead of the underline
attribute for italic fonts.
Note that many terminals don't have support for this (including xterm).
.ULE
.
.
.SH 2
grohtml

.ULS
.LI
Color support for glyphs has been added.

.LI
New option `\-h' to select the style of headings in HTML output.

.LI
New option `\-b' to set the background colour to white.

.LI
New options `\-a' and `\-g' to control the number of bits for anti-aliasing
used for text and graphics, respectively.
Default value is\~4; 0\~means no anti-aliasing.

.LI
groff character/glyph entities now map onto HTML\~4 character entities.
.ULE
.
.
.SH 2
Grolbp

.ULS
.LI
Valid paper sizes are now specified as with the new `papersize' keyword
in the DESC file.
Specifically, the old custom paper type format `custAAAxBBB' is no longer
supported.
.ULE
.
.
.SH 2
Miscellaneous

.ULS
.LI
A new manual page `ditroff.7' is available.

.LI
The groff texinfo manual will now be installed, together with a bunch
of examples.

.LI
A new keyword `papersize' has been added to the DESC file format.
Its argument is either

.RS
.IP \[bu]
a predefined paper format (e.g. `A4' or `letter')

.IP \[bu]
a file name pointing to a file which must contain a paper size
specification in its first line (e.g. `/etc/papersize')

.IP \[bu]
a custom paper size definition like `35c,4i'
.RE

See groff_font(5) for more details.
This keyword only affects the physical dimensions of the output medium;
grops, grolj4, and grolbp use it currently.
troff completely ignores it.
.ULE
.
.HR
.
\s-2\fIThis document was produced using
.URL http://groff.ffii.org/ groff-\n[.x].\n[.y].\n[.Y] .
.br
The image at the top has been contributed by Imogen Mulley (born 1991),
based on a similar picture found on the
.URL http://www.gnu.org "GNU server" .
.HR
.\" EOF
