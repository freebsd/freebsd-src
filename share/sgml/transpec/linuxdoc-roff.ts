<!--

  $FreeBSD$

  Copyright (C) 1996
       John R. Fieber.  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY JOHN R. FIEBER AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL JOHN R. FIEBER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

-->

<!--

  This is an instant(1) translation specification to turn an
  SGML document marked up according to the linuxdoc DTD into
  a document suitable for processing with groff(1) using mm
  macros.
  
  Special thanks to Chuck Robey <chuckr@freebsd.org> for helping
  to unravel the mysteries of groff.

-->

<!DOCTYPE transpec PUBLIC "-//FreeBSD//DTD transpec//EN" [

<!ENTITY r.pass CDATA "1">
<!ENTITY r.passw CDATA "2">
<!ENTITY r.phack CDATA "3">
<!ENTITY r.label CDATA "4">
<!ENTITY r.initr CDATA "5">

<!ENTITY gt CDATA ">">
<!ENTITY lt CDATA "<">
<!ENTITY amp CDATA "&">

<!ENTITY cmap SYSTEM "/usr/share/sgml/transpec/roff.cmap">
<!ENTITY sdata SYSTEM "/usr/share/sgml/transpec/roff.sdata">

<!ENTITY family CDATA "P">

]>

<transpec>

<!-- Character and SDATA entity mapping -->
<cmap>&cmap;</cmap>
<smap>&sdata;</smap>

<!-- Transform rules -->

<!-- Inside a HEADING, all these need to be suppressed or deferred
     to a later time. -->
<rule>
<match>
<gi>EM IT BF SF SL TT CPARAM LABEL REF PAGEREF CITE URL HTMLURL NCITE EMAIL IDX CDX F X
<relation>ancestor HEADING
</rule>

<rule>
<match>
<gi>LINUXDOC
<action>
<start>^.\" Generated ${date} using ${transpec}
.\" by ${user}@${host}
.if t \{\
.  nr W 6i
.  nr O 1.25i
.  nr L 11i
.\}
.if n .nr W 79n
.so /usr/share/tmac/tmac.m
.nr Pt 0
.ie t \{\
.  fam &family;
.  ds HP 16 14 12 10 10 10 10
.  ds HF HB HB HB HBI HI HI HI 
.\}
.el \{\
.  SA 0
.  ftr C R
.\}
.PH "'${_followrel descendant TITLE &r.pass;}''%'"
.nr N 1     <!-- header at the bottom of the first page -->
.nr Pgps 0  <!-- header/footer size immune from .S -->
.nr Hy 1    <!-- hypenation on -->
.nr H0 0 1  <!-- Part counter -->
.af H0 I    
${_followrel descendant LABEL &r.initr;}</start>
<end>^</end>
</rule>

<!-- This is used with the above ${_followrel ...} to insert the
     .INITR command which opens a file for writing cross reference
     information.  If there are no <label> tags, we don't want to
     bother with this. -->
<rule id="&r.initr;">
<match>
<gi>_initr
<action>
<start>^.INITR "${filename}"^</start>
</rule>

<rule>
<match>
<gi>ARTICLE
<action>
<start>^.nr Hb 4
.nr Hs 4^</start>
<end>^.bp
.TC^</end>
</rule>

<rule>
<match>
<gi>REPORT BOOK
<action>
<start>^.nr Cl 3    <!-- TOC goes to level 3 -->
.nr Hb 5
.nr Hs 5^</start>
<end>
^.bp
.TC^</end>
</rule>

<rule>
<match>
<gi>NOTES
</rule>

<rule>
<match>
<gi>MANPAGE
</rule>

<rule>
<match>
<gi>TITLEPAG
<action>
<start>^\&
.if t .SP 1i^</start>
<end>^.SP 3^</end>
</rule>

<rule>
<match>
<gi>TITLE
<action>
<start>^.if t .S 18
.DS C F
.if t .fam H
.B
.if t .SA 0^</start>
<end>^.if t .SA 1
.R
.if t .fam &family;
.DE
.if t .S D^</end>
</rule>

<rule>
<match>
<gi>SUBTITLE
</rule>

<rule>
<match>
<gi>DATE
<action>
<start>^.DS C F^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>ABSTRACT
<action>
<start>^.SP 3
.DS C
.B Abstract
.DE
.DS I F^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>AUTHOR
<action>
<start>^.DS C F^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>NAME
</rule>

<rule>
<match>
<gi>AND
<action>
<start>^.br^</start>
</rule>

