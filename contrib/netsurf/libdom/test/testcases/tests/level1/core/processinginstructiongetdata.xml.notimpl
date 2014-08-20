<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2001-2004 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
--><!DOCTYPE test SYSTEM "dom1.dtd">

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="processinginstructiongetdata">
<metadata>
<title>processinginstructionGetData</title>
<creator>NIST</creator>
<description>
    The "getData()" method returns the content of the  
   processing instruction.  It starts at the first non
   white character following the target and ends at the
   character immediately preceding the "?&gt;".
   
   Retrieve the ProcessingInstruction node located  
   immediately after the prolog.  Create a nodelist of the 
   child nodes of this document.  Invoke the "getData()"
   method on the first child in the list. This should
   return the content of the ProcessingInstruction.
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-837822393"/>
</metadata>
<var name="doc" type="Document"/>
<var name="childNodes" type="NodeList"/>
<var name="piNode" type="ProcessingInstruction"/>
<var name="data" type="DOMString"/>
<load var="doc" href="staff" willBeModified="false"/>
<childNodes obj="doc" var="childNodes"/>
<item interface="NodeList" obj="childNodes" var="piNode" index="0"/>
<data interface="ProcessingInstruction" obj="piNode" var="data"/>
<assertEquals actual="data" expected="&quot;PIDATA&quot;" id="processinginstructionGetTargetAssert" ignoreCase="false"/>
</test>
