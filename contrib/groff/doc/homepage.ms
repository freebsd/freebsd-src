.\" This file gives a small example how a web page could look like if created
.\" with groff.
.\"
.\"
.\" To make it work with other output devices also, we include arkup.tmac
.\" directly.
.mso arkup.tmac
.\"
.LP
.\"
.\"  set a longer line than the default as we have some long ftp addresses
.\"  which groff cannot split
.nr LL 7i
.TL
Home of Groff (GNU Troff).
.sp 1i
.LP
GNU Troff
.URL (Groff) http://groff.ffii.org/
\(em a
.URL GNU http://www.gnu.org/
project.
Hosted by
.URL FFII http://ffii.org/ .
.br
.LINE
.LP
Groff (GNU Troff) is a document processor which reads plain text mixed with
formatting commands and produces formatted output.
.LP
Groff now supports HTML for the Web.
Please see
.URL "Groff Web Tips" "#Web Tips" .
.sp
.SH
Download
.LP
.URL Germany "ftp://groff.ffii.org/pub/groff/"
|
.URL USA "ftp://ftp.gnu.org/gnu/groff/"
.LP
.URL License http://www.gnu.org/copyleft/gpl.html
|
.URL "Mailing list" http://ffii.org/mailman/listinfo/groff/
|
.URL "Groff for Windows" "http://www.cs.vu.nl/~tmgil/misc/wingroff.html"
.sp
User issues lead:
.MAILTO Ted.Harding@nessie.mcc.ac.uk "Ted Harding"
.br
Technical issues lead:
.MAILTO wl@gnu.org "Werner Lemberg"
.SH
README
.LP
This is the GNU groff document formatting system.
The version number is given in the file VERSION.
.LP
Included in this release are implementations of troff, pic, eqn, tbl, refer,
the -man macros and the -ms macros, and drivers for PostScript, TeX dvi
format, HP LaserJet 4 printers, HTML format (still alpha), and
typewriter-like devices.
Also included is a modified version of the Berkeley -me macros, an enhanced
version of the X11 xditview previewer, and an implementation of the -mm
macros contributed by
.MAILTO jh@axis.se "J\(:orgen H\(:agg" .
.LP
See the file INSTALL for installation instructions.
You will require a C++ compiler.
.LP
The file NEWS describes recent user-visible changes to groff.
.LP
Groff is free software.
See the file COPYING for copying permission.
.LP
The file PROBLEMS describes various problems that have been encountered in
compiling, installing, and running groff.
.LP
For the moment, the documentation assumes that you are already familiar with
the Unix versions of troff, -man, -ms and the preprocessors.
.LP
The most recent released version of groff is always available by anonymous
ftp from ftp.gnu.org in the directory pub/gnu/groff.
.LP
A CVS repository is now available, containing the current development
version of groff.
You can access it with the commands:
.sp
.RS
.nf
\fC
export CVSROOT=:pserver:anoncvs@anoncvs.ffii.org:/var/cvs
cvs login
cvs -z9 co groff
\fR
.fi
.RE
.sp
.LP
(if the prompt for the password appears, just press the enter key).
After a successful login you no longer need the first two commands; an
update of a checked out repository should be done with:
.sp
.RS
\fC
cvs -z9 update -dP
\fR
.RE
.sp
.LP
Please read the info pages of cvs for further details.
.LP
Alternatively, you can download snapshots (which are updated twice a day)
from
.URL here ftp://ftp.ffii.org/pub/groff/devel/groff-current.tar.gz
or a diff file relative to the latest official groff release as:
.sp
.RS
\fC
ftp://ftp.ffii.org/pub/groff/devel/groff-[version]-current.diff.gz
\fR
.RE
.sp
.LP
Please report bugs using the form in the file BUG-REPORT; the idea of this
is to make sure that FSF has all the information it needs to fix the bug.
At the very least, read the BUG-REPORT form and make sure that you supply
all the information that it asks for.
Even if you are not sure that something is a bug, report it using
BUG-REPORT: this will enable us to determine whether it really is a bug or
not.
.LP
Three mailing lists are available:
.sp
.RS
.nf
\fC
bug-groff@gnu.org      for reporting bugs
groff@gnu.org          for general discussion of groff
groff-commit@ffii.org  a read-only list showing logs of
                       commitments to the CVS repository
\fP
.fi
.RE
.sp
.LP
Note that groff@gnu.org is an alias for
.MAILTO groff@ffii.org groff@ffii.org ;
you must be subscribed to the `groff' and `groff-commit' lists to send mails.
.LP
To subscribe, send e-mail to [list]-request@[domain] (example:
.MAILTO groff-request@ffii.org groff-request@ffii.org )
with the word `subscribe' in either the
subject or body of the e-mail (don't include the quotes).
.LP
GNU groff was written by
.MAILTO jjc@jclark.com "James Clark" .
It is now maintained by
.MAILTO Ted.Harding@nessie.mcc.ac.uk "Ted Harding"
and
.MAILTO wl@gnu.org "Werner Lemberg" .
.br
.
.\" EOF
