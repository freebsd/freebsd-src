<!--

  $Id: docbook-html.ts,v 1.2 1996/12/17 01:48:30 jfieber Exp $

  Copyright (C) 1997
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

  The rules in this file are in alphabetical order according
  to the name of the element to which they apply.  Rules
  intended to be invoked from other rules are at the end of
  the file.  The order of the rules affects how they are processed
  and an alphabetical order helps prevent unpleasnt surprises.
    
-->

<!DOCTYPE transpec PUBLIC "-//FreeBSD//DTD transpec//EN" [

<!ENTITY lt CDATA "<">
<!ENTITY gt CDATA ">">
<!ENTITY amp CDATA "&">

<!-- Entities for frequently used constructs. -->

<!ENTITY wspace CDATA "&nbsp;&nbsp;">

<!ENTITY hlofont CDATA '<FONT COLOR="#660000">'>
<!ENTITY hlofont CDATA '<FONT FACE="Helvetica">'>
<!ENTITY hlcfont CDATA '</FONT>'>

<!ENTITY c.admon CDATA ''>
<!ENTITY c.admon CDATA ' BGCOLOR="FFFFEE"'>

<!ENTITY m.preblk '<start>${_action &r.blkps;t}
&lt;BLOCKQUOTE>&lt;PRE></start>
<end>&lt;/PRE>&lt;/BLOCKQUOTE>
${_action &r.blkpe;t}</end>'>

<!ENTITY m.blk '<start>${_action &r.blkps;t}
&lt;BLOCKQUOTE></start>
<end>&lt;/BLOCKQUOTE>
${_action &r.blkpe;t}</end>'>

<!ENTITY m.tt '<start>&lt;TT></start>
<end>&lt;/TT></end>'>

<!ENTITY m.i '<start>&lt;I></start>
<end>&lt;/I></end>'>

<!ENTITY m.b '<start>&lt;B></start>
<end>&lt;/B></end>'>

<!ENTITY m.u '<start>&lt;U></start>
<end>&lt;/U></end>'>

<!-- Rule names (instant(1) only allows cryptic numbers). -->

<!ENTITY r.pass "1">
<!ENTITY r.ignore "6">
<!ENTITY r.admon "7">
<!ENTITY r.prgi "9">
<!ENTITY r.anchor "10">

<!ENTITY r.pttoc "16">
<!ENTITY r.pttoci "17">
<!ENTITY r.pftoci "18">
<!ENTITY r.chtoc "19">
<!ENTITY r.chtoci "20">
<!ENTITY r.s1toc "21">
<!ENTITY r.s1toci "22">
<!ENTITY r.s2toc "23">
<!ENTITY r.s2toci "24">
<!ENTITY r.s3toc "25">
<!ENTITY r.s3toci "26">
<!ENTITY r.aptoci "27">

<!ENTITY r.fnote "40">
<!ENTITY r.fnotei "41">

<!ENTITY r.blkps "50">
<!ENTITY r.blkpe "51">

<!ENTITY r.hyphen "60">
<!ENTITY r.nl "61">

<!ENTITY cmap SYSTEM "/usr/share/sgml/transpec/html.cmap">
<!ENTITY sdata SYSTEM "/usr/share/sgml/transpec/html.sdata">

]>

<transpec>

<!-- Character and SDATA entity mapping -->

<cmap>&cmap;</cmap>
<smap>&sdata;</smap>

<!-- Numerous counters -->

<var>partnum	1
<var>pfnum  	1
<var>chapnum	1
<var>sect1num	1
<var>sect2num	1
<var>sect3num	1
<var>sect4num	1
<var>sect5num	1
<var>subsect	1
<var>appnum	A
<var>exnum	1
<var>fignum	1
<var>tabnum	1
<var>eqnum	1
<var>fnotenum	1
<var>tmpchapnum 1

<!-- This is a bit of a hack.  The rule for ANCHOR looks at this
     and does nothing if it is set.  Rules that go collecting
     data from other parts of the document should set this to 1
     to prevent a given anchor from appearing more than once.
     Generally, dealing with IDs needs to be completely reworked. -->

<var>anchorinhibit 0</var>

<!-- Transform rules -->

<rule> <!-- Abbreviation, especially one followed by a period -->
<match>
<gi>ABBREV
</rule>

<rule> <!-- Document summary -->
<match>
<gi>ABSTRACT
<action>
<start>${_action &r.anchor;t}</start>
</rule>

<rule> <!-- Keycap used with a meta key to activate a graphical user interface -->
<match>
<gi>ACCEL
<action>
&m.u;
</rule>

<rule> <!-- Acknowledgements in an Article -->
<match>
<gi>ACKNO
<action>
<start>^&lt;P></start>
<end>&lt;/P>^</end>
</rule>

<rule> <!-- Pronounceable contraction of initials -->
<match>
<gi>ACRONYM
<content>^[^a-z]*$</content>
<action>
<replace>&lt;SMALL>${_! echo "${+content}" | tr "[:lower:]" "[:upper:]"}&lt;/SMALL></replace>
</rule>

<rule> <!-- Pronounceable contraction of initials -->
<match>
<gi>ACRONYM
</rule>

<rule> <!-- Function invoked in response to a user event -->
<match>
<gi>ACTION
</rule>

<rule> <!-- Real-world address -->
<match>
<gi>ADDRESS
<action>
<start>${_action &r.blkps;t}
&lt;P></start>
<end>&lt;/P>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Author's institutional affiliation -->
<match>
<gi>AFFILIATION
<action>
<start>&lt;BR></start>
</rule>

<rule> <!-- Prose explanation of a nonprose element -->
<match>
<gi>ALT
<action>
&m.i;
</rule>

<rule> <!-- Spot in text -->
<match>
<gi>ANCHOR
<action>
<start>${_isset anchorinhibit 0 &r.anchor;t}</start>
</rule>

<rule> <!-- Appendix for a Book -->
<match>
<gi>APPENDIX
<action>
<start>^${_set chapnum ${appnum}}&lt;!-- Start APPENDIX ${appnum} (${ID}): 
${_followrel child TITLE &r.pass;} -->^</start>
<end>^&lt;!-- End APPENDIX -->^</end>
<incr>appnum
<set>sect1num	1
</rule>

<rule> <!-- Name of a software program -->
<match>
<gi>APPLICATION
<action>
&m.b;
</rule>

<rule> <!-- Region defined in a graphic or code example -->
<match>
<gi>AREA
</rule>

<rule> <!-- Set of related areas in a graphic or code example -->
<match>
<gi>AREASET
</rule>

<rule> <!-- Collection of regions in a graphic or code example -->
<match>
<gi>AREASPEC
</rule>

<rule> <!-- Argument in a CmdSynopsis -->
<match>
<gi>ARG
<attval>CHOICE OPT
<attval>REP NOREPEAT
<action>
<start>^[</start>
<end>]^</end>
</rule>

<rule> <!-- Argument in a CmdSynopsis -->
<match>
<gi>ARG
<attval>CHOICE OPT
<action>
<start>^[</start>
<end>&amp;nbsp;...&amp;nbsp;]^</end>
</rule>

<rule> <!-- Argument in a CmdSynopsis -->
<match>
<gi>ARG
<attval>CHOICE REQ
<attval>REP NOREPEAT
<action>
<start>^{</start>
<end>}^</end>
</rule>

<rule> <!-- Argument in a CmdSynopsis -->
<match>
<gi>ARG
<attval>CHOICE REQ
<action>
<start>^{</start>
<end>&amp;nbsp;...&amp;nbsp;}^</end>
</rule>

<rule> <!-- Argument in a CmdSynopsis -->
<match>
<gi>ARG
<attval>REP REPEAT
<action>
<start>^</start>
<end>&amp;nbsp;...^</end>
</rule>

<rule> <!-- Argument in a CmdSynopsis -->
<match>
<gi>ARG
<action>
<start>^</start>
<end>^</end>
</rule>

<rule> <!-- Metainformation for an Article -->
<match>
<gi>ARTHEADER
</rule>

<rule> <!-- Article --> 
<match>
<gi>ARTICLE
</rule>

<rule> <!-- Page numbers of an Article as published -->
<match>
<gi>ARTPAGENUMS
</rule>

<rule> <!-- Attribution of content for a BlockQuote or Epigraph -->
<match>
<gi>ATTRIBUTION
<action>
<ignore>all
</rule>

<rule> <!-- Author of a document -->
<match>
<gi>AUTHOR
<action>
<start>^&lt;P></start>
<!-- <end>&lt;/P>^ -->
</rule>

<rule> <!-- Short description of author -->
<match>
<gi>AUTHORBLURB
</rule>

