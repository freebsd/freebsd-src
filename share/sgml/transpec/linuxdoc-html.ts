<!--

  $Id$

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

<cmap>
&cmap;
</cmap>

<smap>
&sdata;
</smap>

<rule>
<match>
<gi>LINUXDOC
<action>
<start>
</rule>

<rule>
<match>
  <gi>ARTICLE
<action>
  <start>
    <end>
</rule>

<rule>
<match>
  <gi>REPORT
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>BOOK
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>NOTES
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>MANPAGE
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>TITLEPAG
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>TITLE
<action>
  <start>^&lt;@@title&gt;
  <end>
</rule>

<rule>
<match>
  <gi>SUBTITLE
<action>
  <start>^&lt;h2&gt;
  <end>&lt;/h2&gt;^
</rule>

<rule>
<match>
<gi>DATE
<action>
<start>
</rule>

<rule>
<match>
  <gi>AUTHOR
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>NAME
<action>
  <start>^&lt;h2&gt;
  <end>&lt;/h2&gt;
</rule>

<rule>
<match>
  <gi>AND
<action>
  <start>and 
  <end>
</rule>

<rule>
<match>
  <gi>THANKS
<action>
  <start>^Thanks 
  <end>
</rule>

<rule>
<match>
  <gi>INST
<action>
  <start>^&lt;h3&gt;
  <end>&lt;/h3&gt;^
</rule>

<rule>
<match>
  <gi>NEWLINE
<action>
  <start>&lt;br&gt;
</rule>

<rule>
<match>
  <gi>LABEL
<action>
  <start>^&lt;@@label&gt;${ID}^
</rule>

<rule>
<match>
  <gi>HEADER
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>LHEAD
<action>
  <start>^&lt;!-- 
  <end>--&gt;^
</rule>

<rule>
<match>
  <gi>RHEAD
<action>
  <start>^&lt;!-- 
  <end>--&gt;^
</rule>

<rule>
<match>
  <gi>COMMENT
<action>
  <start>^&lt;h4&gt;Comment&lt;/h4&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>ABSTRACT
<action>
  <start>^&lt;p&gt;&lt;hr&gt;&lt;em&gt;
  <end>&lt;/em&gt;&lt;hr&gt;&lt;/p&gt;^
</rule>

<rule>
<match>
  <gi>APPENDIX
<action>
  <start>^&lt;h2&gt;Appendix&lt;/h2&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>TOC
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>LOF
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>LOT
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>PART
<action>
  <start>^&lt;@@part&gt;
  <end>
</rule>

<rule>
<match>
  <gi>CHAPT
<action>
  <start>^&lt;@@sect&gt;
  <end>^&lt;@@endsect&gt;^
</rule>

<rule>
<match>
  <gi>SECT
<action>
  <start>^&lt;@@sect&gt;
  <end>^&lt;@@endsect&gt;^
</rule>

<rule>
<match>
  <gi>SECT1
<action>
  <start>^&lt;@@sect&gt;
  <end>^&lt;@@endsect&gt;^
</rule>

<rule>
<match>
  <gi>SECT2
<action>
  <start>^&lt;@@sect&gt;
  <end>^&lt;@@endsect&gt;^
</rule>

<rule>
<match>
  <gi>SECT3
<action>
  <start>^&lt;@@sect&gt;
  <end>^&lt;@@endsect&gt;^
</rule>

<rule>
<match>
  <gi>SECT4
<action>
  <start>^&lt;@@sect&gt;
  <end>^&lt;@@endsect&gt;^
</rule>

<rule>
<match>
  <gi>HEADING
<action>
  <start>^&lt;@@head&gt;
  <end>^&lt;@@endhead&gt;^
</rule>

<rule>
<match>
  <gi>P
<action>
  <start>&lt;p&gt;
  <end>&lt;/p&gt;^
</rule>

<rule>
<match>
  <gi>ITEMIZE
<action>
  <start>^&lt;ul&gt;^
  <end>^&lt;/ul&gt;^
</rule>

<rule>
<match>
  <gi>ENUM
<action>
  <start>^&lt;ol&gt;^
  <end>^&lt;/ol&gt;^
</rule>

<rule>
<match>
  <gi>DESCRIP
<action>
  <start>^&lt;dl&gt;^
  <end>^&lt;/dl&gt;^
</rule>

<rule>
<match>
  <gi>ITEM
