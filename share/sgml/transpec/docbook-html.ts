<!--

  $Id: docbook-html.ts,v 1.1 1996/11/09 02:04:05 jfieber Exp $

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
  SGML document marked up according to the Docbook DTD into
  HTML.

  There are <em>many</em> parts of the Docbook DTD that this
  translation does nothing with, however most of the basic
  elements handled.

  This needs to be structured in such a way that a post-processor
  can split the file into nice sized chunks.
    
-->

<!DOCTYPE transpec PUBLIC "-//FreeBSD//DTD transpec//EN" [

<!ENTITY lt CDATA "<">
<!ENTITY gt CDATA ">">
<!ENTITY amp CDATA "&">

<!ENTITY r.pass "1">
<!ENTITY r.astart "2">
<!ENTITY r.aend "3">
<!ENTITY r.ignore "6">
<!ENTITY r.admona "7">
<!ENTITY r.admonb "8">

<!ENTITY cmap SYSTEM "/usr/share/sgml/transpec/html.cmap">
<!ENTITY sdata SYSTEM "/usr/share/sgml/transpec/html.sdata">

]>

<transpec>

<!-- Character and SDATA entity mapping -->
<cmap>&cmap;</cmap>
<smap>&sdata;</smap>

<!-- Transform rules -->
<var>partnum	1
<var>chapnum	1
<var>sect1num	1
<var>sect2num	1
<var>sect3num	1
<var>sect4num	1
<var>sect5num	1
<var>subsect	1
<var>app	1
<var>exnum	1
<var>fignum	1
<var>tabnum	1
<var>eqnum	1

<rule>
<match>
<gi>ABBREV
</rule>

<rule>
<match>
<gi>ABSTRACT
<action>
<start>&lt;P ALIGN="CENTER">&lt;STRONG>Abstract&lt;/STRONG>&lt;/P>
&lt;BLOCKQUOTE>
${_attval ID &r.astart;}</start>
<end>^${_attval ID &r.aend;}
&lt;/BLOCKQUOTE>^</end>
</rule>

<rule>
<match>
<gi>ACKNO
</rule>

<rule>
<match>
<gi>ACRONYM
</rule>

<rule>
<match>
<gi>ACTION
</rule>

<rule>
<match>
<gi>ADDRESS
</rule>

<rule>
<match>
<gi>AFFILIATION
<action>
<start>&lt;BR></start>
</rule>

<rule>
<match>
<gi>ANCHOR
</rule>

<rule>
<match>
<gi>APPENDIX
</rule>

<rule>
<match>
<gi>APPLICATION
</rule>

<rule>
<match>
<gi>ARG
</rule>

<rule>
<match>
<gi>ARTHEADER
</rule>

<rule>
<match>
<gi>ARTICLE
</rule>

<rule>
<match>
<gi>ARTPAGENUMS
</rule>

<rule>
<match>
<gi>AUTHOR
<action>
<start>^&lt;P></start>
<!-- <end>&lt;/P>^ -->
</rule>

<rule>
<match>
<gi>AUTHORBLURB
</rule>

<rule>
<match>
<gi>AUTHORGROUP
</rule>

<rule>
<match>
<gi>AUTHORINITIALS
</rule>

<rule>
<match>
<gi>BEGINPAGE
</rule>

<rule>
<match>
<gi>BIBLIODIV
</rule>

<rule>
<match>
<gi>BIBLIOENTRY
</rule>

<rule>
<match>
<gi>BIBLIOGRAPHY
</rule>

<rule>
<match>
<gi>BIBLIOMISC
</rule>

<rule>
<match>
<gi>BLOCKQUOTE
<action>
<start>^&lt;BLOCKQUOTE>
${_attval ID &r.astart;}^</start>
<end>^${_attval ID &r.aend;}
&lt;/BLOCKQUOTE>^</end>
</rule>

<rule>
<match>
<gi>BOOK
<action>
<start>^&lt;!-- Generated on ${date} using ${transpec} -->
&lt;!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
&lt;HTML>&lt;TITLE>${_followrel descendant TITLE &r.pass;}&lt;/TITLE>&lt;BODY>^</start>
<end>^&lt;/BODY>&lt;/HTML></end>
</rule>

<rule>
<match>
<gi>BOOKBIBLIO
<action>
<start>^&lt;H1>${_followrel child TITLE &r.pass;}&lt;/H1></start>
</rule>