<rule> <!-- Wrapper for Author information -->
<match>
<gi>AUTHORGROUP
</rule>

<rule> <!-- Initials or other identifier for the author of a Revision or Comment -->
<match>
<gi>AUTHORINITIALS
</rule>

<rule> <!-- Page break in a print version of a work that may be displayed online -->
<match>
<gi>BEGINPAGE
</rule>

<rule> <!-- Section of a Bibliography -->
<match>
<gi>BIBLIODIV
</rule>

<rule> <!-- Entry in a Bibliography -->
<match>
<gi>BIBLIOENTRY
</rule>

<rule> <!-- Bibliography -->
<match>
<gi>BIBLIOGRAPHY
</rule>

<rule> <!-- Untyped information supplied in a BiblioEntry or BookInfo -->
<match>
<gi>BIBLIOMISC
</rule>

<rule> <!-- Entry in a Bibliography -->
<match>
<gi>BIBLIOMIXED
</rule>

<rule> <!-- Container for related bibliographic information -->
<match>
<gi>BIBLIOMSET
</rule>

<rule> <!-- Container for related bibliographic information -->
<match>
<gi>BIBLIOSET
</rule>

<rule> <!-- Quotation set off from the main text, rather than occurring in-line -->
<match>
<gi>BLOCKQUOTE
<relation>child ATTRIBUTION
<action>
<start>${_action &r.blkps;t}
${_action &r.anchor;t}&lt;BLOCKQUOTE></start>
<end>&lt;P ALIGN=RIGHT>&lt;I>--
${_followrel child ATTRIBUTION &r.pass;}&lt;/I>&lt;/P>
&lt;/BLOCKQUOTE>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Quotation set off from the main text, rather than occurring in-line -->
<match>
<gi>BLOCKQUOTE
<action>
<start>${_action &r.blkps;t}
${_action &r.anchor;t}&lt;BLOCKQUOTE></start>
<end>&lt;/BLOCKQUOTE>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Book -->
<match>
<gi>BOOK
<action>
<start>^&lt;!-- Generated on ${date} using ${transpec} -->
&lt;!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
&lt;HTML>&lt;TITLE>${_followrel descendant TITLE &r.pass;}&lt;/TITLE>
&lt;BODY BGCOLOR="#FFFFFF" TEXT="#000000">^</start>
<end>^${_set fnotenum 1}${_action &r.fnote;t}
&lt;/BODY>&lt;/HTML></end>
</rule>

<rule> <!-- Information about a book used in a bibliographical citation -->
<match>
<gi>BOOKBIBLIO
<action>
<start>^&lt;H1>&hlofont;${_followrel child TITLE &r.pass;}&hlcfont;&lt;/H1></start>
</rule>

<rule> <!-- Metainformation for a Book -->
<match>
<gi>BOOKINFO
<relation>sibling PART
<action>
<end>^&lt;H1>&hlofont;Contents&hlcfont;&lt/H1>
${_followrel parent BOOK &r.pttoc;t}^</end>
</rule>

<rule> <!-- Metainformation for a Book -->
<match>
<gi>BOOKINFO
<action>
<end>^&lt;H1>&hlofont;Contents&hlcfont;&lt/H1>
${_followrel parent BOOK &r.chtoc;t}^</end>
</rule>

<rule> <!-- Free-floating heading not tied to the Sect hierarchy -->
<match>
<gi>BRIDGEHEAD
<action>
<start>^&lt;H4>&hlofont;&lt;EM></start>
<end>&lt;/EM>&hlcfont;&lt;/H4>^</end>
</rule>

<rule> <!-- Description linked to Areas in a graphic or code example -->
<match>
<gi>CALLOUT
<action>
<start>^&lt;LI></start>
<end>&lt;/LI>^</end>
</rule>

<rule> <!-- Collection of callout descriptions -->
<match>
<gi>CALLOUTLIST
<action>
<start>${_action &r.blkps;t}
&lt;UL>^</start>
<end>^&lt;/UL>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Admonition set off from the text -->
<match>
<gi>CAUTION
<action>
<do>&r.admon;
</rule>

<rule> <!-- Chapter of a Book -->
<match>
<gi>CHAPTER
<action>
<start>^&lt;!-- Start CHAPTER ${chapnum} (${ID}): ${_followrel child TITLE &r.pass;} -->^</start>
<end>^&lt;!-- End CHAPTER -->^</end>
<incr>chapnum 
<set>sect1num	1
</rule>

<rule> <!-- In-line bibliographic reference to another published
            work that uses a reference string, such as an abbreviation
            in a Bibliography -->
<match>
<gi>CITATION
<action>
<start>&lt;CITE></start>
<end>&lt;/CITE></end>
</rule>

<rule> <!-- Citation of a RefEntry -->
<match>
<gi>CITEREFENTRY
<!-- a link to a man page cgi would be good here... -->
</rule>

<rule> <!-- Citation of some published work -->
<match>
<gi>CITETITLE
<action>
<start>&lt;CITE></start>
<end>&lt;/CITE></end>
</rule>

<rule> <!-- Name of a city in an Address -->
<match>
<gi>CITY
</rule>

<rule> <!-- Name of the class to which a program component belongs -->
<match>
<gi>CLASSNAME
</rule>

<rule> <!-- Synopsis for a Command -->
<match>
<gi>CMDSYNOPSIS
<action>
&m.blk;
</rule>

<rule> <!-- Callout area specification embedded in a code example -->
<match>
<gi>CO
</rule>

<rule> <!-- A collaborative group of authors -->
<match>
<gi>COLLAB
</rule>

<rule> <!-- Name of a collaborative group of authors -->
<match>
<gi>COLLABNAME
</rule>

<rule> <!-- Formatting specification for a column in a Table -->
<match>
<gi>COLSPEC
</rule>

<rule> <!-- Executable program, or the entry a user makes to execute a command -->
<match>
<gi>COMMAND
<action>
&m.tt;
</rule>

<rule> <!-- Remark made within the document file that is intended for use
            during interim stages of production -->
<match>
<gi>COMMENT
<action>
<ignore>all
</rule>

<rule> <!-- Data presented to the user by a computer -->
<match>
<gi>COMPUTEROUTPUT
<action>
&m.tt;
</rule>

<rule> <!-- Dates of a conference in connection with which a document was written -->
<match>
<gi>CONFDATES
</rule>

<rule> <!-- Wrapper for information about a conference -->
<match>
<gi>CONFGROUP
</rule>

<rule> <!-- Formal number of a conference in connection with which a document was written -->
<match>
<gi>CONFNUM
</rule>

<rule> <!-- Sponsor of a conference in connection with which a document was written -->
<match>
<gi>CONFSPONSOR
</rule>

<rule> <!-- Title of a conference in connection with which a document was written -->
<match>
<gi>CONFTITLE
</rule>

<rule> <!-- Number of a contract under which a document was written -->
<match>
<gi>CONTRACTNUM
</rule>

<rule> <!-- Sponsor of a contract under which a document was written -->
<match>
<gi>CONTRACTSPONSOR
</rule>

<rule> <!-- Information about the contributions of an Author,
            Editor, or OtherCredit to the work in question -->
<match>
<gi>CONTRIB
</rule>

<rule> <!-- Copyright information about a documen -->
<match>
<gi>COPYRIGHT
<action>
<start>^&lt;P>Copyright &amp;copy; </start>
<!-- <end>&lt;/P>^ -->
</rule>

<rule> <!-- Corporate author of a Book -->
<match>
<gi>CORPAUTHOR
</rule>

<rule> <!-- Name of a corporation -->
<match>
<gi>CORPNAME
</rule>

<rule> <!-- Name of a country in an Address -->
<match>
<gi>COUNTRY
</rule>

<rule> <!-- Name of an organized set of data -->
<match>
<gi>DATABASE
</rule>

<rule> <!-- Date of publication or revision of a document -->
<match>
<gi>DATE
</rule>

<rule> <!-- Metainformation for a book component -->
<match>
<gi>DOCINFO
</rule>

<rule> <!-- Name or number of an edition of a document -->
<match>
<gi>EDITION
</rule>

<rule> <!-- Editor of a document -->
<match>
<gi>EDITOR
</rule>

<rule> <!-- Email address in an Address -->
<match>
<gi>EMAIL
<action>
<start>&lt;A HREF="mailto:${_action &r.pass;}"></start>
<end>&lt;/A></end>
</rule>

<!-- Emphasis: The remap attribute indicates the procedural markup tags
     used in the linuxdoc DTD. -->
     
<rule> <!-- Emphasized text: Bold -->
<match>
<gi>EMPHASIS
<attval>REMAP bf
<action>
&m.b;
</rule>

