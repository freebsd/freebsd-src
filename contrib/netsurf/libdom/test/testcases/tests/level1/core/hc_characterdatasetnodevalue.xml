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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_characterdatasetnodevalue">
<metadata>
<title>hc_characterdataSetNodeValue</title>
<creator>Curt Arnold</creator>
<description>
  The "setNodeValue()" method changes the character data 
  currently stored in the node.
  Retrieve the character data from the second child 
  of the first employee and invoke the "setNodeValue()" 
  method, call "getData()" and compare.
</description>
<date qualifier="created">2002-06-09</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-72AB8359"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="nameNode" type="Node"/>
<var name="child" type="CharacterData"/>
<var name="childData" type="DOMString"/>
<var name="childValue" type="DOMString"/>
<load var="doc" href="hc_staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc" tagname='"strong"' var="elementList"/>
<item interface="NodeList" obj="elementList" index="0" var="nameNode"/>
<firstChild interface="Node" obj="nameNode" var="child"/>
<nodeValue obj="child" value='"Marilyn Martin"'/>
<data interface="CharacterData" obj="child" var="childData"/>
<assertEquals actual="childData" expected='"Marilyn Martin"' id="data" ignoreCase="false"/>
<nodeValue obj="child" var="childValue"/>
<assertEquals actual="childValue" expected='"Marilyn Martin"' id="value" ignoreCase="false"/>
</test>
