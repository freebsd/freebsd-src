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
  HTML. The output is intended to be post-processed by
  sgmlfmt(1).
  
-->

<!DOCTYPE transpec PUBLIC "-//FreeBSD//DTD transpec//EN" [
<!ENTITY lt CDATA "<">
<!ENTITY gt CDATA ">">
<!--
<!ENTITY % ISOnum PUBLIC 
    "ISO 8879:1986//ENTITIES Numeric and Special Graphic//EN">
    %ISOnum;
-->

<!ENTITY cmap SYSTEM "/usr/share/sgml/transpec/html.cmap">
<!ENTITY sdata SYSTEM "/usr/share/sgml/transpec/html.sdata">

]>

<transpec>

<!-- Character and SDATA entity mapping -->
<cmap>&cmap;</cmap>
<smap>&sdata;</smap>

<!-- Transform rules -->

<rule>
<match>
<gi>LINUXDOC
</rule>

<rule>
<match>
  <gi>ARTICLE
</rule>

<rule>
<match>
  <gi>REPORT
</rule>

<rule>
<match>
  <gi>BOOK
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
</rule>

<rule>
<match>
  <gi>TITLE
<action>
  <start>^&lt;@@title&gt;</start>
</rule>

<rule>
<match>
  <gi>SUBTITLE
<action>
  <start>^&lt;h2&gt;</start>
  <end>&lt;/h2&gt;^</end>
</rule>

<rule>
<match>
<gi>DATE
</rule>

<rule>
<match>
  <gi>AUTHOR
</rule>

<rule>
<match>
  <gi>NAME
<action>
  <start>^&lt;h2&gt;</start>
  <end>&lt;/h2&gt;</end>
</rule>

<rule>
<match>
  <gi>AND
<action>
  <start>and </start>
</rule>

<rule>
<match>
  <gi>THANKS
<action>
  <start>^Thanks </start>
</rule>

<rule>
<match>
  <gi>INST
<action>
  <start>^&lt;h3&gt;</start>
  <end>&lt;/h3&gt;^</end>
</rule>

<rule>
<match>
  <gi>NEWLINE
<action>
  <start>&lt;br&gt;</start>
</rule>

<rule>
<match>
  <gi>LABEL
<action>
  <start>^&lt;@@label&gt;${ID}^</start>
</rule>

<rule>
<match>
  <gi>HEADER
</rule>

<rule>
<match>
  <gi>LHEAD
<action>
  <start>^&lt;!-- </start>
  <end>--&gt;^</end>
</rule>

<rule>
<match>
  <gi>RHEAD
<action>
  <start>^&lt;!-- </start>
  <end>--&gt;^</end>
</rule>

<rule>
<match>
  <gi>COMMENT
<action>
  <start>^&lt;h4&gt;Comment&lt;/h4&gt;^</start>
</rule>

<rule>
<match>
  <gi>ABSTRACT
<action>
  <start>^&lt;p&gt;&lt;hr&gt;&lt;em&gt;</start>
  <end>&lt;/em&gt;&lt;hr&gt;&lt;/p&gt;^</end>
</rule>

<rule>
<match>
  <gi>APPENDIX
<action>
  <start>^&lt;h2&gt;Appendix&lt;/h2&gt;^</start>
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
<action>
  <start>^&lt;@@part&gt;</start>
</rule>

<rule>
<match>
  <gi>CHAPT
<action>
  <start>^&lt;@@sect&gt;</start>
  <end>^&lt;@@endsect&gt;^</end>
</rule>

<rule>
<match>
  <gi>SECT
<action>
  <start>^&lt;@@sect&gt;</start>
  <end>^&lt;@@endsect&gt;^</end>
</rule>

<rule>
<match>
  <gi>SECT1
<action>
  <start>^&lt;@@sect&gt;</start>
  <end>^&lt;@@endsect&gt;^</end>
</rule>

<rule>
<match>
  <gi>SECT2
