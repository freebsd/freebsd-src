<!-- ...................................................................... -->
<!-- DocBook document hierarchy module V3.0 ............................... -->
<!-- File dbhier.mod ...................................................... -->

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

<!-- This module contains the definitions for the overall document
     hierarchies of DocBook documents.  It covers computer documentation
     manuals and manual fragments, as well as reference entries (such as
     man pages) and technical journals or anthologies containing
     articles.

     This module depends on the DocBook information pool module.  All
     elements and entities referenced but not defined here are assumed
     to be defined in the information pool module.

     In DTD driver files referring to this module, please use an entity
     declaration that uses the public identifier shown below:

     <!ENTITY % dbhier PUBLIC
     "-//Davenport//ELEMENTS DocBook Document Hierarchy V3.0//EN">
     %dbhier;

     See the documentation for detailed information on the parameter
     entity and module scheme used in DocBook, customizing DocBook and
     planning for interchange, and changes made since the last release
     of DocBook.
-->

<!-- ...................................................................... -->
<!-- Entities for module inclusions ....................................... -->

<!ENTITY % dbhier.redecl.module		"IGNORE">
<!ENTITY % dbhier.redecl2.module	"IGNORE">

<!-- ...................................................................... -->
<!-- Entities for element classes ......................................... -->

<!ENTITY % local.appendix.class "">
<!ENTITY % appendix.class	"Appendix %local.appendix.class;">

<!ENTITY % local.article.class "">
<!ENTITY % article.class	"Article %local.article.class">

<!ENTITY % local.book.class "">
<!ENTITY % book.class		"Book %local.book.class;">

<!ENTITY % local.chapter.class "">
<!ENTITY % chapter.class	"Chapter %local.chapter.class;">

<!ENTITY % local.index.class "">
<!ENTITY % index.class		"Index|SetIndex %local.index.class;">

<!-- SetInfo and BookInfo are not included in otherinfo.class because
they have different attribute lists. -->
<!ENTITY % local.otherinfo.class "">
<!--FUTURE USE (V4.0):
......................
The DocInfo element will be split out into ChapterInfo, AppendixInfo,
etc.
......................
-->
<!ENTITY % otherinfo.class	"DocInfo|Sect1Info|Sect2Info|Sect3Info
				|Sect4Info|Sect5Info|RefSect1Info
				|RefSect2Info|RefSect3Info|RefSynopsisDivInfo
				%local.otherinfo.class;">

<!ENTITY % local.refentry.class "">
<!ENTITY % refentry.class	"RefEntry %local.refentry.class;">

<!ENTITY % local.nav.class "">
<!ENTITY % nav.class		"ToC|LoT|Index|Glossary|Bibliography 
				%local.nav.class;">

<!-- Redeclaration placeholder ............................................ -->

<!-- For redeclaring entities that are declared after this point while
     retaining their references to the entities that are declared before
     this point -->

<![ %dbhier.redecl.module; [
%rdbhier;
<!--end of dbhier.redecl.module-->]]>

<!-- ...................................................................... -->
<!-- Entities for element mixtures ........................................ -->

<!ENTITY % local.divcomponent.mix "">
<!ENTITY % divcomponent.mix
		"%list.class;		|%admon.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|%formal.class;		|%compound.class;
		|%genobj.class;		|%descobj.class;
		%local.divcomponent.mix;">

<!ENTITY % local.refcomponent.mix "">
<!ENTITY % refcomponent.mix
		"%list.class;		|%admon.class;
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|%formal.class;		|%compound.class;
		|%genobj.class;		|%descobj.class;
		%local.refcomponent.mix;">

<!ENTITY % local.indexdivcomponent.mix "">
<!ENTITY % indexdivcomponent.mix
		"ItemizedList|OrderedList|VariableList|SimpleList
		|%linespecific.class;	|%synop.class;
		|%para.class;		|%informal.class;
		|Anchor|Comment
		|%link.char.class;
		%local.indexdivcomponent.mix;">

<!ENTITY % local.refname.char.mix "">
<!ENTITY % refname.char.mix
		"#PCDATA
		|%tech.char.class;
		%local.refname.char.mix;">

<!ENTITY % local.partcontent.mix "">
<!ENTITY % partcontent.mix
		"%appendix.class;|%chapter.class;|%nav.class;|%article.class;
		|Preface|%refentry.class;|Reference %local.partcontent.mix;">

<!ENTITY % local.refinline.char.mix "">
<!ENTITY % refinline.char.mix
		"#PCDATA
		|%xref.char.class;	|%gen.char.class;
		|%link.char.class;	|%tech.char.class;
		|%base.char.class;	|%docinfo.char.class;
		|%other.char.class;
		%local.refinline.char.mix;">

<!ENTITY % local.refclass.char.mix "">
<!ENTITY % refclass.char.mix
		"#PCDATA
		|Application
		%local.refclass.char.mix;">

<!ENTITY % local.setinfo.char.mix "">
<!ENTITY % setinfo.char.mix
		"#PCDATA
		|%docinfo.char.class;|Title|Copyright|CorpName
		|Date|Editor|Edition|InvPartNumber|ISBN
		|LegalNotice|OrgName|PrintHistory|Publisher
		|PubsNumber|ReleaseInfo|Subtitle|VolumeNum
		%local.setinfo.char.mix;">

