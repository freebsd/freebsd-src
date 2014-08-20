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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_namednodemapnumberofnodes">
<metadata>
<title>hc_namednodemapnumberofnodes</title>
<creator>Curt Arnold</creator>
<description>
   Retrieve the second "p" element and evaluate Node.attributes.length.
</description>

<date qualifier="created">2002-06-09</date>
<!--attributes attribute -->
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-84CF096"/>
<!--length attribute -->
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-6D0FB19E"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=250"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="testEmployee" type="Node"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="length" type="int"/>
<load var="doc" href="hc_staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc" var="elementList" tagname='"acronym"'/>
<item interface="NodeList" obj="elementList" var="testEmployee" index="2"/>
<attributes obj="testEmployee" var="attributes"/>
<length var="length" obj="attributes" interface="NamedNodeMap"/>
<if><contentType type="text/html"/>
<assertEquals actual="length" expected="2" id="htmlLength" ignoreCase="false"/>
<else>
<assertEquals actual="length" expected="3" id="length" ignoreCase="false"/>
</else>
</if>
</test>
