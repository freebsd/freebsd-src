<!-- ...................................................................... -->
<!-- DocBook document hierarchy module V2.4.1 ............................. -->
<!-- File dbhier.mod ...................................................... -->

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
     "-//Davenport//ELEMENTS DocBook Document Hierarchy V2.4.1//EN">
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

<!ENTITY % appendix.module		"INCLUDE">
<!ENTITY % article.module		"INCLUDE">
<!ENTITY % bibliography.content.module	"INCLUDE">
<!ENTITY % bibliography.module		"INCLUDE">
<!ENTITY % bibliodiv.module		"INCLUDE">
<!ENTITY % book.content.module		"INCLUDE">
<!ENTITY % book.module			"INCLUDE">
<!ENTITY % bookbiblio.module		"INCLUDE">
<!ENTITY % bookinfo.module		"INCLUDE">
<!ENTITY % chapter.module		"INCLUDE">
<!ENTITY % docinfo.module		"INCLUDE">
<!ENTITY % glossary.content.module	"INCLUDE">
<!ENTITY % glossary.module		"INCLUDE">
<!ENTITY % glossdiv.module		"INCLUDE">
<!--       index.module			use indexes.module-->
<!ENTITY % index.content.module		"INCLUDE">
<!ENTITY % indexdiv.module		"INCLUDE">
<!ENTITY % indexentry.module		"INCLUDE">
<!ENTITY % indexes.module		"INCLUDE">
<!ENTITY % lot.content.module		"INCLUDE">
<!ENTITY % lot.module			"INCLUDE">
<!ENTITY % lotentry.module		"INCLUDE">
<!ENTITY % part.module			"INCLUDE">
<!ENTITY % partintro.module		"INCLUDE">
<!ENTITY % preface.module		"INCLUDE">
<!--       primaryie.module		use primsecterie.module-->
<!ENTITY % primsecterie.module		"INCLUDE">
<!ENTITY % refclass.module		"INCLUDE">
<!ENTITY % refdescriptor.module		"INCLUDE">
<!ENTITY % refentry.content.module	"INCLUDE">
<!ENTITY % refentry.module		"INCLUDE">
<!ENTITY % reference.module		"INCLUDE">
<!ENTITY % refmeta.module		"INCLUDE">
<!ENTITY % refmiscinfo.module		"INCLUDE">
<!ENTITY % refname.module		"INCLUDE">
<!ENTITY % refnamediv.module		"INCLUDE">
<!ENTITY % refpurpose.module		"INCLUDE">
<!ENTITY % refsect1.module		"INCLUDE">
<!ENTITY % refsect2.module		"INCLUDE">
<!ENTITY % refsect3.module		"INCLUDE">
<!ENTITY % refsynopsisdiv.module	"INCLUDE">
<!--       secondaryie.module		use primsecterie.module-->
<!ENTITY % sect1.module			"INCLUDE">
<!ENTITY % sect2.module			"INCLUDE">
<!ENTITY % sect3.module			"INCLUDE">
<!ENTITY % sect4.module			"INCLUDE">
<!ENTITY % sect5.module			"INCLUDE">
<!ENTITY % seealsoie.module		"INCLUDE">
<!ENTITY % seeie.module			"INCLUDE">
<!ENTITY % seriesinfo.module		"INCLUDE">
<!ENTITY % set.content.module		"INCLUDE">
<!ENTITY % set.module			"INCLUDE">
<!--       setindex.module		use indexes.module-->
<!ENTITY % setinfo.module		"INCLUDE">
<!ENTITY % simplesect.module		"INCLUDE">
<!--       tertiaryie.module		use primsecterie.module-->
<!ENTITY % toc.content.module		"INCLUDE">
<!ENTITY % toc.module			"INCLUDE">
<!ENTITY % tocback.module		"INCLUDE">
<!ENTITY % tocchap.module		"INCLUDE">
<!ENTITY % tocentry.module		"INCLUDE">
<!ENTITY % tocfront.module		"INCLUDE">
<!ENTITY % toclevel1.module		"INCLUDE">
<!ENTITY % toclevel2.module		"INCLUDE">
<!ENTITY % toclevel3.module		"INCLUDE">
<!ENTITY % toclevel4.module		"INCLUDE">
<!ENTITY % toclevel5.module		"INCLUDE">
<!ENTITY % tocpart.module		"INCLUDE">

