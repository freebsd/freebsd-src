.nr PS 12
.nr VS 14
.LP
.TL
Design of grohtml
.sp 1i
.SH
What is grohtml
.LP
Grohtml is a back end for groff which generates html.
The aim of grohtml is to produce respectible html given
fairly typical groff input.
.SH
Limitations of grohtml
.LP
Although basic text can be translated
in a straightforward fashion there are some areas where grohtml
has to try and guess text relationship. In particular whenever
grohtml encounters text tables and indented paragraphs or
two column mode it will try and utilize the html table construct
to preserve columns. Grohtml also attempts to work out which
lines should be automatically formatted by the browser.
Ultimately in trying to make reasonable guesses most of the time
it will make mistakes occasionally.
.PP
Tbl, pic, eqn's are also generated using images which may be
considered a limitation.
.SH
Overview of html.cc
.LP
This file briefly provides an overview of how html.cc operates.
The html device driver works as follows:
.IP (i) .5i
firstly it creates a linked list of all words on a page.
.IP (ii) .5i
it runs through the page and finds the left most margin. Later
on when generating the page it removes the margin.
.IP (iii) .5i
scans a page and builds two kinds of regions ascii text and graphical.
The graphical regions consist of tbl's, eqn's, pic's
(basically anything that cannot be textually displayed).
It will scan through a page to find lines (such as footer etc)
and places these into tiny graphical regions. Certain fonts
also are treated as a graphical region - as html has no easy
equivalent. For example Greek math symbols.
.LP
Finally all graphical regions are translated into png files and
all text regions into html text.
.PP
To give grohtml a sporting chance of accuratly deciding which
is a graphical region and which is text, the front end programs
tbl, eqn, pic have all been tweeked to encapsulate pictures, tables
and equations with the following lines:
.sp
.nf
\f[CR]\&.if '\\*(.T'html' \\X(graphic-start(\c

\&.if '\\*(.T'html' \\X(graphic-end(\c
\fP
.fi
.sp
these appear to grohtml as:
.sp
.nf
\f[CR]\&x X graphic-start

\&...

\&x X graphic-end\fP
.fi
.sp
.LP
In addition to graphic-start and graphic-end there are two
other "special characters" which are used.
.sp
\f[CR]\&x X index:N\fP
.sp
where N is a number. The purpose of this sequence is to stop
devhtml from automatically producing links to headings which
have a header level >N.
The line:
.sp
\f[CR]\&x X html:STRING\fR
.sp
.LP
allows a STRING to be passed through to the output file with
no processing whatsoever. Ie it allows users to include html
commands, via macro, such as:
.sp
\f[CR]\&.URL "Latest Emacs" "ftp://somewonderful.gnu.software"\fP
.sp
.LP
Where the URL macro bundles the info into STRING above.
For more info consult: \f[CR]tmac/tmac.arkup\fP.
.PP
While scanning through a page the html device copies headings and titles
into a list of links which are later written to the beginning
of the html document.
.SH
Table handling code
.LP
Provided that the -t option is not present when grohtml is run the grohtml
driver will attempt to find textual tables and generate html tables.
This allows .RS and .RE commands to operate with auto formatting. It also
should grohtml to process .2C correctly. However, the table handling code
has to examine the troff output and \fIguess\fR when a table starts and
finishes. It is well to know the limitations of this approach as it
sometimes makes the wrong decision.
.LP
Here are some of the rules that grohtml uses for terminating a html table:
.LP
.IP "(i)" .5i
A table will be terminated when grohtml finds line which is all in bold
font (it believes that this is a header which is outside of a table).
This might be considered incorrect behaviour especially if you use .2C
which generates a heading on the left column when the corresponding
right row is blank.
.IP "(ii)" .5i
A table is terminated when grohtml sees that the complete line is
has been spanned by words. Ie no gaps exist.
.IP "(nb)" .5i
the documentation about these rules is particularly incomplete and needs finishing
when time prevails.
.SH
Dependencies
.LP
Grohtml is dependent upon grops, gs which are invoked to
generate all png files. Png files are generated whenever a table, picture,
equation or line is encountered.