<rule>
<match>
<gi>BOOKINFO
</rule>

<rule>
<match>
<gi>BRIDGEHEAD
<action>
<start>^&lt;H4>&lt;EM></start>
<end>&lt;/EM>&lt;/H4>^</end>
</rule>

<rule>
<match>
<gi>CAUTION
<action>
<do>&r.admona;
</rule>

<rule>
<match>
<gi>CHAPTER
<action>
<start>^&lt;!-- Start CHAPTER ${chapnum} (${ID}): 
${_followrel child TITLE &r.pass;} -->^</start>
<end>^&lt;!-- End CHAPTER -->^</end>
<incr>chapnum 
<set>sect1num	1
</rule>

<rule>
<match>
<gi>CITATION
<action>
<start>&lt;CITE></start>
<end>&lt;/CITE></end>
</rule>

<rule>
<match>
<gi>CITEREFENTRY
<action>
<start>&lt;TT></start>
<end>&lt;/TT></end>
</rule>

<rule>
<match>
<gi>CITETITLE
</rule>

<rule>
<match>
<gi>CITY
</rule>

<rule>
<match>
<gi>CLASSNAME
</rule>

<rule>
<match>
<gi>CMDSYNOPSIS
</rule>

<rule>
<match>
<gi>COLLAB
</rule>

<rule>
<match>
<gi>COLLABNAME
</rule>

<rule>
<match>
<gi>COLSPEC
</rule>

<rule>
<match>
<gi>COMMAND
</rule>

<rule>
<match>
<gi>COMMENT
</rule>

<rule>
<match>
<gi>COMPUTEROUTPUT
<action>
<start>&lt;CODE></start>
<end>&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>CONFDATES
</rule>

<rule>
<match>
<gi>CONFGROUP
</rule>

<rule>
<match>
<gi>CONFNUM
</rule>

<rule>
<match>
<gi>CONFSPONSOR
</rule>

<rule>
<match>
<gi>CONFTITLE
</rule>

<rule>
<match>
<gi>CONTRACTNUM
</rule>

<rule>
<match>
<gi>CONTRACTSPONSOR
</rule>

<rule>
<match>
<gi>CONTRIB
</rule>

<rule>
<match>
<gi>COPYRIGHT
<action>
<start>^&lt;P>Copyright &amp;copy; </start>
<!-- <end>&lt;/P>^ -->
</rule>

<rule>
<match>
<gi>CORPAUTHOR
</rule>

<rule>
<match>
<gi>CORPNAME
</rule>

<rule>
<match>
<gi>COUNTRY
</rule>

<rule>
<match>
<gi>DATABASE
</rule>

<rule>
<match>
<gi>DATE
</rule>

<rule>
<match>
<gi>DOCINFO
</rule>

<rule>
<match>
<gi>EDITION
</rule>

<rule>
<match>
<gi>EDITOR
</rule>

<rule>
<match>
<gi>EMAIL
<action>
<start>&lt;A HREF="mailto:${_action &r.pass;}"></start>
<end>&lt;/A></end>
</rule>

<rule>
<match>
<gi>EMPHASIS
<action>
<start>&lt;EM></start>
<end>&lt;/EM></end>
</rule>

<rule>
<match>
<gi>ENTRY
<context>ROW THEAD
<action>
<start>^&lt;TH></start>
<end>&lt;/TH>^</end>
</rule>

<rule>
<match>
<gi>ENTRY
<action>
<start>^&lt;TD></start>
<end>&lt;/TD>^</end>
</rule>

<rule>
<match>
<gi>ENTRYTBL
</rule>

<rule>
<match>
<gi>EPIGRAPH
</rule>

<rule>
<match>
<gi>EQUATION
<action>
<start>^&lt;HR>${_attval ID &r.astart;}&lt;STRONG>${_gi M} ${eqnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}^</start>
<end>^&lt;HR>^</end>
<incr>eqnum
</rule>

<rule>
<match>
<gi>ERRORNAME
</rule>

<rule>
<match>
<gi>ERRORTYPE
</rule>

<rule>
<match>
<gi>EXAMPLE
<action>
<start>^&lt;HR>${_attval ID &r.astart;}&lt;STRONG>${_gi M} ${exnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}^</start>
<end>^&lt;HR>^</end>
<incr>exnum
</rule>