<action>
  <start>^&lt;@@sect&gt;</start>
  <end>^&lt;@@endsect&gt;^</end>
</rule>

<rule>
<match>
  <gi>SECT3
<action>
  <start>^&lt;@@sect&gt;</start>
  <end>^&lt;@@endsect&gt;^</end>
</rule>

<rule>
<match>
  <gi>SECT4
<action>
  <start>^&lt;@@sect&gt;</start>
  <end>^&lt;@@endsect&gt;^</end>
</rule>

<rule>
<match>
  <gi>HEADING
<action>
  <start>^&lt;@@head&gt;</start>
  <end>^&lt;@@endhead&gt;^</end>
</rule>

<rule>
<match>
  <gi>P
<action>
  <start>&lt;p&gt;</start>
  <end>&lt;/p&gt;^</end>
</rule>

<rule>
<match>
  <gi>ITEMIZE
<action>
  <start>^&lt;ul&gt;^</start>
  <end>^&lt;/ul&gt;^</end>
</rule>

<rule>
<match>
  <gi>ENUM
<action>
  <start>^&lt;ol&gt;^</start>
  <end>^&lt;/ol&gt;^</end>
</rule>

<rule>
<match>
  <gi>DESCRIP
<action>
  <start>^&lt;dl&gt;^</start>
  <end>^&lt;/dl&gt;^</end>
</rule>

<rule>
<match>
  <gi>ITEM
<action>
  <start>^&lt;li&gt;</start>
  <end>&lt;/li&gt;^</end>
</rule>

<rule>
<match>
  <gi>TAG
<action>
  <start>&lt;dt&gt;&lt;b&gt;</start>
  <end>&lt;/b&gt;&lt;dd&gt;</end>
</rule>

<rule>
<match>
  <gi>CITE
<action>
  <start>[&lt;i&gt;${ID}&lt;/i&gt;]</start>
</rule>

<rule>
<match>
  <gi>NCITE
<action>
  <start>[&lt;i&gt;${NOTE} (${ID})&lt;/i&gt;]</start>
</rule>

<rule>
<match>
  <gi>FOOTNOTE
<action>
  <start>^&lt;sl&gt;</start>
  <end>&lt;/sl&gt;^</end>
</rule>

<rule>
<match>
  <gi>SQ
<action>
  <start>"</start>
  <end>"</end>
</rule>

<rule>
<match>
  <gi>LQ
<action>
  <start>^&lt;LQ&gt;^</start>
  <end>^&lt;/LQ&gt;^</end>
</rule>

<rule>
<match>
  <gi>EM
<action>
  <start>&lt;em&gt;</start>
  <end>&lt;/em&gt;</end>
</rule>

<rule>
<match>
  <gi>BF
<action>
  <start>&lt;b&gt;</start>
  <end>&lt;/b&gt;</end>
</rule>

<rule>
<match>
  <gi>IT
<action>
  <start>&lt;i&gt;</start>
  <end>&lt;/i&gt;</end>
</rule>

<rule>
<match>
  <gi>SF
<action>
  <start>&lt;SF&gt;</start>
  <end>&lt;/SF&gt;</end>
</rule>

<rule>
<match>
  <gi>SL
<action>
  <start>&lt;i&gt;</start>
  <end>&lt;/i&gt;</end>
</rule>

<rule>
<match>
  <gi>TT
<action>
  <start>&lt;code&gt;</start>
  <end>&lt;/code&gt;</end>
</rule>

<rule>
<match>
  <gi>URL HTMLURL
  <attval>NAME .
<action>
  <start>&lt;A href="${URL}">${NAME}&lt;/A></start>
</rule>

<rule>
<match>
  <gi>URL HTMLURL
<action>
  <start>&lt;A href="${URL}">${URL}&lt;/A></start>
</rule>

<rule>
<match>
  <gi>REF
  <attval>NAME .
<action>
  <start>^&lt;@@ref&gt;${ID}
${NAME}&lt;/A&gt;</start>
</rule>

<rule>
<match>
  <gi>REF
