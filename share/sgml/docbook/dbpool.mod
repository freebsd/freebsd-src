<!-- ...................................................................... -->
<!-- DocBook information pool module V2.4.1 ............................... -->
<!-- File dbpool.mod ...................................................... -->

<!-- Copyright 1992, 1993, 1994, 1995 HaL Computer Systems, Inc.,
     O'Reilly & Associates, Inc., and ArborText, Inc.

     Permission to use, copy, modify and distribute the DocBook DTD and
     its accompanying documentation for any purpose and without fee is
     hereby granted in perpetuity, provided that the above copyright
     notice and this paragraph appear in all copies.  The copyright
     holders make no representation about the suitability of the DTD for
     any purpose.  It is provided "as is" without expressed or implied
     warranty.

     If you modify the DocBook DTD in any way, except for declaring and
     referencing additional sets of general entities and declaring
     additional notations, label your DTD as a variant of DocBook.  See
     the maintenance documentation for more information.

     Please direct all questions, bug reports, or suggestions for
     changes to the davenport@online.ora.com mailing list or to one of
     the maintainers:

     o Terry Allen, O'Reilly & Associates, Inc.
       101 Morris St., Sebastopol, CA 95472
       <terry@ora.com>

     o Eve Maler, ArborText, Inc.
       105 Lexington St., Burlington, MA 01803
       <elm@arbortext.com>
-->

<!-- ...................................................................... -->

<!-- This module contains the definitions for the objects, inline
     elements, and so on that are available to be used as the main
     content of DocBook documents.  Some elements are useful for general
     publishing, and others are useful specifically for computer
     documentation.

     This module has the following dependencies on other modules:

     o It assumes that a %notation.class; entity is defined by the
       driver file or other high-level module.  This entity is
       referenced in the NOTATION attributes for the graphic-related and
       ModeSpec elements.

     o It assumes that an appropriately parameterized table module is
       available for use with the table-related elements.

     In DTD driver files referring to this module, please use an entity
     declaration that uses the public identifier shown below:

     <!ENTITY % dbpool PUBLIC
     "-//Davenport//ELEMENTS DocBook Information Pool V2.4.1//EN">
     %dbpool;

     See the documentation for detailed information on the parameter
     entity and module scheme used in DocBook, customizing DocBook and
     planning for interchange, and changes made since the last release
     of DocBook.
-->

<!-- ...................................................................... -->
<!-- Entities for module inclusions ....................................... -->

<!ENTITY % dbpool.redecl.module		"IGNORE">

<!ENTITY % abbrev.module		"INCLUDE">
<!ENTITY % abstract.module		"INCLUDE">
<!ENTITY % accel.module			"INCLUDE">
<!ENTITY % ackno.module			"INCLUDE">
<!ENTITY % acronym.module		"INCLUDE">
<!ENTITY % action.module		"INCLUDE">
<!ENTITY % address.module		"INCLUDE">
<!ENTITY % address.content.module	"INCLUDE">
<!ENTITY % admon.module			"INCLUDE">
<!ENTITY % affiliation.module		"INCLUDE">
<!ENTITY % affiliation.content.module	"INCLUDE">
<!ENTITY % anchor.module		"INCLUDE">
<!ENTITY % application.module		"INCLUDE">
<!ENTITY % area.module			"INCLUDE">
<!ENTITY % areaset.module		"INCLUDE">
<!ENTITY % areaspec.module		"INCLUDE">
<!ENTITY % areaspec.content.module	"INCLUDE">
<!ENTITY % arg.module			"INCLUDE">
<!ENTITY % artheader.module		"INCLUDE">
<!ENTITY % artpagenums.module		"INCLUDE">
<!ENTITY % attribution.module		"INCLUDE">
<!ENTITY % author.module		"INCLUDE">
<!ENTITY % authorblurb.module		"INCLUDE">
<!ENTITY % authorgroup.module		"INCLUDE">
<!ENTITY % authorgroup.content.module	"INCLUDE">
<!ENTITY % authorinitials.module	"INCLUDE">
<!ENTITY % beginpage.module		"INCLUDE">
<!ENTITY % biblioentry.content.module	"INCLUDE">
<!ENTITY % biblioentry.module		"INCLUDE">
<!ENTITY % bibliomisc.module		"INCLUDE">
<!ENTITY % blockquote.module		"INCLUDE">
<!ENTITY % bookbiblio.module		"INCLUDE">
<!ENTITY % bridgehead.module		"INCLUDE">
<!ENTITY % callout.module		"INCLUDE">
<!ENTITY % calloutlist.module		"INCLUDE">
<!ENTITY % calloutlist.content.module	"INCLUDE">
<!--       caution.module		use admon.module-->
<!ENTITY % citation.module		"INCLUDE">
<!ENTITY % citerefentry.content.module	"INCLUDE">
<!ENTITY % citerefentry.module		"INCLUDE">
<!ENTITY % citetitle.module		"INCLUDE">
<!ENTITY % city.module			"INCLUDE">
<!ENTITY % classname.module		"INCLUDE">
<!ENTITY % cmdsynopsis.content.module	"INCLUDE">
<!ENTITY % cmdsynopsis.module		"INCLUDE">
<!ENTITY % collab.module		"INCLUDE">
<!ENTITY % co.module			"INCLUDE">
<!ENTITY % collab.content.module	"INCLUDE">
<!ENTITY % collabname.module		"INCLUDE">
<!ENTITY % command.module		"INCLUDE">
<!ENTITY % comment.module		"INCLUDE">
<!ENTITY % computeroutput.module	"INCLUDE">
<!ENTITY % confdates.module		"INCLUDE">
<!ENTITY % confgroup.module		"INCLUDE">
<!ENTITY % confgroup.content.module	"INCLUDE">
<!ENTITY % confnum.module		"INCLUDE">
<!ENTITY % confsponsor.module		"INCLUDE">
<!ENTITY % conftitle.module		"INCLUDE">
<!ENTITY % contractnum.module		"INCLUDE">
<!ENTITY % contractsponsor.module	"INCLUDE">
<!ENTITY % contrib.module		"INCLUDE">
<!ENTITY % copyright.module		"INCLUDE">
<!ENTITY % copyright.content.module	"INCLUDE">
<!ENTITY % corpauthor.module		"INCLUDE">
<!ENTITY % corpname.module		"INCLUDE">
<!ENTITY % country.module		"INCLUDE">
<!ENTITY % database.module		"INCLUDE">
<!ENTITY % date.module			"INCLUDE">
<!ENTITY % docinfo.content.module	"INCLUDE">
<!ENTITY % edition.module		"INCLUDE">
<!ENTITY % editor.module		"INCLUDE">
<!ENTITY % email.module			"INCLUDE">
<!ENTITY % emphasis.module		"INCLUDE">
<!ENTITY % epigraph.module		"INCLUDE">
<!ENTITY % equation.module		"INCLUDE">
<!ENTITY % errorname.module		"INCLUDE">
<!ENTITY % errortype.module		"INCLUDE">
<!ENTITY % example.module		"INCLUDE">
<!ENTITY % fax.module			"INCLUDE">
<!ENTITY % figure.module		"INCLUDE">
<!ENTITY % filename.module		"INCLUDE">
<!ENTITY % firstname.module		"INCLUDE">
<!ENTITY % firstterm.module		"INCLUDE">
<!ENTITY % footnote.module		"INCLUDE">
<!ENTITY % footnoteref.module		"INCLUDE">
<!ENTITY % foreignphrase.module		"INCLUDE">
<!ENTITY % formalpara.module		"INCLUDE">
<!ENTITY % funcdef.module		"INCLUDE">
<!ENTITY % funcparams.module		"INCLUDE">
<!ENTITY % funcprototype.module		"INCLUDE">
<!ENTITY % funcsynopsis.content.module	"INCLUDE">
<!ENTITY % funcsynopsis.module		"INCLUDE">
<!ENTITY % funcsynopsisinfo.module	"INCLUDE">
<!ENTITY % function.module		"INCLUDE">
<!ENTITY % glossdef.module		"INCLUDE">
<!ENTITY % glossentry.content.module	"INCLUDE">
<!ENTITY % glossentry.module		"INCLUDE">
<!ENTITY % glosslist.module		"INCLUDE">
<!ENTITY % glosssee.module		"INCLUDE">
<!ENTITY % glossseealso.module		"INCLUDE">
<!ENTITY % glossterm.module		"INCLUDE">
<!ENTITY % graphic.module		"INCLUDE">
<!ENTITY % graphicco.module		"INCLUDE">
<!ENTITY % group.module			"INCLUDE">
<!ENTITY % guibutton.module		"INCLUDE">
<!ENTITY % guiicon.module		"INCLUDE">
<!ENTITY % guilabel.module		"INCLUDE">
<!ENTITY % guimenu.module		"INCLUDE">
<!ENTITY % guimenuitem.module		"INCLUDE">
<!ENTITY % guisubmenu.module		"INCLUDE">
<!ENTITY % hardware.module		"INCLUDE">
<!ENTITY % highlights.module		"INCLUDE">
<!ENTITY % holder.module		"INCLUDE">
<!ENTITY % honorific.module		"INCLUDE">
<!ENTITY % indexterm.content.module	"INCLUDE">
<!ENTITY % indexterm.module		"INCLUDE">
<!ENTITY % informalequation.module	"INCLUDE">
<!ENTITY % informalexample.module	"INCLUDE">
<!ENTITY % informaltable.module		"INCLUDE">
<!ENTITY % inlineequation.module	"INCLUDE">
<!ENTITY % inlinegraphic.module		"INCLUDE">
<!ENTITY % interface.module		"INCLUDE">
<!ENTITY % interfacedefinition.module	"INCLUDE">
<!ENTITY % invpartnumber.module		"INCLUDE">
<!ENTITY % isbn.module			"INCLUDE">
<!ENTITY % issn.module			"INCLUDE">
<!ENTITY % issuenum.module		"INCLUDE">
<!ENTITY % itemizedlist.module		"INCLUDE">
<!ENTITY % jobtitle.module		"INCLUDE">
<!ENTITY % keycap.module		"INCLUDE">
<!ENTITY % keycode.module		"INCLUDE">
<!ENTITY % keycombo.module		"INCLUDE">
<!ENTITY % keysym.module		"INCLUDE">
<!ENTITY % legalnotice.module		"INCLUDE">
<!ENTITY % lineage.module		"INCLUDE">
<!ENTITY % lineannotation.module	"INCLUDE">
<!ENTITY % link.module			"INCLUDE">
<!ENTITY % listitem.module		"INCLUDE">
<!ENTITY % literal.module		"INCLUDE">
<!ENTITY % literallayout.module		"INCLUDE">
<!ENTITY % manvolnum.module		"INCLUDE">
<!ENTITY % markup.module		"INCLUDE">
<!ENTITY % medialabel.module		"INCLUDE">
<!ENTITY % member.module		"INCLUDE">
<!ENTITY % menuchoice.content.module	"INCLUDE">
<!ENTITY % menuchoice.module		"INCLUDE">
<!ENTITY % modespec.module		"INCLUDE">
<!ENTITY % mousebutton.module		"INCLUDE">
<!ENTITY % msg.module			"INCLUDE">
<!ENTITY % msgaud.module		"INCLUDE">
<!ENTITY % msgentry.module		"INCLUDE">
<!ENTITY % msgexplan.module		"INCLUDE">
<!ENTITY % msginfo.module		"INCLUDE">
<!ENTITY % msglevel.module		"INCLUDE">
<!ENTITY % msgmain.module		"INCLUDE">
<!ENTITY % msgorig.module		"INCLUDE">
<!ENTITY % msgrel.module		"INCLUDE">
<!ENTITY % msgset.content.module	"INCLUDE">
<!ENTITY % msgset.module		"INCLUDE">
<!ENTITY % msgsub.module		"INCLUDE">
<!ENTITY % msgtext.module		"INCLUDE">
<!--       note.module			use admon.module-->
<!ENTITY % olink.module			"INCLUDE">
<!ENTITY % option.module		"INCLUDE">
<!ENTITY % optional.module		"INCLUDE">
<!ENTITY % orderedlist.module		"INCLUDE">
<!ENTITY % orgdiv.module		"INCLUDE">
<!ENTITY % orgname.module		"INCLUDE">
<!ENTITY % otheraddr.module		"INCLUDE">
<!ENTITY % othercredit.module		"INCLUDE">
<!ENTITY % othername.module		"INCLUDE">
<!ENTITY % pagenums.module		"INCLUDE">
<!ENTITY % para.module			"INCLUDE">
<!ENTITY % paramdef.module		"INCLUDE">
<!ENTITY % parameter.module		"INCLUDE">
<!ENTITY % person.ident.module		"INCLUDE">
<!ENTITY % phone.module			"INCLUDE">
<!ENTITY % phrase.module		"INCLUDE">
<!ENTITY % pob.module			"INCLUDE">
<!ENTITY % postcode.module		"INCLUDE">
<!--       primary.module		use primsecter.module-->
<!ENTITY % primsecter.module		"INCLUDE">
<!ENTITY % printhistory.module		"INCLUDE">
<!ENTITY % procedure.content.module	"INCLUDE">
<!ENTITY % procedure.module		"INCLUDE">
<!ENTITY % productname.module		"INCLUDE">
<!ENTITY % productnumber.module		"INCLUDE">
<!ENTITY % programlisting.module	"INCLUDE">
<!ENTITY % programlistingco.module	"INCLUDE">
<!ENTITY % property.module		"INCLUDE">
<!ENTITY % pubdate.module		"INCLUDE">
<!ENTITY % publisher.module		"INCLUDE">
<!ENTITY % publisher.content.module	"INCLUDE">
<!ENTITY % publishername.module		"INCLUDE">
<!ENTITY % pubsnumber.module		"INCLUDE">
<!ENTITY % quote.module			"INCLUDE">
<!ENTITY % refentrytitle.module		"INCLUDE">
<!ENTITY % releaseinfo.module		"INCLUDE">
<!ENTITY % replaceable.module		"INCLUDE">
<!ENTITY % returnvalue.module		"INCLUDE">
<!ENTITY % revhistory.module		"INCLUDE">
<!ENTITY % revhistory.content.module	"INCLUDE">
<!ENTITY % revision.module		"INCLUDE">
<!ENTITY % revnumber.module		"INCLUDE">
<!ENTITY % revremark.module		"INCLUDE">
<!ENTITY % sbr.module			"INCLUDE">
<!ENTITY % screen.module		"INCLUDE">
<!ENTITY % screenco.module		"INCLUDE">
<!ENTITY % screeninfo.module		"INCLUDE">
<!ENTITY % screenshot.content.module	"INCLUDE">
<!ENTITY % screenshot.module		"INCLUDE">
<!--       secondary.module		use primsecter.module-->
<!--       see.module			use seeseealso.module-->
<!--       seealso.module		use seeseealso.module-->
<!ENTITY % seeseealso.module		"INCLUDE">
<!ENTITY % seg.module			"INCLUDE">
<!ENTITY % seglistitem.module		"INCLUDE">
<!ENTITY % segmentedlist.content.module	"INCLUDE">
<!ENTITY % segmentedlist.module		"INCLUDE">
<!ENTITY % segtitle.module		"INCLUDE">
<!ENTITY % seriesinfo.module		"INCLUDE">
<!ENTITY % seriesvolnums.module		"INCLUDE">
<!ENTITY % sgmltag.module		"INCLUDE">
<!ENTITY % shortaffil.module		"INCLUDE">
<!ENTITY % shortcut.module		"INCLUDE">
<!ENTITY % sidebar.module		"INCLUDE">
<!ENTITY % simpara.module		"INCLUDE">
<!ENTITY % simplelist.content.module	"INCLUDE">
<!ENTITY % simplelist.module		"INCLUDE">
<!ENTITY % ssscript.module		"INCLUDE">
<!ENTITY % state.module			"INCLUDE">
<!ENTITY % step.module			"INCLUDE">
<!ENTITY % street.module		"INCLUDE">
<!ENTITY % structfield.module		"INCLUDE">
<!ENTITY % structname.module		"INCLUDE">
<!ENTITY % substeps.module		"INCLUDE">
<!--       subscript.module		use ssscript.module-->
<!ENTITY % subtitle.module		"INCLUDE">
<!--       superscript.module		use ssscript.module-->
<!ENTITY % surname.module		"INCLUDE">
<!ENTITY % symbol.module		"INCLUDE">
<!ENTITY % synopfragment.module		"INCLUDE">
<!ENTITY % synopfragmentref.module	"INCLUDE">
<!ENTITY % synopsis.module		"INCLUDE">
<!ENTITY % systemitem.module		"INCLUDE">
<!ENTITY % table.module			"INCLUDE">
<!ENTITY % term.module			"INCLUDE">
<!--       tertiary.module		use primsecter.module-->
<!ENTITY % title.module			"INCLUDE">
<!ENTITY % titleabbrev.module		"INCLUDE">
<!ENTITY % token.module			"INCLUDE">
<!ENTITY % trademark.module		"INCLUDE">
<!ENTITY % type.module			"INCLUDE">
<!ENTITY % ulink.module			"INCLUDE">
<!ENTITY % userinput.module		"INCLUDE">
<!ENTITY % varargs.module		"INCLUDE">
<!ENTITY % variablelist.content.module	"INCLUDE">
<!ENTITY % variablelist.module		"INCLUDE">
<!ENTITY % varlistentry.module		"INCLUDE">
<!ENTITY % void.module			"INCLUDE">
<!ENTITY % volumenum.module		"INCLUDE">
<!--       warning.module		use admon.module-->
<!ENTITY % wordasword.module		"INCLUDE">
<!ENTITY % xref.module			"INCLUDE">
<!ENTITY % year.module			"INCLUDE">