<!-- ...................................................................... -->
<!-- Entities for element classes ......................................... -->

<!ENTITY % local.appendix.class "">
<!ENTITY % appendix.class      "Appendix %local.appendix.class;">

<!ENTITY % local.article.class "">
<!ENTITY % article.class       "Article %local.article.class">

<!ENTITY % local.book.class "">
<!ENTITY % book.class          "Book %local.book.class;">

<!ENTITY % local.chapter.class "">
<!ENTITY % chapter.class       "Chapter %local.chapter.class;">

<!ENTITY % local.index.class "">
<!ENTITY % index.class         "Index|SetIndex %local.index.class;">

<!ENTITY % local.refentry.class "">
<!ENTITY % refentry.class      "RefEntry %local.refentry.class;">

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
		|%cptr.char.class;
		%local.refname.char.mix;">

<!ENTITY % local.partcontent.mix "">
<!ENTITY % partcontent.mix
		"%appendix.class;|%chapter.class;|%nav.class;|Preface
		|%refentry.class;|Reference %local.partcontent.mix;">

<!ENTITY % local.refinline.char.mix "">
<!ENTITY % refinline.char.mix
		"#PCDATA
		|%xref.char.class;	|%word.char.class;
		|%link.char.class;	|%cptr.char.class;
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

<!ENTITY % bookcomponent.title.content
           "DocInfo?, Title, TitleAbbrev?">

<!ENTITY % sect.title.content
           "Title, TitleAbbrev?">

<!ENTITY % refsect.title.content "Title, TitleAbbrev?">

<!ENTITY % bookcomponent.content
		"((%divcomponent.mix;)+, 
		(Sect1*|(%refentry.class;)*|SimpleSect*))
		| (Sect1+|(%refentry.class;)+|SimpleSect+)">

<!-- ...................................................................... -->
<!-- Set and SetInfo ...................................................... -->

<![ %set.content.module; [
<![ %set.module; [
<!ENTITY % local.set.attrib "">
<!ELEMENT Set - O ((%div.title.content;)?, SetInfo?, ToC?, (%book.class;),
		(%book.class;)+, SetIndex?) +(%ubiq.mix;)>
<!ATTLIST Set
		--Preferred formal public ID of set--
		FPI		CDATA		#IMPLIED
		%common.attrib;
		%local.set.attrib;
>
<!--end of set.module-->]]>

<![ %setinfo.module; [
<!ENTITY % local.setinfo.attrib "">
<!ELEMENT SetInfo - - ((%setinfo.char.mix;)+) -(%ubiq.mix;)>
<!ATTLIST SetInfo
		--Contents: points to the IDs of the book pieces in the
		order of their appearance--
		Contents	IDREFS		#IMPLIED
		%common.attrib;
		%local.setinfo.attrib;
>
<!--end of setinfo.module-->]]>
<!--end of set.content.module-->]]>

<!-- ...................................................................... -->
<!-- Book and BookInfo .................................................... -->

<![ %book.content.module; [
<![ %book.module; [
<!--FUTURE USE (V4.0):
......................
The %article.class; entity *may* be removed from the Book content model.
(Article may be made part of a new top-level document hierarchy.)
......................
-->

<!ENTITY % local.book.attrib "">
<!ELEMENT Book - O ((%div.title.content;)?, BookInfo?, ToC?, LoT*, 
		(Glossary|Bibliography|Preface)*,
		(((%chapter.class;)+, Reference*) | Part+ 
		| Reference+ | (%article.class;)+), 
		(%appendix.class;)*, (Glossary|Bibliography)*, 
		(%index.class;)*, LoT*, ToC?)
		+(%ubiq.mix;)>
<!ATTLIST Book	
		--FPI: Preferred formal public ID of book--
		FPI		CDATA		#IMPLIED
		%label.attrib;
		%common.attrib;
		%local.book.attrib;
>
<!--end of book.module-->]]>

<![ %bookinfo.module; [
<!ENTITY % local.bookinfo.attrib "">
<!ELEMENT BookInfo - - (Graphic*, BookBiblio, LegalNotice*, ModeSpec*)
		-(%ubiq.mix;)>
<!ATTLIST BookInfo
		--Contents: points to the IDs of the book pieces in the
		order of their appearance--
		Contents	IDREFS		#IMPLIED
		%common.attrib;
		%local.bookinfo.attrib;
>
<!--end of bookinfo.module-->]]>
<!--end of book.content.module-->]]>