<!-- Redeclaration placeholder 2 .......................................... -->

<!-- For redeclaring entities that are declared after this point while
     retaining their references to the entities that are declared before
     this point -->

<![ %dbhier.redecl2.module; [
%rdbhier2;
<!--end of dbhier.redecl2.module-->]]>

<!-- ...................................................................... -->
<!-- Entities for content models .......................................... -->

<!ENTITY % div.title.content
	"Title, TitleAbbrev?">

<!--FUTURE USE (V4.0):
......................
The DocInfo element will be split out into ChapterInfo, AppendixInfo,
etc.
......................
-->
<!ENTITY % bookcomponent.title.content
	"DocInfo?, Title, TitleAbbrev?">

<!ENTITY % sect.title.content
	"Title, TitleAbbrev?">

<!ENTITY % refsect.title.content
	"Title, TitleAbbrev?">

<!ENTITY % bookcomponent.content
	"((%divcomponent.mix;)+, 
	(Sect1*|(%refentry.class;)*|SimpleSect*))
	| (Sect1+|(%refentry.class;)+|SimpleSect+)">

<!-- ...................................................................... -->
<!-- Set and SetInfo ...................................................... -->

<!ENTITY % set.content.module "INCLUDE">
<![ %set.content.module; [
<!ENTITY % set.module "INCLUDE">
<![ %set.module; [
<!ENTITY % local.set.attrib "">
<!ENTITY % set.role.attrib "%role.attrib;">
<!ELEMENT Set - O ((%div.title.content;)?, SetInfo?, ToC?, (%book.class;),
		(%book.class;)+, SetIndex?) +(%ubiq.mix;)>
<!ATTLIST Set
		--
		FPI: SGML formal public identifier
		--
		FPI		CDATA		#IMPLIED
		%status.attrib;
		%common.attrib;
		%set.role.attrib;
		%local.set.attrib;
>
<!--end of set.module-->]]>

<!ENTITY % setinfo.module "INCLUDE">
<![ %setinfo.module; [
<!ENTITY % local.setinfo.attrib "">
<!ENTITY % setinfo.role.attrib "%role.attrib;">
<!ELEMENT SetInfo - - ((Graphic | LegalNotice | ModeSpec | SubjectSet 
	| KeywordSet | ITermSet | %bibliocomponent.mix;)+) -(BeginPage)>
<!ATTLIST SetInfo
		--
		Contents: IDs of the ToC, Books, and SetIndex that comprise 
		the set, in the order of their appearance
		--
		Contents	IDREFS		#IMPLIED
		%common.attrib;
		%setinfo.role.attrib;
		%local.setinfo.attrib;
>
<!--end of setinfo.module-->]]>
<!--end of set.content.module-->]]>

<!-- ...................................................................... -->
<!-- Book and BookInfo .................................................... -->

<!ENTITY % book.content.module "INCLUDE">
<![ %book.content.module; [
<!ENTITY % book.module "INCLUDE">
<![ %book.module; [
<!--FUTURE USE (V4.0):
......................
The %article.class; entity *may* be removed from the Book content model.
(Article may be made part of a new top-level document hierarchy.)
......................
-->

<!ENTITY % local.book.attrib "">
<!ENTITY % book.role.attrib "%role.attrib;">
<!ELEMENT Book - O ((%div.title.content;)?, BookInfo?, Dedication?, ToC?, LoT*, 
		(Glossary|Bibliography|Preface)*,
		(((%chapter.class;)+, Reference*) | Part+ 
		| Reference+ | (%article.class;)+), 
		(%appendix.class;)*, (Glossary|Bibliography)*, 
		(%index.class;)*, LoT*, ToC?)
		+(%ubiq.mix;)>
<!ATTLIST Book	
		--
		FPI: SGML formal public identifier
		--
		FPI		CDATA		#IMPLIED
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%book.role.attrib;
		%local.book.attrib;
>
<!--end of book.module-->]]>

<!ENTITY % bookinfo.module "INCLUDE">
<![ %bookinfo.module; [
<!--FUTURE USE (V4.0):
......................
BookBiblio will be discarded.
......................
-->
<!ENTITY % local.bookinfo.attrib "">
<!ENTITY % bookinfo.role.attrib "%role.attrib;">
<!ELEMENT BookInfo - - ((Graphic | LegalNotice | ModeSpec | SubjectSet 
	| KeywordSet | ITermSet | %bibliocomponent.mix; | BookBiblio)+)
	-(BeginPage)>
<!ATTLIST BookInfo
		--
		Contents: IDs of the ToC, LoTs, Prefaces, Parts, Chapters,
		Appendixes, References, GLossary, Bibliography, and indexes
		comprising the Book, in the order of their appearance
		--
		Contents	IDREFS		#IMPLIED
		%common.attrib;
		%bookinfo.role.attrib;
		%local.bookinfo.attrib;
>
<!--end of bookinfo.module-->]]>
<!--end of book.content.module-->]]>

<!-- ...................................................................... -->
<!-- Dedication, ToC, and LoT ............................................. -->

