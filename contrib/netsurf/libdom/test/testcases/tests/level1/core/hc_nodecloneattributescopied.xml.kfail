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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_nodecloneattributescopied">
<metadata>
<title>hc_nodecloneattributescopied</title>
<creator>Curt Arnold</creator>
<description>
    Retrieve the second acronym element and invoke
    the cloneNode method.   The
    duplicate node returned by the method should copy the
    attributes associated with this node.
</description>

<date qualifier="created">2002-06-09</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-84CF096"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=236"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=184"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="addressNode" type="Node"/>
<var name="clonedNode" type="Node"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="attributeNode" type="Node"/>
<var name="attributeName" type="DOMString"/>
<var name="result" type="Collection"/>
<var name="htmlExpected" type="Collection">
<member>"class"</member>
<member>"title"</member>
</var>
<var name="expected" type="Collection">
<member>"class"</member>
<member>"title"</member>
<member>"dir"</member>
</var>
<load var="doc" href="hc_staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc" tagname='"acronym"' var="elementList"/>
<item interface="NodeList" obj="elementList" index="1" var="addressNode"/>
<cloneNode obj="addressNode" deep="false" var="clonedNode"/>
<attributes obj="clonedNode" var="attributes"/>
<for-each collection="attributes" member="attributeNode">
<nodeName obj="attributeNode" var="attributeName"/>
<append collection="result" item="attributeName"/>
</for-each>
<if><contentType type="text/html"/>
<assertEquals actual="result" expected="htmlExpected" id="nodeNames_html" ignoreCase="true"/>
<else>
<assertEquals actual="result" expected="expected" id="nodeNames" ignoreCase="false"/>
</else>
</if>
</test>