<!-- ...................................................................... -->
<!-- ToC and LoT .......................................................... -->

<![ %toc.content.module [
<![ %toc.module [
<!ENTITY % local.toc.attrib "">
<!ELEMENT ToC - O ((%bookcomponent.title.content;)?, ToCfront*,
		(ToCpart | ToCchap)*, ToCback*)>
<!ATTLIST ToC
		%common.attrib;
		%local.toc.attrib;
>
<!--end of toc.module-->]]>

<![ %tocfront.module [
<!ENTITY % local.tocfront.attrib "">
<!ELEMENT ToCfront - O ((%para.char.mix;)+)>
<!ATTLIST ToCfront
		%label.attrib;
		%pagenum.attrib;
		%common.attrib;
		%local.tocfront.attrib;
>
<!--end of tocfront.module-->]]>

<![ %tocentry.module [
<!ENTITY % local.tocentry.attrib "">
<!ELEMENT ToCentry - - ((%para.char.mix;)+)>
<!ATTLIST ToCentry
		%linkend.attrib; --to element that this entry represents--
		%pagenum.attrib;
		%common.attrib;
		%local.tocentry.attrib;
>
<!--end of tocentry.module-->]]>

<![ %tocpart.module [
<!ENTITY % local.tocpart.attrib "">
<!ELEMENT ToCpart - O (ToCentry+, ToCchap*)>
<!ATTLIST ToCpart
		%common.attrib;
		%local.tocpart.attrib;
>
<!--end of tocpart.module-->]]>

<![ %tocchap.module [
<!ENTITY % local.tocchap.attrib "">
<!ELEMENT ToCchap - O (ToCentry+, ToClevel1*)>
<!ATTLIST ToCchap
		%label.attrib;
		%common.attrib;
		%local.tocchap.attrib;
>
<!--end of tocchap.module-->]]>

<![ %toclevel1.module [
<!ENTITY % local.toclevel1.attrib "">
<!ELEMENT ToClevel1 - O (ToCentry+, ToClevel2*)>
<!ATTLIST ToClevel1
		%common.attrib;
		%local.toclevel1.attrib;
>
<!--end of toclevel1.module-->]]>

<![ %toclevel2.module [
<!ENTITY % local.toclevel2.attrib "">
<!ELEMENT ToClevel2 - O (ToCentry+, ToClevel3*)>
<!ATTLIST ToClevel2
		%common.attrib;
		%local.toclevel2.attrib;
>
<!--end of toclevel2.module-->]]>

<![ %toclevel3.module [
<!ENTITY % local.toclevel3.attrib "">
<!ELEMENT ToClevel3 - O (ToCentry+, ToClevel4*)>
<!ATTLIST ToClevel3
		%common.attrib;
		%local.toclevel3.attrib;
>
<!--end of toclevel3.module-->]]>

<![ %toclevel4.module [
<!ENTITY % local.toclevel4.attrib "">
<!ELEMENT ToClevel4 - O (ToCentry+, ToClevel5*)>
<!ATTLIST ToClevel4
		%common.attrib;
		%local.toclevel4.attrib;
>
<!--end of toclevel4.module-->]]>

<![ %toclevel5.module [
<!ENTITY % local.toclevel5.attrib "">
<!ELEMENT ToClevel5 - O (ToCentry+)>
<!ATTLIST ToClevel5
		%common.attrib;
		%local.toclevel5.attrib;
>
<!--end of toclevel5.module-->]]>

<![ %tocback.module [
<!ENTITY % local.tocback.attrib "">
<!ELEMENT ToCback - O ((%para.char.mix;)+)>
<!ATTLIST ToCback
		%label.attrib;
		%pagenum.attrib;
		%common.attrib;
		%local.tocback.attrib;
>
<!--end of tocback.module-->]]>
<!--end of toc.content.module-->]]>

