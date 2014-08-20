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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="nodedocumentfragmentnodetype">
<metadata>
<title>nodeDocumentFragmentNodeType</title>
<creator>NIST</creator>
<description>
    The "getNodeType()" method for a DocumentFragment Node
    returns the constant value 11.

    Invoke the "createDocumentFragment()" method and    
    examine the NodeType of the document fragment
    returned by the "getNodeType()" method.   The method 
    should return 11. 
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-111237558"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-B63ED1A3"/>
</metadata>
<var name="doc" type="Document"/>
<var name="documentFragmentNode" type="DocumentFragment"/>
<var name="nodeType" type="int"/>
<load var="doc" href="staff" willBeModified="true"/>
<createDocumentFragment obj="doc" var="documentFragmentNode"/>
<nodeType obj="documentFragmentNode" var="nodeType"/>
<assertEquals actual="nodeType" expected="11" id="nodeDocumentFragmentNodeTypeAssert1" ignoreCase="false"/>
</test>