<!-- ...................................................................... -->
<!-- Entities for element classes and mixtures ............................ -->

<!-- Object-level classes ................................................. -->

<!ENTITY % local.list.class "">
<!ENTITY % list.class
		"CalloutList|GlossList|ItemizedList|OrderedList|SegmentedList
		|SimpleList|VariableList %local.list.class;">

<!ENTITY % local.admon.class "">
<!ENTITY % admon.class
		"Caution|Important|Note|Tip|Warning %local.admon.class;">

<!ENTITY % local.linespecific.class "">
<!ENTITY % linespecific.class
		"LiteralLayout|ProgramListing|ProgramListingCO|Screen
		|ScreenCO|ScreenShot %local.linespecific.class;">

<!ENTITY % local.synop.class "">
<!ENTITY % synop.class
		"Synopsis|CmdSynopsis|FuncSynopsis %local.synop.class;">

<!ENTITY % local.para.class "">
<!ENTITY % para.class
		"FormalPara|Para|SimPara %local.para.class;">

<!ENTITY % local.informal.class "">
<!ENTITY % informal.class
		"Address|BlockQuote|Graphic|GraphicCO|InformalEquation
		|InformalExample|InformalTable %local.informal.class;">

<!ENTITY % local.formal.class "">
<!ENTITY % formal.class
		"Equation|Example|Figure|Table %local.formal.class;">

<!ENTITY % local.compound.class "">
<!ENTITY % compound.class
		"MsgSet|Procedure|Sidebar %local.compound.class;">

<!ENTITY % local.genobj.class "">
<!ENTITY % genobj.class
		"Anchor|BridgeHead|Comment|Highlights
		%local.genobj.class;">

<!ENTITY % local.descobj.class "">
<!ENTITY % descobj.class
		"Abstract|AuthorBlurb|Epigraph
		%local.descobj.class;">

<!-- Character-level classes .............................................. -->

<!ENTITY % local.ndxterm.class "">
<!ENTITY % ndxterm.class
		"IndexTerm %local.ndxterm.class;">

<!ENTITY % local.xref.char.class "">
<!ENTITY % xref.char.class
		"FootnoteRef|XRef %local.xref.char.class;">

<!ENTITY % local.word.char.class "">
<!ENTITY % word.char.class
		"Abbrev|Acronym|Citation|CiteTitle|CiteRefEntry|Emphasis
		|FirstTerm|ForeignPhrase|GlossTerm|Footnote|Phrase
		|Quote|Trademark|WordAsWord %local.word.char.class;">

<!ENTITY % local.link.char.class "">
<!ENTITY % link.char.class
		"Link|OLink|ULink %local.link.char.class;">

<!ENTITY % local.cptr.char.class "">
<!ENTITY % cptr.char.class
		"Action|Application|ClassName|Command|ComputerOutput
		|Database|Email|ErrorName|ErrorType|Filename|Function
		|GUIButton|GUIIcon|GUILabel|GUIMenu|GUIMenuItem|GUISubmenu
		|Hardware|Interface|InterfaceDefinition|KeyCap|KeyCode
		|KeyCombo|KeySym|Literal|Markup|MediaLabel|MenuChoice
		|MouseButton|MsgText|Option|Optional|Parameter|Property
		|Replaceable|ReturnValue|SGMLTag|StructField|StructName
		|Symbol|SystemItem|Token|Type|UserInput
		%local.cptr.char.class;">

<!ENTITY % local.base.char.class "">
<!ENTITY % base.char.class
		"Anchor %local.base.char.class;">

<!ENTITY % local.docinfo.char.class "">
<!ENTITY % docinfo.char.class
		"Author|AuthorInitials|CorpAuthor|ModeSpec|OtherCredit
		|ProductName|ProductNumber|RevHistory
		%local.docinfo.char.class;">

<!ENTITY % local.other.char.class "">
<!ENTITY % other.char.class
		"Comment|Subscript|Superscript %local.other.char.class;">

<!ENTITY % local.inlineobj.char.class "">
<!ENTITY % inlineobj.char.class
		"InlineGraphic|InlineEquation %local.inlineobj.char.class;">

<!-- Redeclaration placeholder ............................................ -->

<!-- For redeclaring entities that are declared after this point while
     retaining their references to the entities that are declared before
     this point -->

<![ %dbpool.redecl.module; [
%rdbpool;
<!--end of dbpool.redecl.module-->]]>

<!-- Object-level mixtures ................................................ -->

<!--
                      list admn line synp para infm form cmpd gen  desc
Component mixture       X    X    X    X    X    X    X    X    X    X
Sidebar mixture         X    X    X    X    X    X    X    a    X
Footnote mixture        X         X    X    X    X
Example mixture         X         X    X    X    X
Highlights mixture      X    X              X
Paragraph mixture       X         X    X         X
Admonition mixture      X         X    X    X    X    X    b    c
Figure mixture                    X    X         X
Table entry mixture     X    X    X         X    d
Glossary def mixture    X         X    X    X    X         e
Legal notice mixture    X    X    X         X    f

a. Just Procedure; not Sidebar itself or MsgSet.
b. No MsgSet.
c. No Highlights.
d. Just Graphic; no other informal objects.
e. No Anchor, BridgeHead, or Highlights.
f. Just BlockQuote; no other informal objects.
-->

<!ENTITY % local.component.mix "">
<!ENTITY % component.mix
		"%list.class;		|%admon.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|%formal.class;		|%compound.class;
		|%genobj.class;		|%descobj.class;
		%local.component.mix;">

<!ENTITY % local.sidebar.mix "">
<!ENTITY % sidebar.mix
		"%list.class;		|%admon.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|%formal.class;		|Procedure
		|%genobj.class;
		%local.sidebar.mix;">

<!ENTITY % local.footnote.mix "">
<!ENTITY % footnote.mix
		"%list.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		%local.footnote.mix;">

<!ENTITY % local.example.mix "">
<!ENTITY % example.mix
		"%list.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		%local.example.mix;">

<!ENTITY % local.highlights.mix "">
<!ENTITY % highlights.mix
		"%list.class;		|%admon.class;
		|%para.class;
		%local.highlights.mix;">

<!-- %synop.class; is already included in para.char.mix because synopses
     used inside paragraph-like contexts are "inline" synopses -->
<!-- %formal.class; is explicitly excluded from many contexts in which
     paragraphs are used -->
<!ENTITY % local.para.mix "">
<!ENTITY % para.mix
		"%list.class;           |%admon.class;
		|%linespecific.class;
					|%informal.class;
		|%formal.class;
		%local.para.mix;">

<!ENTITY % local.admon.mix "">
<!ENTITY % admon.mix
		"%list.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|%formal.class;		|Procedure|Sidebar
		|Anchor|BridgeHead|Comment
		%local.admon.mix;">

<!ENTITY % local.figure.mix "">
<!ENTITY % figure.mix
		"%linespecific.class;	|%synop.class;
					|%informal.class;
		%local.figure.mix;">

<!ENTITY % local.tabentry.mix "">
<!ENTITY % tabentry.mix
		"%list.class;		|%admon.class;
		|%linespecific.class;
		|%para.class;		|Graphic
		%local.tabentry.mix;">

<!ENTITY % local.glossdef.mix "">
<!ENTITY % glossdef.mix
		"%list.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|%formal.class;
		|Comment
		%local.glossdef.mix;">

<!ENTITY % local.legalnotice.mix "">
<!ENTITY % legalnotice.mix
		"%list.class;		|%admon.class;
		|%linespecific.class;
		|%para.class;		|BlockQuote
		%local.legalnotice.mix;">

<!-- Character-level mixtures ............................................. -->

<!ENTITY % local.ubiq.mix "">
<!ENTITY % ubiq.mix
		"%ndxterm.class;|BeginPage %local.ubiq.mix;">

<!--
                    #PCD xref word link cptr base dnfo othr inob (synop)
para.char.mix         X    X    X    X    X    X    X    X    X     X
title.char.mix        X    X    X    X    X    X    X    X    X
ndxterm.char.mix      X    X    X    X    X    X    X    X    a
cptr.char.mix         X              X    X    X         X    a
smallcptr.char.mix    X                   b                   a
word.char.mix         X         c    X         X         X    a
docinfo.char.mix      X         c         b              X    a

a. Just InlineGraphic; no InlineEquation.
b. Just Replaceable; no other computer terms.
c. Just Emphasis and Trademark; no other word elements.
-->

<!-- Note that synop.class is not usually used for *.char.mixes,
     but is used here because synopses used inside paragraph
     contexts are "inline" synopses -->
<!ENTITY % local.para.char.mix "">
<!ENTITY % para.char.mix
		"#PCDATA
		|%xref.char.class;	|%word.char.class;
		|%link.char.class;	|%cptr.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;	|%inlineobj.char.class;
		|%synop.class;
		%local.para.char.mix;">

<!ENTITY % local.title.char.mix "">
<!ENTITY % title.char.mix
		"#PCDATA
		|%xref.char.class;	|%word.char.class;
		|%link.char.class;	|%cptr.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;	|%inlineobj.char.class;
		%local.title.char.mix;">

<!ENTITY % local.ndxterm.char.mix "">
<!ENTITY % ndxterm.char.mix
		"#PCDATA
		|%xref.char.class;	|%word.char.class;
		|%link.char.class;	|%cptr.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;	|InlineGraphic
		%local.ndxterm.char.mix;">

<!--FUTURE USE (V4.0):
......................
All elements containing cptr.char.mix will be examined and the content
models of most of them reduced (to remove, e.g., themselves and most
of the other computer terms); cptr.char.mix itself may be reduced to
help accomplish this.
......................
-->

<!ENTITY % local.cptr.char.mix "">
<!ENTITY % cptr.char.mix
		"#PCDATA
		|%link.char.class;	|%cptr.char.class;
		|%base.char.class;
		|%other.char.class;	|InlineGraphic
		%local.cptr.char.mix;">

<!ENTITY % local.smallcptr.char.mix "">
<!ENTITY % smallcptr.char.mix
		"#PCDATA
					|Replaceable
					|InlineGraphic
		%local.smallcptr.char.mix;">

<!ENTITY % local.word.char.mix "">
<!ENTITY % word.char.mix
		"#PCDATA
					|Emphasis|Trademark
		|%link.char.class;
		|%base.char.class;
		|%other.char.class;	|InlineGraphic
		%local.word.char.mix;">

<!ENTITY % local.docinfo.char.mix "">
<!ENTITY % docinfo.char.mix
		"#PCDATA
					|Emphasis|Trademark
					|Replaceable
		|%other.char.class;	|InlineGraphic
		%local.docinfo.char.mix;">
<!--ENTITY % person.ident.mix (see Document Information section, below)-->

<!-- ...................................................................... -->
<!-- Entities for content models .......................................... -->

<!ENTITY % formalobject.title.content "Title, TitleAbbrev?">

<!ENTITY % equation.content "(Graphic+)">

<!ENTITY % inlineequation.content "(Graphic+)">

<!ENTITY % programlisting.content "CO | LineAnnotation | %para.char.mix;">

<!ENTITY % screen.content "CO | LineAnnotation | %para.char.mix;">

<!-- ...................................................................... -->
<!-- Entities for attributes and attribute components ..................... -->

<!-- Effectivity attributes ............................................... -->

<!ENTITY % os.attrib
	--OS: operating system to which element applies; no default--
	"OS		CDATA		#IMPLIED">

<!ENTITY % arch.attrib
	--Arch: computer or chip architecture to which element applies; no 
	default--
	"Arch		CDATA		#IMPLIED">

<!ENTITY % vendor.attrib
	--Vendor: computer vendor to which element applies; no default--
	"Vendor		CDATA		#IMPLIED">

<!ENTITY % userlevel.attrib
	--UserLevel: level of user experience to which element applies; no 
	default--
	"UserLevel	CDATA		#IMPLIED">

<!ENTITY % revision.attrib
	--Revision: editorial revision to which element belongs; no default--
	"Revision	CDATA		#IMPLIED">

<!ENTITY % local.effectivity.attrib "">
<!ENTITY % effectivity.attrib
	"%os.attrib;
	%arch.attrib;
	%vendor.attrib;
	%userlevel.attrib;
	%revision.attrib;
	%local.effectivity.attrib;"
>

<!-- Common attributes .................................................... -->

<!ENTITY % id.attrib
	--Id: unique identifier of element; no default--
	"Id		ID		#IMPLIED">

<!ENTITY % idreq.attrib
	--Id: unique identifier of element; a value must be supplied; no 
	default--
	"Id		ID		#REQUIRED">

<!ENTITY % lang.attrib
	--Lang: indicator of language in which element is written, for
	translation, character set management, etc.; no default--
	"Lang		CDATA		#IMPLIED">

<!ENTITY % remap.attrib
	--Remap: previous role of element before conversion; no default--
	"Remap		CDATA		#IMPLIED">

<!ENTITY % role.attrib
	--Role: new role of element in local environment; no default--
	"Role		CDATA		#IMPLIED">

<!ENTITY % xreflabel.attrib
	--XRefLabel: alternate labeling string for XRef text generation;
	default is usually title or other appropriate label text already
	contained in element--
	"XRefLabel	CDATA		#IMPLIED">

<!ENTITY % revisionflag.attrib
	--RevisionFlag: revision status of element; default is that element
	wasn't revised--
	"RevisionFlag	(Changed
			|Added
			|Deleted
			|Off)		#IMPLIED">

