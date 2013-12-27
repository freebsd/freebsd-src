.\" vim: ft=groff
.CS
Portable Document Format
Publishing with GNU Troff
.AU Keith Marshall
.AI <keith.d.marshall@ntlworld.com>
.CE
.\"
.\" Specify the Internet address for the groff web site.
.\" Currently, there are two available addresses; a copy is maintained at ...
.\"
.ds GROFF-WEBSITE http://www.gnu.org/software/groff
.\"
.\" ... but the official home site is at ...
.\"
.ds GROFF-WEBSITE http://groff.ffii.org
.\"
.\" Set the PDF default document view attribute, to ensure that the document
.\" outline is visible, each time the document is opened in Acrobat Reader.
.\"
.pdfview /PageMode /UseOutlines
.\"
.\" Initialise the outline view to show only three heading levels,
.\" with additional subordinate level headings folded.
.\"
.nr PDFOUTLINE.FOLDLEVEL 3
.\"
.\" Add document identification meta-data
.\"
.pdfinfo /Title     Portable Document Format Publishing with GNU Troff
.pdfinfo /Author    Keith Marshall
.pdfinfo /Subject   Tips and Techniques for Exploiting PDF Features with GNU Troff
.pdfinfo /Keywords  groff troff PDF pdfmark
.\"
.\" Set the default cross reference format to indicate section numbers,
.\" rather than page numbers, when we insert a reference pointer.
.\"
.ds PDFHREF.INFO section \\*[SN-NO-DOT] \\$*
.\"
.\" Define a macro, to print reference links WITHOUT the usual "see" prefix.
.\"
.de XR-NO-PREFIX
.rn PDFHREF.PREFIX xx
.ds PDFHREF.PREFIX
.XR \\$@
.rn xx PDFHREF.PREFIX
..
.\"
.\" Define a string,
.\" to insert a Registered Trade Mark symbol as a superscript.
.\"
.ds rg \*{\(rg\*}
.\"
.\" Establish the page layout.
.\"
.nr PO  2.5c
.nr LL 17.0c
.nr LT 17.0c
.nr HY  0
.nr FF  3
.nr DI  5n
.\"
.\" Generate headers in larger point sizes, for NH levels < 4,
.\" with point size increasing by 1.5p, for each lesser NH level.
.\"
.nr GROWPS 4
.nr PSINCR 1.5p
.\"
.de EM
.\".I "\s'+0.3'\\$1\s0" "\\$2" "\\$3"
.I \\$@
..
.de CWB
\\$5\fC\\$3\fP\f(CB\\$1\fP\fC\\$2\fP\\$4
..
.de CWI
\\$5\fC\\$3\fP\f(CI\\$1\fP\fC\\$2\fP\\$4
..
.de CWBI
\\$5\fC\\$3\fP\f[CBI]\\$1\fP\fC\\$2\fP\\$4
..
.ds = \f(CB\\$1\f(CR\\$4\f[CBI]\\$2\f(CR\\$3
.\"
.NH 1
.\" When we use numbered section headings, we might like to automatically
.\" insert a table of contents entry, using the text of the heading itself.
.\" The "ms" macros don't provide any standard mechanism for doing this,
.\" but "spdf.tmac" adds the "XN" macro, which will do it for us.
.\"
.\" Here's a simple example of how we might use it.  In this case, the word
.\" "Introduction" will appear both in the body of the document, as the text
.\" of the heading, and it will be added to the table of contents, which is
.\" subsequently "printed" using the "TC" macro; in both locations, it will
.\" be prefixed by the section number.
.\"
.\" As an additional side effect, any use of "XN" will cause the table of
.\" contents entry to be automatically reproduced, with the exception of its
.\" page number reference, as a PDF document outline entry.  Thus, the use
.\" of "XN" to specify numbered section headings results in the automatic
.\" creation of a numbered PDF document outline.  This automatic creation
.\" of the outline is completely transparent, and will occur regardless
.\" of whether the "TC" macro is subsequently invoked, or not.
.\"
.XN Introduction
.\"
.\" If using an old s.tmac, without the SN-NO-DOT extension,
.\" make sure we get SOMETHING in section number references.
.\"
.if !dSN-NO-DOT .als SN-NO-DOT SN
.LP
It might appear that it is a fairly simple matter to
produce documents in Adobe\*(rg\~\(lqPortable\~Document\~Format\(rq,
commonly known as PDF, using
.CW groff ) GNU\~Troff\~(
as the document formatter.
Indeed,
.CW groff 's
default output format is the native Adobe\*(rg\~PostScript\*(rg format,
which PDF producers such as Adobe\*(rg Acrobat\*(rg Distiller\*(rg,
or GhostScript, expect as their input format.
Thus, the PDF production process would seem to entail simply
formatting the document source with
.CW groff ,
to produce a PostScript\*(rg version of the document,
which can subsequently be processed by Acrobat\*(rg Distiller\*(rg
or GhostScript, to generate the final PDF document.
.LP
For many PDF production requirements,
the production cycle described above may be sufficient.
However, this is a limited PDF production method,
in which the resultant PDF document represents no more than
an on screen image of the printed form of the document, if
.CW groff 's
PostScript\*(rg output were printed directly.
.LP
The Portable Document Format provides a number of features,
which significantly enhance the experience of reading a document on screen,
but which are of little or no value to a document which is merely printed.
It
.EM is
possible to exploit these PDF features, which are described in the Adobe\*(rg
.\"
.de pdfmark-manual
.\" This is an example of a resource reference specified by URI ...
.\" We may need to refer often to the Adobe pdfmark Reference Manual,
.\" so we create the internet link definition using a macro, to make
.\" it reusable.
.\"
.\" Note also, that we protect the description of the reference by
.\" preceding it with "--", to avoid "invalid character in name" type
.\" error messages from groff (caused by the use of "\~").
.\"
.pdfhref W -D http://partners.adobe.com/asn/acrobat/docs/pdfmark.pdf \
    -P \(lq -A \(rq\\$1 -- pdfmark\~Reference\~Manual
..
.pdfmark-manual ,
with some refinement of the simple PDF production method, provided
appropriate \(lqfeature implementing\(rq instructions can be embedded into
.CW groff 's
PostScript\*(rg rendering of the document.
This, of course, implies that the original document source, which
.CW groff
will process to generate the PostScript\*(rg description of the document,
must include appropriate markup to exploit the desired PDF features.
It is this preparation of the
.CW groff
document source to exploit a number of these features,
which provides the principal focus of this document.
.LP
The markup techniques to be described have been utilised in the production of
the PDF version of this document itself.
This has been formatted using
.CW groff 's
.CW ms
macro package;
thus, usage examples may be found in the document source file,
.CW \n(.F ,
to which comments have been added,
to help identify appropriate markup examples for implementing PDF features,
such as:\(en
.QS
.IP \(bu
Selecting a default document view, which defines how the document will appear
when opened in the reader application; for example, when this document is
opened in Acrobat\*(rg\~Reader, it should display the top of the cover sheet,
in the document view pane, while a document outline should appear to the left,
in the \(lqBookmarks\(rq pane.
.IP \(bu
Adding document identification \(lqmeta\(hydata\(rq,
which can be accessed, in Acrobat\*(rg\~Reader,
by inspecting the \(lqFile\^/\^Document\~Properties\^/\^Summary\(rq.
.IP \(bu
Creating a document outline, which will be displayed in the \(lqBookmarks\(rq
pane of Acrobat\*(rg\~Reader, such that readers may quickly navigate to any
section of the document, simply by clicking on the associated heading
in the outline view.
.IP \(bu
Embedding active links in the body of the document, such that readers may
quickly navigate to related material at another location within the same
document, or in another PDF document, or even to a related Internet resource,
specified by its URI.
.IP \(bu
Adding annotations, in the form of \(lqsticky notes\(rq, at strategic
points within the PDF document.
.QE
.LP
All of the techniques described have been tested on
.EM both
GNU/Linux, and on Microsoft\*(rg Windows\(tm2000 operating platforms, using
.CW groff
.CW 1.19.1 ,\c
.pdfhref L -D footnote1 -- \**
.FS
.pdfhref M footnote1
Later versions should, and some earlier versions may, be equally suitable.
See
.pdfhref W \*[GROFF-WEBSITE]
for information and availability of the latest version.
.FE
in association with
.CW AFPL
.CW GhostScript
.CW 8.14 .\c
.pdfhref L -D footnote2 -- \**
.FS
.pdfhref M footnote2
Again, other versions may be suitable.
See
.pdfhref W http://ghostscript.com
for information and availability.
.FE
Other tools employed, which should be readily available on
.EM any
.SM
UNIX\(tm
.LG
or GNU/Linux system, are
.CW sed ,
.CW awk
and
.CW make ,
together with an appropriate text editor, for creating and marking up the
.CW groff
input files.
These additional utilities are not provided, as standard,
on the Microsoft\*(rg Windows\(tm platform,
but several third party implementations are available.
Some worth considering include the MKS\*(rg\~Toolkit,\**
.FS
A commercial offering; see
.pdfhref W http://mkssoftware.com/products/tk/default.asp
for information.
.FE
Cygwin,\**
.FS
A
.EM free
but comprehensive
.SM
POSIX
.LG
emulation environment and
.SM
UNIX\(tm
.LG
toolkit for 32\(hybit Microsoft\*(rg Windows\(tm platforms; see
.pdfhref W http://cygwin.com
for information and download.
.FE
or MSYS.\**
.FS
Another free, but minimal suite of common
.SM
UNIX\(tm
.LG
tools for 32\(hybit Microsoft\*(rg Windows\(tm, available for download from
.pdfhref W -A ; http://www.mingw.org
it
.EM does 
include those tools listed above,
and is the package which was actually used when performing the Windows\(tm2000
platform tests referred to in the text.
.FE
This list is by no means exhaustive, and should in no way be construed as an
endorsement of any of these packages, nor to imply that other similar packages,
which may be available, are in any way inferior to them.
.bp
.NH 1
.\" We may wish a section heading to represent a named destination,
.\" so that we can create a linked reference to it, from some other 
.\" part of the PDF document, (or even from another PDF document).
.\"
.\" Here we use the "-N" option of the "XN" macro, to create a named
.\" PDF link destination, at the location of the heading.  Notice that
.\" we also use the "--" marker to separate the heading text from the
.\" preceding option specification; it is not strictly necessary in
.\" this case, but it does help to set off the heading text from the
.\" option specification.
.\"
.XN -N pdf-features -- Exploiting PDF Document Features
.LP
To establish a consistent framework for adding PDF features, a
.CW groff
macro package, named
.CW pdfmark.tmac ,
has been provided.
Thus, to incorporate PDF features in a document,
the appropriate macro calls, as described below, may be placed in the
.CW groff
document source, which should then be processed with a
.CW groff
command of the form
.QP
.fam C
groff -Tps [-m
.I name "] -m"
.B pdfmark
.I options \& [-
.I "file ..." \& "...] "
.LP
It may be noted that the
.CW pdfmark
macros have no dependencies on, and no known conflicts with,
any other
.CW groff
macro package;  thus, users are free to use any other macro package,
of their choice, to format their documents, while also using the
.CW pdfmark
macros to add PDF features.
.NH 2
.XN -N pdfmark-operator -- The \F[C]pdfmark\F[] Operator
.LP
All PDF features are implemented by embedding instances of the
.B \F[C]pdfmark\F[]
operator, as described in the Adobe\*(rg
.pdfmark-manual ,
into
.CW groff 's
PostScript\*(rg output stream.
To facilitate the use of this operator, the
.CW pdfmark
macro package defines the primitive
.CW pdfmark
macro; it simply emits its argument list,
as arguments to a
.CW pdfmark
operator, in the PostScript\*(rg output stream.
.LP
.pdfhref M -N pdfmark-example
To illustrate the use of the
.CW pdfmark
macro, the following is a much simplified example of how a bookmark
may be added to a PDF document outline
.QP
.CW ".pdfmark \e"
.RS 4
.nf
.fam C
/Count 2 \e
/Title (An Example of a Bookmark with Two Children) \e
/View  [/FitH \en[PDFPAGE.Y]] \e
/OUT
.RE
.LP
In general, users should rarely need to use the
.CW pdfmark
macro directly.
In particular, the above example is too simple for general use; it
.EM will
create a bookmark, but it does
.EM not
address the issues of setting the proper value for the
.CW /Count
key, nor of computing the
.CW PDFPAGE.Y
value used in the
.CW /View
key. The
.CW pdfmark
macro package includes a more robust mechanism for creating bookmarks,
.\"
.\" Here is an example of how a local reference may be planted,
.\" using the automatic formatting feature of the "pdfhref" macro.
.\"
.\" This is a forward reference to the named destination "add-outline",
.\" which is defined below, using the "XN" wrapper macro, from the
.\" "spdf.tmac" macro package.  The automatically formatted reference
.\" will be enclosed in parentheses, as specified by the use of
.\" "-P" and "-A" options.
.\"
.pdfhref L -P ( -A ), -D add-outline
.\"
which addresses these issues automatically.
Nevertheless, the
.CW pdfmark
macro may be useful to users wishing to implement more advanced PDF features,
than those currently supported directly by the
.CW pdfmark
macro package.
.NH 2
.XN -N docview -- Selecting an Initial Document View
.LP
By default,
when a PDF document is opened,
the first page will be displayed,
at the default magnification set for the reader,
and outline and thumbnail views will be hidden.
When using a PDF reader,
such as Acrobat\*(rg\~Reader,
which supports the
.CW /DOCVIEW
class of the
.CW pdfmark
operator,
these default initial view settings may be overridden,
using the
.CW pdfview
macro.
For example
.QP
.CW ".pdfview /PageMode /UseOutlines"
.LP
will cause Acrobat\*(rg\~Reader to open the document outline view,
to the left of the normal page view,
while
.QP
.CW ".pdfview /PageMode /UseThumbs"
.LP
will open the thumbnail view instead.
.LP
Note that the two
.CW /PageMode
examples, above, are mutually exclusive \(em it is not possible to have
.EM both
outline and thumbnail views open simultaneously.
However, it
.EM is
permitted to add
.CW /Page
and
.CW /View
keys, to force the document to open at a page other than the first,
or to change the magnification at which the document is initially displayed;
see the
.pdfmark-manual
for more information.
.LP
It should be noted that the view controlling meta\(hydata, defined by the
.CW pdfview
macro, is not written immediately to the PostScript\*(rg output stream,
but is stored in an internal meta\(hydata \(lqcache\(rq,
(simply implemented as a
.CW groff
diversion).
This \(lqcached\(lq meta\(hydata must be written out later, by invoking the
.CW pdfsync
macro,
.\"
.\" Here is another example of how we may introduce a forward reference.
.\" This time we are using the shorter notation afforded by the "XR" macro
.\" provided by "spdf.tmac"; this example is equivalent to the native
.\" "pdfmark.tmac" form
.\"     .pdfhref L -D pdfsync -P ( -A ).
.\"
.XR pdfsync ). (
.\"
.NH 2
.XN -N docinfo -- Adding Document Identification Meta-Data
.LP
In addition to the
.CW /DOCVIEW
class of meta\(hydata described above,
.XR docview ), (
we may also wish to include document identification meta\(hydata,
which belongs to the PDF
.CW /DOCINFO
class.
.LP
To do this, we use the
.CW pdfinfo
macro.
As an example of how it is used,
the identification meta\(hydata attached to this document
was specified using a macro sequence similar to:\(en
.DS I
.CW
\&.pdfinfo /Title     PDF Document Publishing with GNU Troff
\&.pdfinfo /Author    Keith Marshall
\&.pdfinfo /Subject   How to Exploit PDF Features with GNU Troff
\&.pdfinfo /Keywords  groff troff PDF pdfmark
.DE
Notice that the
.CW pdfinfo
macro is repeated, once for each
.CW /DOCINFO
record to be placed in the document.
In each case, the first argument is the name of the applicable
.CW /DOCINFO
key, which
.EM must
be named with an initial solidus character;
all additional arguments are collected together,
to define the value to be associated with the specified key.
.LP
As is the case with the
.CW pdfview
macro,
.XR docview ), (
the
.CW /DOCINFO
records specified with the
.CW pdfinfo
macro are not immediately written to the PostScript\*(rg output stream;
they are stored in the same meta\(hydata cache as
.CW /DOCVIEW
specifications, until this cache is explicitly flushed,
by invoking the
.CW pdfsync
macro,
.XR pdfsync ). (
.NH 2
.XN -N add-outline -- Creating a Document Outline
.LP
A PDF document outline comprises a table of references,
to \(lqbookmarked\(rq locations within the document.
When the document is viewed in an \(lqoutline\~aware\(rq PDF document reader,
such as Adobe\*(rg Acrobat\*(rg Reader,
this table of \(lqbookmarks\(rq may be displayed in a document outline pane,
or \(lqBookmarks\(rq pane, to the left of the main document view.
Individual references in the outline view may then be selected,
by clicking with the mouse,
to jump directly to the associated marked location in the document view.
.LP
The document outline may be considered as a collection of \(lqhypertext\(rq
references to \(lqbookmarked\(rq locations within the document.
The
.CW pdfmark
macro package provides a single generalised macro,
.CW pdfhref ,
for creating and linking to \(lqhypertext\(rq reference marks.
This macro will be described more comprehensively in a later section,
.XR pdfhref ); (
the description here is restricted to its use for defining document outline entries.
.NH 3
.XN -N basic-outline -- A Basic Document Outline
.LP
In its most basic form, the document outline comprises a structured list of headings,
each associated with a marked location, or \(lqbookmark\(rq, in the document text,
and a specification for how that marked location should be displayed,
when this bookmark is selected.
.LP
To create a PDF bookmark, the
.CW pdfhref
macro is used,
at the point in the document where the bookmark is to be placed,
in the form
.QP
.fam C
.B ".pdfhref O"
.I level > <
.I "descriptive text ..."
.LP
in which the reference class
.CWB O \& \& \(rq \(lq
stipulates that this is an outline reference.
.LP
Alternatively, for those users who may prefer to think of a document outline
simply as a collection of bookmarks, the
.CW pdfbookmark
macro is also provided \(em indeed,
.CW pdfhref
invokes it, when processing the
.CWB O \& \& \(rq \(lq
reference class operator.
It may be invoked directly, in the form
.QP
.fam C
.B .pdfbookmark
.I level > <
.I "descriptive text ..."
.LP
Irrespective of which of the above macro forms is employed, the
.CWI level > <
argument is required.
It is a numeric argument, defining the nesting level of the \(lqbookmark\(rq
in the outline hierarchy, with one being the topmost level.
Its function may be considered analagous to the
.EM "heading level"
of the document's section headings,
for example, as specified with the
.CW NH
macro, if using the
.CW ms
macros to format the document.
.LP
All further arguments, following the
.CWI level > <
argument, are collected together, to specify the heading text which will appear
in the document's outline view.
Thus, the outline entry for this section of this document,
which has a level three heading,
might be specified as
.QP
.CW
\&.pdfhref O 3 \*(SN A Basic Document Outline
.LP
or, in the alternative form using the
.CW pdfbookmark
macro, as
.QP
.CW
\&.pdfbookmark 3 \*(SN A Basic Document Outline
.NH 3
.XN Hierarchical Structure in a Document Outline
.LP
When a document outline is created, using the
.CW pdfhref
macro as described in
.\"
.\" Here is an example of how we can temporarily modify the format of
.\" a reference link, in this case to indicate only the section number
.\" of the link target, in the form "section #", (or, if we define
.\" "SECREF.BEGIN" before the call, its content followed by the
.\" section number).
.\"
.\" We first define a macro, which will get the reference data from
.\" pdfhref, as arguments, and will return the formatted output, as we
.\" require it, the string "PDFHREF.TEXT".
.\"
.de SECREF
.while \\n(.$ \{\
.   ie '\\$1'section' \{\
.      if !dSECREF.BEGIN .ds SECREF.BEGIN \\$1
.      ds PDFHREF.TEXT \\*[SECREF.BEGIN]\~\\$2
.      rm SECREF.BEGIN
.      shift \\n(.$
.      \}
.   el .shift
.   \}
..
.\" We now tell "pdfhref" to use our formatting macro, in place of
.\" its builtin default formatter, before we specify the reference.
.\"
.pdfhref F SECREF
.pdfhref L -A , -D basic-outline
.\"
.\" At this point, we would normally revert the "pdfhref" formatter
.\" to use its default, built in macro.  However, in this particular
.\" case, we want to use our custom format one more time, before we
.\" revert it, so we will omit the reversion step this time.
.\"
and any entry is added at a nesting level greater than one,
then a hierarchical structure is automatically defined for the outline.
However, as was noted in the simplified
.pdfhref L -D pdfmark-example -- example
in
.pdfhref L -A , -D pdfmark-operator
.\"
.\" And now, we revert to default "pdfhref" formatting behaviour,
.\" by completing the call we delayed above.
.\"
.pdfhref F
.\"
the data required by the
.CW pdfmark
operator to create the outline entry may not be fully defined,
when the outline reference is defined in the
.CW groff
document source.
Specifically, when the outline entry is created, its
.CW /Count
key must be assigned a value equal to the number of its subordinate entries,
at the next inner level of the outline hierarchy;
typically however,
these subordinate entries will be defined
.EM later
in the document source, and the appropriate
.CW /Count
value will be unknown, when defining the parent entry.
.LP
To resolve this paradox, the
.CW pdfhref
macro creates the outline entry in two distinct phases \(em
a destination marker is placed in the PostScript\*(rg output stream immediately,
when the outline reference is defined,
but the actual outline entry is stored in an internal \(lqoutline cache\(rq,
until its subordinate hierarchy has been fully defined;
it can then be inserted in the output stream, with its
.CW /Count
value correctly assigned.
Effectively, to ensure integrity of the document outline structure,
this means that each top level outline entry, and
.EM all
of its subordinates, are retained in the cache, until the
.EM next
top level entry is defined.
.LP
One potential problem, which arises from the use of the \(lqoutline cache\(rq,
is that, at the end of any document formatting run, the last top level outline entry,
and any subordinates defined after it, will remain in the cache, and will 
.EM not
be automatically written to the output stream.
To avoid this problem, the user should follow the guidelines given in
.\"
.\" Here is a more conventional example of how to temporarily change
.\" to the format used to display reference links.  We will again use
.\" the "SECREF" format, which we defined above, but on this occasion
.\" we will immediately revert to the default format, after the link
.\" has been placed.
.\"
.pdfhref F SECREF
.pdfhref L -D pdfsync -A ,
.pdfhref F
.\"
to synchronise the output state with the cache state,
.XR pdfsync ), (
at the end of the
.CW groff
formatting run.
.NH 3
.XN -N outline-view -- Associating a Document View with an Outline Reference
.LP
Each \(lqbookmark\(rq entry, in a PDF document outline,
is associated with a specific document view.
When the reader selects any outline entry,
the document view changes to display the document context
associated with that entry.
.LP
The document view specification,
to be associated with any document outline entry,
is established at the time when the outline entry is created.
However, rather than requiring that each individual use of the
.CW pdhref
macro, to create an outline entry,
should include its own view specification,
the actual specification assigned to each entry is derived from
a generalised specification defined in the string
.CW PDFBOOKMARK.VIEW ,
together with the setting of the numeric register
.CW PDFHREF.VIEW.LEADING ,
which determine the effective view specification as follows:\(en
.QS
.IP \*[= PDFBOOKMARK.VIEW]
Establishes the magnification at which the document will be viewed,
at the location of the \(lqbookmark\(rq; by default, it is defined by
.RS
.QP
.CW ".ds PDFBOOKMARK.VIEW /FitH \e\en[PDFPAGE.Y] u"
.RE
.IP
which displays the associated document view,
with the \(lqbookmark\(rq location positioned at the top of the display window,
and with the magnification set to fit the page width to the width of the window.
.IP \*[= PDFHREF.VIEW.LEADING]
Specifies additional spacing,
to be placed between the top of the display window
and the actual location of the \(lqbookmark\(rq on the displayed page view.
By default, it is set as
.RS
.QP
.CW ".nr PDFHREF.VIEW.LEADING 5.0p"
.RE
.IP
Note that
.CW PDFHREF.VIEW.LEADING
does not represent true \(lqleading\(rq, in the typographical sense,
since any preceding text, set in the specified display space,
will be visible at the top of the document viewing window,
when the reference is selected.
.IP
Also note that the specification of
.CW PDFHREF.VIEW.LEADING
is shared by
.EM all
reference views defined by the
.CW pdfhref
macro; whereas
.CW PDFBOOKMARK.VIEW
is applied exclusively to outline references,
there is no independent
.CW PDFBOOKMARK.VIEW.LEADING
specification.
.QE
.LP
If desired, the view specification may be changed, by redefining the string
.CW PDFBOOKMARK.VIEW ,
and possibly also the numeric register
.CW PDFHREF.VIEW.LEADING .
Any alternative definition for
.CW PDFBOOKMARK.VIEW
.EM must
be specified in terms of valid view specification parameters,
as described in the Adobe\*(rg
.pdfmark-manual .
.LP
Note the use of the register
.CW PDFPAGE.Y ,
in the default definition of
.CW PDFBOOKMARK.VIEW
above.
This register is computed by
.CW pdfhref ,
when creating an outline entry;
it specifies the vertical position of the \(lqbookmark\(rq,
in basic
.CW groff
units, relative to the
.EM bottom
edge of the document page on which it is defined,
and is followed, in the
.CW PDFBOOKMARK.VIEW
definition, by the
.CW grops
.CW u \(rq \(lq
operator, to convert it to PostScript\*(rg units on output.
It may be used in any redefined specification for
.CW PDFBOOKMARK.VIEW ,
(or in the analogous definition of
.CW PDFHREF.VIEW ,
described in
.XR-NO-PREFIX pdfhref-view ),
but
.EM not
in any other context,
since its value is undefined outside the scope of the
.CW pdfhref
macro.
.LP
Since
.CW PDFPAGE.Y
is computed relative to the
.EM bottom
of the PDF output page,
it is important to ensure that the page length specified to
.CW troff
correctly matches the size of the logical PDF page.
This is most effectively ensured,
by providing
.EM identical
page size specifications to
.CW groff ,
.CW grops
and to the PostScript\*(rg to PDF converter employed,
and avoiding any page length changes within the document source.
.LP
Also note that
.CW PDFPAGE.Y
is the only automatically computed \(lqbookmark\(rq location parameter;
if the user redefines
.CW PDFBOOKMARK.VIEW ,
and the modified view specification requires any other positional parameters,
then the user
.EM must
ensure that these are computed
.EM before
invoking the
.CW pdfhref
macro.
.NH 3
.XN -N outline-folding -- Folding the Outline to Conceal Less Significant Headings
.LP
When a document incorporates many subheadings,
at deeply nested levels,
it may be desirable to \(lqfold\(rq the outline
such that only the major heading levels are initially visible,
yet making the inferior subheadings accessible,
by allowing the reader to expand the view of any heading branch on demand.
.LP
The
.CW pdfmark
macros support this capability,
through the setting of the
.CW PDFOUTLINE.FOLDLEVEL
register.
This register should be set to the number of heading levels
which it is desired to show in expanded form, in the
.EM initial
document outline display;
all subheadings at deeper levels will still be added to the outline,
but will not become visible until the outline branch containing them is expanded.
'ne 5
For example, the setting used in this document:
.QS
.LD
.fam C
\&.\e" Initialise the outline view to show only three heading levels,
\&.\e" with additional subordinate level headings folded.
\&.\e"
\&.nr PDFOUTLINE.FOLDLEVEL 3
.DE
.QE
.LP
results in only the first three levels of headings being displayed
in the document outline,
.EM until
the reader chooses to expand the view,
and so reveal the lower level headings in any outline branch.
.LP
The initial default setting of
.CW PDFOUTLINE.FOLDLEVEL ,
if the document author does not choose to change it,
is 10,000.
This is orders of magnitude greater than the maximum heading level
which is likely to be used in any document;
thus the default behaviour will be to show document outlines fully expanded,
to display all headings defined,
at all levels within each document.
.LP
The setting of
.CW PDFOUTLINE.FOLDLEVEL
may be changed at any time;
however, the effect of each such change may be difficult to predict,
since it is applied not only to outline entries which are defined
.EM after
the setting is changed,
but also to any entries which remain in the outline cache,
.EM at
this time.
Therefore, it is recommended that
.CW PDFOUTLINE.FOLDLEVEL
should be set
.EM once ,
at the start of each document;
if it
.EM is
deemed necessary to change it at any other time,
the outline cache should be flushed,
.XR pdfsync ), (
.EM immediately
before the change,
which should immediately preceed a level one heading.
.NH 3
.XN -N multipart-outline -- Outlines for Multipart Documents
.LP
When a document outline is created, using the
.CW pdfhref
macro, each reference mark is automatically assigned a name,
composed of a fixed stem followed by a serially generated numeric qualifier.
This ensures that, for each single part document, every outline reference
has a uniquely named destination.
.LP
As the overall size of the PDF document increases,
it may become convenient to divide it into smaller,
individually formatted PostScript\*(rg components,
which are then assembled, in the appropriate order,
to create a composite PDF document.
While this strategy may simplify the overall process of creating and
editing larger documents, it does introduce a problem in creating
an overall document outline,
since each individual PostScript\*(rg component will be assigned
duplicated sequences of \(lqbookmark\(rq names,
with each name ultimately referring to multiple locations in the composite document.
To avoid such reference naming conflicts, the
.CW pdfhref
macro allows the user to specify a \(lqtag\(rq,
which is appended to the automatically generated \(lqbookmark\(rq name;
this may be used as a discriminating mark, to distinguish otherwise
similarly named destinations, in different sections of the composite document.
.LP
To create a \(lqtagged\(rq document outline,
the syntax for invocation of the
.CW pdfhref
macro is modified, by the inclusion of an optional \(lqtag\(rq specification,
.EM before
the nesting level argument, i.e.
.QP
.fam C
.B ".pdfhref O"
.B -T \& [
.I tag >] <
.I level > <
.I "descriptive text ..."
.LP
The optional
.CWI tag > <
argument may be composed of any characters of the user's choice;
however, its initial character
.EM "must not"
be any decimal digit, and ideally it should be kept short
\(em one or two characters at most.
.LP
By employing a different tag in each section,
the user can ensure that \(lqbookmark\(rq names remain unique,
throughout all the sections of a composite document.
For example, when using the
.CW spdf.tmac
macro package, which adds
.CW pdfmark
capabilities to the standard
.CW ms
package,
.XR using-spdf ), (
the table of contents is collected into a separate PostScript\*(rg section
from the main body of the document.
In the \(lqbody\(rq section, the document outline is \(lquntagged\(rq,
but in the \(lqTable\~of\~Contents\(rq section, a modified version of the
.CW TC
macro adds an outline entry for the start of the \(lqTable\~of\~Contents\(rq,
invoking the
.CW pdfhref
macro as
.QP
.CW ".pdfhref O -T T 1 \e\e*[TOC]"
.LP
to tag the associated outline destination name with the single character suffix,
.CW T \(rq. \(lq
Alternatively, as in the case of the basic outline,
.XR basic-outline ), (
this may equally well be specified as
.QP
.CW ".pdfbookmark -T T 1 \e\e*[TOC]"
.NH 3
.XN Delegation of the Outline Definition
.LP
Since the most common use of a document outline
is to provide a quick method of navigating through a document,
using active \(lqhypertext\(rq links to chapter and section headings,
it may be convenient to delegate the responsibility of creating the outline
to a higher level macro, which is itself used to
define and format the section headings.
This approach has been adopted in the
.CW spdf.tmac
package, to be described later,
.XR using-spdf ). (
.LP
When such an approach is adopted,
the user will rarely, if ever, invoke the
.CW pdfhref
macro directly, to create a document outline.
For example, the structure and content of the outline for this document
has been exclusively defined, using a combination of the
.CW NH
macro, from the
.CW ms
package, to establish the structure, and the
.CW XN
macro from
.CW spdf.tmac ,
to define the content.
In this case,
the responsibility for invoking the
.CW pdfhref
macro, to create the document outline,
is delegated to the
.CW XN
macro.
.NH 2
.XN -N pdfhref -- Adding Reference Marks and Links
.LP
.pdfhref F SECREF
.ds SECREF.BEGIN Section
.pdfhref L -D add-outline
.pdfhref F
has shown how the
.CW pdfhref
macro may be used to create a PDF document outline.
While this is undoubtedly a powerful capability,
it is by no means the only trick in the repertoire of this versatile macro.
.LP
The macro name,
.CW pdfhref ,
which is a contraction of \(lqPDF HyperText Reference\(rq,
indicates that the general purpose of this macro is to define
.EM any
type of dynamic reference mark, within a PDF document.
Its generalised usage syntax takes the form
.QP
.fam C
.B .pdfhref
.BI class > <
.I "-options ...\&" ] [
[--]
.I "descriptive text ...\&" ] [
.LP
where
.CW <\f(CIclass\fP>
represents a required single character argument,
which defines the specific reference operation to be performed,
and may be selected from:\(en
.QS
.IP \*[= O]
Add an entry to the document outline.
This operation has been described earlier,
.XR add-outline ). (
.IP \*[= M]
Place a \(lqnamed destination\(rq reference mark at the current output position,
in the current PDF document,
.XR mark-dest ). (
.IP \*[= D]
Specify the content of a PDF document reference dictionary entry;
typically, such entries are generated automatically,
by transformation of the intermediate output resulting from the use of
.CW pdfhref
.CWB M \& \& \(rq, \(lq
with the
.CWB -X \& \& \(rq \(lq
modifier,
.XR create-map ); (
however, it is also possible to specify such entries manually,
.XR user-format ). (
.IP \*[= L]
Insert an active link to a named destination,
.XR link-named ), (
at the current output position in the current PDF document,
such that when the reader clicks on the link text,
the document view changes to show the location of the named destination.
.IP \*[= W]
Insert an active link to a \(lqweb\(rq resource,
.XR add-weblink ), (
at the current output position in the current PDF document.
This is effectively the same as using the
.CWB L \& \& \(rq \(lq
operator to establish a link to a named destination in another PDF document,
.XR link-extern ), (
except that in this case, the destination is specified by a
\(lquniform resource identifier\(rq, or
.CW URI ;
this may represent any Internet or local resource
which can be specified in this manner.
.IP \*[= F]
Specify a user defined macro, to be called by
.CW pdfhref ,
when formatting the text in the active region of a link,
.XR set-format ). (
.IP \*[= Z]
Define the absolute position on the physical PDF output page,
where the \(lqhot\(hyspot\(rq associated with an active link is to be placed.
Invoked in pairs, marking the starting and ending PDF page co\(hyordinates
for each link \(lqhot\(hyspot\(rq, this operator is rarely, if ever,
specified directly by the user;
rather, appropriate
.CW pdfhref
.CWB Z \& \& \(rq \(lq
specifications are inserted automatically into the document reference map
during the PDF document formatting process,
.XR create-map ). (
.IP \*[= I]
Initialise support for
.CW pdfhref
features.
The current
.CW pdfhref
implementation provides only one such feature which requires initialisation
\(em a helper macro which must be attached to a user supplied page trap handler,
in order to support mapping of reference \(lqhot\(hyspots\(rq
which extend through a page transition;
.XR page-trap ). (
.QE
.NH 3
.XN Optional Features of the \F[C]pdfhref\F[] Macro
.LP
The behaviour of a number of the
.CW pdfhref
macro operations can be modified,
by including
.EM "option specifiers" \(rq \(lq
after the operation specifying argument,
but
.EM before
any other arguments normally associated with the operation.
In
.EM all
cases, an option is specified by an
.EM "option flag" \(rq, \(lq
comprising an initial hyphen,
followed by one or two option identifying characters.
Additionally,
.EM some
options require
.EM "exactly one"
option argument;
for these options, the argument
.EM must
be specified, and it
.EM must
be separated from the preceding option flag by one or more
.EM spaces ,
(tabs
.EM "must not"
be used).
It may be noted that this paradigm for specifying options
is reminiscent of most
.SM
UNIX\(tm
.LG
shells; however, in the case of the
.CW pdfhref
macro, omission of the space separating an option flag from its argument is
.EM never
permitted.
.LP
A list of
.EM all
general purpose options supported by the
.CW pdfhref
macro is given below.
Note that not all options are supported for all
.CW pdfhref
operations; the operations affected by each option are noted in the list.
For
.EM most
operations, if an unsupported option is specified,
it will be silently ignored; however, this behaviour should
not be relied upon.
.LP
The general purpose options, supported by the
.CW pdfhref
macro, are:\(en
.QS
.IP \*[= -N\0 name > <]
Allows the
.CWI name > <
associated with a PDF reference destination
to be defined independently from the following text,
which describes the reference.
This option affects only the
.CWB M \& \& \(rq \(lq
operation of the
.CW pdfhref
macro,
.XR mark-dest ). (
.IP \*[= -E]
Also used exclusively with the
.CWB M \& \& \(rq \(lq
operator, the
.CWB -E
option causes any specified
.CWI descriptive \& \& \~\c
.CWI text
arguments,
.XR mark-dest ), (
to be copied, or
.EM echoed ,
in the body text of the document,
at the point where the reference mark is defined;
(without the
.CWB -E
option, such
.CWI descriptive \& \& \~\c
.CWI text
will appear
.EM only
at points where links to the reference mark are placed,
and where the standard reference display format,
.XR set-format ), (
is used).
.IP \*[= -D\0 dest > <]
Specifies the
.CW URI ,
or the destination name associated with a PDF active link,
independently of the following text,
which describes the link and demarcates the link \(lqhot\(hyspot\(rq.
This option affects the behaviour of the
.CW pdfhref
macro's
.CWB L \& \& \(rq \(lq
and
.CWB W \& \& \(rq \(lq
operations.
.IP
When used with the
.CWB L \& \& \(rq \(lq
operator, the
.CWI dest > <
argument must specify a PDF \(lqnamed destination\(rq,
as defined using
.CW pdfhref
with the
.CWB M \& \& \(rq \(lq
operator.
.IP
When used with the
.CWB W \& \& \(rq \(lq
operator,
.CWI dest > <
must specify a link destination in the form of a
\(lquniform resource identifier\(rq, or
.CW URI ,
.XR add-weblink ). (
.IP \*[= -F\0 file > <]
When used with the
.CWB L \& \& \(rq \(lq
.CW pdfhref
operator,
.CWI file > <
specifies an external PDF file in which the named destination
for the link reference is defined.
This option
.EM must
be specified with the
.CWB L \& \& \(rq \(lq
operator,
to create a link to a destination in a different PDF document;
when the
.CWB L \& \& \(rq \(lq
operator is used
.EM without
this option, the link destination is assumed to be defined
within the same document.
.IP \*[= -P\0 \(dqprefix\(hytext\(dq > <]
Specifies
.CWI \(dqprefix\(hytext\(dq > <
to be attached to the
.EM start
of the text describing an active PDF document link,
with no intervening space, but without itself being included in the
active area of the link \(lqhot\(hyspot\(rq;
it is effective with the
.CWB L \& \& \(rq \(lq
and
.CWB W \& \& \(rq \(lq
.CW pdfhref
operators.
.IP
Typically, this option would be used to insert punctuation before
the link \(lqhot\(hyspot\(rq.
Thus, there is little reason for the inclusion of spaces in
.CWI \(dqprefix\(hytext\(dq > < ;
however, if such space is required, then the enclosing double quotes
.EM must
be specified, as indicated.
.IP \*[= -A\0 \(dqaffixed\(hytext\(dq > <]
Specifies
.CWI \(dqaffixed\(hytext\(dq > <
to be attached to the
.EM end
of the text describing an active PDF document link,
with no intervening space, but without itself being included in the
active area of the link \(lqhot\(hyspot\(rq;
it is effective with the
.CWB L \& \& \(rq \(lq
and
.CWB W \& \& \(rq \(lq
.CW pdfhref
operators.
.IP
Typically, this option would be used to insert punctuation after
the link \(lqhot\(hyspot\(rq.
Thus, there is little reason for the inclusion of spaces in
.CWI \(dqaffixed\(hytext\(dq > < ;
however, if such space is required, then the enclosing double quotes
.EM must
be specified, as indicated.
.IP \*[= -T\0 tag > <]
When specified with the
.CWB O \& \& \(rq \(lq
operator,
.CWI tag > <
is appended to the \(lqbookmark\(rq name assigned to the generated outline entry.
This option is
.EM required ,
to distinguish between the series of \(lqbookmark\(rq names generated in
individual passes of the
.CW groff
formatter, when the final PDF document is to be assembled
from a number of separately formatted components;
.XR multipart-outline ). (
.IP \*[= -X]
This
.CW pdfhref
option is used with either the
.CWB M \& \& \(rq \(lq
operator, or with the
.CWB L \& \& \(rq \(lq
operator.
.IP
When used with the
.CWB M \& \& \(rq \(lq
operator,
.XR mark-dest ), (
it ensures that a cross reference record for the marked destination
will be included in the document reference map,
.XR export-map ). (
.IP
When used with the
.CWB L \& \& \(rq \(lq
operator,
.XR link-named ), (
it causes the reference to be displayed in the standard cross reference format,
.XR set-format ), (
but substituting the
.CWI descriptive \& \& \~\c
.CWI text
specified in the
.CW pdfhref \& \(lq
.CW L \(rq
argument list,
for the description specified in the document reference map.
.IP \*[= --]
Marks the end of the option specifiers.
This may be used with all
.CW pdfhref
operations which accept options, to prevent
.CW pdfhref
from interpreting any following arguments as option specifiers,
even if they would otherwise be interpreted as such.
It is also useful when the argument list to
.CW pdfhref
contains special characters \(em any special character,
which is not legal in a
.CW groff
macro name, will cause a parsing error, if
.CW pdfhref
attempts to match it as a possible option flag;
using the
.CW -- \(rq \(lq
flag prevents this, so suppressing the
.CW groff
warning message, which would otherwise ensue.
.IP
Using this flag after
.EM all
sequences of macro options is recommended,
even when it is not strictly necessary,
if only for the entirely cosmetic benefit of visually separating
the main argument list from the sequence of preceding options.
.QE
.LP
In addition to the
.CW pdfhref
options listed above, a supplementary set of two character options are defined.
These supplementary options, listed below, are intended for use with the
.CWB L \& \& \(rq \(lq
operator, in conjunction with the
.CWB -F \& \& \~\c
.CWBI file > <
option, to specify alternate file names,
in formats compatible with the file naming conventions
of alternate operating systems;
they will be silently ignored, if used in any other context.
.LP
The supported alternate file name options,
which are ignored if the
.CWB -F \& \& \~\c
.CWBI file > <
option is not specified, are:\(en
.QS
.IP \*[= -DF\0 dos\(hyfile > <]
Specifies the name of the file in which a link destination is defined,
using the file naming semantics of the
.CW MS\(hyDOS \*(rg
operating system.
When the PDF document is read on a machine
where the operating system uses the
.CW MS\(hyDOS \*(rg
file system, then
.CWI dos\(hyfile > <
is used as the name of the file containing the reference destination,
overriding the
.CWI file > <
argument specified with the
.CWB -F
option.
.IP \*[= -MF\0 mac\(hyfile > <]
Specifies the name of the file in which a link destination is defined,
using the file naming semantics of the
.CW Apple \*(rg
.CW Macintosh \*(rg
operating system.
When the PDF document is read on a machine
where the operating system uses the
.CW Macintosh \*(rg
file system, then
.CWI mac\(hyfile > <
is used as the name of the file containing the reference destination,
overriding the
.CWI file > <
argument specified with the
.CWB -F
option.
.IP \*[= -UF\0 unix\(hyfile > <]
Specifies the name of the file in which a link destination is defined,
using the file naming semantics of the
.CW UNIX \(tm
operating system.
When the PDF document is read on a machine
where the operating system uses
.CW POSIX
file naming semantics, then
.CWI unix\(hyfile > <
is used as the name of the file containing the reference destination,
overriding the
.CWI file > <
argument specified with the
.CWB -F
option.
.IP \*[= -WF\0 win\(hyfile > <]
Specifies the name of the file in which a link destination is defined,
using the file naming semantics of the
.CW MS\(hyWindows \*(rg
32\(hybit operating system.
When the PDF document is read on a machine
where the operating system uses any of the
.CW MS\(hyWindows \*(rg
file systems, with long file name support, then
.CWI win\(hyfile > <
is used as the name of the file containing the reference destination,
overriding the
.CWI file > <
argument specified with the
.CWB -F
option.
.QE
.NH 3
.XN -N mark-dest -- Marking a Reference Destination
.LP
The
.CW pdfhref
macro may be used to create active links to any Internet resource,
specified by its
.CW URI ,
or to any \(lqnamed destination\(rq,
either within the same document, or in another PDF document.
Although the PDF specification allows link destinations to be defined
in terms of a page number, and an associated view specification,
this style of reference is not currently supported by the
.CW pdfhref
macro, because it is not possible to adequately bind the specification
for the destination with the intended reference context.
.LP
References to Internet resources are interpreted in accordance with the
.CW W3C
standard for defining a
.CW URI ;
hence the only prerequisite, for creating a link to any Internet resource,
is that the
.CW URI
be properly specified, when declaring the reference;
.XR add-weblink ). (
In the case of references to \(lqnamed destinations\(rq in PDF documents,
however, it is necessary to provide a mechanism for creating such
\(lqnamed destinations\(rq.
This may be accomplished, by invoking the
.CW pdfhref
macro in the form
.QP
.fam C
.B ".pdfhref M"
.B -N \& [
.I name >] <
.B -X ] [
.B -E ] [
.I "descriptive text ...\&" ] [
.LP
This creates a \(lqnamed destination\(rq reference mark, with its name specified by
.CWI name > < ,
or, if the
.CWB -N
option is not specified, by the first word of
.CWI descriptive \& \& \~\c
.CWI text \& \& ;
(note that this imposes the restriction that,
if the
.CWB -N
option is omitted, then
.EM "at least"
one word of
.CWI descriptive \& \& \~\c
.CWI text
.EM must
be specified).
Additionally, a reference view will be automatically defined,
and associated with the reference mark,
.XR pdfhref-view ), (
.\" and, if any
.\" .CWI descriptive
.\" .CWI text
.\" is specified, or the
and, if the
.CWB -X
option is specified, and no document cross reference map has been imported,
.XR import-map ), (
then a cross reference mapping record,
.XR export-map ), (
will be written to the
.CW stdout
stream;
this may be captured, and subsequently used to generate a cross reference map
for the document,
.XR create-map ). (
.LP
When a \(lqnamed destination\(rq reference mark is created, using the
.CW pdfhref
macro's
.CWB M \& \& \(rq \(lq
operator, there is normally no visible effect in the formatted document; any
.CWI descriptive \& \& \~\c
.CWI text
which is specified will simply be stored in the cross reference map,
for use when a link to the reference mark is created.
This default behaviour may be changed, by specifying the
.CWB -E
option, which causes any specified
.CWI descriptive \& \& \~\c
.CWI text
to be \(lqechoed\(rq in the document text,
at the point where the reference mark is placed,
in addition to its inclusion in the cross reference map.
.NH 4
.XN -N export-map -- Mapping a Destination for Cross Referencing
.LP
Effective cross referencing of
.EM any
document formatted by
.CW groff
requires multiple pass formatting.
Details of how this multiple pass formatting may be accomplished,
when working with the
.CW pdfmark
macros, will be discussed later,
.XR do-xref ); (
at this stage, the discussion will be restricted to the initial preparation,
which is required at the time when the cross reference destinations are defined.
.LP
The first stage, in the process of cross referencing a document,
is the generation of a cross reference map.
Again, the details of
.EM how
the cross reference map is generated will be discussed in
.pdfhref F SECREF L -D do-xref -A ;
.pdfhref F
however, it is important to recognise that
.EM what
content is included in the cross reference map is established
when the reference destination is defined \(em it is derived
from the reference data exported on the
.CW stderr
stream by the
.CW pdfhref
macro, when it is invoked with the
.CWB M \& \& \(rq \(lq
operator, and is controlled by whatever definition of the string
.CW PDFHREF.INFO
is in effect, when the
.CW pdfhref
macro is invoked.
.LP
The initial default setting of
.CW PDFHREF.INFO
is
.QP
.CW ".ds PDFHREF.INFO page \e\en% \e\e$*"
.LP
which ensures that the cross reference map will contain
at least a page number reference, supplemented by any
.CWI descriptive \& \& \~\c
.CWI text
which is specified for the reference mark, as defined by the
.CW pdfhref
macro, with its
.CWB M \& \& \(rq \(lq
operator; this may be redefined by the user,
to export additional cross reference information,
or to modify the default format for cross reference links,
.XR set-format ). (
.NH 4
.XN -N pdfhref-view -- Associating a Document View with a Reference Mark
.LP
In the same manner as each document outline reference, defined by the
.CW pdfhref
macro with the
.CWB O \& \& \(rq \(lq
operator,
.XR add-outline ), (
has a specific document view associated with it,
each reference destination marked by
.CW pdfhref
with the
.CWB M \& \& \(rq \(lq
operator, requires an associated document view specification.
.LP
The mechanism whereby a document view is associated with a reference mark
is entirely analogous to that employed for outline references,
.XR outline-view ), (
except that the
.CW PDFHREF.VIEW
string specification is used, in place of the
.CW PDFBOOKMARK.VIEW
specification.
Thus, the reference view is defined in terms of:\(en
.QS
.IP \*[= PDFHREF.VIEW]
A string,
establishing the position of the reference mark within the viewing window,
and the magnification at which the document will be viewed,
at the location of the marked reference destination;
by default, it is defined by
.RS
.QP
.CW ".ds PDFHREF.VIEW /FitH \e\en[PDFPAGE.Y] u"
.RE
.IP
which displays the reference destination at the top of the viewing window,
with the magnification set to fit the page width to the width of the window.
.IP \*[= PDFHREF.VIEW.LEADING]
A numeric register,
specifying additional spacing, to be placed between the top of the display
window and the actual position at which the location of the reference
destination appears within the window.
This register is shared with the view specification for outline references,
and thus has the same default initial setting,
.RS
.QP
.CW ".nr PDFHREF.VIEW.LEADING 5.0p"
.RE
.IP
as in the case of outline reference views.
.IP
Again, notice that
.CW PDFHREF.VIEW.LEADING
does not represent true typographic \(lqleading\(rq,
since any preceding text, set in the specified display space,
will be visible at the top of the viewing window,
when the reference is selected.
.QE
.LP
Just as the view associated with outline references may be changed,
by redefining
.CW PDFBOOKMARK.VIEW ,
so the view associated with marked reference destinations may be changed,
by redefining
.CW PDFHREF.VIEW ,
and, if desired,
.CW PDFHREF.VIEW.LEADING ;
such changes will become effective for all reference destinations marked
.EM after
these definitions are changed.
(Notice that, since the specification of
.CW PDFHREF.VIEW.LEADING
is shared by both outline reference views and marked reference views,
if it is changed, then the views for
.EM both
reference types are changed accordingly).
.LP
It may again be noted, that the
.CW PDFPAGE.Y
register is used in the definition of
.CW PDFHREF.VIEW ,
just as it is in the definition of
.CW PDFBOOKMARK.VIEW ;
all comments in
.pdfhref F SECREF L -D outline-view
.pdfhref F
relating to its use, and indeed to page position computations in general,
apply equally to marked reference views and to outline reference views.
.NH 3
.XN -N link-named -- Linking to a Marked Reference Destination
.LP
Any named destination, such as those marked by the
.CW pdfhref
macro, using it's
.CWB M \& \& \(rq \(lq
operator, may be referred to from any point in
.EM any
PDF document, using an
.EM "active link" ;
such active links are created by again using the
.CW pdfhref
macro, but in this case, with the
.CWB L \& \& \(rq \(lq
operator.
This operator provides support for two distinct cases,
depending on whether the reference destination is defined in 
the same document as the link,
.XR link-intern ), (
or is defined as a named destination in a different PDF document,
.XR link-extern ). (
.NH 4
.XN -N link-intern -- References within a Single PDF Document
.LP
The general syntactic form for invoking the
.CW pdfhref
macro,
when creating a link to a named destination within the same PDF document is
.QP
.fam C
.B .pdfhref
.B L
.B -D \& [
.BI dest-name >] <
.B -P \& [
.BI prefix-text >] <
.B -A \& [
.BI affixed-text >] <
\e
.br
\0\0\0
.B -X ] [
.B -- ] [
.I "descriptive text ...\&" ] [
.LP
where
.CWI dest-name > <
specifies the name of the link destination,
as specified using the
.CW pdfhref
.CWB M \& \& \(rq \(lq
operation; (it may be defined either earlier in the document,
to create a backward reference, or later, to create a forward reference).
.\"
.\" Here's a example of how to add an iconic annotation.
.\"
.\".pdfnote -T "Internal Cross References" \
.\"   This description is rather terse, and could benefit from \
.\"   the inclusion of an example.
.LP
If any
.CWI descriptive \& \& \~\c
.CWI text
arguments are specified, then they will be inserted into the
.CW groff
output stream, to define the text appearing in the \(lqhot\(hyspot\(rq
region of the link;
this will be printed in the link colour specified by the string,
.CW PDFHREF.TEXT.COLOUR ,
which is described in
.XR-NO-PREFIX set-colour .
If the
.CWB -X
option is also specified, then the
.CWI descriptive \& \& \~\c
.CWI text
will be augmented, by prefacing it with page and section number indicators,
in accordance with the reference formatting rules which are in effect,
.XR set-format ); (
such indicators will be included within the active link region,
and will also be printed in the link colour.
.LP
Note that
.EM either
the
.CWB -D \& \& \~\c
.CWBI dest\(hyname > <
option,
.EM or
the
.CWI descriptive \& \& \~\c
.CWI text
arguments,
.EM "but not both" ,
may be omitted.
If the
.CWB -D \& \& \~\c
.CWBI dest\(hyname > <
option is omitted, then the first word of
.CWI descriptive \& \& \~\c
.CWI text \& \& ,
i.e.\~all text up to but not including the first space,
will be interpreted as the
.CWBI dest\(hyname > <
for the link; this text will also appear in the running text of the document,
within the active region of the link.
Alternatively, if the
.CWB -D \& \& \~\c
.CWBI dest\(hyname > <
option
.EM is
specified, and
.CWI descriptive \& \& \~\c
.CWI text
is not,
then the running text which defines the reference,
and its active region,
will be derived from the reference description which is specified
when the named destination is marked,
.XR mark-dest ), (
and will be formatted according to the reference formatting rules
which are in effect, when the reference is placed,
.XR set-format ); (
in this case, it is not necessary to specify the
.CWB -X
option to activate automatic formatting of the reference \(em it is implied,
by the omission of all
.CWI descriptive \& \& \~\c
.CWI text
arguments.
.LP
The
.CWB -P \& \& \~\c
.CWBI prefix\(hytext > <
and
.CWB -A \& \& \~\c
.CWBI affixed\(hytext > <
options may be used to specify additional text
which will be placed before and after the linked text respectively,
with no intervening space.
Such prefixed and affixed text will be printed in the normal text colour,
and will not be included within the active region of the link.
This feature is mostly useful for creating parenthetical references,
or for placing punctuation adjacent to,
but not included within,
the text which defines the active region of the link.
.LP
The operation of the
.CW pdfhref
macro, when used with its
.CWB L \& \& \(rq \(lq
operator to place a link to a named PDF destination,
may best be illustrated by an example.
However, since the appearance of the link will be influenced by
factors established when the named destination is marked,
.XR mark-dest ), (
and also by the formatting rules in effect when the link is placed,
the presentation of a suitable exanple will be deferred,
until the formatting mechanism has been explained,
.XR set-format ). (
.NH 4
.XN -N link-extern -- References to Destinations in Other PDF Documents
.LP
The
.CW pdfhref
macro's
.CWB L \& \& \(rq \(lq
operator is not restricted to creating reference links
within a single PDF document.
When the link destination is defined in a different document,
then the syntactic form for invoking
.CW pdfhref
is modified, by the addition of options to specify the
name and location of the PDF file in which the destination is defined.
Thus, the extended
.CW pdfhref
syntactic form becomes
.QP
.fam C
.B .pdfhref
.B L
.B -F
.BI file > <
.B -D \& [
.BI dest-name >] <
\e
.br
\0\0\0
.B -DF \& [
.BI dos-file >] <
.B -MF \& [
.BI mac-file >] <
.B -UF \& [
.BI unix-file >] <
\e
.br
\0\0\0
.B -WF \& [
.BI win-file >] <
.B -P \& [
.BI prefix-text >] <
.B -A \& [
.BI affixed-text >] <
\e
.br
\0\0\0
.B -X ] [
.B -- ] [
.I "descriptive text ...\&" ] [
.LP
where the
.CWB -F \& \& \~\c
.CWBI file > <
option serves
.EM two
purposes: it both indicates to the
.CW pdfhref
macro that the specified reference destination
is defined in an external PDF file,
and it also specifies the normal path name,
which is to be used to locate this file,
when a user selects the reference.
.LP
In addition to the
.CWB -F \& \& \~\c
.CWBI file > <
option, which
.EM must
be specified when referring to a destination in an external PDF file,
the
.CWB -DF \& \& \~\c
.CWBI dos\(hyfile > < ,
.CWB -MF \& \& \~\c
.CWBI mac\(hyfile > < ,
.CWB -UF \& \& \~\c
.CWBI unix\(hyfile > <
and
.CWB -WF \& \& \~\c
.CWBI win\(hyfile > <
options may be used to specify the location of the file
containing the reference destination,
in a variety of operating system dependent formats.
These options assign their arguments to the
.CW /DosFile ,
.CW /MacFile ,
.CW /UnixFile
and
.CW /WinFile
keys of the generated
.CW pdfmark
respectively; thus when any of these options are specified,
.EM "in addition to"
the
.CWB -F \& \& \~\c
.CWBI file > <
option, and the document is read on the appropriate operating systems,
then the path names specified by
.CWBI dos\(hyfile > < ,
.CWBI mac\(hyfile > < ,
.CWBI unix\(hyfile > <
and
.CWBI win\(hyfile > <
will be searched,
.EM instead
of the path name specified by
.CWBI file > < ,
for each of the
.CW MS\(hyDOS \*(rg,
.CW Apple \*(rg
.CW Macintosh \*(rg,
.CW UNIX \(tm
and
.CW MS\(hyWindows \*(rg
operating systems, respectively; see the
.pdfmark-manual ,
for further details.
.LP
Other than the use of these additional options,
which specify that the reference destination is in an external PDF file,
the behaviour of the
.CW pdfhref
.CWB L \& \& \(rq \(lq
operator, with the
.CWB -F \& \& \~\c
.CWBI file > <
option, remains identical to its behaviour
.EM without
this option,
.XR link-intern ), (
with respect to the interpretation of other options,
the handling of the
.CWI descriptive \& \& \~\c
.CWI text
arguments, and the formatting of the displayed reference.
.LP
Once again, since the appearance of the reference is determined by
factors specified in the document reference map,
and also by the formatting rules in effect when the reference is placed,
the presentation of an example of the placing of
a reference to an external destination will be deferred,
until the formatting mechanism has been explained,
.XR set-format ). (
.NH 3
.XN -N add-weblink -- Linking to Internet Resources
.LP
In addition to supporting the creation of cross references
to named destinations in PDF documents, the
.CW pdfhref
macro also has the capability to create active links to Internet resources,
or indeed to
.EM any
resource which may be specified by a Uniform Resource Identifier,
(which is usually abbreviated to the acronym \(lqURI\(rq,
and sometimes also referred to as a Uniform Resource Locator,
or \(lqURL\(rq).
.LP
Since the mechanism for creating a link to a URI differs somewhat
from that for creating PDF references, the
.CW pdfhref
macro is invoked with the
.CWB W \& \& \(rq \(lq
(for \(lqweb\(hylink\(rq) operator, rather than the
.CWB L \& \& \(rq \(lq
operator; nevertheless, the invocation syntax is similar, having the form
.QP
.fam C
.B .pdfhref
.B W
.B -D \& [
.BI URI >] <
.B -P \& [
.BI prefix-text >] <
.B -A \& [
.BI affixed-text >] <
\e
.br
\0\0\0
.B -- ] [
.I "descriptive text ...\&"
.LP
where the optional
.CWB -D
.CWBI URI > <
modifier specifies the address for the target Internet resource,
in any appropriate
.EM "Uniform Resource Identifier"
format, while the
.CWI descriptive
.CWI text
argument specifies the text which is to appear in the \(lqhot\(hyspot\(rq
region, and the
.CWB -P
.CWBI prefix\(hytext > <
and
.CWB -A
.CWBI affixed\(hytext > <
options have the same effect as in the case of local document links,
.XR link-intern ). (
.LP
Notice that it is not mandatory to include the
.CWB -D
.CWBI URI > <
in the link specification; if it
.EM is
specified, then it is not necessary for the URI to appear,
in the running text of the document \(em the
.CWI descriptive
.CWI text
argument exactly defines the text
which will appear within the \(lqhot\(hyspot\(rq region,
and this need not include the URI.
However, if the
.CWB -D \& \& \~\c
.CWBI URI > <
specification is omitted, then the
.CWI descriptive
.CWI text
argument
.EM must
be an
.EM exact
representation of the URI, which
.EM will ,
therefore, appear as the entire content of the \(lqhot\(hyspot\(rq.
For example, we could introduce a reference to
.pdfhref W -D \*[GROFF-WEBSITE] -A , the groff web site
in which the actual URI is concealed, by using mark up such as:\(en
.DS I
.CW
For example, we could introduce a reference to
\&.pdfhref W -D \*[GROFF-WEBSITE] -A , the groff web site
in which the actual URI is concealed,
.DE
Alternatively,
to refer the reader to the groff web site,
making it obvious that the appropriate URI is
.pdfhref W -A , \*[GROFF-WEBSITE]
the requisite mark up might be:\(en
.DS I
.CW
to refer the reader to the groff web site,
making it obvious that the appropriate URI is
\&.pdfhref W -A , \*[GROFF-WEBSITE]
the requisite mark up might be:\e(en
.DE
.NH 3
.XN -N set-format -- Establishing a Format for References
.LP
There are two principal aspects to be addressed,
when defining the format to be used when displaying references.
Firstly, it is desirable to provide a visual cue,
to indicate that the text describing the reference is imbued
with special properties \(em it is dynamically linked to the reference
destination \(em and secondly, the textual content should
describe where the link leads, and ideally,
it should also describe the content of the reference destination.
.LP
The visual cue,
that a text region defines a dynamically linked reference,
is most commonly provided by printing the text within the active
region in a distinctive colour.
This technique will be employed automatically by the
.CW pdfhref
macro \(em
.XR set-colour
\(em unless the user specifically chooses to adopt, and implement,
some alternative strategy.
.NH 4
.XN -N set-colour -- Using Colour to Demarcate Link Regions
.LP
Typically, when a PDF document contains
.EM active
references to other locations, either within the same document,
or even in other documents, or on the World Wide Web,
it is usually desirable to make the regions
where these active links are placed stand out from the surrounding text.
.NH 4
.XN -N user-format -- Specifying Reference Text Explicitly
.NH 4
.XN -N auto-format -- Using Automatically Formatted Reference Text
.NH 4
.XN -N custom-format -- Customising Automatically Formatted Reference Text
.LP
It is incumbent on the user,
if employing automatic formatting of the displayed reference,
.XR set-format ), (
to ensure that an appropriate reference definition
is created for the reference destination,
and is included in the reference map for the document
in which the reference will appear;
thus, it may be easiest to
.EM always
use manual formatting for external references.
.NH 3
.XN Problematic Links
.LP
Irrespective of whether a
.CW pdfhref
reference is placed using the
.CWB L \& \& \(rq \(lq
operator, or the
.CWB W \& \& \(rq \(lq
operator, there may be occasions when the resulting link
does function as expected.
A number of scenarios, which are known to be troublesome,
are described below.
.NH 4
.XN -N page-trap -- Links with a Page Transition in the Active Region
.LP
When a link is placed near the bottom of a page,
it is possible that its active region, or \(lqhot\(hyspot\(rq,
may extend on to the next page.
In this situation, a page trap macro is required
to intercept the page transition, and to restart the mapping of
the \(lqhot\(hyspot\(rq boundary on the new page.
.LP
The
.CW pdfmark
macro package includes a suitable page trap macro, to satisfy this requirement.
However, to avoid pre\(hyempting any other requirement the user may have for
a page transition trap, this is
.EM not
installed as an active page trap,
unless explicitly requested by the user.
.LP
To enable proper handling of page transitions,
which occur within the active regions of reference links,
the user should:\(en
.QS
.nr ITEM 0 1
.IP \n+[ITEM].
Define a page transition macro, to provide whatever features may be required,
when a page transition occurs \(em e.g.\& printing footnotes,
adding page footers and headers, etc.
This macro should end by setting the output position at the correct
vertical page offset, where the printing of running text is to restart,
following the page transition.
.IP \n+[ITEM].
Plant a trap to invoke this macro, at the appropriate vertical position
marking the end of normal running text on each page.
.KS
.IP \n+[ITEM].
Initialise the
.CW pdfhref
hook into this page transition trap, by invoking
.RS
.IP
.fam C
.B "pdfhref I -PT"
.BI macro-name > <
.LP
where
.CWBI macro-name > <
is the name of the user supplied page trap macro,
to ensure that
.CW pdfhref
will correctly restart mapping of active link regions,
at the start of each new page.
.KE
.RE
.QE
.LP
It may be observed that this initialisation of the
.CW pdfhref
page transition hook is, typically, required only once
.EM before
document formatting begins.
Users of document formatting macro packages may reasonably expect that
this initialisation should be performed by the macro package itself.
Thus, writers of such macro packages which include
.CW pdfmark
bindings, should provide appropriate initialisation,
so relieving the end user of this responsibility.
The following example, abstracted from the sample
.CW ms
binding package,
.CW spdf.tmac ,
illustrates how this may be accomplished:\(en
.DS I
.CW
\&.\e" groff "ms" provides the "pg@bottom" macro, which has already
\&.\e" been installed as a page transition trap.  To ensure proper
\&.\e" mapping of "pdfhref" links which overflow the bottom of any
\&.\e" page, we need to install the "pdfhref" page transition hook,
\&.\e" as an addendum to this macro.
\&.
\&.pdfhref I -PT pg@bottom
.DE
.NH 2
.XN -N add-note -- Annotating a PDF Document using Pop-Up Notes
.NH 2
.XN -N pdfsync -- Synchronising Output and \F[C]pdfmark\F[] Contexts
.LP
It has been noted previously, that the
.CW pdfview
macro,
.XR docview ), (
the
.CW pdfinfo
macro,
.XR docinfo ), (
and the
.CW pdfhref
macro, when used to create a document outline,
.XR add-outline ), (
do not immediately write their
.CW pdfmark
output to the PostScript\*(rg data stream;
instead, they cache their output, in a
.CW groff
diversion, in the case of the
.CW pdfview
and
.CW pdfinfo
macros, or in an ordered collection of strings and numeric registers,
in the case of the document outline,
until a more appropriate time for copying it out.
In the case of
.CW pdfview
and
.CW pdfinfo
\(lqmeta\(hydata\(rq,
this \(lqmore appropriate time\(rq is explicitly chosen by the user;
in the case of document outline data,
.EM some
cached data may be implicitly written out as the document outline is compiled,
but there will
.EM always
be some remaining data, which must be explicitly flushed out, before the
.CW groff
formatting process is allowed to complete.
.LP
To allow the user to choose when cached
.CW pdfmark
data is to be flushed to the output stream, the
.CW pdfmark
macro package provides the
.CW pdfsync
macro, (to synchronise the cache and output states).
In its simplest form, it is invoked without arguments, i.e.
.QP
.fam C
.B .pdfsync
.LP
This form of invocation ensures that
.EM both
the \(lqmeta\(hydata cache\(rq, containing
.CW pdfview
and
.CW pdfinfo
data,
.EM and 
the \(lqoutline cache\(rq,
containing any previously uncommitted document outline data,
are flushed; ideally, this should be included in a
.CW groff
\(lqend macro\(rq, to ensure that
.EM both
caches are flushed, before
.CW groff
terminates.
.LP
Occasionally,
it may be desirable to flush either the \(lqmeta\(hydata cache\(rq,
without affecting the \(lqoutline cache\(rq, or vice\(hyversa,
at a user specified time, prior to reaching the end of the document.
This may be accomplished, by invoking the
.CW pdfsync
macro with an argument, i.e.
.QP
.fam C
.B ".pdfsync M"
.LP
to flush only the \(lqmeta\(hydata cache\(rq, or
.QP
.fam C
.B ".pdfsync O"
.LP
to flush only the \(lqoutline cache\(rq.
.LP
The \(lqmeta\(hydata cache\(rq can normally be safely flushed
in this manner, at any time
.EM after
output of the first page has started;
(it may cause formatting problems,
most notably the appearance of unwanted white space, if flushed earlier,
or indeed, if flushed immediately after a page transition,
but before the output of the content on the new page has commenced).
Caution is required, however, when explicitly flushing the
\(lqoutline cache\(rq, since if the outline is to be
subsequently extended, then the first outline entry after flushing
.EM must
be specified at level 1.
Nevertheless, such explict flushing may occasionally be necessary;
for example, the
.CW TC
macro in the
.CW spdf.tmac
package,
.XR using-spdf ), (
invokes
.CW ".pdfsync\ O" \(rq \(lq
to ensure that the outline for the \(lqbody\(rq section of the document
is terminated,
.EM before
it commences the formatting of the table of contents section.
.bp
.NH 1
.XN -N pdf-layout -- PDF Document Layout
.LP
The
.CW pdfmark
macros described in the preceding section,
.XR pdf-features ), (
provide no inherent document formatting capability of their own.
However,
they may be used in conjunction with any other
.CW groff
macro package of the user's choice,
to add such capability.
.LP
In preparing this document, the standard
.CW ms
macro package, supplied as a component of the GNU Troff distribution,
has been employed.
To facilitate the use of the
.CW pdfmark
macros with the
.CW ms
macros,
a binding macro package,
.CW spdf.tmac ,
has been created.
The use of this binding macro package is described in the following section,
.XR using-spdf ); (
it may also serve as an example to users of other standard
.CW groff
macro packages,
as to how the
.CW pdfmark
macros may be employed with their chosen primary macro package.
.NH 2
.XN -N using-spdf -- Using \F[C]pdfmark\F[] Macros with the \F[C]ms\F[] Macro Package
.LP
The use of the binding macro package,
.CW spdf.tmac ,
allows for the use of the
.CW pdfmark
macros in conjunction with the
.CW ms
macros,
simply by issuing a
.CW groff
command of the form
.QP
.fam C
groff -Tps
.B -mspdf
.I "-options ...\&" ] [
file(s) ...
.LP
When using the
.CW spdf.tmac
package, the
.CW groff
input files may be marked up using any of the standard
.CW ms
macros to specify document formatting,
while PDF features may be added,
using any of the
.CW pdfmark
macros described previously,
.XR pdf-features ). (
Additionally,
.CW spdf.tmac
defines a number of convenient extensions to the
.CW ms
macro set, to better accomodate the use of PDF features within the
.CW ms
formatting framework,
and to address a number of
.CW ms
document layout issues,
which require special handling when producing PDF documents.
These additional macros,
and the issues they are intended to address,
are described below.
.NH 3
.XN \F[C]ms\F[] Section Headings in PDF Documents
.LP
Traditionally,
.CW ms
provides the
.CW NH
and
.CW SH
macros, to specify section headings.
However,
there is no standard mechanism for generating a
table of contents entry based on the text of the section heading;
neither is there any recognised standard method for establishing a
cross reference link to the section.
.LP
To address this
.CW ms
limitation,
.CW spdf.tmac
defines the
.CW XN
macro,
.XR xn-macro ), (
to be used in conjunction with the
.CW NH
macro.
.NH 4
.XN -N xn-macro -- The \F[C]XN\F[] Macro
.NH 1
.XN The PDF Publishing Process
.NH 2
.XN -N do-xref -- Resolving Cross References
.NH 3
.XN -N create-map -- Creating a Document Reference Map
.NH 3
.XN -N import-map -- Deploying a Document Reference Map
.TC