<![ %lot.content.module [
<![ %lot.module [
<!ENTITY % local.lot.attrib "">
<!ELEMENT LoT - O ((%bookcomponent.title.content;)?, LoTentry*)>
<!ATTLIST LoT
		%label.attrib;
		%common.attrib;
		%local.lot.attrib;
>
<!--end of lot.module-->]]>

<![ %lotentry.module [
<!ENTITY % local.lotentry.attrib "">
<!ELEMENT LoTentry - - ((%para.char.mix;)+ )>
<!ATTLIST LoTentry
		--SrcCredit: credit for source of illustration--
		SrcCredit	CDATA		#IMPLIED
		%pagenum.attrib;
		%common.attrib;
		%local.lotentry.attrib;
>
<!--end of lotentry.module-->]]>
<!--end of lot.content.module-->]]>

<!-- ...................................................................... -->
<!-- Appendix, Chapter, Part, Preface, Reference, PartIntro ............... -->

<![ %appendix.module; [
<!ENTITY % local.appendix.attrib "">
<!ELEMENT Appendix - O ((%bookcomponent.title.content;), ToCchap?,
		(%bookcomponent.content;)) +(%ubiq.mix;)>
<!ATTLIST Appendix
		%label.attrib;
		%common.attrib;
		%local.appendix.attrib;
>
<!--end of appendix.module-->]]>

<![ %chapter.module; [
<!ENTITY % local.chapter.attrib "">
<!ELEMENT Chapter - O ((%bookcomponent.title.content;), ToCchap?,
		(%bookcomponent.content;), (Index | Glossary | Bibliography)*)
		+(%ubiq.mix;)>
<!ATTLIST Chapter
		%label.attrib;
		%common.attrib;
		%local.chapter.attrib;
>
<!--end of chapter.module-->]]>

<![ %part.module; [
<!ENTITY % local.part.attrib "">
<!ELEMENT Part - - ((%bookcomponent.title.content;), PartIntro?,
		(%partcontent.mix;)+) +(%ubiq.mix;)>
<!ATTLIST Part
		%label.attrib;
		%common.attrib;
		%local.part.attrib;
>
<!--ELEMENT PartIntro (defined below)-->
<!--end of part.module-->]]>

<![ %preface.module; [
<!ENTITY % local.preface.attrib "">
<!ELEMENT Preface - O ((%bookcomponent.title.content;), 
		(%bookcomponent.content;)) +(%ubiq.mix;)>
<!ATTLIST Preface
		%common.attrib;
		%local.preface.attrib;
>
<!--end of preface.module-->]]>

<![ %reference.module; [
<!ENTITY % local.reference.attrib "">
<!ELEMENT Reference - O ((%bookcomponent.title.content;), PartIntro?,
		(%refentry.class;)+) +(%ubiq.mix;)>
<!ATTLIST Reference
		%label.attrib;
		%common.attrib;
		%local.reference.attrib;
>
<!--ELEMENT PartIntro (defined below)-->
<!--end of reference.module-->]]>

<![ %partintro.module; [
<!ENTITY % local.partintro.attrib "">
<!ELEMENT PartIntro - O ((%div.title.content;)?, (%bookcomponent.content;))
		+(%ubiq.mix;)>
<!ATTLIST PartIntro	
		%label.attrib;
		%common.attrib;
		%local.partintro.attrib;
>
<!--end of partintro.module-->]]>

<!-- ...................................................................... -->
<!-- DocInfo .............................................................. -->

<![ %docinfo.module; [
<!ENTITY % local.docinfo.attrib "">
<!ELEMENT DocInfo - - (Graphic*, (%div.title.content;), Subtitle?, 
		AuthorGroup+, Abstract*, RevHistory?, LegalNotice*)
		-(%ubiq.mix;)>
<!ATTLIST DocInfo
		%common.attrib;
		%local.docinfo.attrib;
>
<!--end of docinfo.module-->]]>
		
<!-- ...................................................................... -->
<!-- Sect1, Sect2, Sect3, Sect4, Sect5 .................................... -->

<![ %sect1.module; [
<!ENTITY % local.sect1.attrib "">
<!ELEMENT Sect1 - O ((%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect2* | SimpleSect*))
		| (%refentry.class;)+ | Sect2+ | SimpleSect+), (%nav.class;)*)
		+(%ubiq.mix;)>
<!ATTLIST Sect1
		--Renderas: alternate level at which this section should
		appear to be--
		Renderas	(Sect2
				|Sect3
				|Sect4
				|Sect5)		#IMPLIED
		%label.attrib;
		%common.attrib;
		%local.sect1.attrib;
