<!-- ...................................................................... -->
<!-- DocBook information pool module V3.0 ................................. -->
<!-- File dbpool.mod ...................................................... -->

<!-- Copyright 1992, 1993, 1994, 1995, 1996 HaL Computer Systems, Inc., 
     O'Reilly & Associates, Inc., ArborText, Inc., and Fujitsu Software
     Corporation.

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

     o Terry Allen, Fujitsu Software Corporation
       3055 Orchard Drive, San Jose, CA 95134
       <tallen@fsc.fujitsu.com>

     o Eve Maler, ArborText Inc.
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
     "-//Davenport//ELEMENTS DocBook Information Pool V3.0//EN">
     %dbpool;

     See the documentation for detailed information on the parameter
     entity and module scheme used in DocBook, customizing DocBook and
     planning for interchange, and changes made since the last release
     of DocBook.
-->

<!-- ...................................................................... -->
<!-- General-purpose semantics entities ................................... -->

<!ENTITY % yesorno.attvals	"NUMBER">
<!ENTITY % yes.attval		"1">
<!ENTITY % no.attval		"0">

<!-- ...................................................................... -->
<!-- Entities for module inclusions ....................................... -->

<!ENTITY % dbpool.redecl.module "IGNORE">

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

<!ENTITY % local.gen.char.class "">
<!ENTITY % gen.char.class
		"Abbrev|Acronym|Citation|CiteRefEntry|CiteTitle|Emphasis
		|FirstTerm|ForeignPhrase|GlossTerm|Footnote|Phrase
		|Quote|Trademark|WordAsWord %local.gen.char.class;">

<!ENTITY % local.link.char.class "">
<!ENTITY % link.char.class
		"Link|OLink|ULink %local.link.char.class;">

<!ENTITY % local.tech.char.class "">
<!--FUTURE USE (V4.0):
......................
MsgText will be removed from tech.char.class to a more appropriate
parameter entity.
......................
-->
<!ENTITY % tech.char.class
		"Action|Application|ClassName|Command|ComputerOutput
		|Database|Email|EnVar|ErrorCode|ErrorName|ErrorType|Filename
		|Function|GUIButton|GUIIcon|GUILabel|GUIMenu|GUIMenuItem
		|GUISubmenu|Hardware|Interface|InterfaceDefinition|KeyCap
		|KeyCode|KeyCombo|KeySym|Literal|Markup|MediaLabel|MenuChoice
		|MouseButton|MsgText|Option|Optional|Parameter|Prompt|Property
		|Replaceable|ReturnValue|SGMLTag|StructField|StructName
		|Symbol|SystemItem|Token|Type|UserInput
		%local.tech.char.class;">

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
docinfo.char.mix      X         d         b              X    a

a. Just InlineGraphic; no InlineEquation.
b. Just Replaceable; no other computer terms.
c. Just Emphasis and Trademark; no other word elements.
d. Just Acronym, Emphasis, and Trademark; no other word elements.
-->

<!-- Note that synop.class is not usually used for *.char.mixes,
     but is used here because synopses used inside paragraph
     contexts are "inline" synopses -->
<!ENTITY % local.para.char.mix "">
<!ENTITY % para.char.mix
		"#PCDATA
		|%xref.char.class;	|%gen.char.class;
		|%link.char.class;	|%tech.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;	|%inlineobj.char.class;
		|%synop.class;
		%local.para.char.mix;">

<!ENTITY % local.title.char.mix "">
<!ENTITY % title.char.mix
		"#PCDATA
		|%xref.char.class;	|%gen.char.class;
		|%link.char.class;	|%tech.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;	|%inlineobj.char.class;
		%local.title.char.mix;">

<!ENTITY % local.ndxterm.char.mix "">
<!ENTITY % ndxterm.char.mix
		"#PCDATA
		|%xref.char.class;	|%gen.char.class;
		|%link.char.class;	|%tech.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;	|InlineGraphic
		%local.ndxterm.char.mix;">

<!ENTITY % local.cptr.char.mix "">
<!ENTITY % cptr.char.mix
		"#PCDATA
		|%link.char.class;	|%tech.char.class;
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
					|Acronym|Emphasis|Trademark
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
<!--ENTITY % bibliocomponent.mix (see Bibliographic section, below)-->
<!--ENTITY % person.ident.mix (see Bibliographic section, below)-->

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

<!ENTITY % arch.attrib
	--Arch: Computer or chip architecture to which element applies; no 
	default--
	"Arch		CDATA		#IMPLIED">

<!ENTITY % conformance.attrib
	--Conformance: Standards conformance characteristics--
	"Conformance	NMTOKENS	#IMPLIED">

<!ENTITY % os.attrib
	--OS: Operating system to which element applies; no default--
	"OS		CDATA		#IMPLIED">

<!ENTITY % revision.attrib
	--Revision: Editorial revision to which element belongs; no default--
	"Revision	CDATA		#IMPLIED">

<!ENTITY % userlevel.attrib
	--UserLevel: Level of user experience to which element applies; no 
	default--
	"UserLevel	CDATA		#IMPLIED">

<!ENTITY % vendor.attrib
	--Vendor: Computer vendor to which element applies; no default--
	"Vendor		CDATA		#IMPLIED">

<!ENTITY % local.effectivity.attrib "">
<!ENTITY % effectivity.attrib
	"%arch.attrib;
	%conformance.attrib;
	%os.attrib;
	%revision.attrib;
	%userlevel.attrib;
	%vendor.attrib;
	%local.effectivity.attrib;"
>

<!-- Common attributes .................................................... -->

<!ENTITY % id.attrib
	--Id: Unique identifier of element; no default--
	"Id		ID		#IMPLIED">

<!ENTITY % idreq.attrib
	--Id: Unique identifier of element; a value must be supplied; no 
	default--
	"Id		ID		#REQUIRED">

<!ENTITY % lang.attrib
	--Lang: Indicator of language in which element is written, for
	translation, character set management, etc.; no default--
	"Lang		CDATA		#IMPLIED">

<!ENTITY % remap.attrib
	--Remap: Previous role of element before conversion; no default--
	"Remap		CDATA		#IMPLIED">

<!ENTITY % role.attrib
	--Role: New role of element in local environment; no default--
	"Role		CDATA		#IMPLIED">

<!ENTITY % xreflabel.attrib
	--XRefLabel: Alternate labeling string for XRef text generation;
	default is usually title or other appropriate label text already
	contained in element--
	"XRefLabel	CDATA		#IMPLIED">

<!ENTITY % revisionflag.attrib
	--RevisionFlag: Revision status of element; default is that element
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
	--Role is included explicitly on each element--
	%xreflabel.attrib;
	%revisionflag.attrib;
	%effectivity.attrib;
	%local.common.attrib;"
>

<!ENTITY % idreq.common.attrib
	"%idreq.attrib;
	%lang.attrib;
	%remap.attrib;
	--Role is included explicitly on each element--
	%xreflabel.attrib;
	%revisionflag.attrib;
	%effectivity.attrib;
	%local.common.attrib;"
>

<!-- Semi-common attributes and other attribute entities .................. -->

<!ENTITY % local.graphics.attrib "">
<!ENTITY % graphics.attrib
	"
	--EntityRef: Name of an external entity containing the content
	of the graphic--
	EntityRef	ENTITY		#IMPLIED

	--FileRef: Filename, qualified by a pathname if desired, 
	designating the file containing the content of the graphic--
	FileRef 	CDATA		#IMPLIED

	--Format: Notation of the element content, if any--
	Format		NOTATION
			(%notation.class;)
					#IMPLIED

	--SrcCredit: Information about the source of the Graphic--
	SrcCredit	CDATA		#IMPLIED

	--Width: Same as CALS reprowid (desired width)--
	Width		NUTOKEN		#IMPLIED

	--Depth: Same as CALS reprodep (desired depth)--
	Depth		NUTOKEN		#IMPLIED

	--Align: Same as CALS hplace with 'none' removed; #IMPLIED means 
	application-specific--
	Align		(Left
			|Right 
			|Center)	#IMPLIED

	--Scale: Conflation of CALS hscale and vscale--
	Scale		NUMBER		#IMPLIED

	--Scalefit: Same as CALS scalefit--
	Scalefit	%yesorno.attvals;
					#IMPLIED
	%local.graphics.attrib;"
>

<!ENTITY % local.keyaction.attrib "">
<!ENTITY % keyaction.attrib
	"
	--Action: Key combination type; default is unspecified if one 
	child element, Simul if there is more than one; if value is 
	Other, the OtherAction attribute must have a nonempty value--
	Action		(Click
			|Double-Click
			|Press
			|Seq
			|Simul
			|Other)		#IMPLIED

	--OtherAction: User-defined key combination type--
	OtherAction	CDATA		#IMPLIED"
>

<!ENTITY % label.attrib
	--Label: Identifying number or string; default is usually the
	appropriate number or string autogenerated by a formatter--
	"Label		CDATA		#IMPLIED">

<!ENTITY % linespecific.attrib
	--Format: whether element is assumed to contain significant white
	space--
	"Format		NOTATION
			(linespecific)	linespecific">

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

<!ENTITY % local.mark.attrib "">
<!ENTITY % mark.attrib
	"Mark		CDATA		#IMPLIED
	%local.mark.attrib;"
>

<!ENTITY % moreinfo.attrib
	--MoreInfo: whether element's content has an associated RefEntry--
	"MoreInfo	(RefEntry|None)	None">

<!ENTITY % pagenum.attrib
	--Pagenum: number of page on which element appears; no default--
	"Pagenum	CDATA		#IMPLIED">

<!ENTITY % local.status.attrib "">
<!ENTITY % status.attrib
	--Status: Editorial or publication status of the element
	it applies to, such as "in review" or "approved for distribution"--
	"Status		CDATA		#IMPLIED
	%local.status.attrib;"
>

<!ENTITY % width.attrib
	--Width: width of the longest line in the element to which it
	pertains, in number of characters--
	"Width		NUMBER		#IMPLIED">

<!-- ...................................................................... -->
<!-- Title elements ....................................................... -->

<!ENTITY % title.module "INCLUDE">
<![ %title.module; [
<!ENTITY % local.title.attrib "">
<!ENTITY % title.role.attrib "%role.attrib;">
<!ELEMENT Title - O ((%title.char.mix;)+)>
<!ATTLIST Title
		%pagenum.attrib;
		%common.attrib;
		%title.role.attrib;
		%local.title.attrib;
>
<!--end of title.module-->]]>

<!ENTITY % titleabbrev.module "INCLUDE">
<![ %titleabbrev.module; [
<!ENTITY % local.titleabbrev.attrib "">
<!ENTITY % titleabbrev.role.attrib "%role.attrib;">
<!ELEMENT TitleAbbrev - O ((%title.char.mix;)+)>
<!ATTLIST TitleAbbrev
		%common.attrib;
		%titleabbrev.role.attrib;
		%local.titleabbrev.attrib;
>
<!--end of titleabbrev.module-->]]>

<!ENTITY % subtitle.module "INCLUDE">
<![ %subtitle.module; [
<!ENTITY % local.subtitle.attrib "">
<!ENTITY % subtitle.role.attrib "%role.attrib;">
<!ELEMENT Subtitle - O ((%title.char.mix;)+)>
<!ATTLIST Subtitle
		%common.attrib;
		%subtitle.role.attrib;
		%local.subtitle.attrib;
>
<!--end of subtitle.module-->]]>

<!-- ...................................................................... -->
<!-- Bibliographic entities and elements .................................. -->

<!-- The bibliographic elements are typically used in the document
     hierarchy. They do not appear in content models of information
     pool elements.  See also the document information elements,
     below. -->

<!ENTITY % local.person.ident.mix "">
<!--FUTURE USE (V4.0):
......................
AuthorBlurb and Affiliation will be removed from %person.ident.mix; and a new
wrapper element created to allow association of those two elements with
Author name information.
......................
-->
<!ENTITY % person.ident.mix
		"Honorific|FirstName|Surname|Lineage|OtherName|Affiliation
		|AuthorBlurb|Contrib %local.person.ident.mix;">

<!ENTITY % local.bibliocomponent.mix "">
<!ENTITY % bibliocomponent.mix
		"Abbrev|Abstract|Address|ArtPageNums|Author
		|AuthorGroup|AuthorInitials|BiblioMisc|BiblioSet
		|Collab|ConfGroup|ContractNum|ContractSponsor
		|Copyright|CorpAuthor|CorpName|Date|Edition
		|Editor|InvPartNumber|ISBN|ISSN|IssueNum|OrgName
		|OtherCredit|PageNums|PrintHistory|ProductName
		|ProductNumber|PubDate|Publisher|PublisherName
		|PubsNumber|ReleaseInfo|RevHistory|SeriesVolNums
		|Subtitle|Title|TitleAbbrev|VolumeNum
		|%person.ident.mix;
		%local.bibliocomponent.mix;">

<!ENTITY % biblioentry.module "INCLUDE">
<![ %biblioentry.module; [
<!ENTITY % local.biblioentry.attrib "">
<!--FUTURE USE (V4.0):
......................
The ArtHeader element will be renamed to ArticleInfo.
......................
-->
<!ENTITY % biblioentry.role.attrib "%role.attrib;">
<!ELEMENT BiblioEntry - O ((ArtHeader | BookBiblio | SeriesInfo
	| (%bibliocomponent.mix;))+) -(%ubiq.mix;)>
<!ATTLIST BiblioEntry
		%common.attrib;
		%biblioentry.role.attrib;
		%local.biblioentry.attrib;
>
<!--end of biblioentry.module-->]]>