<rule>
<match>
<gi>FAX
</rule>

<rule>
<match>
<gi>FIGURE
<action>
<start>^&lt;HR>${_attval ID &r.astart;}&lt;STRONG>${_gi M} ${fignum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}^</start>
<end>^&lt;HR>^</end>
<incr>fignum
</rule>

<rule>
<match>
<gi>FILENAME
<action>
<start>&lt;B></start>
<end>&lt;/B></end>
</rule>

<rule>
<match>
<gi>FIRSTNAME
<action>
<start>^</start>
<end>^</end>
</rule>

<rule>
<match>
<gi>FIRSTTERM
<action>
<start>&lt;STRONG></start>
<end>&lt;/STRONG></end>
</rule>

<rule>
<match>
<gi>FOOTNOTE
</rule>

<rule>
<match>
<gi>FOOTNOTEREF
</rule>

<rule>
<match>
<gi>FOREIGNPHRASE
<action>
<start>&lt;EM></start>
<end>&lt;/EM></end>
</rule>

<rule>
<match>
<gi>FORMALPARA
</rule>

<rule>
<match>
<gi>FUNCDEF
</rule>

<rule>
<match>
<gi>FUNCPARAMS
</rule>

<rule>
<match>
<gi>FUNCSYNOPSIS
</rule>

<rule>
<match>
<gi>FUNCSYNOPSISINFO
</rule>

<rule>
<match>
<gi>FUNCTION
</rule>

<rule>
<match>
<gi>GLOSSARY
<action>
<start>&lt;H1>${_find gi TITLE &r.pass;}&lt;/H1>
&lt;DL>^</start>
<end>&lt;/DL></end>
</rule>

<rule>
<match>
<gi>GLOSSDEF
<action>
<start>&lt;DD></start>
<end>&lt;/DD>^</end>
</rule>

<rule>
<match>
<gi>GLOSSDIV
</rule>

<rule>
<match>
<gi>GLOSSENTRY
</rule>

<rule>
<match>
<gi>GLOSSLIST
</rule>

<rule>
<match>
<gi>GLOSSSEE
</rule>

<rule>
<match>
<gi>GLOSSSEEALSO
</rule>

<rule>
<match>
<gi>GLOSSTERM
<action>
<start>^&lt;DT>&lt;STRONG></start>
<end>&lt;/STRONG>&lt;/DT></end>
</rule>

<rule>
<match>
<gi>GRAPHIC
<action>
<replace>^&lt;P>&lt;A HREF="${_filename}">[image]&lt;/A>&lt;/P>^</replace>
</rule>

<rule>
<match>
<gi>GROUP
</rule>

<rule>
<match>
<gi>HARDWARE
</rule>

<rule>
<match>
<gi>HIGHLIGHTS
</rule>

<rule>
<match>
<gi>HOLDER
</rule>

<rule>
<match>
<gi>HONORIFIC
</rule>

<rule>
<match>
<gi>IMPORTANT
<action>
<do>&r.admona;
</rule>

<rule>
<match>
<gi>INDEX
</rule>

<rule>
<match>
<gi>INDEXDIV
</rule>

<rule>
<match>
<gi>INDEXENTRY
</rule>

<rule>
<match>
<gi>INDEXTERM
<action>
<ignore>all
</rule>

<rule>
<match>
<gi>INFORMALEQUATION
</rule>

<rule>
<match>
<gi>INFORMALEXAMPLE
</rule>

<rule>
<match>
<gi>INFORMALTABLE
<action>
<start>^&lt;TABLE>^</start>
<end>^&lt;/TABLE>^</end>
</rule>

<rule>
<match>
<gi>INLINEEQUATION
</rule>

<rule>
<match>
<gi>INLINEGRAPHIC
</rule>

<rule>
<match>
<gi>INTERFACE
</rule>

<rule>
<match>
<gi>INTERFACEDEFINITIONID
</rule>

<rule>
<match>
<gi>INVPARTNUMBER
</rule>

<rule>
<match>
<gi>ISBN
</rule>

<rule>
<match>
<gi>ISSN
</rule>

<rule>
<match>
<gi>ISSUENUM
</rule>

<rule>
<match>
<gi>ITEMIZEDLIST
<context>PARA
<action>
<start>&lt;/P>
&lt;UL>^</start>
<end>^&lt;/UL>
&lt;P></end>
</rule>