<!ENTITY % local.common.attrib "">
<!ENTITY % common.attrib
	"%id.attrib;
	%lang.attrib;
	%remap.attrib;
	%role.attrib;
	%xreflabel.attrib;
	%revisionflag.attrib;
	%effectivity.attrib;
	%local.common.attrib;"
>

<!ENTITY % idreq.common.attrib
	"%idreq.attrib;
	%lang.attrib;
	%remap.attrib;
	%role.attrib;
	%xreflabel.attrib;
	%revisionflag.attrib;
	%effectivity.attrib;
	%local.common.attrib;"
>

<!-- Semi-common attributes and other attribute entities .................. -->

<!ENTITY % linkend.attrib
	--Linkend: link to related information; no default--
	"Linkend	IDREF		#IMPLIED">

<!ENTITY % linkendreq.attrib
	--Linkend: required link to related information--
	"Linkend	IDREF		#REQUIRED">

<!ENTITY % linkends.attrib
	--Linkends: link to one or more sets of related information; no 
	default--
	"Linkends	IDREFS		#IMPLIED">

<!ENTITY % linkendsreq.attrib
	--Linkends: required link to one or more sets of related information--
	"Linkends	IDREFS		#REQUIRED">

<!ENTITY % label.attrib
	--Label: number or identifying string; default is usually the
	appropriate number or string autogenerated by a formatter--
	"Label		CDATA		#IMPLIED">

<!ENTITY % pagenum.attrib
	--Pagenum: number of page on which element appears; no default--
	"Pagenum	CDATA		#IMPLIED">

<!ENTITY % moreinfo.attrib
	--MoreInfo: whether element's content has an associated RefEntry--
	"MoreInfo	(RefEntry|None)	None">

<!ENTITY % linespecific.attrib
	--Format: whether element is assumed to contain significant white
	space--
	"Format		NOTATION
			(linespecific)	linespecific">

<!ENTITY % local.graphics.attrib "">
<!ENTITY % graphics.attrib
	"Entityref	ENTITY		#IMPLIED
	Fileref 	CDATA		#IMPLIED
	Format		NOTATION
			(%notation.class;)
					#IMPLIED
	SrcCredit	CDATA		#IMPLIED
	%local.graphics.attrib;"
>

<!ENTITY % local.mark.attrib "">
<!ENTITY % mark.attrib
	"Mark		CDATA		#IMPLIED
	%local.mark.attrib;"
>

<!ENTITY % local.keyaction.attrib "">
<!ENTITY % keyaction.attrib
	--Action: Key combination type; default is unspecified if one 
	child element, Simul if there is more than one; if value is 
	Other, the OtherAction attribute must have a nonempty value--
	--OtherAction: User-defined key combination type--
	"Action		(Click
			|Double-Click
			|Press
			|Seq
			|Simul
			|Other)		#IMPLIED
	OtherAction	CDATA		#IMPLIED"
>

<!ENTITY % yesorno.attvals	"NUMBER">
<!ENTITY % yes.attval		"1">
<!ENTITY % no.attval		"0">

<!-- ...................................................................... -->
<!-- Title and bibliographic elements ..................................... -->

<![ %title.module; [
<!ENTITY % local.title.attrib "">
<!ELEMENT Title - O ((%title.char.mix;)+)>
<!ATTLIST Title
		%pagenum.attrib;
		%common.attrib;
		%local.title.attrib;
>
<!--end of title.module-->]]>

<![ %titleabbrev.module; [
<!ENTITY % local.titleabbrev.attrib "">
<!ELEMENT TitleAbbrev - O ((%title.char.mix;)+)>
<!ATTLIST TitleAbbrev
		%common.attrib;
		%local.titleabbrev.attrib;
>
<!--end of titleabbrev.module-->]]>

<![ %subtitle.module; [
<!ENTITY % local.subtitle.attrib "">
<!ELEMENT Subtitle - O ((%title.char.mix;)+)>
<!ATTLIST Subtitle
		%common.attrib;
		%local.subtitle.attrib;
>
<!--end of subtitle.module-->]]>

<!-- The bibliographic elements are typically used in the document
     hierarchy. They do not appear in content models of information
     pool elements.  See also the document information elements,
     below. -->

<![ %biblioentry.content.module; [

<!-- This model of BiblioEntry produces info in the order "title, author";
     TEI prefers "author, title". -->

<![ %biblioentry.module; [
<!ENTITY % local.biblioentry.attrib "">
<!ELEMENT BiblioEntry - O (BiblioMisc?, (ArtHeader | BookBiblio | SeriesInfo),
		BiblioMisc?)>
<!ATTLIST BiblioEntry
		%common.attrib;
		%local.biblioentry.attrib;
>
<!--end of biblioentry.module-->]]>

<![ %bibliomisc.module; [
<!ENTITY % local.bibliomisc.attrib "">
<!ELEMENT BiblioMisc - - ((%para.char.mix;)+)>
<!ATTLIST BiblioMisc
		%common.attrib;
		%local.bibliomisc.attrib;
>
<!--end of bibliomisc.module-->]]>
<!--end of biblioentry.content.module-->]]>

<![ %bookbiblio.module; [
<!ENTITY % local.bookbiblio.attrib "">
<!ELEMENT BookBiblio - - ((Title, TitleAbbrev?)?, Subtitle?, Edition?,
		AuthorGroup+, ((ISBN, VolumeNum?) | (ISSN, VolumeNum?,
		IssueNum?, PageNums?))?, InvPartNumber?, ProductNumber?,
		ProductName?, PubsNumber?, ReleaseInfo?, PubDate*,
		Publisher*, Copyright?, SeriesInfo?, Abstract*, ConfGroup*,
		(ContractNum | ContractSponsor)*, PrintHistory?, RevHistory?)
		-(%ubiq.mix;)>
<!ATTLIST BookBiblio
		%common.attrib;
		%local.bookbiblio.attrib;
>
<!--end of bookbiblio.module-->]]>

<![ %seriesinfo.module; [
<!ENTITY % local.seriesinfo.attrib "">
<!ELEMENT SeriesInfo - - ((%formalobject.title.content;), Subtitle?, 
		AuthorGroup*, ISBN?, VolumeNum?, IssueNum?, SeriesVolNums, 
		PubDate*, Publisher*, Copyright?) -(%ubiq.mix;)>
<!ATTLIST SeriesInfo
		%common.attrib;
		%local.seriesinfo.attrib;
>
<!--end of seriesinfo.module-->]]>

<![ %artheader.module; [
<!ENTITY % local.artheader.attrib "">
<!ELEMENT ArtHeader - - ((%formalobject.title.content;), Subtitle?, 
		AuthorGroup+, BookBiblio?, ArtPageNums, Abstract*, ConfGroup*,
		(ContractNum | ContractSponsor)*)>
<!ATTLIST ArtHeader
		%common.attrib;
		%local.artheader.attrib;
>
<!--end of artheader.module-->]]>

<!-- ...................................................................... -->
<!-- Compound (section-ish) elements ...................................... -->

<!-- Message set ...................... -->

<![ %msgset.content.module; [
<![ %msgset.module; [
<!ENTITY % local.msgset.attrib "">
<!ELEMENT MsgSet - - (MsgEntry+)>
<!ATTLIST MsgSet
		%common.attrib;
		%local.msgset.attrib;
>
<!--end of msgset.module-->]]>

<![ %msgentry.module; [
<!ENTITY % local.msgentry.attrib "">
<!ELEMENT MsgEntry - O (Msg+, MsgInfo?, MsgExplan*)>
<!ATTLIST MsgEntry
		%common.attrib;
		%local.msgentry.attrib;
>
<!--end of msgentry.module-->]]>

<![ %msg.module; [
<!ENTITY % local.msg.attrib "">
<!ELEMENT Msg - O (Title?, MsgMain, (MsgSub | MsgRel)*)>
<!ATTLIST Msg
		%common.attrib;
		%local.msg.attrib;
>
<!--end of msg.module-->]]>

<![ %msgmain.module; [
<!ENTITY % local.msgmain.attrib "">
<!ELEMENT MsgMain - - (Title?, MsgText)>
<!ATTLIST MsgMain
		%common.attrib;
		%local.msgmain.attrib;
>
<!--end of msgmain.module-->]]>

<![ %msgsub.module; [
<!ENTITY % local.msgsub.attrib "">
<!ELEMENT MsgSub - - (Title?, MsgText)>
<!ATTLIST MsgSub
		%common.attrib;
		%local.msgsub.attrib;
>
<!--end of msgsub.module-->]]>

<![ %msgrel.module; [
<!ENTITY % local.msgrel.attrib "">
<!ELEMENT MsgRel - - (Title?, MsgText)>
<!ATTLIST MsgRel
		%common.attrib;
		%local.msgrel.attrib;
>
<!--end of msgrel.module-->]]>

<!--ELEMENT MsgText (defined in the Inlines section, below)-->

<![ %msginfo.module; [
<!ENTITY % local.msginfo.attrib "">
<!ELEMENT MsgInfo - - ((MsgLevel | MsgOrig | MsgAud)*)>
<!ATTLIST MsgInfo
		%common.attrib;
		%local.msginfo.attrib;
>
<!--end of msginfo.module-->]]>

<![ %msglevel.module; [
<!ENTITY % local.msglevel.attrib "">
<!ELEMENT MsgLevel - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MsgLevel
		%common.attrib;
		%local.msglevel.attrib;
>
<!--end of msglevel.module-->]]>

<![ %msgorig.module; [
<!ENTITY % local.msgorig.attrib "">
<!ELEMENT MsgOrig - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MsgOrig
		%common.attrib;
		%local.msgorig.attrib;
>
<!--end of msgorig.module-->]]>

<![ %msgaud.module; [
<!ENTITY % local.msgaud.attrib "">
<!ELEMENT MsgAud - - ((%para.char.mix;)+)>
<!ATTLIST MsgAud
		%common.attrib;
		%local.msgaud.attrib;
>
<!--end of msgaud.module-->]]>

<![ %msgexplan.module; [
<!ENTITY % local.msgexplan.attrib "">
<!ELEMENT MsgExplan - - (Title?, (%component.mix;)+)>
<!ATTLIST MsgExplan
		%common.attrib;
		%local.msgexplan.attrib;
>
<!--end of msgexplan.module-->]]>
<!--end of msgset.content.module-->]]>

<!-- Procedure ........................ -->

<![ %procedure.content.module; [
<![ %procedure.module; [
<!ENTITY % local.procedure.attrib "">
<!ELEMENT Procedure - - ((%formalobject.title.content;)?,
                         (%component.mix;)*, Step+)>
<!ATTLIST Procedure
		%common.attrib;
		%local.procedure.attrib;
>
<!--end of procedure.module-->]]>

<![ %step.module; [
<!ENTITY % local.step.attrib "">
<!ELEMENT Step - O (Title?, (((%component.mix;)+, (SubSteps,
		(%component.mix;)*)?) | (SubSteps, (%component.mix;)*)))>
<!ATTLIST Step
		--Performance: whether step must be performed--
		Performance	(Optional
				|Required)	Required -- not #REQUIRED! --
		%common.attrib;
		%local.step.attrib;
>
<!--end of step.module-->]]>

<![ %substeps.module; [
<!ENTITY % local.substeps.attrib "">
<!ELEMENT SubSteps - - (Step+)>
<!ATTLIST SubSteps
		--Performance: whether whole set of substeps must be
		performed--
		Performance	(Optional
				|Required)	Required -- not #REQUIRED! --
		%common.attrib;
		%local.substeps.attrib;
>
<!--end of substeps.module-->]]>
<!--end of procedure.content.module-->]]>

<!-- Sidebar .......................... -->

<![ %sidebar.module; [
<!ENTITY % local.sidebar.attrib "">
<!ELEMENT Sidebar - - ((%formalobject.title.content)?, (%sidebar.mix;)+)>
<!ATTLIST Sidebar
		%common.attrib;
		%local.sidebar.attrib;
>
<!--end of sidebar.module-->]]>

<!-- ...................................................................... -->
<!-- Paragraph-related elements ........................................... -->

<![ %abstract.module; [
<!ENTITY % local.abstract.attrib "">
<!ELEMENT Abstract - - (Title?, (%para.class;)+)>
<!ATTLIST Abstract
		%common.attrib;
		%local.abstract.attrib;
>
<!--end of abstract.module-->]]>

<![ %authorblurb.module; [
<!ENTITY % local.authorblurb.attrib "">
<!ELEMENT AuthorBlurb - - (Title?, (%para.class;)+)>
<!ATTLIST AuthorBlurb
		%common.attrib;
		%local.authorblurb.attrib;
>
<!--end of authorblurb.module-->]]>

<![ %blockquote.module; [
<!--FUTURE USE (V4.0):
......................
Epigraph will be disallowed from appearing in BlockQuote
......................
-->

<!ENTITY % local.blockquote.attrib "">
<!ELEMENT BlockQuote - - (Title?, Attribution?, (%component.mix;)+)>
<!ATTLIST BlockQuote
		%common.attrib;
		%local.blockquote.attrib;
>
<!--end of blockquote.module-->]]>

<![ %attribution.module; [
<!ENTITY % local.attribution.attrib "">
<!ELEMENT Attribution - O ((%para.char.mix;)+)>
<!ATTLIST Attribution
		%common.attrib;
		%local.attribution.attrib;
>
<!--end of attribution.module-->]]>

<![ %bridgehead.module; [
<!ENTITY % local.bridgehead.attrib "">
<!ELEMENT BridgeHead - - ((%title.char.mix;)+)>
<!ATTLIST BridgeHead
		Renderas	(Other
				|Sect1
				|Sect2
				|Sect3
				|Sect4
				|Sect5)		#IMPLIED
		%common.attrib;
		%local.bridgehead.attrib;
>
<!--end of bridgehead.module-->]]>

