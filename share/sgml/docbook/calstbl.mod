<!-- ...................................................................... -->
<!-- CALS-based DocBook table model V2.4.1 ................................ -->
<!-- File calstbl.mod ..................................................... -->

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

<!-- This module contains the definitions for table markup in DocBook
     documents.  It has no dependencies.  It is based on a preliminary
     parameterization of the CALS MIL-M-28001B model done by SGML Open;
     thanks to SGML Open for allowing Davenport to examine and use 
     these constructs.

     This module is referenced and parameterized by the information
     pool module; see that module for more information.  In modules
     or driver files referring to this module, please use an entity
     declaration that uses the public identifier shown below:

     "-//Davenport//ELEMENTS CALS-Based DocBook Table Model V2.4.1//EN"

     See the maintenance documentation for detailed information on the
     parameter entity and module scheme used in DocBook, customizing
     DocBook and planning for interchange, and changes made since the
     last release of DocBook.
-->

<!ENTITY % bodyatt "">
<!ENTITY % secur "">
<!ENTITY % yesorno 'NUMBER'>
<!ENTITY % titles 'title'>
<!ENTITY % paracon '#PCDATA'>

<!ENTITY % tblelm "(Table|Chart)">
<!ENTITY % tblmdl "(%titles;?, TGroup+)|Graphic+">
<!ENTITY % tblexpt " -(Table|Chart|Figure)">
<!ENTITY % tblatt '
		tabstyle	NMTOKEN		#IMPLIED
		orient		(port|land)	#IMPLIED
		pgwide		%yesorno;	#IMPLIED'>

<!ENTITY % tblgrp "ColSpec*, SpanSpec*, THead?, TFoot?, TBody">
<!ENTITY % tblgrpatt '
		tgroupstyle	NMTOKEN		#IMPLIED'>

<!ENTITY % tblhfmd "Colspec*, Row+">
<!ENTITY % tblhdft "(THead|TFoot)">
<!ENTITY % tblhfex " -(EntryTbl)">
<!ENTITY % tblrow "Entry|EntryTbl">
<!ENTITY % tblrowex "">
<!ENTITY % tblcon "(Para|Warning|Caution|Note|Legend|%paracon;)+">
<!ENTITY % tblconex "">

<!ELEMENT %tblelm; - - (%tblmdl;) %tblexpt;>
<!ATTLIST %tblelm;
		Colsep		%yesorno;	#IMPLIED
		Frame		(Top
				|Bottom
				|Topbot
				|All
				|Sides
				|None)		#IMPLIED
		Rowsep		%yesorno;	#IMPLIED
		Shortentry	%yesorno;	#IMPLIED
		Tocentry	%yesorno;	1
		%tblatt;
		%bodyatt;
		%secur;
>

<!ELEMENT TGroup - O (%tblgrp;)>
<!ATTLIST TGroup
		Align		(Left
				|Right
				|Center
				|Justify
				|Char)		Left
		Char		CDATA		""
		Charoff		NUTOKEN		"50"
		Cols		NUMBER		#REQUIRED
		Colsep		%yesorno;	#IMPLIED
		Rowsep		%yesorno;	#IMPLIED
		%tblgrpatt;
		%secur;
>

<!ELEMENT ColSpec - O EMPTY>
<!ATTLIST ColSpec
		Align		(Left
				|Right
				|Center
				|Justify
				|Char)		#IMPLIED
		Char		CDATA		#IMPLIED
		Charoff		NUTOKEN		#IMPLIED
		Colname		NMTOKEN		#IMPLIED
		Colnum		NUMBER		#IMPLIED
		Colsep		%yesorno;	#IMPLIED
		Colwidth	CDATA		#IMPLIED
		Rowsep		%yesorno;	#IMPLIED
>

<!ELEMENT SpanSpec - O  EMPTY>
<!ATTLIST SpanSpec
		Align		(Left
				|Right
				|Center
				|Justify
				|Char)		"Center"
		Char		CDATA		#IMPLIED
		Charoff		NUTOKEN		#IMPLIED
		Colsep		%yesorno;	#IMPLIED
		Nameend		NMTOKEN		#REQUIRED
		Namest		NMTOKEN		#REQUIRED
		Rowsep		%yesorno;	#IMPLIED
		Spanname	NMTOKEN		#REQUIRED
>

<!ELEMENT %tblhdft; - O (%tblhfmd;) %tblhfex;>

<!-- Original VAlign default was Bottom for THead and Top for TFoot. -->
<!ATTLIST %tblhdft;
		VAlign		(Top
				|Middle
				|Bottom)	#IMPLIED
		%secur;
>

<!ELEMENT TBody - O (Row+)>
<!ATTLIST TBody
		VAlign		(Top
				|Middle
				|Bottom)	"Top"
		%secur;
>

<!ELEMENT Row - O (%tblrow;)+ %tblrowex;>
<!ATTLIST Row
		Rowsep		%yesorno;	#IMPLIED
		VAlign		(Top
				|Middle
				|Bottom)	#IMPLIED
		%secur;
>

<!ELEMENT Entry - O %tblcon; %tblconex;>
<!ATTLIST Entry
		Align		(Left
				|Right
				|Center
				|Justify
				|Char)		#IMPLIED
		Char		CDATA		#IMPLIED
		Charoff		NUTOKEN		#IMPLIED
		Colname		NMTOKEN		#IMPLIED
		Colsep		%yesorno;	#IMPLIED
		Morerows	NUMBER		"0"
		Nameend		NMTOKEN		#IMPLIED
		Namest		NMTOKEN		#IMPLIED
		Rotate		%yesorno;	"0"
		Rowsep		%yesorno;	#IMPLIED
		Spanname	NMTOKEN		#IMPLIED
		VAlign		(Top
				|Middle
				|Bottom)	#IMPLIED
		%secur;
>

<!ELEMENT EntryTbl - - ((ColSpec*, SpanSpec*, THead?, TBody)+) -(EntryTbl)>
<!ATTLIST EntryTbl
		Align		(Left
				|Right
				|Center
				|Justify
				|Char)		#IMPLIED
		Char		CDATA		#IMPLIED
		Charoff		NUTOKEN		#IMPLIED
		Colname		NMTOKEN		#IMPLIED
		Cols		NUMBER		#REQUIRED
		Colsep		%yesorno;	#IMPLIED
		Nameend		NMTOKEN		#IMPLIED
		Namest		NMTOKEN		#IMPLIED
		Rowsep		%yesorno;	#IMPLIED
		Spanname	NMTOKEN		#IMPLIED
		%tblgrpatt;
		%secur;
>

<!-- End of CALS-based DocBook table model V2.4.1 ......................... -->
<!-- ...................................................................... -->