<rule>
<match>
<gi>ITEMIZEDLIST
<action>
<start>^&lt;UL>^</start>
<end>^&lt;/UL>^</end>
</rule>

<rule>
<match>
<gi>JOBTITLE
</rule>

<rule>
<match>
<gi>KEYCAP
</rule>

<rule>
<match>
<gi>KEYCODE
</rule>

<rule>
<match>
<gi>KEYSYM
</rule>

<rule>
<match>
<gi>LEGALNOTICE
</rule>

<rule>
<match>
<gi>LINEAGE
</rule>

<rule>
<match>
<gi>LINEANNOTATION
<action>
<start>&lt;EM></start>
<end>&lt;/EM></end>
</rule>

<rule>
<match>
<gi>LINK
</rule>

<rule>
<match>
<gi>LISTITEM
<context>VARLISTENTRY
<action>
<start>^&lt;DD></start>
<end>&lt;/DD>^</end>
</rule>

<rule>
<match>
<gi>LISTITEM
<action>
<start>^&lt;LI></start>
<end>&lt;/LI>^</end>
</rule>

<rule>
<match>
<gi>LITERAL
<context>LITERALLAYOUT
</rule>

<rule>
<match>
<gi>LITERAL
<action>
<start>&lt;CODE></start>
<end>&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>LITERALLAYOUT
<context>PARA
<action>
<start>&lt;/P>
&lt;PRE>^</start>
<end>^&lt;/PRE>
&lt;P></end>
</rule>

<rule>
<match>
<gi>LITERALLAYOUT
<action>
<start>^&lt;PRE></start>
<end>^&lt;/PRE></end>
</rule>

<rule>
<match>
<gi>LOT
</rule>

<rule>
<match>
<gi>LOTENTRY
</rule>

<rule>
<match>
<gi>MANVOLNUM
<action>
<start>(</start>
<end>)</end>
</rule>

<rule>
<match>
<gi>MARKUP
</rule>

<rule>
<match>
<gi>MEDIALABEL
</rule>

<rule>
<match>
<gi>MEMBER
<action>
<start>^&lt;LI></start>
<end>&lt;/LI>^</end>
</rule>

<rule>
<match>
<gi>MODESPEC
</rule>

<rule>
<match>
<gi>MSG
</rule>

<rule>
<match>
<gi>MSGAUD
</rule>

<rule>
<match>
<gi>MSGENTRY
</rule>

<rule>
<match>
<gi>MSGEXPLAN
</rule>

<rule>
<match>
<gi>MSGINFO
</rule>

<rule>
<match>
<gi>MSGLEVEL
</rule>

<rule>
<match>
<gi>MSGMAIN
</rule>

<rule>
<match>
<gi>MSGORIG
</rule>

<rule>
<match>
<gi>MSGREL
</rule>

<rule>
<match>
<gi>MSGSET
</rule>

<rule>
<match>
<gi>MSGSUB
</rule>

<rule>
<match>
<gi>MSGTEXT
</rule>

<rule>
<match>
<gi>NOTE
<action>
<do>&r.admonb;
</rule>

<rule>
<match>
<gi>OLINK
</rule>

<rule>
<match>
<gi>OPTION
</rule>

<rule>
<match>
<gi>OPTIONAL
</rule>

<rule>
<match>
<gi>ORDEREDLIST
<context>PARA
<action>
<start>&lt;/P>
&lt;OL>^</start>
<end>^&lt;/OL>
&lt;P></end>
</rule>

<rule>
<match>
<gi>ORDEREDLIST
<action>
<start>^&lt;OL>^</start>
<end>^&lt;/OL>^</end>
</rule>

<rule>
<match>
<gi>ORGDIV
</rule>

<rule>
<match>
<gi>ORGNAME
</rule>

<rule>
<match>
<gi>OTHERADDR
</rule>

<rule>
<match>
<gi>OTHERCREDIT
</rule>

<rule>
<match>
<gi>OTHERNAME
</rule>

<rule>
<match>
<gi>PAGENUMS
</rule>

<rule>
<match>
<gi>PARA
<context>LISTITEM|VARLISTENTRY|GLOSSDEF
<action>
<start>^&lt;P></start>
<end>&lt;/P></end>
</rule>

<rule>
<match>
<gi>PARA
<context>LISTITEM
</rule>