<!ENTITY % bibliomixed.module "INCLUDE">
<![ %bibliomixed.module; [
<!ENTITY % local.bibliomixed.attrib "">
<!ENTITY % bibliomixed.role.attrib "%role.attrib;">
<!ELEMENT BiblioMixed - O ((%bibliocomponent.mix; | BiblioMSet | #PCDATA)+)
	-(%ubiq.mix;)>
<!ATTLIST BiblioMixed
		%common.attrib;
		%bibliomixed.role.attrib;
		%local.biblioentry.attrib;
>
<!--end of bibliomixed.module-->]]>

<!ENTITY % bookbiblio.module "INCLUDE">
<![ %bookbiblio.module; [
<!--FUTURE USE (V4.0):
......................
BookBiblio will be discarded.
......................
-->
<!ENTITY % local.bookbiblio.attrib "">
<!ENTITY % bookbiblio.role.attrib "%role.attrib;">
<!ELEMENT BookBiblio - - ((%bibliocomponent.mix; | SeriesInfo)+) -(%ubiq.mix;)>
<!ATTLIST BookBiblio
		%common.attrib;
		%bookbiblio.role.attrib;
		%local.bookbiblio.attrib;
>
<!--end of bookbiblio.module-->]]>

<!ENTITY % seriesinfo.module "INCLUDE">
<![ %seriesinfo.module; [
<!--FUTURE USE (V4.0):
......................
SeriesInfo *may* be discarded; it has become a special case of BiblioSet.
......................
-->
<!ENTITY % local.seriesinfo.attrib "">
<!ENTITY % seriesinfo.role.attrib "%role.attrib;">
<!ELEMENT SeriesInfo - - ((%bibliocomponent.mix;)+) -(%ubiq.mix;)>
<!ATTLIST SeriesInfo
		%common.attrib;
		%seriesinfo.role.attrib;
		%local.seriesinfo.attrib;
>
<!--end of seriesinfo.module-->]]>

<!ENTITY % artheader.module "INCLUDE">
<![ %artheader.module; [
<!--FUTURE USE (V4.0):
......................
BookBiblio will be discarded and will be removed from ArtHeader, which
will be renamed to ArticleInfo.
......................
-->
<!ENTITY % local.artheader.attrib "">
<!ENTITY % artheader.role.attrib "%role.attrib;">
<!ELEMENT ArtHeader - - ((%bibliocomponent.mix; | BookBiblio)+) -(%ubiq.mix;)>
<!ATTLIST ArtHeader
		%common.attrib;
		%artheader.role.attrib;
		%local.artheader.attrib;
>
<!--end of artheader.module-->]]>

<!ENTITY % biblioset.module "INCLUDE">
<![ %biblioset.module; [
<!ENTITY % local.biblioset.attrib "">
<!ENTITY % biblioset.role.attrib "%role.attrib;">
<!ELEMENT BiblioSet - - ((%bibliocomponent.mix;)+) -(%ubiq.mix;)>
<!ATTLIST BiblioSet
		--
		Relation: Relationship of elements contained within BiblioSet
		--
		Relation	CDATA		#IMPLIED
		%common.attrib;
		%biblioset.role.attrib;
		%local.biblioset.attrib;
>
<!--end of biblioset.module-->]]>

<!ENTITY % bibliomset.module "INCLUDE">
<![ %bibliomset.module; [
<!ENTITY % bibliomset.role.attrib "%role.attrib;">
<!ENTITY % local.bibliomset.attrib "">
<!ELEMENT BiblioMSet - - ((%bibliocomponent.mix; | BiblioMSet | #PCDATA)+)
	-(%ubiq.mix;)>
<!ATTLIST BiblioMSet
		--
		Relation: Relationship of elements contained within BiblioMSet
		--
		Relation	CDATA		#IMPLIED
		%bibliomset.role.attrib;
		%common.attrib;
		%local.bibliomset.attrib;
>
<!--end of bibliomset.module-->]]>

<!ENTITY % bibliomisc.module "INCLUDE">
<![ %bibliomisc.module; [
<!ENTITY % local.bibliomisc.attrib "">
<!ENTITY % bibliomisc.role.attrib "%role.attrib;">
<!ELEMENT BiblioMisc - - ((%para.char.mix;)+)>
<!ATTLIST BiblioMisc
		%common.attrib;
		%bibliomisc.role.attrib;
		%local.bibliomisc.attrib;
>
<!--end of bibliomisc.module-->]]>

<!-- ...................................................................... -->
<!-- Subject, Keyword, and ITermSet elements .............................. -->

<!ENTITY % subjectset.content.module "INCLUDE">
<![ %subjectset.content.module; [
<!ENTITY % subjectset.module "INCLUDE">
<![ %subjectset.module; [
<!ENTITY % local.subjectset.attrib "">
<!ENTITY % subjectset.role.attrib "%role.attrib;">
<!ELEMENT SubjectSet - - (Subject+)>
<!ATTLIST SubjectSet
		--
		Scheme: Controlled vocabulary employed in SubjectTerms
		--
		Scheme		NAME		#IMPLIED
		%common.attrib;
		%subjectset.role.attrib;
		%local.subjectset.attrib;
>
<!--end of subjectset.module-->]]>

<!ENTITY % subject.module "INCLUDE">
<![ %subject.module; [
<!ENTITY % local.subject.attrib "">
<!ENTITY % subject.role.attrib "%role.attrib;">
<!ELEMENT Subject - - (SubjectTerm+)>
<!ATTLIST Subject
		--
		Weight: Ranking of this group of SubjectTerms relative 
		to others, 0 is low, no highest value specified
		--
		Weight		NUMBER		#IMPLIED
		%common.attrib;
		%subject.role.attrib;
		%local.subject.attrib;
>
<!--end of subject.module-->]]>

<!ENTITY % subjectterm.module "INCLUDE">
<![ %subjectterm.module; [
<!ENTITY % local.subjectterm.attrib "">
<!ENTITY % subjectterm.role.attrib "%role.attrib;">
<!ELEMENT SubjectTerm - - (#PCDATA)>
<!ATTLIST SubjectTerm
		%common.attrib;
		%subjectterm.role.attrib;
		%local.subjectterm.attrib;
>
<!--end of subjectterm.module-->]]>
<!--end of subjectset.content.module-->]]>

<!ENTITY % keywordset.content.module "INCLUDE">
<![ %keywordset.content.module; [
<!ENTITY % local.keywordset.attrib "">
<!ENTITY % keywordset.module "INCLUDE">
<![ %keywordset.module; [
<!ENTITY % local.keywordset.attrib "">
<!ENTITY % keywordset.role.attrib "%role.attrib;">
<!ELEMENT KeywordSet - - (Keyword+)>
<!ATTLIST KeywordSet
		%common.attrib;
		%keywordset.role.attrib;
		%local.keywordset.attrib;
>
<!--end of keywordset.module-->]]>

<!ENTITY % keyword.module "INCLUDE">
<![ %keyword.module; [
<!ENTITY % local.keyword.attrib "">
<!ENTITY % keyword.role.attrib "%role.attrib;">
<!ELEMENT Keyword - - (#PCDATA)>
<!ATTLIST Keyword
		%common.attrib;
		%keyword.role.attrib;
		%local.keyword.attrib;
>
<!--end of keyword.module-->]]>
<!--end of keywordset.content.module-->]]>

<!ENTITY % itermset.module "INCLUDE">
<![ %itermset.module; [
<!ENTITY % local.itermset.attrib "">
<!ENTITY % itermset.role.attrib "%role.attrib;">
<!ELEMENT ITermSet - - (IndexTerm+)>
<!ATTLIST ITermSet
		%common.attrib;
		%itermset.role.attrib;
		%local.itermset.attrib;
>
<!--end of itermset.module-->]]>

<!-- ...................................................................... -->
<!-- Compound (section-ish) elements ...................................... -->

<!-- Message set ...................... -->

<!ENTITY % msgset.content.module "INCLUDE">
<![ %msgset.content.module; [
<!ENTITY % msgset.module "INCLUDE">
<![ %msgset.module; [
<!ENTITY % local.msgset.attrib "">
<!ENTITY % msgset.role.attrib "%role.attrib;">
<!ELEMENT MsgSet - - (MsgEntry+)>
<!ATTLIST MsgSet
		%common.attrib;
		%msgset.role.attrib;
		%local.msgset.attrib;
>
<!--end of msgset.module-->]]>

<!ENTITY % msgentry.module "INCLUDE">
<![ %msgentry.module; [
<!ENTITY % local.msgentry.attrib "">
<!ENTITY % msgentry.role.attrib "%role.attrib;">
<!ELEMENT MsgEntry - O (Msg+, MsgInfo?, MsgExplan*)>
<!ATTLIST MsgEntry
		%common.attrib;
		%msgentry.role.attrib;
		%local.msgentry.attrib;
>
<!--end of msgentry.module-->]]>

<!ENTITY % msg.module "INCLUDE">
<![ %msg.module; [
<!ENTITY % local.msg.attrib "">
<!ENTITY % msg.role.attrib "%role.attrib;">
<!ELEMENT Msg - O (Title?, MsgMain, (MsgSub | MsgRel)*)>
<!ATTLIST Msg
		%common.attrib;
		%msg.role.attrib;
		%local.msg.attrib;
>
<!--end of msg.module-->]]>

<!ENTITY % msgmain.module "INCLUDE">
<![ %msgmain.module; [
<!ENTITY % local.msgmain.attrib "">
<!ENTITY % msgmain.role.attrib "%role.attrib;">
<!ELEMENT MsgMain - - (Title?, MsgText)>
<!ATTLIST MsgMain
		%common.attrib;
		%msgmain.role.attrib;
		%local.msgmain.attrib;
>
<!--end of msgmain.module-->]]>

<!ENTITY % msgsub.module "INCLUDE">
<![ %msgsub.module; [
<!ENTITY % local.msgsub.attrib "">
<!ENTITY % msgsub.role.attrib "%role.attrib;">
<!ELEMENT MsgSub - - (Title?, MsgText)>
<!ATTLIST MsgSub
		%common.attrib;
		%msgsub.role.attrib;
		%local.msgsub.attrib;
>
<!--end of msgsub.module-->]]>

<!ENTITY % msgrel.module "INCLUDE">
<![ %msgrel.module; [
<!ENTITY % local.msgrel.attrib "">
<!ENTITY % msgrel.role.attrib "%role.attrib;">
<!ELEMENT MsgRel - - (Title?, MsgText)>
<!ATTLIST MsgRel
		%common.attrib;
		%msgrel.role.attrib;
		%local.msgrel.attrib;
>
<!--end of msgrel.module-->]]>

<!--ELEMENT MsgText (defined in the Inlines section, below)-->

<!ENTITY % msginfo.module "INCLUDE">
<![ %msginfo.module; [
<!ENTITY % local.msginfo.attrib "">
<!ENTITY % msginfo.role.attrib "%role.attrib;">
<!ELEMENT MsgInfo - - ((MsgLevel | MsgOrig | MsgAud)*)>
<!ATTLIST MsgInfo
		%common.attrib;
		%msginfo.role.attrib;
		%local.msginfo.attrib;
>
<!--end of msginfo.module-->]]>

<!ENTITY % msglevel.module "INCLUDE">
<![ %msglevel.module; [
<!ENTITY % local.msglevel.attrib "">
<!ENTITY % msglevel.role.attrib "%role.attrib;">
<!ELEMENT MsgLevel - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MsgLevel
		%common.attrib;
		%msglevel.role.attrib;
		%local.msglevel.attrib;
>
<!--end of msglevel.module-->]]>

<!ENTITY % msgorig.module "INCLUDE">
<![ %msgorig.module; [
<!ENTITY % local.msgorig.attrib "">
<!ENTITY % msgorig.role.attrib "%role.attrib;">
<!ELEMENT MsgOrig - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MsgOrig
		%common.attrib;
		%msgorig.role.attrib;
		%local.msgorig.attrib;
>
<!--end of msgorig.module-->]]>

<!ENTITY % msgaud.module "INCLUDE">
<![ %msgaud.module; [
<!ENTITY % local.msgaud.attrib "">
<!ENTITY % msgaud.role.attrib "%role.attrib;">
<!ELEMENT MsgAud - - ((%para.char.mix;)+)>
<!ATTLIST MsgAud
		%common.attrib;
		%msgaud.role.attrib;
		%local.msgaud.attrib;
>
<!--end of msgaud.module-->]]>

<!ENTITY % msgexplan.module "INCLUDE">
<![ %msgexplan.module; [
<!ENTITY % local.msgexplan.attrib "">
<!ENTITY % msgexplan.role.attrib "%role.attrib;">
<!ELEMENT MsgExplan - - (Title?, (%component.mix;)+)>
<!ATTLIST MsgExplan
		%common.attrib;
		%msgexplan.role.attrib;
		%local.msgexplan.attrib;
>
<!--end of msgexplan.module-->]]>
<!--end of msgset.content.module-->]]>

<!-- Procedure ........................ -->

<!ENTITY % procedure.content.module "INCLUDE">
<![ %procedure.content.module; [
<!ENTITY % procedure.module "INCLUDE">
<![ %procedure.module; [
<!ENTITY % local.procedure.attrib "">
<!ENTITY % procedure.role.attrib "%role.attrib;">
<!ELEMENT Procedure - - ((%formalobject.title.content;)?,
	(%component.mix;)*, Step+)>
<!ATTLIST Procedure
		%common.attrib;
		%procedure.role.attrib;
		%local.procedure.attrib;
>
<!--end of procedure.module-->]]>

<!ENTITY % step.module "INCLUDE">
<![ %step.module; [
<!ENTITY % local.step.attrib "">
<!ENTITY % step.role.attrib "%role.attrib;">
<!ELEMENT Step - O (Title?, (((%component.mix;)+, (SubSteps,
		(%component.mix;)*)?) | (SubSteps, (%component.mix;)*)))>
<!ATTLIST Step
		--
		Performance: Whether the Step must be performed
		--
		Performance	(Optional
				|Required)	Required -- not #REQUIRED! --
		%common.attrib;
		%step.role.attrib;
		%local.step.attrib;
>
<!--end of step.module-->]]>

<!ENTITY % substeps.module "INCLUDE">
<![ %substeps.module; [
<!ENTITY % local.substeps.attrib "">
<!ENTITY % substeps.role.attrib "%role.attrib;">
<!ELEMENT SubSteps - - (Step+)>
<!ATTLIST SubSteps
		--
		Performance: whether entire set of substeps must be performed
		--
		Performance	(Optional
				|Required)	Required -- not #REQUIRED! --
		%common.attrib;
		%substeps.role.attrib;
		%local.substeps.attrib;
>
<!--end of substeps.module-->]]>
<!--end of procedure.content.module-->]]>

<!-- Sidebar .......................... -->

<!ENTITY % sidebar.module "INCLUDE">
<![ %sidebar.module; [
<!ENTITY % local.sidebar.attrib "">
<!ENTITY % sidebar.role.attrib "%role.attrib;">
<!ELEMENT Sidebar - - ((%formalobject.title.content;)?, (%sidebar.mix;)+)>
<!ATTLIST Sidebar
		%common.attrib;
		%sidebar.role.attrib;
		%local.sidebar.attrib;
>
<!--end of sidebar.module-->]]>

<!-- ...................................................................... -->
<!-- Paragraph-related elements ........................................... -->

<!ENTITY % abstract.module "INCLUDE">
<![ %abstract.module; [
<!ENTITY % local.abstract.attrib "">
<!ENTITY % abstract.role.attrib "%role.attrib;">
<!ELEMENT Abstract - - (Title?, (%para.class;)+)>
<!ATTLIST Abstract
		%common.attrib;
		%abstract.role.attrib;
		%local.abstract.attrib;
>
<!--end of abstract.module-->]]>

<!ENTITY % authorblurb.module "INCLUDE">
<![ %authorblurb.module; [
<!ENTITY % local.authorblurb.attrib "">
<!ENTITY % authorblurb.role.attrib "%role.attrib;">
<!ELEMENT AuthorBlurb - - (Title?, (%para.class;)+)>
<!ATTLIST AuthorBlurb
		%common.attrib;
		%authorblurb.role.attrib;
		%local.authorblurb.attrib;
>
<!--end of authorblurb.module-->]]>

<!ENTITY % blockquote.module "INCLUDE">
<![ %blockquote.module; [
<!--FUTURE USE (V4.0):
......................
Epigraph will be disallowed from appearing in BlockQuote.
......................
-->

<!ENTITY % local.blockquote.attrib "">
<!ENTITY % blockquote.role.attrib "%role.attrib;">
<!ELEMENT BlockQuote - - (Title?, Attribution?, (%component.mix;)+)>
<!ATTLIST BlockQuote
		%common.attrib;
		%blockquote.role.attrib;
		%local.blockquote.attrib;
>
<!--end of blockquote.module-->]]>

<!ENTITY % attribution.module "INCLUDE">
<![ %attribution.module; [
<!ENTITY % local.attribution.attrib "">
<!ENTITY % attribution.role.attrib "%role.attrib;">
<!ELEMENT Attribution - O ((%para.char.mix;)+)>
<!ATTLIST Attribution
		%common.attrib;
		%attribution.role.attrib;
		%local.attribution.attrib;
>
<!--end of attribution.module-->]]>

<!ENTITY % bridgehead.module "INCLUDE">
<![ %bridgehead.module; [
<!ENTITY % local.bridgehead.attrib "">
<!ENTITY % bridgehead.role.attrib "%role.attrib;">
<!ELEMENT BridgeHead - - ((%title.char.mix;)+)>
<!ATTLIST BridgeHead
		--
		Renderas: Indicates the format in which the BridgeHead
		should appear
		--
		Renderas	(Other
				|Sect1
				|Sect2
				|Sect3
				|Sect4
				|Sect5)		#IMPLIED
		%common.attrib;
		%bridgehead.role.attrib;
		%local.bridgehead.attrib;
>
<!--end of bridgehead.module-->]]>

<!ENTITY % comment.module "INCLUDE">
<![ %comment.module; [
<!--FUTURE USE (V4.0):
......................
Comment will be renamed to Remark and will be excluded from itself.
......................
-->
<!ENTITY % local.comment.attrib "">
<!ENTITY % comment.role.attrib "%role.attrib;">
<!ELEMENT Comment - - ((%para.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST Comment
		%common.attrib;
		%comment.role.attrib;
		%local.comment.attrib;
>
<!--end of comment.module-->]]>

<!ENTITY % epigraph.module "INCLUDE">
<![ %epigraph.module; [
<!ENTITY % local.epigraph.attrib "">
<!ENTITY % epigraph.role.attrib "%role.attrib;">
<!ELEMENT Epigraph - - (Attribution?, (%para.class;)+)>
<!ATTLIST Epigraph
		%common.attrib;
		%epigraph.role.attrib;
		%local.epigraph.attrib;
>
<!--ELEMENT Attribution (defined above)-->
<!--end of epigraph.module-->]]>

<!ENTITY % footnote.module "INCLUDE">
<![ %footnote.module; [
<!ENTITY % local.footnote.attrib "">
<!ENTITY % footnote.role.attrib "%role.attrib;">
<!ELEMENT Footnote - - ((%footnote.mix;)+) -(Footnote|%formal.class;)>
<!ATTLIST Footnote
		%label.attrib;
		%common.attrib;
		%footnote.role.attrib;
		%local.footnote.attrib;
>
<!--end of footnote.module-->]]>

<!ENTITY % highlights.module "INCLUDE">
<![ %highlights.module; [
<!ENTITY % local.highlights.attrib "">
<!ENTITY % highlights.role.attrib "%role.attrib;">
<!ELEMENT Highlights - - ((%highlights.mix;)+) -(%ubiq.mix;|%formal.class;)>
<!ATTLIST Highlights
		%common.attrib;
		%highlights.role.attrib;
		%local.highlights.attrib;
>
<!--end of highlights.module-->]]>

<!ENTITY % formalpara.module "INCLUDE">
<![ %formalpara.module; [
<!ENTITY % local.formalpara.attrib "">
<!ENTITY % formalpara.role.attrib "%role.attrib;">
<!ELEMENT FormalPara - O (Title, Para)>
<!ATTLIST FormalPara
		%common.attrib;
		%formalpara.role.attrib;
		%local.formalpara.attrib;
>
<!--end of formalpara.module-->]]>

<!ENTITY % para.module "INCLUDE">
<![ %para.module; [
<!ENTITY % local.para.attrib "">
<!ENTITY % para.role.attrib "%role.attrib;">
<!ELEMENT Para - O ((%para.char.mix; | %para.mix;)+)>
<!ATTLIST Para
		%common.attrib;
		%para.role.attrib;
		%local.para.attrib;
>
<!--end of para.module-->]]>

<!ENTITY % simpara.module "INCLUDE">
<![ %simpara.module; [
<!ENTITY % local.simpara.attrib "">
<!ENTITY % simpara.role.attrib "%role.attrib;">
<!ELEMENT SimPara - O ((%para.char.mix;)+)>
<!ATTLIST SimPara
		%common.attrib;
		%simpara.role.attrib;
		%local.simpara.attrib;
>
<!--end of simpara.module-->]]>

<!ENTITY % admon.module "INCLUDE">
<![ %admon.module; [
<!ENTITY % local.admon.attrib "">
<!ENTITY % admon.role.attrib "%role.attrib;">
<!ELEMENT (%admon.class;) - - (Title?, (%admon.mix;)+) -(%admon.class;)>
<!ATTLIST (%admon.class;)
		%common.attrib;
		%admon.role.attrib;
		%local.admon.attrib;
>
<!--end of admon.module-->]]>

<!-- ...................................................................... -->
<!-- Lists ................................................................ -->

<!-- GlossList ........................ -->

<!ENTITY % glosslist.module "INCLUDE">
<![ %glosslist.module; [
<!ENTITY % local.glosslist.attrib "">
<!ENTITY % glosslist.role.attrib "%role.attrib;">
<!ELEMENT GlossList - - (GlossEntry+)>
<!ATTLIST GlossList
		%common.attrib;
		%glosslist.role.attrib;
		%local.glosslist.attrib;
>
<!--end of glosslist.module-->]]>

<!ENTITY % glossentry.content.module "INCLUDE">
<![ %glossentry.content.module; [
<!ENTITY % glossentry.module "INCLUDE">
<![ %glossentry.module; [
<!ENTITY % local.glossentry.attrib "">
<!ENTITY % glossentry.role.attrib "%role.attrib;">
<!ELEMENT GlossEntry - O (GlossTerm, Acronym?, Abbrev?, (GlossSee|GlossDef+))>
<!ATTLIST GlossEntry
		--
		SortAs: String by which the GlossEntry is to be sorted
		(alphabetized) in lieu of its proper content
		--
		SortAs		CDATA		#IMPLIED
		%common.attrib;
		%glossentry.role.attrib;
		%local.glossentry.attrib;
>
<!--end of glossentry.module-->]]>

<!--FUTURE USE (V4.0):
......................
GlossTerm will be excluded from itself.  Also, GlossTerm *may* be split
into an element that appears in a Glossary and an element that can
appear in the main text.
......................
-->
<!--ELEMENT GlossTerm (defined in the Inlines section, below)-->
<!ENTITY % glossdef.module "INCLUDE">
<![ %glossdef.module; [
<!ENTITY % local.glossdef.attrib "">
<!ENTITY % glossdef.role.attrib "%role.attrib;">
<!ELEMENT GlossDef - O ((%glossdef.mix;)+, GlossSeeAlso*)>
<!--FUTURE USE (V4.0):
......................
The Subject attribute will be renamed Keyword.
......................
-->
<!ATTLIST GlossDef
		--
		Subject: List of subjects; keywords for the definition
		--
		Subject		CDATA		#IMPLIED
		%common.attrib;
		%glossdef.role.attrib;
		%local.glossdef.attrib;
>
<!--end of glossdef.module-->]]>

<!ENTITY % glosssee.module "INCLUDE">
<![ %glosssee.module; [
<!ENTITY % local.glosssee.attrib "">
<!ENTITY % glosssee.role.attrib "%role.attrib;">
<!ELEMENT GlossSee - O ((%para.char.mix;)+)>
<!ATTLIST GlossSee
		--
		OtherTerm: Reference to the GlossEntry whose GlossTerm
		should be displayed at the point of the GlossSee
		--
		OtherTerm	IDREF		#CONREF
		%common.attrib;
		%glosssee.role.attrib;
		%local.glosssee.attrib;
>
<!--end of glosssee.module-->]]>

<!ENTITY % glossseealso.module "INCLUDE">
<![ %glossseealso.module; [
<!ENTITY % local.glossseealso.attrib "">
<!ENTITY % glossseealso.role.attrib "%role.attrib;">
<!ELEMENT GlossSeeAlso - O ((%para.char.mix;)+)>
<!ATTLIST GlossSeeAlso
		--
		OtherTerm: Reference to the GlossEntry whose GlossTerm
		should be displayed at the point of the GlossSeeAlso
		--
		OtherTerm	IDREF		#CONREF
		%common.attrib;
		%glossseealso.role.attrib;
		%local.glossseealso.attrib;
>
<!--end of glossseealso.module-->]]>
<!--end of glossentry.content.module-->]]>

<!-- ItemizedList and OrderedList ..... -->

<!ENTITY % itemizedlist.module "INCLUDE">
<![ %itemizedlist.module; [
<!ENTITY % local.itemizedlist.attrib "">
<!ENTITY % itemizedlist.role.attrib "%role.attrib;">
<!ELEMENT ItemizedList - - (ListItem+)>
<!ATTLIST ItemizedList	
		--
		Spacing: Whether the vertical space in the list should be
		compressed
		--
		Spacing		(Normal
				|Compact)	#IMPLIED
		--
		Mark: Keyword, e.g., bullet, dash, checkbox, none;
		list of keywords and defaults are implementation specific
		--
		%mark.attrib;
		%common.attrib;
		%itemizedlist.role.attrib;
		%local.itemizedlist.attrib;
>
<!--end of itemizedlist.module-->]]>

<!ENTITY % orderedlist.module "INCLUDE">
<![ %orderedlist.module; [
<!ENTITY % local.orderedlist.attrib "">
<!ENTITY % orderedlist.role.attrib "%role.attrib;">
<!ELEMENT OrderedList - - (ListItem+)>
<!ATTLIST OrderedList
		--
		Numeration: Style of ListItem numbered; default is expected
		to be Arabic
		--
		Numeration	(Arabic
				|Upperalpha
				|Loweralpha
				|Upperroman
				|Lowerroman)	#IMPLIED
		--
		InheritNum: Specifies for a nested list that the numbering
		of ListItems should include the number of the item
		within which they are nested (e.g., 1a and 1b within 1,
		rather than a and b)--
		InheritNum	(Inherit
				|Ignore)	Ignore
		--
		Continuation: Where list numbering begins afresh (Restarts,
		the default) or continues that of the immediately preceding 
		list (Continues)
		--
		Continuation	(Continues
				|Restarts)	Restarts
		--
		Spacing: Whether the vertical space in the list should be
		compressed
		--
		Spacing		(Normal
				|Compact)	#IMPLIED
		%common.attrib;
		%orderedlist.role.attrib;
		%local.orderedlist.attrib;
>
<!--end of orderedlist.module-->]]>

<!ENTITY % listitem.module "INCLUDE">
<![ %listitem.module; [
<!ENTITY % local.listitem.attrib "">
<!ENTITY % listitem.role.attrib "%role.attrib;">
<!ELEMENT ListItem - O ((%component.mix;)+)>
<!ATTLIST ListItem
		--
		Override: Indicates the mark to be used for this ListItem
		instead of the default mark or the mark specified by
		the Mark attribute on the enclosing ItemizedList
		--
		Override	CDATA		#IMPLIED
		%common.attrib;
		%listitem.role.attrib;
		%local.listitem.attrib;
>
<!--end of listitem.module-->]]>

<!-- SegmentedList .................... -->
<!ENTITY % segmentedlist.content.module "INCLUDE">
<![ %segmentedlist.content.module; [
<!--FUTURE USE (V4.0):
......................
Two SegTitles will be required.
......................
-->
<!ENTITY % segmentedlist.module "INCLUDE">
<![ %segmentedlist.module; [
<!ENTITY % local.segmentedlist.attrib "">
<!ENTITY % segmentedlist.role.attrib "%role.attrib;">
<!ELEMENT SegmentedList - - ((%formalobject.title.content;)?, SegTitle*,
		SegListItem+)>
<!ATTLIST SegmentedList
		%common.attrib;
		%segmentedlist.role.attrib;
		%local.segmentedlist.attrib;
>
<!--end of segmentedlist.module-->]]>

<!ENTITY % segtitle.module "INCLUDE">
<![ %segtitle.module; [
<!ENTITY % local.segtitle.attrib "">
<!ENTITY % segtitle.role.attrib "%role.attrib;">
<!ELEMENT SegTitle - O ((%title.char.mix;)+)>
<!ATTLIST SegTitle
		%common.attrib;
		%segtitle.role.attrib;
		%local.segtitle.attrib;
>
<!--end of segtitle.module-->]]>

<!ENTITY % seglistitem.module "INCLUDE">
<![ %seglistitem.module; [
<!ENTITY % local.seglistitem.attrib "">
<!ENTITY % seglistitem.role.attrib "%role.attrib;">
<!ELEMENT SegListItem - O (Seg, Seg+)>
<!ATTLIST SegListItem
		%common.attrib;
		%seglistitem.role.attrib;
		%local.seglistitem.attrib;
>
<!--end of seglistitem.module-->]]>

<!ENTITY % seg.module "INCLUDE">
<![ %seg.module; [
<!ENTITY % local.seg.attrib "">
<!ENTITY % seg.role.attrib "%role.attrib;">
<!ELEMENT Seg - O ((%para.char.mix;)+)>
<!ATTLIST Seg
		%common.attrib;
		%seg.role.attrib;
		%local.seg.attrib;
>
<!--end of seg.module-->]]>
<!--end of segmentedlist.content.module-->]]>

<!-- SimpleList ....................... -->

<!ENTITY % simplelist.content.module "INCLUDE">
<![ %simplelist.content.module; [
<!ENTITY % simplelist.module "INCLUDE">
<![ %simplelist.module; [
<!ENTITY % local.simplelist.attrib "">
<!ENTITY % simplelist.role.attrib "%role.attrib;">
<!ELEMENT SimpleList - - (Member+)>
<!ATTLIST SimpleList
		--
		Columns: The number of columns the array should contain
		--
		Columns		NUMBER		#IMPLIED
		--
		Type: How the Members of the SimpleList should be
		formatted: Inline (members separated with commas etc.
		inline), Vert (top to bottom in n Columns), or Horiz (in
		the direction of text flow) in n Columns.  If Column
		is 1 or implied, Type=Vert and Type=Horiz give the same
		results.
		--
		Type		(Inline
				|Vert
				|Horiz)		Vert
		%common.attrib;
		%simplelist.role.attrib;
		%local.simplelist.attrib;
>
<!--end of simplelist.module-->]]>

<!ENTITY % member.module "INCLUDE">
<![ %member.module; [
<!ENTITY % local.member.attrib "">
<!ENTITY % member.role.attrib "%role.attrib;">
<!ELEMENT Member - O ((%para.char.mix;)+)>
<!ATTLIST Member
		%common.attrib;
		%member.role.attrib;
		%local.member.attrib;
>
<!--end of member.module-->]]>
<!--end of simplelist.content.module-->]]>

<!-- VariableList ..................... -->

<!ENTITY % variablelist.content.module "INCLUDE">
<![ %variablelist.content.module; [
<!ENTITY % variablelist.module "INCLUDE">
<![ %variablelist.module; [
<!ENTITY % local.variablelist.attrib "">
<!ENTITY % variablelist.role.attrib "%role.attrib;">
<!ELEMENT VariableList - - ((%formalobject.title.content;)?, VarListEntry+)>
<!ATTLIST VariableList
		--
		TermLength: Length beyond which the presentation engine
		may consider the Term too long and select an alternate
		presentation of the Term and, or, its associated ListItem.
		--
		TermLength	CDATA		#IMPLIED
		%common.attrib;
		%variablelist.role.attrib;
		%local.variablelist.attrib;
>
<!--end of variablelist.module-->]]>

<!ENTITY % varlistentry.module "INCLUDE">
<![ %varlistentry.module; [
<!ENTITY % local.varlistentry.attrib "">
<!ENTITY % varlistentry.role.attrib "%role.attrib;">
<!ELEMENT VarListEntry - O (Term+, ListItem)>
<!ATTLIST VarListEntry
		%common.attrib;
		%varlistentry.role.attrib;
		%local.varlistentry.attrib;
>
<!--end of varlistentry.module-->]]>

<!ENTITY % term.module "INCLUDE">
<![ %term.module; [
<!ENTITY % local.term.attrib "">
<!ENTITY % term.role.attrib "%role.attrib;">
<!ELEMENT Term - O ((%para.char.mix;)+)>
<!ATTLIST Term
		%common.attrib;
		%term.role.attrib;
		%local.term.attrib;
>
<!--end of term.module-->]]>

<!--ELEMENT ListItem (defined above)-->
<!--end of variablelist.content.module-->]]>

<!-- CalloutList ...................... -->

<!ENTITY % calloutlist.content.module "INCLUDE">
<![ %calloutlist.content.module; [
<!ENTITY % calloutlist.module "INCLUDE">
<![ %calloutlist.module; [
<!ENTITY % local.calloutlist.attrib "">
<!ENTITY % calloutlist.role.attrib "%role.attrib;">
<!ELEMENT CalloutList - - ((%formalobject.title.content;)?, Callout+)>
<!ATTLIST CalloutList
		%common.attrib;
		%calloutlist.role.attrib;
		%local.calloutlist.attrib;
>
<!--end of calloutlist.module-->]]>

<!ENTITY % callout.module "INCLUDE">
<![ %callout.module; [
<!ENTITY % local.callout.attrib "">
<!ENTITY % callout.role.attrib "%role.attrib;">
<!ELEMENT Callout - O ((%component.mix;)+)>
<!ATTLIST Callout
		--
		AreaRefs: IDs of one or more Areas or AreaSets described
		by this Callout
		--
		AreaRefs	IDREFS		#REQUIRED
		%common.attrib;
		%callout.role.attrib;
		%local.callout.attrib;
>
<!--end of callout.module-->]]>
<!--end of calloutlist.content.module-->]]>

<!-- ...................................................................... -->
<!-- Objects .............................................................. -->

<!-- Examples etc. .................... -->

<!ENTITY % example.module "INCLUDE">
<![ %example.module; [
<!ENTITY % local.example.attrib "">
<!ENTITY % example.role.attrib "%role.attrib;">
<!ELEMENT Example - - ((%formalobject.title.content;), (%example.mix;)+)
		-(%formal.class;)>
<!ATTLIST Example
		%label.attrib;
		%width.attrib;
		%common.attrib;
		%example.role.attrib;
		%local.example.attrib;
>
<!--end of example.module-->]]>

<!ENTITY % informalexample.module "INCLUDE">
<![ %informalexample.module; [
<!ENTITY % local.informalexample.attrib "">
<!ENTITY % informalexample.role.attrib "%role.attrib;">
<!ELEMENT InformalExample - - ((%example.mix;)+)>
<!ATTLIST InformalExample
		%width.attrib;
		%common.attrib;
		%informalexample.role.attrib;
		%local.informalexample.attrib;
>
<!--end of informalexample.module-->]]>

<!ENTITY % programlistingco.module "INCLUDE">
<![ %programlistingco.module; [
<!ENTITY % local.programlistingco.attrib "">
<!ENTITY % programlistingco.role.attrib "%role.attrib;">
<!ELEMENT ProgramListingCO - - (AreaSpec, ProgramListing, CalloutList*)>
<!ATTLIST ProgramListingCO
		%common.attrib;
		%programlistingco.role.attrib;
		%local.programlistingco.attrib;
>
<!--ELEMENT CalloutList (defined above in Lists)-->
<!--end of informalexample.module-->]]>

<!ENTITY % areaspec.content.module "INCLUDE">
<![ %areaspec.content.module; [
<!ENTITY % areaspec.module "INCLUDE">
<![ %areaspec.module; [
<!ENTITY % local.areaspec.attrib "">
<!ENTITY % areaspec.role.attrib "%role.attrib;">
<!ELEMENT AreaSpec - - ((Area|AreaSet)+)>
<!ATTLIST AreaSpec
		--
		Units: global unit of measure in which coordinates in
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
		and ScreenCO get LineColumn)
		--
		Units		(CALSPair
				|LineColumn
				|LineRange
				|LineColumnPair
				|Other)		#IMPLIED
		--
		OtherUnits: User-defined units
		--
		OtherUnits	NAME		#IMPLIED
		%common.attrib;
		%areaspec.role.attrib;
		%local.areaspec.attrib;
>
<!--end of areaspec.module-->]]>

<!ENTITY % area.module "INCLUDE">
<![ %area.module; [
<!ENTITY % local.area.attrib "">
<!ENTITY % area.role.attrib "%role.attrib;">
<!ELEMENT Area - O EMPTY>
<!ATTLIST Area
		%label.attrib; --bug number/symbol override or initialization--
		%linkends.attrib; --to any related information--
		--
		Units: unit of measure in which coordinates in this
		area are expressed; inherits from AreaSet and AreaSpec
		--
		Units		(CALSPair
				|LineColumn
				|LineRange
				|LineColumnPair
				|Other)		#IMPLIED
		--
		OtherUnits: User-defined units
		--
		OtherUnits	NAME		#IMPLIED
		Coords		CDATA		#REQUIRED
		%idreq.common.attrib;
		%area.role.attrib;
		%local.area.attrib;
>
<!--end of area.module-->]]>

<!ENTITY % areaset.module "INCLUDE">
<![ %areaset.module; [
<!ENTITY % local.areaset.attrib "">
<!ENTITY % areaset.role.attrib "%role.attrib;">
<!ELEMENT AreaSet - - (Area+)>
<!ATTLIST AreaSet
		%label.attrib; --bug number/symbol override or initialization--

		--
		Units: unit of measure in which coordinates in this
		area are expressed; inherits from AreaSpec
		--
		Units		(CALSPair
				|LineColumn
				|LineRange
				|LineColumnPair
				|Other)		#IMPLIED
		OtherUnits	NAME		#IMPLIED
		Coords		CDATA		#REQUIRED
		%idreq.common.attrib;
		%areaset.role.attrib;
		%local.area.attrib;
>
<!--end of areaset.module-->]]>
<!--end of areaspec.content.module-->]]>

<!ENTITY % programlisting.module "INCLUDE">
<![ %programlisting.module; [
<!ENTITY % local.programlisting.attrib "">
<!ENTITY % programlisting.role.attrib "%role.attrib;">
<!ELEMENT ProgramListing - - ((%programlisting.content;)+)>
<!ATTLIST ProgramListing
		%width.attrib;
		%linespecific.attrib;
		%common.attrib;
		%programlisting.role.attrib;
		%local.programlisting.attrib;
>
<!--end of programlisting.module-->]]>

<!ENTITY % literallayout.module "INCLUDE">
<![ %literallayout.module; [
<!ENTITY % local.literallayout.attrib "">
<!ENTITY % literallayout.role.attrib "%role.attrib;">
<!ELEMENT LiteralLayout - - ((LineAnnotation | %para.char.mix;)+)>
<!ATTLIST LiteralLayout
		%width.attrib;
		%linespecific.attrib;
		%common.attrib;
		%literallayout.role.attrib;
		%local.literallayout.attrib;
>
<!--ELEMENT LineAnnotation (defined in the Inlines section, below)-->
<!--end of literallayout.module-->]]>

<!ENTITY % screenco.module "INCLUDE">
<![ %screenco.module; [
<!ENTITY % local.screenco.attrib "">
<!ENTITY % screenco.role.attrib "%role.attrib;">
<!ELEMENT ScreenCO - - (AreaSpec, Screen, CalloutList*)>
<!ATTLIST ScreenCO
		%common.attrib;
		%screenco.role.attrib;
		%local.screenco.attrib;
>
<!--ELEMENT AreaSpec (defined above)-->
<!--ELEMENT CalloutList (defined above in Lists)-->
<!--end of screenco.module-->]]>

<!ENTITY % screen.module "INCLUDE">
<![ %screen.module; [
<!ENTITY % local.screen.attrib "">
<!ENTITY % screen.role.attrib "%role.attrib;">
<!ELEMENT Screen - - ((%screen.content;)+)>
<!ATTLIST Screen
		%width.attrib;
		%linespecific.attrib;
		%common.attrib;
		%screen.role.attrib;
		%local.screen.attrib;
>
<!--end of screen.module-->]]>

<!ENTITY % screenshot.content.module "INCLUDE">
<![ %screenshot.content.module; [
<!ENTITY % screenshot.module "INCLUDE">
<![ %screenshot.module; [
<!ENTITY % local.screenshot.attrib "">
<!ENTITY % screenshot.role.attrib "%role.attrib;">
<!ELEMENT ScreenShot - - (ScreenInfo?, (Graphic|GraphicCO))>
<!ATTLIST ScreenShot
		%common.attrib;
		%screenshot.role.attrib;
		%local.screenshot.attrib;
>
<!--end of screenshot.module-->]]>

<!ENTITY % screeninfo.module "INCLUDE">
<![ %screeninfo.module; [
<!ENTITY % local.screeninfo.attrib "">
<!ENTITY % screeninfo.role.attrib "%role.attrib;">
<!ELEMENT ScreenInfo - O ((%para.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST ScreenInfo
		%common.attrib;
		%screeninfo.role.attrib;
		%local.screeninfo.attrib;
>
<!--end of screeninfo.module-->]]>
<!--end of screenshot.content.module-->]]>

<!-- Figures etc. ..................... -->

<!ENTITY % figure.module "INCLUDE">
<![ %figure.module; [
<!ENTITY % local.figure.attrib "">
<!ENTITY % figure.role.attrib "%role.attrib;">
<!ELEMENT Figure - - ((%formalobject.title.content;), (%figure.mix; |
		%link.char.class;)+)>
<!ATTLIST Figure
		--
		Float: Whether the Figure is supposed to be rendered
		where convenient (yes (1) value) or at the place it occurs
		in the text (no (0) value, the default)
		--
		Float		%yesorno.attvals;	%no.attval;
		%label.attrib;
		%common.attrib;
		%figure.role.attrib;
		%local.figure.attrib;
>
<!--end of figure.module-->]]>

<!ENTITY % graphicco.module "INCLUDE">
<![ %graphicco.module; [
<!ENTITY % local.graphicco.attrib "">
<!ENTITY % graphicco.role.attrib "%role.attrib;">
<!ELEMENT GraphicCO - - (AreaSpec, Graphic, CalloutList*)>
<!ATTLIST GraphicCO
		%common.attrib;
		%graphicco.role.attrib;
		%local.graphicco.attrib;
>
<!--ELEMENT AreaSpec (defined above in Examples)-->
<!--ELEMENT CalloutList (defined above in Lists)-->
<!--end of graphicco.module-->]]>

<!-- Graphical data can be the content of Graphic, or you can reference
     an external file either as an entity (Entitref) or a filename
     (Fileref). -->

<!ENTITY % graphic.module "INCLUDE">
<![ %graphic.module; [
<!--FUTURE USE (V4.0):
......................
Graphic will be declared EMPTY.  This change will require that end-tags
be removed and that any embedded graphic content be stored outside the
SGML source and pointed to from an Entityref or Fileref attribute.
......................
-->
<!ENTITY % local.graphic.attrib "">
<!ENTITY % graphic.role.attrib "%role.attrib;">
<!ELEMENT Graphic - - CDATA>
<!ATTLIST Graphic
		%graphics.attrib;
		%common.attrib;
		%graphic.role.attrib;
		%local.graphic.attrib;
>
<!--end of graphic.module-->]]>

<!ENTITY % inlinegraphic.module "INCLUDE">
<![ %inlinegraphic.module; [
<!--FUTURE USE (V4.0):
......................
InlineGraphic will be declared EMPTY.  This change will require that
end-tags be removed and that any embedded graphic content be stored
outside the SGML source and pointed to from an Entityref or Fileref
attribute.
......................
-->
<!ENTITY % local.inlinegraphic.attrib "">
<!ENTITY % inlinegraphic.role.attrib "%role.attrib;">
<!ELEMENT InlineGraphic - - CDATA>
<!ATTLIST InlineGraphic
		%graphics.attrib;
		%common.attrib;
		%inlinegraphic.role.attrib;
		%local.inlinegraphic.attrib;
>
<!--end of inlinegraphic.module-->]]>

<!-- Equations ........................ -->

<!ENTITY % equation.module "INCLUDE">
<![ %equation.module; [
<!ENTITY % local.equation.attrib "">
<!ENTITY % equation.role.attrib "%role.attrib;">
<!ELEMENT Equation - - ((%formalobject.title.content;)?, (InformalEquation |
		(Alt?, %equation.content;)))>
<!ATTLIST Equation
		%label.attrib;
	 	%common.attrib;
		%equation.role.attrib;
		%local.equation.attrib;
>
<!--end of equation.module-->]]>

<!ENTITY % informalequation.module "INCLUDE">
<![ %informalequation.module; [
<!ENTITY % local.informalequation.attrib "">
<!ENTITY % informalequation.role.attrib "%role.attrib;">
<!ELEMENT InformalEquation - - (Alt?, %equation.content;)>
<!ATTLIST InformalEquation
		%common.attrib;
		%informalequation.role.attrib;
		%local.informalequation.attrib;
>
<!--end of informalequation.module-->]]>

<!ENTITY % inlineequation.module "INCLUDE">
<![ %inlineequation.module; [
<!ENTITY % local.inlineequation.attrib "">
<!ENTITY % inlineequation.role.attrib "%role.attrib;">
<!ELEMENT InlineEquation - - (Alt?, %inlineequation.content;)>
<!ATTLIST InlineEquation
		%common.attrib;
		%inlineequation.role.attrib;
		%local.inlineequation.attrib;
>
<!--end of inlineequation.module-->]]>

<!ENTITY % alt.module "INCLUDE">
<![ %alt.module; [
<!ENTITY % local.alt.attrib "">
<!ENTITY % alt.role.attrib "%role.attrib;">
<!ELEMENT Alt - - (#PCDATA)>
<!ATTLIST Alt 
		%common.attrib;
		%alt.role.attrib;
		%local.alt.attrib;
>
<!--end of alt.module-->]]>

<!-- Tables ........................... -->

<!ENTITY % table.module "INCLUDE">
<![ %table.module; [

<!ENTITY % tables.role.attrib "%role.attrib;">

<!-- Add Label attribute to Table element (and InformalTable element). -->
<!ENTITY % bodyatt "%label.attrib;">

<!-- Add common attributes to Table, TGroup, TBody, THead, TFoot, Row, 
     EntryTbl, and Entry (and InformalTable element). -->
<!ENTITY % secur
	"%common.attrib;
	%tables.role.attrib;">

<!-- Remove Chart. -->
<!ENTITY % tbl.table.name "Table">

<!-- Content model for Table. -->
<!ENTITY % tbl.table.mdl
	"((%formalobject.title.content;), (Graphic+|TGroup+))">

<!-- Exclude all DocBook tables and formal objects. -->
<!ENTITY % tbl.table.excep "-(InformalTable|%formal.class;)">

<!-- Remove pgbrk exception on Row. -->
<!ENTITY % tbl.row.excep "">

<!-- Allow either objects or inlines; beware of REs between elements. -->
<!ENTITY % tbl.entry.mdl "((%tabentry.mix;)+ | (%para.char.mix;)+)">

<!-- Remove pgbrk exception on Entry. -->
<!ENTITY % tbl.entry.excep "">

<!-- Remove pgbrk exception on EntryTbl, but leave exclusion of itself. -->
<!ENTITY % tbl.entrytbl.excep "-(EntryTbl)">

<!-- Reference CALS table module. -->
<!ENTITY % calstbls PUBLIC "-//USA-DOD//DTD Table Model 951010//EN">
%calstbls;
<!--end of table.module-->]]>

<!ENTITY % informaltable.module "INCLUDE">
<![ %informaltable.module; [

<!-- Note that InformalTable is dependent on some of the entity
     declarations that customize Table. -->

<!ENTITY % local.informaltable.attrib "">
<!ELEMENT InformalTable - - (Graphic+|TGroup+) %tbl.table.excep;>
<!ATTLIST InformalTable
		--
		Frame, Colsep, and Rowsep must be repeated because
		they are not in entities in the table module.
		--
		Frame		(Top
				|Bottom
				|Topbot
				|All
				|Sides
				|None)			#IMPLIED
		Colsep		%yesorno.attvals;	#IMPLIED
		Rowsep		%yesorno.attvals;	#IMPLIED
		%tbl.table.att; -- includes TabStyle, ToCentry, ShortEntry, 
				Orient, PgWide --
		%bodyatt; -- includes Label --
		%secur; -- includes common attributes --
		%local.informaltable.attrib;
>
<!--end of informaltable.module-->]]>

<!-- ...................................................................... -->
<!-- Synopses ............................................................. -->

<!-- Synopsis ......................... -->

<!ENTITY % synopsis.module "INCLUDE">
<![ %synopsis.module; [
<!ENTITY % local.synopsis.attrib "">
<!ENTITY % synopsis.role.attrib "%role.attrib;">
<!ELEMENT Synopsis - - ((LineAnnotation | %para.char.mix; | Graphic)+)>
<!ATTLIST Synopsis
		%label.attrib;
		%linespecific.attrib;
		%common.attrib;
		%synopsis.role.attrib;
		%local.synopsis.attrib;
>

<!--ELEMENT LineAnnotation (defined in the Inlines section, below)-->
<!--end of synopsis.module-->]]>

<!-- CmdSynopsis ...................... -->

<!ENTITY % cmdsynopsis.content.module "INCLUDE">
<![ %cmdsynopsis.content.module; [
<!ENTITY % cmdsynopsis.module "INCLUDE">
<![ %cmdsynopsis.module; [
<!ENTITY % local.cmdsynopsis.attrib "">
<!ENTITY % cmdsynopsis.role.attrib "%role.attrib;">
<!ELEMENT CmdSynopsis - - ((Command | Arg | Group | SBR)+, SynopFragment*)>
<!ATTLIST CmdSynopsis
		%label.attrib;
		--
		Sepchar: Character that should separate command and all 
		top-level arguments; alternate value might be e.g., &Delta;
		--
		Sepchar		CDATA		" "
		%common.attrib;
		%cmdsynopsis.role.attrib;
		%local.cmdsynopsis.attrib;
>
<!--end of cmdsynopsis.module-->]]>

<!ENTITY % arg.module "INCLUDE">
<![ %arg.module; [
<!ENTITY % local.arg.attrib "">
<!ENTITY % arg.role.attrib "%role.attrib;">
<!ELEMENT Arg - - ((#PCDATA 
		| Arg 
		| Group 
		| Option 
		| SynopFragmentRef 
		| Replaceable
		| SBR)+)>
<!ATTLIST Arg
		--
		Choice: Whether Arg must be supplied: Opt (optional to 
		supply, e.g. [arg]; the default), Req (required to supply, 
		e.g. {arg}), or Plain (required to supply, e.g. arg)
		--
		Choice		(Opt
				|Req
				|Plain)		Opt
		--
		Rep: whether Arg is repeatable: Norepeat (e.g. arg without 
		ellipsis; the default), or Repeat (e.g. arg...)
		--
		Rep		(Norepeat
				|Repeat)	Norepeat
		%common.attrib;
		%arg.role.attrib;
		%local.arg.attrib;
>
<!--end of arg.module-->]]>

<!ENTITY % group.module "INCLUDE">
<![ %group.module; [
<!--FUTURE USE (V4.0):
......................
The OptMult and ReqMult values for the Choice attribute on Group will be
removed.  Use the Rep attribute instead to indicate that the choice is
repeatable.
......................
-->

<!ENTITY % local.group.attrib "">
<!ENTITY % group.role.attrib "%role.attrib;">
<!ELEMENT Group - - ((Arg | Group | Option | SynopFragmentRef 
		| Replaceable | SBR)+)>
<!ATTLIST Group
		--
		Choice: Whether Group must be supplied: Opt (optional to
		supply, e.g.  [g1|g2|g3]; the default), Req (required to
		supply, e.g.  {g1|g2|g3}), Plain (required to supply,
		e.g.  g1|g2|g3), OptMult (can supply zero or more, e.g.
		[[g1|g2|g3]]), or ReqMult (must supply one or more, e.g.
		{{g1|g2|g3}})
		--
		Choice		(Opt
				|Req
				|Plain
				|Optmult
				|Reqmult)	Opt
		--
		Rep: whether Group is repeatable: Norepeat (e.g. group 
		without ellipsis; the default), or Repeat (e.g. group...)
		--
		Rep		(Norepeat
				|Repeat)	Norepeat
		%common.attrib;
		%group.role.attrib;
		%local.group.attrib;
>
<!--end of group.module-->]]>

<!ENTITY % sbr.module "INCLUDE">
<![ %sbr.module; [
<!ENTITY % local.sbr.attrib "">
<!-- Synopsis break -->
<!ENTITY % sbr.role.attrib "%role.attrib;">
<!ELEMENT SBR - O EMPTY>
<!ATTLIST SBR
		%common.attrib;
		%sbr.role.attrib;
		%local.sbr.attrib;
>
<!--end of sbr.module-->]]>

<!ENTITY % synopfragmentref.module "INCLUDE">
<![ %synopfragmentref.module; [
<!ENTITY % local.synopfragmentref.attrib "">
<!ENTITY % synopfragmentref.role.attrib "%role.attrib;">
<!ELEMENT SynopFragmentRef - - RCDATA >
<!ATTLIST SynopFragmentRef
		%linkendreq.attrib; --to SynopFragment of complex synopsis
			material for separate referencing--
		%common.attrib;
		%synopfragmentref.role.attrib;
		%local.synopfragmentref.attrib;
>
<!--end of synopfragmentref.module-->]]>

<!ENTITY % synopfragment.module "INCLUDE">
<![ %synopfragment.module; [
<!ENTITY % local.synopfragment.attrib "">
<!ENTITY % synopfragment.role.attrib "%role.attrib;">
<!ELEMENT SynopFragment - - ((Arg | Group)+)>
<!ATTLIST SynopFragment
		%idreq.common.attrib;
		%synopfragment.role.attrib;
		%local.synopfragment.attrib;
>
<!--end of synopfragment.module-->]]>

<!--ELEMENT Command (defined in the Inlines section, below)-->
<!--ELEMENT Option (defined in the Inlines section, below)-->
<!--ELEMENT Replaceable (defined in the Inlines section, below)-->
<!--end of cmdsynopsis.content.module-->]]>

<!-- FuncSynopsis ..................... -->

<!ENTITY % funcsynopsis.content.module "INCLUDE">
<![ %funcsynopsis.content.module; [
<!ENTITY % funcsynopsis.module "INCLUDE">
<![ %funcsynopsis.module; [
<!--FUTURE USE (V4.0):
......................
The content model group starting with FuncDef will not be available; you
will have to use FuncPrototype.  Also, you will be able to have a
mixture of FuncPrototypes and FuncSynopsisInfos (this is not
backwards-incompatible all by itself).

<!ELEMENT FuncSynopsis - - ((FuncSynopsisInfo|FuncPrototype)+)>
......................
-->

<!ENTITY % local.funcsynopsis.attrib "">
<!ENTITY % funcsynopsis.role.attrib "%role.attrib;">
<!ELEMENT FuncSynopsis - - (FuncSynopsisInfo?, (FuncPrototype+ |
		(FuncDef, (Void | VarArgs | ParamDef+))+), FuncSynopsisInfo?)>
<!ATTLIST FuncSynopsis
		%label.attrib;
		%common.attrib;
		%funcsynopsis.role.attrib;
		%local.funcsynopsis.attrib;
>
<!--end of funcsynopsis.module-->]]>

<!ENTITY % funcsynopsisinfo.module "INCLUDE">
<![ %funcsynopsisinfo.module; [
<!ENTITY % local.funcsynopsisinfo.attrib "">
<!ENTITY % funcsynopsisinfo.role.attrib "%role.attrib;">
<!ELEMENT FuncSynopsisInfo - O ((LineAnnotation | %cptr.char.mix;)* )>
<!ATTLIST FuncSynopsisInfo
		%linespecific.attrib;
		%common.attrib;
		%funcsynopsisinfo.role.attrib;
		%local.funcsynopsisinfo.attrib;
>
<!--end of funcsynopsisinfo.module-->]]>

<!ENTITY % funcprototype.module "INCLUDE">
<![ %funcprototype.module; [
<!ENTITY % local.funcprototype.attrib "">
<!ENTITY % funcprototype.role.attrib "%role.attrib;">
<!ELEMENT FuncPrototype - O (FuncDef, (Void | VarArgs | ParamDef+))>
<!ATTLIST FuncPrototype
		%common.attrib;
		%funcprototype.role.attrib;
		%local.funcprototype.attrib;
>
<!--end of funcprototype.module-->]]>

<!ENTITY % funcdef.module "INCLUDE">
<![ %funcdef.module; [
<!ENTITY % local.funcdef.attrib "">
<!ENTITY % funcdef.role.attrib "%role.attrib;">
<!ELEMENT FuncDef - - ((#PCDATA 
		| Replaceable 
		| Function)*)>
<!ATTLIST FuncDef
		%common.attrib;
		%funcdef.role.attrib;
		%local.funcdef.attrib;
>
<!--end of funcdef.module-->]]>

<!ENTITY % void.module "INCLUDE">
<![ %void.module; [
<!ENTITY % local.void.attrib "">
<!ENTITY % void.role.attrib "%role.attrib;">
<!ELEMENT Void - O EMPTY>
<!ATTLIST Void
		%common.attrib;
		%void.role.attrib;
		%local.void.attrib;
>
<!--end of void.module-->]]>

<!ENTITY % varargs.module "INCLUDE">
<![ %varargs.module; [
<!ENTITY % local.varargs.attrib "">
<!ENTITY % varargs.role.attrib "%role.attrib;">
<!ELEMENT VarArgs - O EMPTY>
<!ATTLIST VarArgs
		%common.attrib;
		%varargs.role.attrib;
		%local.varargs.attrib;
>
<!--end of varargs.module-->]]>

<!-- Processing assumes that only one Parameter will appear in a
     ParamDef, and that FuncParams will be used at most once, for
     providing information on the "inner parameters" for parameters that
     are pointers to functions. -->

<!ENTITY % paramdef.module "INCLUDE">
<![ %paramdef.module; [
<!ENTITY % local.paramdef.attrib "">
<!ENTITY % paramdef.role.attrib "%role.attrib;">
<!ELEMENT ParamDef - - ((#PCDATA 
		| Replaceable 
		| Parameter 
		| FuncParams)*)>
<!ATTLIST ParamDef
		%common.attrib;
		%paramdef.role.attrib;
		%local.paramdef.attrib;
>
<!--end of paramdef.module-->]]>

<!ENTITY % funcparams.module "INCLUDE">
<![ %funcparams.module; [
<!ENTITY % local.funcparams.attrib "">
<!ENTITY % funcparams.role.attrib "%role.attrib;">
<!ELEMENT FuncParams - - ((%cptr.char.mix;)*)>
<!ATTLIST FuncParams
		%common.attrib;
		%funcparams.role.attrib;
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

<!-- The document information elements include some elements that are
     currently used only in the document hierarchy module. They are
     defined here so that they will be available for use in customized
     document hierarchies. -->

<!-- .................................. -->

<!ENTITY % docinfo.content.module "INCLUDE">
<![ %docinfo.content.module; [

<!-- Ackno ............................ -->

<!ENTITY % ackno.module "INCLUDE">
<![ %ackno.module; [
<!ENTITY % local.ackno.attrib "">
<!ENTITY % ackno.role.attrib "%role.attrib;">
<!ELEMENT Ackno - - ((%docinfo.char.mix;)+)>
<!ATTLIST Ackno
		%common.attrib;
		%ackno.role.attrib;
		%local.ackno.attrib;
>
<!--end of ackno.module-->]]>

<!-- Address .......................... -->

<!ENTITY % address.content.module "INCLUDE">
<![ %address.content.module; [
<!ENTITY % address.module "INCLUDE">
<![ %address.module; [
<!ENTITY % local.address.attrib "">
<!ENTITY % address.role.attrib "%role.attrib;">
<!ELEMENT Address - - (#PCDATA|Street|POB|Postcode|City|State|Country|Phone
		|Fax|Email|OtherAddr)*>
<!ATTLIST Address
		%linespecific.attrib;
		%common.attrib;
		%address.role.attrib;
		%local.address.attrib;
>
<!--end of address.module-->]]>

  <!ENTITY % street.module "INCLUDE">
  <![ %street.module; [
 <!ENTITY % local.street.attrib "">
  <!ENTITY % street.role.attrib "%role.attrib;">
  <!ELEMENT Street - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Street
		%common.attrib;
		%street.role.attrib;
		%local.street.attrib;
>
  <!--end of street.module-->]]>

  <!ENTITY % pob.module "INCLUDE">
  <![ %pob.module; [
  <!ENTITY % local.pob.attrib "">
  <!ENTITY % pob.role.attrib "%role.attrib;">
  <!ELEMENT POB - - ((%docinfo.char.mix;)+)>
  <!ATTLIST POB
		%common.attrib;
		%pob.role.attrib;
		%local.pob.attrib;
>
  <!--end of pob.module-->]]>

  <!ENTITY % postcode.module "INCLUDE">
  <![ %postcode.module; [
  <!ENTITY % local.postcode.attrib "">
  <!ENTITY % postcode.role.attrib "%role.attrib;">
  <!ELEMENT Postcode - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Postcode
		%common.attrib;
		%postcode.role.attrib;
		%local.postcode.attrib;
>
  <!--end of postcode.module-->]]>

  <!ENTITY % city.module "INCLUDE">
  <![ %city.module; [
  <!ENTITY % local.city.attrib "">
  <!ENTITY % city.role.attrib "%role.attrib;">
  <!ELEMENT City - - ((%docinfo.char.mix;)+)>
  <!ATTLIST City
		%common.attrib;
		%city.role.attrib;
		%local.city.attrib;
>
  <!--end of city.module-->]]>

  <!ENTITY % state.module "INCLUDE">
  <![ %state.module; [
  <!ENTITY % local.state.attrib "">
  <!ENTITY % state.role.attrib "%role.attrib;">
  <!ELEMENT State - - ((%docinfo.char.mix;)+)>
  <!ATTLIST State
		%common.attrib;
		%state.role.attrib;
		%local.state.attrib;
>
  <!--end of state.module-->]]>

  <!ENTITY % country.module "INCLUDE">
  <![ %country.module; [
  <!ENTITY % local.country.attrib "">
  <!ENTITY % country.role.attrib "%role.attrib;">
  <!ELEMENT Country - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Country
		%common.attrib;
		%role.attrib;
		%local.country.attrib;
>
  <!--end of country.module-->]]>

  <!ENTITY % phone.module "INCLUDE">
  <![ %phone.module; [
  <!ENTITY % local.phone.attrib "">
  <!ENTITY % phone.role.attrib "%role.attrib;">
  <!ELEMENT Phone - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Phone
		%common.attrib;
		%phone.role.attrib;
		%local.phone.attrib;
>
  <!--end of phone.module-->]]>

  <!ENTITY % fax.module "INCLUDE">
  <![ %fax.module; [
  <!ENTITY % local.fax.attrib "">
  <!ENTITY % fax.role.attrib "%role.attrib;">
  <!ELEMENT Fax - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Fax
		%common.attrib;
		%fax.role.attrib;
		%local.fax.attrib;
>
  <!--end of fax.module-->]]>

  <!--ELEMENT Email (defined in the Inlines section, below)-->

  <!ENTITY % otheraddr.module "INCLUDE">
  <![ %otheraddr.module; [
  <!ENTITY % local.otheraddr.attrib "">
  <!ENTITY % otheraddr.role.attrib "%role.attrib;">
  <!ELEMENT OtherAddr - - ((%docinfo.char.mix;)+)>
  <!ATTLIST OtherAddr
		%common.attrib;
		%otheraddr.role.attrib;
		%local.otheraddr.attrib;
>
  <!--end of otheraddr.module-->]]>
<!--end of address.content.module-->]]>

<!-- Affiliation ...................... -->

<!ENTITY % affiliation.content.module "INCLUDE">
<![ %affiliation.content.module; [
<!ENTITY % affiliation.module "INCLUDE">
<![ %affiliation.module; [
<!ENTITY % local.affiliation.attrib "">
<!ENTITY % affiliation.role.attrib "%role.attrib;">
<!ELEMENT Affiliation - - (ShortAffil?, JobTitle*, OrgName?, OrgDiv*,
		Address*)>
<!ATTLIST Affiliation
		%common.attrib;
		%affiliation.role.attrib;
		%local.affiliation.attrib;
>
<!--end of affiliation.module-->]]>

  <!ENTITY % shortaffil.module "INCLUDE">
  <![ %shortaffil.module; [
  <!ENTITY % local.shortaffil.attrib "">
  <!ENTITY % shortaffil.role.attrib "%role.attrib;">
  <!ELEMENT ShortAffil - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ShortAffil
		%common.attrib;
		%shortaffil.role.attrib;
		%local.shortaffil.attrib;
>
  <!--end of shortaffil.module-->]]>

  <!ENTITY % jobtitle.module "INCLUDE">
  <![ %jobtitle.module; [
  <!ENTITY % local.jobtitle.attrib "">
  <!ENTITY % jobtitle.role.attrib "%role.attrib;">
  <!ELEMENT JobTitle - - ((%docinfo.char.mix;)+)>
  <!ATTLIST JobTitle
		%common.attrib;
		%jobtitle.role.attrib;
		%local.jobtitle.attrib;
>
  <!--end of jobtitle.module-->]]>

  <!--ELEMENT OrgName (defined elsewhere in this section)-->

  <!ENTITY % orgdiv.module "INCLUDE">
  <![ %orgdiv.module; [
  <!ENTITY % local.orgdiv.attrib "">
  <!ENTITY % orgdiv.role.attrib "%role.attrib;">
  <!ELEMENT OrgDiv - - ((%docinfo.char.mix;)+)>
  <!ATTLIST OrgDiv
		%common.attrib;
		%orgdiv.role.attrib;
		%local.orgdiv.attrib;
>
  <!--end of orgdiv.module-->]]>

  <!--ELEMENT Address (defined elsewhere in this section)-->
<!--end of affiliation.content.module-->]]>

<!-- ArtPageNums ...................... -->

<!ENTITY % artpagenums.module "INCLUDE">
<![ %artpagenums.module; [
<!ENTITY % local.artpagenums.attrib "">
<!ENTITY % argpagenums.role.attrib "%role.attrib;">
<!ELEMENT ArtPageNums - - ((%docinfo.char.mix;)+)>
<!ATTLIST ArtPageNums
		%common.attrib;
		%argpagenums.role.attrib;
		%local.artpagenums.attrib;
>
<!--end of artpagenums.module-->]]>

<!-- Author ........................... -->

<!ENTITY % author.module "INCLUDE">
<![ %author.module; [
<!--FUTURE USE (V4.0):
......................
AuthorBlurb and Affiliation will be removed from %person.ident.mix; and a new 
wrapper element created to allow association of those two elements with 
Author name information.
......................
-->
<!ENTITY % local.author.attrib "">
<!ENTITY % author.role.attrib "%role.attrib;">
<!ELEMENT Author - - ((%person.ident.mix;)+)>
<!ATTLIST Author
		%common.attrib;
		%author.role.attrib;
		%local.author.attrib;
>
<!--(see "Personal identity elements" for %person.ident.mix;)-->
<!--end of author.module-->]]>

<!-- AuthorGroup ...................... -->

<!ENTITY % authorgroup.content.module "INCLUDE">
<![ %authorgroup.content.module; [
<!ENTITY % authorgroup.module "INCLUDE">
<![ %authorgroup.module; [
<!ENTITY % local.authorgroup.attrib "">
<!ENTITY % authorgroup.role.attrib "%role.attrib;">
<!ELEMENT AuthorGroup - - ((Author|Editor|Collab|CorpAuthor|OtherCredit)+)>
<!ATTLIST AuthorGroup
		%common.attrib;
		%authorgroup.role.attrib;
		%local.authorgroup.attrib;
>
<!--end of authorgroup.module-->]]>

  <!--ELEMENT Author (defined elsewhere in this section)-->
  <!--ELEMENT Editor (defined elsewhere in this section)-->

  <!ENTITY % collab.content.module "INCLUDE">
  <![ %collab.content.module; [
  <!ENTITY % collab.module "INCLUDE">
  <![ %collab.module; [
  <!ENTITY % local.collab.attrib "">
  <!ENTITY % collab.role.attrib "%role.attrib;">
  <!ELEMENT Collab - - (CollabName, Affiliation*)>
  <!ATTLIST Collab
		%common.attrib;
		%collab.role.attrib;
		%local.collab.attrib;
>
  <!--end of collab.module-->]]>

    <!ENTITY % collabname.module "INCLUDE">
  <![ %collabname.module; [
  <!ENTITY % local.collabname.attrib "">
  <!ENTITY % collabname.role.attrib "%role.attrib;">
    <!ELEMENT CollabName - - ((%docinfo.char.mix;)+)>
    <!ATTLIST CollabName
		%common.attrib;
		%collabname.role.attrib;
		%local.collabname.attrib;
>
    <!--end of collabname.module-->]]>

    <!--ELEMENT Affiliation (defined elsewhere in this section)-->
  <!--end of collab.content.module-->]]>

  <!--ELEMENT CorpAuthor (defined elsewhere in this section)-->
  <!--ELEMENT OtherCredit (defined elsewhere in this section)-->

<!--end of authorgroup.content.module-->]]>

<!-- AuthorInitials ................... -->

<!ENTITY % authorinitials.module "INCLUDE">
<![ %authorinitials.module; [
<!ENTITY % local.authorinitials.attrib "">
<!ENTITY % authorinitials.role.attrib "%role.attrib;">
<!ELEMENT AuthorInitials - - ((%docinfo.char.mix;)+)>
<!ATTLIST AuthorInitials
		%common.attrib;
		%authorinitials.role.attrib;
		%local.authorinitials.attrib;
>
<!--end of authorinitials.module-->]]>

<!-- ConfGroup ........................ -->

<!ENTITY % confgroup.content.module "INCLUDE">
<![ %confgroup.content.module; [
<!ENTITY % confgroup.module "INCLUDE">
<![ %confgroup.module; [
<!ENTITY % local.confgroup.attrib "">
<!ENTITY % confgroup.role.attrib "%role.attrib;">
<!ELEMENT ConfGroup - - ((ConfDates|ConfTitle|ConfNum|Address|ConfSponsor)*)>
<!ATTLIST ConfGroup
		%common.attrib;
		%confgroup.role.attrib;
		%local.confgroup.attrib;
>
<!--end of confgroup.module-->]]>

  <!ENTITY % confdates.module "INCLUDE">
  <![ %confdates.module; [
  <!ENTITY % local.confdates.attrib "">
  <!ENTITY % confdates.role.attrib "%role.attrib;">
  <!ELEMENT ConfDates - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfDates
		%common.attrib;
		%confdates.role.attrib;
		%local.confdates.attrib;
>
  <!--end of confdates.module-->]]>

  <!ENTITY % conftitle.module "INCLUDE">
  <![ %conftitle.module; [
  <!ENTITY % local.conftitle.attrib "">
  <!ENTITY % conftitle.role.attrib "%role.attrib;">
  <!ELEMENT ConfTitle - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfTitle
		%common.attrib;
		%conftitle.role.attrib;
		%local.conftitle.attrib;
>
  <!--end of conftitle.module-->]]>

  <!ENTITY % confnum.module "INCLUDE">
  <![ %confnum.module; [
  <!ENTITY % local.confnum.attrib "">
  <!ENTITY % confnum.role.attrib "%role.attrib;">
  <!ELEMENT ConfNum - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfNum
		%common.attrib;
		%confnum.role.attrib;
		%local.confnum.attrib;
>
  <!--end of confnum.module-->]]>

  <!--ELEMENT Address (defined elsewhere in this section)-->

  <!ENTITY % confsponsor.module "INCLUDE">
  <![ %confsponsor.module; [
  <!ENTITY % local.confsponsor.attrib "">
  <!ENTITY % confsponsor.role.attrib "%role.attrib;">
  <!ELEMENT ConfSponsor - - ((%docinfo.char.mix;)+)>
  <!ATTLIST ConfSponsor
		%common.attrib;
		%confsponsor.role.attrib;
		%local.confsponsor.attrib;
>
  <!--end of confsponsor.module-->]]>
<!--end of confgroup.content.module-->]]>

<!-- ContractNum ...................... -->

<!ENTITY % contractnum.module "INCLUDE">
<![ %contractnum.module; [
<!ENTITY % local.contractnum.attrib "">
<!ENTITY % contractnum.role.attrib "%role.attrib;">
<!ELEMENT ContractNum - - ((%docinfo.char.mix;)+)>
<!ATTLIST ContractNum
		%common.attrib;
		%contractnum.role.attrib;
		%local.contractnum.attrib;
>
<!--end of contractnum.module-->]]>

<!-- ContractSponsor .................. -->

<!ENTITY % contractsponsor.module "INCLUDE">
<![ %contractsponsor.module; [
<!ENTITY % local.contractsponsor.attrib "">
<!ENTITY % contractsponsor.role.attrib "%role.attrib;">
<!ELEMENT ContractSponsor - - ((%docinfo.char.mix;)+)>
<!ATTLIST ContractSponsor
		%common.attrib;
		%contractsponsor.role.attrib;
		%local.contractsponsor.attrib;
>
<!--end of contractsponsor.module-->]]>

<!-- Copyright ........................ -->

<!ENTITY % copyright.content.module "INCLUDE">
<![ %copyright.content.module; [
<!ENTITY % copyright.module "INCLUDE">
<![ %copyright.module; [
<!ENTITY % local.copyright.attrib "">
<!ENTITY % copyright.role.attrib "%role.attrib;">
<!ELEMENT Copyright - - (Year+, Holder*)>
<!ATTLIST Copyright
		%common.attrib;
		%copyright.role.attrib;
		%local.copyright.attrib;
>
<!--end of copyright.module-->]]>

  <!ENTITY % year.module "INCLUDE">
  <![ %year.module; [
  <!ENTITY % local.year.attrib "">
  <!ENTITY % year.role.attrib "%role.attrib;">
  <!ELEMENT Year - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Year
		%common.attrib;
		%year.role.attrib;
		%local.year.attrib;
>
  <!--end of year.module-->]]>

  <!ENTITY % holder.module "INCLUDE">
  <![ %holder.module; [
  <!ENTITY % local.holder.attrib "">
  <!ENTITY % holder.role.attrib "%role.attrib;">
  <!ELEMENT Holder - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Holder
		%common.attrib;
		%holder.role.attrib;
		%local.holder.attrib;
>
  <!--end of holder.module-->]]>
<!--end of copyright.content.module-->]]>

<!-- CorpAuthor ....................... -->

<!ENTITY % corpauthor.module "INCLUDE">
<![ %corpauthor.module; [
<!ENTITY % local.corpauthor.attrib "">
<!ENTITY % corpauthor.role.attrib "%role.attrib;">
<!ELEMENT CorpAuthor - - ((%docinfo.char.mix;)+)>
<!ATTLIST CorpAuthor
		%common.attrib;
		%corpauthor.role.attrib;
		%local.corpauthor.attrib;
>
<!--end of corpauthor.module-->]]>

<!-- CorpName ......................... -->

<!ENTITY % corpname.module "INCLUDE">
<![ %corpname.module; [
<!ENTITY % local.corpname.attrib "">
<!ELEMENT CorpName - - ((%docinfo.char.mix;)+)>
<!ENTITY % corpname.role.attrib "%role.attrib;">
<!ATTLIST CorpName
		%common.attrib;
		%corpname.role.attrib;
		%local.corpname.attrib;
>
<!--end of corpname.module-->]]>

<!-- Date ............................. -->

<!ENTITY % date.module "INCLUDE">
<![ %date.module; [
<!ENTITY % local.date.attrib "">
<!ENTITY % date.role.attrib "%role.attrib;">
<!ELEMENT Date - - ((%docinfo.char.mix;)+)>
<!ATTLIST Date
		%common.attrib;
		%date.role.attrib;
		%local.date.attrib;
>
<!--end of date.module-->]]>

<!-- Edition .......................... -->

<!ENTITY % edition.module "INCLUDE">
<![ %edition.module; [
<!ENTITY % local.edition.attrib "">
<!ENTITY % edition.role.attrib "%role.attrib;">
<!ELEMENT Edition - - ((%docinfo.char.mix;)+)>
<!ATTLIST Edition
		%common.attrib;
		%edition.role.attrib;
		%local.edition.attrib;
>
<!--end of edition.module-->]]>

<!-- Editor ........................... -->

<!ENTITY % editor.module "INCLUDE">
<![ %editor.module; [
<!--FUTURE USE (V4.0):
......................
AuthorBlurb and Affiliation will be removed from %person.ident.mix; and a new 
wrapper element created to allow association of those two elements with 
Editor name information.
......................
-->
<!ENTITY % local.editor.attrib "">
<!ENTITY % editor.role.attrib "%role.attrib;">
<!ELEMENT Editor - - ((%person.ident.mix;)+)>
<!ATTLIST Editor
		%common.attrib;
		%editor.role.attrib;
		%local.editor.attrib;
>
  <!--(see "Personal identity elements" for %person.ident.mix;)-->
<!--end of editor.module-->]]>

<!-- ISBN ............................. -->

<!ENTITY % isbn.module "INCLUDE">
<![ %isbn.module; [
<!ENTITY % local.isbn.attrib "">
<!ENTITY % isbn.role.attrib "%role.attrib;">
<!ELEMENT ISBN - - ((%docinfo.char.mix;)+)>
<!ATTLIST ISBN
		%common.attrib;
		%isbn.role.attrib;
		%local.isbn.attrib;
>
<!--end of isbn.module-->]]>

<!-- ISSN ............................. -->

<!ENTITY % issn.module "INCLUDE">
<![ %issn.module; [
<!ENTITY % local.issn.attrib "">
<!ENTITY % issn.role.attrib "%role.attrib;">
<!ELEMENT ISSN - - ((%docinfo.char.mix;)+)>
<!ATTLIST ISSN
		%common.attrib;
		%issn.role.attrib;
		%local.issn.attrib;
>
<!--end of issn.module-->]]>

<!-- InvPartNumber .................... -->

<!ENTITY % invpartnumber.module "INCLUDE">
<![ %invpartnumber.module; [
<!ENTITY % local.invpartnumber.attrib "">
<!ENTITY % invpartnumber.role.attrib "%role.attrib;">
<!ELEMENT InvPartNumber - - ((%docinfo.char.mix;)+)>
<!ATTLIST InvPartNumber
		%common.attrib;
		%invpartnumber.role.attrib;
		%local.invpartnumber.attrib;
>
<!--end of invpartnumber.module-->]]>

<!-- IssueNum ......................... -->

<!ENTITY % issuenum.module "INCLUDE">
<![ %issuenum.module; [
<!ENTITY % local.issuenum.attrib "">
<!ENTITY % issuenum.role.attrib "%role.attrib;">
<!ELEMENT IssueNum - - ((%docinfo.char.mix;)+)>
<!ATTLIST IssueNum
		%common.attrib;
		%issuenum.role.attrib;
		%local.issuenum.attrib;
>
<!--end of issuenum.module-->]]>

<!-- LegalNotice ...................... -->

<!ENTITY % legalnotice.module "INCLUDE">
<![ %legalnotice.module; [
<!ENTITY % local.legalnotice.attrib "">
<!ENTITY % legalnotice.role.attrib "%role.attrib;">
<!ELEMENT LegalNotice - - (Title?, (%legalnotice.mix;)+) -(%formal.class;)>
<!ATTLIST LegalNotice
		%common.attrib;
		%legalnotice.role.attrib;
		%local.legalnotice.attrib;
>
<!--end of legalnotice.module-->]]>

<!-- ModeSpec ......................... -->

<!ENTITY % modespec.module "INCLUDE">
<![ %modespec.module; [
<!ENTITY % local.modespec.attrib "">
<!ENTITY % modespec.role.attrib "%role.attrib;">
<!ELEMENT ModeSpec - - ((%docinfo.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST ModeSpec
		--
		Application: Type of action required for completion
		of the links to which the ModeSpec is relevant (e.g.,
		retrieval query)
		--
		Application	NOTATION
				(%notation.class;)	#IMPLIED
		%common.attrib;
		%modespec.role.attrib;
		%local.modespec.attrib;
>
<!--end of modespec.module-->]]>

<!-- OrgName .......................... -->

<!ENTITY % orgname.module "INCLUDE">
<![ %orgname.module; [
<!ENTITY % local.orgname.attrib "">
<!ENTITY % orgname.role.attrib "%role.attrib;">
<!ELEMENT OrgName - - ((%docinfo.char.mix;)+)>
<!ATTLIST OrgName
		%common.attrib;
		%orgname.role.attrib;
		%local.orgname.attrib;
>
<!--end of orgname.module-->]]>

<!-- OtherCredit ...................... -->

<!ENTITY % othercredit.module "INCLUDE">
<![ %othercredit.module; [
<!--FUTURE USE (V4.0):
......................
AuthorBlurb and Affiliation will be removed from %person.ident.mix; and a new 
wrapper element created to allow association of those two elements with 
OtherCredit name information.
......................
-->
<!ENTITY % local.othercredit.attrib "">
<!ENTITY % othercredit.role.attrib "%role.attrib;">
<!ELEMENT OtherCredit - - ((%person.ident.mix;)+)>
<!ATTLIST OtherCredit
		%common.attrib;
		%othercredit.role.attrib;
		%local.othercredit.attrib;
>
  <!--(see "Personal identity elements" for %person.ident.mix;)-->
<!--end of othercredit.module-->]]>

<!-- PageNums ......................... -->

<!ENTITY % pagenums.module "INCLUDE">
<![ %pagenums.module; [
<!ENTITY % local.pagenums.attrib "">
<!ENTITY % pagenums.role.attrib "%role.attrib;">
<!ELEMENT PageNums - - ((%docinfo.char.mix;)+)>
<!ATTLIST PageNums
		%common.attrib;
		%pagenums.role.attrib;
		%local.pagenums.attrib;
>
<!--end of pagenums.module-->]]>

<!-- Personal identity elements ....... -->

<!-- These elements are used only within Author, Editor, and 
OtherCredit. -->

<!ENTITY % person.ident.module "INCLUDE">
<![ %person.ident.module; [
<!--FUTURE USE (V4.0):
......................
AuthorBlurb and Affiliation will be removed from %person.ident.mix; and
a new wrapper element created to allow association of those two elements
with Contrib name information.
......................
-->
  <!ENTITY % contrib.module "INCLUDE">
  <![ %contrib.module; [
  <!ENTITY % local.contrib.attrib "">
  <!ENTITY % contrib.role.attrib "%role.attrib;">
  <!ELEMENT Contrib - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Contrib
		%common.attrib;
		%contrib.role.attrib;
		%local.contrib.attrib;
>
  <!--end of contrib.module-->]]>

  <!ENTITY % firstname.module "INCLUDE">
  <![ %firstname.module; [
  <!ENTITY % local.firstname.attrib "">
  <!ENTITY % firstname.role.attrib "%role.attrib;">
  <!ELEMENT FirstName - - ((%docinfo.char.mix;)+)>
  <!ATTLIST FirstName
		%common.attrib;
		%firstname.role.attrib;
		%local.firstname.attrib;
>
  <!--end of firstname.module-->]]>

  <!ENTITY % honorific.module "INCLUDE">
  <![ %honorific.module; [
  <!ENTITY % local.honorific.attrib "">
  <!ENTITY % honorific.role.attrib "%role.attrib;">
  <!ELEMENT Honorific - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Honorific
		%common.attrib;
		%honorific.role.attrib;
		%local.honorific.attrib;
>
  <!--end of honorific.module-->]]>

  <!ENTITY % lineage.module "INCLUDE">
  <![ %lineage.module; [
  <!ENTITY % local.lineage.attrib "">
  <!ENTITY % lineage.role.attrib "%role.attrib;">
  <!ELEMENT Lineage - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Lineage
		%common.attrib;
		%lineage.role.attrib;
		%local.lineage.attrib;
>
  <!--end of lineage.module-->]]>

  <!ENTITY % othername.module "INCLUDE">
  <![ %othername.module; [
  <!ENTITY % local.othername.attrib "">
  <!ENTITY % othername.role.attrib "%role.attrib;">
  <!ELEMENT OtherName - - ((%docinfo.char.mix;)+)>
  <!ATTLIST OtherName
		%common.attrib;
		%othername.role.attrib;
		%local.othername.attrib;
>
  <!--end of othername.module-->]]>

  <!ENTITY % surname.module "INCLUDE">
  <![ %surname.module; [
  <!ENTITY % local.surname.attrib "">
  <!ENTITY % surname.role.attrib "%role.attrib;">
  <!ELEMENT Surname - - ((%docinfo.char.mix;)+)>
  <!ATTLIST Surname
		%common.attrib;
		%surname.role.attrib;
		%local.surname.attrib;
>
  <!--end of surname.module-->]]>
<!--end of person.ident.module-->]]>

<!-- PrintHistory ..................... -->

<!ENTITY % printhistory.module "INCLUDE">
<![ %printhistory.module; [
<!ENTITY % local.printhistory.attrib "">
<!ENTITY % printhistory.role.attrib "%role.attrib;">
<!ELEMENT PrintHistory - - ((%para.class;)+)>
<!ATTLIST PrintHistory
		%common.attrib;
		%printhistory.role.attrib;
		%local.printhistory.attrib;
>
<!--end of printhistory.module-->]]>

<!-- ProductName ...................... -->

<!ENTITY % productname.module "INCLUDE">
<![ %productname.module; [
<!ENTITY % local.productname.attrib "">
<!ENTITY % productname.role.attrib "%role.attrib;">
<!ELEMENT ProductName - - ((%para.char.mix;)+)>
<!ATTLIST ProductName
		--
		Class: More precisely identifies the item the element names
		--
		Class		(Service
				|Trade
				|Registered
				|Copyright)	Trade
		%common.attrib;
		%productname.role.attrib;
		%local.productname.attrib;
>
<!--end of productname.module-->]]>

<!-- ProductNumber .................... -->

<!ENTITY % productnumber.module "INCLUDE">
<![ %productnumber.module; [
<!ENTITY % local.productnumber.attrib "">
<!ENTITY % productnumber.role.attrib "%role.attrib;">
<!ELEMENT ProductNumber - - ((%docinfo.char.mix;)+)>
<!ATTLIST ProductNumber
		%common.attrib;
		%productnumber.role.attrib;
		%local.productnumber.attrib;
>
<!--end of productnumber.module-->]]>

<!-- PubDate .......................... -->

<!ENTITY % pubdate.module "INCLUDE">
<![ %pubdate.module; [
<!ENTITY % local.pubdate.attrib "">
<!ENTITY % pubdate.role.attrib "%role.attrib;">
<!ELEMENT PubDate - - ((%docinfo.char.mix;)+)>
<!ATTLIST PubDate
		%common.attrib;
		%pubdate.role.attrib;
		%local.pubdate.attrib;
>
<!--end of pubdate.module-->]]>

<!-- Publisher ........................ -->

<!ENTITY % publisher.content.module "INCLUDE">
<![ %publisher.content.module; [
<!ENTITY % publisher.module "INCLUDE">
<![ %publisher.module; [
<!ENTITY % local.publisher.attrib "">
<!ENTITY % publisher.role.attrib "%role.attrib;">
<!ELEMENT Publisher - - (PublisherName, Address*)>
<!ATTLIST Publisher
		%common.attrib;
		%publisher.role.attrib;
		%local.publisher.attrib;
>
<!--end of publisher.module-->]]>

  <!ENTITY % publishername.module "INCLUDE">
  <![ %publishername.module; [
  <!ENTITY % local.publishername.attrib "">
  <!ENTITY % publishername.role.attrib "%role.attrib;">
  <!ELEMENT PublisherName - - ((%docinfo.char.mix;)+)>
  <!ATTLIST PublisherName
		%common.attrib;
		%publishername.role.attrib;
		%local.publishername.attrib;
>
  <!--end of publishername.module-->]]>

  <!--ELEMENT Address (defined elsewhere in this section)-->
<!--end of publisher.content.module-->]]>

<!-- PubsNumber ....................... -->

<!ENTITY % pubsnumber.module "INCLUDE">
<![ %pubsnumber.module; [
<!ENTITY % local.pubsnumber.attrib "">
<!ENTITY % pubsnumber.role.attrib "%role.attrib;">
<!ELEMENT PubsNumber - - ((%docinfo.char.mix;)+)>
<!ATTLIST PubsNumber
		%common.attrib;
		%pubsnumber.role.attrib;
		%local.pubsnumber.attrib;
>
<!--end of pubsnumber.module-->]]>

<!-- ReleaseInfo ...................... -->

<!ENTITY % releaseinfo.module "INCLUDE">
<![ %releaseinfo.module; [
<!ENTITY % local.releaseinfo.attrib "">
<!ENTITY % releaseinfo.role.attrib "%role.attrib;">
<!ELEMENT ReleaseInfo - - ((%docinfo.char.mix;)+)>
<!ATTLIST ReleaseInfo
		%common.attrib;
		%releaseinfo.role.attrib;
		%local.releaseinfo.attrib;
>
<!--end of releaseinfo.module-->]]>

<!-- RevHistory ....................... -->

<!ENTITY % revhistory.content.module "INCLUDE">
<![ %revhistory.content.module; [
<!ENTITY % revhistory.module "INCLUDE">
<![ %revhistory.module; [
<!ENTITY % local.revhistory.attrib "">
<!ENTITY % revhistory.role.attrib "%role.attrib;">
<!ELEMENT RevHistory - - (Revision+)>
<!ATTLIST RevHistory
		%common.attrib;
		%revhistory.role.attrib;
		%local.revhistory.attrib;
>
<!--end of revhistory.module-->]]>

  <!ENTITY % revision.module "INCLUDE">
  <![ %revision.module; [
  <!ENTITY % local.revision.attrib "">
  <!ENTITY % revision.role.attrib "%role.attrib;">
  <!ELEMENT Revision - - (RevNumber, Date, AuthorInitials*, RevRemark?)>
  <!ATTLIST Revision
		%common.attrib;
		%revision.role.attrib;
		%local.revision.attrib;
>
  <!--end of revision.module-->]]>

  <!ENTITY % revnumber.module "INCLUDE">
  <![ %revnumber.module; [
  <!ENTITY % local.revnumber.attrib "">
  <!ENTITY % revnumber.role.attrib "%role.attrib;">
  <!ELEMENT RevNumber - - ((%docinfo.char.mix;)+)>
  <!ATTLIST RevNumber
		%common.attrib;
		%revnumber.role.attrib;
		%local.revnumber.attrib;
>
  <!--end of revnumber.module-->]]>

  <!--ELEMENT Date (defined elsewhere in this section)-->
  <!--ELEMENT AuthorInitials (defined elsewhere in this section)-->

  <!ENTITY % revremark.module "INCLUDE">
  <![ %revremark.module; [
  <!ENTITY % local.revremark.attrib "">
  <!ENTITY % revremark.role.attrib "%role.attrib;">
  <!ELEMENT RevRemark - - ((%docinfo.char.mix;)+)>
  <!ATTLIST RevRemark
		%common.attrib;
		%revremark.role.attrib;
		%local.revremark.attrib;
>
  <!--end of revremark.module-->]]>
<!--end of revhistory.content.module-->]]>

<!-- SeriesVolNums .................... -->

<!ENTITY % seriesvolnums.module "INCLUDE">
<![ %seriesvolnums.module; [
<!ENTITY % local.seriesvolnums.attrib "">
<!ENTITY % seriesvolnums.role.attrib "%role.attrib;">
<!ELEMENT SeriesVolNums - - ((%docinfo.char.mix;)+)>
<!ATTLIST SeriesVolNums
		%common.attrib;
		%seriesvolnums.role.attrib;
		%local.seriesvolnums.attrib;
>
<!--end of seriesvolnums.module-->]]>

<!-- VolumeNum ........................ -->

<!ENTITY % volumenum.module "INCLUDE">
<![ %volumenum.module; [
<!ENTITY % local.volumenum.attrib "">
<!ENTITY % volumenum.role.attrib "%role.attrib;">
<!ELEMENT VolumeNum - - ((%docinfo.char.mix;)+)>
<!ATTLIST VolumeNum
		%common.attrib;
		%volumenum.role.attrib;
		%local.volumenum.attrib;
>
<!--end of volumenum.module-->]]>

<!-- .................................. -->

<!--end of docinfo.content.module-->]]>

<!-- ...................................................................... -->
<!-- Inline, link, and ubiquitous elements ................................ -->

<!-- Technical and computer terms ......................................... -->

<!ENTITY % accel.module "INCLUDE">
<![ %accel.module; [
<!ENTITY % local.accel.attrib "">
<!ENTITY % accel.role.attrib "%role.attrib;">
<!ELEMENT Accel - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Accel
		%common.attrib;
		%accel.role.attrib;
		%local.accel.attrib;
>
<!--end of accel.module-->]]>

<!ENTITY % action.module "INCLUDE">
<![ %action.module; [
<!--FUTURE USE (V4.0):
......................
Action will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.action.attrib "">
<!ENTITY % action.role.attrib "%role.attrib;">
<!ELEMENT Action - - ((%cptr.char.mix;)+)>
<!ATTLIST Action
		%moreinfo.attrib;
		%common.attrib;
		%action.role.attrib;
		%local.action.attrib;
>
<!--end of action.module-->]]>

<!ENTITY % application.module "INCLUDE">
<![ %application.module; [
<!ENTITY % local.application.attrib "">
<!ENTITY % application.role.attrib "%role.attrib;">
<!ELEMENT Application - - ((%para.char.mix;)+)>
<!ATTLIST Application
		Class 		(Hardware
				|Software)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%application.role.attrib;
		%local.application.attrib;
>
<!--end of application.module-->]]>

<!ENTITY % classname.module "INCLUDE">
<![ %classname.module; [
<!ENTITY % local.classname.attrib "">
<!ENTITY % classname.role.attrib "%role.attrib;">
<!ELEMENT ClassName - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ClassName
		%common.attrib;
		%classname.role.attrib;
		%local.classname.attrib;
>
<!--end of classname.module-->]]>

<!ENTITY % co.module "INCLUDE">
<![ %co.module; [
<!ENTITY % local.co.attrib "">
<!-- CO is a callout area of the LineColumn unit type (a single character 
     position); the position is directly indicated by the location of CO. -->
<!ENTITY % co.role.attrib "%role.attrib;">
<!ELEMENT CO - O EMPTY>
<!ATTLIST CO
		%label.attrib; --bug number/symbol override or initialization--
		%linkends.attrib; --to any related information--
		%idreq.common.attrib;
		%co.role.attrib;
		%local.co.attrib;
>
<!--end of co.module-->]]>

<!ENTITY % command.module "INCLUDE">
<![ %command.module; [
<!--FUTURE USE (V4.0):
......................
Command will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.command.attrib "">
<!ENTITY % command.role.attrib "%role.attrib;">
<!ELEMENT Command - - ((%cptr.char.mix;)+)>
<!ATTLIST Command
		%moreinfo.attrib;
		%common.attrib;
		%command.role.attrib;
		%local.command.attrib;
>
<!--end of command.module-->]]>

<!ENTITY % computeroutput.module "INCLUDE">
<![ %computeroutput.module; [
<!ENTITY % local.computeroutput.attrib "">
<!ENTITY % computeroutput.role.attrib "%role.attrib;">
<!ELEMENT ComputerOutput - - ((%cptr.char.mix;)+)>
<!ATTLIST ComputerOutput
		%moreinfo.attrib;
		%common.attrib;
		%computeroutput.role.attrib;
		%local.computeroutput.attrib;
>
<!--end of computeroutput.module-->]]>

<!ENTITY % database.module "INCLUDE">
<![ %database.module; [
<!--FUTURE USE (V4.0):
......................
Database will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.database.attrib "">
<!ENTITY % database.role.attrib "%role.attrib;">
<!ELEMENT Database - - ((%cptr.char.mix;)+)>
<!ATTLIST Database
		--
		Class: Type of database the element names; no default
		--
		Class 		(Name
				|Table
				|Field
				|Key1
				|Key2
				|Record)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%database.role.attrib;
		%local.database.attrib;
>
<!--end of database.module-->]]>

<!ENTITY % email.module "INCLUDE">
<![ %email.module; [
<!ENTITY % local.email.attrib "">
<!ENTITY % email.role.attrib "%role.attrib;">
<!ELEMENT Email - - ((%docinfo.char.mix;)+)>
<!ATTLIST Email
		%common.attrib;
		%email.role.attrib;
		%local.email.attrib;
>
<!--end of email.module-->]]>

<!ENTITY % envar.module "INCLUDE">
<![ %envar.module; [
<!ENTITY % local.envar.attrib "">
<!ENTITY % envar.role.attrib "%role.attrib;">
<!ELEMENT EnVar - - ((%smallcptr.char.mix;)+)>
<!ATTLIST EnVar
		%common.attrib;
		%envar.role.attrib;
		%local.envar.attrib;
>
<!--end of envar.module-->]]>


<!ENTITY % errorcode.module "INCLUDE">
<![ %errorcode.module; [
<!ENTITY % local.errorcode.attrib "">
<!ENTITY % errorcode.role.attrib "%role.attrib;">
<!ELEMENT ErrorCode - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ErrorCode
		%moreinfo.attrib;
		%common.attrib;
		%errorcode.role.attrib;
		%local.errorcode.attrib;
>
<!--end of errorcode.module-->]]>

<!ENTITY % errorname.module "INCLUDE">
<![ %errorname.module; [
<!ENTITY % local.errorname.attrib "">
<!ENTITY % errorname.role.attrib "%role.attrib;">
<!ELEMENT ErrorName - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ErrorName
		%common.attrib;
		%errorname.role.attrib;
		%local.errorname.attrib;
>
<!--end of errorname.module-->]]>

<!ENTITY % errortype.module "INCLUDE">
<![ %errortype.module; [
<!ENTITY % local.errortype.attrib "">
<!ENTITY % errortype.role.attrib "%role.attrib;">
<!ELEMENT ErrorType - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ErrorType
		%common.attrib;
		%errortype.role.attrib;
		%local.errortype.attrib;
>
<!--end of errortype.module-->]]>

<!ENTITY % filename.module "INCLUDE">
<![ %filename.module; [
<!--FUTURE USE (V4.0):
......................
Filename will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.filename.attrib "">
<!ENTITY % filename.role.attrib "%role.attrib;">
<!ELEMENT Filename - - ((%cptr.char.mix;)+)>
<!ATTLIST Filename
		--
		Class: Type of filename the element names; no default
		--
		Class		(HeaderFile
				|SymLink
				|Directory)	#IMPLIED
		--
		Path: Search path (possibly system-specific) in which 
		file can be found
		--
		Path		CDATA		#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%filename.role.attrib;
		%local.filename.attrib;
>
<!--end of filename.module-->]]>

<!ENTITY % function.module "INCLUDE">
<![ %function.module; [
<!ENTITY % local.function.attrib "">
<!ENTITY % function.role.attrib "%role.attrib;">
<!ELEMENT Function - - ((%cptr.char.mix;)+)>
<!ATTLIST Function
		%moreinfo.attrib;
		%common.attrib;
		%function.role.attrib;
		%local.function.attrib;
>
<!--end of function.module-->]]>

<!ENTITY % guibutton.module "INCLUDE">
<![ %guibutton.module; [
<!ENTITY % local.guibutton.attrib "">
<!ENTITY % guibutton.role.attrib "%role.attrib;">
<!ELEMENT GUIButton - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIButton
		%moreinfo.attrib;
		%common.attrib;
		%guibutton.role.attrib;
		%local.guibutton.attrib;
>
<!--end of guibutton.module-->]]>

<!ENTITY % guiicon.module "INCLUDE">
<![ %guiicon.module; [
<!ENTITY % local.guiicon.attrib "">
<!ENTITY % guiicon.role.attrib "%role.attrib;">
<!ELEMENT GUIIcon - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIIcon
		%moreinfo.attrib;
		%common.attrib;
		%guiicon.role.attrib;
		%local.guiicon.attrib;
>
<!--end of guiicon.module-->]]>

<!ENTITY % guilabel.module "INCLUDE">
<![ %guilabel.module; [
<!ENTITY % local.guilabel.attrib "">
<!ENTITY % guilabel.role.attrib "%role.attrib;">
<!ELEMENT GUILabel - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUILabel
		%moreinfo.attrib;
		%common.attrib;
		%guilabel.role.attrib;
		%local.guilabel.attrib;
>
<!--end of guilabel.module-->]]>

<!ENTITY % guimenu.module "INCLUDE">
<![ %guimenu.module; [
<!ENTITY % local.guimenu.attrib "">
<!ENTITY % guimenu.role.attrib "%role.attrib;">
<!ELEMENT GUIMenu - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIMenu
		%moreinfo.attrib;
		%common.attrib;
		%guimenu.role.attrib;
		%local.guimenu.attrib;
>
<!--end of guimenu.module-->]]>

<!ENTITY % guimenuitem.module "INCLUDE">
<![ %guimenuitem.module; [
<!ENTITY % local.guimenuitem.attrib "">
<!ENTITY % guimenuitem.role.attrib "%role.attrib;">
<!ELEMENT GUIMenuItem - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUIMenuItem
		%moreinfo.attrib;
		%common.attrib;
		%guimenuitem.role.attrib;
		%local.guimenuitem.attrib;
>
<!--end of guimenuitem.module-->]]>

<!ENTITY % guisubmenu.module "INCLUDE">
<![ %guisubmenu.module; [
<!ENTITY % local.guisubmenu.attrib "">
<!ENTITY % guisubmenu.role.attrib "%role.attrib;">
<!ELEMENT GUISubmenu - - ((%smallcptr.char.mix;|Accel)+)>
<!ATTLIST GUISubmenu
		%moreinfo.attrib;
		%common.attrib;
		%guisubmenu.role.attrib;
		%local.guisubmenu.attrib;
>
<!--end of guisubmenu.module-->]]>

<!ENTITY % hardware.module "INCLUDE">
<![ %hardware.module; [
<!--FUTURE USE (V4.0):
......................
Hardware will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.hardware.attrib "">
<!ENTITY % hardware.role.attrib "%role.attrib;">
<!ELEMENT Hardware - - ((%cptr.char.mix;)+)>
<!ATTLIST Hardware
		%moreinfo.attrib;
		%common.attrib;
		%hardware.role.attrib;
		%local.hardware.attrib;
>
<!--end of hardware.module-->]]>

<!ENTITY % interface.module "INCLUDE">
<![ %interface.module; [
<!--FUTURE USE (V4.0):
......................
Interface will no longer have a Class attribute; if you want to subclass
interface information, use GUIButton, GUIIcon, GUILabel, GUIMenu,
GUIMenuItem, or GUISubmenu, or use a Role value on Interface.  Also,
Interface will have its  content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.interface.attrib "">
<!ENTITY % interface.role.attrib "%role.attrib;">
<!ELEMENT Interface - - ((%cptr.char.mix;|Accel)+)>
<!ATTLIST Interface
		--
		Class: Type of the Interface item; no default
		--
		Class 		(Button
				|Icon
				|Menu
				|MenuItem)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%interface.role.attrib;
		%local.interface.attrib;
>
<!--end of interface.module-->]]>

<!ENTITY % interfacedefinition.module "INCLUDE">
<![ %interfacedefinition.module; [
<!--FUTURE USE (V4.0):
......................
InterfaceDefinition will be discarded. 
......................
-->
<!ENTITY % local.interfacedefinition.attrib "">
<!ENTITY % interfacedefinition.role.attrib "%role.attrib;">
<!ELEMENT InterfaceDefinition - - ((%cptr.char.mix;)+)>
<!ATTLIST InterfaceDefinition
		%moreinfo.attrib;
		%common.attrib;
		%interfacedefinition.role.attrib;
		%local.interfacedefinition.attrib;
>
<!--end of interfacedefinition.module-->]]>

<!ENTITY % keycap.module "INCLUDE">
<![ %keycap.module; [
<!--FUTURE USE (V4.0):
......................
KeyCap will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.keycap.attrib "">
<!ENTITY % keycap.role.attrib "%role.attrib;">
<!ELEMENT KeyCap - - ((%cptr.char.mix;)+)>
<!ATTLIST KeyCap
		%moreinfo.attrib;
		%common.attrib;
		%keycap.role.attrib;
		%local.keycap.attrib;
>
<!--end of keycap.module-->]]>

<!ENTITY % keycode.module "INCLUDE">
<![ %keycode.module; [
<!ENTITY % local.keycode.attrib "">
<!ENTITY % keycode.role.attrib "%role.attrib;">
<!ELEMENT KeyCode - - ((%smallcptr.char.mix;)+)>
<!ATTLIST KeyCode
		%common.attrib;
		%keycode.role.attrib;
		%local.keycode.attrib;
>
<!--end of keycode.module-->]]>

<!ENTITY % keycombo.module "INCLUDE">
<![ %keycombo.module; [
<!ENTITY % local.keycombo.attrib "">
<!ENTITY % keycombo.role.attrib "%role.attrib;">
<!ELEMENT KeyCombo - - ((KeyCap|KeyCombo|KeySym|MouseButton)+)>
<!ATTLIST KeyCombo
		%keyaction.attrib;
		%moreinfo.attrib;
		%common.attrib;
		%keycombo.role.attrib;
		%local.keycombo.attrib;
>
<!--end of keycombo.module-->]]>

<!ENTITY % keysym.module "INCLUDE">
<![ %keysym.module; [
<!ENTITY % local.keysym.attrib "">
<!ENTITY % keysysm.role.attrib "%role.attrib;">
<!ELEMENT KeySym - - ((%smallcptr.char.mix;)+)>
<!ATTLIST KeySym
		%common.attrib;
		%keysysm.role.attrib;
		%local.keysym.attrib;
>
<!--end of keysym.module-->]]>

<!ENTITY % lineannotation.module "INCLUDE">
<![ %lineannotation.module; [
<!ENTITY % local.lineannotation.attrib "">
<!ENTITY % lineannotation.role.attrib "%role.attrib;">
<!ELEMENT LineAnnotation - - ((%para.char.mix;)+)>
<!ATTLIST LineAnnotation
		%common.attrib;
		%lineannotation.role.attrib;
		%local.lineannotation.attrib;
>
<!--end of lineannotation.module-->]]>

<!ENTITY % literal.module "INCLUDE">
<![ %literal.module; [
<!--FUTURE USE (V4.0):
......................
Literal will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.literal.attrib "">
<!ENTITY % literal.role.attrib "%role.attrib;">
<!ELEMENT Literal - - ((%cptr.char.mix;)+)>
<!ATTLIST Literal
		%moreinfo.attrib;
		%common.attrib;
		%literal.role.attrib;
		%local.literal.attrib;
>
<!--end of literal.module-->]]>

<!ENTITY % markup.module "INCLUDE">
<![ %markup.module; [
<!ENTITY % local.markup.attrib "">
<!ENTITY % markup.role.attrib "%role.attrib;">
<!ELEMENT Markup - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Markup
		%common.attrib;
		%markup.role.attrib;
		%local.markup.attrib;
>
<!--end of markup.module-->]]>

<!ENTITY % medialabel.module "INCLUDE">
<![ %medialabel.module; [
<!ENTITY % local.medialabel.attrib "">
<!ENTITY % medialabel.role.attrib "%role.attrib;">
<!ELEMENT MediaLabel - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MediaLabel
		--
		Class: Type of medium named by the element; no default
		--
		Class 		(Cartridge
				|CDRom
				|Disk
				|Tape)		#IMPLIED
		%common.attrib;
		%medialabel.role.attrib;
		%local.medialabel.attrib;
>
<!--end of medialabel.module-->]]>

<!ENTITY % menuchoice.content.module "INCLUDE">
<![ %menuchoice.content.module; [
<!ENTITY % menuchoice.module "INCLUDE">
<![ %menuchoice.module; [
<!ENTITY % local.menuchoice.attrib "">
<!ENTITY % menuchoice.role.attrib "%role.attrib;">
<!ELEMENT MenuChoice - - (Shortcut?, (GUIButton|GUIIcon|GUILabel
		|GUIMenu|GUIMenuItem|GUISubmenu|Interface)+)>
<!ATTLIST MenuChoice
		%moreinfo.attrib;
		%common.attrib;
		%menuchoice.role.attrib;
		%local.menuchoice.attrib;
>
<!--end of menuchoice.module-->]]>

<!ENTITY % shortcut.module "INCLUDE">
<![ %shortcut.module; [
<!-- See also KeyCombo -->
<!ENTITY % local.shortcut.attrib "">
<!ENTITY % shortcut.role.attrib "%role.attrib;">
<!ELEMENT Shortcut - - ((KeyCap|KeyCombo|KeySym|MouseButton)+)>
<!ATTLIST Shortcut
		%keyaction.attrib;
		%moreinfo.attrib;
		%common.attrib;
		%shortcut.role.attrib;
		%local.shortcut.attrib;
>
<!--end of shortcut.module-->]]>
<!--end of menuchoice.content.module-->]]>

<!ENTITY % mousebutton.module "INCLUDE">
<![ %mousebutton.module; [
<!ENTITY % local.mousebutton.attrib "">
<!ENTITY % mousebutton.role.attrib "%role.attrib;">
<!ELEMENT MouseButton - - ((%smallcptr.char.mix;)+)>
<!ATTLIST MouseButton
		%moreinfo.attrib;
		%common.attrib;
		%mousebutton.role.attrib;
		%local.mousebutton.attrib;
>
<!--end of mousebutton.module-->]]>

<!ENTITY % msgtext.module "INCLUDE">
<![ %msgtext.module; [
<!ENTITY % local.msgtext.attrib "">
<!ENTITY % msgtext.role.attrib "%role.attrib;">
<!ELEMENT MsgText - - ((%component.mix;)+)>
<!ATTLIST MsgText
		%common.attrib;
		%msgtext.role.attrib;
		%local.msgtext.attrib;
>
<!--end of msgtext.module-->]]>

<!ENTITY % option.module "INCLUDE">
<![ %option.module; [
<!--FUTURE USE (V4.0):
......................
Option will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.option.attrib "">
<!ENTITY % option.role.attrib "%role.attrib;">
<!ELEMENT Option - - ((%cptr.char.mix;)+)>
<!ATTLIST Option
		%common.attrib;
		%option.role.attrib;
		%local.option.attrib;
>
<!--end of option.module-->]]>

<!ENTITY % optional.module "INCLUDE">
<![ %optional.module; [
<!ENTITY % local.optional.attrib "">
<!ENTITY % optional.role.attrib "%role.attrib;">
<!ELEMENT Optional - - ((%cptr.char.mix;)+)>
<!ATTLIST Optional
		%common.attrib;
		%optional.role.attrib;
		%local.optional.attrib;
>
<!--end of optional.module-->]]>

<!ENTITY % parameter.module "INCLUDE">
<![ %parameter.module; [
<!--FUTURE USE (V4.0):
......................
Parameter will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.parameter.attrib "">
<!ENTITY % parameter.role.attrib "%role.attrib;">
<!ELEMENT Parameter - - ((%cptr.char.mix;)+)>
<!ATTLIST Parameter
		--
		Class: Type of the Parameter; no default
		--
		Class 		(Command
				|Function
				|Option)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%parameter.role.attrib;
		%local.parameter.attrib;
>
<!--end of parameter.module-->]]>

<!ENTITY % prompt.module "INCLUDE">
<![ %prompt.module; [
<!ENTITY % local.prompt.attrib "">
<!ENTITY % prompt.role.attrib "%role.attrib;">
<!ELEMENT Prompt - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Prompt
		%moreinfo.attrib;
		%common.attrib;
		%prompt.role.attrib;
		%local.prompt.attrib;
>
<!--end of prompt.module-->]]>

<!ENTITY % property.module "INCLUDE">
<![ %property.module; [
<!--FUTURE USE (V4.0):
......................
Property will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.property.attrib "">
<!ENTITY % property.role.attrib "%role.attrib;">
<!ELEMENT Property - - ((%cptr.char.mix;)+)>
<!ATTLIST Property
		%moreinfo.attrib;
		%common.attrib;
		%property.role.attrib;
		%local.property.attrib;
>
<!--end of property.module-->]]>

<!ENTITY % replaceable.module "INCLUDE">
<![ %replaceable.module; [
<!ENTITY % local.replaceable.attrib "">
<!ENTITY % replaceable.role.attrib "%role.attrib;">
<!ELEMENT Replaceable - - ((#PCDATA 
		| %link.char.class; 
		| Optional
		| %base.char.class; 
		| %other.char.class; 
		| InlineGraphic)+)>
<!ATTLIST Replaceable
		--
		Class: Type of information the element represents; no
		default
		--
		Class		(Command
				|Function
				|Option
				|Parameter)	#IMPLIED
		%common.attrib;
		%replaceable.role.attrib;
		%local.replaceable.attrib;
>
<!--end of replaceable.module-->]]>

<!ENTITY % returnvalue.module "INCLUDE">
<![ %returnvalue.module; [
<!ENTITY % local.returnvalue.attrib "">
<!ENTITY % returnvalue.role.attrib "%role.attrib;">
<!ELEMENT ReturnValue - - ((%smallcptr.char.mix;)+)>
<!ATTLIST ReturnValue
		%common.attrib;
		%returnvalue.role.attrib;
		%local.returnvalue.attrib;
>
<!--end of returnvalue.module-->]]>

<!ENTITY % sgmltag.module "INCLUDE">
<![ %sgmltag.module; [
<!ENTITY % local.sgmltag.attrib "">
<!ENTITY % sgmltag.role.attrib "%role.attrib;">
<!ELEMENT SGMLTag - - ((%smallcptr.char.mix;)+)>
<!ATTLIST SGMLTag
		--
		Class: Type of SGML construct the element names; no default
		--
		Class 		(Attribute
				|AttValue
				|Element
				|EndTag
				|GenEntity
				|NumCharRef
				|ParamEntity
				|PI
				|StartTag
				|SGMLComment)	#IMPLIED
		%common.attrib;
		%sgmltag.role.attrib;
		%local.sgmltag.attrib;
>
<!--end of sgmltag.module-->]]>

<!ENTITY % structfield.module "INCLUDE">
<![ %structfield.module; [
<!ENTITY % local.structfield.attrib "">
<!ENTITY % structfield.role.attrib "%role.attrib;">
<!ELEMENT StructField - - ((%smallcptr.char.mix;)+)>
<!ATTLIST StructField
		%common.attrib;
		%structfield.role.attrib;
		%local.structfield.attrib;
>
<!--end of structfield.module-->]]>

<!ENTITY % structname.module "INCLUDE">
<![ %structname.module; [
<!ENTITY % local.structname.attrib "">
<!ENTITY % structname.role.attrib "%role.attrib;">
<!ELEMENT StructName - - ((%smallcptr.char.mix;)+)>
<!ATTLIST StructName
		%common.attrib;
		%structname.role.attrib;
		%local.structname.attrib;
>
<!--end of structname.module-->]]>

<!ENTITY % symbol.module "INCLUDE">
<![ %symbol.module; [
<!ENTITY % local.symbol.attrib "">
<!ENTITY % symbol.role.attrib "%role.attrib;">
<!ELEMENT Symbol - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Symbol
		--
		Class: Type of symbol; no default
		--
		Class		(Limit)		#IMPLIED
		%common.attrib;
		%symbol.role.attrib;
		%local.symbol.attrib;
>
<!--end of symbol.module-->]]>

<!ENTITY % systemitem.module "INCLUDE">
<![ %systemitem.module; [
<!--FUTURE USE (V4.0):
......................
SystemItem will have its content constrained to smallcptr.char.mix.
......................
-->
<!ENTITY % local.systemitem.attrib "">
<!ENTITY % systemitem.role.attrib "%role.attrib;">
<!ELEMENT SystemItem - - ((%cptr.char.mix; | Acronym)+)>
<!--FUTURE USE (V4.0):
......................
The EnvironVar and Prompt values of Class will be eliminated; 
use the EnVar and Prompt elements new in 3.0 instead.
......................
-->
<!ATTLIST SystemItem
		--
		Class: Type of system item the element names; no default
		--
		Class	(Constant
			|EnvironVar
			|Macro
			|OSname
			|Prompt
			|Resource
			|SystemName)	#IMPLIED
		%moreinfo.attrib;
		%common.attrib;
		%systemitem.role.attrib;
		%local.systemitem.attrib;
>
<!--end of systemitem.module-->]]>


<!ENTITY % token.module "INCLUDE">
<![ %token.module; [
<!ENTITY % local.token.attrib "">
<!ENTITY % token.role.attrib "%role.attrib;">
<!ELEMENT Token - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Token
		%common.attrib;
		%token.role.attrib;
		%local.token.attrib;
>
<!--end of token.module-->]]>

<!ENTITY % type.module "INCLUDE">
<![ %type.module; [
<!ENTITY % local.type.attrib "">
<!ENTITY % type.role.attrib "%role.attrib;">
<!ELEMENT Type - - ((%smallcptr.char.mix;)+)>
<!ATTLIST Type
		%common.attrib;
		%type.role.attrib;
		%local.type.attrib;
>
<!--end of type.module-->]]>

<!ENTITY % userinput.module "INCLUDE">
<![ %userinput.module; [
<!ENTITY % local.userinput.attrib "">
<!ENTITY % userinput.role.attrib "%role.attrib;">
<!ELEMENT UserInput - - ((%cptr.char.mix;)+)>
<!ATTLIST UserInput
		%moreinfo.attrib;
		%common.attrib;
		%userinput.role.attrib;
		%local.userinput.attrib;
>
<!--end of userinput.module-->]]>

<!-- General words and phrases ............................................ -->

<!ENTITY % abbrev.module "INCLUDE">
<![ %abbrev.module; [
<!ENTITY % local.abbrev.attrib "">
<!ENTITY % abbrev.role.attrib "%role.attrib;">
<!ELEMENT Abbrev - - ((%word.char.mix;)+)>
<!ATTLIST Abbrev
		%common.attrib;
		%abbrev.role.attrib;
		%local.abbrev.attrib;
>
<!--end of abbrev.module-->]]>

<!ENTITY % acronym.module "INCLUDE">
<![ %acronym.module; [
<!ENTITY % local.acronym.attrib "">
<!ENTITY % acronym.role.attrib "%role.attrib;">
<!ELEMENT Acronym - - ((%word.char.mix;)+) -(Acronym)>
<!ATTLIST Acronym
		%common.attrib;
		%acronym.role.attrib;
		%local.acronym.attrib;
>
<!--end of acronym.module-->]]>

<!ENTITY % citation.module "INCLUDE">
<![ %citation.module; [
<!ENTITY % local.citation.attrib "">
<!ENTITY % citation.role.attrib "%role.attrib;">
<!ELEMENT Citation - - ((%para.char.mix;)+)>
<!ATTLIST Citation
		%common.attrib;
		%citation.role.attrib;
		%local.citation.attrib;
>
<!--end of citation.module-->]]>

<!ENTITY % citerefentry.module "INCLUDE">
<![ %citerefentry.module; [
<!ENTITY % local.citerefentry.attrib "">
<!ENTITY % citerefentry.role.attrib "%role.attrib;">
<!ELEMENT CiteRefEntry - - (RefEntryTitle, ManVolNum?)>
<!ATTLIST CiteRefEntry
		%common.attrib;
		%citerefentry.role.attrib;
		%local.citerefentry.attrib;
>
<!--end of citerefentry.module-->]]>

<!ENTITY % refentrytitle.module "INCLUDE">
<![ %refentrytitle.module; [
<!ENTITY % local.refentrytitle.attrib "">
<!ENTITY % refentrytitle.role.attrib "%role.attrib;">
<!ELEMENT RefEntryTitle - O ((%para.char.mix;)+)>
<!ATTLIST RefEntryTitle
		%common.attrib;
		%refentrytitle.role.attrib;
		%local.refentrytitle.attrib;
>
<!--end of refentrytitle.module-->]]>

<!ENTITY % manvolnum.module "INCLUDE">
<![ %manvolnum.module; [
<!ENTITY % local.manvolnum.attrib "">
<!ENTITY % namvolnum.role.attrib "%role.attrib;">
<!ELEMENT ManVolNum - O ((%word.char.mix;)+)>
<!ATTLIST ManVolNum
		%common.attrib;
		%namvolnum.role.attrib;
		%local.manvolnum.attrib;
>
<!--end of manvolnum.module-->]]>

<!ENTITY % citetitle.module "INCLUDE">
<![ %citetitle.module; [
<!ENTITY % local.citetitle.attrib "">
<!ENTITY % citetitle.role.attrib "%role.attrib;">
<!ELEMENT CiteTitle - - ((%para.char.mix;)+)>
<!ATTLIST CiteTitle
		--
		Pubwork: Genre of published work cited; no default
		--
		Pubwork		(Article
				|Book
				|Chapter
				|Part
				|RefEntry
				|Section)	#IMPLIED
		%common.attrib;
		%citetitle.role.attrib;
		%local.citetitle.attrib;
>
<!--end of citetitle.module-->]]>

<!ENTITY % emphasis.module "INCLUDE">
<![ %emphasis.module; [
<!ENTITY % local.emphasis.attrib "">
<!ENTITY % emphasis.role.attrib "%role.attrib;">
<!ELEMENT Emphasis - - ((%para.char.mix;)+)>
<!ATTLIST Emphasis
		%common.attrib;
		%emphasis.role.attrib;
		%local.emphasis.attrib;
>
<!--end of emphasis.module-->]]>

<!ENTITY % firstterm.module "INCLUDE">
<![ %firstterm.module; [
<!ENTITY % local.firstterm.attrib "">
<!ENTITY % firstterm.role.attrib "%role.attrib;">
<!ELEMENT FirstTerm - - ((%word.char.mix;)+)>
<!ATTLIST FirstTerm
		%linkend.attrib; --to GlossEntry or other explanation--
		%common.attrib;
		%firstterm.role.attrib;
		%local.firstterm.attrib;
>
<!--end of firstterm.module-->]]>

<!ENTITY % foreignphrase.module "INCLUDE">
<![ %foreignphrase.module; [
<!ENTITY % local.foreignphrase.attrib "">
<!ENTITY % foreignphrase.role.attrib "%role.attrib;">
<!ELEMENT ForeignPhrase - - ((%para.char.mix;)+)>
<!ATTLIST ForeignPhrase
		%common.attrib;
		%foreignphrase.role.attrib;
		%local.foreignphrase.attrib;
>
<!--end of foreignphrase.module-->]]>

<!ENTITY % glossterm.module "INCLUDE">
<![ %glossterm.module; [
<!ENTITY % local.glossterm.attrib "">
<!ENTITY % glossterm.role.attrib "%role.attrib;">
<!ELEMENT GlossTerm - O ((%para.char.mix;)+)>
<!ATTLIST GlossTerm
		%linkend.attrib; --to GlossEntry if Glossterm used in text--
		--
		BaseForm: Provides the form of GlossTerm to be used
		for indexing
		--
		BaseForm	CDATA		#IMPLIED
		%common.attrib;
		%glossterm.role.attrib;
		%local.glossterm.attrib;
>
<!--end of glossterm.module-->]]>

<!ENTITY % phrase.module "INCLUDE">
<![ %phrase.module; [
<!ENTITY % local.phrase.attrib "">
<!ENTITY % phrase.role.attrib "%role.attrib;">
<!ELEMENT Phrase - - ((%para.char.mix;)+)>
<!ATTLIST Phrase
		%common.attrib;
		%phrase.role.attrib;
		%local.phrase.attrib;
>
<!--end of phrase.module-->]]>

<!ENTITY % quote.module "INCLUDE">
<![ %quote.module; [
<!ENTITY % local.quote.attrib "">
<!ENTITY % quote.role.attrib "%role.attrib;">
<!ELEMENT Quote - - ((%para.char.mix;)+)>
<!ATTLIST Quote
		%common.attrib;
		%quote.role.attrib;
		%local.quote.attrib;
>
<!--end of quote.module-->]]>

<!ENTITY % ssscript.module "INCLUDE">
<![ %ssscript.module; [
<!ENTITY % local.ssscript.attrib "">
<!ENTITY % ssscript.role.attrib "%role.attrib;">
<!ELEMENT (Subscript | Superscript) - - ((#PCDATA 
		| %link.char.class;
		| Emphasis
		| Replaceable 
		| Symbol 
		| InlineGraphic 
		| %base.char.class; 
		| %other.char.class;)+)
		-(%ubiq.mix;)>
<!ATTLIST (Subscript | Superscript)
		%common.attrib;
		%ssscript.role.attrib;
		%local.ssscript.attrib;
>
<!--end of ssscript.module-->]]>

<!ENTITY % trademark.module "INCLUDE">
<![ %trademark.module; [
<!ENTITY % local.trademark.attrib "">
<!ENTITY % trademark.role.attrib "%role.attrib;">
<!ELEMENT Trademark - - ((#PCDATA 
		| %link.char.class; 
		| %tech.char.class;
		| %base.char.class; 
		| %other.char.class; 
		| InlineGraphic
		| Emphasis)+)>
<!ATTLIST Trademark
		--
		Class: More precisely identifies the item the element names
		--
		Class		(Service
				|Trade
				|Registered
				|Copyright)	Trade
		%common.attrib;
		%trademark.role.attrib;
		%local.trademark.attrib;
>
<!--end of trademark.module-->]]>

<!ENTITY % wordasword.module "INCLUDE">
<![ %wordasword.module; [
<!ENTITY % local.wordasword.attrib "">
<!ENTITY % wordasword.role.attrib "%role.attrib;">
<!ELEMENT WordAsWord - - ((%word.char.mix;)+)>
<!ATTLIST WordAsWord
		%common.attrib;
		%wordasword.role.attrib;
		%local.wordasword.attrib;
>
<!--end of wordasword.module-->]]>

<!-- Links and cross-references ........................................... -->

<!ENTITY % link.module "INCLUDE">
<![ %link.module; [
<!--FUTURE USE (V4.0):
......................
All link elements will be excluded from themselves and each other.
......................
-->
<!ENTITY % local.link.attrib "">
<!ENTITY % link.role.attrib "%role.attrib;">
<!ELEMENT Link - - ((%para.char.mix;)+)>
<!ATTLIST Link
		--
		Endterm: ID of element containing text that is to be
		fetched from elsewhere in the document to appear as
		the content of this element
		--
		Endterm		IDREF		#IMPLIED
		%linkendreq.attrib; --to linked-to object--
		--
		Type: Freely assignable parameter
		--
		Type		CDATA		#IMPLIED
		%common.attrib;
		%link.role.attrib;
		%local.link.attrib;
>
<!--end of link.module-->]]>

<!ENTITY % olink.module "INCLUDE">
<![ %olink.module; [
<!ENTITY % local.olink.attrib "">
<!ENTITY % olink.role.attrib "%role.attrib;">
<!ELEMENT OLink - - ((%para.char.mix;)+)>
<!ATTLIST OLink
		--
		TargetDocEnt: Name of an entity to be the target of the link
		--
		TargetDocEnt	ENTITY 		#IMPLIED
		--
		LinkMode: ID of a ModeSpec containing instructions for
		operating on the entity named by TargetDocEnt
		--
		LinkMode	IDREF		#IMPLIED
		--
		LocalInfo: Information that may be passed to ModeSpec
		--
		LocalInfo 	CDATA		#IMPLIED
		--
		Type: Freely assignable parameter
		--
		Type		CDATA		#IMPLIED
		%common.attrib;
		%olink.role.attrib;
		%local.olink.attrib;
>
<!--end of olink.module-->]]>

<!ENTITY % ulink.module "INCLUDE">
<![ %ulink.module; [
<!ENTITY % local.ulink.attrib "">
<!ENTITY % ulink.role.attrib "%role.attrib;">
<!ELEMENT ULink - - ((%para.char.mix;)+)>
<!ATTLIST ULink
		--
		URL: uniform resource locator; the target of the ULink
		--
		URL		CDATA		#REQUIRED
		--
		Type: Freely assignable parameter
		--
		Type		CDATA		#IMPLIED
		%common.attrib;
		%ulink.role.attrib;
		%local.ulink.attrib;
>
<!--end of ulink.module-->]]>

<!ENTITY % footnoteref.module "INCLUDE">
<![ %footnoteref.module; [
<!ENTITY % local.footnoteref.attrib "">
<!ENTITY % footnoteref.role.attrib "%role.attrib;">
<!ELEMENT FootnoteRef - O EMPTY>
<!ATTLIST FootnoteRef
		%linkendreq.attrib; --to footnote content supplied elsewhere--
		%label.attrib;
		%common.attrib;
		%footnoteref.role.attrib;
		%local.footnoteref.attrib;
>
<!--end of footnoteref.module-->]]>

<!ENTITY % xref.module "INCLUDE">
<![ %xref.module; [
<!ENTITY % local.xref.attrib "">
<!ENTITY % xref.role.attrib "%role.attrib;">
<!ELEMENT XRef - O EMPTY>
<!ATTLIST XRef
		--
		Endterm: ID of element containing text that is to be
		fetched from elsewhere in the document to appear as
		the content of this element
		--
		Endterm		IDREF		#IMPLIED
		%linkendreq.attrib; --to linked-to object--
		%common.attrib;
		%xref.role.attrib;
		%local.xref.attrib;
>
<!--end of xref.module-->]]>

<!-- Ubiquitous elements .................................................. -->

<!ENTITY % anchor.module "INCLUDE">
<![ %anchor.module; [
<!ENTITY % local.anchor.attrib "">
<!ENTITY % anchor.role.attrib "%role.attrib;">
<!ELEMENT Anchor - O EMPTY>
<!ATTLIST Anchor
		%idreq.attrib; -- required --
		%pagenum.attrib; --replaces Lang --
		%remap.attrib;
		%xreflabel.attrib;
		%revisionflag.attrib;
		%effectivity.attrib;
		%anchor.role.attrib;
		%local.anchor.attrib;
>
<!--end of anchor.module-->]]>

<!ENTITY % beginpage.module "INCLUDE">
<![ %beginpage.module; [
<!ENTITY % local.beginpage.attrib "">
<!ENTITY % beginpage.role.attrib "%role.attrib;">
<!ELEMENT BeginPage - O EMPTY>
<!ATTLIST BeginPage
		--
		PageNum: Number of page that begins at this point
		--
		%pagenum.attrib;
		%common.attrib;
		%beginpage.role.attrib;
		%local.beginpage.attrib;
>
<!--end of beginpage.module-->]]>

<!-- IndexTerms appear in the text flow for generating or linking an
     index. -->

<!ENTITY % indexterm.content.module "INCLUDE">
<![ %indexterm.content.module; [
<!ENTITY % indexterm.module "INCLUDE">
<![ %indexterm.module; [
<!ENTITY % local.indexterm.attrib "">
<!ENTITY % indexterm.role.attrib "%role.attrib;">
<!ELEMENT IndexTerm - O (Primary, ((Secondary, ((Tertiary, (See|SeeAlso+)?)
		| See | SeeAlso+)?) | See | SeeAlso+)?) -(%ubiq.mix;)>
<!ATTLIST IndexTerm
		%pagenum.attrib;
		--
		Scope: Indicates which generated indices the IndexTerm
		should appear in: Global (whole document set), Local (this
		document only), or All (both)
		--
		Scope		(All
				|Global
				|Local)		#IMPLIED
		--
		Significance: Whether this IndexTerm is the most pertinent
		of its series (Preferred) or not (Normal, the default)
		--
		Significance	(Preferred
				|Normal)	Normal
		--
		Class: Indicates type of IndexTerm; default is Singular, 
		or EndOfRange if StartRef is supplied; StartOfRange value 
		must be supplied explicitly on starts of ranges
		--
		Class		(Singular
				|StartOfRange
				|EndOfRange)	#IMPLIED
		--
		StartRef: ID of the IndexTerm that starts the indexing 
		range ended by this IndexTerm
		--
		StartRef		IDREF		#CONREF
		--
		Zone: IDs of the elements to which the IndexTerm applies,
		and indicates that the IndexTerm applies to those entire
		elements rather than the point at which the IndexTerm
		occurs
		--
		Zone			IDREFS		#IMPLIED
		%common.attrib;
		%indexterm.role.attrib;
		%local.indexterm.attrib;
>
<!--end of indexterm.module-->]]>

<!ENTITY % primsecter.module "INCLUDE">
<![ %primsecter.module; [
<!ENTITY % local.primsecter.attrib "">
<!ENTITY % primsecter.role.attrib "%role.attrib;">
<!ELEMENT (Primary | Secondary | Tertiary) - O ((%ndxterm.char.mix;)+)>
<!ATTLIST (Primary | Secondary | Tertiary)
		--
		SortAs: Alternate sort string for index sorting, e.g.,
		"fourteen" for an element containing "14"
		--
		SortAs		CDATA		#IMPLIED
		%common.attrib;
		%primsecter.role.attrib;
		%local.primsecter.attrib;
>
<!--end of primsecter.module-->]]>

<!ENTITY % seeseealso.module "INCLUDE">
<![ %seeseealso.module; [
<!ENTITY % local.seeseealso.attrib "">
<!ENTITY % seeseealso.role.attrib "%role.attrib;">
<!ELEMENT (See | SeeAlso) - O ((%ndxterm.char.mix;)+)>
<!ATTLIST (See | SeeAlso)
		%common.attrib;
		%seeseealso.role.attrib;
		%local.seeseealso.attrib;
>
<!--end of seeseealso.module-->]]>
<!--end of indexterm.content.module-->]]>

<!-- End of DocBook information pool module V3.0 .......................... -->
<!-- ...................................................................... -->