<action>
  <start>^&lt;@@ref&gt;${ID}
${ID}&lt;/A&gt;</start>
</rule>

<rule>
<match>
  <gi>HREF
<action>
  <start>^&lt;@@ref&gt;${ID}^</start>
</rule>

<rule>
<match>
  <gi>PAGEREF
<action>
  <start>^&lt;@@ref&gt;${ID}^</start>
</rule>

<rule>
<match>
  <gi>X
</rule>

<rule>
<match>
  <gi>MC
<action>
  <start>&lt;MC&gt;</start>
  <end>&lt;/MC&gt;</end>
</rule>

<rule>
<match>
  <gi>BIBLIO
<action>
  <start>^&lt;BIBLIO STYLE="${STYLE}" FILES="${FILES}"&gt;^</start>
</rule>

<rule>
<match>
  <gi>CODE
<action>
  <start>^&lt;hr&gt;&lt;pre&gt;^</start>
  <end>^&lt;/pre&gt;&lt;hr&gt;^</end>
</rule>

<rule>
<match>
  <gi>VERB
<action>
  <start>^&lt;pre&gt;^</start>
  <end>^&lt;/pre&gt;^</end>
</rule>

<rule>
<match>
  <gi>TSCREEN
<action>
  <start>^&lt;blockquote&gt;&lt;code&gt;^</start>
  <end>^&lt;/code&gt;&lt;/blockquote&gt;^</end>
</rule>

<rule>
<match>
  <gi>QUOTE
<action>
  <start>^&lt;blockquote&gt;^</start>
  <end>^&lt;/blockquote&gt;^</end>
</rule>

<rule>
<match>
  <gi>DEF
<action>
  <start>^&lt;DEF&gt;</start>
  <end>^&lt;/DEF&gt;^</end>
</rule>

<rule>
<match>
  <gi>PROP
<action>
  <start>^&lt;PROP&gt;</start>
  <end>^&lt;/PROP&gt;^</end>
</rule>

<rule>
<match>
  <gi>LEMMA
<action>
  <start>^&lt;LEMMA&gt;</start>
  <end>^&lt;/LEMMA&gt;^</end>
</rule>

<rule>
<match>
  <gi>COROLL
<action>
  <start>^&lt;COROLL&gt;</start>
  <end>^&lt;/COROLL&gt;^</end>
</rule>

<rule>
<match>
  <gi>PROOF
<action>
  <start>^&lt;PROOF&gt;</start>
  <end>^&lt;/PROOF&gt;^</end>
</rule>

<rule>
<match>
  <gi>THEOREM
<action>
  <start>^&lt;THEOREM&gt;</start>
  <end>^&lt;/THEOREM&gt;^</end>
</rule>

<rule>
<match>
  <gi>THTAG
<action>
  <start>&lt;THTAG&gt;</start>
  <end>&lt;/THTAG&gt;</end>
</rule>

<rule>
<match>
  <gi>F
<action>
  <start>&lt;F&gt;</start>
  <end>&lt;/F&gt;</end>
</rule>

<rule>
<match>
  <gi>DM
<action>
  <start>^&lt;DM&gt;^</start>
  <end>^&lt;/DM&gt;^</end>
</rule>

<rule>
<match>
  <gi>EQ
<action>
  <start>^&lt;EQ&gt;^</start>
  <end>^&lt;/EQ&gt;^</end>
</rule>

<rule>
<match>
  <gi>FR
<action>
  <start>&lt;FR&gt;</start>
  <end>&lt;/FR&gt;</end>
</rule>

<rule>
<match>
  <gi>NU
<action>
  <start>&lt;NU&gt;</start>
  <end>&lt;/NU&gt;</end>
</rule>

<rule>
<match>
  <gi>DE
<action>
  <start>&lt;DE&gt;</start>
  <end>&lt;/DE&gt;</end>
</rule>

<rule>
<match>
  <gi>LIM
<action>
  <start>&lt;LIM&gt;</start>
  <end>&lt;/LIM&gt;</end>