<![ %comment.module; [
<!ENTITY % local.comment.attrib "">
<!ELEMENT Comment - - ((%para.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST Comment
		%common.attrib;
		%local.comment.attrib;
>
<!--end of comment.module-->]]>

<![ %epigraph.module; [
<!ENTITY % local.epigraph.attrib "">
<!ELEMENT Epigraph - - (Attribution?, (%para.class;)+)>
<!ATTLIST Epigraph
		%common.attrib;
		%local.epigraph.attrib;
>
<!--ELEMENT Attribution (defined above)-->
<!--end of epigraph.module-->]]>

<![ %footnote.module; [
<!ENTITY % local.footnote.attrib "">
<!ELEMENT Footnote - - ((%footnote.mix;)+) -(Footnote|%formal.class;)>
<!ATTLIST Footnote
		%common.attrib;
		%local.footnote.attrib;
>
<!--end of footnote.module-->]]>

<![ %highlights.module; [
<!ENTITY % local.highlights.attrib "">
<!ELEMENT Highlights - - ((%highlights.mix;)+) -(%ubiq.mix;|%formal.class;)>
<!ATTLIST Highlights
		%common.attrib;
		%local.highlights.attrib;
>
<!--end of highlights.module-->]]>

<![ %formalpara.module; [
<!ENTITY % local.formalpara.attrib "">
<!ELEMENT FormalPara - O (Title, Para)>
<!ATTLIST FormalPara
		%common.attrib;
		%local.formalpara.attrib;
>
<!--end of formalpara.module-->]]>

<![ %para.module; [
<!ENTITY % local.para.attrib "">
<!ELEMENT Para - O ((%para.char.mix; | %para.mix;)+)>
<!ATTLIST Para
		%common.attrib;
		%local.para.attrib;
>
<!--end of para.module-->]]>

<![ %simpara.module; [
<!ENTITY % local.simpara.attrib "">
<!ELEMENT SimPara - O ((%para.char.mix;)+)>
<!ATTLIST SimPara
		%common.attrib;
		%local.simpara.attrib;
>
<!--end of simpara.module-->]]>

<![ %admon.module; [
<!ENTITY % local.admon.attrib "">
<!ELEMENT (%admon.class;) - - (Title?, (%admon.mix;)+) -(%admon.class;)>
<!ATTLIST (%admon.class;)
		%common.attrib;
		%local.admon.attrib;
>
<!--end of admon.module-->]]>

<!-- ...................................................................... -->
<!-- Lists ................................................................ -->

<!-- GlossList ........................ -->

<![ %glosslist.module; [
<!ENTITY % local.glosslist.attrib "">
<!ELEMENT GlossList - - (GlossEntry+)>
<!ATTLIST GlossList
		%common.attrib;
		%local.glosslist.attrib;
>
<!--end of glosslist.module-->]]>

<![ %glossentry.content.module; [
<![ %glossentry.module; [
<!ENTITY % local.glossentry.attrib "">
<!ELEMENT GlossEntry - O (GlossTerm, Acronym?, Abbrev?, (GlossSee|GlossDef+))>
<!ATTLIST GlossEntry
		--SortAs: alternate sort string for automatically
		alphabetized set of glossary entries--
		SortAs		CDATA		#IMPLIED
		%common.attrib;
		%local.glossentry.attrib;
>
<!--end of glossentry.module-->]]>

<!--ELEMENT GlossTerm (defined in the Inlines section, below)-->

<![ %glossdef.module; [
<!ENTITY % local.glossdef.attrib "">
<!ELEMENT GlossDef - O ((%glossdef.mix;)+, GlossSeeAlso*)>
<!ATTLIST GlossDef
		--Subject: one or more subject area keywords for searching--
		Subject		CDATA		#IMPLIED
		%common.attrib;
		%local.glossdef.attrib;
>
<!--end of glossdef.module-->]]>

<![ %glosssee.module; [
<!ENTITY % local.glosssee.attrib "">
<!ELEMENT GlossSee - O ((%para.char.mix;)+)>
<!ATTLIST GlossSee
		--OtherTerm: link to GlossEntry of real term to look up--
		OtherTerm	IDREF		#CONREF
		%common.attrib;
		%local.glosssee.attrib;
>
<!--end of glosssee.module-->]]>

<![ %glossseealso.module; [
<!ENTITY % local.glossseealso.attrib "">
<!ELEMENT GlossSeeAlso - O ((%para.char.mix;)+)>
<!ATTLIST GlossSeeAlso
		--OtherTerm: link to GlossEntry of related term--
		OtherTerm	IDREF		#CONREF
		%common.attrib;
		%local.glossseealso.attrib;
>
<!--end of glossseealso.module-->]]>
<!--end of glossentry.content.module-->]]>

<!-- ItemizedList and OrderedList ..... -->

<![ %itemizedlist.module; [
<!ENTITY % local.itemizedlist.attrib "">
<!ELEMENT ItemizedList - - (ListItem+)>
<!ATTLIST ItemizedList	
		--Spacing: relative desired compactness of list, in author's 
		judgment--
		Spacing		(Normal
				|Compact)	#IMPLIED

		--Mark: keyword, e.g., bullet, dash, checkbox, none;
		list of keywords and defaults are implementation specific--
		%mark.attrib;
		%common.attrib;
		%local.itemizedlist.attrib;
>
<!--end of itemizedlist.module-->]]>

<![ %orderedlist.module; [
<!ENTITY % local.orderedlist.attrib "">
<!ELEMENT OrderedList - - (ListItem+)>
<!ATTLIST OrderedList
		--Numeration: style of list numbering; defaults are
		implementation specific--
		Numeration	(Arabic
				|Upperalpha
				|Loweralpha
				|Upperroman
				|Lowerroman)	#IMPLIED

		--InheritNum: builds lower-level numbers by prefixing
		higher-level item numbers (e.g., 1, 1a, 1b)--
		InheritNum	(Inherit
				|Ignore)	Ignore

		--Continuation: whether numbers are reset from previous list--
		Continuation	(Continues
				|Restarts)	Restarts

		--Spacing: relative desired compactness of list, in author's 
		judgment--
		Spacing		(Normal
				|Compact)	#IMPLIED

		%common.attrib;
		%local.orderedlist.attrib;
>
<!--end of orderedlist.module-->]]>

<![ %listitem.module; [
<!ENTITY % local.listitem.attrib "">
<!ELEMENT ListItem - O ((%component.mix;)+)>
<!ATTLIST ListItem
		--Override: character or string to replace default mark for
		this item only; default is implementation specific--
		Override	CDATA		#IMPLIED
		%common.attrib;
		%local.listitem.attrib;
>
<!--end of listitem.module-->]]>

<!-- SegmentedList .................... -->

<![ %segmentedlist.content.module; [
<![ %segmentedlist.module; [
<!ENTITY % local.segmentedlist.attrib "">
<!ELEMENT SegmentedList - - ((%formalobject.title.content;)?, SegTitle*,
		SegListItem+)>
<!ATTLIST SegmentedList
		%common.attrib;
		%local.segmentedlist.attrib;
>
<!--end of segmentedlist.module-->]]>

<![ %segtitle.module; [
<!ENTITY % local.segtitle.attrib "">
<!ELEMENT SegTitle - O ((%title.char.mix;)+)>
<!ATTLIST SegTitle
		%common.attrib;
		%local.segtitle.attrib;
>
<!--end of segtitle.module-->]]>

<![ %seglistitem.module; [
<!ENTITY % local.seglistitem.attrib "">
<!ELEMENT SegListItem - O (Seg, Seg+)>
<!ATTLIST SegListItem
		%common.attrib;
		%local.seglistitem.attrib;
>
<!--end of seglistitem.module-->]]>

<![ %seg.module; [
<!ENTITY % local.seg.attrib "">
<!ELEMENT Seg - O ((%para.char.mix;)+)>
<!ATTLIST Seg
		%common.attrib;
		%local.seg.attrib;
>
<!--end of seg.module-->]]>
<!--end of segmentedlist.content.module-->]]>

<!-- SimpleList ....................... -->

<![ %simplelist.content.module; [
<![ %simplelist.module; [
<!ENTITY % local.simplelist.attrib "">
<!ELEMENT SimpleList - - (Member+)>
<!ATTLIST SimpleList
		--Columns: number of columns--
		Columns		NUMBER		#IMPLIED

		--Type: Inline: members separated with commas etc. inline
			Vert: members top to bottom in n Columns
			Horiz: members left to right in n Columns
		If Column=1 or implied, Vert and Horiz are the same--
		Type		(Inline
				|Vert
				|Horiz)		Vert
		%common.attrib;
		%local.simplelist.attrib;
>
<!--end of simplelist.module-->]]>

<![ %member.module; [
<!ENTITY % local.member.attrib "">
<!ELEMENT Member - O ((%para.char.mix;)+)>
<!ATTLIST Member
		%common.attrib;
		%local.member.attrib;
>
<!--end of member.module-->]]>
<!--end of simplelist.content.module-->]]>

<!-- VariableList ..................... -->

<![ %variablelist.content.module; [
<![ %variablelist.module; [
<!ENTITY % local.variablelist.attrib "">
<!ELEMENT VariableList - - ((%formalobject.title.content;)?, VarListEntry+)>
<!ATTLIST VariableList
		--TermLength: approximate length of term content that should 
		fit onto one line, in same units that table ColWidth accepts--
		TermLength	CDATA		#IMPLIED
		%common.attrib;
		%local.variablelist.attrib;
>
<!--end of variablelist.module-->]]>

<![ %varlistentry.module; [
<!ENTITY % local.varlistentry.attrib "">
<!ELEMENT VarListEntry - O (Term+, ListItem)>
<!ATTLIST VarListEntry
		%common.attrib;
		%local.varlistentry.attrib;
>
<!--end of varlistentry.module-->]]>

<![ %term.module; [
<!ENTITY % local.term.attrib "">
<!ELEMENT Term - O ((%para.char.mix;)+)>
<!ATTLIST Term
		%common.attrib;
		%local.term.attrib;
>
<!--end of term.module-->]]>

<!--ELEMENT ListItem (defined above)-->
<!--end of variablelist.content.module-->]]>

<!-- CalloutList ...................... -->

<![ %calloutlist.content.module; [
<![ %calloutlist.module; [
<!ENTITY % local.calloutlist.attrib "">
<!ELEMENT CalloutList - - ((%formalobject.title.content;)?, Callout+)>
<!ATTLIST CalloutList
		%common.attrib;
		%local.calloutlist.attrib;
>
<!--end of calloutlist.module-->]]>

<![ %callout.module; [
<!ENTITY % local.callout.attrib "">
<!ELEMENT Callout - O ((%component.mix;)+)>
<!ATTLIST Callout
		--AreaRefs: links to one or more areas or area sets that
		this callout describes--
		AreaRefs	IDREFS		#REQUIRED
		%common.attrib;
		%local.callout.attrib;
>
<!--end of callout.module-->]]>
<!--end of calloutlist.content.module-->]]>

<!-- ...................................................................... -->
<!-- Objects .............................................................. -->

<!-- Examples etc. .................... -->

<![ %example.module; [
<!ENTITY % local.example.attrib "">
<!ELEMENT Example - - ((%formalobject.title.content;), (%example.mix;)+)
		-(%formal.class;)>
<!ATTLIST Example
		%label.attrib;
		%common.attrib;
		%local.example.attrib;
>
<!--end of example.module-->]]>

<![ %informalexample.module; [
<!ENTITY % local.informalexample.attrib "">
<!ELEMENT InformalExample - - ((%example.mix;)+)>
<!ATTLIST InformalExample
		%common.attrib;
		%local.informalexample.attrib;
>
<!--end of informalexample.module-->]]>

<![ %programlistingco.module; [
<!ENTITY % local.programlistingco.attrib "">
<!ELEMENT ProgramListingCO - - (AreaSpec, ProgramListing, CalloutList*)>
<!ATTLIST ProgramListingCO
		%common.attrib;
		%local.programlistingco.attrib;
>
<!--ELEMENT CalloutList (defined above in Lists)-->
<!--end of informalexample.module-->]]>

<![ %areaspec.content.module; [
<![ %areaspec.module; [
<!ENTITY % local.areaspec.attrib "">
<!ELEMENT AreaSpec - - ((Area|AreaSet)+)>
<!ATTLIST AreaSpec
		--Units: global unit of measure in which coordinates in
		this spec are expressed:

		- CALSPair "x1,y1 x2,y2": lower-left and upper-right 
		coordinates in a rectangle describing repro area in which 
		graphic is placed, where X and Y dimensions are each some 
		number 0..10000 (taken from CALS graphic attributes)

		- LineColumn "line column": line number and column number
		at which to start callout text in "linespecific" content

		- LineRange "startline endline": whole lines from startline
		to endline in "linespecific" content

		- LineColumnPair "line1 col1 line2 col2": starting and ending
		points of area in "linespecific" content that starts at
		first position and ends at second position (including the
		beginnings of any intervening lines)

		- Other: directive to look at value of OtherUnits attribute
		to get implementation-specific keyword

		The default is implementation-specific; usually dependent on 
		the parent element (GraphicCO gets CALSPair, ProgramListingCO
		and ScreenCO get LineColumn)--
		Units		(CALSPair
				|LineColumn
				|LineRange
				|LineColumnPair
				|Other)		#IMPLIED
		OtherUnits	NAME		#IMPLIED
		%common.attrib;
		%local.areaspec.attrib;
>
<!--end of areaspec.module-->]]>

<![ %area.module; [
<!ENTITY % local.area.attrib "">
<!ELEMENT Area - O EMPTY>
<!ATTLIST Area
		%label.attrib; --bug number/symbol override or initialization--
		%linkends.attrib; --to any related information--

		--Units: unit of measure in which coordinates in this
		area are expressed; inherits from set and spec--
		Units		(CALSPair
				|LineColumn
				|LineRange
				|LineColumnPair
				|Other)		#IMPLIED
		OtherUnits	NAME		#IMPLIED
		Coords		CDATA		#REQUIRED
		%idreq.common.attrib;
		%local.area.attrib;
>
<!--end of area.module-->]]>

<![ %areaset.module; [
<!ENTITY % local.areaset.attrib "">
<!ELEMENT AreaSet - - (Area+)>
<!ATTLIST AreaSet
		%label.attrib; --bug number/symbol override or initialization--

		--Units: unit of measure in which coordinates in this
		area set are expressed; inherits from spec--
		Units		(CALSPair
				|LineColumn
				|LineRange
				|LineColumnPair
				|Other)		#IMPLIED
		OtherUnits	NAME		#IMPLIED
		Coords		CDATA		#REQUIRED
		%idreq.common.attrib;
		%local.area.attrib;
>
<!--end of areaset.module-->]]>
<!--end of areaspec.content.module-->]]>

<![ %programlisting.module; [
<!ENTITY % local.programlisting.attrib "">
<!ELEMENT ProgramListing - - ((%programlisting.content;)+)>
<!ATTLIST ProgramListing
		--Width: number of columns in longest line, for management
		of wide output (e.g., 80)--
		Width		NUMBER		#IMPLIED
		%linespecific.attrib;
		%common.attrib;
		%local.programlisting.attrib;
>
<!--end of programlisting.module-->]]>

<![ %literallayout.module; [
<!ENTITY % local.literallayout.attrib "">
<!ELEMENT LiteralLayout - - ((LineAnnotation | %para.char.mix;)+)>
<!ATTLIST LiteralLayout
		--Width: number of columns in longest line, for management
		of wide output (e.g., 80)--
		Width		NUMBER		#IMPLIED
		%linespecific.attrib;
		%common.attrib;
		%local.literallayout.attrib;
>
<!--ELEMENT LineAnnotation (defined in the Inlines section, below)-->
<!--end of literallayout.module-->]]>

<![ %screenco.module; [
<!ENTITY % local.screenco.attrib "">
<!ELEMENT ScreenCO - - (AreaSpec, Screen, CalloutList*)>
<!ATTLIST ScreenCO
		%common.attrib;
		%local.screenco.attrib;
>
<!--ELEMENT AreaSpec (defined above)-->
<!--ELEMENT CalloutList (defined above in Lists)-->
<!--end of screenco.module-->]]>

<![ %screen.module; [
<!ENTITY % local.screen.attrib "">
<!ELEMENT Screen - - ((%screen.content;)+)>
<!ATTLIST Screen
		--Width: number of columns in longest line, for management
		of wide output (e.g., 80)--
		Width		NUMBER		#IMPLIED
		%linespecific.attrib;
		%common.attrib;
		%local.screen.attrib;
>
<!--end of screen.module-->]]>

<![ %screenshot.content.module; [
<![ %screenshot.module; [
<!ENTITY % local.screenshot.attrib "">
<!ELEMENT ScreenShot - - (ScreenInfo?, (Graphic|GraphicCO))>
<!ATTLIST ScreenShot
		%common.attrib;
		%local.screenshot.attrib;
>
<!--end of screenshot.module-->]]>

<![ %screeninfo.module; [
<!ENTITY % local.screeninfo.attrib "">
<!ELEMENT ScreenInfo - O ((%para.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST ScreenInfo
		%common.attrib;
		%local.screeninfo.attrib;
>
<!--end of screeninfo.module-->]]>
<!--end of screenshot.content.module-->]]>

<!-- Figures etc. ..................... -->

<![ %figure.module; [
<!ENTITY % local.figure.attrib "">
<!ELEMENT Figure - - ((%formalobject.title.content;), (%figure.mix; |
		%link.char.class;)+)>
<!ATTLIST Figure
		--Float: whether figure can float in output--
		Float		%yesorno.attvals;	%no.attval;
		%label.attrib;
		%common.attrib;
		%local.figure.attrib;
>
<!--end of figure.module-->]]>

<![ %graphicco.module; [
<!ENTITY % local.graphicco.attrib "">
<!ELEMENT GraphicCO - - (AreaSpec, Graphic, CalloutList*)>
<!ATTLIST GraphicCO
		%common.attrib;
		%local.graphicco.attrib;
>
<!--ELEMENT AreaSpec (defined above in Examples)-->
<!--ELEMENT CalloutList (defined above in Lists)-->
<!--end of graphicco.module-->]]>

<!-- Graphical data can be the content of Graphic, or you can reference
     an external file either as an entity (Entitref) or a filename
     (Fileref). -->

<![ %graphic.module; [
<!ENTITY % local.graphic.attrib "">
<!ELEMENT Graphic - - CDATA>
<!ATTLIST Graphic
		%graphics.attrib;
		%common.attrib;
		%local.graphic.attrib;
>
<!--end of graphic.module-->]]>

<![ %inlinegraphic.module; [
<!ENTITY % local.inlinegraphic.attrib "">
<!ELEMENT InlineGraphic - - CDATA>
<!ATTLIST InlineGraphic
		%graphics.attrib;
		%common.attrib;
		%local.inlinegraphic.attrib;
>
<!--end of inlinegraphic.module-->]]>

<!-- Equations ........................ -->

<![ %equation.module; [
<!ENTITY % local.equation.attrib "">
<!ELEMENT Equation - - ((%formalobject.title.content;)?, (InformalEquation |
		%equation.content;))>
<!ATTLIST Equation
		%label.attrib;
	 	%common.attrib;
		%local.equation.attrib;
>
<!--end of equation.module-->]]>

<![ %informalequation.module; [
<!ENTITY % local.informalequation.attrib "">
<!ELEMENT InformalEquation - - (%equation.content;)>
<!ATTLIST InformalEquation
		%common.attrib;
		%local.informalequation.attrib;
>
<!--end of informalequation.module-->]]>

<![ %inlineequation.module; [
<!ENTITY % local.inlineequation.attrib "">
<!ELEMENT InlineEquation - - (%inlineequation.content;)>
<!ATTLIST InlineEquation
		%common.attrib;
		%local.inlineequation.attrib;
>
<!--end of inlineequation.module-->]]>

<!-- Tables ........................... -->

<![ %table.module; [

<!ENTITY % bodyatt "%label.attrib;"	-- add Label to main element -->
<!ENTITY % secur   "%common.attrib;"	-- add common atts to all elements -->
<!ENTITY % tblelm "Table"		-- remove Chart -->
<!ENTITY % tblmdl  "((%formalobject.title.content;), (Graphic+|TGroup+))"
					-- content model for formal tables -->
<!ENTITY % tblexpt "-(InformalTable|%formal.class;)"
					-- exclude all DocBook tables -->
<!ENTITY % tblcon  "((%tabentry.mix;)+|(%para.char.mix;)+)"
					-- allow either blocks or inlines;
					   beware of REs between elems -->
<!ENTITY % tblrowex ""			-- remove pgbrk exception on row -->
<!ENTITY % tblconex ""			-- remove pgbrk exception on entry -->

<!ENTITY % calstbl PUBLIC
	"-//Davenport//ELEMENTS CALS-Based DocBook Table Model V2.4.1//EN">
%calstbl;

<!--end of table.module-->]]>

<![ %informaltable.module; [
<!ENTITY % local.informaltable.attrib "">
<!ELEMENT InformalTable - - (Graphic+|TGroup+) %tblexpt;>
<!ATTLIST InformalTable
		ToCEntry	%yesorno.attvals;	#IMPLIED
		ShortEntry	%yesorno.attvals;	#IMPLIED
		Frame		(Top
				|Bottom
				|Topbot
				|All
				|Sides
				|None)			#IMPLIED
		Colsep		%yesorno.attvals;	#IMPLIED
		Rowsep		%yesorno.attvals;	#IMPLIED
		%tblatt;	-- includes TabStyle, Orient, PgWide --
		%bodyatt;	-- includes Label --
		%secur;		-- includes common atts --
		%local.informaltable.attrib;
>
<!--end of informaltable.module-->]]>

<!-- ...................................................................... -->
<!-- Synopses ............................................................. -->

<!-- Synopsis ......................... -->

<![ %synopsis.module; [
<!ENTITY % local.synopsis.attrib "">
<!ELEMENT Synopsis - - ((LineAnnotation | %para.char.mix; | Graphic)+)>
<!ATTLIST Synopsis
		%label.attrib;
		%linespecific.attrib;
		%common.attrib;
		%local.synopsis.attrib;
>

<!--ELEMENT LineAnnotation (defined in the Inlines section, below)-->
<!--end of synopsis.module-->]]>

<!-- CmdSynopsis ...................... -->

<![ %cmdsynopsis.content.module; [
<![ %cmdsynopsis.module; [
<!ENTITY % local.cmdsynopsis.attrib "">
<!ELEMENT CmdSynopsis - - ((Command | Arg | Group | SBR)+, SynopFragment*)>
<!ATTLIST CmdSynopsis
		%label.attrib;

		--Sepchar: character that should separate command and
		all top-level arguments; alternate value might be &Delta;--
		Sepchar		CDATA		" "
		%common.attrib;
		%local.cmdsynopsis.attrib;
>
<!--end of cmdsynopsis.module-->]]>

<![ %arg.module; [
<!ENTITY % local.arg.attrib "">
<!ELEMENT Arg - - ((#PCDATA 
		| Arg 
		| Group 
		| Option 
		| SynopFragmentRef 
		| Replaceable
		| SBR)+)>
<!ATTLIST Arg
		--Choice: whether Arg must be supplied:
			Opt: optional to supply (e.g. [arg])
			Req: required to supply (e.g. {arg})
			Plain: required to supply (e.g. arg)--
		Choice		(Opt
				|Req
				|Plain)		Opt

		--Rep: whether Arg is repeatable:
			Norepeat: no (e.g. arg without ellipsis)
			Repeat: yes (e.g. arg...)--
		Rep		(Norepeat
				|Repeat)	Norepeat
		%common.attrib;
		%local.arg.attrib;
>
<!--end of arg.module-->]]>

<![ %group.module; [
<!--FUTURE USE (V4.0):
......................
The OptMult and ReqMult values for the Choice attribute on Group will be
removed.  Use the Rep attribute instead to indicate that the choice is
repeatable.
......................
-->

<!ENTITY % local.group.attrib "">
<!ELEMENT Group - - ((Arg | Group | Option | SynopFragmentRef 
		| Replaceable | SBR)+)>
<!ATTLIST Group
		--Choice: whether Group must be supplied:
			Opt: optional to supply (e.g. [g1|g2|g3])
			Req: required to supply (e.g. {g1|g2|g3})
			Plain: required to supply (e.g. g1|g2|g3)
			OptMult: can supply 0+ (e.g. [[g1|g2|g3]])
			ReqMult: must supply 1+ (e.g. {{g1|g2|g3}})--
		Choice		(Opt
				|Req
				|Plain
				|Optmult
				|Reqmult)	Opt

		--Rep: whether Group is repeatable:
			Norepeat: no (e.g. group without ellipsis)
			Repeat: yes (e.g. group...)--
		Rep		(Norepeat
				|Repeat)	Norepeat
		%common.attrib;
		%local.group.attrib;
>
<!--end of group.module-->]]>

<![ %sbr.module; [
<!ENTITY % local.sbr.attrib "">
<!-- Synopsis break -->
<!ELEMENT SBR - O EMPTY>
<!ATTLIST SBR
		%common.attrib;
		%local.sbr.attrib;
>
<!--end of sbr.module-->]]>

<![ %synopfragmentref.module; [
<!ENTITY % local.synopfragmentref.attrib "">
<!ELEMENT SynopFragmentRef - - RCDATA >
<!ATTLIST SynopFragmentRef
		%linkendreq.attrib; --to SynopFragment of complex synopsis
		material for separate referencing--
		%common.attrib;
		%local.synopfragmentref.attrib;
>
<!--end of synopfragmentref.module-->]]>

<![ %synopfragment.module; [
<!ENTITY % local.synopfragment.attrib "">
<!ELEMENT SynopFragment - - ((Arg | Group)+)>
<!ATTLIST SynopFragment
		%idreq.common.attrib;
		%local.synopfragment.attrib;
>
<!--end of synopfragment.module-->]]>

<!--ELEMENT Command (defined in the Inlines section, below)-->
<!--ELEMENT Option (defined in the Inlines section, below)-->
<!--ELEMENT Replaceable (defined in the Inlines section, below)-->
<!--end of cmdsynopsis.content.module-->]]>

<!-- FuncSynopsis ..................... -->

<![ %funcsynopsis.content.module; [
<![ %funcsynopsis.module; [

<!--FUTURE USE (V4.0):
......................
The block starting with FuncDef will not be repeatable; you will have
to use FuncPrototype if you want multiple blocks.

<!ELEMENT FuncSynopsis - - (FuncSynopsisInfo?, (FuncPrototype+ |
		(FuncDef, (Void | VarArgs | ParamDef+))))>
......................
-->

<!ENTITY % local.funcsynopsis.attrib "">
<!ELEMENT FuncSynopsis - - (FuncSynopsisInfo?, (FuncPrototype+ |
		(FuncDef, (Void | VarArgs | ParamDef+))+))>
<!ATTLIST FuncSynopsis
		%label.attrib;
		%common.attrib;
		%local.funcsynopsis.attrib;
>
<!--end of funcsynopsis.module-->]]>

<![ %funcsynopsisinfo.module; [
<!ENTITY % local.funcsynopsisinfo.attrib "">
<!ELEMENT FuncSynopsisInfo - O ((LineAnnotation | %cptr.char.mix;)* )>
<!ATTLIST FuncSynopsisInfo
		%linespecific.attrib;
		%common.attrib;
		%local.funcsynopsisinfo.attrib;
>
<!--end of funcsynopsisinfo.module-->]]>

<![ %funcprototype.module; [
<!ENTITY % local.funcprototype.attrib "">
<!ELEMENT FuncPrototype - O (FuncDef, (Void | VarArgs | ParamDef+))>
<!ATTLIST FuncPrototype
		%common.attrib;
		%local.funcprototype.attrib;
>
<!--end of funcprototype.module-->]]>

<![ %funcdef.module; [
<!ENTITY % local.funcdef.attrib "">
<!ELEMENT FuncDef - - ((#PCDATA 
		| Replaceable 
		| Function)*)>
<!ATTLIST FuncDef
		%common.attrib;
		%local.funcdef.attrib;
>
<!--end of funcdef.module-->]]>

<![ %void.module; [
<!ENTITY % local.void.attrib "">
<!ELEMENT Void - O EMPTY>
<!ATTLIST Void
		%common.attrib;
		%local.void.attrib;
>
<!--end of void.module-->]]>

<![ %varargs.module; [
<!ENTITY % local.varargs.attrib "">
<!ELEMENT VarArgs - O EMPTY>
<!ATTLIST VarArgs
		%common.attrib;
		%local.varargs.attrib;
>
<!--end of varargs.module-->]]>

<!-- Processing assumes that only one Parameter will appear in a
     ParamDef, and that FuncParams will be used at most once, for
     providing information on the "inner parameters" for parameters that
     are pointers to functions. -->

<![ %paramdef.module; [
<!ENTITY % local.paramdef.attrib "">
<!ELEMENT ParamDef - - ((#PCDATA 
		| Replaceable 
		| Parameter 
		| FuncParams)*)>
<!ATTLIST ParamDef
		%common.attrib;
		%local.paramdef.attrib;
>
<!--end of paramdef.module-->]]>

<![ %funcparams.module; [
<!ENTITY % local.funcparams.attrib "">
<!ELEMENT FuncParams - - ((%cptr.char.mix;)*)>
<!ATTLIST FuncParams
		%common.attrib;
		%local.funcparams.attrib;
>
<!--end of funcparams.module-->]]>

<!--ELEMENT LineAnnotation (defined in the Inlines section, below)-->
<!--ELEMENT Replaceable (defined in the Inlines section, below)-->
<!--ELEMENT Function (defined in the Inlines section, below)-->
<!--ELEMENT Parameter (defined in the Inlines section, below)-->
<!--end of funcsynopsis.content.module-->]]>

<!-- ...................................................................... -->
<!-- Document information entities and elements ........................... -->

<!ENTITY % local.person.ident.mix "">
<!ENTITY % person.ident.mix
		"Honorific|FirstName|Surname|Lineage|OtherName|Affiliation
		|AuthorBlurb|Contrib %local.person.ident.mix;">

<!-- The document information elements include some elements that are
     currently used only in the document hierarchy module. They are
     defined here so that they will be available for use in customized
     document hierarchies. -->

<!-- .................................. -->

<![ %docinfo.content.module; [

<!-- Ackno ............................ -->

<![ %ackno.module; [
<!ENTITY % local.ackno.attrib "">
<!ELEMENT Ackno - - ((%docinfo.char.mix;)+)>
<!ATTLIST Ackno
		%common.attrib;
		%local.ackno.attrib;
>
<!--end of ackno.module-->]]>

<!-- Address .......................... -->

<![ %address.content.module; [
<![ %address.module; [
<!ENTITY % local.address.attrib "">
<!ELEMENT Address - - (#PCDATA|Street|POB|Postcode|City|State|Country|Phone
		|Fax|Email|OtherAddr)*>
<!ATTLIST Address
		%linespecific.attrib;
		%common.attrib;
		%local.address.attrib;
>
<!--end of address.module-->]]>

  <![ %street.module; [
  <!ENTITY % local.street.attrib "">
  <!ELEMENT Street - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Street
		%common.attrib;
		%local.street.attrib;
>
  <!--end of street.module-->]]>

  <![ %pob.module; [
  <!ENTITY % local.pob.attrib "">
  <!ELEMENT POB - - ((%docinfo.char.mix;)+)>
  <!ATTLIST POB
		%common.attrib;
		%local.pob.attrib;
>
  <!--end of pob.module-->]]>

  <![ %postcode.module; [
  <!ENTITY % local.postcode.attrib "">
  <!ELEMENT Postcode - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Postcode
		%common.attrib;
		%local.postcode.attrib;
>
  <!--end of postcode.module-->]]>

  <![ %city.module; [
  <!ENTITY % local.city.attrib "">
  <!ELEMENT City - - ((%docinfo.char.mix;)+)>
  <!ATTLIST City
		%common.attrib;
		%local.city.attrib;
>
  <!--end of city.module-->]]>

  <![ %state.module; [
  <!ENTITY % local.state.attrib "">
  <!ELEMENT State - - ((%docinfo.char.mix;)+)>
  <!ATTLIST State
		%common.attrib;
		%local.state.attrib;
>
  <!--end of state.module-->]]>

  <![ %country.module; [
  <!ENTITY % local.country.attrib "">
  <!ELEMENT Country - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Country
		%common.attrib;
		%local.country.attrib;
>
  <!--end of country.module-->]]>

  <![ %phone.module; [
  <!ENTITY % local.phone.attrib "">
  <!ELEMENT Phone - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Phone
		%common.attrib;
		%local.phone.attrib;
>
  <!--end of phone.module-->]]>

  <![ %fax.module; [
  <!ENTITY % local.fax.attrib "">
  <!ELEMENT Fax - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Fax
		%common.attrib;
		%local.fax.attrib;
>
  <!--end of fax.module-->]]>

  <!--ELEMENT Email (defined in the Inlines section, below)-->

  <![ %otheraddr.module; [
  <!ENTITY % local.otheraddr.attrib "">
  <!ELEMENT OtherAddr - - ((%docinfo.char.mix;)+)>
  <!ATTLIST OtherAddr
		%common.attrib;
		%local.otheraddr.attrib;
>
  <!--end of otheraddr.module-->]]>
<!--end of address.content.module-->]]>

<!-- Affiliation ...................... -->

<![ %affiliation.content.module; [
<![ %affiliation.module; [
<!ENTITY % local.affiliation.attrib "">
<!ELEMENT Affiliation - - (ShortAffil?, JobTitle*, OrgName?, OrgDiv*,
		Address*)>
<!ATTLIST Affiliation
		%common.attrib;
		%local.affiliation.attrib;
>
<!--end of affiliation.module-->]]>

  <![ %shortaffil.module; [
  <!ENTITY % local.shortaffil.attrib "">
  <!ELEMENT ShortAffil - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ShortAffil
		%common.attrib;
		%local.shortaffil.attrib;
>
  <!--end of shortaffil.module-->]]>

  <![ %jobtitle.module; [
  <!ENTITY % local.jobtitle.attrib "">
  <!ELEMENT JobTitle - - ((%docinfo.char.mix;)+)>
  <!ATTLIST JobTitle
		%common.attrib;
		%local.jobtitle.attrib;
>
  <!--end of jobtitle.module-->]]>

  <!--ELEMENT OrgName (defined elsewhere in this section)-->

  <![ %orgdiv.module; [
  <!ENTITY % local.orgdiv.attrib "">
  <!ELEMENT OrgDiv - - ((%docinfo.char.mix;)+)>
  <!ATTLIST OrgDiv
		%common.attrib;
		%local.orgdiv.attrib;
>
  <!--end of orgdiv.module-->]]>

  <!--ELEMENT Address (defined elsewhere in this section)-->
<!--end of affiliation.content.module-->]]>

<!-- ArtPageNums ...................... -->

<![ %artpagenums.module; [
<!ENTITY % local.artpagenums.attrib "">
<!ELEMENT ArtPageNums - - ((%docinfo.char.mix;)+)>
<!ATTLIST ArtPageNums
		%common.attrib;
		%local.artpagenums.attrib;
>
<!--end of artpagenums.module-->]]>

<!-- Author ........................... -->

<![ %author.module; [
<!ENTITY % local.author.attrib "">
<!ELEMENT Author - - ((%person.ident.mix;)+)>
<!ATTLIST Author
		%common.attrib;
		%local.author.attrib;
>
<!--(see "personal identity elements" for %person.ident.mix;)-->
<!--end of author.module-->]]>

<!-- AuthorGroup ...................... -->

<![ %authorgroup.content.module; [
<![ %authorgroup.module; [
<!ENTITY % local.authorgroup.attrib "">
<!ELEMENT AuthorGroup - - ((Author|Editor|Collab|CorpAuthor|OtherCredit)+)>
<!ATTLIST AuthorGroup
		%common.attrib;
		%local.authorgroup.attrib;
>
<!--end of authorgroup.module-->]]>

  <!--ELEMENT Author (defined elsewhere in this section)-->
  <!--ELEMENT Editor (defined elsewhere in this section)-->

  <![ %collab.content.module; [
  <![ %collab.module; [
  <!ENTITY % local.collab.attrib "">
  <!ELEMENT Collab - - (CollabName, Affiliation*)>
  <!ATTLIST Collab
		%common.attrib;
		%local.collab.attrib;
>
  <!--end of collab.module-->]]>

    <![ %collabname.module; [
  <!ENTITY % local.collabname.attrib "">
    <!ELEMENT CollabName - - ((%docinfo.char.mix;)+)>
    <!ATTLIST CollabName
		%common.attrib;
		%local.collabname.attrib;
>
    <!--end of collabname.module-->]]>

    <!--ELEMENT Affiliation (defined elsewhere in this section)-->
  <!--end of collab.content.module-->]]>

  <!--ELEMENT CorpAuthor (defined elsewhere in this section)-->
  <!--ELEMENT OtherCredit (defined elsewhere in this section)-->

<!--end of authorgroup.content.module-->]]>

<!-- AuthorInitials ................... -->

<![ %authorinitials.module; [
<!ENTITY % local.authorinitials.attrib "">
<!ELEMENT AuthorInitials - - ((%docinfo.char.mix;)+)>
<!ATTLIST AuthorInitials
		%common.attrib;
		%local.authorinitials.attrib;
>
<!--end of authorinitials.module-->]]>

<!-- ConfGroup ........................ -->

<![ %confgroup.content.module; [
<![ %confgroup.module; [
<!ENTITY % local.confgroup.attrib "">
<!ELEMENT ConfGroup - - ((ConfDates|ConfTitle|ConfNum|Address|ConfSponsor)*)>
<!ATTLIST ConfGroup
		%common.attrib;
		%local.confgroup.attrib;
>
<!--end of confgroup.module-->]]>

  <![ %confdates.module; [
  <!ENTITY % local.confdates.attrib "">
  <!ELEMENT ConfDates - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfDates
		%common.attrib;
		%local.confdates.attrib;
>
  <!--end of confdates.module-->]]>

  <![ %conftitle.module; [
  <!ENTITY % local.conftitle.attrib "">
  <!ELEMENT ConfTitle - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfTitle
		%common.attrib;
		%local.conftitle.attrib;
>
  <!--end of conftitle.module-->]]>

  <![ %confnum.module; [
  <!ENTITY % local.confnum.attrib "">
  <!ELEMENT ConfNum - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfNum
		%common.attrib;
		%local.confnum.attrib;
>
  <!--end of confnum.module-->]]>

  <!--ELEMENT Address (defined elsewhere in this section)-->

  <![ %confsponsor.module; [
  <!ENTITY % local.confsponsor.attrib "">
  <!ELEMENT ConfSponsor - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfSponsor
		%common.attrib;
		%local.confsponsor.attrib;
>
  <!--end of confsponsor.module-->]]>
<!--end of confgroup.content.module-->]]>

<!-- ContractNum ...................... -->

<![ %contractnum.module; [
<!ENTITY % local.contractnum.attrib "">
<!ELEMENT ContractNum - - ((%docinfo.char.mix;)+)>
<!ATTLIST ContractNum
		%common.attrib;
		%local.contractnum.attrib;
>
<!--end of contractnum.module-->]]>

<!-- ContractSponsor .................. -->

<![ %contractsponsor.module; [
<!ENTITY % local.contractsponsor.attrib "">
<!ELEMENT ContractSponsor - - ((%docinfo.char.mix;)+)>
<!ATTLIST ContractSponsor
		%common.attrib;
		%local.contractsponsor.attrib;
>
<!--end of contractsponsor.module-->]]>

<!-- Copyright ........................ -->

<![ %copyright.content.module; [
<![ %copyright.module; [
<!ENTITY % local.copyright.attrib "">
<!ELEMENT Copyright - - (Year+, Holder*)>
<!ATTLIST Copyright
		%common.attrib;
		%local.copyright.attrib;
>
<!--end of copyright.module-->]]>

  <![ %year.module; [
  <!ENTITY % local.year.attrib "">
  <!ELEMENT Year - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Year
		%common.attrib;
		%local.year.attrib;
>
  <!--end of year.module-->]]>

  <![ %holder.module; [
  <!ENTITY % local.holder.attrib "">
  <!ELEMENT Holder - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Holder
		%common.attrib;
		%local.holder.attrib;
>
  <!--end of holder.module-->]]>
<!--end of copyright.content.module-->]]>

<!-- CorpAuthor ....................... -->

<![ %corpauthor.module; [
<!ENTITY % local.corpauthor.attrib "">
<!ELEMENT CorpAuthor - - ((%docinfo.char.mix;)+)>
<!ATTLIST CorpAuthor
		%common.attrib;
		%local.corpauthor.attrib;
>
<!--end of corpauthor.module-->]]>

<!-- CorpName ......................... -->

<![ %corpname.module; [
<!ENTITY % local.corpname.attrib "">
<!ELEMENT CorpName - - ((%docinfo.char.mix;)+)>
<!ATTLIST CorpName
		%common.attrib;
		%local.corpname.attrib;
>
<!--end of corpname.module-->]]>

<!-- Date ............................. -->

<![ %date.module; [
<!ENTITY % local.date.attrib "">
<!ELEMENT Date - - ((%docinfo.char.mix;)+)>
<!ATTLIST Date
		%common.attrib;
		%local.date.attrib;
>
<!--end of date.module-->]]>

<!-- Edition .......................... -->

<![ %edition.module; [
<!ENTITY % local.edition.attrib "">
<!ELEMENT Edition - - ((%docinfo.char.mix;)+)>
<!ATTLIST Edition
		%common.attrib;
		%local.edition.attrib;
>
<!--end of edition.module-->]]>

<!-- Editor ........................... -->

<![ %editor.module; [
<!ENTITY % local.editor.attrib "">
<!ELEMENT Editor - - ((%person.ident.mix;)+)>
<!ATTLIST Editor
		%common.attrib;
		%local.editor.attrib;
>
  <!--(see "personal identity elements" for %person.ident.mix;)-->
<!--end of editor.module-->]]>

<!-- ISBN ............................. -->

<![ %isbn.module; [
<!ENTITY % local.isbn.attrib "">
<!ELEMENT ISBN - - ((%docinfo.char.mix;)+)>
<!ATTLIST ISBN
		%common.attrib;
		%local.isbn.attrib;
>
<!--end of isbn.module-->]]>

<!-- ISSN ............................. -->

<![ %issn.module; [
<!ENTITY % local.issn.attrib "">
<!ELEMENT ISSN - - ((%docinfo.char.mix;)+)>
<!ATTLIST ISSN
		%common.attrib;
		%local.issn.attrib;
>
<!--end of issn.module-->]]>

<!-- InvPartNumber .................... -->

<![ %invpartnumber.module; [
<!ENTITY % local.invpartnumber.attrib "">
<!ELEMENT InvPartNumber - - ((%docinfo.char.mix;)+)>
<!ATTLIST InvPartNumber
		%common.attrib;
		%local.invpartnumber.attrib;
>
<!--end of invpartnumber.module-->]]>

<!-- IssueNum ......................... -->

<![ %issuenum.module; [
<!ENTITY % local.issuenum.attrib "">
<!ELEMENT IssueNum - - ((%docinfo.char.mix;)+)>
<!ATTLIST IssueNum
		%common.attrib;
		%local.issuenum.attrib;
>
<!--end of issuenum.module-->]]>

<!-- LegalNotice ...................... -->

<![ %legalnotice.module; [
<!ENTITY % local.legalnotice.attrib "">
<!ELEMENT LegalNotice - - (Title?, (%legalnotice.mix;)+) -(%formal.class;)>
<!ATTLIST LegalNotice
		%common.attrib;
		%local.legalnotice.attrib;
>
<!--end of legalnotice.module-->]]>

<!-- ModeSpec ......................... -->

<![ %modespec.module; [
<!ENTITY % local.modespec.attrib "">
<!ELEMENT ModeSpec - - ((%docinfo.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST ModeSpec
		--Application: type of retrieval query--
		Application	NOTATION
				(%notation.class;)	#IMPLIED
		%common.attrib;
		%local.modespec.attrib;
>
<!--end of modespec.module-->]]>

<!-- OrgName .......................... -->

<![ %orgname.module; [
<!ENTITY % local.orgname.attrib "">
<!ELEMENT OrgName - - ((%docinfo.char.mix;)+)>
<!ATTLIST OrgName
		%common.attrib;
		%local.orgname.attrib;
>
<!--end of orgname.module-->]]>

<!-- OtherCredit ...................... -->

<![ %othercredit.module; [
<!ENTITY % local.othercredit.attrib "">
<!ELEMENT OtherCredit - - ((%person.ident.mix;)+)>
<!ATTLIST OtherCredit
		%common.attrib;
		%local.othercredit.attrib;
>
  <!--(see "personal identity elements" for %person.ident.mix;)-->
<!--end of othercredit.module-->]]>

<!-- PageNums ......................... -->

<![ %pagenums.module; [
<!ENTITY % local.pagenums.attrib "">
<!ELEMENT PageNums - - ((%docinfo.char.mix;)+)>
<!ATTLIST PageNums
		%common.attrib;
		%local.pagenums.attrib;
>
<!--end of pagenums.module-->]]>

<!-- personal identity elements ....... -->

<!-- These elements are used only within Author, Editor, and OtherCredit. -->

<![ %person.ident.module; [
  <![ %contrib.module; [
  <!ENTITY % local.contrib.attrib "">
  <!ELEMENT Contrib - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Contrib
		%common.attrib;
		%local.contrib.attrib;
>
  <!--end of contrib.module-->]]>

  <![ %firstname.module; [
  <!ENTITY % local.firstname.attrib "">
  <!ELEMENT FirstName - - ((%docinfo.char.mix;)+)>
  <!ATTLIST FirstName
		%common.attrib;
		%local.firstname.attrib;
>
  <!--end of firstname.module-->]]>

  <![ %honorific.module; [
  <!ENTITY % local.honorific.attrib "">
  <!ELEMENT Honorific - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Honorific
		%common.attrib;
		%local.honorific.attrib;
>
  <!--end of honorific.module-->]]>

  <![ %lineage.module; [
  <!ENTITY % local.lineage.attrib "">
  <!ELEMENT Lineage - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Lineage
		%common.attrib;
		%local.lineage.attrib;
>
  <!--end of lineage.module-->]]>

  <![ %othername.module; [
  <!ENTITY % local.othername.attrib "">
  <!ELEMENT OtherName - - ((%docinfo.char.mix;)+)>
  <!ATTLIST OtherName
		%common.attrib;
		%local.othername.attrib;
>
  <!--end of othername.module-->]]>

  <![ %surname.module; [
  <!ENTITY % local.surname.attrib "">
  <!ELEMENT Surname - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Surname
		%common.attrib;
		%local.surname.attrib;
>
  <!--end of surname.module-->]]>
<!--end of person.ident.module-->]]>

<!-- PrintHistory ..................... -->

<![ %printhistory.module; [
<!ENTITY % local.printhistory.attrib "">
<!ELEMENT PrintHistory - - ((%para.class;)+)>
<!ATTLIST PrintHistory
		%common.attrib;
		%local.printhistory.attrib;
>
<!--end of printhistory.module-->]]>

<!-- ProductName ...................... -->

<![ %productname.module; [
<!ENTITY % local.productname.attrib "">
<!ELEMENT ProductName - - ((%para.char.mix;)+)>
<!ATTLIST ProductName
		Class		(Service
				|Trade
				|Registered
				|Copyright)	Trade
		%common.attrib;
		%local.productname.attrib;
>
<!--end of productname.module-->]]>

<!-- ProductNumber .................... -->

<![ %productnumber.module; [
<!ENTITY % local.productnumber.attrib "">
<!ELEMENT ProductNumber - - ((%docinfo.char.mix;)+)>
<!ATTLIST ProductNumber
		%common.attrib;
		%local.productnumber.attrib;
>
<!--end of productnumber.module-->]]>

<!-- PubDate .......................... -->

<![ %pubdate.module; [
<!ENTITY % local.pubdate.attrib "">
<!ELEMENT PubDate - - ((%docinfo.char.mix;)+)>
<!ATTLIST PubDate
		%common.attrib;
		%local.pubdate.attrib;
>
<!--end of pubdate.module-->]]>

<!-- Publisher ........................ -->

<![ %publisher.content.module; [
<![ %publisher.module; [
<!ENTITY % local.publisher.attrib "">
<!ELEMENT Publisher - - (PublisherName, Address*)>
<!ATTLIST Publisher
		%common.attrib;
		%local.publisher.attrib;
>
<!--end of publisher.module-->]]>

  <![ %publishername.module; [
  <!ENTITY % local.publishername.attrib "">
  <!ELEMENT PublisherName - - ((%docinfo.char.mix;)+)>
  <!ATTLIST PublisherName
		%common.attrib;
		%local.publishername.attrib;
>
  <!--end of publishername.module-->]]>

  <!--ELEMENT Address (defined elsewhere in this section)-->
<!--end of publisher.content.module-->]]>

<!-- PubsNumber ....................... -->

<![ %pubsnumber.module; [
<!ENTITY % local.pubsnumber.attrib "">
<!ELEMENT PubsNumber - - ((%docinfo.char.mix;)+)>
<!ATTLIST PubsNumber
		%common.attrib;
		%local.pubsnumber.attrib;
>
<!--end of pubsnumber.module-->]]>

<!-- ReleaseInfo ...................... -->

<![ %releaseinfo.module; [
<!ENTITY % local.releaseinfo.attrib "">
<!ELEMENT ReleaseInfo - - ((%docinfo.char.mix;)+)>
<!ATTLIST ReleaseInfo
		%common.attrib;
		%local.releaseinfo.attrib;
>
<!--end of releaseinfo.module-->]]>

<!-- RevHistory ....................... -->

<![ %revhistory.content.module; [
<![ %revhistory.module; [

<!--FUTURE USE (V3.0):
......................
The RevHistory element will require content:

<!ELEMENT RevHistory - - (Revision+)>
......................
-->

<!ENTITY % local.revhistory.attrib "">
<!ELEMENT RevHistory - - (Revision*)>
<!ATTLIST RevHistory
		%common.attrib;
		%local.revhistory.attrib;
>
<!--end of revhistory.module-->]]>

  <![ %revision.module; [
  <!ENTITY % local.revision.attrib "">
  <!ELEMENT Revision - - (RevNumber, Date, AuthorInitials*, RevRemark?)>
  <!ATTLIST Revision
		%common.attrib;
		%local.revision.attrib;
>
  <!--end of revision.module-->]]>

  <![ %revnumber.module; [
  <!ENTITY % local.revnumber.attrib "">
  <!ELEMENT RevNumber - - ((%docinfo.char.mix;)+)>
  <!ATTLIST RevNumber
		%common.attrib;
		%local.revnumber.attrib;
>
  <!--end of revnumber.module-->]]>

  <!--ELEMENT Date (defined elsewhere in this section)-->
  <!--ELEMENT AuthorInitials (defined elsewhere in this section)-->

  <![ %revremark.module; [
  <!ENTITY % local.revremark.attrib "">
  <!ELEMENT RevRemark - - ((%docinfo.char.mix;)+)>
  <!ATTLIST RevRemark
		%common.attrib;
		%local.revremark.attrib;
>
  <!--end of revremark.module-->]]>
<!--end of revhistory.content.module-->]]>

<!-- SeriesVolNums .................... -->

<![ %seriesvolnums.module; [
<!ENTITY % local.seriesvolnums.attrib "">
<!ELEMENT SeriesVolNums - - ((%docinfo.char.mix;)+)>
<!ATTLIST SeriesVolNums
		%common.attrib;
		%local.seriesvolnums.attrib;
>
<!--end of seriesvolnums.module-->]]>

<!-- VolumeNum ........................ -->

<![ %volumenum.module; [
<!ENTITY % local.volumenum.attrib "">
<!ELEMENT VolumeNum - - ((%docinfo.char.mix;)+)>
<!ATTLIST VolumeNum
		%common.attrib;
		%local.volumenum.attrib;
>
<!--end of volumenum.module-->]]>

<!-- .................................. -->

<!--end of docinfo.content.module-->]]>

<!-- ...................................................................... -->
<!-- Inline, link, and ubiquitous elements ................................ -->

<!-- Computer terms ....................................................... -->

<![ %accel.module; [
<!ENTITY % local.accel.attrib "">
<!ELEMENT Accel - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Accel
		%common.attrib;
		%local.accel.attrib;
>
<!--end of accel.module-->]]>

<![ %action.module; [
<!ENTITY % local.action.attrib "">
<!ELEMENT Action - - ((%cptr.char.mix;)+)>
<!ATTLIST Action
		%moreinfo.attrib;
		%common.attrib;
		%local.action.attrib;
>
<!--end of action.module-->]]>

<![ %application.module; [
<!ENTITY % local.application.attrib "">
<!ELEMENT Application - - ((%para.char.mix;)+)>
<!ATTLIST Application
		Class 		(Hardware
				|Software)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%local.application.attrib;
>
<!--end of application.module-->]]>

<![ %classname.module; [
<!ENTITY % local.classname.attrib "">
<!ELEMENT ClassName - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ClassName
		%common.attrib;
		%local.classname.attrib;
>
<!--end of classname.module-->]]>

<![ %command.module; [
<!ENTITY % local.command.attrib "">
<!ELEMENT Command - - ((%cptr.char.mix;)+)>
<!ATTLIST Command
		%moreinfo.attrib;
		%common.attrib;
		%local.command.attrib;
>
<!--end of command.module-->]]>

<![ %computeroutput.module; [
<!ENTITY % local.computeroutput.attrib "">
<!ELEMENT ComputerOutput - - ((%cptr.char.mix;)+)>
<!ATTLIST ComputerOutput
		%moreinfo.attrib;
		%common.attrib;
		%local.computeroutput.attrib;
>
<!--end of computeroutput.module-->]]>

<![ %database.module; [
<!ENTITY % local.database.attrib "">
<!ELEMENT Database - - ((%cptr.char.mix;)+)>
<!ATTLIST Database
		Class 		(Name
				|Table
				|Field
				|Key1
				|Key2
				|Record)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%local.database.attrib;
>
<!--end of database.module-->]]>

<![ %email.module; [
<!ENTITY % local.email.attrib "">
<!ELEMENT Email - - ((%docinfo.char.mix;)+)>
<!ATTLIST Email
		%common.attrib;
		%local.email.attrib;
>
<!--end of email.module-->]]>

<![ %errorname.module; [
<!ENTITY % local.errorname.attrib "">
<!ELEMENT ErrorName - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ErrorName
		%common.attrib;
		%local.errorname.attrib;
>
<!--end of errorname.module-->]]>

<![ %errortype.module; [
<!ENTITY % local.errortype.attrib "">
<!ELEMENT ErrorType - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ErrorType
		%common.attrib;
		%local.errortype.attrib;
>
<!--end of errortype.module-->]]>

<![ %filename.module; [
<!ENTITY % local.filename.attrib "">
<!ELEMENT Filename - - ((%cptr.char.mix;)+)>
<!ATTLIST Filename
		Class		(HeaderFile
				|SymLink
				|Directory)	#IMPLIED

		--Path: search path (possibly system-specific) in which 
		file can be found--
		Path		CDATA		#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%local.filename.attrib;
>
<!--end of filename.module-->]]>

<![ %function.module; [
<!ENTITY % local.function.attrib "">
<!ELEMENT Function - - ((%cptr.char.mix;)+)>
<!ATTLIST Function
		%moreinfo.attrib;
		%common.attrib;
		%local.function.attrib;
>
<!--end of function.module-->]]>

<![ %guibutton.module; [
<!ENTITY % local.guibutton.attrib "">
<!ELEMENT GUIButton - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIButton
		%moreinfo.attrib;
		%common.attrib;
		%local.guibutton.attrib;
>
<!--end of guibutton.module-->]]>

<![ %guiicon.module; [
<!ENTITY % local.guiicon.attrib "">
<!ELEMENT GUIIcon - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIIcon
		%moreinfo.attrib;
		%common.attrib;
		%local.guiicon.attrib;
>
<!--end of guiicon.module-->]]>

<![ %guilabel.module; [
<!ENTITY % local.guilabel.attrib "">
<!ELEMENT GUILabel - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUILabel
		%moreinfo.attrib;
		%common.attrib;
		%local.guilabel.attrib;
>
<!--end of guilabel.module-->]]>

<![ %guimenu.module; [
<!ENTITY % local.guimenu.attrib "">
<!ELEMENT GUIMenu - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIMenu
		%moreinfo.attrib;
		%common.attrib;
		%local.guimenu.attrib;
>
<!--end of guimenu.module-->]]>

<![ %guimenuitem.module; [
<!ENTITY % local.guimenuitem.attrib "">
<!ELEMENT GUIMenuItem - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIMenuItem
		%moreinfo.attrib;
		%common.attrib;
		%local.guimenuitem.attrib;
>
<!--end of guimenuitem.module-->]]>

<![ %guisubmenu.module; [
<!ENTITY % local.guisubmenu.attrib "">
<!ELEMENT GUISubmenu - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUISubmenu
		%moreinfo.attrib;
		%common.attrib;
		%local.guisubmenu.attrib;
>
<!--end of guisubmenu.module-->]]>

<![ %hardware.module; [
<!ENTITY % local.hardware.attrib "">
<!ELEMENT Hardware - - ((%cptr.char.mix;)+)>
<!ATTLIST Hardware
		%moreinfo.attrib;
		%common.attrib;
		%local.hardware.attrib;
>
<!--end of hardware.module-->]]>

<![ %interface.module; [
<!--FUTURE USE (V4.0):
......................
Interface will no longer have a Class attribute; if you want to subclass
interface information, use GUIButton, GUIIcon, GUILabel, GUIMenu,
GUIMenuItem, or GUISubmenu, or use a Role value on Interface.
......................
-->
<!ENTITY % local.interface.attrib "">
<!ELEMENT Interface - - ((%cptr.char.mix;|Accel)+)>
<!ATTLIST Interface
		Class 		(Button
				|Icon
				|Menu
				|MenuItem)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%local.interface.attrib;
>
<!--end of interface.module-->]]>

<![ %interfacedefinition.module; [
<!ENTITY % local.interfacedefinition.attrib "">
<!ELEMENT InterfaceDefinition - - ((%cptr.char.mix;)+)>
<!ATTLIST InterfaceDefinition
		%moreinfo.attrib;
		%common.attrib;
		%local.interfacedefinition.attrib;
>
<!--end of interfacedefinition.module-->]]>

<![ %keycap.module; [
<!ENTITY % local.keycap.attrib "">
<!ELEMENT KeyCap - - ((%cptr.char.mix;)+)>
<!ATTLIST KeyCap
		%moreinfo.attrib;
		%common.attrib;
		%local.keycap.attrib;
>
<!--end of keycap.module-->]]>

<![ %keycode.module; [
<!ENTITY % local.keycode.attrib "">
<!ELEMENT KeyCode - - ((%smallcptr.char.mix;)+)>
<!ATTLIST KeyCode
		%common.attrib;
		%local.keycode.attrib;
>
<!--end of keycode.module-->]]>

<![ %keycombo.module; [
<!ENTITY % local.keycombo.attrib "">
<!ELEMENT KeyCombo - - ((KeyCap|KeyCombo|KeySym|MouseButton)+)>
<!ATTLIST KeyCombo
		%keyaction.attrib;
		%moreinfo.attrib;
		%common.attrib;
		%local.keycombo.attrib;
>
<!--end of keycombo.module-->]]>

<![ %keysym.module; [
<!ENTITY % local.keysym.attrib "">
<!ELEMENT KeySym - - ((%smallcptr.char.mix;)+)>
<!ATTLIST KeySym
		%common.attrib;
		%local.keysym.attrib;
>
<!--end of keysym.module-->]]>

<![ %literal.module; [
<!ENTITY % local.literal.attrib "">
<!ELEMENT Literal - - ((%cptr.char.mix;)+)>
<!ATTLIST Literal
		%moreinfo.attrib;
		%common.attrib;
		%local.literal.attrib;
>
<!--end of literal.module-->]]>

<![ %medialabel.module; [
<!ENTITY % local.medialabel.attrib "">
<!ELEMENT MediaLabel - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MediaLabel
		Class 		(Cartridge
				|CDRom
				|Disk
				|Tape)		#IMPLIED
		%common.attrib;
		%local.medialabel.attrib;
>
<!--end of medialabel.module-->]]>

<![ %menuchoice.content.module; [
<![ %menuchoice.module; [
<!ENTITY % local.menuchoice.attrib "">
<!ELEMENT MenuChoice - - (Shortcut?, (GUIButton|GUIIcon|GUILabel
		|GUIMenu|GUIMenuItem|GUISubmenu|Interface)+)>
<!ATTLIST MenuChoice
		%moreinfo.attrib;
		%common.attrib;
		%local.menuchoice.attrib;
>
<!--end of menuchoice.module-->]]>

<![ %shortcut.module; [
<!-- See also KeyCombo -->
<!ENTITY % local.shortcut.attrib "">
<!ELEMENT Shortcut - - ((KeyCap|KeyCombo|KeySym|MouseButton)+)>
<!ATTLIST Shortcut
		%keyaction.attrib;
		%moreinfo.attrib;
		%common.attrib;
>
<!--end of shortcut.module-->]]>
<!--end of menuchoice.content.module-->]]>

<![ %mousebutton.module; [
<!ENTITY % local.mousebutton.attrib "">
<!ELEMENT MouseButton - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MouseButton
		%moreinfo.attrib;
		%common.attrib;
		%local.mousebutton.attrib;
>
<!--end of mousebutton.module-->]]>

<![ %msgtext.module; [
<!ENTITY % local.msgtext.attrib "">
<!ELEMENT MsgText - - ((%component.mix;)+)>
<!ATTLIST MsgText
		%common.attrib;
		%local.msgtext.attrib;
>
<!--end of msgtext.module-->]]>

<![ %option.module; [
<!ENTITY % local.option.attrib "">
<!ELEMENT Option - - ((%cptr.char.mix;)+)>
<!ATTLIST Option
		%common.attrib;
		%local.option.attrib;
>
<!--end of option.module-->]]>

<![ %optional.module; [
<!ENTITY % local.optional.attrib "">
<!ELEMENT Optional - - ((%cptr.char.mix;)+)>
<!ATTLIST Optional
		%common.attrib;
		%local.optional.attrib;
>
<!--end of optional.module-->]]>

<![ %parameter.module; [
<!ENTITY % local.parameter.attrib "">
<!ELEMENT Parameter - - ((%cptr.char.mix;)+)>
<!ATTLIST Parameter
		Class 		(Command
				|Function
				|Option)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%local.parameter.attrib;
>
<!--end of parameter.module-->]]>

<![ %property.module; [
<!ENTITY % local.property.attrib "">
<!ELEMENT Property - - ((%cptr.char.mix;)+)>
<!ATTLIST Property
		%moreinfo.attrib;
		%common.attrib;
		%local.property.attrib;
>
<!--end of property.module-->]]>

<![ %replaceable.module; [
<!ENTITY % local.replaceable.attrib "">
<!ELEMENT Replaceable - - ((#PCDATA 
		| %link.char.class; 
		| Optional
		| %base.char.class; 
		| %other.char.class; 
		| InlineGraphic)+)>
<!ATTLIST Replaceable
		Class		(Command
				|Function
				|Option
				|Parameter)	#IMPLIED
		%common.attrib;
		%local.replaceable.attrib;
>
<!--end of replaceable.module-->]]>

<![ %returnvalue.module; [
<!ENTITY % local.returnvalue.attrib "">
<!ELEMENT ReturnValue - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ReturnValue
		%common.attrib;
		%local.returnvalue.attrib;
>
<!--end of returnvalue.module-->]]>

<![ %structfield.module; [
<!ENTITY % local.structfield.attrib "">
<!ELEMENT StructField - - ((%smallcptr.char.mix;)+)>
<!ATTLIST StructField
		%common.attrib;
		%local.structfield.attrib;
>
<!--end of structfield.module-->]]>

<![ %structname.module; [
<!ENTITY % local.structname.attrib "">
<!ELEMENT StructName - - ((%smallcptr.char.mix;)+)>
<!ATTLIST StructName
		%common.attrib;
		%local.structname.attrib;
>
<!--end of structname.module-->]]>

<![ %symbol.module; [
<!ENTITY % local.symbol.attrib "">
<!ELEMENT Symbol - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Symbol
		Class		(Limit)		#IMPLIED
		%common.attrib;
		%local.symbol.attrib;
>
<!--end of symbol.module-->]]>

<![ %systemitem.module; [
<!ENTITY % local.systemitem.attrib "">
<!ELEMENT SystemItem - - ((%cptr.char.mix;)+)>
<!ATTLIST SystemItem
		Class		(Constant
				|EnvironVar
				|Macro
				|OSname
				|Prompt
				|Resource
				|SystemName)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%local.systemitem.attrib;
>
<!--end of systemitem.module-->]]>


<![ %token.module; [
<!ENTITY % local.token.attrib "">
<!ELEMENT Token - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Token
		%common.attrib;
		%local.token.attrib;
>
<!--end of token.module-->]]>

<![ %type.module; [
<!ENTITY % local.type.attrib "">
<!ELEMENT Type - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Type
		%common.attrib;
		%local.type.attrib;
>
<!--end of type.module-->]]>

<![ %userinput.module; [
<!ENTITY % local.userinput.attrib "">
<!ELEMENT UserInput - - ((%cptr.char.mix;)+)>
<!ATTLIST UserInput
		%moreinfo.attrib;
		%common.attrib;
		%local.userinput.attrib;
>
<!--end of userinput.module-->]]>

<!-- Words ................................................................ -->

<![ %abbrev.module; [
<!ENTITY % local.abbrev.attrib "">
<!ELEMENT Abbrev - - ((%word.char.mix;)+)>
<!ATTLIST Abbrev
		%common.attrib;
		%local.abbrev.attrib;
>
<!--end of abbrev.module-->]]>

<![ %acronym.module; [
<!ENTITY % local.acronym.attrib "">
<!ELEMENT Acronym - - ((%word.char.mix;)+)>
<!ATTLIST Acronym
		%common.attrib;
		%local.acronym.attrib;
>
<!--end of acronym.module-->]]>

<![ %citation.module; [
<!ENTITY % local.citation.attrib "">
<!ELEMENT Citation - - ((%para.char.mix;)+)>
<!ATTLIST Citation
		%common.attrib;
		%local.citation.attrib;
>
<!--end of citation.module-->]]>

<![ %citerefentry.content.module; [
<![ %citerefentry.module; [
<!ENTITY % local.citerefentry.attrib "">
<!ELEMENT CiteRefEntry - - (RefEntryTitle, ManVolNum?)>
<!ATTLIST CiteRefEntry
		%common.attrib;
		%local.citerefentry.attrib;
>
<!--end of citerefentry.module-->]]>

  <![ %refentrytitle.module; [
  <!ENTITY % local.refentrytitle.attrib "">
  <!ELEMENT RefEntryTitle - O ((%para.char.mix;)+)>
  <!ATTLIST RefEntryTitle
		%common.attrib;
		%local.refentrytitle.attrib;
>
  <!--end of refentrytitle.module-->]]>

  <![ %manvolnum.module; [
  <!ENTITY % local.manvolnum.attrib "">
  <!ELEMENT ManVolNum - O ((%word.char.mix;)+)>
  <!ATTLIST ManVolNum
		%common.attrib;
		%local.manvolnum.attrib;
>
  <!--end of manvolnum.module-->]]>
<!--end of citerefentry.content.module-->]]>

<![ %citetitle.module; [
<!ENTITY % local.citetitle.attrib "">
<!ELEMENT CiteTitle - - ((%para.char.mix;)+)>
<!ATTLIST CiteTitle
		--Pubwork: type of published work being cited--
		Pubwork		(Article
				|Book
				|Chapter
				|Part
				|RefEntry
				|Section)	#IMPLIED
		%common.attrib;
		%local.citetitle.attrib;
>
<!--end of citetitle.module-->]]>

<![ %co.module; [
<!ENTITY % local.co.attrib "">
<!-- CO is a callout area of the LineColumn unit type (a single character 
     position); the position is directly indicated by the location of CO. -->
<!ELEMENT CO - O EMPTY>
<!ATTLIST CO
		%label.attrib; --bug number/symbol override or initialization--
		%linkends.attrib; --to any related information--
		%idreq.common.attrib;
		%local.co.attrib;
>
<!--end of co.module-->]]>

<![ %emphasis.module; [
<!ENTITY % local.emphasis.attrib "">
<!ELEMENT Emphasis - - ((%para.char.mix;)+)>
<!ATTLIST Emphasis
		%common.attrib;
		%local.emphasis.attrib;
>
<!--end of emphasis.module-->]]>

<![ %firstterm.module; [
<!ENTITY % local.firstterm.attrib "">
<!ELEMENT FirstTerm - - ((%word.char.mix;)+)>
<!ATTLIST FirstTerm
		%linkend.attrib; --to GlossEntry or other explanation--
		%common.attrib;
		%local.firstterm.attrib;
>
<!--end of firstterm.module-->]]>

<![ %foreignphrase.module; [
<!ENTITY % local.foreignphrase.attrib "">
<!ELEMENT ForeignPhrase - - ((%para.char.mix;)+)>
<!ATTLIST ForeignPhrase
		%common.attrib;
		%local.foreignphrase.attrib;
>
<!--end of foreignphrase.module-->]]>

<![ %glossterm.module; [
<!ENTITY % local.glossterm.attrib "">
<!ELEMENT GlossTerm - O ((%para.char.mix;)+)>
<!ATTLIST GlossTerm
		%linkend.attrib; --to GlossEntry if Glossterm used in text--

		--BaseForm: the form of the term in GlossEntry when this
		GlossTerm is used in text in alternate form (e.g. plural),
		for doing automatic linking--
		BaseForm	CDATA		#IMPLIED
		%common.attrib;
		%local.glossterm.attrib;
>
<!--end of glossterm.module-->]]>

<![ %lineannotation.module; [
<!ENTITY % local.lineannotation.attrib "">
<!ELEMENT LineAnnotation - - ((%para.char.mix;)+)>
<!ATTLIST LineAnnotation
		%common.attrib;
		%local.lineannotation.attrib;
>
<!--end of lineannotation.module-->]]>

<![ %markup.module; [
<!ENTITY % local.markup.attrib "">
<!ELEMENT Markup - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Markup
		%common.attrib;
		%local.markup.attrib;
>
<!--end of markup.module-->]]>

<![ %phrase.module; [
<!ENTITY % local.phrase.attrib "">
<!ELEMENT Phrase - - ((%para.char.mix;)+)>
<!ATTLIST Phrase
		%common.attrib;
		%local.phrase.attrib;
>
<!--end of phrase.module-->]]>

<![ %quote.module; [
<!ENTITY % local.quote.attrib "">
<!ELEMENT Quote - - ((%para.char.mix;)+)>
<!ATTLIST Quote
		%common.attrib;
		%local.quote.attrib;
>
<!--end of quote.module-->]]>

<![ %sgmltag.module; [
<!ENTITY % local.sgmltag.attrib "">
<!ELEMENT SGMLTag - - ((%smallcptr.char.mix;)+)>
<!ATTLIST SGMLTag
		Class 		(Attribute
				|AttValue
				|Element
				|EndTag
				|GenEntity
				|ParamEntity
				|PI
				|StartTag
				|SGMLComment)	#IMPLIED
		%common.attrib;
		%local.sgmltag.attrib;
>
<!--end of sgmltag.module-->]]>

<![ %ssscript.module; [
<!ENTITY % local.ssscript.attrib "">
<!ELEMENT (Subscript | Superscript) - - ((#PCDATA 
		| %link.char.class;
		| Replaceable 
		| Symbol 
		| InlineGraphic 
		| %base.char.class; 
		| %other.char.class;)+)
		-(%ubiq.mix;)>
<!ATTLIST (Subscript | Superscript)
		%common.attrib;
		%local.ssscript.attrib;
>
<!--end of ssscript.module-->]]>

<![ %trademark.module; [
<!ENTITY % local.trademark.attrib "">
<!ELEMENT Trademark - - ((#PCDATA 
		| %link.char.class; 
		| %cptr.char.class;
		| %base.char.class; 
		| %other.char.class; 
		| InlineGraphic
		| Emphasis)+)>
<!ATTLIST Trademark
		Class		(Service
				|Trade
				|Registered
				|Copyright)	Trade
		%common.attrib;
		%local.trademark.attrib;
>
<!--end of trademark.module-->]]>

<![ %wordasword.module; [
<!ENTITY % local.wordasword.attrib "">
<!ELEMENT WordAsWord - - ((%word.char.mix;)+)>
<!ATTLIST WordAsWord
		%common.attrib;
		%local.wordasword.attrib;
>
<!--end of wordasword.module-->]]>

<!-- Links and cross-references ........................................... -->

<![ %link.module; [
<!ENTITY % local.link.attrib "">
<!ELEMENT Link - - ((%para.char.mix;)+)>
<!ATTLIST Link
                --Endterm: pointer to description of linked-to object--
                Endterm		IDREF		#IMPLIED

		%linkendreq.attrib; --to linked-to object--

                --Type: user-defined role of link--
                Type            CDATA           #IMPLIED
		%common.attrib;
		%local.link.attrib;
>
<!--end of link.module-->]]>

<![ %olink.module; [
<!ENTITY % local.olink.attrib "">
<!ELEMENT OLink - - ((%para.char.mix;)+)>
<!ATTLIST OLink
                --TargetDocEnt: HyTimeish Docorsub pointer--
		TargetDocEnt	ENTITY 		#IMPLIED

                --LinkMode: points to a ModeSpec containing app-specific info--
		LinkMode	IDREF		#IMPLIED
		LocalInfo 	CDATA		#IMPLIED

                --Type: user-defined role of link--
		Type		CDATA		#IMPLIED
		%common.attrib;
		%local.olink.attrib;
>
<!--end of olink.module-->]]>

<![ %ulink.module; [
<!ENTITY % local.ulink.attrib "">
<!ELEMENT ULink - - ((%para.char.mix;)+)>
<!ATTLIST ULink
                --URL: uniform resource locator--
                URL		CDATA           #REQUIRED

                --Type: user-defined role of link--
                Type            CDATA           #IMPLIED
		%common.attrib;
		%local.ulink.attrib;
>
<!--end of ulink.module-->]]>

<![ %footnoteref.module; [

<!--FUTURE USE (V3.0):
......................
FootnoteRef will be a declared-empty element, which means you will
have to use the new Label attribute rather than element content to 
supply a mark.

<!ELEMENT FootnoteRef - O EMPTY>
......................
-->

<!ENTITY % local.footnoteref.attrib "">
<!ELEMENT FootnoteRef - - (#PCDATA) -(%ubiq.mix;)>
<!ATTLIST FootnoteRef
		%linkendreq.attrib; --to footnote content already supplied--

		--FUTURE USE (V3.0):
		....................
		Mark will be renamed to Label
		....................--

		--Mark: symbol (e.g. dagger) for use in pointing to
		footnote in text; default is whatever was used
		in original footnote being referenced--
                Mark		CDATA		#IMPLIED
		%common.attrib;
		%local.footnoteref.attrib;
>
<!--end of footnoteref.module-->]]>

<![ %xref.module; [
<!ENTITY % local.xref.attrib "">
<!ELEMENT XRef - O  EMPTY>
<!ATTLIST XRef
                --Endterm: pointer to description of linked-to object--
		Endterm		IDREF		#IMPLIED

		%linkendreq.attrib; --to linked-to object--
		%common.attrib;
		%local.xref.attrib;
>
<!--end of xref.module-->]]>

<!-- Ubiquitous elements .................................................. -->

<![ %anchor.module; [
<!ENTITY % local.anchor.attrib "">
<!ELEMENT Anchor - O  EMPTY>
<!ATTLIST Anchor
		%idreq.attrib; -- required --
		%pagenum.attrib; --replaces Lang --
		%remap.attrib;
		%role.attrib;
		%xreflabel.attrib;
		%revisionflag.attrib;
		%effectivity.attrib;
		%local.anchor.attrib;
>
<!--end of anchor.module-->]]>

<![ %beginpage.module; [
<!ENTITY % local.beginpage.attrib "">
<!ELEMENT BeginPage - O  EMPTY>
<!ATTLIST BeginPage
		--PageNum: number of page that begins at this point--
		%pagenum.attrib;
		%common.attrib;
		%local.beginpage.attrib;
>
<!--end of beginpage.module-->]]>

<!-- IndexTerms appear in the text flow for generating or linking an
     index. -->

<![ %indexterm.content.module; [
<![ %indexterm.module; [
<!ENTITY % local.indexterm.attrib "">
<!ELEMENT IndexTerm - O (Primary, ((Secondary, ((Tertiary, (See|SeeAlso+)?)
		| See | SeeAlso+)?) | See | SeeAlso+)?) -(%ubiq.mix;)>
<!ATTLIST IndexTerm
		%pagenum.attrib;

		--Scope: indexing applies to this doc (Local), whole doc
		set (Global), or both (All)--
		Scope		(All
				|Global
				|Local)		#IMPLIED

		--Significance: whether term is best source of info for
		this topic (Preferred) or not (Normal)--
		Significance	(Preferred
				|Normal)	Normal

		--FUTURE USE (V3.0):
		....................
		Class: indicates type of IndexTerm; default is Singular, 
		or EndOfRange if StartRef is supplied; StartOfRange value 
		must be supplied explicitly on starts of ranges

		Class		(Singular
				|StartOfRange
				|EndOfRange)	#IMPLIED
		....................--

		--FUTURE USE (V3.0):
		....................
		SpanEnd will be renamed to StartRef
		....................--

		--SpanEnd: points to the IndexTerm that starts
		the indexing range ended by this IndexTerm--
		SpanEnd		IDREF		#CONREF

		--Zone: points to elements where IndexTerms originated;
		for use if IndexTerms are assembled together in source
		instance--
		Zone		IDREFS		#IMPLIED
		%common.attrib;
		%local.indexterm.attrib;
>
<!--end of indexterm.module-->]]>

<![ %primsecter.module; [
<!ENTITY % local.primsecter.attrib "">
<!ELEMENT (Primary | Secondary | Tertiary) - O ((%ndxterm.char.mix;)+)>
<!ATTLIST (Primary | Secondary | Tertiary)
		--SortAs: alternate sort string for index sorting--
		SortAs		CDATA		#IMPLIED
		%common.attrib;
		%local.primsecter.attrib;
>
<!--end of primsecter.module-->]]>

<![ %seeseealso.module; [
<!ENTITY % local.seeseealso.attrib "">
<!ELEMENT (See | SeeAlso) - O ((%ndxterm.char.mix;)+)>
<!ATTLIST (See | SeeAlso)
		%common.attrib;
		%local.seeseealso.attrib;
>
<!--end of seeseealso.module-->]]>
<!--end of indexterm.content.module-->]]>

<!-- End of DocBook information pool module V2.4.1 ........................ -->
<!-- ...................................................................... -->