<!ENTITY % dedication.module "INCLUDE">
<![ %dedication.module; [
<!ENTITY % local.dedication.attrib "">
<!ENTITY % dedication.role.attrib "%role.attrib;">
<!ELEMENT Dedication - O ((%sect.title.content;)?, (%legalnotice.mix;)+)>
<!ATTLIST Dedication
		%status.attrib;
		%common.attrib;
		%dedication.role.attrib;
		%local.dedication.attrib;
>
<!--end of dedication.module-->]]>

<!ENTITY % toc.content.module "INCLUDE">
<![ %toc.content.module; [
<!ENTITY % toc.module "INCLUDE">
<![ %toc.module; [
<!ENTITY % local.toc.attrib "">
<!ENTITY % toc.role.attrib "%role.attrib;">
<!ELEMENT ToC - O ((%bookcomponent.title.content;)?, ToCfront*,
		(ToCpart | ToCchap)*, ToCback*)>
<!ATTLIST ToC
		%pagenum.attrib;
		%common.attrib;
		%toc.role.attrib;
		%local.toc.attrib;
>
<!--end of toc.module-->]]>

<!ENTITY % tocfront.module "INCLUDE">
<![ %tocfront.module; [
<!ENTITY % local.tocfront.attrib "">
<!ENTITY % tocfront.role.attrib "%role.attrib;">
<!ELEMENT ToCfront - O ((%para.char.mix;)+)>
<!ATTLIST ToCfront
		%label.attrib;
		%linkend.attrib; --to element that this entry represents--
		%pagenum.attrib;
		%common.attrib;
		%tocfront.role.attrib;
		%local.tocfront.attrib;
>
<!--end of tocfront.module-->]]>

<!ENTITY % tocentry.module "INCLUDE">
<![ %tocentry.module; [
<!ENTITY % local.tocentry.attrib "">
<!ENTITY % tocentry.role.attrib "%role.attrib;">
<!ELEMENT ToCentry - - ((%para.char.mix;)+)>
<!ATTLIST ToCentry
		%linkend.attrib; --to element that this entry represents--
		%pagenum.attrib;
		%common.attrib;
		%tocentry.role.attrib;
		%local.tocentry.attrib;
>
<!--end of tocentry.module-->]]>

<!ENTITY % tocpart.module "INCLUDE">
<![ %tocpart.module; [
<!ENTITY % local.tocpart.attrib "">
<!ENTITY % tocpart.role.attrib "%role.attrib;">
<!ELEMENT ToCpart - O (ToCentry+, ToCchap*)>
<!ATTLIST ToCpart
		%common.attrib;
		%tocpart.role.attrib;
		%local.tocpart.attrib;
>
<!--end of tocpart.module-->]]>

<!ENTITY % tocchap.module "INCLUDE">
<![ %tocchap.module; [
<!ENTITY % local.tocchap.attrib "">
<!ENTITY % tocchap.role.attrib "%role.attrib;">
<!ELEMENT ToCchap - O (ToCentry+, ToClevel1*)>
<!ATTLIST ToCchap
		%label.attrib;
		%common.attrib;
		%tocchap.role.attrib;
		%local.tocchap.attrib;
>
<!--end of tocchap.module-->]]>

<!ENTITY % toclevel1.module "INCLUDE">
<![ %toclevel1.module; [
<!ENTITY % local.toclevel1.attrib "">
<!ENTITY % toclevel1.role.attrib "%role.attrib;">
<!ELEMENT ToClevel1 - O (ToCentry+, ToClevel2*)>
<!ATTLIST ToClevel1
		%common.attrib;
		%toclevel1.role.attrib;
		%local.toclevel1.attrib;
>
<!--end of toclevel1.module-->]]>

<!ENTITY % toclevel2.module "INCLUDE">
<![ %toclevel2.module; [
<!ENTITY % local.toclevel2.attrib "">
<!ENTITY % toclevel2.role.attrib "%role.attrib;">
<!ELEMENT ToClevel2 - O (ToCentry+, ToClevel3*)>
<!ATTLIST ToClevel2
		%common.attrib;
		%toclevel2.role.attrib;
		%local.toclevel2.attrib;
>
<!--end of toclevel2.module-->]]>

<!ENTITY % toclevel3.module "INCLUDE">
<![ %toclevel3.module; [
<!ENTITY % local.toclevel3.attrib "">
<!ENTITY % toclevel3.role.attrib "%role.attrib;">
<!ELEMENT ToClevel3 - O (ToCentry+, ToClevel4*)>
<!ATTLIST ToClevel3
		%common.attrib;
		%toclevel3.role.attrib;
		%local.toclevel3.attrib;
>
<!--end of toclevel3.module-->]]>

<!ENTITY % toclevel4.module "INCLUDE">
<![ %toclevel4.module; [
<!ENTITY % local.toclevel4.attrib "">
<!ENTITY % toclevel4.role.attrib "%role.attrib;">
<!ELEMENT ToClevel4 - O (ToCentry+, ToClevel5*)>
<!ATTLIST ToClevel4
		%common.attrib;
		%toclevel4.role.attrib;
		%local.toclevel4.attrib;
>
<!--end of toclevel4.module-->]]>

<!ENTITY % toclevel5.module "INCLUDE">
<![ %toclevel5.module; [
<!ENTITY % local.toclevel5.attrib "">
<!ENTITY % toclevel5.role.attrib "%role.attrib;">
<!ELEMENT ToClevel5 - O (ToCentry+)>
<!ATTLIST ToClevel5
		%common.attrib;
		%toclevel5.role.attrib;
		%local.toclevel5.attrib;
>
<!--end of toclevel5.module-->]]>

<!ENTITY % tocback.module "INCLUDE">
<![ %tocback.module; [
<!ENTITY % local.tocback.attrib "">
<!ENTITY % tocback.role.attrib "%role.attrib;">
<!ELEMENT ToCback - O ((%para.char.mix;)+)>
<!ATTLIST ToCback
		%label.attrib;
		%linkend.attrib; --to element that this entry represents--
		%pagenum.attrib;
		%common.attrib;
		%tocback.role.attrib;
		%local.tocback.attrib;
>
<!--end of tocback.module-->]]>
<!--end of toc.content.module-->]]>

