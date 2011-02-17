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
.
.HEAD "<link rel=""stylesheet"" type=""text/css"" href=""groff.css"" >"
.\" .HEAD "<link rel=""icon"" type=""image/png"" href=""http://www.gnu.org/graphics/gnu-mini.png"" >"
.
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
.NHR
.HTL
.
Home of Groff (GNU Troff).
.
.
.HTML-IMAGE-LEFT
.PSPIC -L gnu.eps 2i 2i
.ie r ps4html \
.  nop \v'-0.66i'\h'2.0i'\s[100]\*[GNU] Troff\s0
.el \
.  nop \v'-0.66i'\h'2.0i'\s[60]\*[GNU] Troff\s0
.HTML-IMAGE-END
.
.HR
.HnS 1
GNU Troff (\c
.URL http://\:groff.ffii.org/ Groff )
\[em] a
.URL http://\:www.gnu.org/ GNU
project.
.HnE
.HR
.LK
.
.LP
.DC T HE mydarkred
groff (GNU Troff) software
is a typesetting package which reads plain text mixed with
formatting commands and produces formatted output.
Groff now supports HTML.
.
.
.SH
Download

The source code of the currently released versions of groff is
available at the 
.URL "http://\:groff.ffii.org/\:groff/" "FFII host (Germany)" ,
.URL "ftp://\:ftp.gnu.org/\:gnu/\:groff/" "GNU host (USA)" ,
and its mirrors.
.
The USA site also contains older, obsolete versions.

The most actual pre-release, development version is available from a
CVS repository, see below.
.
Development snapshots (produced twice a day from the CVS repository)
can be downloaded from
.URL "http://\:groff.ffii.org/\:groff/\:devel" here .

For a special version of groff on the Microsoft operating systems, see
.URL "http://\:gnuwin32.sf.net/\:packages/\:groffl.htm" "Groff for Windows" .

GNU troff is released under the
.URL http://\:www.gnu.org/\:copyleft/\:gpl.html "GNU Copyright License" .

User issues lead:
.MTO Ted.Harding@\:nessie.mcc.ac.uk "Ted Harding" .
.br
Technical issues lead:
.MTO wl@\:gnu.org "Werner Lemberg" .
.
.
.SH
README

This is the GNU groff document formatting system.
The version number is given in the file VERSION.

Included in this release are implementations of troff, pic, eqn, tbl,
grn, refer, \-man, \-mdoc, and \-ms macros, and drivers for PostScript,
TeX dvi format, HP LaserJet\~4 printers, Canon CAPSL printers, HTML format
(beta status), and typewriter-like devices.
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

The most recent released version of groff is always available by
.URL "ftp://\:ftp.gnu.org/\:gnu/\:groff" "anonymous ftp" .

The current development version of groff is available from a
.URL "http://\:savannah.gnu.org/\:cvs/\:?group=groff" "CVS repository" .
You can access it by first selecting a parent directory in
which to create a working copy (call it, say, ~/cvswork), and then
executing the commands

.in 2m
.nf
.ft C
cd ~/cvswork
CVS_RSH=ssh; export CVS_RSH
cvs -d:ext:anoncvs@savannah.gnu.org/cvsroot/groff -z5 co groff
.ft P
.fi
.in

(Note that you need an SSH client for security reasons.)

This will create a subdirectory, ~/cvswork/groff, with a `checked out'
copy of the CVS repository.
An update of this working copy may be
achieved, at any later time by invoking the commands

.in 2m
.nf
.ft C
cd ~/cvswork/groff
CVS_RSH=ssh cvs -z5 update -dP
.ft P
.fi
.in

Please read the CVS info pages for further details.

Finally, it is possible to access the CVS with a
.URL http://\:savannah.gnu.org/\:cvs/\:?group=groff "web browser" .

Alternatively, you can download snapshots (which are updated twice a day).
The complete groff source as a single file is available
.URL http://\:groff.ffii.org/\:groff/\:devel/\:groff-current.tar.gz here .

A diff file relative to groff-\[la]version\[ra], the latest official groff
release is available at

.in 2m
.ft C
http://groff.ffii.org/groff/devel/groff-\[la]version\[ra]-current.diff.gz
.ft P
.in

Assuming that groff-\[la]version\[ra].tar.gz and
groff-\[la]version\[ra]-current.diff.gz are in the same directory, do the
following to apply the diff file:

.in 2m
.nf
.ft C
tar xzvf groff-\[la]version\[ra].tar.gz
cd groff-\[la]version\[ra]
gunzip -c ../groff-\[la]version\[ra]-current.diff.gz | patch -p1
.ft P
.fi
.in

Depending on your requirements, you may need at least some of the
following tools to build groff directly from its source:

.in 2m
.nf
ghostscript
the psutils package
the netpbm package
texinfo 4.8
bison \[>=] 1.875b or byacc
.fi
.in

Note that texinfo and bison or byacc are required only for building from
CVS sources (either a checked out working copy, or a daily snapshot).
They are not required for building from a stable release tarball.
Also note that the version numbers stated are the minimum supported.
No version of texinfo < 4.8 will work, and the original release of bison
1.875 is known not to work; you \fImay\fP find that bison releases < 1.875
will work, but in case of difficulty, please update to a later version
\fIbefore\fP posting a bug report.

For \fIall\fP sources, you need ghostscript for creation of either PDF or
HTML output; the netpbm and psutils packages are required only for HTML
output.
If you don't intend to produce output in either of these formats, then
these packages are unnecessary.

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
.MTO groff-commit@gnu.org groff-commit@gnu.org
a read-only list showing commitments to the CVS repository
.ULE

You can post mails directly to the `bug-groff' list, without subscribing;
to post mails to the `groff' list you must subscribe to it.

To subscribe, send e-mail to \[la]list\[ra]-request@\[la]domain\[ra]
(example:
.MTO groff-request@\:gnu.org groff-request@\:gnu.org
for the `groff' list) with the word `subscribe' in either the
subject or body of the e-mail (don't include the quotes).
Alternatively, you may subscribe by visiting the web pages at

.in 2m
.nf
.URL http://lists.gnu.org/mailman/listinfo/bug-groff
.URL http://lists.gnu.org/mailman/listinfo/groff
.URL http://lists.gnu.org/mailman/listinfo/groff-commit
.fi
.in

Each of these web pages also provides a link to a browseable archive of
postings to the corresponding mailing list.

GNU groff was written by
.MTO jjc@\:jclark.com "James Clark" .
It is now maintained by
.MTO Ted.Harding@\:nessie.mcc.ac.uk "Ted Harding"
and
.MTO wl@\:gnu.org "Werner Lemberg" .
.
.
.SH
NEWS VERSION 1.19.2
.
.SH 2 troff

.ULS
.LI
Analogously to the .ft and \ef pair, two new requests `gcolor' and
`fcolor' (which pair with \em and \eM, respectively) have been added to
set the glyph and background colours.

.LI
A new read-only, string-valued register `.sty' returns the name of the
current style.

.LI
Two new conditional operators `F\~\[la]name\[ra]' and `S\~\[la]name\[ra]'
have been added.
`F' is true if a font \[la]name\[ra] exists.
`S' is true if a style \[la]name\[ra] has been registered.

.LI
Cyrillic characters have been added to the `utf8' and `html' output
devices.
.ULE
.
.
.SH 2
pic

.ULS
.LI
The `by' argument in a `for' loop can now be negative if it is additive.
For the multiplicative case, it must be greater than zero.
.ULE
.
.
.SH 2
eqn

.ULS
.LI
The following keywords aren't new but haven't been documented previously:

.in 2m
\fBundef NAME\fP (to undefine a macro)
.br
\fBcopy "FILE"\fP (a synonym for `include')
.br
\fBspace n\fP (to modify the vertical spacing before and after an equation)
.in

.LI
The following macros aren't new but haven't been documented previously:

.in 2m
\fBAlpha, .\|.\|., Omega\fP (the same as `ALPHA', .\|.\|., `OMEGA')
.br
\fBldots\fP (three dots on the base line)
.br
\fBdollar\fP (a dollar glyph)
.in

.LI
The following keywords have been extended.  Again, this isn't new but
hasn't been documented previously:

.in 2m
.ft B
col n { .\|.\|. }
.br
lcol n { .\|.\|. }
.br
rcol n { .\|.\|. }
.br
ccol n { .\|.\|. }
.br
pile n { .\|.\|. }
.br
lpile n { .\|.\|. }
.br
rpile n { .\|.\|. }
.br
cpile n { .\|.\|. }
.ft P
(set vertical spacing between rows to\~N)
.ULE
.
.
.SH 2
grohtml

.ULS
.LI
This device driver has been raised to beta stage; its set of tags should
be stable now.

.LI
New command line option `\-s' to set the base point size.

.LI
New command line option `\-S' to set the split level while generating
multiple files.
.ULE
.
.
.SH 2
grotty

.ULS
.LI
Experimental support for zero-width and double-width characters.
.ULE
.
.
.SH 2
gxditview

.ULS
.LI
On platforms which have the X\~Window System this program is now built and
installed automatically.
.ULE
.
.
.SH 2
xtotroff

.ULS
.LI
This program to create font definition files for xditview isn't new but
hasn't been installed previously.
.ULE
.
.
.SH 2
groffer

.ULS
.LI
A security problem (reported as CAN-2004-0969) has been fixed.
.ULE
.
.
.SH 2
gdiffmk

.ULS
.LI
A new script contributed by Mike Bianchi.
It compares two groff, nroff, or troff documents and creates an output with
added margin characters (using `.mc') to indicate the differences.
.ULE
.
.
.SH 2
pdfroff

.ULS
.LI
A new wrapper script contributed by Keith Marshall to easily create PDF
documents with groff.
.ULE
.
.
.SH 2
macro packages

.ULS
.LI
ms.tmac

.ULS
.LI
Support for fractional point sizes: A value for the `PS', `VS', `FPS',
and `VPS' register larger than or equal to 1000 is always divided by 1000.
For example, `.nr\~PS\~10250' sets the document's font size to 10.25 points.

.LI
The `Ds' and `De' macros provided in ms since groff version 1.19 have
been removed; the equivalent `DS' and `DE' macros should be used instead.
X11 documents which actually use `Ds' and `De' always load a
specific macro file from the X11 distribution (`macros.t') which
provides proper definitions for the two macros.

.LI
The following registers have been added for improving layout control:

.IP PORPHANS
Defines number of lines following `LP', `PP', `QP', `IP' or `XP' which
must be kept together, before any automatic page break.

.IP HORPHANS
Sets number of lines of following paragraph which must be kept with a
heading, defined by `NH' or `SH', before any automatic page break.

.IP GROWPS
Sets the first level of heading (set with `NH') which will keep the
same point size as body text.

.IP PSINCR
Sets the point size increment for each level of heading (set with
`NH'), below the threshold level set by `GROWPS'; e.g., if
\en[PS]\~=\~10, \en[GROWPS]\~=\~3 and \en[PSINCR]\~=\~2.0p, then `.NH\~1'
will produce 14pt headings, `.NH\~2' will produce 12pt, and all other levels
will remain at 10pt (because \en[PS]\~=\~10).

.LI
The `SH' macro now accepts a numeric argument, to make heading size
match that of `NH' with same argument value when the `GROWPS'/`PSINCR'
feature is enabled.
.ULE

Please refer to the documentation of the ms package for other, minor
improvements.

.LI
me.tmac

The section type set with the `++' request is available in the `_M'
register.
This isn't new but hasn't been documented before.

.LI
www.tmac

The `HR' macro no longer causes an empty line for non-HTML devices.

A new macro `HEAD' has been added to directly add data to the
<head>...</head> block.

New macros `OLS' and `OLE' to start and end an ordered list.

New macros `DLS' and `DLE' to start and end a definition list.
.ULE
.
.
.SH 2
pdfmark

.ULS
.LI
A new macro package contributed by Keith Marshall which implements PDF
marks.
This is in alpha stage currently.
.ULE
.
.
.SH 2
miscellaneous

.ULS
.LI
Two new keywords to the DESC file have been added which are needed for
grohtml: `image_generator' and `unscaled_charwidths'.
The former gives the name of the program which creates PNG images,
and the latter makes troff always use unscaled character widths.
.ULE
.
.
.SH
NEWS VERSION 1.19.1
.
.SH 2
groff

.ULS
.LI
The argument of the command line option `\-I' is now also passed to troff
and grops, specifying a directory to search for files on the command line,
files named in `so' and `psbb' requests, and files named in \eX'ps:\~file'
and \eX'ps:\~import' escapes.

.LI
If option `\-V' is used more than once, the commands will be both printed
on standard error and run.
.ULE
.
.
.SH 2
troff

.ULS
.LI
Two new read-only, string-valued registers `.m' and `.M' return the
name of the current drawing and background color, respectively.

.LI
New read-only register `.U' which is set to\~1 if in safer mode and set
to\~0 if in unsafe mode.

.LI
An input encoding file for \%latin-5 (a.k.a.\& ISO \%8859-9) has been
added.
Example use:

.in 2m
.ft C
groff \-Tdvi \-mlatin5 my_file > my_file.dvi
.ft R
.in

Note that some output devices don't support all glyphs of this encoding.

.LI
If the `return' request is called with an argument, it exits twice,
namely the current macro and the macro one level higher.
This is used to define a wrapper macro for `return' in trace.tmac.

.LI
For completeness, two new requests have been added: `dei1' and `ami1'.
They are equivalent to `dei' and `ami', respectively, but the macros
are executed with compatibility mode off (similar to `de1' and `am1').

.LI
New command line option `\-I' to specify a directory for files (both
those on the command line and those named in `psbb' requests).
This is also handled by the groff wrapper program.

.LI
Since version 1.19 you can say `.vs\~0'.
Older versions emit a warning and convert this to `.vs\~\en[.V]'.

This hasn't been documented properly.
Note that `.vs\~0' isn't saved in a diversion since it doesn't result
in vertical motion.
.ULE
.
.
.SH 2
pic

.ULS
.LI
Dashed and dotted ellipses have been implemented.
.ULE
.
.
.SH 2
tbl

.ULS
.LI
New key character `x' to make tbl call a user-defined macro on a table
cell.
Patch by
.MTO hj.oertel@\:surfeu.de "Heinz-J\[:u]rgen Oertel" .
.ULE
.
.
.SH 2
grap2graph

.ULS
.LI
A new script contributed by
.MTO esr@\:thyrsus.com "Eric S. Raymond" .
It converts a grap diagram into a cropped image.
Since it uses gs and the PNM library, virtually all graphics formats
are available for output.
[Note that the grap program itself isn't part of the groff package;
see the file MORE.STUFF how to obtain grap.]
.ULE
.
.
.SH 2
grohtml

.ULS
.LI
New option `\-j' to emit output splitted into multiple files.
.ULE
.
.
.SH 2
grops

.ULS
.LI
New command line option `\-I' to specify a directory to search for files
on the command line and files named in \eX'ps:\~import' and \eX'ps:\~file'
escapes.
This is also handled by the groff wrapper program.

.LI
The default value for the `broken' keyword in the DESC file is now\~0.
.ULE
.
.
.SH 2
grolj4

.ULS
.LI
A new man page `lj4_font(5)' documents how fonts are accessed with
grolj4.

.LI
The built-in fonts for LJ4 and newer PCL\~5 devices have been completely
revised, mainly to access as much glyphs as possible.
The provided metric files should be compatible with recent PCL\~5 printers
also.
Additionally, font description files have been added for the Arial and
Times New Roman family, the MS symbol, and Wingdings fonts.
.ULE
.
.
.SH 2
afmtodit

.ULS
.LI
New option `\-x' to prevent use of built-in Adobe Glyph List.
.ULE
.
.
.SH 2
hpftodit

.ULS
.LI
Completely revised to handle HP TrueType metric files also.
See the hpftodit manual page for more details.
.ULE
.
.
.SH 2
groffer

.ULS
.LI
This version is a rewrite of groffer in many parts, but it is kept in
the old single script style.

.ULS
.LI
New options: \-\-text (\-\-mode\~text), \-\-tty-viewer, \-\-X (\-\-mode\~X),
\-\-X-viewer, \-\-html (\-\-mode\~html), \-\-html-view, \-\-apropos-data,
\-\-apropos-devel, \-\-apropos-progs.

.LI
New documentation file README_SH.

.LI
Enhancement of the configuration files and the `apropos' handling.
.ULE
.ULE
.
.
.SH 2
macro packages

.ULS
.LI
www.tmac: New macro `JOBNAME' to split output into multiple files.

.LI
In mdoc, multiple calls to `.Lb' are now supported in the LIBRARY
section.
.ULE
.
.
.SH
NEWS VERSION 1.19
.
.SH 2
troff

.ULS
.LI
Input encoding files for \%latin-9 (a.k.a.\& \%latin-0 and ISO \%8859-15)
and \%latin-2 (ISO \%8859-2) have been added.
Example use:

.in 2m
.ft C
groff -Tdvi -mlatin9 my_file > my_file.dvi
.ft P
.in

You still need proper fonts with the necessary glyphs.
Out of the box, the groff package supports \%latin-9 only for \-Tps,
\-Tdvi, and \-Tutf8, and \%latin-2 only for \-Tdvi and \-Tutf8.

.LI
Composite glyphs are now supported.
To do this, a subset of the Adobe Glyph List (AGL) Algorithm as described in
.URL http://partners.adobe.com/asn/tech/type/unicodegn.jsp \
     http://partners.adobe.com/asn/tech/type/unicodegn.jsp
is used to construct glyph names based on Unicode character codes.
The existing groff glyph names are frozen; no glyph names which can't be
constructed algorithmically will be added in the future.

The \e[...] escape sequence has been extended to specify multiple glyph
components.
Example:

.in 2m
.ft C
\e[A ho]
.ft R
.in

this accesses a glyph with the name `u0041_0328'.

Some groff glyphs which are useful as composites map to `wrong' Unicode
code points.
For example, `ho' maps to U+02DB which is a spacing ogonek, whereas a
non-spacing ogonek U+0328 is needed for composite glyphs.
The new request

.in 2m
.ft C
\&.composite from to
.ft R
.in

will change the mapping while a composite glyph name is constructed.
To make \e[A\~ho] yield the expected result,

.in 2m
.ft C
\&.composite ho u0328
.ft R
.in

is needed.
[The new file `composite.tmac' loaded at start-up already
contains proper calls to `.composite'.]

Please refer to the info pages of groff and to the groff_char man page
for more details.

.LI
A new request `fschar' has been added to define font-specific fallback
characters.
They are searched after the list of fonts declared with the
`fspecial' request but before the list of fonts declared with `special'.

.LI
Fallback characters defined with `fschar' can be removed with the
new `rfschar' request.

.LI
A new request `schar' has been added to define global fallback
characters.
They are searched after the list of fonts declared with the
`special' request but before the already mounted special fonts.

.LI
In groff versions 1.18 and 1.18.1, \eD'f\~...'\& didn't move the current
point horizontally.
Despite of being silly, this change has been reverted for backwards
compatibility.
Consequently, the intermediate output command `Df' also moves the 
position horizontally again.

\eD'f\~...'\& is deprecated since it depends on the horizontal resolution
of the output device (given with the `hor' parameter in the DESC file).
Use the new \eD'Fg\~...'\& escape instead.

.LI
For orthogonality, new \eD subcommands to change the fill color are
available:

.in 2m
.ft C
\eD'Fr ...' \f[R](rgb)\f[]
.br
\eD'Fc ...' \f[R](cmy)\f[]
.br
\eD'Fg ...' \f[R](gray)\f[]
.br
\eD'Fk ...' \f[R](cmyk)\f[]
.br
\eD'Fd' \f[R](default color)\f[]
.ft R
.in

The arguments are the same as with the `defcolor' request.
The current position is \f[I]not\f[] changed.

.LI
The values set with \eH and \eS are now available in number registers
\en[.height] and \en[.slant], respectively.

.LI
The `.pe' number register isn't new but hasn't been documented before.
It is set to\~1 during a page ejection caused by the `bp' request.

.LI
The new glyph symbol `tno' is a textual variant of `no'.

.LI
The new glyph symbol `+e' represents U+03F5, GREEK LUNATE EPSILON SYMBOL.
(Well, it is not really new since it has been previously supported by
grolj4.)
The mapping for both the dvi and lj4 symbol font has been
changed accordingly so that Greek small letter epsilon, `*e', has the
same glyph shape as with other devices.
.ULE
.
.
.SH 2
grops

.ULS
.LI
The font `freeeuro.pfa' has been added to provide various default glyph
shapes for `eu' and `Eu'.

.LI
It is now possible to access all glyphs in a Type\~1 font, not only 256
(provided the font file created by afmtodit has proper entries).
grops constructs additional encoding vectors on the fly if necessary.

.LI
The paper size is now emitted via the %%DocumentMedia and PageSize
mechanisms so that it is no longer required to tell `gv' or `ps2pdf'
about the paper size.
The `broken' flag value\~16 omits this feature
(the used PostScript command `setpagedevice' is a LanguageLevel\~2
extension).
Patch by
.MTO egil@\:kvaleberg.no "Egil Kvaleberg" .

.LI
Non-slanted PostScript metrics have been changed again; they no longer
contain negative left italic correction values.
This assures correct spacing with eqn.
.ULE
.
.
.SH 2
grodvi

.ULS
.LI
The font cmtex10 has been added as the special font `SC' to the DVI fonts.
It is used as a font-specific special font for CW and CWI.

.LI
New options \-l and \-p to set landscape orientation and the paper size.
grodvi now emits a `papersize' special which is understood by DVI drivers
like dvips.

Consequently, the DESC file should contain a `papersize' keyword.

.LI
The glyph shapes for \e[*f] and \e[*e] have been exchanged with \e[+f] and
\e[+e], respectively, to be in sync with all other devices.

.LI
Glyphs \e[HE] and \e[DI] have been replaced with \e[u2662] and \e[u2661],
respectively, since the former two glyphs have a black (filled) shape
which grodvi doesn't provide by default (it never has actually).
.ULE
.
.
.SH 2
grolj4

.ULS
.LI
The glyphs \e[*e] and \e[+e] have been exchanged to be in sync with
all other devices.

.LI
The glyph \e[~=] is now called \e[|=].  Similar to other devices, \e[~=]
is now another name for glyph \e[~~].
.ULE
.
.
.SH 2
grotty

.ULS
.LI
New option `\-r'.
It is similar to the \-i option except it tells grotty to
use the `reverse video' attribute to render italic fonts.
.ULE
.
.
.SH 2
pic

.ULS
.LI
New command `figname' to set the name of a picture's output box in TeX
mode.
.ULE
.
.
.SH 2
refer

.ULS
.LI
The environment variable `REFER' to override the name of the default
database isn't new but hasn't been documented before.
.ULE
.
.
.SH 2
soelim

.ULS
.LI
New option `\-r' to avoid emission of `.lf' lines.

.LI
New option `\-t' to emit TeX comment lines (giving current file and the
line number) instead of `.lf' lines.
.ULE
.
.
.SH 2
afmtodit

.ULS
.LI
Unencoded glyphs in an AFM file are output also (since grops can now emit
multiple encoding vectors for a single font).

.LI
New option `\-m' to prevent negative left italic correction values.

.LI
The mapping and encoding file together with file `DESC' are now searched
in the default font directory also.
Please refer to the man page of afmtodit for more details.
.ULE
.
.
.SH 2
macro packages

.ULS
.LI
.MTO kollar@\:alltel.net "Larry Kollar"
and others made the man macros more customizable.

.ULS
.LI
New command line options \-rFT, \-rIN, and \-rSN to set the vertical
location of the footer line, the body text indentation, and the
sub-subheading indentation.

.LI
New command line option \-rHY (similar to the ms macros) to control
hyphenation.

.LI
New macros `.PT' and `.BT' to print the header and footer strings.
They can be replaced with a customized version in `man.local'.

.LI
The string `HF' now holds the typeface to print headings and
subheadings.

.LI
Similar to the ms macros, the LT register now defaults to LL if not
explicitly specified on the command line.
.ULE

.LI
troff's start-up file `troffrc' now includes `papersize.tmac' to set
the paper size with the command line option `-dpaper=<size>'.

Possible values for `<size>' are the same as the predefined `papersize'
values in the DESC file (only lowercase; see the groff_font man page)
except \%a7-d7.
An appended `l' (ell) character denotes landscape orientation.
Examples: `a4', `c3l', `letterl'.

Most output drivers need additional command line switches `\-p' and
`\-l' to override the default paper length and orientation as set in
the driver specific DESC file.

For example, use the following for PS output on A4 paper in landscape
orientation:

.in 2m
.ft C
groff -Tps -dpaper=a4l -P-pa4 -P-l -ms foo.ms > foo.ps
.ft R
.in
.ULE
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

.in 2m
.ft C
papersize /etc/papersize a4
.ft P
.in

.LI
A local font directory has been prepended to the default font path; it
defaults to /usr/local/share/groff/site-font.  Similar to the normal
font searching process, files must be placed into a dev\f[I]XXX\f[]
subdirectory, e.g.

.in 2m
.ft C
/usr/local/share/groff/site-font/devps/FOO
.ft P
.in

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

.in 2m
.nf
.ft C
\&.ds xxx This is a \e\e$1 test.
\e*[xxx nice]
.ft P
.fi
.in

.LI
It is now possible to have whitespace between the first and second dot (or
the name of the ending macro) to end a macro definition.
Example:

.in 2m
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
.in

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

.in 2m
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
.in

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

.in 2m
.nf
.ft C
\&.pi foo
\&.pi bar
.ft P
.fi
.in

is now equivalent to

.in 2m
.ft C
\&.pi foo | bar
.ft P
.in

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

.in 2m
.nf
MAILTO -> MTO
IMAGE  -> IMG
LINE   -> HR
.fi
.in

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

.in 2m
.ft C
circle shaded "green" outline "black" ;
.ft P
.in

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

.in 2m
.nf
.ft C
\&.if '\e*[.T]'dvi' \e
\&.  sizes 500 600 700 800 900 1000 1095 1200 1400 1440 1600 \e
\&         1728 1800 2000 2074 2200 2400 2488 2800 3600
.ft P
.fi
.in

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

.ULS
.LI
a predefined paper format (e.g. `A4' or `letter')

.LI
a file name pointing to a file which must contain a paper size
specification in its first line (e.g. `/etc/papersize')

.LI
a custom paper size definition like `35c,4i'
.ULE

See groff_font(5) for more details.
This keyword only affects the physical dimensions of the output medium;
grops, grolj4, and grolbp use it currently.
troff completely ignores it.
.ULE
.
.HR
.
\s[-2]\f[I]This document was produced using
.URL http://groff.ffii.org/ groff-\n[.x].\n[.y].\n[.Y] .
.br
The image at the top has been contributed by Imogen Mulley (born 1991),
based on a similar picture found on the
.URL http://www.gnu.org "GNU server" .
.HR
.\" EOF
