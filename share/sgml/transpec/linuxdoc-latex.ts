<!--

  $Id: linuxdoc-latex.ts,v 1.2 1996/09/08 19:20:04 jfieber Exp $

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

<!DOCTYPE transpec PUBLIC "-//FreeBSD//DTD transpec//EN" [

<!ENTITY lt CDATA "<">
<!ENTITY gt CDATA ">">
<!ENTITY amp CDATA "&">

<!--<!ENTITY % ISOlat1 PUBLIC "ISO 8879:1986//ENTITIES Added Latin 1//EN">
%ISOlat1;-->

<!ENTITY texstf 
"\def\obeyspaces{\catcode`\ =\active}
{\obeyspaces\global\let =\space}
{\catcode`\^^M=\active %
\gdef\obeylines{\catcode`\^^M=\active \let^^M=\par}%
\global\let^^M=\par} %">

<!ENTITY cmap SYSTEM "/usr/share/sgml/transpec/latex.cmap">
<!ENTITY sdata SYSTEM "/usr/share/sgml/transpec/latex.sdata">

]>

<transpec>

<!-- Character and SDATA entity mapping -->
<cmap>&cmap;</cmap>
<smap>&sdata;</smap>

<!-- Transform rules -->

<rule>
<match>
<gi>LINUXDOC
<action>
<start>% Generated ${date} using ${transpec}
% by ${user}@${host}</start>
</rule>

<rule>
<match>
<gi>ARTICLE
<attval>OPTS .
<action>
<start>^\documentclass{${_gi L}}
\usepackage{${OPTS}}
%\usepackage{linuxdoc}
&texstf;^</start>
<end>^\end{document}^</end>
</rule>

<rule>
<match>
<gi>ARTICLE
<action>
<start>^\documentclass{${_gi L}}
\usepackage{linuxdoc}
&texstf;^</start>
<end>^\end{document}^</end>
</rule>

<rule>
<match>
<gi>REPORT BOOK
<attval>OPTS .
<action>
<start>^\documentclass{${_gi L}}
\usepackage{${OPTS}}
\usepackage{linuxdoc}
\pagestyle{headings}
&texstf;^</start>
<end>^\end{document}^</end>
</rule>

<rule>
<match>
<gi>REPORT BOOK
<action>
<start>^\documentclass{${_gi L}}
\usepackage{linuxdoc}
\pagestyle{headings}
&texstf;^</start>
<end>^\end{document}^</end>
</rule>

<rule>
<match>
<gi>MANPAGE
</rule>

<rule>
<match>
<gi>TITLEPAG
<action>
<end>^\begin{document}
\maketitle^</end>
</rule>

