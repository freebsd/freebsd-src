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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_nodeelementnodeattributes">
<metadata>
<title>hc_nodeelementnodeattributes</title>
<creator>Curt Arnold</creator>
<description>
    Retrieve the third "acronym" element and evaluate Node.attributes.
</description>

<date qualifier="created">2002-06-09</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-84CF096"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=236"/>
<subject resource="http://lists.w3.org/Archives/Public/www-dom-ts/2003Jun/0011.html"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=184"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="testAddr" type="Element"/>
<var name="addrAttr" type="NamedNodeMap"/>
<var name="attrNode" type="Node"/>
<var name="attrName" type="DOMString"/>
<var name="attrList" type="Collection"/>
<var name="htmlExpected" type="Collection">
<member>"title"</member>
<member>"class"</member>
</var>
<var name="expected" type="Collection">
<member>"title"</member>
<member>"class"</member>
<member>"dir"</member>
</var>
<load var="doc" href="hc_staff" willBeModified="false"/>
<getElementsByTagName interface="Document" obj="doc" tagname='"acronym"' var="elementList"/>
<item interface="NodeList" obj="elementList" index="2" var="testAddr"/>
<attributes obj="testAddr" var="addrAttr"/>
<for-each collection="addrAttr" member="attrNode">
<nodeName obj="attrNode" var="attrName"/>
<append collection="attrList" item="attrName"/>
</for-each>
<if><contentType type="text/html"/>
<assertEquals actual="attrList" expected="htmlExpected" id="attrNames_html" 
	ignoreCase="true"/>
<else>
<assertEquals actual="attrList" expected="expected" id="attrNames" ignoreCase="false"/>
</else>
</if>
</test>
