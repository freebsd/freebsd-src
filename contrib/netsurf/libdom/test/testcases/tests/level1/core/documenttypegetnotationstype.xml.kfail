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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="documenttypegetnotationstype">
<metadata>
<title>documenttypeGetNotationsType</title>
<creator>NIST</creator>
<description>
    Every node in the map returned by the "getNotations()"
   method implements the Notation interface.
   
   Retrieve the Document Type for this document and create
   a NamedNodeMap object of all the notations.  Traverse
   the entire list and examine the NodeType of each node.
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-D46829EF"/>
</metadata>
<var name="doc" type="Document"/>
<var name="docType" type="DocumentType"/>
<var name="notationList" type="NamedNodeMap"/>
<var name="notation" type="Node"/>
<var name="notationType" type="int"/>
<load var="doc" href="staff" willBeModified="false"/>
<doctype obj="doc" var="docType"/>
<assertNotNull actual="docType" id="docTypeNotNull"/>
<notations obj="docType" var="notationList"/>
<assertNotNull actual="notationList" id="notationsNotNull"/>
<for-each collection="notationList" member="notation">
<nodeType obj="notation" var="notationType"/>
<assertEquals actual="notationType" expected="12" id="documenttypeGetNotationsTypeAssert" ignoreCase="false"/>
</for-each>
</test>
