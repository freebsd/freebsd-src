.\"	Version:
.ds RE 1.16
.ig

Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.
mgm is written by Jörgen Hägg (jh@efd.lth.se)

mgm is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

mgm is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Almost complete. The letter format is not included.
Maybe as a separate package.
Should be better as time goes.

Please send bugreports with examples to jh@efd.lth.se.

Naming convention stolen from mgs.
Local names	module*name
Extern names	module@name
Env.var		environ:name
Index		array!index
..
.warn
.\" ####### init ######
.\"	Contents level [0:7], contents saved if heading level <= Cl
.nr Cl 2
.\"	Eject page between LIST OF XXXX if Cp == 0
.nr Cp 0
.\"	Debugflag
.if !r D .nr D 0
.\"	Eject after floating display is output [0:1]
.nr De 0
.\"	Floating keep output [0;5]
.nr Df 5
.\"	space before and after display if == 1 [0:1]
.nr Ds 1
.\"	Eject page
.nr Ej 0
.\"	Eqation lable adjust 0=left, 1=right
.nr Eq 0
.\"	Em dash string
.ds EM \-
.\"	Footnote spacing
.nr Fs 1
.\"	H1-H7	heading counters
.nr H1 0 1
.nr H2 0 1
.nr H3 0 1
.nr H4 0 1
.nr H5 0 1
.nr H6 0 1
.nr H7 0 1
.\"	Heading break level [0:7]
.nr Hb 2
.\"	heading centering level, [0:7]
.nr Hc 0
.\"	header format
.ds HF 2 2 2 2 2 2 2
.\"	heading temp. indent [0:2]
.\"	0 -> 0 indent, left margin
.\"	1 -> indent to right , like .P 1
.\"	2 -> indent to line up with text part of preceding heading
.nr Hi 1
.\"	header pointsize
.ds HP 0 0 0 0 0 0 0
.\"	heading space level [0:7]
.nr Hs 2
.\"	heading numbering type
.\"	0 -> multiple (1.1.1 ...)
.\"	1 -> single
.nr Ht 0
.\"	Unnumbered heading level
.nr Hu 2
.\"	hyphenation in body
.\"	0 -> no hyphenation
.\"	1 -> hyphenation 14 on
.nr Hy 1
.\"	text for toc, selfexplanatory. Look in the new variable section
.ds Lf LIST OF FIGURES
.nr Lf 1
.ds Lt LIST OF TABLES
.nr Lt 1
.ds Lx LIST OF EXHIBITS
.nr Lx 1
.ds Le LIST OF EQUATIONS
.nr Le 0
.\"	List indent, used by .AL
.nr Li 6
.\"	List space, if listlevel > Ls then no spacing will occur around lists.
.nr Ls 99
.\"	Numbering style [0:5]
.if !r N .nr N 0
.\"	numbered paragraphs
.\"	0 == not numbered
.\"	1 == numbered in first level headings.
.nr Np 0
.\"	Format of figure,table,exhibit,equation titles.
.\"	0= ". ", 1=" - "
.nr Of 0
.\"	Page-number, normally same as %.
.nr P 0
.\"	paragraph indent
.nr Pi 5
.\"	paragraph spacing
.nr Ps 1
.\"	paragraph type
.\"	0 == left-justified
.\"	1 == indented .P
.\"	2 == indented .P except after .H, .DE or .LE.
.nr Pt 0
.\"	Reference title
.ds Rp REFERENCES
.\"	Display indent
.nr Si 5
.\"
.ds Tm \(tm
.\"
.\"---------------------------------------------
.\"	Internal global variables
.\"
.\" These two are for cover macro .MT
.\" Change @langage in the national file.
.ds @cur-lib /usr/local/lib/groff/tmac
.\" .ds @language
.\"
.\"	Current pointsize and vertical space, always in points.
.nr @ps 10
.nr @vs 12
.\"	Page length
.ie r L .nr @pl \n[L]
.el .nr @pl 11i
.\"	page width
.ie r W .nr @ll \n[W]
.el .nr @ll 6i
.\"	page offset
.ie r O .nr @po \n[O]
.el .nr @po 1i
.\"
.\" cheating...
.pl \n[@pl]u
.ll \n[@ll]u
.lt \n[@ll]u
.po \n[@po]u
.nr @cur-ll \n[@ll]
.\"
.\" non-zero if escape mechanism is turned off. Used by VERBON/OFF
.nr @verbose-flag 0
.\"---------------------------------------------
.\"	New variables
.\"
.\" Appendix name
.ds App APPENDIX
.\" print appendixheader, 0 == don't
.nr Aph 1
.\"
.\" header prespace level. If level <= Hps, then two lines will be printed
.\" before the header instead of one.
.nr Hps 1
.\"
.\" These variables controls the number of lines preceding .H.
.\" Hps1 is the number of lines when level > Hps
.nr Hps1 0.5v
.if n .nr Hps1 1v
.\"
.\" Hps2 is the number of lines when level >= Hps
.nr Hps2 1v
.if n .nr Hps2 2v
.\"
.\" flag for mkindex
.if !r Idxf .nr Idxf 0
.\"	Change these in the national configuration file
.ds Lifg Figure
.ds Litb TABLE
.ds Liex Exhibit
.ds Liec Equation
.ds Licon CONTENTS
.\"
.\" Lsp controls the height of an empty line. Normally 0.5v
.\" Normally used for nroff compatibility.
.nr Lsp 0.5v
.if n .nr Lsp 1v
.ds MO1 January
.ds MO2 February
.ds MO3 March
.ds MO4 April
.ds MO5 May
.ds MO6 June
.ds MO7 July
.ds MO8 August
.ds MO9 September
.ds MO10 October
.ds MO11 November
.ds MO12 December
.\" for GETR
.ds Qrf See chapter \\*[Qrfh], page \\*[Qrfp].
.\"	test for mgm macro. This can be used if the text must test
.\"	what macros is used.
.\"
.\" section-page if Sectp > 0
.nr Sectp 0
.if (\n[N]=3):(\n[N]=5) \{\
.	nr Sectp 1
.	nr Ej 1
.\}
.\" section-figure if Sectf > 0
.nr Sectf 0
.if \n[N]=5 .nr Sectf 1
.\"
.\" argument to .nm in .VERBON.
.ds Verbnm "1
.\" indent for VERBON
.nr Verbin 5n
.\"
.nr .mgm 1
.\"
.\"---------------------------------------------
.\" set local variables.
.ie d @language .mso mm/\\*[@language]_locale
.el .mso mm/locale
.\"---------------------------------------------
.if \n[D] .tm Groff mm, version \*[RE].
.\" ####### module init ######
.\"	reset all things
.de init@reset
.ie \\n[misc@adjust] 'ad
.el 'na
.ie \\n[Hy] 'hy 14
.el 'nh
'in 0
'ti 0
'ps \\n[@ps]
'vs \\n[@vs]
..
.de @error
.tm ******************
.tm ERROR:(\\n[.F]) input line \\n[.c]:\\$*
.if \\n[D] .backtrace
.tm ******************
.ab
..
.\" ####### module debug #################################
.de debug
.tm \\$1:\\n[.F]:\\n[c.] ll=\\n[.l] vs=\\n[.v] ps=\\n[.s],\\n[.ps] \
in=\\n[.i] fi=\\n[.u] .d=\\n[.d] nl=\\n[nl] pg=\\n[%]
..
.de debug-all
.nr debug*n 1n
.nr debug*m 1m
.tm \\$1:\\n[.F]:\\n[c.] ll=\\n[.l] vs=\\n[.v] ps=\\n[.s] in=\\n[.i]\
 ad=\\n[.j] fi=\\n[.u] pl=\\n[.p] page=\\n[%] .o=\\n[.o]
.tm _______ .d=\\n[.d] .f=\\n[.f] .h=\\n[.h] .k=\\n[.k] .n=\\n[.n]\
 .p=\\n[.p] .t=\\n[.t] .z=\\n[.z] nl=\\n[nl] dn=\\n[dn] n=\\n[debug*n]
..
.\" ####### module par #################################
.nr par@ind-flag 1	\" indent on following P if Pt=2
.nr hd*last-pos -1
.nr hd*last-hpos -1
.nr par*number 0 1
.af par*number 01
.nr par*num-count 0 1
.af par*num-count 01
.\"	reset numbered paragraphs, arg1 = headerlevel
.de par@reset-num
.if \\$1<3 .nr par*num-count 0
.if (\\$1=1)&(\\n[Np]=1) .nr par*number 0
..
.\"------------
.\" paragraph
.de P
.\"	skip P if previous heading
.ie !((\\n[nl]=\\n[hd*last-pos]):(\\n[nl]=(\\n[hd*last-pos]-.5v))) \{\
.	if \\n[D]>2 .tm Paragraph 
.	par@doit \\$*
.\}
.el .if !(\\n[hd*last-hpos]=\\n[.k]) \{\
.	if \\n[D]>2 .tm Paragraph 
.	par@doit \\$*
.\}
.nr par@ind-flag 1
..
.de par@doit
.SP (u;\\n[Ps]*\\n[Lsp])
.ie  \\n[.$] \{\
.	if \\$1=1 .ti +\\n[Pi]n
.\}
.el \{\
.	if \\n[Pt]=1 .ti +\\n[Pi]n
.	if (\\n[Pt]=2)&\\n[par@ind-flag] .ti +\\n[Pi]n
.\}
.if \\n[Np] \{\
\\n[H1].\\n+[par*number]\ \ \c
.\" $$$ 'br
.\}
..
.\" ####### module line #######################################
.de SP
.br
.if !r line*lp\\n[.z] .nr line*lp\\n[.z] 0
.if !r line*ac\\n[.z] .nr line*ac\\n[.z] 0
.ie \\n[.$] .nr line*temp (v;\\$1)
.el .nr line*temp 1v
.\"
.ie \\n[line*lp\\n[.z]]=\\n[.d] \{\
.	\" go here if no output since the last .SP
.	nr line*output \\n[line*temp]-\\n[line*ac\\n[.z]]
.	if \\n[line*output]<0 .nr line*output 0
.	nr line*ac\\n[.z] +\\n[line*output]
.\}
.el \{\
.	nr line*ac\\n[.z] \\n[line*temp]
.	nr line*output \\n[line*temp]
.	\" no extra space in the beginning of a page
.	if (\\n[.d]<0):(\\n[pg*head-mark]=\\n[.d]) .nr line*output 0
.\}
.if \\n[line*output] .sp \\n[line*output]u
.nr line*lp\\n[.z] \\n[.d]
..
.\" ######## module misc ###############
.nr misc@adjust 14
.de SA
.if \\n[.$] \{\
.	if \\$1-1 .@error "SA: bad arg: \\$1"
.	nr misc@adjust 0\\$1
.\}
.ie \\n[misc@adjust] 'ad
.el 'na
..
.\"-------------
.\" switch environment, keep all important settings.
.de misc@ev-keep
.nr misc*ll \\n[.l]
.ev \\$1
.ll \\n[misc*ll]u
.lt \\n[misc*ll]u
..
.\"-------------
.\" .misc@push stackname value
.de misc@push
.ie d misc*st-\\$1 .ds misc*st-\\$1 \\$2 \\*[misc*st-\\$1]
.el .ds misc*st-\\$1 \\$2
..
.\"-------------
.\" .misc@pop stackname
.\" value returned in the string misc*pop
.de misc@pop
.misc@pop-set misc*st-\\$1 \\*[misc*st-\\$1]
..
.\"-------------
.de misc@pop-set
.ds misc*st-name \\$1
.shift
.if \\n[.$]<1 .@error "stack \\*[misc*st-name] empty"
.ds misc*pop \\$1
.shift
.ds \\*[misc*st-name] \\$*
..
.\"-------------
.\" .misc@pop-nr stackname varname
.de misc@pop-nr
.misc@pop \\$1
.nr \\$2 \\*[misc*pop]
..
.\"-------------
.\" .misc@pop-ds stackname varname
.de misc@pop-ds
.misc@pop \\$1
.ds \\$2 \\*[misc*pop]
..
.\"-----------
.\" reset tabs
.de TAB
.ta T 5n
..
.\"-------------
.\" .PGFORM linelength [ pagelength [ pageoffset ] ]
.de PGFORM
.\" Break here to avoid problems with new linesetting of the previous line.
.\" Hope this doesn't break anything else :-)
.br
.if !''\\$1' .nr @ll \\$1
.if !''\\$2' .nr @pl \\$2
.if !''\\$3' .nr @po \\$3
.ll \\n[@ll]u
.lt \\n[@ll]u
.po \\n[@po]u
.pl \\n[@pl]u
.nr @cur-ll \\n[@ll]
.in 0
.pg@move-trap
..
.\"-------------
.\" .MOVE y [[x] linelength]
.\" move to line y, indent to x
.de MOVE
.if !\\n[.$] .@error "MOVE y [x]: no arguments"
.if \\n[nl]<0 \c
.\" move to Y-pos
.sp |(v;\\$1)
.\" calc linelength
.ie \\n[.$]>2 .nr pg*i (n;\\$3)
.el \{\
.	ie \\n[.$]>1 .nr pg*i (n;\\n[@ll]u-\\$2)
.	el .nr pg*i \\n[@ll]u
.\}
.\" move to X-pos, if any
.if !''\\$2' .po \\$2
.\" set linelength
.ll \\n[pg*i]u
..
.\"-------------
.de SM
.if !\\n[.$] .@error "SM: no arguments"
.if \\n[.$]=1 \s-1\\$1\s0
.if \\n[.$]=2 \s-1\\$1\s0\\$2
.if \\n[.$]=3 \\$1\s-1\\$2\s0\\$3
..
.\"-------------
.nr misc*S-ps \n[@ps]
.nr misc*S-vs \n[@vs]
.nr misc*S-ps1 \n[@ps]
.nr misc*S-vs1 \n[@vs]
.ds misc*a
.ds misc*b
.de S
.ie !\\n[.$] \{\
.	ds misc*a P
.	ds misc*b P
.\}
.el \{\
.	ie \\n[.$]=1 .ds misc*b D
.	el \{\
.		ie \w@\\$2@=0 .ds misc*b C
.		el .ds misc*b \\$2
.	\}
.	ie \w@\\$1@=0 .ds misc*a C
.	el .ds misc*a \\$1
.\}
.\"
.\" set point size
.if !'\\*[misc*a]'C' \{\
.	ie '\\*[misc*a]'P' .nr @ps \\n[misc*S-ps]
.	el \{\
.		ie '\\*[misc*a]'D' .nr @ps 10
.		el .nr @ps \\*[misc*a]
.	\}
.\}
.\"
.\" set vertical spacing
.if !'\\*[misc*b]'C' \{\
.	ie '\\*[misc*b]'P' .nr @vs \\n[misc*S-vs]
.	el \{\
.		ie '\\*[misc*b]'D' .nr @vs \\n[@ps]+2
.		el .nr @vs \\*[misc*b]
.	\}
.\}
'ps \\n[@ps]
'vs \\n[@vs]
.if \\n[D]>1 .tm point-size \\n[@ps] (\\n[.s]), vertical spacing \\n[@vs] (\\n[.v])
.nr misc*S-ps \\n[misc*S-ps1]
.nr misc*S-vs \\n[misc*S-vs1]
.nr misc*S-ps1 \\n[@ps]
.nr misc*S-vs1 \\n[@vs]
..
.\"------------
.de HC
.ev 0
.hc \\$1
.ev
.ev 1
.hc \\$1
.ev
.ev 2
.hc \\$1
.ev
..
.\"------------
.de RD
.di misc*rd
'fl
.rd \\$1
.br
.di
.ie !''\\$3' \{\
.	di misc*rd2
.	ds \\$3 "\\*[misc*rd]
.	br
.	di
.\}
.if !''\\$2' .rn misc*rd \\$2
.rm misc*rd misc*rd2
..
.\"------------
.\" VERBON [flag [pointsize [font]]]
.\"	flag
.\"	bit	function
.\"	0	escape on
.\"	1	add an empty line before verbose text
.\"	2	add an empty line after verbose text
.\"	4	numbered lines (controlled by the string Verbnm)
.\"	8	indent text by the numbervariable Verbin.
.de VERBON
.nr misc*verb 0\\$1
.if (0\\n[misc*verb]%4)/2 .SP \\n[Lsp]u
.br
.misc@ev-keep misc*verb-ev
.nf
.if (0\\n[misc*verb]%16)/8 .nm \\*[Verbnm]
.ie !'\\$3'' .ft \\$3
.el .ft CR
.ss 12
.ta T 8u*\w@n@u
.if 0\\$2 \{\
.	ps \\$2
.	vs \\$2
.\}
.if (0\\n[misc*verb]%32)/16 .in +\\n[Verbin]u
.if !(0\\n[misc*verb]%2) \{\
.	eo
.	nr @verbose-flag 1		\" tell pageheader to set ec/eo
.\}
..
.de VERBOFF
.ec
.if (0\\n[misc*verb]%8)/4 .SP \\n[Lsp]u
.if (0\\n[misc*verb]%16)/8 .nm
.if (0\\n[misc*verb]%32)/16 .in
.ev
.nr @verbose-flag 0
..
.\" ######## module acc #################
.\"-----------
.\" accents. These are copied from mgs, written by James Clark.
.de acc@over-def
.ds \\$1 \Z'\v'(u;\w'x'*0+\En[rst]-\En[.cht])'\
\h'(u;-\En[skw]+(-\En[.w]-\w'\\$2'/2)+\En[.csk])'\\$2'
..
.de acc@under-def
.ds \\$1 \Z'\v'\En[.cdp]u'\h'(u;-\En[.w]-\w'\\$2'/2)'\\$2'
..
.acc@over-def ` \`
.acc@over-def ' \'
.acc@over-def ^ ^
.acc@over-def ~ ~
.acc@over-def : \(ad
.acc@over-def ; \(ad
.acc@under-def , \(ac
.\" ######## module uni #################
.\" unimplemented macros
.de OK
.tm "OK: not implemented"
..
.de PM
.tm "PM: not implemented"
..
.\" ######## module hd #################
.\" support for usermacro
.nr hd*h1-page 1	\" last page-number for level 1 header.
.nr hd*htype 0
.ds hd*sect-pg
.ds hd*mark
.ds hd*suf-space
.nr hd*need 0
.aln ;0 hd*htype
.als }0 hd*mark
.als }2 hd*suf-space
.aln ;3 hd*need
.\"-------------
.\" .hd@split varable index name val1 val2 ...
.de hd@split
.if \\$2>(\\n[.$]-3) .@error "\\$3 must have at least \\$2 values (\\*[\\$3]).
.nr hd*sp-tmp \\$2+3
.ds \\$1 \\$[\\n[hd*sp-tmp]]
..
.de HU
.H 0 "\\$1"
..
.\"-------------
.de H
.if !r hd*cur-bline .nr hd*cur-bline \\n[nl]
.br
.ds@print-float 2\"	$$$ could be wrong...
.\" terminate all lists
.LC 0
.init@reset
.nr hd*level 0\\$1
.nr hd*arg1 0\\$1
.if !\\n[hd*level] .nr hd*level \\n[Hu]
.\"
.\"	clear lower counters
.nr hd*i 1 1
.while \\n+[hd*i]<8 .if \\n[hd*level]<\\n[hd*i] .nr H\\n[hd*i] 0 1
.\"
.\"	Check if it's time for new page. Only if text has
.\"	appeared before.
.if \\n[Ej]&(\\n[Ej]>=\\n[hd*level])&(\\n[nl]>\\n[hd*cur-bline]) .pg@next-page
.\"
.\" increment current counter
.nr H\\n[hd*level] +1
.\"
.\" if level==1 -> prepare for new section.
.if \\n[hd*level]=1 .rr hd*h1-page
.\"
.\"
.\" hd*mark is the text written to the left of the header.
.ds hd*mark \\n[H1].
.\"
.if \\n[hd*level]>1 .as hd*mark \\n[H2]
.\"
.nr hd*i 2 1
.while \\n+[hd*i]<8 .if \\n[hd*level]>(\\n[hd*i]-1) .as hd*mark .\\n[H\\n[hd*i]]
.if \\n[Ht] .ds hd*mark \\n[H\\n[hd*level]].
.\"
.\" toc-mark is sent to the table of contents
.ds hd*toc-mark \\*[hd*mark]
.as hd*mark \ \ \"			add spaces between mark and heading
.if !\\n[hd*arg1] \{\
.	ds hd*mark\"		no mark for unnumbered
.	ds hd*toc-mark
.\}
.\"
.if \\n[D]>1 .tm At header \\*[hd*toc-mark] "\\$2"
.nr hd*htype 0				\" hd*htype = check break and space
.					\" 0 = run-in, 1 = break only, 2 = space
.if \\n[hd*level]<=\\n[Hb] .nr hd*htype 1
.if \\n[hd*level]<=\\n[Hs] .nr hd*htype 2
.					\" two spaces if hd*htype == 0
.ie (\\n[hd*htype]=0)&(\w@\\$2@) .ds hd*suf-space "  \"
.el .ds hd*suf-space
.nr hd*need 2v				\" hd*need = header need space
.if \\n[hd*htype]<2 .nr hd*need +\\n[Lsp]u \"	add some extra space
.\"---------- user macro HX ------------
.\" User exit macro to override numbering.
.\" May change hd*mark (}0), hd*suf-space (}2) and hd*need (;3)
.\" Can also change Hps1/2.
.if d HX .HX \\n[hd*level] \\n[hd*arg1] "\\$2\\$3"
.\"-------------------------------------- 
.\" pre-space
.ie \\n[hd*level]<=\\n[Hps] .SP \\n[Hps2]u
.el .SP \\n[Hps1]u
.\"
.par@reset-num \\n[hd*level]\"			reset numbered paragraph
.\" start diversion to measure size of header
.di hd*div
\\*[hd*mark]\\$2\\$3\\*[hd*suf-space]
.br
.di
.rm hd*div
.ne \\n[hd*need]u+\\n[dn]u+.5p  \"	this is the needed space for a header
.if \\n[hd*htype] .na \"		no adjust if run-in
.\"
.\" size and font calculations
.hd@split hd*font \\n[hd*level] HF \\*[HF]\"	get font for this level
.ft \\*[hd*font]\"			set new font
.hd@split hd*new-ps \\n[hd*level] HP \\*[HP]\"	get point size
.ie (\\*[hd*new-ps]=0):(\w@\\*[hd*new-ps]@=0) \{\
.	if \\n[hd*htype] \{\
.		if '\\*[hd*font]'3' \{\
.			ps -1
.			vs -1
.		\}
.		if '\\*[hd*font]'B' \{\
.			ps -1
.			vs -1
.		\}
.	\}
.\}
.el \{\
.	ps \\*[hd*new-ps]
.	vs \\*[hd*new-ps]+2
.\}
.\"
.\"---------- user macro HY ------------- 
.\"	user macro to reset indents
.if d HY .HY \\n[hd*level] \\n[hd*arg1] "\\$2\\$3"
.\"-------------------------------------- 
.nr hd*mark-size \w@\\*[hd*mark]@
.if (\\n[hd*level]<=\\n[Hc])&\\n[hd*htype] .ce\" center if level<=Hc
.\"
.\"	finally, output the header
\\*[hd*mark]\&\c
.\"	and the rest of the header
.ie \\n[hd*htype] \{\
\\$2\\$3
.	br
.\}
.el \\$2\\$3\\*[hd*suf-space]\&\c
.ft 1
.\" restore pointsize and vertical size.
.ps \\n[@ps]
.vs \\n[@vs]
.\"
.\" table of contents
.if (\\n[hd*level]<=\\n[Cl])&\w@\\$2@ \{\
.	ie \\n[Sectp] \{\
.		toc@save \\n[hd*level] "\\*[hd*toc-mark]" "\\$2" \\*[hd*sect-pg]
.	\}
.	el .toc@save \\n[hd*level] "\\*[hd*toc-mark]" "\\$2" \\n[%]
.\}
.\"	set adjust to previous value
.SA
.\"	do break or space
.if \\n[hd*htype] .br
.if \\n[hd*htype]>1 .SP \\n[Lsp]u
.if \\n[hd*htype] \{\
.	\"	indent if Hi=1 and Pt=1
.	if (\\n[Hi]=1)&(\\n[Pt]=1) .ti +\\n[Pi]n
.	\"	indent size of mark if Hi=2
.	if \\n[hd*htype]&(\\n[Hi]>1) .ti +\\n[hd*mark-size]u
.\}
.nr par@ind-flag 0			\" no indent on .P if Pt=2
.\"
.\"	check if it is time to reset footnotes
.if (\\n[hd*level]=1)&\\n[ft*clear-at-header] .nr ft*nr 0 1
.\"
.\"	check if it is time to reset indexes
.if (\\n[hd*level]=1)&\\n[Sectf] \{\
.	nr lix*fg-nr 0 1
.	nr lix*tb-nr 0 1
.	nr lix*ec-nr 0 1
.	nr lix*ex-nr 0 1
.\}
.\"---------- user macro HZ ----------
.if d HZ .HZ \\n[hd*level] \\n[hd*arg1] "\\$2\\$3"
.nr hd*last-pos \\n[nl]
.nr hd*last-hpos \\n[.k]
.nr par@ind-flag 0
..
.\"--------
.de HM
.nr hd*i 0 1
.while \\n+[hd*i]<8 .af H\\n[hd*i] \\$[\\n[hd*i]] 1
..
.\"----------------------
.\" set page-nr, called from header 
.\" 
.de hd@set-page
.if !r hd*h1-page .nr hd*h1-page \\n[%]
.\"
.ie \\n[Sectp] .nr P \\n[%]-\\n[hd*h1-page]+1
.el .nr P \\n[%]
.\"
.\" Set section-page-string
.ds hd*sect-pg \\n[H1]-\\n[P]
..
.\"########### module pg ####################
.\" set end of text trap
.wh 0 pg@header
.em pg@end-of-text
.\"
.ie \n[N]=4 .ds pg*header ''''
.el .ds pg*header ''- % -''
.ds pg*even-footer
.ds pg*odd-footer
.ds pg*even-header
.ds pg*odd-header
.ds pg*footer
.\"
.nr pg*top-margin 0
.nr pg*foot-margin 0
.nr pg*block-size 0
.nr pg*footer-size 5\"			 1v+footer+even/odd footer+2v
.nr pg*header-size 7\"			 3v+header+even/odd header+2v
.nr pg*extra-footer-size 0
.nr pg*extra-header-size 0
.nr ft*note-size 0
.nr pg*cur-column 0
.nr pg*cols-per-page 1
.nr pg*cur-po \n[@po]
.nr pg*head-mark 0
.\"
.nr pg*ps \n[@ps]
.nr pg*vs \n[@vs]
.\"-------------------------
.\" footer traps: set, enable and disable
.de pg@set-new-trap
.nr pg*foot-trap \\n[@pl]u-(\\n[pg*block-size]u+\\n[ft*note-size]u+\\n[pg*foot-margin]u+\\n[pg*footer-size]v+\\n[pg*extra-footer-size]u)
.if \\n[D]>2 .tm pg*foot-trap \\n[@pl]u-(\\n[pg*block-size]u+\\n[ft*note-size]u+\\n[pg*foot-margin]u+\\n[pg*footer-size]v) = \\n[pg*foot-trap]
.\" last-pos points to the position of the footer and bottom 
.\" block below foot-notes.
.nr pg*last-pos \\n[@pl]u-(\\n[pg*block-size]u+\\n[pg*foot-margin]u+\\n[pg*footer-size]v)
..
.de pg@enable-trap
.wh \\n[pg*foot-trap]u pg@footer
.if \\n[D]>2 .tm pg@enable-trap .t=\\n[.t] nl=\\n[nl]
..
.de pg@disable-trap
.ch pg@footer
..
.\" move to new trap (if changed).
.de pg@move-trap
.pg@disable-trap
.pg@set-new-trap
.pg@enable-trap
..
.de pg@enable-top-trap
.\" set trap for pageheader.
.nr pg*top-enabled 1
..
.de pg@disable-top-trap
.\" remove trap for pageheader.
.nr pg*top-enabled 0
..
.\" no header on the next page
.de PGNH
.nr pg*top-enabled -1
..
.\" set first trap for pagefooter
.pg@enable-top-trap
.pg@set-new-trap
.pg@enable-trap
.\"-------------------------
.\" stop output and begin on next page. Fix footnotes and all that.
.de pg@next-page
.\".debug next-page
.ne 999i		\" activate trap
.\" .pg@footer
..
.\"-------------------------
.\" support for PX and TP
.als }t pg*header
.als }e pg*even-header
.als }o pg*odd-header
.\"------------------------------------------------------------
.\" HEADER
.de pg@header
.if \\n[D]>1 .tm Page# \\n[%] (\\n[.F]:\\n[c.])
.if \\n[Idxf] \{\
.tl '<pagenr\ \\n[%]>'''
.\}
.\" assign current page-number to P
.hd@set-page
.\"
.\" suppress pageheader if pagenumber == 1 and N == [124]
.if \\n[pg*top-enabled] \{\
.	if \\n[pg*extra-header-size] 'sp \\n[pg*extra-header-size]u
.	if \\n[pg*top-margin] 'sp \\n[pg*top-margin]u
.	ev pg*tl-ev
.	pg@set-env
.	ie !d TP \{\
'		sp 3
.		lt \\n[@ll]u
.		ie ((\\n[%]=1)&(\\n[N]=1):(\\n[N]=2)) .sp
.		el .tl \\*[pg*header]
.		ie o .tl \\*[pg*odd-header]
.		el .tl \\*[pg*even-header]
'		sp 2
.	\}
.	el .TP
.	ev
.	\" why no-space??
.	if d PX \{\
.		ns
.		PX
.		rs
.	\}
.	\" check for pending footnotes 
.	ft@check-old
.	\"
.	\" back to normal text processing
.	\" .pg@enable-trap
.	\" mark for multicolumn
.	nr pg*head-mark \\n[nl]u
.	\" set multicolumn
.	\" 
.	pg@set-po
.	\" print floating displays
.	ds@print-float 4
.	tbl@top-hook
.	ns
.\}
.if \\n[pg*top-enabled]<0 .nr pg*top-enabled 1
.nr hd*cur-bline \\n[nl]	\" .H needs to know if output has occured
..
.\"---------------------------------------------------------
.\" FOOTER
.de pg@footer
.ec
.if \\n[D]>2 .tm Footer# \\n[%] (\\n[.F]:\\n[c.])
.pg@disable-trap
.\".debug footer
.tbl@bottom-hook
.\" increment pageoffset for MC
.\" move to the exact start of footer.
'sp |\\n[pg*foot-trap]u+1v
.\"
.if \\n[D]>3 .tm FOOTER after .sp
.\" print footnotes
.if d ft*div .ft@print
.\"
.pg@inc-po
.if !\\n[pg*cur-column] .pg@print-footer
.\" next column
.pg@set-po
.pg@enable-trap
.if \\n[@verbose-flag] .eo		\" to help VERBON/VERBOFF
..
.\"-------------------------
.de pg@print-footer
.\" jump to the position just below the foot-notes.
'sp |\\n[pg*last-pos]u+1v
.\" check if there are any bottom block
.if d pg*block-div .pg@block
.\"
.\" print the footer and eject new page
.ev pg*tl-ev
.pg@set-env
.lt \\n[@ll]u
.ie o .tl \\*[pg*odd-footer]
.el .tl \\*[pg*even-footer]
.ie (\\n[%]=1)&(\\n[N]=1) .tl \\*[pg*header]
.el .tl \\*[pg*footer]
.ev
.ie (\\n[ds*fnr]>=\\n[ds*o-fnr]):\\n[ft*exist] \{\
.	ev ne
'	bp
.	ev
.\}
.el 'bp
..
.\"-------------------------
.\"
.\" Initialize the title environment
.de pg@set-env
'na
'nh
'in 0
'ti 0
'ps \\n[pg*ps]
'vs \\n[pg*vs]
..
.\"-------------------------
.de PH
.ds pg*header "\\$1
.pg@set-new-size
..
.de PF
.ds pg*footer "\\$1
.pg@set-new-size
..
.de OH
.ds pg*odd-header "\\$1
.pg@set-new-size
..
.de EH
.ds pg*even-header "\\$1
.pg@set-new-size
..
.de OF
.ds pg*odd-footer "\\$1
.pg@set-new-size
..
.de EF
.ds pg*even-footer "\\$1
.pg@set-new-size
..
.de pg@clear-hd
.ds pg*even-header
.ds pg*odd-header
.ds pg*header
..
.de pg@clear-ft
.ds pg*even-footer
.ds pg*odd-footer
.ds pg*footer
..
.de pg@set-new-size
.nr pg*ps \\n[@ps]
.nr pg*vs \\n[@vs]
.pg@move-trap
..
.\"-------------------------
.\" end of page processing
.de pg@footnotes
.\".debug footnotes
.\" output footnotes. set trap for block
.\"
..
.\"-------------------------
.\" print bottom block
.de pg@block
.ev pg*block-ev
'nf
'in 0
.ll 100i
.pg*block-div
.br
.ev
..
.\"-------------------------
.\" define bottom block
.de BS
.misc@ev-keep pg*block-ev
.init@reset
.br
.di pg*block-div
..
.\"-------------------------
.de BE
.br
.di
.nr pg*block-size \\n[dn]u
.ev
.pg@move-trap
..
.\"-------------------------
.\" print out all pending text
.de pg@end-of-text
.if \\n[D]>2 .tm ---------- End of text processing ----------------
.ds@eot-print
.ref@eot-print
..
.\"-------------------------
.\" set top and bottom margins 
.de VM
.if \\n[.$]=0 \{\
.	nr pg*extra-footer-size 0
.	nr pg*extra-header-size 0
.\}
.if \\n[.$]>0 .nr pg*extra-header-size (v;\\$1)
.if \\n[.$]>1 .nr pg*extra-footer-size (v;\\$2)
.if \\n[D]>2 \{\
.	tm extra top \\n[pg*extra-footer-size]
.	tm extra bottom \\n[pg*extra-header-size]
.\}
.pg@move-trap
..
.\"---------------------
.\" multicolumn output. 
.de pg@set-po
.if \\n[pg*cols-per-page]>1 \{\
.	ll \\n[pg*column-size]u
.\}
..
.de pg@inc-po
.if \\n[pg*cols-per-page]>1 \{\
.	ie \\n+[pg*cur-column]>=\\n[pg*cols-per-page] \{\
.		nr pg*cur-column 0 1
.		nr pg*cur-po \\n[@po]u
.		po \\n[@po]u
.		ll \\n[@ll]u
.	\}
.	el \{\
.		nr pg*cur-po +(\\n[pg*column-size]u+\\n[pg*column-sep]u)
.		po \\n[pg*cur-po]u
'		sp |\\n[pg*head-mark]u
.		tbl@top-hook
.	\}
.\}
..
.de 1C
.br
.if \\n[pg*cols-per-page]<=1 .@error "1C: multicolumn mode not active"
.nr pg*cols-per-page 1
.nr pg*column-sep 0
.nr pg*column-size \\n[@ll]
.nr pg*cur-column 0 1
.nr pg*cur-po \\n[@po]u
.PGFORM
.\".pg@next-page
.SK
..
.de 2C
.br
.nr pg*head-mark \\n[nl]u
.if \\n[pg*cols-per-page]>1 .@error "2C: multicolumn mode already active"
.nr pg*cols-per-page 2
.nr pg*column-sep \\n[@ll]/15
.nr pg*column-size (\\n[@ll]u*7)/15
.nr pg*cur-column 0 1
.nr pg*cur-po \\n[@po]u
.ll \\n[pg*column-size]u
.lt \\n[pg*column-size]u
..
.\" MC column-size [ column-separation ]
.de MC
.br
.nr pg*head-mark \\n[nl]u
.if \\n[pg*cols-per-page]>1 .@error "MC: multicolumn mode already active"
.ie ''\\$1' .nr pg*column-size \\n[.l]
.el .nr pg*column-size (n;\\$1)
.ie ''\\$2' .nr pg*column-sep \\n[pg*column-size]/15
.el .nr pg*column-sep (n;\\$2)
.\"
.nr pg*cols-per-page (u;\\n[.l]/(\\n[pg*column-size]+\\n[pg*column-sep]+1))
.nr pg*cur-column 0 1
.nr pg*cur-po \\n[@po]u
.ll \\n[pg*column-size]u
.lt \\n[pg*column-size]u
..
.\" begin a new column
.de NCOL
.br
.pg@footer
..
.\" skip pages
.de SK
.br
.bp
.nr pg*i 0 1
.\" force new page by writing something invisible.
.while \\n+[pg*i]<=(0\\$1) \{\
\&
.	bp
.\}
..
.\"-------------------------------
.\" MULB width1 space1 width2 space2 width3 space3 ...
.de MULB
.br
.nr pg*i 0 1
.nr pg*mul-x 0 1
.nr pg*mul-ind 0
.nr pg*mul-last 0
.while \\n[.$] \{\
.	nr pg*mul!\\n+[pg*i] (n;0\\$1)
.	nr pg*muls!\\n[pg*i] (n;0\\$2)
.	shift 2
.\}
.nr pg*mul-max-col \\n[pg*i]
.ds pg*mul-fam \\n[.fam]
.nr pg*mul-font \\n[.f]
.ev pg*mul-ev
.fam \\*[pg*mul-fam]
.ft \\n[pg*mul-font]
.fi
.hy 14
.di pg*mul-div
.MULN
..
.\"-----------
.de MULN
.if \\n[pg*mul-x]>=\\n[pg*mul-max-col] .@error "MULN: Undefined columnwidth"
.br
.if \\n[.d]>\\n[pg*mul-last] .nr pg*mul-last \\n[.d]
.rt +0
.in \\n[pg*mul-ind]u
.ll (u;\\n[.i]+\\n[pg*mul!\\n+[pg*mul-x]])u
.nr pg*mul-ind +(u;\\n[pg*mul!\\n[pg*mul-x]]+\\n[pg*muls!\\n[pg*mul-x]])
..
.\"-----------
.\" MULE
.de MULE
.br
.if \\n[.d]>\\n[pg*mul-last] .nr pg*mul-last \\n[.d]
.di
.ev
.ne \\n[pg*mul-last]u
.nf
.mk
.pg*mul-div
.rt
.sp \\n[pg*mul-last]u
.fi
..
.\"-----------
.de OP
.br
.ie o .if !\\n[pg*head-mark]=\\n[nl] \{\
.	bp +1
.	bp +1
.\}
.el .bp
..
.\"########### module footnotes ###################
.nr ft*note-size 0
.nr ft*busy 0
.nr ft*nr 0 1
.nr ft*wide 0
.nr ft*hyphen 0\"	hyphenation value
.nr ft*adjust 1\"	>0 if adjust true
.nr ft*indent 1\"	>0 if text indent true (not imp. $$$)
.nr ft*just 0\"	0=left justification, 1=right (not imp. $$$)
.nr ft*exist 0\"	not zero if there are any footnotes to be printed
.nr ft*clear-at-header 0\" >0 if footnotes should be reset at first level head.
.\"
.ds F \v'-.4m'\s-3\\n+[ft*nr]\s0\v'.4m'
.\"
.\"-----------------
.\" init footnote environment
.de ft@init
.\" indentcontrol not implemented $$$
.\" label justification not implemented $$$
'in 0
'fi
.ie \\n[ft*adjust] 'ad
.el 'na
.ie \\n[ft*hyphen] 'hy 14
.el 'hy 0
.ll \\n[@cur-ll]u
.lt \\n[@cur-ll]u
.ps (\\n[@ps]-2)
.vs (\\n[@vs]-1)
..
.\"-----------------
.\" set footnote format
.\" no support for two column processing (yet). $$$
.de FD
.if \\n[.$]=0 .@error "FD: bad arg \\$1"
.ie \\n[.$]=2 .nr ft*clear-at-header 1
.el .nr ft*clear-at-header 0
.\"
.if !'\\$1'' \{\
.	ie \\$1>11 .nr ft*format 0
.	el .nr ft*format \\$1
.	\"
.	nr ft*hyphen (\\n[ft*format]%2)*14
.	nr ft*format \\n[ft*format]/2
.	\"
.	nr ft*adjust 1-(\\n[ft*format]%2)
.	nr ft*format \\n[ft*format]/2
.	\"
.	nr ft*indent 1-(\\n[ft*format]%2)
.	nr ft*format \\n[ft*format]/2
.	\"
.	nr ft*just \\n[ft*format]%2
.\}
..
.\"---------------
.\" Footnote and display width control $$$
.de WC
.nr ft*i 0 1
.while \\n+[ft*i]<=\\n[.$] \{\
.	ds ft*x \\$[\\n[ft*i]]
.	if '\\*[ft*x]'N' \{\
.		nr ft*wide 0
.		nr ft*first-fn 0
.		nr ds*wide 0
.		nr ds*float-break 1
.	\}
.	if '\\*[ft*x]'-WF' .nr ft*wide 0
.	if '\\*[ft*x]'WF' .nr ft*wide 1
.	if '\\*[ft*x]'-FF' .nr ft*first-fn 0
.	if '\\*[ft*x]'FF' .nr ft*first-fn 1
.	if '\\*[ft*x]'-WD' .nr ds*wide 0
.	if '\\*[ft*x]'WD' .nr ds*wide 1
.	if '\\*[ft*x]'-FB' .nr ds*float-break 0
.	if '\\*[ft*x]'FB' .nr ds*float-break 1
.\}
..
.\"-----------------
.\" begin footnote
.de FS
.if \\n[ft*busy] .@error "FS: missing FE"
.nr ft*busy 1
.ev ft*ev
.ft@init
.if !\\n[ft*wide] .pg@set-po
.if !d ft*div .ft@init-footnote
.di ft*tmp-div
.nr ft*space (u;\\n[Fs]*\\n[Lsp])
.sp \\n[ft*space]u
.\" print mark
.ie \\n[.$] .ds ft*mark \\$1
.el .ds ft*mark \\n[ft*nr].
\\*[ft*mark]
.in +.75c
.sp -1
.nr ft*exist 1
..
.\"-----------------
.\" init footnote diversion
.de ft@init-footnote
.di ft*div
\l'20n'
.br
.di
.nr ft*note-size \\n[dn]
..
.\"-----------------
.\" end footnote
.de FE
.nr ft*busy 0
.br
.di
'in 0
'nf
.if \\n[@pl]u<\\n[dn]u .@error "FE: too big footnote"
.ie (\\n[pg*foot-trap]u-\\n[.d]u)<\\n[dn]u \{\
.	da ft*next-div
.	ft*tmp-div
.	br
.	di
.\}
.el \{\
.	da ft*div
.	ft*tmp-div
.	di
.	nr ft*note-size +\\n[dn]
.\}
.rm ft*tmp-div
.ev
.pg@move-trap
..
.\"-----------------
.\" print footnotes, see pg@footer
.de ft@print
.ev ft*print-ev
'nf
'in 0
.ll 100i
.ft*div
.br
.ev
.rm ft*div
.nr ft*note-size 0
.pg@move-trap
..
.\"-----------------
.\" check if any pending footnotes, see pg@header
.de ft@check-old
.if d ft*next-div \{\
.	ev ft*ev
.	ft@init
.	ft@init-footnote
.	nf
.	in 0
.	da ft*div
.	ft*next-div
.	di
.	nr ft*note-size +\\n[dn]
.	rm ft*next-div
.	ev
.	nr ft*exist 0
.	pg@move-trap
.\}
..
.\"########### module display ###################
.nr ds*wide 0\"		>0 if wide displays wanted
.nr ds*fnr 0 1\"	floating display counter
.nr ds*o-fnr 1\"	floating display counter, already printed
.nr ds*snr 0 1\"	static display counter
.nr ds*lvl 0 1\"	display level
.nr ds*float-busy 0\"	>0 if printing float
.nr ds*ffloat 0\"	>0 if DF, 0 if DS
.\" static display start
.\" nested DS/DE is allowed. No limit on depth.
.de DS
.br
.ds@start 0 DS \\$@
..
.\" floating display start
.\" nested DF/DE is not allowed.
.de DF
.if \\n[ds*lvl] .@error "DF:nested floating is not allowed. Use DS."
.ds@start 1 DF \\$@
..
.\"---------------
.nr ds*format 0\"	dummy value for .En/.EQ
.nr ds*format! 0\"	no indent
.nr ds*format!0 0\"	no indent
.nr ds*format!L 0\"	no indent
.nr ds*format!I 1\"	indent
.nr ds*format!1 1\"	indent
.nr ds*format!C 2\"	center each line
.nr ds*format!2 2\"	center each line
.nr ds*format!CB 3\"	center as block
.nr ds*format!3 3\"	center as block
.nr ds*format!R 4\"	right justify each line
.nr ds*format!4 4\"	right justify each line
.nr ds*format!RB 5\"	right justify as block
.nr ds*format!5 5\"	right justify as block
.\"---------------
.nr ds*fill! 0\"	no fill
.nr ds*fill!N 0\"	no fill
.nr ds*fill!0 0\"	no fill
.nr ds*fill!F 1\"	fill on
.nr ds*fill!1 1\"	fill on
.\"---------------
.de ds@start
.nr ds*ffloat \\$1
.ds ds*type \\$2
.shift 2
.nr ds*lvl +1
.\" get format of the display
.ie \\n[.$] \{\
.	ie r ds*format!\\$1 .nr ds*format \\n[ds*format!\\$1]
.	el .@error "\\*[ds*type]:wrong format:\\$1"
.\}
.el .nr ds*format 0
.\" fill or not to fill, that is the...
.nr ds*fill 0
.ie \\n[.$]>1 \{\
.	ie r ds*fill!\\$2 .nr ds*fill \\n[ds*fill!\\$2]
.	el .@error "\\*[ds*type]:wrong fill:\\$2"
.\}
.nr ds*rindent 0
.if \\n[.$]>2 .nr ds*rindent \\$3
.\"
.\"---------------
.nr ds*old-ll \\n[.l]
.misc@push ds-ll \\n[.l]
.misc@push ds-form \\n[ds*format]
.misc@push ds-ffloat \\n[ds*ffloat]
.nr ds*i \\n[.i]
.nr ds*ftmp \\n[.f]
.misc@ev-keep ds*ev!\\n+[ds*snr]
.ft \\n[ds*ftmp]
.\"
.init@reset
'in \\n[ds*i]u
.di ds*div!\\n[ds*snr]
.\"
.ll \\n[ds*old-ll]u
.lt \\n[ds*old-ll]u
.if \\n[ds*rindent] \{\
.	ll -\\n[ds*rindent]n
.	lt -\\n[ds*rindent]n
.\}
.if \\n[ds*wide] \{\
.	ll \\n[@ll]u
.	lt \\n[@ll]u
.\}
.\"
.ie \\n[ds*fill] 'fi
.el 'nf
.\"
.if \\n[ds*format]=1 \{\
.	ll -\\n[Si]n
.	lt -\\n[Si]n
'	in +\\n[Si]n
.\}
.if (\\n[ds*format]=3):(\\n[ds*format]=5) 'in 0
..
.\"---------------
.de DE
.if \\n-[ds*lvl]<0 .@error "DE: no corresponding DS or DF"
.br
.if \\n[ds*ffloat] .SP \\n[Lsp]u
.di
.nr ds*width \\n[dl]
.nr ds*height \\n[dn]
.misc@pop-nr ds-ll ds*old-ll
.misc@pop-nr ds-form ds*format
.misc@pop-nr ds-ffloat ds*ffloat
.if (\\n[ds*format]>=2)&(\\n[ds*width]>\\n[ds*old-ll]) \{\
.	@error "DE: display too wide for current line-length"
.\}
.\" prepare copy to floating display
.if \\n[ds*ffloat] .di ds*fdiv!\\n+[ds*fnr]
.\"
'in 0
'nf
.if \\n[ds*format]=2 'ce 9999
.if \\n[ds*format]=3 'in (u;(\\n[ds*old-ll]-\\n[ds*width])/2)
.if \\n[ds*format]=4 'rj 9999
.if \\n[ds*format]=5 'in (u;\\n[ds*old-ll]-\\n[ds*width])
.\"
.ie !\\n[ds*ffloat] \{\
.	\"
.	\"	Print static display
.	\"	Eject page if display will fit one page and
.	\"	there are less than half of the page left.
.	nr ds*i \\n[pg*foot-trap]-\\n[pg*header-size]v-\\n[pg*extra-header-size]v
.	if (\\n[ds*height]>\\n[ds*i])&(\\n[.t]<(\\n[ds*i]/2)) \{\
.		ne \\n[.t]u+1v
.	\}
.	if (\\n[ds*height]<\\n[ds*i])&(\\n[.t]<(\\n[ds*height])) \{\
.		ne \\n[.t]u+1v
.	\}
.	if \\n[Ds] .SP \\n[Lsp]u
.\}
.el .SP \\n[Lsp]u
.ds*div!\\n[ds*snr]
.ie !\\n[ds*ffloat] .if \\n[Ds] .SP \\n[Lsp]u
.el \{\
.	SP \\n[Lsp]u
.	di
.\}
.if \\n[ds*format]=2 'ce 0
.if \\n[ds*format]=4 'rj 0
.rm ds*div!\\n[ds*snr]
.nr ds*snr -1
.nr par@ind-flag 0
.\"		move div to the floating display list
.ev
.if \\n[ds*ffloat] \{\
.	nr ds*fsize!\\n[ds*fnr] \\n[dn]
.	\" print float if queue is empty and the display fits into
.	\" the current page
.	if (\\n[ds*fnr]>\\n[ds*o-fnr])&(\\n[ds*height]<\\n[.t]) \{\
.		ds@print-float 1
.	\}
.\}
..
.\"-------------
.\" called by end-of-text
.de ds@eot-print
.if \\n[ds*o-fnr]<=\\n[ds*fnr] \{\
.	if \\n[D]>2 .tm Print remaining displays.
.\" still some floats left, make non-empty environment
.	misc@ev-keep ne
.	init@reset
\c
.	ds@print-float 3
.	ev
.\}
..
.\"---------------
.\" print according to Df and De.
.\" .ds@print-float type
.\"	type	called from
.\"	1	.DE
.\"	2	end of section
.\"	3	end of document
.\"	4	beginning of new page
.\"
.de ds@print-float
.if \\n[Df]>5 .@error "Df=\\n[Df], max value is 5"
.if !\\n[ds*float-busy] \{\
.	nr ds*float-busy 1
.\" at .DE
.	if (\\$1=1)&((\\n[Df]%2)=1) \{\
.		if \\n[.t]>\\n[ds*fsize!\\n[ds*fnr]] \{\
.			\" Df = 1,3 or 5
.			ds@print-one-float
.		\}
.	\}
.\" print all if Df<2 and end of section
.	if (\\$1=2)&(\\n[Df]<2) .ds@print-all-floats
.\" print all if end of document. Where should they go instead?
.	if \\$1=3 .ds@print-all-floats
.\" new page
.	if (\\$1=4)&(\\n[Df]>1) \{\
.		if \\n[Df]=2 .ds@print-one-float
.		if \\n[Df]=3 .ds@print-one-float
.		if \\n[Df]>3 \{\
.			ie \\n[De] .ds@print-all-floats
.			el .ds@print-this-page
.		\}
.	\}
.	nr ds*float-busy 0
.\}
..
.\"---------------
.\" print a floating diversion
.de ds@output-div
.ev ds*fev
.in 0
.nf
.ds*fdiv!\\n[ds*o-fnr]
.ev
.rm ds*fdiv!\\n[ds*o-fnr]
.rm ds*fsize!\\n[ds*o-fnr]
.rm ds*fformat!\\n[ds*o-fnr]
.nr ds*o-fnr +1
..
.\"---------------
.\" print one floating display if there is one.
.de ds@print-one-float
.if \\n[ds*o-fnr]<=\\n[ds*fnr] \{\
.	if \\n[.t]<\\n[ds*fsize!\\n[ds*o-fnr]] .pg@next-page
.	ds@output-div
.	if \\n[De] .pg@next-page
.\}
..
.\"---------------
.\" print all queued floats.
.\" if De>0 do a page eject between the floats.
.de ds@print-all-floats
.while \\n[ds*o-fnr]<=\\n[ds*fnr] \{\
.	if \\n[.t]<\\n[ds*fsize!\\n[ds*o-fnr]] .pg@next-page
.	ds@output-div
.	if \\n[De] .pg@next-page
.\}
..
.\"---------------
.\" print as many floats as will fit on the current page
.de ds@print-this-page
.while \\n[ds*o-fnr]<=\\n[ds*fnr] \{\
.	if \\n[.t]<\\n[ds*fsize!\\n[ds*o-fnr]] .break
.	ds@output-div
.\}
..
.\"########### module list ###################
.\" .LI text-indent mark-indent pad type [mark [LI-space [LB-space] ] ]
.\"
.nr li*tind 0
.nr li*mind 0
.nr li*pad 0
.nr li*type 0
.ds li*mark 0
.nr li*li-spc 0
.nr li*lvl 0 1
.nr li*cur-vpos 0
.\"--------------------------
.\"	the major list-begin macro.
.\"	If type == -1 a 'break' will occur.
.de LB
.if \\n[.$]<4 .@error "LB: not enough arguments, min 4"
.misc@push cind \\n[.i]
.misc@push tind \\n[li*tind]
.misc@push mind \\n[li*mind]
.misc@push pad \\n[li*pad]
.misc@push type \\n[li*type]
.misc@push li-spc \\n[li*li-spc]
.ds li*mark-list!\\n[li*lvl] \\*[li*mark]
.nr li*lvl +1
.\"
.nr li*tind (n;0\\$1)\"			text-indent
.nr li*mind (n;0\\$2)\"			mark-indent
.nr li*pad (n;0\\$3)\"			pad
.nr li*type 0\\$4\"			type
.ds li*mark \\$5\"			mark
.ie !'\\$6'' .nr li*li-spc \\$6\"	LI-space
.el .nr li*li-spc 1
.ie !'\\$7'' .nr li*lb-spc \\$6\"	LB-space
.el .nr li*lb-spc 0
.\" init listcounter
.nr li*cnt!\\n[li*lvl] 0 1
.\" assign format
.af li*cnt!\\n[li*lvl] 1
.if \\n[li*type] .if !'\\*[li*mark]'' .af li*cnt!\\n[li*lvl] \\*[li*mark]
.\"
.if \\n[li*lb-spc] .SP (u;\\n[li*lb-spc]*\\n[Lsp])
.in +\\n[li*tind]u
..
.\"---------------
.de LI
.if \\n[li*lvl]<1 .@error "LI:no lists active"
.if \\n[li*li-spc]&(\\n[Ls]>=\\n[li*lvl]) .SP (u;\\n[li*li-spc]*\\n[Lsp])
.ne 2v
.\"
.ds li*c-mark \\*[li*mark]
.nr li*cnt!\\n[li*lvl] +1
.if \\n[li*type]=1 .ds li*c-mark \\n[li*cnt!\\n[li*lvl]].
.if \\n[li*type]=2 .ds li*c-mark \\n[li*cnt!\\n[li*lvl]])
.if \\n[li*type]=3 .ds li*c-mark (\\n[li*cnt!\\n[li*lvl]])
.if \\n[li*type]=4 .ds li*c-mark [\\n[li*cnt!\\n[li*lvl]]]
.if \\n[li*type]=5 .ds li*c-mark <\\n[li*cnt!\\n[li*lvl]]>
.if \\n[li*type]=6 .ds li*c-mark {\\n[li*cnt!\\n[li*lvl]]}
.if \\n[.$]=1 .ds li*c-mark \\$1
.if \\n[.$]=2 .ds li*c-mark \\$1\ \\*[li*c-mark]
.if '\\*[li*c-mark]'\ ' .ds li*c-mark
.\"
.\" determine where the text begins
.nr li*text-begin \\n[li*tind]>?\w@\\*[li*c-mark]\ @
.\"
.\" determine where the mark begin
.ie !\\n[li*pad] .nr li*in \\n[li*mind]
.el .nr li*in \\n[li*text-begin]-\\n[li*pad]-\w@\\*[li*c-mark]@
.if !\\n[li*in] .nr li*in 0
.\"
.ti -\\n[li*tind]u
.\" no indentation if hanging indent
.if (\w@\\*[li*c-mark]@=0)&((\\n[.$]=0):(\w@\\$1@=0)) .nr li*text-begin 0
\Z'\&\h'\\n[li*in]u'\\*[li*c-mark]'\h'\\n[li*text-begin]u'\&\c
.if \\n[li*type]=-1 .br
..
.\"
.\"-------------
.de li@pop
.nr li*lvl -1
.misc@pop-nr cind li*tmp
.in \\n[li*tmp]u
.misc@pop-nr tind li*tind
.misc@pop-nr mind li*mind
.misc@pop-nr pad li*pad
.misc@pop-nr type li*type
.misc@pop-nr li-spc li*li-spc
.ds li*mark \\*[li*mark-list!\\n[li*lvl]]
..
.de LE
.if \\n[li*lvl]<1 .@error "LE:mismatched"
.li@pop
.if '\\$1'1' .SP \\n[Lsp]u
..
.\"-------------
.\"	list status clear.
.de LC
.if !\\n[.$]=1 .@error "LC: no argument"
.if \\$1>\\n[li*lvl] .@error "LC: incorrect argument: \\$1 (too big)"
.while \\n[li*lvl]>\\$1 .li@pop
.nr par@ind-flag 0
..
.\"-------------
.de AL
.if \\n[.$]>3 .@error "AL: too many arguments"
.if \\n[D]>2 .tm AL $*
.ie \\n[.$]<=1 .LB \\n[Li] 0 2 1 "\\$1"
.el \{\
.	ie \\n[.$]=2 .LB 0\\$2 0 2 1 "\\$1"
.	el \{\
.		ie !'\\$2'' .LB \\$2 0 2 1 "\\$1" 0 1
.		el .LB \\n[Li] 0 2 1 "\\$1" 0 1
.	\}
.\}
..
.de ML
.if \\n[.$]>3 .@error "ML: too many arguments"
.if \\n[D]>2 .tm ML $*
.nr li*ml-width \w@\\$1@u+1n
.if \\n[.$]<2 .LB \\n[li*ml-width]u 0 1 0 "\\$1"
.if \\n[.$]=2 .LB 0\\$2 0 1 0 "\\$1"
.if \\n[.$]=3 \{\
.	ie '\\$2'' .LB \\n[li*ml-width]u 0 1 0 "\\$1" 0 1
.	el .LB \\n[Li] 0 1 0 "\\$1" 0 1
.\}
..
.de VL
.if \\n[D]>2 .tm VL $*
.if \\n[.$]>3 .@error "VL: too many arguments"
.if \\n[.$]<1 .@error "VL: missing text-indent"
.ie \\n[.$]<3 .LB 0\\$1 0\\$2 0 0
.el .LB 0\\$1 0\\$2 0 0 \& 0 1
..
.\"	Bullet (for .BL)
.de BL
.if \\n[D]>2 .tm BL $*
.ds BU \s-2\(bu\s0
.if \\n[.$]>2 .@error "BL: too many arguments"
.if \\n[.$]<1 .LB \\n[Pi] 0 1 0 \\*[BU]
.if \\n[.$]=1 .LB 0\\$1 0 1 0 \\*[BU]
.if \\n[.$]=2 \{\
.	ie '\\$1'' .LB \\n[Pi] 0 1 0 \\*[BU] 0 1
.	el .LB 0\\$1 0 1 0 \\*[BU] 0 1
.\}
..
.de DL
.if \\n[D]>2 .tm DL $*
.if \\n[.$]>2 .@error "DL: too many arguments"
.if \\n[.$]<1 .LB \\n[Pi] 0 1 0 \(em
.if \\n[.$]=1 .LB 0\\$1 0 1 0 \(em
.if \\n[.$]=2 \{\
.	ie '\\$1'' .LB \\n[Pi] 0 1 0 \(em 0 1
.	el .LB 0\\$1 0 1 0 \(em 0 1
.\}
..
.de RL
.if \\n[D]>2 .tm RL $*
.if \\n[.$]>2 .@error "RL: too many arguments"
.if \\n[.$]<1 .LB 6 0 2 4
.if \\n[.$]=1 .LB 0\\$1 0 2 4
.if \\n[.$]=2 \{\
.	ie '\\$1'' .LB 6 0 2 4 1 0 1
.	el .LB 0\\$1 0 2 4 1 0 1
.\}
..
.\" Broken Variable List. As .VL but text begin on the next line
.de BVL
.if \\n[D]>2 .tm BVL $*
.if \\n[.$]>3 .@error "BVL: too many arguments"
.if \\n[.$]<1 .@error "BVL: missing text-indent"
.ie \\n[.$]<3 .LB 0\\$1 0\\$2 0 -1
.el .LB 0\\$1 0\\$2 0 -1 \& 0 1
..
.\" ####### module tbl #######################################
.\" This module is copied from groff_ms and modified for mgm.
.\" Yes, it does not resemble the original anymore :-).
.\" Don't know if I missed something important.
.\" Groff_ms is written by James Clark.
.nr tbl*have-header 0
.nr tbl*header-written 0
.de TS
.br
.if ''\\n[.z]' .SP
.if '\\$1'H' .di tbl*header-div
..
.de tbl@top-hook
.if \\n[tbl*have-header] \{\
.	ie \\n[.t]-\\n[tbl*header-ht]-1v .tbl@print-header
.	el .sp \\n[.t]u
.\}
..
.de tbl@bottom-hook
.if \\n[tbl*have-header] \{\
.	nr T. 1
.\" draw bottom and side lines of boxed tables.
.	T#
.\}
.nr tbl*header-written 0
..
.de tbl@print-header
.ev tbl*ev
'nf
.tbl*header-div
.ev
.mk #T
.nr tbl*header-written 1
..
.de TH
.ie '\\n[.z]'tbl*header-div' \{\
.	nr T. 0
.	T#
.	br
.	di
.	nr tbl*header-ht \\n[dn]
.	ne \\n[dn]u+1v
.	nr tbl*have-header 1
.	ie '\\$1'N' .if !\\n[tbl*header-written] .tbl@print-header
.	el .tbl@print-header
.\}
.el .@error ".TH without .TS H"
..
.de TE
.ie '\\n(.z'tbl*header-div' .@error ".TS H but no .TH before .TE"
.el \{\
.	nr tbl*have-header 0
.\}
.\" reset tabs
.TAB
..
.de T&
..
.\" ####### module pic #######################################
.de PS
.nr pic*in 0
.br
.SP .5
.ie \\n[.$]<2 .@error "PS: bad arguments. Probably not processed with pic."
.el \{\
.	if !\\n[ds*lvl] .ne (u;\\$1)+1v
.\" should be contained between .DS/.DE
.if r ds*format \{\
.		if \\n[ds*lvl]&((\\n[ds*format]=2):(\\n[ds*format]=3)) \{\
.			nr pic*in \\n[.i]
.\" .		in +(u;\\n[.l]-\\n[.i]-\\$2/2)
.		\}
.	\}
.\}
..
.de PE
.init@reset
.SP .5
..
.\" ####### module eq #######################################
.\" 
.nr eq*number 0 1
.de EQ
.ds eq*lable "\\$1
.di eq*div
.misc@ev-keep eq*ev
.in 0
.nf
..
.de EN
.br
.di
.ta
.\" equation with lable.
.if \\n[dl] \{\
.	br
.	chop eq*div
.	ie (\\n[Eq]%2) \{\
.		\"	lable to the left
.		if \\n[ds*format]<2 \{\
.			ta (u;(\\n[.l]/7)+\\n[.i]) \\n[.l]u\" L
\\*[eq*lable]\t\\*[eq*div]\t\&
.		\}
.		if (\\n[ds*format]=2):(\\n[ds*format]=3) \{\
.			ta (u;(\\n[.l]-\\n[.i])/2+\\n[.i])C \\n[.l]u\" C
\\*[eq*lable]\t\\*[eq*div]\t\&
.		\}
.		if \\n[ds*format]>3 \{\
.			ta \\n[.l]uR\"	R
\\*[eq*lable]\t\\*[eq*div]
.		\}
.	\}
.	el \{\
.		\"	lable to the right
.		if \\n[ds*format]<2 \{\
.			ta \\n[.l]uR\" L
\\*[eq*div]\t\\*[eq*lable]
.		\}
.		if (\\n[ds*format]=2):(\\n[ds*format]=3) \{\
.			ta (u;(\\n[.l]-\\n[.i])/2+\\n[.i])C \\n[.l]uR\" C
\t\\*[eq*div]\t\\*[eq*lable]
.		\}
.		if \\n[ds*format]>3 \{\
.			ta (\\n[.l]u-\w@\\*[eq*lable]@u-1m)R \\n[.l]uR\" R
\t\\*[eq*div]\t\\*[eq*lable]
.		\}
.	\}
.\}
.TAB
.ev
..
.\"########### module toc ###################
.\" table of contents
.nr toc*slevel 1
.nr toc*spacing \n[Lsp]u
.nr toc*tlevel 2
.nr toc*tab 0
.\"-----------
.\" Table of contents with friends (module lix)
.de TC
.br
.\" print any pending displays and references
.ds@print-float 3
.if \\n[ref*flag] .RP 0 1
.\"
.if \w@\\$1@>0 .nr toc*slevel \\$1
.if \w@\\$2@>0 .nr toc*spacing (u;\\$2*\\n[Lsp])
.if \w@\\$3@>0 .nr toc*tlevel \\$3
.if \w@\\$4@>0 .nr toc*tab \\$4
.if \\n[pg*cols-per-page]>1 .1C
.pg@clear-hd
.pg@next-page
.pg@clear-ft
.\"-------------
.if d Ci .toc@read-Ci \\*[Ci]
.nf
.in 0
.nr toc*pn 0 1
.af toc*pn i
.PF "''\\\\\\\\n+[toc*pn]''"
.nr toc*i 4 1
.while \\n+[toc*i]<10 \{\
.	if !'\\$\\n[toc*i]'' \{\
.		ce
\\$\\n[toc*i]
.		br
.	\}
.\}
.if \\n[.$]<=4 .if d TX .TX
.ie d TY .if \\n[.$]<=4 .TY
.el \{\
.	ce
\\*[Licon]
.	br
.	SP 3
.\}
.if d toc*list .toc*list
.\" print LIST OF XXX
.if d lix*dsfg .lix@print-ds fg "\\*[Lf]"
.if d lix*dstb .lix@print-ds tb "\\*[Lt]"
.if d lix*dsec .lix@print-ds ec "\\*[Le]"
.if d lix*dsex .lix@print-ds ex "\\*[Lx]"
..
.\"-----------
.\" .toc@read-Ci lev1 lev2 lev3 lev4 ... lev7
.de toc@read-Ci
.nr toc*i 0 1
.while \\n+[toc*i]<8 \{\
.	nr toc*hl!\\n[toc*i] \\$\\n[toc*i]
.\}
..
.\"-----------
.de toc@save
.\" collect maxsize of mark if string Ci don't exist.
.if !d Ci \{\
.	if !r toc*hl!\\$1 .nr toc*hl!\\$1 0
.	if \\n[toc*hl!\\$1]<=\w@\\$2@ \{\
.		nr toc*hl!\\$1 \w@\\$2@u+1m
.	\}
.\}
.am toc*list
.\" .toc@set level headernumber text pagenr
.toc@set \\$1 "\\$2" "\\$3" \\$4
\\..
..
.\"-----------
.\" level mark text pagenumber
.de toc@set
.if \\$1<=\\n[toc*slevel] .SP \\n[toc*spacing]u
.ne 2v
.na
.fi
.nr toc*ind 0
.nr toc*i 0 1
.ie d Ci \{\
.	nr toc*ind +\\n[toc*hl!\\$1]u
.\}
.el \{\
.	while \\n+[toc*i]<\\$1 \{\
.		nr toc*ind +\\n[toc*hl!\\n[toc*i]]u
.	\}
.\}
.nr toc*text \\n[toc*ind]u+\\n[toc*hl!\\$1]u
.in \\n[toc*text]u
.ti -\\n[toc*hl!\\$1]u
.\"
.\" length of headernum space
.nr toc*i \\n[toc*hl!\\$1]-\w@\\$2@
.\"
.ll \\n[@ll]u-\w@\\$4@u-2m
.\" ragged right ---------------------------------
.ie \\$1>\\n[toc*tlevel] \{\
\\$2
.	sp -1
\\$3\ \ \ \\$4
.	br
.\}
.el \{\
.	\" unnumbered heading --------------------
.	ie '\\$2'' \{\
.		in \\n[toc*ind]u
\\$3\h'1m'
.	\}
.	\" normal heading ------------------------
.	el \{\
\\$2
.		sp -1
\\$3\h'1m'
.	\}
.	ll \\n[@ll]u
.	sp -1
.	nr toc*sep (u;\\n[.l]-\\n[.n]-\\n[.i]-\w@\\$4@)-1m
\h'|\\n[.n]u'\l'\\n[toc*sep]u.'\h'1m'\\$4
.\}
.ll \\n[@ll]u
..
.\"########################### module lix ############################
.\" LIST OF figures, tables, exhibits and equations 
.nr lix*fg-nr 0 1
.nr lix*tb-nr 0 1
.nr lix*ec-nr 0 1
.nr lix*ex-nr 0 1
.aln Fg lix*fg-nr
.aln Tb lix*tb-nr
.aln Ec lix*ec-nr
.aln Ex lix*ex-nr
.\"------------
.de FG
.lix@print-line fg Lf \\n+[lix*fg-nr] "\\$1" "\\$2" "\\$3" "\\$4"
..
.de TB
.lix@print-line tb Lt \\n+[lix*tb-nr] "\\$1" "\\$2" "\\$3" "\\$4"
..
.de EC
.lix@print-line ec Le \\n+[lix*ec-nr] "\\$1" "\\$2" "\\$3" "\\$4"
..
.de EX
.lix@print-line ex Lx \\n+[lix*ex-nr] "\\$1" "\\$2" "\\$3" "\\$4"
..
.\"------------
.\" print line with 'figure' in the text
.\" type stringvar number text override flag refname
.de lix@print-line
.ds lix*text "\\$4
.\"
.ie \\n[Sectf] .ds lix*numb \\n[H1]-\\$3
.el .ds lix*numb \\$3
.\"
.ie !\\n[Of] .ds lix*ds-form .\ \ \"
.el .ds lix*ds-form "\ \(em\ \"
.nr lix*in \\n[.i]
.ds lix*lable \\*[Li\\$1]\ \\*[lix*numb]\\*[lix*ds-form]
.if !'\\$5'' \{\
.	if !0\\$6 .ds lix*lable \\*[Li\\$1]\ \\$5\\*[lix*numb]\\*[lix*ds-form]
.	if 0\\$6=1 .ds lix*lable \\*[Li\\$1]\ \\*[lix*numb]\\$5\\*[lix*ds-form]
.	if 0\\$6=2 .ds lix*lable \\*[Li\\$1]\ \\$5\\*[lix*ds-form]
.\}
.ie \\n[Sectp] .ds lix*pgnr \\*[hd*sect-pg]
.el .ds lix*pgnr \\n[%]
.\" print line if not between DS/DE
.ie \\n[ds*lvl]<1 .lix@print-text "\\*[lix*lable]" "\\*[lix*text]"
.el .lix@embedded-text "\\*[lix*lable]" "\\*[lix*text]"
.\"
.\" save line for LIST OF XXX
.if !r lix*wth\\$1 .nr lix*wth\\$1 0
.if \w@\\*[lix*lable]@>\\n[lix*wth\\$1] .nr lix*wth\\$1 \w@\\*[lix*lable]@
.if \\n[\\$2] .lix@ds-save \\$1 \\*[lix*pgnr] "\\$4" "\\*[lix*lable]"
.if !'\\$7'' .SETR \\$7 \\*[lix*numb]
..
.\"-----------
.de lix@print-text
.SP \\n[Lsp]u
.misc@ev-keep lix
.init@reset
.br
.ie (\w@\\$1\\$2@)>(\\n[.l]-\\n[.i]) \{\
.	in +\w@\\$1@u
.	ti 0
.\}
.el .ce 1
\fB\\$1\fP\\$2
.br
.ev
..
.\" hide printout until diversion is evaluated
.de lix@embedded-text
\!.SP \\n[Lsp]u
\!.misc@ev-keep lix
\!.init@reset
\!.br
\!.ie (\w@\\$1\\$2@)>(\\n[.l]-\\n[.i]) \{\
\!.	in +\w@\\$1@u
\!.	ti 0
\!.\}
\!.el .ce 1
\!\fB\\$1\fP\\$2
\!.br
\!.ev
..
.\"------------
.\" print complete list of XXXX
.de lix@print-ds
.\" arg: fg,tb,ec,ex text
.if !\\n[Cp] .pg@next-page
.\" print LIST OF XXXX
.ce
\\$2
.SP 3
.in \\n[lix*wth\\$1]u
.fi
.lix*ds\\$1
..
.\"------------
.\" save line of list in macro
.de lix@ds-save
.\" type pagenumber text
.am lix*ds\\$1
.lix@dsln \\$1 \\$2 "\\$3" "\\$4" \\$5
\\..
..
.\"------------
.\" print appended macro
.\" lix@dsln type pagenumber text headernr
.de lix@dsln
.nr lix*i \\n[lix*wth\\$1]-\w@\\$4@
.ne 2v
.nr lix*sep \\n[.l]-\\n[lix*i]-\w'\\$4\\$3\\$2'-1m-1n-\\n[.i]
\h'\\n[lix*i]u'\\$4\\$3\h'1n'\l'\\n[lix*sep]u.'\h'1m'\\$2
.SP \\n[toc*spacing]u
..
.\"########################### module fnt ############################
.\" some font macros.
.de R
.ft R
.ul 0
..
.\"-----------
.de fnt@switch
.ul 0
.ds fnt*tmp
.nr fnt*prev \\n[.f]
.nr fnt*i 2 1
.while \\n+[fnt*i]<=\\n[.$] \{\
.	if \\n[fnt*i]>3 .as fnt*tmp \,
.	ie (\\n[fnt*i]%2)=1 .as fnt*tmp \\$1\\$[\\n[fnt*i]]
.	el .as fnt*tmp \\$2\\$[\\n[fnt*i]]
.	if \\n[fnt*i]<\\n[.$] .as fnt*tmp \/
.\}
\&\\*[fnt*tmp]\f[\\n[fnt*prev]]
..
.\"-----------
.de B
.ie \\n[.$] .fnt@switch \fB \f[\\n[.f]] \\$@
.el .ft B
..
.de I
.ie \\n[.$] .fnt@switch \fI \f[\\n[.f]] \\$@
.el .ft I
..
.de IB
.if \\n[.$] .fnt@switch \fI \fB \\$@
..
.de BI
.if \\n[.$] .fnt@switch \fB \fI \\$@
..
.de IR
.if \\n[.$] .fnt@switch \fI \fR \\$@
..
.de RI
.if \\n[.$] .fnt@switch \fR \fI \\$@
..
.de RB
.if \\n[.$] .fnt@switch \fR \fB \\$@
..
.de BR
.if \\n[.$] .fnt@switch \fB \fR \\$@
..
.\"########################### module box ############################
.\" draw a box around some text. Text will be kept on the same page.
.\"
.nr box*ll 0
.\" .B1 and .B2 works like .DS
.de B1
.if \\n[box*ll] .@error "B1: missing B2"
.nr box*ll \\n[.l]
.nr box*ind \\n[.i]
.nr box*hyp \\n[.hy]
.nr box*wid \\n[.l]-\\n[.i]
.\"
.\" jump to new environment.
.ev box*ev
.di box*div
.in 1n
.ll (u;\\n[box*wid]-1n)
.hy \\n[.hy]
..
.de B2
.if !\\n[box*ll] .@error "B2: missing B1"
.br
.di
.nr box*height \\n[dn]
.ne \\n[dn]u+1v
.ll \\n[box*ll]u
.in \\n[box*ind]u
.nr box*y-pos \\n[.d]u
.nf
.box*div
.fi
\v'-1v+.25m'\
\D'l \\n[box*wid]u 0'\
\D'l 0 -\\n[box*height]u'\
\D'l -\\n[box*wid]u 0'\
\D'l 0 \\n[box*height]u'
.br
.sp -1
.ev
.sp .20v
.in \\n[box*ind]u
.ll \\n[box*ll]u
.rm box*div
.nr box*ll 0
..
.\"########################### module ref ############################
.nr ref*nr 0 1
.nr ref*nr-width 5n
.nr ref*flag 0		\" for end-of-text
.ds Rf \v'-.4m'\s-3[\\n+[ref*nr]]\s0\v'.4m'
.\"
.\" start reference
.de RS
.if !''\\$1' .ds \\$1 \\n[ref*nr]
.nr ref*flag 1
.ev ref*ev
.da ref*div
.init@reset
.ll \\n[@ll]u
.in \\n[ref*nr-width]u
.ti -(\w@\\n[ref*nr].@u+1n)
\\n[ref*nr].
.sp -1
..
.de RF
.br
.if \\n[Ls] .SP \\n[Lsp]u
.di
.ev
..
.\"-----------
.de RP
.if !d ref*div .@error "RP: No references!"
.nr ref*flag 0
.nr ref*i 0\\$2
.if \\n[ref*i]<2 .SK
.SP 2
.ref@print-refs
.if 0\\$1<1 .nr ref*nr 0 1
.if ((\\n[ref*i]=0):(\\n[ref*i]=2)) .SK
..
.\"-----------
.\" called by end-of-text!
.de ref@eot-print
.if \\n[ref*flag] \{\
.	if \\n[D]>2 .tm Print references, called by eot
.	nr ref*flag 0
.	br
.	misc@ev-keep ne
.	init@reset
\c
'	bp
.	ev
.	ref@print-refs
.\}
..
.\"-----------
.\" prints the references
.de ref@print-refs
.toc@save 1 "" "\\*[Rp]" \\n[%]
.ev ref*ev
.ce
\fI\\*[Rp]\fP
.sp
.in 0
.nf
.ref*div
.in
.rm ref*div
.ev
..
.\"########################### module app ############################
.\" 
.nr app*nr 0 1
.af app*nr A
.nr app*dnr 0 1
.\"------------
.\" .APP name text
.\" name == "" -> autonumber
.de APP
.\" .if \\n[.$]<2 .@error "APP: too few arguments"
.app@set-ind "\\$1"
.\"
.ie \\n[Aph] .app@header \\*[app*ind] "\\$2"
.el .bp
.app@index "\\*[app*ind]" "\\$2"
..
.\"------------
.\" .APPSK name pages text
.\" name == "" -> autonumber
.de APPSK
.if \\n[.$]<2 .@error "APPSK: too few arguments"
.app@set-ind "\\$1"
.\"
.ie \\n[Aph] .app@header \\*[app*ind] "\\$3"
.el .bp
.app@index "\\*[app*ind]" "\\$3"
.pn +\\$2
..
.\"------------
.de app@set-ind
.ie \w@\\$1@ .ds app*ind \\$1
.el \{\
.	if !\\n[app*dnr] \{\
.		nr H1 0 1
.		af H1 A
.	\}
.	ds app*ind \\n+[app*nr]
.	nr H1 \\n+[app*dnr]
.\}
.\"	clear lower counters
.nr app*i 1 1
.while \\n+[app*i]<8 .nr H\\n[app*i] 0 1
..
.\"------------
.de app@index
.toc@save 1 "" "\\*[App] \\$1: \\$2" \\n[%]
..
.\"------------
.\" app@heaer name text
.de app@header
.bp
.SP (u;\\n[Lsp]*4)
.ce 1
\s+4\fB\\*[App]\ \\$1\fP\s0
.SP (u;\\n[Lsp]*2)
.if \w@\\$2@<\\n[.l] .ce 1
\fB\s+2\\$2\s0\fP
.SP (u;\\n[Lsp]*4)
..
.\"########################### module cov ############################
.\" title stored in diversion cov*title
.\" abstract stored in diversion cov*abstract
.\"	arg to abstract stored in cov*abs-arg
.\"	indent stored in cov*abs-ind
.\" number of authors stored in cov*au
.\" author(s) stored in cov*au!x!y
.\" author(s) title stored in cov*at!x!y
.\" 	x is the author-index [1-cov*au], y is the argument-index [1-9].
.\" author(s) firm stored in cov*firm
.\" new date (if .ND exists) is stored in cov*new-date
.\"
.\"
.ds cov*abs-name ABSTRACT
.\"
.nr cov*au 0
.de TL
.if \\n[.$]>0 .ds cov*title-charge-case \\$1
.if \\n[.$]>1 .ds cov*title-file-case \\$2
.pg@disable-top-trap
.eo
.de cov*title AU
..
.\"-------------------
.de cov@title-end
.ec
..
.\"-------------------
.\" .AU name [initials [loc [dept [ext [room [arg [arg [arg]]]]]]]]
.de AU
.cov@title-end
.pg@disable-top-trap
.if \\n[.$]<1 .@error "AU: no arguments"
.nr cov*au +1
.nr cov*i 0 1
.while \\n[.$]>=\\n+[cov*i] \{\
.	ds cov*au!\\n[cov*au]!\\n[cov*i] "\\$[\\n[cov*i]]
.\}
.if (\\n[.$]>=3)&(\w@\\$3@) \{\
.	if d cov*location-\\$3] \{\
.		ds cov*au!3!\\n[cov*au] \\*[cov*location-\\$3]
.	\}
.\}
..
.\"-------------------
.\" .AT title1 [title2 [... [title9] ]]]]
.\" Well, thats all that COVEND look for.
.\" Must appear directly after .AU
.de AT
.if \\n[.$]<1 .@error "AT: no arguments"
.nr cov*i 0 1
.while \\n[.$]>=\\n+[cov*i] \{\
.	ds cov*at!\\n[cov*au]!\\n[cov*i] "\\$[\\n[cov*i]]
.\}
..
.\"-------------------
.de AF
.cov@title-end
.if \\n[.$]<1 .@error "AF: no arguments"
.ds cov*firm \\$1
..
.de AST
.ds cov*abs-name \\$1
..
.de AS
.pg@disable-top-trap
.if d cov*abstract .@error "AS: only one abstract allowed"
.if !''\\n[.z]' .@error "AS: no diversion allowed (previous .AS?)"
.nr cov*abs-arg 0\\$1
.nr cov*abs-ind (n;0\\$2)
.de cov*abstract AE
..
.de AE
..
.\" I am planning to use mgm some time :-)
.ie \\n[yr]<50 .ds cov*new-date \\*[MO\\n[mo]] \\n[dy], 20\\n[yr]
.el .ds cov*new-date \\*[MO\\n[mo]] \\n[dy], 19\\n[yr]
.als DT cov*new-date
.de ND
.\" don't remember why I did this: .pg@disable-top-trap
.ds cov*new-date \\$1
..
.\"-------------------
.\" save technical numbers.
.de TM
.nr cov*i 0 1
.while \\n[.$]>=\\n+[cov*i] .ds cov*mt-tm!\\n[cov*i] \\$[\\n[cov*i]]
.nr cov*mt-tm-max \\n[.$]
..
.\"-----------------------
.\" cover sheet
.\" the file must have the following last lines (somewhere):
.\" .pg@enable-top-trap
.\" .bp 1
.\" .pg@enable-trap
.ds cov*mt-file!0 0.MT
.ds cov*mt-file!1 0.MT
.ds cov*mt-file!2 0.MT
.ds cov*mt-file!3 0.MT
.ds cov*mt-file!4 4.MT
.ds cov*mt-file!5 5.MT
.ds cov*mt-file!6 0.MT
.\"------------
.de MT
.ie \\n[.$] \{\
.	ie d cov*mt-file!\\$1 .ds cov*mt-type \\$1
.	el .ds cov*mt-type 6
.\}
.el .ds cov*mt-type 1
.ds cov*mt-addresse "\\$2
.ds cov*mt-type-text "\\$1
.ie d @language .ds cov*str mm/\\*[@language]_
.el .ds cov*str mm/
.mso \\*[cov*str]\\*[cov*mt-file!\\*[cov*mt-type]]
..
.de COVER
.ie !\\n[.$] .ds cov*cov-type ms
.el .ds cov*cov-type \\$1
.pg@disable-top-trap
.ie d @language .ds cov*str mm/\\*[@language]_\\*[cov*cov-type].cov
.el .ds cov*str mm/\\*[cov*cov-type].cov
.mso \\*[cov*str]
..
.\"########################### module qrf ############################
.\" forward and backward reference thru special files.
.\"
.\" init reference system
.de INITR
.if \\n[.$]<1 .@error "INITR:filename missing"
.\" ignore if INITR has already been used
.if !r qrf*pass \{\
.\"
.	ds qrf*file \\$1
.	sy test -f \\*[qrf*file].tmp
.	ie \\n[systat] \{\
.		\" PASS 1
.		if \\n[D]=1 .tm PASS 1
.		if \\n[D]>1 .tm INITR: file \\*[qrf*file].tmp, PASS 1
.		nr qrf*pass 1
.		open qrf*stream \\*[qrf*file].tmp
.		write qrf*stream .\\\\" references for \\*[qrf*file]
.		close qrf*stream
.	\}
.	el \{\
.		nr qrf*pass 2
.		if \\n[D]=1 .tm PASS 2
.		if \\n[D]>1 .tm INITR: file \\*[qrf*file].tmp, PASS 2
.		sy mv  \\*[qrf*file].tmp  \\*[qrf*file].qrf
'		so  \\*[qrf*file].qrf
.	\}
.\}
..
.\"---------------
.\" set a reference.
.de SETR
.if \\n[.$]<1 .@error "SETR:reference name missing"
.ie !r qrf*pass .tm "SETR: No .INITR in this file"
.el \{\
.	ds qrf*name qrf*ref-\\$1
.\" probably pass two if already defined
.	if \\n[qrf*pass]<2 \{\
.		if \\n[D]>2 .tm SETR: ref \\*[qrf*name]=\\*[hd*toc-mark],\\n[%]
.		\" heading-number
.		ds \\*[qrf*name]-hn \\*[hd*toc-mark]
.		\" page-number
.		ds \\*[qrf*name]-pn \\n[%]
.		\"
.		\" append to file
.		opena qrf*stream \\*[qrf*file].tmp
.		write qrf*stream .ds \\*[qrf*name]-hn \\*[hd*toc-mark]
.		write qrf*stream .ds \\*[qrf*name]-pn \\n[%]
.		if !'\\$2'' .write qrf*stream .ds \\*[qrf*name]-xx \\$2
.		close qrf*stream
.	\}
.\}
..
.\"---------------
.\" get misc-string, output <->42<-> in pass 1
.\" If two arg -> set var. arg to misc-string.
.de GETST
.if \\n[.$]<1 .@error "GETST:reference name missing"
.if !r qrf*pass .tm "GETST: No .INITR in this file"
.ds qrf*name qrf*ref-\\$1
.ie \\n[qrf*pass]=2 \{\
.	ie !d \\*[qrf*name]-xx .tm "GETHN:\\$1 not defined"
.	el \{\
.		ie \\n[.$]>1 .ds \\$2 \\*[\\*[qrf*name]-xx]
.		el \\*[\\*[qrf*name]-xx]\c
.	\}
.\}
.\" The answer...
.el <->42<->\c
..
.\"---------------
.\" get header-number, output X.X.X. in pass 1
.\" If two arg -> set var. arg to header-number.
.de GETHN
.if \\n[.$]<1 .@error "GETHN:reference name missing"
.if !r qrf*pass .tm "GETHN: No .INITR in this file"
.ds qrf*name qrf*ref-\\$1
.ie \\n[qrf*pass]=2 \{\
.	ie !d \\*[qrf*name]-hn .tm "GETHN:\\$1 not defined"
.	el \{\
.		ie \\n[.$]>1 .ds \\$2 \\*[\\*[qrf*name]-hn]
.		el \\*[\\*[qrf*name]-hn]\c
.	\}
.\}
.el X.X.X.\c
..
.\"---------------
.\" get page-number, output 9999 in pass 1
.\" If two arg -> set var. arg to page-number.
.de GETPN
.if \\n[.$]<1 .@error "GETPN:reference name missing"
.if !r qrf*pass .tm "GETPN: No .INITR in this file"
.ds qrf*name qrf*ref-\\$1
.ie \\n[qrf*pass]=2 \{\
.	ie !d \\*[qrf*name]-pn .tm "GETPN:\\$1 not defined"
.	el \{\
.		ie \\n[.$]>1 .ds \\$2 \\*[\\*[qrf*name]-pn]
.		el \\*[\\*[qrf*name]-pn]\c
.	\}
.\}
.el 9999\c
..
.\"----------
.de GETR
.if \\n[.$]<1 .@error "GETR:reference name missing"
.ie !r qrf*pass \{\
.	tm "GETR: No .INITR in this file"
.\}
.el \{\
.	if \\n[qrf*pass]=2 \{\
.		GETHN \\$1 Qrfh
.		GETPN \\$1 Qrfp
\\*[Qrf]
.	\}
.\}
..