<action>
  <start>^&lt;li&gt;
  <end>&lt;/li&gt;^
</rule>

<rule>
<match>
  <gi>TAG
<action>
  <start>&lt;dt&gt;&lt;b&gt;
  <end>&lt;/b&gt;&lt;dd&gt;
</rule>

<rule>
<match>
  <gi>CITE
<action>
  <start>[&lt;i&gt;${ID}&lt;/i&gt;]
  <end>
</rule>

<rule>
<match>
  <gi>NCITE
<action>
  <start>[&lt;i&gt;${NOTE} (${ID})&lt;/i&gt;]
  <end>
</rule>

<rule>
<match>
  <gi>FOOTNOTE
<action>
  <start>^&lt;sl&gt;
  <end>&lt;/sl&gt;^
</rule>

<rule>
<match>
  <gi>SQ
<action>
  <start>"
  <end>"
</rule>

<rule>
<match>
  <gi>LQ
<action>
  <start>^&lt;LQ&gt;^
  <end>^&lt;/LQ&gt;^
</rule>

<rule>
<match>
  <gi>EM
<action>
  <start>&lt;em&gt;
  <end>&lt;/em&gt;
</rule>

<rule>
<match>
  <gi>BF
<action>
  <start>&lt;b&gt;
  <end>&lt;/b&gt;
</rule>

<rule>
<match>
  <gi>IT
<action>
  <start>&lt;i&gt;
  <end>&lt;/i&gt;
</rule>

<rule>
<match>
  <gi>SF
<action>
  <start>&lt;SF&gt;
  <end>&lt;/SF&gt;
</rule>

<rule>
<match>
  <gi>SL
<action>
  <start>&lt;i&gt;
  <end>&lt;/i&gt;
</rule>

<rule>
<match>
  <gi>TT
<action>
  <start>&lt;code&gt;
  <end>&lt;/code&gt;
</rule>

<rule>
<match>
  <gi>URL HTMLURL
  <attval>NAME .
<action>
  <start>&lt;A href="${URL}">${NAME}&lt;/A>
</rule>

<rule>
<match>
  <gi>URL HTMLURL
<action>
  <start>&lt;A href="${URL}">${URL}&lt;/A>
</rule>

<rule>
<match>
  <gi>REF
  <attval>NAME .
<action>
  <start>
^&lt;@@ref&gt;${ID}
${NAME}&lt;/A&gt;
</rule>

<rule>
<match>
  <gi>REF
<action>
  <start>
^&lt;@@ref&gt;${ID}
${ID}&lt;/A&gt;
</rule>

<rule>
<match>
  <gi>HREF
<action>
  <start>^&lt;@@ref&gt;${ID}^
  <end>
</rule>

<rule>
<match>
  <gi>PAGEREF
<action>
  <start>^&lt;@@ref&gt;${ID}^
  <end>
</rule>

<rule>
<match>
  <gi>X
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>MC
<action>
  <start>&lt;MC&gt;
  <end>&lt;/MC&gt;
</rule>

<rule>
<match>
  <gi>BIBLIO
<action>
  <start>^&lt;BIBLIO STYLE="${STYLE}" FILES="${FILES}"&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>CODE
<action>
  <start>^&lt;hr&gt;&lt;pre&gt;^
  <end>^&lt;/pre&gt;&lt;hr&gt;^
</rule>

<rule>
<match>
  <gi>VERB
<action>
  <start>^&lt;pre&gt;^
  <end>^&lt;/pre&gt;^
</rule>

<rule>
<match>
  <gi>TSCREEN
<action>
  <start>^&lt;blockquote&gt;&lt;code&gt;^
  <end>^&lt;/code&gt;&lt;/blockquote&gt;^
</rule>

<rule>
<match>
  <gi>QUOTE
<action>
  <start>^&lt;blockquote&gt;^
  <end>^&lt;/blockquote&gt;^
</rule>

<rule>
<match>
  <gi>DEF
<action>
  <start>^&lt;DEF&gt;
  <end>^&lt;/DEF&gt;^
</rule>

<rule>
<match>
  <gi>PROP
<action>
  <start>^&lt;PROP&gt;
  <end>^&lt;/PROP&gt;^
</rule>

<rule>
<match>
  <gi>LEMMA
<action>
  <start>^&lt;LEMMA&gt;
  <end>^&lt;/LEMMA&gt;^
</rule>

<rule>
<match>
  <gi>COROLL