>
<!--end of sect1.module-->]]>

<![ %sect2.module; [
<!ENTITY % local.sect2.attrib "">
<!ELEMENT Sect2 - O ((%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect3* | SimpleSect*))
		| (%refentry.class;)+ | Sect3+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect2
		--Renderas: alternate level at which this section should
		appear to be--
		Renderas	(Sect1
				|Sect3
				|Sect4
				|Sect5)		#IMPLIED
		%label.attrib;
		%common.attrib;
		%local.sect2.attrib;
>
<!--end of sect2.module-->]]>

<![ %sect3.module; [
<!ENTITY % local.sect3.attrib "">
<!ELEMENT Sect3 - O ((%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect4* | SimpleSect*))
		| (%refentry.class;)+ | Sect4+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect3
		--Renderas: alternate level at which this section should
		appear to be--
		Renderas	(Sect1
				|Sect2
				|Sect4
				|Sect5)		#IMPLIED
		%label.attrib;
		%common.attrib;
		%local.sect3.attrib;
>
<!--end of sect3.module-->]]>

<![ %sect4.module; [
<!ENTITY % local.sect4.attrib "">
<!ELEMENT Sect4 - O ((%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, 
		((%refentry.class;)* | Sect5* | SimpleSect*))
		| (%refentry.class;)+ | Sect5+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect4
		--Renderas: alternate level at which this section should
		appear to be--
		Renderas	(Sect1
				|Sect2
				|Sect3
				|Sect5)		#IMPLIED
		%label.attrib;
		%common.attrib;
		%local.sect4.attrib;
>
<!--end of sect4.module-->]]>

<![ %sect5.module; [
<!ENTITY % local.sect5.attrib "">
<!ELEMENT Sect5 - O ((%sect.title.content;), (%nav.class;)*,
		(((%divcomponent.mix;)+, ((%refentry.class;)* | SimpleSect*))
		| (%refentry.class;)+ | SimpleSect+), (%nav.class;)*)>
<!ATTLIST Sect5
		--Renderas: alternate level at which this section should
		appear to be--
		Renderas	(Sect1
				|Sect2
				|Sect3
				|Sect4)		#IMPLIED
		%label.attrib;
		%common.attrib;
		%local.sect5.attrib;
>
<!--end of sect5.module-->]]>

<![ %simplesect.module; [
<!ENTITY % local.simplesect.attrib "">
<!ELEMENT SimpleSect - O ((%sect.title.content;), (%divcomponent.mix;)+)
		+(%ubiq.mix;)>
<!ATTLIST SimpleSect
		%common.attrib;
		%local.simplesect.attrib;
>
<!--end of simplesect.module-->]]>

<!-- ...................................................................... -->
<!-- Bibliography ......................................................... -->

<![ %bibliography.content.module; [
<![ %bibliography.module; [
<!ENTITY % local.bibliography.attrib "">
<!ELEMENT Bibliography - O ((%bookcomponent.title.content;)?,
		(%component.mix;)*, (BiblioDiv+ | BiblioEntry+))>
<!ATTLIST Bibliography
		%common.attrib;
		%local.bibliography.attrib;
>
<!--end of bibliography.module-->]]>

<![ %bibliodiv.module; [
<!ENTITY % local.bibliodiv.attrib "">
<!ELEMENT BiblioDiv - O ((%sect.title.content;)?, (%component.mix;)*,
		BiblioEntry+)>
<!ATTLIST BiblioDiv
		%common.attrib;
		%local.bibliodiv.attrib;
>
<!--end of bibliodiv.module-->]]>
<!--end of bibliography.content.module-->]]>

<!-- ...................................................................... -->
<!-- Glossary ............................................................. -->

<![ %glossary.content.module; [
<![ %glossary.module; [
<!ENTITY % local.glossary.attrib "">
<!ELEMENT Glossary - O ((%bookcomponent.title.content;)?, (%component.mix;)*,
		(GlossDiv+ | GlossEntry+), Bibliography?)>
<!ATTLIST Glossary
		%common.attrib;
		%local.glossary.attrib;
>
<!--end of glossary.module-->]]>

<![ %glossdiv.module; [
<!ENTITY % local.glossdiv.attrib "">
<!ELEMENT GlossDiv - O ((%sect.title.content;), (%component.mix;)*,
		GlossEntry+)>
<!ATTLIST GlossDiv
		%common.attrib;
		%local.glossdiv.attrib;
>
<!--end of glossdiv.module-->]]>
<!--end of glossary.content.module-->]]>

<!-- ...................................................................... -->
<!-- Index and SetIndex ................................................... -->

<![ %index.content.module; [
<![ %indexes.module; [
<!ENTITY % local.indexes.attrib "">
<!ELEMENT (%index.class;) - O ((%bookcomponent.title.content;)?,
		(%component.mix;)*, (IndexDiv* | IndexEntry*))
		-(%ndxterm.class;)>
<!ATTLIST (%index.class;)
		%common.attrib;
		%local.indexes.attrib;
>
<!--end of indexes.module-->]]>

<![ %indexdiv.module; [

<!-- SegmentedList in this content is useful for marking up permuted
     indices. -->

<!ENTITY % local.indexdiv.attrib "">
<!ELEMENT IndexDiv - O ((%sect.title.content;)?, ((%indexdivcomponent.mix;)*,
		(IndexEntry+ | SegmentedList)))>
<!ATTLIST IndexDiv
		%common.attrib;
		%local.indexdiv.attrib;
>
<!--end of indexdiv.module-->]]>

<![ %indexentry.module; [
<!-- Index entries appear in the index, not the text. -->

<!ENTITY % local.indexentry.attrib "">
<!ELEMENT IndexEntry - O (PrimaryIE, (SeeIE|SeeAlsoIE)*,
		(SecondaryIE, (SeeIE|SeeAlsoIE|TertiaryIE)*)*)>
<!ATTLIST IndexEntry
		%common.attrib;
		%local.indexentry.attrib;
>
<!--end of indexentry.module-->]]>

<![ %primsecterie.module; [
<!ENTITY % local.primsecterie.attrib "">
<!ELEMENT (PrimaryIE | SecondaryIE | TertiaryIE) - O ((%ndxterm.char.mix;)+)>
<!ATTLIST (PrimaryIE | SecondaryIE | TertiaryIE)
		%linkends.attrib; --to IndexTerms that these entries represent--
		%common.attrib;
		%local.primsecterie.attrib;
>
<!--end of primsecterie.module-->]]>
	
<![ %seeie.module; [
<!ENTITY % local.seeie.attrib "">
<!ELEMENT SeeIE - O ((%ndxterm.char.mix;)+)>
<!ATTLIST SeeIE
		%linkend.attrib; --to IndexEntry to look up--
		%common.attrib;
		%local.seeie.attrib;
>
<!--end of seeie.module-->]]>

<![ %seealsoie.module; [
<!ENTITY % local.seealsoie.attrib "">
<!ELEMENT SeeAlsoIE - O ((%ndxterm.char.mix;)+)>
<!ATTLIST SeeAlsoIE
		%linkends.attrib; --to related IndexEntries--
		%common.attrib;
		%local.seealsoie.attrib;
>
<!--end of seealsoie.module-->]]>
<!--end of index.content.module-->]]>

<!-- ...................................................................... -->
<!-- RefEntry ............................................................. -->

<![ %refentry.content.module; [
<![ %refentry.module; [
<!ENTITY % local.refentry.attrib "">
<!ELEMENT RefEntry - O (DocInfo?, RefMeta?, (Comment|%link.char.class;)*,
                        RefNameDiv, RefSynopsisDiv?, RefSect1+) +(%ubiq.mix;)>
<!ATTLIST RefEntry
		%common.attrib;
		%local.refentry.attrib;
>
<!--end of refentry.module-->]]>

<![ %refmeta.module; [
<!ENTITY % local.refmeta.attrib "">
<!ELEMENT RefMeta - - (RefEntryTitle, ManVolNum?, RefMiscInfo*)
		-(BeginPage)>
<!ATTLIST RefMeta
		 %common.attrib;
		 %local.refmeta.attrib;
>
<!--end of refmeta.module-->]]>

<![ %refmiscinfo.module; [
<!ENTITY % local.refmiscinfo.attrib "">
<!ELEMENT RefMiscInfo - - ((%docinfo.char.mix;)+)>
<!ATTLIST RefMiscInfo
		Class		CDATA		#IMPLIED
		%common.attrib;
		%local.refmiscinfo.attrib;
>
<!--end of refmiscinfo.module-->]]>

<![ %refnamediv.module; [
<!ENTITY % local.refnamediv.attrib "">
<!ELEMENT RefNameDiv - O (RefDescriptor?, RefName+, RefPurpose, RefClass*,
		(Comment|%link.char.class;)*)>
<!ATTLIST RefNameDiv
		%common.attrib;
		%local.refnamediv.attrib;
>
<!--end of refnamediv.module-->]]>
	
<![ %refdescriptor.module; [
<!ENTITY % local.refdescriptor.attrib "">
<!ELEMENT RefDescriptor - O ((%refname.char.mix;)+)>
<!ATTLIST RefDescriptor
		%common.attrib;
		%local.refdescriptor.attrib;
>
<!--end of refdescriptor.module-->]]>

<![ %refname.module; [
<!ENTITY % local.refname.attrib "">
<!ELEMENT RefName - O ((%refname.char.mix;)+)>
<!ATTLIST RefName
		%common.attrib;
		%local.refname.attrib;
>
<!--end of refname.module-->]]>

<![ %refpurpose.module; [
<!ENTITY % local.refpurpose.attrib "">
<!ELEMENT RefPurpose - O ((%refinline.char.mix;)+)>
<!ATTLIST RefPurpose
		%common.attrib;
		%local.refpurpose.attrib;
>
<!--end of refpurpose.module-->]]>

<![ %refclass.module; [
<!ENTITY % local.refclass.attrib "">
<!ELEMENT RefClass - O ((%refclass.char.mix;)+)>
<!ATTLIST RefClass
		%common.attrib;
		%local.refclass.attrib;
>
<!--end of refclass.module-->]]>

<![ %refsynopsisdiv.module; [
<!ENTITY % local.refsynopsisdiv.attrib "">
<!ELEMENT RefSynopsisDiv - O ((%refsect.title.content;)?,
		(((%refcomponent.mix;)+, RefSect2*) | (RefSect2+)))>
<!ATTLIST RefSynopsisDiv
		%common.attrib;
		%local.refsynopsisdiv.attrib;
>
<!--end of refsynopsisdiv.module-->]]>

<![ %refsect1.module; [
<!ENTITY % local.refsect1.attrib "">
<!ELEMENT RefSect1 - O ((%refsect.title.content;),
                        (((%refcomponent.mix;)+, RefSect2*) | RefSect2+))>
<!ATTLIST RefSect1
		%common.attrib;
		%local.refsect1.attrib;
>
<!--end of refsect1.module-->]]>

<![ %refsect2.module; [
<!ENTITY % local.refsect2.attrib "">
<!ELEMENT RefSect2 - O ((%refsect.title.content;),
                        (((%refcomponent.mix;)+, RefSect3*) | RefSect3+))>
<!ATTLIST RefSect2
		%common.attrib;
		%local.refsect2.attrib;
>
<!--end of refsect2.module-->]]>

<![ %refsect3.module; [
<!ENTITY % local.refsect3.attrib "">
<!ELEMENT RefSect3 - O ((%refsect.title.content;), (%refcomponent.mix;)+)>
<!ATTLIST RefSect3
		%common.attrib;
		%local.refsect3.attrib;
>
<!--end of refsect3.module-->]]>
<!--end of refentry.content.module-->]]>

<!-- ...................................................................... -->
<!-- Article .............................................................. -->

<![ %article.module; [
<!-- This Article model is derived from the MAJOUR header DTD.  See
     the DocBook documentation for a summary of changes. -->

<!ENTITY % local.article.attrib "">
<!ELEMENT Article - O (ArtHeader, (%bookcomponent.content;),
                       ((%nav.class;) | (%appendix.class;) | Ackno)*)
                      +(%ubiq.mix;)>
<!ATTLIST Article
		--ParentBook: pointer to book in which this article resides--
		ParentBook	IDREF		#IMPLIED
		%common.attrib;
		%local.article.attrib;
>
<!--end of article.module-->]]>

<!-- End of DocBook document hierarchy module V2.4.1 ...................... -->
<!-- ...................................................................... -->