<rule>
<match>
<gi>TITLE
<action>
<start>^\title{</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>SUBTITLE
<action>
<start>^{\large </start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>AUTHOR
<action>
<start>^\author{</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>NAME
</rule>

<rule>
<match>
<gi>AND
<action>
<start>\and ^</start>
</rule>

<rule>
<match>
<gi>THANKS
<action>
<start>\thanks{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>INST
<action>
<start>\\
\\^</start>
</rule>

<rule>
<match>
<gi>DATE
<action>
<start>^\date{</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>NEWLINE
<action>
<start>\\ </start>
</rule>

<rule>
<match>
<gi>LABEL
<action>
<start>\label{${ID}}</start>
</rule>

<rule>
<match>
<gi>HEADER
<action>
<start>^\markboth</start>
</rule>

<rule>
<match>
<gi>LHEAD
<action>
<start>{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>RHEAD
<action>
<start>{</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>COMMENT
<action>
<start>{\tt </start>
<end>}</end>
</rule>

<rule>
<match>
<gi>ABSTRACT
<action>
<start>^\abstract{</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>APPENDIX
<action>
<start>^\appendix^</start>
</rule>

<rule>
<match>
<gi>TOC
<action>
<start>^\tableofcontents^</start>
</rule>

<rule>
<match>
<gi>LOF
<action>
<start>^\listoffigures^</start>
</rule>

<rule>
<match>
<gi>LOT
<action>
<start>^\listoftables^</start>
</rule>

<rule>
<match>
<gi>PART
<action>
<start>^\part</start>
</rule>

<rule>
<match>
<gi>CHAPT
<action>
<start>^\chapter</start>
</rule>

<rule>
<match>
<gi>SECT
<action>
<start>^\section</start>
</rule>

<rule>
<match>
<gi>SECT1
<action>
<start>^\subsection</start>
</rule>

<rule>
<match>
<gi>SECT2
<action>
<start>^\subsubsection</start>
</rule>

<rule>
<match>
<gi>SECT3
<action>
<start>^\paragraph</start>
</rule>

<rule>
<match>
<gi>SECT4
<action>
<start>^\subparagraph</start>
</rule>

<rule>
<match>
<gi>HEADING
<action>
<start>{</start>
<end>}

</end>
</rule>

<rule>
<match>
<gi>P
<action>
<end>^

^</end>
</rule>

<rule>
<match>
<gi>ITEMIZE
<action>
<start>^\begin{itemize}^</start>
<end>^\end{itemize}^</end>
</rule>

<rule>
<match>
<gi>ENUM
<action>
<start>^\begin{enumerate}^</start>
<end>^\end{enumerate}^</end>
</rule>

<rule>
<match>
<gi>LIST
<action>
<start>^\begin{list}{}{}^</start>
<end>^\end{list}^</end>
</rule>

<rule>
<match>
<gi>DESCRIP
<action>
<start>^\begin{description}^</start>
<end>^\end{description}^</end>
</rule>

<rule>
<match>
<gi>ITEM
<action>
<start>^\item </start>
</rule>

<rule>
<match>
<gi>TAG
<action>
<start>^\item\[</start>
<end>\] \mbox{}</end>
</rule>

<rule>
<match>
<gi>CITE
<action>
<start>\cite{${ID}</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>NCITE
<action>
<start>\cite\[${NOTE}\]{${ID}</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>IDX
<action>
<start>\idx{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>CDX
<action>
<start>\cdx{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>FOOTNOTE
<action>
<start>\footnote{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>SQ
<action>
<start>``</start>
<end>''</end>
</rule>

<rule>
<match>
<gi>LQ
<action>
<start>^\begin{quotation}^</start>
<end>^\end{quotation}^</end>
</rule>

<rule>
<match>
<gi>EM
<action>
<start>\emph{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>BF
<action>
<start>{\bf </start>
<end>}</end>
</rule>

<rule>
<match>
<gi>IT
<action>
<start>\textit{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>SF
<action>
<start>\textsf{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>SL
<action>
<start>\textsl{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>RM
<action>
<start>\textrm{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>TT
<action>
<start>\texttt{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>CPARAM
<action>
<start>{\rm $\langle${\sl </start>
<end>}$\rangle$}</end>
</rule>

<rule>
<match>
<gi>REF
<action>
<start>\ref{${ID}}</start>
</rule>

<rule>
<match>
<gi>PAGEREF
<action>
<start>\pageref{${ID}}</start>
</rule>

<!-- A URL with a NAME attribute -->
<rule>
<match>
<gi>URL
<attval>NAME .
<action>
<start>${NAME}\footnote{${URL}}</start>
</rule>

<!-- A URL without a NAME attribute -->
<rule>
<match>
<gi>URL
<action>
<start>\texttt{&lt;URL:${URL}>}</start>
</rule>

<rule>
<match>
<gi>HTMLURL
<action>
<start>${NAME}</start>
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
<start>^\bibliographystyle{${STYLE}}
\bibliography{${FILES}}^</start>
</rule>

<rule>
<match>
<gi>CODE
<action>
<start>^\par
\addvspace{\medskipamount}
\nopagebreak\hrule
\begin{verbatim}^</start>
<end>^\end{verbatim}
\nopagebreak\hrule
\addvspace{\medskipamount}^</end>
</rule>

<rule>
<match>
<gi>VERB
<action>
<start>^{\obeyspaces\obeylines^</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>TSCREEN
<action>
<start>^\begin{quote}{\small\tt^</start>
<end>^}\end{quote}^</end>
</rule>

<rule>
<match>
<gi>QUOTE
<action>
<start>^\begin{quotation}^</start>
<end>^\end{quotation}^</end>
</rule>

<rule>
<match>
<gi>DEF
<action>
<start>^\begin{definition}</start>
<end>^\end{definition}^</end>
</rule>

<rule>
<match>
<gi>PROP
<action>
<start>^\begin{proposition}^</start>
<end>^\end{proposition}^</end>
</rule>

<rule>
<match>
<gi>LEMMA
<action>
<start>^\begin{lemma}</start>
<end>^\end{lemma}^</end>
</rule>

<rule>
<match>
<gi>COROLL
<action>
<start>^\begin{corollary}</start>
<end>^\end{corollary}^</end>
</rule>

<rule>
<match>
<gi>PROOF
<action>
<start>^{\noindent{\bf Proof.}  ^</start>
<end>^}</end>
</rule>

<rule>
<match>
<gi>THEOREM
<action>
<start>^\begin{theorem}</start>
<end>^\end{theorem}^</end>
</rule>

<rule>
<match>
<gi>THTAG
<action>
<start>\[</start>
<end>\]^</end>
</rule>

<rule>
<match>
<gi>F
<action>
<start>\$</start>
<end>\$</end>
</rule>

<rule>
<match>
<gi>DM
<action>
<start>^\\[</start>
<end>\\]^</end>
</rule>

<rule>
<match>
<gi>EQ
<action>
<start>^\begin{equation}^</start>
<end>^\end{equation}^</end>
</rule>

<rule>
<match>
<gi>FR
<action>
<start>\frac</start>
</rule>

<rule>
<match>
<gi>NU
<action>
<start>{</start>
<end>}</end>
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
<start>_{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>UL
<action>
<start>^{</start>
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
<start>\prod</start>
</rule>

<rule>
<match>
<gi>IN
<action>
<start>\int</start>
</rule>

<rule>
<match>
<gi>SUM
<action>
<start>\sum</start>
</rule>

<rule>
<match>
<gi>ROOT
<action>
<start>\sqrt\[[n]\]{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>AR
<action>
<start>^\begin{array}{[ca]}^</start>
<end>^\end{array}^</end>
</rule>

<rule>
<match>
<gi>ARR
<action>
<start>\\ ^</start>
</rule>

<rule>
<match>
<gi>ARC
<action>
<start>& </start>
</rule>

<rule>
<match>
<gi>SUP
<action>
<start>^{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>INF
<action>
<start>_{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>UNL
<action>
<start>\underline{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>OVL
<action>
<start>\overline{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>RF
<action>
<start>\mbox{\tt </start>
<end>}</end>
</rule>

<rule>
<match>
<gi>V
<action>
<start>\vec{</start>
<end>}</end>
</rule>

<rule>
<match>
<gi>FI
<action>
<start>{\cal </start>
<end>}</end>
</rule>

<rule>
<match>
<gi>PHR
<action>
<start>{\rm </start>
<end>}</end>
</rule>

<rule>
<match>
<gi>TU
<action>
<start>\\</start>
</rule>

<rule>
<match>
<gi>FIGURE
<action>
<start>^\begin{figure}\[${LOC}\]^</start>
<end>^\end{figure}^</end>
</rule>

<rule>
<match>
<gi>EPS
<action>
<start>^\centerline{\epsffile{${FILE}}}^</start>
</rule>

<rule>
<match>
<gi>PH
<action>
<start>^\vspace{${VSPACE}}\par^</start>
</rule>

<rule>
<match>
<gi>CAPTION
<action>
<start>^\caption{</start>
<end>}^</end>
</rule>

<rule>
<match>
<gi>TABLE
<action>
<start>^\begin{table}\[${LOC}\]^</start>
<end>^\end{table}^</end>
</rule>

<rule>
<match>
<gi>TABULAR
<action>
<start>^\begin{center}
\begin{tabular}{${CA}}^</start>
<end>^\end{tabular}
\end{center}^</end>
</rule>

<rule>
<match>
<gi>ROWSEP
<action>
<start>\\ ^</start>
</rule>

<rule>
<match>
<gi>COLSEP
<action>
<start>& </start>
</rule>

<rule>
<match>
<gi>HLINE
<action>
<start>^\hline^</start>
</rule>

<rule>
<match>
<gi>SLIDES
<action>
<start>^\documentstyle\[qwertz,dina4,xlatin1,${OPTS}\]{article}
\input{epsf.tex}
\def\title#1{
\begin{center}
\bf\LARGE
\#1
\end{center}
\bigskip
}
\begin{document}^</start>
<end>^\end{document}^</end>
</rule>

<rule>
<match>
<gi>SLIDE
<action>
<end>^\newpage^</end>
</rule>

</transpec>