<action>
  <start>^&lt;COROLL&gt;
  <end>^&lt;/COROLL&gt;^
</rule>

<rule>
<match>
  <gi>PROOF
<action>
  <start>^&lt;PROOF&gt;
  <end>^&lt;/PROOF&gt;^
</rule>

<rule>
<match>
  <gi>THEOREM
<action>
  <start>^&lt;THEOREM&gt;
  <end>^&lt;/THEOREM&gt;^
</rule>

<rule>
<match>
  <gi>THTAG
<action>
  <start>&lt;THTAG&gt;
  <end>&lt;/THTAG&gt;
</rule>

<rule>
<match>
  <gi>F
<action>
  <start>&lt;F&gt;
  <end>&lt;/F&gt;
</rule>

<rule>
<match>
  <gi>DM
<action>
  <start>^&lt;DM&gt;^
  <end>^&lt;/DM&gt;^
</rule>

<rule>
<match>
  <gi>EQ
<action>
  <start>^&lt;EQ&gt;^
  <end>^&lt;/EQ&gt;^
</rule>

<rule>
<match>
  <gi>FR
<action>
  <start>&lt;FR&gt;
  <end>&lt;/FR&gt;
</rule>

<rule>
<match>
  <gi>NU
<action>
  <start>&lt;NU&gt;
  <end>&lt;/NU&gt;
</rule>

<rule>
<match>
  <gi>DE
<action>
  <start>&lt;DE&gt;
  <end>&lt;/DE&gt;
</rule>

<rule>
<match>
  <gi>LIM
<action>
  <start>&lt;LIM&gt;
  <end>&lt;/LIM&gt;
</rule>

<rule>
<match>
  <gi>OP
<action>
  <start>&lt;OP&gt;
  <end>&lt;/OP&gt;
</rule>

<rule>
<match>
  <gi>LL
<action>
  <start>&lt;LL&gt;
  <end>&lt;/LL&gt;
</rule>

<rule>
<match>
  <gi>UL
<action>
  <start>&lt;UL&gt;
  <end>&lt;/UL&gt;
</rule>

<rule>
<match>
  <gi>OPD
<action>
  <start>&lt;OPD&gt;
  <end>&lt;/OPD&gt;
</rule>

<rule>
<match>
  <gi>PR
<action>
  <start>&lt;PR&gt;
  <end>&lt;/PR&gt;
</rule>

<rule>
<match>
  <gi>IN
<action>
  <start>&lt;INT&gt;
  <end>&lt;/INT&gt;
</rule>

<rule>
<match>
  <gi>SUM
<action>
  <start>&lt;SUM&gt;
  <end>&lt;/SUM&gt;
</rule>

<rule>
<match>
  <gi>ROOT
<action>
  <start>&lt;ROOT&gt;
  <end>&lt;/ROOT&gt;
</rule>

<rule>
<match>
  <gi>AR
<action>
  <start>&lt;AR&gt;
  <end>&lt;/AR&gt;
</rule>

<rule>
<match>
  <gi>ARR
<action>
  <start>&lt;ARR&gt;
  <end>
</rule>

<rule>
<match>
  <gi>ARC
<action>
  <start>&lt;ARC&gt;
  <end>
</rule>

<rule>
<match>
  <gi>SUP
<action>
  <start>&lt;SUP&gt;
  <end>&lt;/SUP&gt;
</rule>

<rule>
<match>
  <gi>INF
<action>
  <start>&lt;INF&gt;
  <end>&lt;/INF&gt;
</rule>

<rule>
<match>
  <gi>UNL
<action>
  <start>&lt;UNL&gt;
  <end>&lt;/UNL&gt;
</rule>

<rule>
<match>
  <gi>OVL
<action>
  <start>&lt;OVL&gt;
  <end>&lt;/OVL&gt;
</rule>

<rule>
<match>
  <gi>RF
<action>
  <start>&lt;RF&gt;
  <end>&lt;/RF&gt;
</rule>

<rule>
<match>
  <gi>V
<action>
  <start>&lt;V&gt;
  <end>&lt;/V&gt;
</rule>

<rule>
<match>
  <gi>FI
<action>
  <start>&lt;FI&gt;
  <end>&lt;/FI&gt;
</rule>

<rule>
<match>
  <gi>PHR
<action>
  <start>&lt;PHR&gt;
  <end>&lt;/PHR&gt;
</rule>