<!ENTITY % lot.content.module "INCLUDE">
<![ %lot.content.module; [
<!ENTITY % lot.module "INCLUDE">
<![ %lot.module; [
<!ENTITY % local.lot.attrib "">
<!ENTITY % lot.role.attrib "%role.attrib;">
<!ELEMENT LoT - O ((%bookcomponent.title.content;)?, LoTentry*)>
<!ATTLIST LoT
		%label.attrib;
		%common.attrib;
		%lot.role.attrib;
		%local.lot.attrib;
>
<!--end of lot.module-->]]>

<!ENTITY % lotentry.module "INCLUDE">
<![ %lotentry.module; [
<!ENTITY % local.lotentry.attrib "">
<!ENTITY % lotentry.role.attrib "%role.attrib;">
<!ELEMENT LoTentry - - ((%para.char.mix;)+ )>
<!ATTLIST LoTentry
		--
		SrcCredit: Information about the source of the entry, 
		as for a list of illustrations
		--
		SrcCredit	CDATA		#IMPLIED
		%pagenum.attrib;
		%common.attrib;
		%lotentry.role.attrib;
		%local.lotentry.attrib;
>
<!--end of lotentry.module-->]]>
<!--end of lot.content.module-->]]>

<!-- ...................................................................... -->
<!-- Appendix, Chapter, Part, Preface, Reference, PartIntro ............... -->

<!ENTITY % appendix.module "INCLUDE">
<![ %appendix.module; [
<!ENTITY % local.appendix.attrib "">
<!ENTITY % appendix.role.attrib "%role.attrib;">
<!ELEMENT Appendix - O ((%bookcomponent.title.content;), ToCchap?,
		(%bookcomponent.content;)) +(%ubiq.mix;)>
<!ATTLIST Appendix
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%appendix.role.attrib;
		%local.appendix.attrib;
>
<!--end of appendix.module-->]]>

<!ENTITY % chapter.module "INCLUDE">
<![ %chapter.module; [
<!ENTITY % local.chapter.attrib "">
<!ENTITY % chapter.role.attrib "%role.attrib;">
<!ELEMENT Chapter - O ((%bookcomponent.title.content;), ToCchap?,
		(%bookcomponent.content;), (Index | Glossary | Bibliography)*)
		+(%ubiq.mix;)>
<!ATTLIST Chapter
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%chapter.role.attrib;
		%local.chapter.attrib;
>
<!--end of chapter.module-->]]>

<!ENTITY % part.module "INCLUDE">
<![ %part.module; [

<!-- Note that Part was to have its content model reduced in V4.0.  This
change will not be made after all. -->

<!ENTITY % local.part.attrib "">
<!ENTITY % part.role.attrib "%role.attrib;">
<!ELEMENT Part - - ((%bookcomponent.title.content;), PartIntro?,
		(%partcontent.mix;)+) +(%ubiq.mix;)>
<!ATTLIST Part
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%part.role.attrib;
		%local.part.attrib;
>
<!--ELEMENT PartIntro (defined below)-->
<!--end of part.module-->]]>

<!ENTITY % preface.module "INCLUDE">
<![ %preface.module; [
<!ENTITY % local.preface.attrib "">
<!ENTITY % preface.role.attrib "%role.attrib;">
<!ELEMENT Preface - O ((%bookcomponent.title.content;), 
		(%bookcomponent.content;)) +(%ubiq.mix;)>
<!ATTLIST Preface
		%status.attrib;
		%common.attrib;
		%preface.role.attrib;
		%local.preface.attrib;
>
<!--end of preface.module-->]]>

<!ENTITY % reference.module "INCLUDE">
<![ %reference.module; [
<!ENTITY % local.reference.attrib "">
<!ENTITY % reference.role.attrib "%role.attrib;">
<!ELEMENT Reference - O ((%bookcomponent.title.content;), PartIntro?,
		(%refentry.class;)+) +(%ubiq.mix;)>
<!ATTLIST Reference
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%reference.role.attrib;
		%local.reference.attrib;
>
<!--ELEMENT PartIntro (defined below)-->
<!--end of reference.module-->]]>