<rule>
<match>
<gi>PARA
<action>
<start>^&lt;P></start>
<!-- <end>&lt;/P> -->
</rule>

<rule>
<match>
<gi>PARAMDEF
</rule>

<rule>
<match>
<gi>PARAMETER
</rule>

<rule>
<match>
<gi>PART
</rule>

<rule>
<match>
<gi>PARTINTRO
</rule>

<rule>
<match>
<gi>PHONE
</rule>

<rule>
<match>
<gi>POB
</rule>

<rule>
<match>
<gi>POSTCODE
</rule>

<rule>
<match>
<gi>PREFACE
</rule>

<rule>
<match>
<gi>PRIMARY
</rule>

<rule>
<match>
<gi>PRIMARYIE
</rule>

<rule>
<match>
<gi>PRINTHISTORY
</rule>

<rule>
<match>
<gi>PROCEDURE
</rule>

<rule>
<match>
<gi>PRODUCTNAME
</rule>

<rule>
<match>
<gi>PRODUCTNUMBER
</rule>

<rule>
<match>
<gi>PROGRAMLISTING
<context>PARA
<action>
<start>&lt;/P>
&lt;BLOCKQUOTE>&lt;PRE>^</start>
<end>^&lt;/PRE>&lt;/BLOCKQUOTE>
&lt;P></end>
</rule>

<rule>
<match>
<gi>PROGRAMLISTING
<action>
<start>^&lt;PRE>^</start>
<end>^&lt;/PRE>^</end>
</rule>

<rule>
<match>
<gi>PROPERTY
</rule>

<rule>
<match>
<gi>PUBDATE
<action>
<start>&lt;P></start>
<end>&lt;/P></end>
</rule>

<rule>
<match>
<gi>PUBLISHER
</rule>

<rule>
<match>
<gi>PUBLISHERNAME
</rule>

<rule>
<match>
<gi>PUBSNUMBER
</rule>

<rule>
<match>
<gi>QUOTE
<action>
<start>``</start>
<end>''</end>
</rule>

<rule>
<match>
<gi>REFCLASS
</rule>

<rule>
<match>
<gi>REFDESCRIPTOR
</rule>

<rule>
<match>
<gi>REFENTRY
<action>
<start>^&lt;!-- Reference Entry --></start>
</rule>

<rule>
<match>
<gi>REFENTRYTITLE
<context>CITEREFENTRY
</rule>

<rule>
<match>
<gi>REFENTRYTITLE
<action>
<start>&lt;HR>&lt;H2>${_followrel ancestor REFENTRY 4}</start>
<end>${_followrel ancestor REFENTRY 5}&lt;/H2></end>
</rule>

<rule>
<match>
<gi>REFERENCE
</rule>

<rule>
<match>
<gi>REFMETA
</rule>

<rule>
<match>
<gi>REFMISCINFO
<action>
<ignore>all
</rule>

<rule>
<match>
<gi>REFNAME
<action>
<start>^&lt;STRONG></start>
<end>^&lt;/STRONG> -^</end>
</rule>

<rule>
<match>
<gi>REFNAMEDIV
<action>
<start> ^&lt;H3>Name&lt;/H3>
&lt;P></start>
<end>^&lt;/P>^</end>
</rule>

<rule>
<match>
<gi>REFPURPOSE
</rule>

<rule>
<match>
<gi>REFSECT1
</rule>

<rule>
<match>
<gi>REFSECT2
</rule>

<rule>
<match>
<gi>REFSECT3
</rule>

<rule>
<match>
<gi>REFSYNOPSISDIV
</rule>

<rule>
<match>
<gi>RELEASEINFO
</rule>

<rule>
<match>
<gi>REPLACEABLE
<action>
<start>&lt;i></start>
<end>&lt;/i></end>
</rule>

<rule>
<match>
<gi>RETURNVALUE
</rule>

<rule>
<match>
<gi>REVHISTORY
</rule>

<rule>
<match>
<gi>REVISION
</rule>

<rule>
<match>
<gi>REVNUMBER
</rule>

<rule>
<match>
<gi>REVREMARK
</rule>

<rule>
<match>
<gi>ROW
<action>
<start>^&lt;TR valign="top">^</start>
<end>^&lt;/TR>^</end>
</rule>

<rule>
<match>
<gi>SCREEN
</rule>

<rule>
<match>
<gi>SCREENINFO
</rule>