<rule>
<match>
  <gi>TU
<action>
  <start>&lt;TU&gt;
  <end>
</rule>

<rule>
<match>
  <gi>FIGURE
<action>
  <start>^&lt;FIGURE&gt;^
  <end>^&lt;/FIGURE&gt;^
</rule>

<rule>
<match>
  <gi>EPS
<action>
  <start>^&lt;EPS FILE="${FILE}"&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>PH
<action>
  <start>^&lt;PH VSPACE="${VSPACE}"&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>CAPTION
<action>
  <start>^&lt;CAPTION&gt;
  <end>&lt;/CAPTION&gt;^
</rule>

<rule>
<match>
  <gi>TABLE
<action>
  <start>^&lt;TABLE&gt;^
  <end>^&lt;/TABLE&gt;^
</rule>

<rule>
<match>
  <gi>TABULAR
<action>
  <start>^&lt;br&gt;^
  <end>^
</rule>

<rule>
<match>
  <gi>ROWSEP
<action>
  <start>&lt;br&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>COLSEP
<action>
  <start>
  <end>
</rule>

<rule>
<match>
  <gi>HLINE
<action>
  <start>^&lt;hr&gt;^
  <end>
</rule>

<rule>
<match>
  <gi>SLIDES
<action>
  <start>^&lt;SLIDES&gt;^
  <end>^&lt;/SLIDES&gt;^
</rule>

<rule>
<match>
  <gi>SLIDE
<action>
  <start>^&lt;SLIDE&gt;^
  <end>^&lt;/SLIDE&gt;^
</rule>

<rule>
<match>
  <gi>LETTER
<action>
  <start>^&lt;LETTER OPTS="${OPTS}"&gt;^
  <end>^&lt;/LETTER&gt;^
</rule>

<rule>
<match>
  <gi>TELEFAX
<action>
  <start>^&lt;TELEFAX OPTS="${OPTS}"&gt;^
  <end>^&lt;/TELEFAX&gt;^
</rule>

<rule>
<match>
  <gi>OPENING
<action>
  <start>^&lt;OPENING&gt;
  <end>&lt;/OPENING&gt;^
</rule>

<rule>
<match>
  <gi>FROM
<action>
  <start>^&lt;FROM&gt;
  <end>^&lt;/FROM&gt;^
</rule>

<rule>
<match>
  <gi>TO
<action>
  <start>^&lt;TO&gt;

  <end>^&lt;/TO&gt;^
</rule>

<rule>
<match>
  <gi>ADDRESS
<action>
  <start>^&lt;ADDRESS&gt;^
  <end>^&lt;/ADDRESS&gt;^
</rule>

<rule>
<match>
  <gi>EMAIL
<action>
  <start>^&lt;@@email&gt;
  <end>&lt;@@endemail&gt;^
</rule>

<rule>
<match>
  <gi>PHONE
<action>
  <start>^&lt;PHONE&gt;
  <end>&lt;/PHONE&gt;^
</rule>

<rule>
<match>
  <gi>FAX
<action>
  <start>^&lt;FAX&gt;
  <end>&lt;/FAX&gt;^
</rule>

<rule>
<match>
  <gi>SUBJECT
<action>
  <start>^&lt;SUBJECT&gt;
  <end>&lt;/SUBJECT&gt;^
</rule>

<rule>
<match>
  <gi>SREF
<action>
  <start>^&lt;SREF&gt;
  <end>&lt;/SREF&gt;^
</rule>

<rule>
<match>
  <gi>RREF
<action>
  <start>^&lt;RREF&gt;
  <end>&lt;/RREF&gt;^
</rule>

<rule>
<match>
  <gi>RDATE
<action>
  <start>^&lt;RDATE&gt;
  <end>&lt;/RDATE&gt;^
</rule>

<rule>
<match>
  <gi>CLOSING
<action>
  <start>^&lt;CLOSING&gt;
  <end>&lt;/CLOSING&gt;^
</rule>

<rule>
<match>
  <gi>CC
<action>
  <start>^&lt;CC&gt;
  <end>&lt;/CC&gt;^
</rule>

<rule>
<match>
  <gi>ENCL
<action>
  <start>^&lt;ENCL&gt;
  <end>&lt;/ENCL&gt;^
</rule>

<rule>
<match>
  <gi>PS
<action>
  <start>^&lt;PS&gt;^
  <end>^&lt;/PS&gt;^


</transpec>