<rule>
<match>
<gi>THANKS
<action>
<start>\*F
.FS^</start>
<end>^.FE^</end>
</rule>

<rule>
<match>
<gi>INST
<action>
<start>^.br^</start>
</rule>

<rule>
<match>
<gi>NEWLINE
<action>
<start>^.br^</start>
</rule>

<rule id="&r.label;">
<match>
<gi>LABEL
<action>
<start>^.SETR "${ID}"^</start>
</rule>

<rule>
<match>
<gi>HEADER
</rule>

<rule>
<match>
<gi>LHEAD
<action>
<start>^.EH '</start>
<end>'''^</end>
</rule>

<rule>
<match>
<gi>RHEAD
<action>
<start>^.OH '''</start>
<end>'^</end>
</rule>

<rule>
<match>
<gi>COMMENT
<action>
<start>^(*^</start>
<end>^*)^</end>
</rule>

<rule>
<match>
<gi>APPENDIX
<action>
<start>^.af H1 A^</start>
</rule>

<rule>
<match>
<gi>TOC
</rule>

<rule>
<match>
<gi>LOF
</rule>

<rule>
<match>
<gi>LOT
</rule>

<rule>
<match>
<gi>PART
</rule>

<rule>
<match>
<gi>CHAPT
<action>
<start>^.if t .SK
${_set sl 1}</start>
</rule>

<rule>
<match>
<gi>SECT
<relation>ancestor BOOK
<action>
<start>${_set sl 2}</start>
</rule>

<rule>
<match>
<gi>SECT
<action>
<start>${_set sl 1}</start>
</rule>

<rule>
<match>
<gi>SECT1
<relation>ancestor BOOK
<action>
<start>${_set sl 3}</start>
</rule>

<rule>
<match>
<gi>SECT1
<action>
<start>${_set sl 2}</start>
</rule>

<rule>
<match>
<gi>SECT2
<relation>ancestor BOOK
<action>
<start>${_set sl 4}</start>
</rule>

<rule>
<match>
<gi>SECT2
<action>
<start>${_set sl 3}</start>
</rule>

<rule>
<match>
<gi>SECT3
<relation>ancestor BOOK
<action>
<start>${_set sl 5}</start>
</rule>

<rule>
<match>
<gi>SECT3
<action>
<start>${_set sl 4}</start>
</rule>

<rule>
<match>
<gi>SECT4
<relation>ancestor BOOK
<action>
<start>${_set sl 6}</start>
</rule>

<rule>
<match>
<gi>SECT4
<action>
<start>${_set sl 5}</start>
</rule>

