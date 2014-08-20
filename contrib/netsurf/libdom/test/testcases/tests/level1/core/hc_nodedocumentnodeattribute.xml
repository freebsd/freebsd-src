<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2001 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
--><!DOCTYPE test SYSTEM "dom1.dtd">

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_nodedocumentnodeattribute">
<metadata>
<title>hc_nodedocumentnodeattribute</title>
<creator>Curt Arnold</creator>
<description>
The "getAttributes()" method invoked on a Document
Node returns null.

Retrieve the DOM Document and invoke the
"getAttributes()" method on the Document Node.
It should return null.
</description>

<date qualifier="created">2002-06-09</date>
<!--attributes attribute -->
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-84CF096"/>
<!-- Document interface -->
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#i-Document"/>
</metadata>
<var name="doc" type="Document"/>
<var name="attrList" type="NamedNodeMap"/>
<load var="doc" href="hc_staff" willBeModified="false"/>
<attributes obj="doc" var="attrList"/>
<assertNull actual="attrList" id="doc_attributes_is_null"/>
</test>