<rule>
<match>
<gi>SCREENSHOT
</rule>

<rule>
<match>
<gi>SECONDARY
</rule>

<rule>
<match>
<gi>SECONDARYIE
</rule>

<rule>
<match>
<gi>SECT1
<action>
<incr>sect1num 
<set>sect2num	1
</rule>

<rule>
<match>
<gi>SECT2
<action>
<incr>sect2num
<set>sect3num	1
</rule>

<rule>
<match>
<gi>SECT3
<action>
<incr>sect3num
<set>sect4num	1
</rule>

<rule>
<match>
<gi>SECT4
<action>
<incr>sect4num
<set>sect5num	1
</rule>

<rule>
<match>
<gi>SECT5
<action>
<incr>sect5num
</rule>

<rule>
<match>
<gi>SEE
</rule>

<rule>
<match>
<gi>SEEALSO
</rule>

<rule>
<match>
<gi>SEEALSOIE
</rule>

<rule>
<match>
<gi>SEEIE
</rule>

<rule>
<match>
<gi>SEG
</rule>

<rule>
<match>
<gi>SEGLISTITEM
</rule>

<rule>
<match>
<gi>SEGMENTEDLIST
</rule>

<rule>
<match>
<gi>SEGTITLE
</rule>

<rule>
<match>
<gi>SERIESINFO
</rule>

<rule>
<match>
<gi>SERIESVOLNUMS
</rule>

<rule>
<match>
<gi>SET
</rule>

<rule>
<match>
<gi>SETINDEX
</rule>

<rule>
<match>
<gi>SETINFO
</rule>

<!-- SGMLTag in its variations -->

<rule>
<match>
<gi>SGMLTAG
<attval>class PARAMENTITY
<action>
<start>&lt;CODE>%</start>
<end>&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>SGMLTAG
<attval>class GENENTITY
<action>
<start>&lt;CODE>&amp;</start>
<end>;&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>SGMLTAG
<attval>class STARTTAG
<action>
<start>&lt;CODE>&amp;lt;</start>
<end>>&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>SGMLTAG
<attval>class ENDTAG
<action>
<start>&lt;CODE>&amp;lt;/</start>
<end>>&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>SGMLTAG
<attval>class PI
<action>
<start>&lt;CODE>&amp;lt;?</start>
<end>>&lt;/CODE></end>
</rule>

<rule>
<match>
<gi>SGMLTAG
<attval>class PI
<action>
<start>&lt;CODE>&amp;lt;--</start>
<end>--&amp;lt;/CODE></end>
</rule>

<!-- catchall for remaining types of tags -->
<rule>
<match>
<gi>SGMLTAG
<action>
<start>&lt;CODE>&amp;lt;</start>
<end>&amp;gt;&lt;/CODE></end>
</rule>



<rule>
<match>
<gi>SHORTAFFIL
</rule>

<rule>
<match>
<gi>SIDEBAR
<action>
<do>&r.admonb;
</rule>

<rule>
<match>
<gi>SIMPARA
</rule>

<rule>
<match>
<gi>SIMPLELIST
<action>
<start>^&lt;UL>^</start>
<end>^&lt;/UL>^</end>
</rule>

<rule>
<match>
<gi>SPANSPEC
</rule>

<rule>
<match>
<gi>STATE
</rule>

<rule>
<match>
<gi>STEP
</rule>

<rule>
<match>
<gi>STREET
</rule>

<rule>
<match>
<gi>STRUCTFIELD
</rule>

<rule>
<match>
<gi>STRUCTNAME
</rule>

<rule>
<match>
<gi>SUBSCRIPT
</rule>

<rule>
<match>
<gi>SUBSTEPS
</rule>

<rule>
<match>
<gi>SUBTITLE
<action>
<start>^&lt;P>&lt;EM></start>
<end>&lt;/EM>&lt;/P>^</end>
</rule>

<rule>
<match>
<gi>SUPERSCRIPT
</rule>

<rule>
<match>
<gi>SURNAME
<action>
<start>^</start>
<end>^</end>
</rule>

<rule>
<match>
<gi>SYMBOL
</rule>

<rule>
<match>
<gi>SYNOPFRAGMENT
</rule>

<rule>
<match>
<gi>SYNOPFRAGMENTREF
</rule>