</rule>

<rule>
<match>
  <gi>OP
<action>
  <start>&lt;OP&gt;</start>
  <end>&lt;/OP&gt;</end>
</rule>

<rule>
<match>
  <gi>LL
<action>
  <start>&lt;LL&gt;</start>
  <end>&lt;/LL&gt;</end>
</rule>

<rule>
<match>
  <gi>UL
<action>
  <start>&lt;UL&gt;</start>
  <end>&lt;/UL&gt;</end>
</rule>

<rule>
<match>
  <gi>OPD
<action>
  <start>&lt;OPD&gt;</start>
  <end>&lt;/OPD&gt;</end>
</rule>

<rule>
<match>
  <gi>PR
<action>
  <start>&lt;PR&gt;</start>
  <end>&lt;/PR&gt;</end>
</rule>

<rule>
<match>
  <gi>IN
<action>
  <start>&lt;INT&gt;</start>
  <end>&lt;/INT&gt;</end>
</rule>

<rule>
<match>
  <gi>SUM
<action>
  <start>&lt;SUM&gt;</start>
  <end>&lt;/SUM&gt;</end>
</rule>

<rule>
<match>
  <gi>ROOT
<action>
  <start>&lt;ROOT&gt;</start>
  <end>&lt;/ROOT&gt;</end>
</rule>

<rule>
<match>
  <gi>AR
<action>
  <start>&lt;AR&gt;</start>
  <end>&lt;/AR&gt;</end>
</rule>

<rule>
<match>
  <gi>ARR
<action>
  <start>&lt;ARR&gt;</start>
</rule>

<rule>
<match>
  <gi>ARC
<action>
  <start>&lt;ARC&gt;</start>
</rule>

<rule>
<match>
  <gi>SUP
<action>
  <start>&lt;SUP&gt;</start>
  <end>&lt;/SUP&gt;</end>
</rule>

<rule>
<match>
  <gi>INF
<action>
  <start>&lt;INF&gt;</start>
  <end>&lt;/INF&gt;</end>
</rule>

<rule>
<match>
  <gi>UNL
<action>
  <start>&lt;UNL&gt;</start>
  <end>&lt;/UNL&gt;</end>
</rule>

<rule>
<match>
  <gi>OVL
<action>
  <start>&lt;OVL&gt;</start>
  <end>&lt;/OVL&gt;</end>
</rule>

<rule>
<match>
  <gi>RF
<action>
  <start>&lt;RF&gt;</start>
  <end>&lt;/RF&gt;</end>
</rule>

<rule>
<match>
  <gi>V
<action>
  <start>&lt;V&gt;</start>
  <end>&lt;/V&gt;</end>
</rule>

<rule>
<match>
  <gi>FI
<action>
  <start>&lt;FI&gt;</start>
  <end>&lt;/FI&gt;</end>
</rule>

<rule>
<match>
  <gi>PHR
<action>
  <start>&lt;PHR&gt;</start>
  <end>&lt;/PHR&gt;</end>
</rule>

<rule>
<match>
  <gi>TU
<action>
  <start>&lt;TU&gt;</start>
</rule>

<rule>
<match>
  <gi>FIGURE
<action>
  <start>^&lt;FIGURE&gt;^</start>
  <end>^&lt;/FIGURE&gt;^</end>
</rule>

<rule>
<match>
  <gi>EPS
<action>
  <start>^&lt;EPS FILE="${FILE}"&gt;^</start>
</rule>

<rule>
<match>
  <gi>PH
<action>
  <start>^&lt;PH VSPACE="${VSPACE}"&gt;^</start>
</rule>

<rule>
<match>
  <gi>CAPTION
<action>
  <start>^&lt;CAPTION&gt;</start>
  <end>&lt;/CAPTION&gt;^</end>
</rule>

<rule>
<match>
  <gi>TABLE
<action>
  <start>^&lt;TABLE&gt;^</start>
  <end>^&lt;/TABLE&gt;^</end>