<rule> <!-- Emphasized text: Italic -->
<match>
<gi>EMPHASIS
<attval>REMAP it
<action>
&m.i;
</rule>

<rule> <!-- Emphasized text: Sans-Serif -->
<match>
<gi>EMPHASIS
<attval>REMAP sf
<action>
<start>&hlofont;</start>
<end>&hlcfont;</end>
</rule>

<rule> <!-- Emphasized text: Slanted -->
<match>
<gi>EMPHASIS
<attval>REMAP sl
<action>
&m.i;
</rule>

<rule> <!-- Emphasized text: Typewriter -->
<match>
<gi>EMPHASIS
<attval>REMAP tt
<action>
&m.tt;
</rule>

<rule> <!-- Emphasized text -->
<match>
<gi>EMPHASIS
<action>
<start>&lt;EM></start>
<end>&lt;/EM></end>
</rule>

<rule> <!-- Cell in a table -->
<match>
<gi>ENTRY
<context>ROW THEAD
<action>
<start>^&lt;TH></start>
<end>&lt;/TH>^</end>
</rule>

<rule> <!-- Cell in a table -->
<match>
<gi>ENTRY
<action>
<start>^&lt;TD></start>
<end>&lt;/TD>^</end>
</rule>

<rule> <!-- Subtable appearing as a table cell -->
<match>
<gi>ENTRYTBL
</rule>

<rule> <!-- Brief text set at the beginning of a document as relevant to its content -->
<match>
<gi>EPIGRAPH
</rule>

<rule> <!-- Formal mathematical equation displayed as a block rather than in-line -->
<match>
<gi>EQUATION
<action>
<start>${_action &r.blkps;t}
{_action &r.anchor;t}&lt;HR NOSHADE>&lt;P>&lt;STRONG>${_gi M} ${eqnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}&lt;/P>^</start>
<end>^&lt;HR NOSHADE>
${_action &r.blkpe;t}</end>
<incr>eqnum
</rule>

<rule> <!-- Error message reported by a computer -->
<match>
<gi>ERRORNAME
<action>
&m.tt;
</rule>

<rule> <!-- Classification of an error message reported by a computer -->
<match>
<gi>ERRORTYPE
</rule>

<rule> <!-- Example of a computer program or related information -->
<match>
<gi>EXAMPLE
<action>
<start>${_action &r.blkps;t}
${_action &r.anchor;t}&lt;HR NOSHADE>&lt;P>&lt;STRONG>${_gi M} ${exnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}&lt;/P>^</start>
<end>^&lt;HR NOSHADE>
${_action &r.blkpe;t}</end>
<incr>exnum
</rule>

<rule> <!-- Fax number in an Address -->
<match>
<gi>FAX
</rule>

<rule> <!-- Formal illustration -->
<match>
<gi>FIGURE
<action>
<start>${_action &r.blkps;t}
${_action &r.anchor;t}&lt;HR NOSHADE>&lt;P>&lt;STRONG>${_gi M} ${fignum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}&lt;/P>^</start>
<end>^&lt;HR NOSHADE>
${_action &r.blkpe;t}</end>
<incr>fignum
</rule>

<rule> <!-- Name of a file, possibly including pathname -->
<match>
<gi>FILENAME
<action>
&m.tt;
</rule>

<rule> <!-- Given name -->
<match>
<gi>FIRSTNAME
<action>
<start>^</start>
<end>^</end>
</rule>

<rule> <!-- First occurrence of a word in a given context -->
<match>
<gi>FIRSTTERM
<action>
&m.i;
</rule>

<rule> <!-- Footnotes: Put both a link and an anchor here
            so that footnotes can be followed in both directions. -->
<match>
<gi>FOOTNOTE
<action>
<start>&lt;A NAME="rfn-${fnotenum}">&lt;/A>&lt;SUP>&lt;SMALL>&lt;A HREF="#fn-${fnotenum}">${fnotenum}&lt/A>&lt/SMALL>&lt;/SUP></start>
<ignore>all
<incr>fnotenum
</rule>

<rule> <!-- Location of a footnote mark -->
<match>
<gi>FOOTNOTEREF
</rule>

<rule> <!-- Word or words in a language other than that of the containing document -->
<match>
<gi>FOREIGNPHRASE
<action>
&m.i;
</rule>

<rule> <!-- Paragraph with a Title -->
<match>
<gi>FORMALPARA
</rule>

<rule> <!-- Function or routine name and its return type in a FuncSynopsis -->
<match>
<gi>FUNCDEF
</rule>

<rule> <!-- Parameter information for a function or routine that is
            pointed to from within a FuncSynopsis -->
<match>
<gi>FUNCPARAMS
</rule>

<rule> <!-- Set of function or routine prototype information in a FuncSynopsis -->
<match>
<gi>FUNCPROTOTYPE
</rule>

<rule> <!-- Synopsis of a Function -->
<match>
<gi>FUNCSYNOPSIS
<action>
&m.blk;
</rule>

<rule> <!-- Information supplementing the FuncDefs of a FuncSynopsis -->
<match>
<gi>FUNCSYNOPSISINFO
</rule>

<rule> <!-- Subroutine in a program or external library -->
<match>
<gi>FUNCTION
<action>
&m.tt;
</rule>

<rule> <!-- Glossary of terms -->
<match>
<gi>GLOSSARY
<action>
<start>&lt;H1>&hlofont;${_find gi TITLE &r.pass;}&hlcfont;&lt;/H1>
&lt;DL>^</start>
<end>&lt;/DL></end>
</rule>

<rule> <!-- Definition attached to a GlossTerm in a GlossEntry -->
<match>
<gi>GLOSSDEF
<action>
<start>&lt;DD></start>
<end>&lt;/DD>^</end>
</rule>

<rule> <!-- Division of a Glossary -->
<match>
<gi>GLOSSDIV
</rule>

<rule> <!-- Entry in a Glossary or GlossList -->
<match>
<gi>GLOSSENTRY
</rule>

<rule> <!-- Wrapper for a set of GlossEntries -->
<match>
<gi>GLOSSLIST
<action>
<start>${_action &r.blkps;t}
&lt;DL></start>
<end>&lt;/DL>
${_action &r.blkps;t}</end>
</rule>

<rule> <!-- Cross-reference from one GlossEntry to another -->
<match>
<gi>GLOSSSEE
</rule>

<rule> <!-- Cross-reference from one GlossDef to another GlossEntry -->
<match>
<gi>GLOSSSEEALSO
</rule>

<rule> <!-- Term outside a Glossary that is defined in some GlossEntry;
            term within a GlossEntry that is defined by that GlossEntry -->
<match>
<gi>GLOSSTERM
<action>
<start>^&lt;DT>&lt;STRONG></start>
<end>&lt;/STRONG>&lt;/DT></end>
</rule>

<rule> <!-- Graphical data, or a pointer to an external entity containing
            such data, to be rendered as an object, not in-line -->
<match>
<gi>GRAPHIC
<action>
<replace>${_action &r.blkps;t}
&lt;P>&lt;A HREF="${_filename}">[image]&lt;/A>&lt;/P>
${_action &r.blkpe;t}</replace>
</rule>

<rule> <!-- Graphic that contains a specification of areas within it that
            have associated callouts -->
<match>
<gi>GRAPHICCO
</rule>

<rule> <!-- Group of constituent parts of a CmdSynopsis -->
<match>
<gi>GROUP
</rule>

<rule> <!-- Text on a button in a graphical user interface -->
<match>
<gi>GUIBUTTON
</rule>

<rule> <!-- Graphic and, or, text appearing as a icon in a graphical user interface -->
<match>
<gi>GUIICON
</rule>

<rule> <!-- Text in a graphical user interface -->
<match>
<gi>GUILABEL
</rule>

<rule> <!-- Name of a menu in a graphical user interface -->
<match>
<gi>GUIMENU
</rule>

<rule> <!-- Name of a terminal menu item in a graphical user interface -->
<match>
<gi>GUIMENUITEM
</rule>

<rule> <!-- Name of a submenu in a graphical user interface -->
<match>
<gi>GUISUBMENU
</rule>

<rule> <!-- Physical part of a computer system -->
<match>
<gi>HARDWARE
</rule>

<rule> <!-- List of main points discussed in a book component such as a Chapter -->
<match>
<gi>HIGHLIGHTS
</rule>

<rule> <!-- Holder of a copyright in the containing document -->
<match>
<gi>HOLDER
</rule>

<rule> <!-- Title of a person -->
<match>
<gi>HONORIFIC
</rule>

<rule> <!-- Admonition set off from the text -->
<match>
<gi>IMPORTANT
<action>
<do>&r.admon;
</rule>