<rule>
<match>
<gi>SYNOPSIS
<context>PARA
<action>
<start>&lt;/P>
&lt;BLOCKQUOTE>&lt;PRE>^</start>
<end>^&lt;/PRE>&lt;/BLOCKQUOTE>
&lt;P></end>
</rule>

<rule>
<match>
<gi>SYNOPSIS
<action>
<start>^&lt;PRE>^</start>
<end>^&lt;/PRE>^</end>
</rule>

<rule>
<match>
<gi>SYSTEMITEM
</rule>

<rule>
<match>
<gi>TABLE
<attval>frame none
<action>
<start>^${_attval ID &r.astart;}&lt;STRONG>${_gi M} ${tabnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}&lt;TABLE>^</start>
<end>^&lt;/TABLE>^</end>
<incr>tabnum
</rule>

<rule>
<match>
<gi>TABLE
<action>
<start>^${_attval ID &r.astart;}&lt;STRONG>${_gi M} ${tabnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}&lt;TABLE border="border">^</start>
<end>^&lt;/TABLE>^</end>
<incr>tabnum
</rule>

<rule>
<match>
<gi>TBODY
</rule>

<rule>
<match>
<gi>TERM
<action>
<start>^&lt;DT></start>
<end>&lt;/DT>^</end>
</rule>

<rule>
<match>
<gi>TERTIARY
</rule>

<rule>
<match>
<gi>TERTIARYIE
</rule>

<rule>
<match>
<gi>TFOOT
</rule>

<rule>
<match>
<gi>TGROUP
</rule>

<rule>
<match>
<gi>THEAD
</rule>

<rule>
<match>
<gi>TIP
<action>
<do>&r.admona;
</rule>

<!-- Titles in the preface -->

<rule>
<match>
<gi>TITLE
<context>PREFACE
<action>
<start>^&lt;H1></start>
<end>&lt;/H1>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT1
<relation>ancestor PREFACE
<action>
<start>^&lt;H2></start>
<end>&lt;/H2>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT2
<relation>ancestor PREFACE
<action>
<start>^&lt;H3></start>
<end>&lt;/H3>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT3
<relation>ancestor PREFACE
<action>
<start>^&lt;H4></start>
<end>&lt;/H4>^</end>
</rule>

<!-- Title in bookbiblio -->

<rule>
<match>
<gi>TITLE
<context>BOOKBIBLIO
<action>
<ignore>all
</rule>

<!-- Titles in other sections -->

<rule>
<match>
<gi>TITLE
<context>CHAPTER
<action>
<start>^&lt;H1>${_followrel parent CHAPTER 4}Chapter ${chapnum}:&lt;BR>^</start>
<end>${_followrel parent CHAPTER 5}&lt;/H1>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>^REF.*
<action>
<start>^&lt;H3></start>
<end>^&lt;/H3></end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT1
<action>
<start>^&lt;H2>${_followrel parent SECT1 4}${chapnum}.${sect1num}^</start>
<end>${_followrel parent SECT1 5}&lt;/H2>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT2
<action>
<start>^&lt;H3>${_followrel parent SECT2 4}${chapnum}.${sect1num}.${sect2num}^</start>
<end>${_followrel parent SECT2 5}&lt;/H3>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT3
<action>
<start>^&lt;H4>${_followrel parent SECT3 4}${chapnum}.${sect1num}.${sect2num}.${sect3num}^</start>
<end>${_followrel parent SECT1 5}&lt;/H4>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT4
<action>
<start>^&lt;H4>${_followrel parent SECT4 4}</start>
<end>${_followrel parent SECT4 5}&lt;/H4>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>SECT5
<action>
<start>^&lt;H4>${_followrel parent SECT5 4}</start>
<end>${_followrel parent SECT5 5}&lt;/H4>^</end>
</rule>

<rule>
<match>
<gi>TITLE
<context>FIGURE|EXAMPLE|TABLE|IMPORTANT
<!--StartText: ^&lt;P>&lt;B> -->
<!--EndText:   &lt;/B>&lt;/P>^ -->
<action>
<ignore>all
</rule>

<rule>
<match>
<gi>TITLE
<context>GLOSSARY
<action>
<ignore>all
</rule>

<rule>
<match>
<gi>TITLE
</rule>

<rule>
<match>
<gi>TITLEABBREV
<action>
<ignore>all
</rule>

<rule>
<match>
<gi>TOC
</rule>