<rule>
<match>
<gi>HEADING
<context>PART
<action>
<start>^.if t .SK
\&
.if t .fam H
.SP 3i
.if t .S 24
Part \n+(H0
.SP 1i
.if t .S 36^</start>
<end>^.if t .S D
.if t .fam &family;
.if t .SK^</end>
</rule>

<rule>
<match>
<gi>HEADING
<action>
<start>^.H ${sl} "</start>
<end>"
${_followrel child LABEL &r.label}</end>
</rule>

<!--
<rule>
<match>
<gi>HEADING
<action>
<start>^.br
.di fbsd-head^</start>
<end>^.br
.di
.asciify fbsd-head
.H ${sl} \*[fbsd-head]
${_followrel child LABEL &r.label}</end>
</rule>
-->

<!-- A paragraph immediately following a <tag> in a <descrip>. -->
<rule>
<match>
<gi>P
<relation>sibling-1 TAG
</rule>

<rule>
<match>
<gi>P
<action>
<start>${_notempty &r.phack;}</start>
</rule>

<!-- Completely empty paragraphs. -->
<rule id="&r.phack;">
<match>
<gi>_phack
<action>
<replace>^.P^</replace>
</rule>


<rule>
<match>
<gi>ITEMIZE
<action>
<start>^.BL^</start>
<end>^.LE^</end>
</rule>

<rule>
<match>
<gi>ENUM
<action>
<start>^.AL^</start>
<end>^.LE^</end>
</rule>

<rule>
<match>
<gi>DESCRIP
<action>
<start>^.BVL \n(Li*2/1 \n(Li^</start>
<end>^.LE^</end>
</rule>

<rule>
<match>
<gi>ITEM
<action>
<start>^.LI^</start>
</rule>

<rule>
<match>
<gi>TAG
<action>
<start>^.LI "</start>
<end>"^</end>
</rule>

<rule>
<match>
<gi>CITE
<action>
<start>^.\[
${ID}
.\]^</start>
</rule>

<rule>
<match>
<gi>NCITE
<action>
<start>^.\[
${ID}
.\]
(${NOTE})</start>
</rule>

<rule>
<match>
<gi>FOOTNOTE
<action>
<start>\*F
.FS^</start>
<end>^.FE^</end>
</rule>

<rule>
<match>
<gi>SQ
<action>
<start>\*Q</start>
<end>\*U</end>
</rule>

<rule>
<match>
<gi>LQ
<action>
<start>^.if t .br
.if t .S -2
.DS I F^</start>
<end>^.DE
.if t .S D^</end>
</rule>

<rule>
<match>
<gi>EM
<action>
<start>\fI</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>BF
<action>
<start>\fB</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>IT
<action>
<start>\fI</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>SF
<action>
<start>\fR</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>SL
<action>
<start>\fI</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>TT
<action>
<start>\fC</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>CPARAM
<action>
<start>\fI<</start>
<end>>\fR</end>
</rule>

<!-- A URL with a NAME attribute -->
<rule>
<match>
<gi>URL
<attval>NAME .
<action>
<start>${NAME}\*F
.FS
\fC&lt;URL:${URL}&gt;\fP
.FE
\&</start>
</rule>

<!-- A URL without a NAME attribute -->
<rule>
<match>
<gi>URL
<action>
<start>\fC&lt;URL:${URL}&gt;\fP</start>
</rule>

<rule>
<match>
<gi>HTMLURL
<action>
<start>${NAME}</start>
</rule>

<rule>
<match>
<gi>REF
<attval>NAME .
<action>
<start>\fI${NAME}\fP (section\~
.GETHN "${ID}"
, page\~
.GETPN "${ID}"
)</start>
</rule>

<rule>
<match>
<gi>REF
<action>
<start>\fI${ID}\fP (section\~
.GETHN "${ID}"
, page\~
.GETPN "${ID}"
)</start>
</rule>

<rule>
<match>
<gi>PAGEREF
<action>
<start>^.GETPN "${ID}"
\&</start>
</rule>

<rule>
<match>
<gi>X
</rule>

<rule>
<match>
<gi>MC
</rule>

<rule>
<match>
<gi>BIBLIO
<action>
<start>^.\[
\$LIST\$
.\]^</start>
</rule>

<rule>
<match>
<gi>VERB CODE
<action>
<start>^.if t .br
.if t .S -2
.DS I
.fam C^</start>
<end>^.DE
.if t .S D^</end>
</rule>

<rule>
<match>
<gi>TSCREEN
<relation>child VERB
</rule>

<rule>
<match>
<gi>TSCREEN
<action>
<start>^.if t .br
.if t .S -2
.DS I
.fam C^</start>
<end>^.DE
.if t .S D^</end>
</rule>

<rule>
<match>
<gi>QUOTE
<action>
<start>^.DS I F^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>DEF
<action>
<start>^.sp
.nr def \n\[def\]+1
.B "Definition \n\[def\] "^</start>
<end>^.ft P
.sp^</end>
</rule>

<rule>
<match>
<gi>PROP
<action>
<start>^.sp
.nr prop \n\[prop\]+1
.B "Proposition \n\[prop\] "^</start>
<end>^.ft P
.sp^</end>
</rule>

<rule>
<match>
<gi>LEMMA
<action>
<start>^.sp
.nr lemma \n\[lemma\]+1
.B "Lemma \n\[lemma\] "^</start>
<end>^.ft P
.sp^</end>
</rule>

<rule>
<match>
<gi>COROLL
<action>
<start>^.sp
.nr coroll \n\[coroll\]+1
.B "Corolloary \n\[coroll\] "^</start>
<end>^.ft P
.sp^</end>
</rule>

<rule>
<match>
<gi>PROOF
<action>
<start>^.sp
.nr proof \n\[proof\]+1
.B "Proof \n\[proof\] "^</start>
<end>^.ft P
.sp^</end>
</rule>

<rule>
<match>
<gi>THEOREM
<action>
<start>^.sp
.nr theorem \n\[theorem\]+1
.B "Theorem \n\[theorem\] "^</start>
<end>^.ft P
.sp^</end>
</rule>

<rule>
<match>
<gi>THTAG
<action>
<start>^.B
(</start>
<end>)
.I^</end>
</rule>

<rule>
<match>
<gi>F
</rule>

<rule>
<match>
<gi>DM
<action>
<start>^.DS L^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>EQ
<action>
<start>^.DS L^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>FR
</rule>

<rule>
<match>
<gi>NU
<action>
<start>{</start>
<end>} over </end>
</rule>

<rule>
<match>
<gi>DE
<action>
<start>{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>LIM
</rule>

<rule>
<match>
<gi>OP
</rule>

<rule>
<match>
<gi>LL
<action>
<start>from {</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>UL
<action>
<start>to {</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>OPD
</rule>

<rule>
<match>
<gi>PR
<action>
<start> prod </start>
</rule>

<rule>
<match>
<gi>IN
<action>
<start> int </start>
</rule>

<rule>
<match>
<gi>SUM
<action>
<start>	sum </start>
</rule>

<rule>
<match>
<gi>ROOT
<action>
<start> sqrt {</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>AR
<action>
<start>^.TS
center, tab(|) ;
${CA}.^</start>
<end>^.TE^</end>
</rule>

<rule>
<match>
<gi>ARR
</rule>

<rule>
<match>
<gi>ARC
<action>
<start>|</start>
</rule>

<rule>
<match>
<gi>SUP
<action>
<start> sup {</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>INF
<action>
<start> sub {</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>UNL
<action>
<start>{</start>
<end>} under </end>
</rule>

<rule>
<match>
<gi>OVL
<action>
<start>{</start>
<end>} bar </end>
</rule>

<rule>
<match>
<gi>RF
<action>
<start> bold{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>V
<action>
<start>{</start>
<end>} vec </end>
</rule>

<rule>
<match>
<gi>FI
<action>
<start>\fI</start>
<end>\fR</end>
</rule>

<rule>
<match>
<gi>PHR
<action>
<start> roman }</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>TU
<action>
<start>^.br^</start>
</rule>

<rule>
<match>
<gi>FIGURE
</rule>

<rule>
<match>
<gi>EPS
<action>
<start>^.if t .PSPIC ${FILE}
.if n .sp 4^</start>
</rule>

<rule>
<match>
<gi>PH
<action>
<start>^.sp ${VSPACE}^</start>
</rule>

<rule>
<match>
<gi>CAPTION
<action>
<start>^.sp
.ce^</start>
</rule>

<rule>
<match>
<gi>TABLE
<action>
<start>^.DF
.R^</start>
<end>^.DE^</end>
</rule>

<rule>
<match>
<gi>TABULAR
<action>
<start>^.TS
center, tab(|) ; 
${CA}.^</start>
<end>^.TE^</end>
</rule>

<rule>
<match>
<gi>ROWSEP
<action>
<start>^</start>
</rule>

<rule>
<match>
<gi>COLSEP
<action>
<start>|</start>
</rule>

<rule>
<match>
<gi>HLINE
<action>
<start>^_^</start>
</rule>

<rule>
<match>
<gi>SLIDES
<action>
<start>^.nr PS 18^</start>
</rule>

<rule>
<match>
<gi>SLIDE
<action>
<end>^.bp
\&^</end>
</rule>

<rule>
<match>
<gi>LETTER
<action>
<start>^.nf^</start>
<end>^</end>
</rule>

<rule>
<match>
<gi>FROM
<action>
<start>^From: </start>
</rule>

<rule>
<match>
<gi>TO
<action>
<start>^To: </start>
</rule>

<rule>
<match>
<gi>ADDRESS
<action>
<start>^.de Ad</start>
<end>^..^</end>
</rule>

<rule>
<match>
<gi>EMAIL
<action>
<start><</start>
<end>></end>
</rule>

<rule>
<match>
<gi>SUBJECT
<action>
<start>^Subject: </start>
</rule>

<rule>
<match>
<gi>SREF
<action>
<start>^Sref: </start>
</rule>

<rule>
<match>
<gi>RREF
<action>
<start>^In-Reply-To: </start>
</rule>

<rule>
<match>
<gi>CC
<action>
<start>^cc: </start>
</rule>

<rule>
<match>
<gi>OPENING
<action>
<start>^.fi
.LP^</start>
</rule>

<rule>
<match>
<gi>CLOSING
<action>
<start>^.LP^</start>
</rule>

<rule>
<match>
<gi>ENCL
<action>
<start>^.XP
encl: </start>
</rule>

<rule>
<match>
<gi>PS
<action>
<start>^.LP
p.s.</start>
</rule>

<!-- Pass the content through -->
<rule id="&r.pass">
<match>
<gi>_pass-text
</rule>

<rule id="&r.passw">
<match>
<gi>_pass-word
<action>
<replace>${each_C} </replace>
</rule>

</transpec>