<rule> <!-- Index to content -->
<match>
<gi>INDEX
</rule>

<rule> <!-- Division of an Index -->
<match>
<gi>INDEXDIV
</rule>

<rule> <!-- Entry in an Index -->
<match>
<gi>INDEXENTRY
</rule>

<rule> <!-- Character string to be indexed, occurring in the text flow but not in the text itself -->
<match>
<gi>INDEXTERM
<action>
<ignore>all
</rule>

<rule> <!-- Informal mathematical equation displayed as a block, rather than in-line -->
<match>
<gi>INFORMALEQUATION
<action>
<start>${_action &r.blkps;t}</start>
<end>${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Untitled Example -->
<match>
<gi>INFORMALEXAMPLE
<action>
<start>${_action &r.blkps;t}</start>
<end>${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Untitled table -->
<match>
<gi>INFORMALTABLE
<action>
<start>${_action &r.blkps;t}</start>
<end>${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Untitled mathematical equation occurring in-line or as the
            content of an Equation -->
<match>
<gi>INLINEEQUATION
</rule>

<rule> <!-- Graphical data, or a pointer to an external entity containing
            such data, to be rendered in-line -->
<match>
<gi>INLINEGRAPHIC
</rule>

<rule> <!-- Element of a graphical user interface -->
<match>
<gi>INTERFACE
</rule>

<rule> <!-- Specification for a graphical user interface -->
<match>
<gi>INTERFACEDEFINITION
</rule>

<rule> <!-- Inventory part number -->
<match>
<gi>INVPARTNUMBER
</rule>

<rule> <!-- International Standard Book Number of a document -->
<match>
<gi>ISBN
</rule>

<rule> <!-- International Standard Serial Number of a journal -->
<match>
<gi>ISSN
</rule>

<rule> <!-- Number of an issue of a journal -->
<match>
<gi>ISSUENUM
</rule>

<rule> <!-- List in which each entry is marked with a bullet, dash, or other
            dingbat -->
<match>
<gi>ITEMIZEDLIST
<action>
<start>${_action &r.blkps;t}
&lt;UL>^</start>
<end>^&lt;/UL>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Title of a remunerated position in an Affiliation -->
<match>
<gi>JOBTITLE
</rule>

<rule> <!-- Text printed on a physical key on a computer keyboard, not
            necessarily the same thing as a KeyCode -->
<match>
<gi>KEYCAP
<relation>parent KEYCOMBO
<action>
<end>${_relation sibling+1 KEYCAP &r.hyphen;}${_relation sibling+1 KEYSYM &r.hyphen;}${_relation sibling+1 MOUSEBUTTON &r.hyphen;}${_relation sibling+1 KEYCOMBO &r.nl;}</end>
</rule>

<rule> <!-- Text printed on a physical key on a computer keyboard, not
            necessarily the same thing as a KeyCode -->
<match>
<gi>KEYCAP
<action>
&m.tt;
</rule>

<rule> <!-- Computer's numeric designation of a key on a computer
            keyboard -->
<match>
<gi>KEYCODE
<action>
&m.tt;
</rule>

<rule> <!-- Combination of input actions -->
<match>
<gi>KEYCOMBO
<relation>parent KEYCOMBO
<relation>sibling+1 KEYCOMBO
<action>
<end>^</end>
</rule>

<rule> <!-- Combination of input actions -->
<match>
<gi>KEYCOMBO
<action>
&m.tt;
</rule>

<rule> <!-- Key symbol name, which is not necessarily the same thing as a
            Keycap -->
<match>
<gi>KEYSYM
<relation>parent KEYCOMBO
<action>
<end>${_relation sibling+1 KEYCAP &r.hyphen;}${_relation sibling+1 KEYSYM &r.hyphen;}${_relation sibling+1 MOUSEBUTTON &r.hyphen;}${_relation sibling+1 KEYCOMBO &r.nl;}</end>
</rule>

<rule> <!-- Key symbol name, which is not necessarily the same thing as a
            Keycap -->
<match>
<gi>KEYSYM
<action>
&m.tt;
</rule>

<rule> <!-- Statement of legal obligations or requirements -->
<match>
<gi>LEGALNOTICE
</rule>

<rule> <!-- Portion of a person's name indicating a relationship to ancestors -->
<match>
<gi>LINEAGE
</rule>

<rule> <!-- Writer's or editor's comment on a line of program code
            within an Example, ProgramListing, or Screen -->
<match>
<gi>LINEANNOTATION
<action>
&m.i;
</rule>

<rule> <!-- Hypertext link -->
<match>
<gi>LINK
</rule>

<rule> <!-- Wrapper for the elements of items in an ItemizedList or
            OrderedList -->
<match>
<gi>LISTITEM
<context>VARLISTENTRY
<action>
<start>^&lt;DD></start>
<end>&lt;/DD>^</end>
</rule>

<rule> <!-- Wrapper for the elements of items in an ItemizedList or
            OrderedList -->
<match>
<gi>LISTITEM
<action>
<start>^&lt;LI></start>
<end>&lt;/LI>^</end>
</rule>

<rule> <!-- Literal string, used in-line, that is part of data in a computer -->
<match>
<gi>LITERAL
<context>LITERALLAYOUT
</rule>

<rule> <!-- Literal string, used in-line, that is part of data in a computer -->
<match>
<gi>LITERAL
<action>
&m.tt;
</rule>

<rule> <!-- Wrapper for lines set off from the main text that are not
            tagged as Screens, Examples, or ProgramListing, in which
            line breaks and leading white space are to be regarded as
            significant -->
<match>
<gi>LITERALLAYOUT
<action>
<start>${_action &r.blkps;t}
&lt;PRE>^</start>
<end>^&lt;/PRE>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- List of titles of objects within a document -->
<match>
<gi>LOT
</rule>

<rule> <!-- Entry in a LoT -->
<match>
<gi>LOTENTRY
</rule>

<rule> <!-- Section of a complete set of UNIX reference pages that a
            reference page belongs to -->
<match>
<gi>MANVOLNUM
<action>
<start>(</start>
<end>)</end>
</rule>

<rule> <!-- String of formatting markup in text, which it is desired to represent
            literally -->
<match>
<gi>MARKUP
</rule>

<rule> <!-- Name of the physical medium on or in which some information
            is contained -->
<match>
<gi>MEDIALABEL
</rule>

<rule> <!-- Member of a SimpleList: first -->
<match>
<gi>MEMBER
<nthchild>1
</rule>

<rule> <!-- Member of a SimpleList: middle -->
<match>
<gi>MEMBER
<relation>sibling+ MEMBER
<action>
<start>, </start>
</rule>

<rule> <!-- Member of a SimpleList: last -->
<match>
<gi>MEMBER
<action>
<start>, and </start>
</rule>

<rule> <!-- Menu selection or series of such selections -->
<match>
<gi>MENUCHOICE
</rule>

<rule> <!-- Application-specific information necessary for the completion
            of an OLink -->
<match>
<gi>MODESPEC
</rule>

<rule> <!-- Conventional name of a mouse button -->
<match>
<gi>MOUSEBUTTON
<relation>parent KEYCOMBO
<action>
<end>${_relation sibling+1 KEYCAP &r.hyphen;}${_relation sibling+1 KEYSYM &r.hyphen;}${_relation sibling+1 MOUSEBUTTON &r.hyphen;}${_relation sibling+1 KEYCOMBO &r.nl;}</end>
</rule>

<rule> <!-- Conventional name of a mouse button -->
<match>
<gi>MOUSEBUTTON
<action>
&m.tt;
</rule>

<rule> <!-- Error message and its subparts, along with explanatory text, in a
            MsgEntry -->
<match>
<gi>MSG
</rule>

<rule> <!-- Audience to which a Msg is relevant -->
<match>
<gi>MSGAUD
</rule>

<rule> <!-- Wrapper for an entry in a MsgSet -->
<match>
<gi>MSGENTRY
</rule>

<rule> <!-- Explanatory material relating to a Msg -->
<match>
<gi>MSGEXPLAN
</rule>

<rule> <!-- Information about the Msg that contains it -->
<match>
<gi>MSGINFO
</rule>

<rule> <!-- Level of importance or severity of a Msg -->
<match>
<gi>MSGLEVEL
</rule>

<rule> <!-- Main error message of a Msg -->
<match>
<gi>MSGMAIN
</rule>

<rule> <!-- Origin of a Msg -->
<match>
<gi>MSGORIG
</rule>

<rule> <!-- Subpart of a Msg containing a message that is related to the main
            message -->
<match>
<gi>MSGREL
</rule>

<rule> <!-- List of error messages produced by a system, with various
            additional information -->
<match>
<gi>MSGSET
</rule>

<rule> <!-- Optional subpart of a Msg, which might contain messages that
            appear in various contexts -->
<match>
<gi>MSGSUB
</rule>

<rule> <!-- Contents of the parts of Msg -->
<match>
<gi>MSGTEXT
</rule>

<rule> <!-- Message to the user, set off from the text -->
<match>
<gi>NOTE
<action>
<do>&r.admon;
</rule>

<rule> <!-- Link that addresses its target by use of an entity -->
<match>
<gi>OLINK
</rule>

<rule> <!-- Option for a computer program command -->
<match>
<gi>OPTION
<action>
&m.tt;
</rule>

<rule> <!-- Optional information contained in a Synopsis -->
<match>
<gi>OPTIONAL
<action>
<start>[</start>
<end>]</end>
</rule>

<rule> <!-- List in which each entry is marked with a sequentially
            incremented label -->
<match>
<gi>ORDEREDLIST
<action>
<start>${_action &r.blkps;t}
&lt;OL>^</start>
<end>^&lt;/OL>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Division of an organization -->
<match>
<gi>ORGDIV
</rule>

<rule> <!-- Name of an organization other than a corporation -->
<match>
<gi>ORGNAME
</rule>

<rule> <!-- Uncategorized information in Address -->
<match>
<gi>OTHERADDR
</rule>

<rule> <!-- Person or entity to be credited, other than an Author or Editor -->
<match>
<gi>OTHERCREDIT
</rule>

<rule> <!-- Name component that is not a Firstname, Surname, or Lineage -->
<match>
<gi>OTHERNAME
</rule>

<rule> <!-- Numbers of the pages contained in a Book, for use in its
            BookBiblio -->
<match>
<gi>PAGENUMS
</rule>

<rule> <!-- Paragraph -->
<match>
<gi>PARA
<context>LISTITEM|VARLISTENTRY|STEP|GLOSSDEF
<action>
<start>^&lt;P></start>
<end>&lt;/P></end>
</rule>

<rule> <!-- Paragraph -->
<match>
<gi>PARA
<action>
<start>^&lt;P></start>
<end>&lt;/P></end>
</rule>

<rule> <!-- Data type information and the name of the Parameter this
            information applies to -->
<match>
<gi>PARAMDEF
</rule>

<rule> <!-- Part of an instruction to a computer -->
<match>
<gi>PARAMETER
<action>
&m.tt;
</rule>

<rule> <!-- Section of a Book containing book components -->
<match>
<gi>PART
<action>
<start>^&lt;!-- Start PART ${partnum} (${ID}): 
${_followrel child TITLE &r.pass;} -->^</start>
<end>^&lt;!-- End PART -->^</end>
<incr>partnum 
</rule>

<rule> <!-- Introduction to the contents of a Part -->
<match>
<gi>PARTINTRO
</rule>

<rule> <!-- Telephone number in an Address -->
<match>
<gi>PHONE
</rule>

<rule> <!-- Post office box number in an Address -->
<match>
<gi>POB
</rule>

<rule> <!-- Postal code in an Address -->
<match>
<gi>POSTCODE
</rule>

<rule> <!-- Introductory textual matter in a Book -->
<match>
<gi>PREFACE
<action>
<start>^&lt;!-- Start PREFACE (${ID}): ${_followrel child TITLE &r.pass;} -->^</start>
<end>^&lt;!-- End PREFACE -->^</end>
<incr>pfnum
<set>sect1num 1
</rule>

<rule> <!-- Word or phrase occurring in the text that is to appear in the index
            under as a primary entry -->
<match>
<gi>PRIMARY
</rule>

<rule> <!-- Primary entry in an Index, not in the text -->
<match>
<gi>PRIMARYIE
</rule>

<rule> <!-- Printing history of a Book -->
<match>
<gi>PRINTHISTORY
</rule>

<rule> <!-- List of operations to be performed -->
<match>
<gi>PROCEDURE
<action>
<start>${_action &r.blkps;t}
&lt;OL>^</start>
<end>^&lt;/OL>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Formal name for a product -->
<match>
<gi>PRODUCTNAME
</rule>

<rule> <!-- Number assigned to a product -->
<match>
<gi>PRODUCTNUMBER
</rule>

<rule> <!-- Listing of all or part of a program -->
<match>
<gi>PROGRAMLISTING
<relation>parent EXAMPLE
<action>
<start>^&lt;PRE></start>
<end>&lt;/PRE>^</end>
</rule>

<rule> <!-- Listing of all or part of a program -->
<match>
<gi>PROGRAMLISTING
<action>
&m.preblk;
</rule>

<rule> <!-- Listing of a program or related information containing
            areas with associated callouts -->
<match>
<gi>PROGRAMLISTINGCO
</rule>

<rule> <!-- Defined set of data associated with a window -->
<match>
<gi>PROPERTY
</rule>

<rule> <!-- Date of publication of a document -->
<match>
<gi>PUBDATE
<action>
<start>&lt;P></start>
<end>&lt;/P></end>
</rule>

<rule> <!-- Publisher of a document -->
<match>
<gi>PUBLISHER
</rule>

<rule> <!-- Name of a publisher of a document in Publisher -->
<match>
<gi>PUBLISHERNAME
</rule>

<rule> <!-- Number assigned to a publication, other than an ISBN or
            ISSN or InvPartNumber -->
<match>
<gi>PUBSNUMBER
</rule>

<rule> <!-- In-line quotation -->
<match>
<gi>QUOTE
<relation>parent QUOTE
<action>
<start>`</start>
<end>'</end>
</rule>

<rule> <!-- In-line quotation -->
<match>
<gi>QUOTE
<action>
<start>``</start>
<end>''</end>
</rule>

<rule> <!-- Applicability or scope of the topic of a RefEntry -->
<match>
<gi>REFCLASS
</rule>

<rule> <!-- Substitute for RefName to be used when a RefEntry covers
            more than one topic and none of the topic names is to be
            used as the sort name -->
<match>
<gi>REFDESCRIPTOR
</rule>

<rule> <!-- A reference page -->
<match>
<gi>REFENTRY
<action>
<start>^&lt;!-- Reference Entry --></start>
</rule>

<rule> <!-- Primary name given to a reference page for sorting and
            indexing -->
<match>
<gi>REFENTRYTITLE
<context>CITEREFENTRY
<action>
&m.i;
</rule>

<rule> <!-- Primary name given to a reference page for sorting and
            indexing -->
<match>
<gi>REFENTRYTITLE
<action>
<start>&lt;HR NOSHADE>&lt;H2>${_followrel ancestor REFENTRY &r.anchor;}</start>
<end>&lt;/H2></end>
</rule>

<rule> <!-- Collection of RefEntries, forming a book component -->
<match>
<gi>REFERENCE
</rule>

<rule> <!-- First major division of a reference page, in which metainformation
            about the reference page is supplied -->
<match>
<gi>REFMETA
</rule>

<rule> <!-- Information in RefMeta that may be supplied by vendors, or a
            descriptive phrase for use in a print header -->
<match>
<gi>REFMISCINFO
<action>
<ignore>all
</rule>

<rule> <!-- Subject or subjects of a reference page -->
<match>
<gi>REFNAME
<action>
<start>^&lt;STRONG></start>
<end>^&lt;/STRONG> -^</end>
</rule>

<rule> <!-- Major division of a reference page containing naming,
            purpose, and classification information -->
<match>
<gi>REFNAMEDIV
<action>
<start> ^&lt;H3>Name&lt;/H3>
&lt;P></start>
<end>^&lt;/P>^</end>
</rule>

<rule> <!-- Subject of a reference page -->
<match>
<gi>REFPURPOSE
</rule>

<rule> <!-- Major subsection of a RefEntry -->
<match>
<gi>REFSECT1
</rule>

<rule> <!-- Subsection of a RefSect1 -->
<match>
<gi>REFSECT2
</rule>

<rule> <!-- Subsection of a Refsect2 -->
<match>
<gi>REFSECT3
</rule>

<rule> <!-- Major division of a reference page, in which the syntax of
            the subject of the reference page is indicated -->
<match>
<gi>REFSYNOPSISDIV
</rule>

<rule> <!-- Information about a particular version of a document -->
<match>
<gi>RELEASEINFO
</rule>

<rule> <!-- Content that may be replaced in a synopsis or command line -->
<match>
<gi>REPLACEABLE
<action>
&m.i;
</rule>

<rule> <!-- Value returned by a function -->
<match>
<gi>RETURNVALUE
</rule>

<rule> <!-- Revisions to a document -->
<match>
<gi>REVHISTORY
</rule>

<rule> <!-- Entry in RevHistory, describing some revision made to the text -->
<match>
<gi>REVISION
</rule>

<rule> <!-- Number of a Revision -->
<match>
<gi>REVNUMBER
</rule>

<rule> <!-- Description of a Revision -->
<match>
<gi>REVREMARK
</rule>

<rule> <!-- Row in a TBody, THead, or TFoot -->
<match>
<gi>ROW
<action>
<start>^&lt;TR valign="top">^</start>
<end>^&lt;/TR>^</end>
</rule>

<rule> <!-- Line break in a command synopsis -->
<match>
<gi>SBR
<action>
<start>&lt;BR></start>
</rule>

<rule> <!-- Text that a user sees or might see on a computer screen -->
<match>
<gi>SCREEN
<relation>parent EXAMPLE
<action>
<start>^&lt;PRE></start>
<end>&lt;/PRE>^</end>
</rule>

<rule> <!-- Text that a user sees or might see on a computer screen -->
<match>
<gi>SCREEN
<action>
&m.preblk;
</rule>

<rule> <!-- Screen containing areas with associated callouts -->
<match>
<gi>SCREENCO
</rule>

<rule> <!-- Information about how a ScreenShot was produced -->
<match>
<gi>SCREENINFO
</rule>

<rule> <!-- Representation of what the user sees or might see on a
            computer screen -->
<match>
<gi>SCREENSHOT
<relation>parent EXAMPLE
</rule>

<rule> <!-- Representation of what the user sees or might see on a
            computer screen -->
<match>
<gi>SCREENSHOT
<action>
<start>${_action &r.blkps;t}</start>
<end>${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Word or phrase in the text that is to appear in the index beneath
            a Primary entry -->
<match>
<gi>SECONDARY
</rule>

<rule> <!-- Part of IndexEntry, like PrimaryIE -->
<match>
<gi>SECONDARYIE
</rule>

<rule> <!-- Top-level section of a book component, including the Title of that
            section -->
<match>
<gi>SECT1
<action>
<incr>sect1num 
<set>sect2num	1
</rule>

<rule> <!-- Section beginning with a second-level heading -->
<match>
<gi>SECT2
<action>
<incr>sect2num
<set>sect3num	1
</rule>

<rule> <!-- Section beginning with a third-level heading -->
<match>
<gi>SECT3
<action>
<incr>sect3num
<set>sect4num	1
</rule>

<rule> <!-- Section beginning with a fourth-level heading -->
<match>
<gi>SECT4
<action>
<incr>sect4num
<set>sect5num	1
</rule>

<rule> <!-- Section beginning with a fifth-level heading -->
<match>
<gi>SECT5
<action>
<incr>sect5num
</rule>

<rule> <!-- Part of IndexTerm, indicating, for a word or phrase in the text,
            the index entry to which the reader is to be directed when he
            consults the stub index entry for another element within the
            IndexTerm -->
<match>
<gi>SEE
</rule>

<rule> <!-- Like See, but indicates the index entries to which the reader
            is also to be directed when he consults a full index entry -->
<match>
<gi>SEEALSO
</rule>

<rule> <!-- "See also" entry in an Index -->
<match>
<gi>SEEALSOIE
</rule>

<rule> <!-- "See" entry in an Index -->
<match>
<gi>SEEIE
</rule>

<rule> <!-- Component of a SegmentedList -->
<match>
<gi>SEG
</rule>

<rule> <!-- List item in a SegmentedList -->
<match>
<gi>SEGLISTITEM
</rule>

<rule> <!-- List of sets of information -->
<match>
<gi>SEGMENTEDLIST
</rule>

<rule> <!-- Title that pertains to one Seg in each SegListItem -->
<match>
<gi>SEGTITLE
</rule>

<rule> <!-- Information about the publication series of which the containing
            Book is a part -->
<match>
<gi>SERIESINFO
</rule>

<rule> <!-- Numbers of all the volumes in a Series, for use in SeriesInfo -->
<match>
<gi>SERIESVOLNUMS
</rule>

<rule> <!-- Two or more Books -->
<match>
<gi>SET
</rule>

<rule> <!-- Index to a Set -->
<match>
<gi>SETINDEX
</rule>

<rule> <!-- Metainformation for a Set, in which it may appear -->
<match>
<gi>SETINFO
</rule>

<!-- SGMLTag in its variations -->

<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<attval>CLASS PARAMENTITY
<action>
<start>&lt;CODE>%</start>
<end>&lt;/CODE></end>
</rule>

<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<attval>CLASS GENENTITY
<action>
<start>&lt;CODE>&amp;amp;</start>
<end>;&lt;/CODE></end>
</rule>

<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<attval>CLASS STARTTAG
<action>
<start>&lt;CODE>&amp;lt;</start>
<end>>&lt;/CODE></end>
</rule>

<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<attval>CLASS ENDTAG
<action>
<start>&lt;CODE>&amp;lt;/</start>
<end>>&lt;/CODE></end>
</rule>

<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<attval>CLASS PI
<action>
<start>&lt;CODE>&amp;lt;?</start>
<end>>&lt;/CODE></end>
</rule>

<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<attval>CLASS SGMLCOMMENT
<action>
<start>&lt;CODE>&amp;lt;--</start>
<end>--&amp;lt;/CODE></end>
</rule>

<!-- catchall for remaining types of tags -->
<rule> <!-- Component of SGML markup -->
<match>
<gi>SGMLTAG
<action>
<start>&lt;CODE></start>
<end>&lt;/CODE></end>
</rule>

<rule> <!-- Brief version of of Affiliation, in which it may appear -->
<match>
<gi>SHORTAFFIL
</rule>

<rule> <!-- Segment of a book component that is isolated from the narrative
            flow of the main text, typically boxed and floating -->
<match>
<gi>SIDEBAR
<action>
<do>&r.admon;
</rule>

<rule> <!-- Paragraph that is only a text block, without included
            block-oriented elements -->
<match>
<gi>SIMPARA
</rule>

<rule> <!-- List of single words or short phrases -->
<match>
<gi>SIMPLELIST
</rule>

<rule> <!-- Section with no subdivisions -->
<match>
<gi>SIMPLESECT
</rule>

<rule> <!-- Formatting information for a spanned column in a TGroup -->
<match>
<gi>SPANSPEC
</rule>

<rule> <!-- State in an Address -->
<match>
<gi>STATE
</rule>

<rule> <!-- Part of a Procedure -->
<match>
<gi>STEP
<action>
<start>^&lt;LI></start>
<end>&lt;/LI>^</end>
</rule>

<rule> <!-- Street in an Address -->
<match>
<gi>STREET
</rule>

<rule> <!-- Field in a Structure -->
<match>
<gi>STRUCTFIELD
</rule>

<rule> <!-- Name of a Structure -->
<match>
<gi>STRUCTNAME
</rule>

<rule> <!-- Subscript -->
<match>
<gi>SUBSCRIPT
<action>
<start>&lt;SUB></start>
<end>&lt;/SUB></end>
</rule>

<rule> <!-- Wrapper for Steps within Steps -->
<match>
<gi>SUBSTEPS
<action>
<start>^&lt;OL>^</start>
<end>^&lt;/OL>^</end>
</rule>

<rule> <!-- Subtitle of a document -->
<match>
<gi>SUBTITLE
<action>
<start>^&lt;P>&lt;EM></start>
<end>&lt;/EM>&lt;/P>^</end>
</rule>

<rule> <!-- Superscript -->
<match>
<gi>SUPERSCRIPT
<action>
<start>&lt;SUP></start>
<end>&lt;/SUP></end>
</rule>

<rule> <!-- Family name -->
<match>
<gi>SURNAME
<action>
<start>^</start>
<end>^</end>
</rule>

<rule> <!-- Name that is replaced by a value before processing -->
<match>
<gi>SYMBOL
<action>
&m.tt;
</rule>

<rule> <!-- Part of CmdSynopsis -->
<match>
<gi>SYNOPFRAGMENT
</rule>

<rule> <!-- Part of a CmdSynopsis -->
<match>
<gi>SYNOPFRAGMENTREF
</rule>

<rule> <!-- Syntax of a command or function -->
<match>
<gi>SYNOPSIS
<action>
&m.blk;
</rule>

<rule> <!-- System-related term or item -->
<match>
<gi>SYSTEMITEM
<action>
&m.tt;
</rule>

<rule> <!-- Table in a document -->
<match>
<gi>TABLE
<attval>FRAME none
<action>
<start>${_action &r.blkps;t}&lt;P>${_action &r.anchor;t}&lt;STRONG>${_gi M} ${tabnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}&lt;/P>&lt;TABLE>^</start>
<end>^&lt;/TABLE>
${_action &r.blkpe;t}</end>
<incr>tabnum
</rule>

<rule> <!-- Table in a document -->
<match>
<gi>TABLE
<action>
<start>${_action &r.blkps;t}&lt;P>${_action &r.anchor;t}&lt;STRONG>${_gi M} ${tabnum}:&lt;/STRONG>
${_followrel child TITLE &r.pass;}&lt;/P>&lt;TABLE border="1">^</start>
<end>^&lt;/TABLE>
${_action &r.blkpe;t}</end>
<incr>tabnum
</rule>

<rule> <!-- Wrapper for the Rows of a Table or InformalTable -->
<match>
<gi>TBODY
</rule>

<rule> <!-- Hanging term attached to a ListItem within a VarListEntry in a
            VariableList -->
<match>
<gi>TERM
<action>
<start>^&lt;DT></start>
<end>&lt;/DT>^</end>
</rule>

<rule> <!-- Word or phrase that is to appear in the index under a Secondary
            entry -->
<match>
<gi>TERTIARY
</rule>

<rule> <!-- Third-level entry in an Index, not in the text -->
<match>
<gi>TERTIARYIE
</rule>

<rule> <!-- Footer row of a table -->
<match>
<gi>TFOOT
</rule>

<rule> <!-- Wrapper for part of a Table that contains an array along
with its
            formatting information -->
<match>
<gi>TGROUP
<relation>parent INFORMALTABLE
<action>
<start>^&lt;TABLE>^</start>
<end>^&lt;/TABLE>^</end>
</rule>

<rule> <!-- Wrapper for part of a Table that contains an array along with its
            formatting information -->
<match>
<gi>TGROUP
</rule>

<rule> <!-- Heading row of a table -->
<match>
<gi>THEAD
</rule>

<rule> <!-- Suggestion to the user, set off from the text -->
<match>
<gi>TIP
<action>
<do>&r.admon;
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<relation>parent BLOCKQUOTE
<action>
<start>^&lt;H4>&hlofont;</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<!-- Titles in the preface -->

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>PREFACE
<action>
<start>^&lt;H1>&lt;A NAME="pf-${pfnum}">&lt;/A>&hlofont;</start>
<end>&hlcfont;&lt;/H1>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT1
<relation>ancestor PREFACE
<action>
<start>^&lt;H2>&hlofont;</start>
<end>&hlcfont;&lt;/H2>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT2
<relation>ancestor PREFACE
<action>
<start>^&lt;H3>&hlofont;</start>
<end>&hlcfont;&lt;/H3>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT3
<relation>ancestor PREFACE
<action>
<start>^&lt;H4>&hlofont;</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT4
<relation>ancestor PREFACE
<action>
<start>^&lt;H4>&hlofont;</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT5
<relation>ancestor PREFACE
<action>
<start>^&lt;H4>&hlofont;</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<!-- Title in bookbiblio -->

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>BOOKBIBLIO
<action>
<ignore>all
</rule>

<!-- Titles in other sections -->

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>^REF.*
<action>
<start>^&lt;H3></start>
<end>^&lt;/H3></end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>PART
<action>
<start>^&lt;H1>&lt;A NAME="pt-${partnum}">&lt;/A>${_followrel parent PART &r.anchor;t}&hlofont;Part ${partnum}:&lt;BR>^</start>
<end>&hlcfont;&lt;/H1>
${_followrel parent PART &r.chtoc;t}^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>CHAPTER
<action>
<start>^&lt;H1>&lt;A NAME="ch-${chapnum}">&lt;/A>${_followrel parent CHAPTER &r.anchor;t}&hlofont;${chapnum}.&wspace;^</start>
<end>&hlcfont;&lt;/H1>
${_followrel parent CHAPTER &r.s1toc;t}^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>APPENDIX
<action>
<start>^&lt;H1>&lt;A NAME="ch-${chapnum}">&lt;/A>${_followrel parent APPENDIX &r.anchor;t}&hlofont;${chapnum}.&wspace;^</start>
<end>&hlcfont;&lt;/H1>
${_followrel parent APPENDIX &r.s1toc;t}^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT1
<action>
<start>^&lt;H2>&lt;A NAME="s1-${chapnum}-${sect1num}">&lt;/A>${_followrel parent SECT1 &r.anchor;t}&hlofont;${chapnum}.${sect1num}.&wspace;^</start>
<end>&hlcfont;&lt;/H2>
${_followrel parent SECT1 &r.s2toc;t}^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT2
<action>
<start>^&lt;H3>&lt;A NAME="s2-${chapnum}-${sect1num}-${sect2num}">&lt;/A>${_followrel parent SECT2 &r.anchor;t}&hlofont;${chapnum}.${sect1num}.${sect2num}.&wspace^</start>
<end>&hlcfont;&lt;/H3>
${_followrel parent SECT2 &r.s3toc;t}^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT3
<action>
<start>^&lt;H4>&lt;A NAME="s3-${chapnum}-${sect1num}-${sect2num}-${sect3num}">&lt;/A>${_followrel parent SECT3 &r.anchor;t}&hlofont;${chapnum}.${sect1num}.${sect2num}.${sect3num}.&wspace;^</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT4
<action>
<start>^&lt;H4>${_followrel parent SECT4 &r.anchor;t}&hlofont;</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>SECT5
<action>
<start>^&lt;H4>${_followrel parent SECT5 &r.anchor;t}&hlofont;</start>
<end>&hlcfont;&lt;/H4>^</end>
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>FIGURE|EXAMPLE|TABLE|CAUTION|IMPORTANT|NOTE|TIP|WARNING
<action>
<ignore>all
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
<context>GLOSSARY
<action>
<ignore>all
</rule>

<rule> <!-- Text of a heading or the title of a block-oriented element -->
<match>
<gi>TITLE
</rule>

<rule> <!-- Abbreviated title -->
<match>
<gi>TITLEABBREV
<action>
<ignore>all
</rule>

<rule> <!-- Table of contents -->
<match>
<gi>TOC
</rule>

<rule> <!-- Entry for back matter in a ToC -->
<match>
<gi>TOCBACK
</rule>

<rule> <!-- Entry in a ToC for a part of the body of a Book -->
<match>
<gi>TOCCHAP
</rule>

<rule> <!-- Entry in a ToC or its subelements -->
<match>
<gi>TOCENTRY
</rule>

<rule> <!-- Entry for introductory matter in a ToC -->
<match>
<gi>TOCFRONT
</rule>

<rule> <!-- Top-level entry within a ToCchap -->
<match>
<gi>TOCLEVEL1
</rule>

<rule> <!-- Second-level entry within a ToCchap -->
<match>
<gi>TOCLEVEL2
</rule>

<rule> <!-- Third-level entry within a ToCchap -->
<match>
<gi>TOCLEVEL3
</rule>

<rule> <!-- Fourth-level entry within a ToCchap -->
<match>
<gi>TOCLEVEL4
</rule>

<rule> <!-- Fifth-level entry within a ToCchap -->
<match>
<gi>TOCLEVEL5
</rule>

<rule> <!-- Entry in a ToC for a Part of a Book -->
<match>
<gi>TOCPART
</rule>

<rule> <!-- Unit of information in the context of lexical analysis -->
<match>
<gi>TOKEN
</rule>

<rule> <!-- Trademark -->
<match>
<gi>TRADEMARK
<action>
<end>&lt;SMALL>&lt;SUP>(TM)&lt;/SUP>&lt;/SMALL></end>
</rule>

<rule> <!-- Classification of a value -->
<match>
<gi>TYPE
<action>
&m.tt;
</rule>

<rule> <!-- Link that addresses its target by means of a Uniform Resource
            Locator -->
<match>
<gi>ULINK
<action>
<start>&lt;A HREF="${URL}"></start>
<end>&lt;/A></end>
</rule>

<rule> <!-- Data entered by the user -->
<match>
<gi>USERINPUT
<action>
<start>&lt;B>&lt;CODE></start>
<end>&lt;/CODE>&lt;/B></end>
</rule>

<rule> <!-- Empty element, part of FuncSynopsis, indicating that the Function in
            question has a variable number of arguments -->
<match>
<gi>VARARGS
</rule>

<rule> <!-- List in which each entry is composed of sets of one or more
            Terms with associated ListItems -->
<match>
<gi>VARIABLELIST
<action>
<start>${_action &r.blkps;t}
&lt;DL>^</start>
<end>^&lt;/DL>
${_action &r.blkpe;t}</end>
</rule>

<rule> <!-- Wrapper for Term and its associated ListItem in a
            VariableList -->
<match>
<gi>VARLISTENTRY
</rule>

<rule> <!-- Empty element, part of FuncSynopsis, that indicates that the Function in
            question takes no arguments -->
<match>
<gi>VOID
</rule>

<rule> <!-- Number of a Book in relation to Set, or of a journal, when Book
            is used to represent a journal by containing Articles -->
<match>
<gi>VOLUMENUM
</rule>

<rule> <!-- Admonition set off from the text -->
<match>
<gi>WARNING
<action>
<do>&r.admon;
</rule>

<rule> <!-- Word -->
<match>
<gi>WORDASWORD
<action>
<start>``</start>
<end>''</end>
</rule>

<rule> <!-- Cross reference link to another part of the document -->
<match>
<gi>XREF
<attval>REMAP .
<action>
<replace>&lt;EM>&lt;A HREF="#${LINKEND}">${REMAP}&lt;/A>&lt;/EM></replace>
</rule>

<rule> <!-- Cross reference link to another part of the document -->
<match>
<gi>XREF
<action>
<replace>&lt;EM>&lt;A HREF="#${LINKEND}">${_chasetogi TITLE &r.pass}&lt;/A>&lt;/EM></replace>
</rule>

<rule> <!-- Year of publication of a document -->
<match>
<gi>YEAR
<action>
<start>^</start>
<end>^</end>
</rule>

<rule> <!-- Absorb anything that mannages to get this
            far without a match so it does not accidentally
            match the rules below which are meant to be
            explicitly invoked from other rules. -->
<match>
<gi>*
</rule>

<!-- Just pass the content and child elements through. -->
<rule id="&r.pass;">
<match>
<gi>_pass-text
</rule>

<!-- Output an HTML anchor if the ID is set. -->
<rule id="&r.anchor;">
<match>
<attval>ID .
<action>
<replace>&lt;A NAME="${ID id}">&lt;/A></replace>
</rule>

<rule id="&r.ignore;">
<match>
<gi>_no_pass_text
<action>
<ignore>all
</rule>

<rule id="&r.admon;">
<match>
<gi>_admonition
<action>
<start>${_action &r.blkps;t}
&lt;CENTER>${_action &r.anchor;t}&lt;TABLE align="center" border="1" cellpadding="5" width="90%">
&lt;TR>
&lt;TD&c.admon;>&lt;P>&lt;STRONG>${_followrel child TITLE &r.pass; &r.prgi;}:&lt;/STRONG>&lt;/P></start>
<end>^&lt;/TD>&lt;/TR>&lt;/TABLE>&lt;/CENTER>
${_action &r.blkpe;t}</end>
</rule>

<rule id="&r.prgi;">
<match>
<gi>_prgi
<action>
<replace>${_gi M}</replace>
</rule>

<!-- Generate tables of contents.  Each r.??toc scans the subtree for
     sectional elements of the specified level, generating a list of links. -->

<rule id="&r.pttoc;">
<match>
<relation>descendant PART
<action>
<replace>^&lt;DL>${_set anchorinhibit 1}
${_find gi PART &r.pttoci;}
${_set anchorinhibit 0}&lt;/DL>${_set partnum 1}^</replace>
</rule>

<rule id="&r.pttoci;">
<match>
<gi>_pttoc
<action>
<replace>&lt;DD>Part ${partnum}.&wspace;&lt;EM>&lt;A HREF="#pt-${partnum}">${_followrel descendant TITLE &r.pass;}&lt;/A>&lt;/EM>&lt;/DD>^</replace>
<incr>partnum
</rule>

<rule id="&r.chtoc;">
<match>
<relation>descendant CHAPTER
<action>
<replace>^&lt;DL>${_set anchorinhibit 1}
${_set tmpchapnum ${pfpnum}}${_find gi PREFACE &r.pftoci;}${_set pfnum ${tmpchapnum}}
${_set tmpchapnum ${chapnum}}${_find gi CHAPTER &r.chtoci;}${_set chapnum ${appnum}}${_find gi APPENDIX &r.chtoci;}
${_set anchorinhibit 0}&lt;/DL>^</replace>
<set>chapnum ${tmpchapnum}
</rule>

<rule id="&r.chtoci;">
<match>
<gi>_chtoc
<action>
<replace>&lt;DD>${chapnum}.&wspace;&lt;EM>&lt;A HREF="#ch-${chapnum}">${_followrel descendant TITLE &r.pass;}&lt;/A>&lt;/EM>&lt;/DD>^</replace>
<incr>chapnum
</rule>

<rule id="&r.pftoci;">
<match>
<gi>_aptoc
<action>
<replace>&lt;DD>&lt;EM>&lt;A HREF="#pf-${pfnum}">${_followrel descendant TITLE &r.pass;}&lt;/A>&lt;/EM>&lt;/DD>^</replace>
<incr>pfnum
</rule>

<rule id="&r.s1toc;">
<match>
<relation>descendant SECT1
<action>
<replace>^${_set anchorinhibit 1}&lt;DL>
${_find gi SECT1 &r.s1toci;}
${_set anchorinhibit 0}&lt;/DL>${_set sect1num 1}^</replace>
</rule>

<rule id="&r.s1toci;">
<match>
<gi>_s1toc
<action>
<replace>&lt;DD>${chapnum}.${sect1num}.&wspace;&lt;EM>&lt;A HREF="#s1-${chapnum}-${sect1num}">${_followrel descendant TITLE &r.pass;}&lt;/A>&lt;/EM>&lt;/DD>^</replace>
<incr>sect1num
</rule>

<rule id="&r.s2toc;">
<match>
<relation>descendant SECT2
<action>
<replace>^${_set anchorinhibit 1}&lt;DL>
${_find gi SECT2 &r.s2toci;}
${_set anchorinhibit 0}&lt;/DL>${_set sect2num 1}^</replace>
</rule>

<rule id="&r.s2toci;">
<match>
<gi>_s2toc
<action>
<replace>&lt;DD>${chapnum}.${sect1num}.${sect2num}.&wspace;&lt;EM>&lt;A HREF="#s2-${chapnum}-${sect1num}-${sect2num}">${_followrel descendant TITLE &r.pass;}&lt;/A>&lt;/EM>&lt;/DD>^</replace>
<incr>sect2num
</rule>

<rule id="&r.s3toc;">
<match>
<relation>descendant SECT3
<action>
<replace>^${_set anchorinhibit 1}&lt;DL>
${_find gi SECT3 &r.s3toci;}
${_set anchorinhibit 0}&lt;/DL>${_set sect3num 1}^</replace>
</rule>

<rule id="&r.s3toci;">
<match>
<gi>_s3toc
<action>
<replace>&lt;DD>${chapnum}.${sect1num}.${sect2num}.${sect3num}.&wspace;&lt;EM>&lt;A HREF="#s3-${chapnum}-${sect1num}-${sect2num}-${sect3num}">${_followrel descendant TITLE &r.pass;}&lt;/A>&lt;/EM>&lt;/DD>^</replace>
<incr>sect3num
</rule>

<rule id="&r.fnote;">
<match>
<relation>descendant FOOTNOTE
<action>
<replace>&lt;H1>&hlofont;Notes&hlcfont;&lt;/H1>
&lt;TABLE width="100%">
${_find top gi FOOTNOTE &r.fnotei;}
&lt;/TABLE></replace>
</rule>

<rule id="&r.fnotei;">
<match>
<gi>_fnote
<action>
<start>^&lt;TR>&lt;TD VALIGN="TOP">&lt;B>&lt;A NAME="fn-${fnotenum}">&lt/A>&lt;A HREF="#rfn-${fnotenum}">${fnotenum}.&lt/A>&lt;/B>&lt;/TD>
&lt;TD VALIGN="TOP"></start>
<end>&lt;/TD>&lt;TR></end>
<incr>fnotenum
</rule>

<!-- These two are for handling the case of a block element that
     can occur in a docbook <para>, but not in an html <p>.  Call
     the first in the <start> of such an element, and the second
     in the <end> which will close and re-open the html <p>. -->
     
<rule id="&r.blkps;">
<match>
<relation>parent PARA
<action>
<replace>&lt;/P>^</replace>
</rule>

<rule id="&r.blkpe;">
<match>
<relation>parent PARA
<action>
<replace>^&lt;P></replace>
</rule>

<!-- Simply output a hyphen -->
<rule id="&r.hyphen;">
<match>
<gi>_hyphen
<action>
<replace>-</replace>
</rule>

<!-- Force a linebreak -->
<rule id="&r.nl;">
<match>
<gi>_hyphen
<action>
<replace>^</replace>
</rule>

</transpec>