<!ENTITY % partintro.module "INCLUDE">
<![ %partintro.module; [
<!ENTITY % local.partintro.attrib "">
<!ENTITY % partintro.role.attrib "%role.attrib;">
<!ELEMENT PartIntro - O ((%div.title.content;)?, (%bookcomponent.content;))
		+(%ubiq.mix;)>
<!ATTLIST PartIntro	
		%label.attrib;
		%common.attrib;
		%local.partintro.attrib;
		%partintro.role.attrib;
>
<!--end of partintro.module-->]]>

<!-- ...................................................................... -->
<!-- Other Info elements .................................................. -->

<!ENTITY % otherinfo.module "INCLUDE">
<![ %otherinfo.module; [
<!ENTITY % local.otherinfo.attrib "">
<!ENTITY % otherinfo.role.attrib "%role.attrib;">
<!ELEMENT (%otherinfo.class;) - - ((Graphic | LegalNotice | ModeSpec 
	| SubjectSet | KeywordSet | ITermSet | %bibliocomponent.mix;)+)
	-(BeginPage)>
<!ATTLIST (%otherinfo.class;)
		%common.attrib;
		%otherinfo.role.attrib;
		%local.otherinfo.attrib;
>
<!--end of otherinfo.module-->]]>

<!-- ...................................................................... -->
<!-- Sect1, Sect2, Sect3, Sect4, Sect5 .................................... -->

<!ENTITY % sect1.module "INCLUDE">
<![ %sect1.module; [
<!ENTITY % local.sect1.attrib "">
<!ENTITY % sect1.role.attrib "%role.attrib;">
<!ELEMENT Sect1 - O (Sect1Info?, (%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect2* | SimpleSect*))
		| (%refentry.class;)+ | Sect2+ | SimpleSect+), (%nav.class;)*)
		+(%ubiq.mix;)>
<!ATTLIST Sect1
		--
		Renderas: Indicates the format in which the heading should
		appear
		--
		Renderas	(Sect2
				|Sect3
				|Sect4
				|Sect5)		#IMPLIED
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%sect1.role.attrib;
		%local.sect1.attrib;
>
<!--end of sect1.module-->]]>

<!ENTITY % sect2.module "INCLUDE">
<![ %sect2.module; [
<!ENTITY % local.sect2.attrib "">
<!ENTITY % sect2.role.attrib "%role.attrib;">
<!ELEMENT Sect2 - O (Sect2Info?, (%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect3* | SimpleSect*))
		| (%refentry.class;)+ | Sect3+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect2
		--
		Renderas: Indicates the format in which the heading should
		appear
		--
		Renderas	(Sect1
				|Sect3
				|Sect4
				|Sect5)		#IMPLIED
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%sect2.role.attrib;
		%local.sect2.attrib;
>
<!--end of sect2.module-->]]>

<!ENTITY % sect3.module "INCLUDE">
<![ %sect3.module; [
<!ENTITY % local.sect3.attrib "">
<!ENTITY % sect3.role.attrib "%role.attrib;">
<!ELEMENT Sect3 - O (Sect3Info?, (%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect4* | SimpleSect*))
		| (%refentry.class;)+ | Sect4+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect3
		--
		Renderas: Indicates the format in which the heading should
		appear
		--
		Renderas	(Sect1
				|Sect2
				|Sect4
				|Sect5)		#IMPLIED
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%sect3.role.attrib;
		%local.sect3.attrib;
>
<!--end of sect3.module-->]]>

<!ENTITY % sect4.module "INCLUDE">
<![ %sect4.module; [
<!ENTITY % local.sect4.attrib "">
<!ENTITY % sect4.role.attrib "%role.attrib;">
<!ELEMENT Sect4 - O (Sect4Info?, (%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect5* | SimpleSect*))
		| (%refentry.class;)+ | Sect5+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect4
		--
		Renderas: Indicates the format in which the heading should
		appear
		--
		Renderas	(Sect1
				|Sect2
				|Sect3
				|Sect5)		#IMPLIED
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%sect4.role.attrib;
		%local.sect4.attrib;
>
<!--end of sect4.module-->]]>