<rule>
<match>
<gi>TOCBACK
</rule>

<rule>
<match>
<gi>TOCCHAP
</rule>

<rule>
<match>
<gi>TOCENTRY
</rule>

<rule>
<match>
<gi>TOCFRONT
</rule>

<rule>
<match>
<gi>TOCLEVEL1
</rule>

<rule>
<match>
<gi>TOCLEVEL2
</rule>

<rule>
<match>
<gi>TOCLEVEL3
</rule>

<rule>
<match>
<gi>TOCLEVEL4
</rule>

<rule>
<match>
<gi>TOCLEVEL5
</rule>

<rule>
<match>
<gi>TOCPART
</rule>

<rule>
<match>
<gi>TOKEN
</rule>

<rule>
<match>
<gi>TRADEMARK
<action>
<end>(TM)</end>
</rule>

<rule>
<match>
<gi>TYPE
</rule>

<rule>
<match>
<gi>ULINK
<action>
<start>&lt;A href="${URL}"></start>
<end>&lt;/A></end>
</rule>

<rule>
<match>
<gi>USERINPUT
<action>
<start>&lt;B></start>
<end>&lt;/B></end>
</rule>

<rule>
<match>
<gi>VARARGS
</rule>

<rule>
<match>
<gi>VARIABLELIST
<context>PARA
<action>
<start>&lt;/P>^&lt;DL>^</start>
<end>^&lt;/DL>^&lt;P></end>
</rule>

<rule>
<match>
<gi>VARIABLELIST
<action>
<start>^&lt;DL>^</start>
<end>^&lt;/DL>^</end>
</rule>

<rule>
<match>
<gi>VARLISTENTRY
</rule>

<rule>
<match>
<gi>VOID
</rule>

<rule>
<match>
<gi>VOLUMENUM
</rule>

<rule>
<match>
<gi>WARNING
<action>
<do>&r.admona;
</rule>

<rule>
<match>
<gi>WORDASWORD
</rule>

<rule>
<match>
<gi>XREF
<action>
<replace>&lt;A HREF="#${LINKEND}">&lt;EM>${_chasetogi TITLE &r.pass}&lt;/EM>&lt;/A></replace>
</rule>

<rule>
<match>
<gi>YEAR
<action>
<start>^</start>
<end>^</end>
</rule>

<!-- Taken from osf-book transpec -->
<rule id="&r.pass;">
<match>
<gi>_pass-text
</rule>

<!-- Just output the anchor tag and ID.  No content. -->
<rule id="&r.astart;">
<match>
<gi>_name
<action>
<start>&lt;A NAME="${ID id}"></start>
<ignore>all
</rule>

<rule id="&r.aend;">
<match>
<gi>_name-end
<action>
<start>&lt;/A></start>
<ignore>all
</rule>

<rule id="4">
<match>
<gi>_anchor-start
<action>
<replace>${_attval ID &r.astart;}</replace>
</rule>

<rule id="5">
<match>
<gi>_anchor-end
<action>
<replace>${_attval ID &r.aend;}</replace>
</rule>

<rule id="&r.ignore;">
<match>
<gi>_no_pass_text
<action>
<ignore>all
</rule>

<rule id="&r.admona;">
<match>
<gi>_admonition
<action>
<start>^&lt;TABLE border="border">
 &lt;TR>&lt;TD>&lt;P align="center">&lt;STRONG>${_attval ID &r.astart;}${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}&lt;/STRONG>&lt;/P>^</start>
<end>^&lt;/TD>&lt;/TR>&lt;/TABLE>^</end>
</rule>

<rule id="&r.admonb;">
<match>
<gi>_admonition
<action>
<start>^&lt;TABLE border="border">
 &lt;TR>&lt;TD>^</start>
<end>^&lt;/TD>&lt;/TR>&lt;/TABLE>^</end>
</rule>

<rule id="8">
<match>
<gi>_titletext
<action>
<start>${_attval ID &r.astart;}${ttext}${_followrel child TITLE &r.pass;}${_attval ID &r.aend;}</start>
<ignore>all
</rule>

<rule>
<match>
<gi>_Start
<action>
<start>^&lt;!-- Magic _Start GI -->^</start>
</rule>

<rule>
<match>
<gi>_End
<action>
<start>^&lt;!-- Magic _End GI -->^</start>
</rule>

</transpec>

