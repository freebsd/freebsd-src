.\"------------
.\" $Id: 4.MT,v 1.2 2000/02/21 20:32:58 jhaegg Exp $
.\" Cover sheet. Memorandum type 4
.\"------------
.de cov@print-title
.if !d cov*title .@error title (.TL) not defined!
.MOVE 2.4c
.S +4
.ad c
.fi
.B
.cov*title
.br
.S
.R
.ad b
.PGFORM
..
.\"------------
.de cov@print-authors
.SP 0.5
.I
.nr cov*i 0 1
.while \\n+[cov*i]<=\\n[cov*au] \{\
.ce
\\*[cov*au!\\n[cov*i]!1]
.br
.\}
.R
.PGFORM
..
.\"------------
.de cov@print-firm
.if !d cov*firm .@error firm (.AF) not defined!
.SP 0.5
.ce
\\*[cov*firm]
..
.\"------------
.de cov@print-abstract
.SP 2
.if d cov*abstract \{\
.	misc@ev-keep cov*ev
.	init@reset
.	if \\n[cov*abs-ind]>0 \{\
.		in +\\n[cov*abs-ind]u
.		ll -\\n[cov*abs-ind]u
.	\}
.	ce
\fI\\*[cov*abs-name]\fP
.	SP 2
.	fi
.	cov*abstract
.	br
.	ev
.\}
..
.\"-----------------
.if d cov*default-firm .if !d cov*firm .ds cov*firm \\*[cov*default-firm]
.if !d cov*mt-printed \{\
.	cov@print-title
.	cov@print-authors
.	cov@print-firm
.	if d cov*abstract \{\
.		if !\n[cov*abs-arg] .cov@print-abstract
.	\}
.	SP 3
.	nr hd*cur-bline \n[nl]
.	ds cov*mt-printed
.	pg@enable-top-trap
.	pg@enable-trap
.\}
.de CS
.pg@disable-top-trap
.SK
.cov@print-title
.cov@print-authors
.cov@print-firm
.cov@print-abstract
..