<!ENTITY % sect5.module "INCLUDE">
<![ %sect5.module; [
<!ENTITY % local.sect5.attrib "">
<!ENTITY % sect5.role.attrib "%role.attrib;">
<!ELEMENT Sect5 - O (Sect5Info?, (%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, ((%refentry.class;)* | SimpleSect*))
		| (%refentry.class;)+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect5
		--
		Renderas: Indicates the format in which the heading should
		appear
		--
		Renderas	(Sect1
				|Sect2
				|Sect3
				|Sect4)		#IMPLIED
		%label.attrib;
		%status.attrib;
		%common.attrib;
		%sect5.role.attrib;
		%local.sect5.attrib;
>
<!--end of sect5.module-->]]>

<!ENTITY % simplesect.module "INCLUDE">
<![ %simplesect.module; [
<!ENTITY % local.simplesect.attrib "">
<!ENTITY % simplesect.role.attrib "%role.attrib;">
<!ELEMENT SimpleSect - O ((%sect.title.content;), (%divcomponent.mix;)+)
		+(%ubiq.mix;)>
<!ATTLIST SimpleSect
		%common.attrib;
		%simplesect.role.attrib;
		%local.simplesect.attrib;
>
<!--end of simplesect.module-->]]>

<!-- ...................................................................... -->
<!-- Bibliography ......................................................... -->

<!ENTITY % bibliography.content.module "INCLUDE">
<![ %bibliography.content.module; [
<!ENTITY % bibliography.module "INCLUDE">
<![ %bibliography.module; [
<!ENTITY % local.bibliography.attrib "">
<!ENTITY % bibliography.role.attrib "%role.attrib;">
<!ELEMENT Bibliography - O ((%bookcomponent.title.content;)?,
		(%component.mix;)*, 
		(BiblioDiv+ | (BiblioEntry|BiblioMixed)+))>
<!ATTLIST Bibliography
		%status.attrib;
		%common.attrib;
		%bibliography.role.attrib;
		%local.bibliography.attrib;
>
<!--end of bibliography.module-->]]>

<!ENTITY % bibliodiv.module "INCLUDE">
<![ %bibliodiv.module; [
<!ENTITY % local.bibliodiv.attrib "">
<!ENTITY % bibliodiv.role.attrib "%role.attrib;">
<!ELEMENT BiblioDiv - O ((%sect.title.content;)?, (%component.mix;)*,
		(BiblioEntry|BiblioMixed)+)>
<!ATTLIST BiblioDiv
		%status.attrib;
		%common.attrib;
		%bibliodiv.role.attrib;
		%local.bibliodiv.attrib;
>
<!--end of bibliodiv.module-->]]>
<!--end of bibliography.content.module-->]]>

<!-- ...................................................................... -->
<!-- Glossary ............................................................. -->

<!ENTITY % glossary.content.module "INCLUDE">
<![ %glossary.content.module; [
<!ENTITY % glossary.module "INCLUDE">
<![ %glossary.module; [
<!ENTITY % local.glossary.attrib "">
<!ENTITY % glossary.role.attrib "%role.attrib;">
<!ELEMENT Glossary - O ((%bookcomponent.title.content;)?, (%component.mix;)*,
		(GlossDiv+ | GlossEntry+), Bibliography?)>
<!ATTLIST Glossary
		%status.attrib;
		%common.attrib;
		%glossary.role.attrib;
		%local.glossary.attrib;
>
<!--end of glossary.module-->]]>

<!ENTITY % glossdiv.module "INCLUDE">
<![ %glossdiv.module; [
<!ENTITY % local.glossdiv.attrib "">
<!ENTITY % glossdiv.role.attrib "%role.attrib;">
<!ELEMENT GlossDiv - O ((%sect.title.content;), (%component.mix;)*,
		GlossEntry+)>
<!ATTLIST GlossDiv
		%status.attrib;
		%common.attrib;
		%glossdiv.role.attrib;
		%local.glossdiv.attrib;
>
<!--end of glossdiv.module-->]]>
<!--end of glossary.content.module-->]]>

<!-- ...................................................................... -->
<!-- Index and SetIndex ................................................... -->

<!ENTITY % index.content.module "INCLUDE">
<![ %index.content.module; [
<!ENTITY % indexes.module "INCLUDE">
<![ %indexes.module; [
<!ENTITY % local.indexes.attrib "">
<!ENTITY % indexes.role.attrib "%role.attrib;">
<!ELEMENT (%index.class;) - O ((%bookcomponent.title.content;)?,
		(%component.mix;)*, (IndexDiv* | IndexEntry*))
		-(%ndxterm.class;)>
<!ATTLIST (%index.class;)
		%common.attrib;
		%indexes.role.attrib;
		%local.indexes.attrib;
>
<!--end of indexes.module-->]]>

<!ENTITY % indexdiv.module "INCLUDE">
<![ %indexdiv.module; [

<!-- SegmentedList in this content is useful for marking up permuted
     indices. -->

<!ENTITY % local.indexdiv.attrib "">
<!ENTITY % indexdiv.role.attrib "%role.attrib;">
<!ELEMENT IndexDiv - O ((%sect.title.content;)?, ((%indexdivcomponent.mix;)*,
		(IndexEntry+ | SegmentedList)))>
<!ATTLIST IndexDiv
		%common.attrib;
		%indexdiv.role.attrib;
		%local.indexdiv.attrib;
>
<!--end of indexdiv.module-->]]>

<!ENTITY % indexentry.module "INCLUDE">
<![ %indexentry.module; [
<!-- Index entries appear in the index, not the text. -->

<!ENTITY % local.indexentry.attrib "">
<!ENTITY % indexentry.role.attrib "%role.attrib;">
<!ELEMENT IndexEntry - O (PrimaryIE, (SeeIE|SeeAlsoIE)*,
		(SecondaryIE, (SeeIE|SeeAlsoIE|TertiaryIE)*)*)>
<!ATTLIST IndexEntry
		%common.attrib;
		%indexentry.role.attrib;
		%local.indexentry.attrib;
>
<!--end of indexentry.module-->]]>

<!ENTITY % primsecterie.module "INCLUDE">
<![ %primsecterie.module; [
<!ENTITY % local.primsecterie.attrib "">
<!ENTITY % primsecterie.role.attrib "%role.attrib;">
<!ELEMENT (PrimaryIE | SecondaryIE | TertiaryIE) - O ((%ndxterm.char.mix;)+)>
<!ATTLIST (PrimaryIE | SecondaryIE | TertiaryIE)
		%linkends.attrib; --to IndexTerms that these entries represent--
		%common.attrib;
		%primsecterie.role.attrib;
		%local.primsecterie.attrib;
>
<!--end of primsecterie.module-->]]>
	
<!ENTITY % seeie.module "INCLUDE">
<![ %seeie.module; [
<!ENTITY % local.seeie.attrib "">
<!ENTITY % seeie.role.attrib "%role.attrib;">
<!ELEMENT SeeIE - O ((%ndxterm.char.mix;)+)>
<!ATTLIST SeeIE
		%linkend.attrib; --to IndexEntry to look up--
		%common.attrib;
		%seeie.role.attrib;
		%local.seeie.attrib;
>
<!--end of seeie.module-->]]>

<!ENTITY % seealsoie.module "INCLUDE">
<![ %seealsoie.module; [
<!ENTITY % local.seealsoie.attrib "">
<!ENTITY % seealsoie.role.attrib "%role.attrib;">
<!ELEMENT SeeAlsoIE - O ((%ndxterm.char.mix;)+)>
<!ATTLIST SeeAlsoIE
		%linkends.attrib; --to related IndexEntries--
		%common.attrib;
		%seealsoie.role.attrib;
		%local.seealsoie.attrib;
>
<!--end of seealsoie.module-->]]>
<!--end of index.content.module-->]]>

<!-- ...................................................................... -->
<!-- RefEntry ............................................................. -->

<!ENTITY % refentry.content.module "INCLUDE">
<![ %refentry.content.module; [
<!ENTITY % refentry.module "INCLUDE">
<![ %refentry.module; [
<!ENTITY % local.refentry.attrib "">
<!ENTITY % refentry.role.attrib "%role.attrib;">
<!--FUTURE USE (V4.0):
......................
The DocInfo element will be split out into ChapterInfo, AppendixInfo,
etc.
......................
-->
<!ELEMENT RefEntry - O (DocInfo?, RefMeta?, (Comment|%link.char.class;)*,
		RefNameDiv, RefSynopsisDiv?, RefSect1+) +(%ubiq.mix;)>
<!ATTLIST RefEntry
		%status.attrib;
		%common.attrib;
		%refentry.role.attrib;
		%local.refentry.attrib;
>
<!--end of refentry.module-->]]>

<!ENTITY % refmeta.module "INCLUDE">
<![ %refmeta.module; [
<!ENTITY % local.refmeta.attrib "">
<!ENTITY % refmeta.role.attrib "%role.attrib;">
<!ELEMENT RefMeta - - (RefEntryTitle, ManVolNum?, RefMiscInfo*)
		-(BeginPage)>
<!ATTLIST RefMeta
		%common.attrib;
		%refmeta.role.attrib;
		%local.refmeta.attrib;
>
<!--end of refmeta.module-->]]>

<!ENTITY % refmiscinfo.module "INCLUDE">
<![ %refmiscinfo.module; [
<!ENTITY % local.refmiscinfo.attrib "">
<!ENTITY % refmiscinfo.role.attrib "%role.attrib;">
<!ELEMENT RefMiscInfo - - ((%docinfo.char.mix;)+)>
<!ATTLIST RefMiscInfo
		--
		Class: Freely assignable parameter; no default
		--
		Class		CDATA		#IMPLIED
		%common.attrib;
		%refmiscinfo.role.attrib;
		%local.refmiscinfo.attrib;
>
<!--end of refmiscinfo.module-->]]>

<!ENTITY % refnamediv.module "INCLUDE">
<![ %refnamediv.module; [
<!ENTITY % local.refnamediv.attrib "">
<!ENTITY % refnamediv.role.attrib "%role.attrib;">
<!ELEMENT RefNameDiv - O (RefDescriptor?, RefName+, RefPurpose, RefClass*,
		(Comment|%link.char.class;)*)>
<!ATTLIST RefNameDiv
		%common.attrib;
		%refnamediv.role.attrib;
		%local.refnamediv.attrib;
>
<!--end of refnamediv.module-->]]>
	
<!ENTITY % refdescriptor.module "INCLUDE">
<![ %refdescriptor.module; [
<!ENTITY % local.refdescriptor.attrib "">
<!ENTITY % refdescriptor.role.attrib "%role.attrib;">
<!ELEMENT RefDescriptor - O ((%refname.char.mix;)+)>
<!ATTLIST RefDescriptor
		%common.attrib;
		%refdescriptor.role.attrib;
		%local.refdescriptor.attrib;
>
<!--end of refdescriptor.module-->]]>

<!ENTITY % refname.module "INCLUDE">
<![ %refname.module; [
<!ENTITY % local.refname.attrib "">
<!ENTITY % refname.role.attrib "%role.attrib;">
<!ELEMENT RefName - O ((%refname.char.mix;)+)>
<!ATTLIST RefName
		%common.attrib;
		%refname.role.attrib;
		%local.refname.attrib;
>
<!--end of refname.module-->]]>

<!ENTITY % refpurpose.module "INCLUDE">
<![ %refpurpose.module; [
<!ENTITY % local.refpurpose.attrib "">
<!ENTITY % refpurpose.role.attrib "%role.attrib;">
<!ELEMENT RefPurpose - O ((%refinline.char.mix;)+)>
<!ATTLIST RefPurpose
		%common.attrib;
		%refpurpose.role.attrib;
		%local.refpurpose.attrib;
>
<!--end of refpurpose.module-->]]>

<!ENTITY % refclass.module "INCLUDE">
<![ %refclass.module; [
<!ENTITY % local.refclass.attrib "">
<!ENTITY % refclass.role.attrib "%role.attrib;">
<!ELEMENT RefClass - O ((%refclass.char.mix;)+)>
<!ATTLIST RefClass
		%common.attrib;
		%refclass.role.attrib;
		%local.refclass.attrib;
>
<!--end of refclass.module-->]]>

<!ENTITY % refsynopsisdiv.module "INCLUDE">
<![ %refsynopsisdiv.module; [
<!ENTITY % local.refsynopsisdiv.attrib "">
<!ENTITY % refsynopsisdiv.role.attrib "%role.attrib;">
<!ELEMENT RefSynopsisDiv - O (RefSynopsisDivInfo?, (%refsect.title.content;)?,
		(((%refcomponent.mix;)+, RefSect2*) | (RefSect2+)))>
<!ATTLIST RefSynopsisDiv
		%common.attrib;
		%refsynopsisdiv.role.attrib;
		%local.refsynopsisdiv.attrib;
>
<!--end of refsynopsisdiv.module-->]]>

<!ENTITY % refsect1.module "INCLUDE">
<![ %refsect1.module; [
<!ENTITY % local.refsect1.attrib "">
<!ENTITY % refsect1.role.attrib "%role.attrib;">
<!ELEMENT RefSect1 - O (RefSect1Info?, (%refsect.title.content;),
		(((%refcomponent.mix;)+, RefSect2*) | RefSect2+))>
<!ATTLIST RefSect1
		%status.attrib;
		%common.attrib;
		%refsect1.role.attrib;
		%local.refsect1.attrib;
>
<!--end of refsect1.module-->]]>

<!ENTITY % refsect2.module "INCLUDE">
<![ %refsect2.module; [
<!ENTITY % local.refsect2.attrib "">
<!ENTITY % refsect2.role.attrib "%role.attrib;">
<!ELEMENT RefSect2 - O (RefSect2Info?, (%refsect.title.content;),
	(((%refcomponent.mix;)+, RefSect3*) | RefSect3+))>
<!ATTLIST RefSect2
		%status.attrib;
		%common.attrib;
		%refsect2.role.attrib;
		%local.refsect2.attrib;
>
<!--end of refsect2.module-->]]>

<!ENTITY % refsect3.module "INCLUDE">
<![ %refsect3.module; [
<!ENTITY % local.refsect3.attrib "">
<!ENTITY % refsect3.role.attrib "%role.attrib;">
<!ELEMENT RefSect3 - O (RefSect3Info?, (%refsect.title.content;), 
	(%refcomponent.mix;)+)>
<!ATTLIST RefSect3
		%status.attrib;
		%common.attrib;
		%refsect3.role.attrib;
		%local.refsect3.attrib;
>
<!--end of refsect3.module-->]]>
<!--end of refentry.content.module-->]]>

<!-- ...................................................................... -->
<!-- Article .............................................................. -->

<!ENTITY % article.module "INCLUDE">
<![ %article.module; [
<!-- An Article is a chapter-level, stand-alone document that is often,
     but need not be, collected into a Book. -->
<!--FUTURE USE (V4.0):
......................
The %nav.class; entity now allows ToC; ToCchap will be allowed instead.
RefEntry will be removed from the main content of Article.
......................
-->

<!--FUTURE USE (V4.0):
......................
The ArtHeader element will be renamed to ArticleInfo.
......................
-->

<!ENTITY % local.article.attrib "">
<!ENTITY % article.role.attrib "%role.attrib;">
<!ELEMENT Article - O (ArtHeader, ToCchap?, LoT*, (%bookcomponent.content;),
		((%nav.class;) | (%appendix.class;) | Ackno)*) +(%ubiq.mix;)>
<!ATTLIST Article
		--
		Class: Indicates the type of a particular article;
		all articles have the same structure and general purpose.
		No default.
		--
		Class		(JournalArticle
				|ProductSheet
				|WhitePaper
				|TechReport)	#IMPLIED
		--
		ParentBook: ID of the enclosing Book
		--
		ParentBook	IDREF		#IMPLIED
		%status.attrib;
		%common.attrib;
		%article.role.attrib;
		%local.article.attrib;
>
<!--end of article.module-->]]>

<!-- End of DocBook document hierarchy module V3.0 ........................ -->
<!-- ...................................................................... -->