</rule>

<rule>
<match>
  <gi>TABULAR
<action>
  <start>^&lt;br&gt;^</start>
  <end>^</end>
</rule>

<rule>
<match>
  <gi>ROWSEP
<action>
  <start>&lt;br&gt;^</start>
</rule>

<rule>
<match>
  <gi>COLSEP
</rule>

<rule>
<match>
  <gi>HLINE
<action>
  <start>^&lt;hr&gt;^</start>
</rule>

<rule>
<match>
  <gi>SLIDES
<action>
  <start>^&lt;SLIDES&gt;^</start>
  <end>^&lt;/SLIDES&gt;^</end>
</rule>

<rule>
<match>
  <gi>SLIDE
<action>
  <start>^&lt;SLIDE&gt;^</start>
  <end>^&lt;/SLIDE&gt;^</end>
</rule>

<rule>
<match>
  <gi>LETTER
<action>
  <start>^&lt;LETTER OPTS="${OPTS}"&gt;^</start>
  <end>^&lt;/LETTER&gt;^</end>
</rule>

<rule>
<match>
  <gi>TELEFAX
<action>
  <start>^&lt;TELEFAX OPTS="${OPTS}"&gt;^</start>
  <end>^&lt;/TELEFAX&gt;^</end>
</rule>

<rule>
<match>
  <gi>OPENING
<action>
  <start>^&lt;OPENING&gt;</start>
  <end>&lt;/OPENING&gt;^</end>
</rule>

<rule>
<match>
  <gi>FROM
<action>
  <start>^&lt;FROM&gt;</start>
  <end>^&lt;/FROM&gt;^</end>
</rule>

<rule>
<match>
  <gi>TO
<action>
  <start>^&lt;TO&gt;</start>
  <end>^&lt;/TO&gt;^</end>
</rule>

<rule>
<match>
  <gi>ADDRESS
<action>
  <start>^&lt;ADDRESS&gt;^</start>
  <end>^&lt;/ADDRESS&gt;^</end>
</rule>

<rule>
<match>
  <gi>EMAIL
<action>
  <start>^&lt;@@email&gt;</start>
  <end>&lt;@@endemail&gt;^</end>
</rule>

<rule>
<match>
  <gi>PHONE
<action>
  <start>^&lt;PHONE&gt;</start>
  <end>&lt;/PHONE&gt;^</end>
</rule>

<rule>
<match>
  <gi>FAX
<action>
  <start>^&lt;FAX&gt;</start>
  <end>&lt;/FAX&gt;^</end>
</rule>

<rule>
<match>
  <gi>SUBJECT
<action>
  <start>^&lt;SUBJECT&gt;</start>
  <end>&lt;/SUBJECT&gt;^</end>
</rule>

<rule>
<match>
  <gi>SREF
<action>
  <start>^&lt;SREF&gt;</start>
  <end>&lt;/SREF&gt;^</end>
</rule>

<rule>
<match>
  <gi>RREF
<action>
  <start>^&lt;RREF&gt;</start>
  <end>&lt;/RREF&gt;^</end>
</rule>

<rule>
<match>
  <gi>RDATE
<action>
  <start>^&lt;RDATE&gt;</start>
  <end>&lt;/RDATE&gt;^</end>
</rule>

<rule>
<match>
  <gi>CLOSING
<action>
  <start>^&lt;CLOSING&gt;</start>
  <end>&lt;/CLOSING&gt;^</end>
</rule>

<rule>
<match>
  <gi>CC
<action>
  <start>^&lt;CC&gt;</start>
  <end>&lt;/CC&gt;^</end>
</rule>

<rule>
<match>
  <gi>ENCL
<action>
  <start>^&lt;ENCL&gt;</start>
  <end>&lt;/ENCL&gt;^</end>
</rule>

<rule>
<match>
  <gi>PS
<action>
  <start>^&lt;PS&gt;^</start>
  <end>^&lt;/PS&gt;^</end>


</transpec>
