.\"------------
.\" $Id: 0.MT,v 2.0 2000/09/14 03:40:25 jhaegg Exp $
.\" Cover sheet. Memorandum type 0-3 and "string".
.\"------------
.if !r Au .nr Au 1
.nr cov*mt0-ind 1.1c
.de cov@print-title
.MOVE 4.8c 1.5c
.S 8
subject:
.sp -1.1
.S
.PGFORM
.ft \\*[@sdf_font]
.ll 9c
.fi
.cov*title
.ft
.ll
.nf
.if d cov*title-charge-case \fBCharge Case \\*[cov*title-charge-case]\fP
.if d cov*title-file-case \fBFile Case \\*[cov*title-file-case]\fP
.fi
..
.\"------------
.de cov@print-authors
.MOVE 5.7c 13.3c
.nf
.S 8
\\$1:
.br
.S
.sp -1
.in 0.8c
.ft \\*[@sdf_font]
.nr cov*i 0 1
.while \\n+[cov*i]<=\\n[cov*au] \{\
.	cov@print-au1 \\n[cov*i] 1
.	if \\n[Au] \{\
.		cov@print-au2 \\n[cov*i] 3 4
.		cov@print-au2 \\n[cov*i] 6 5
.		cov@print-au1 \\n[cov*i] 7
.		cov@print-au1 \\n[cov*i] 8
.		cov@print-au1 \\n[cov*i] 9
.	\}
.	if \\n[cov*i]<\\n[cov*au] .SP 1
.\}
.ft
.if r cov*mt-tm-max \{\
.	SP 1
.	nr cov*i 0 1
.	ft \\*[@sdf_font]
TM
.	in 1.5c
.	sp -1
.	while \\n+[cov*i]<\\n[cov*mt-tm-max] \\*[cov*mt-tm!\\n[cov*i]]
.	in
.	ft
.\}
.fi
.PGFORM
..
.\"------------
.\" index arg1
.de cov@print-au1
.if d cov*au!\\$1!\\$2 \\*[cov*au!\\$1!\\$2] 
..
.\"------------
.de cov@print-au2
.\" index arg1 arg2
.if d cov*au!\\$1!\\$2 \\*[cov*au!\\$1!\\$2] \c
.if \\$3=5 .if d cov*au!\\$1!\\$3 x\c
.if d cov*au!\\$1!\\$3 \\*[cov*au!\\$1!\\$3]\c
.br
..
.\"------------
.de cov@print-date
.MOVE 4.8c 13.3c
.S 8
.nf
\\$1:
.br
.S
.sp -1
.in 0.8c
\f[\\*[@sdf_font]]\\*[cov*new-date]\fP
.br
.fi
.PGFORM
..
.\"------------
.de cov@print-firm
.if d cov*firm \{\
.	MOVE 2.8c 0 17.7c
.	S 18
.	rj 1
\fB\\*[cov*firm]\fP
.	S
.	PGFORM
.\}
..
.\"------------
.de cov@print-abstract
.SP 3
.if d cov*abstract \{\
.	misc@ev-keep cov*ev
.	if \\n[cov*abs-ind]>0 \{\
.		in +\\n[cov*abs-ind]u
.		ll -\\n[cov*abs-ind]u
.	\}
.	ce
\fI\\$1\fP
.	SP 1.5
.	fi
.	cov*abstract
.	br
.	ev
.\}
..
.\"-----------------
.ds cov*mt0-txt!1 MEMORANDUM FOR FILE
.ds cov*mt0-txt!2 PROGRAMMER'S NOTES
.ds cov*mt0-txt!3 ENGINEER'S NOTES
.if d cov*default-firm .if !d cov*firm .ds cov*firm \\*[cov*default-firm]
.\"
.if !d cov*mt-printed \{\
.	cov@print-firm
.	cov@print-title subject
.	cov@print-date date
.	cov@print-authors from
.	cov@print-abstract \\*[cov*abs-name]
.	SP 3
.	if (\*[cov*mt-type]>=1)&(\*[cov*mt-type]<=3) \{\
.		ce
\fI\*[cov*mt0-txt!\*[cov*mt-type]]\fP
.		SP 1.5
.	\}
.	if \*[cov*mt-type]=6 \{\
.		ce
\fI\*[cov*mt-type-text]\fP
.		SP 1.5
.	\}
.	pg@enable-top-trap
.	pg@enable-trap
.	ds cov*mt-printed
.\}
