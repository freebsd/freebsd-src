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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_documentgetimplementation">
<metadata>
<title>hc_documentgetimplementation</title>
<creator>Curt Arnold</creator>
<description>
   Retrieve the entire DOM document and invoke its 
   "getImplementation()" method.  If contentType="text/html", 
   DOMImplementation.hasFeature("HTML","1.0") should be true.  
   Otherwise, DOMImplementation.hasFeature("XML", "1.0")
   should be true.
</description>

<date qualifier="created">2002-06-09</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-1B793EBA"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=245"/>
</metadata>
<var name="doc" type="Document"/>
<var name="docImpl" type="DOMImplementation"/>
<var name="xmlstate" type="boolean"/>
<var name="htmlstate" type="boolean"/>
<load var="doc" href="hc_staff" willBeModified="false"/>
<implementation obj="doc" var="docImpl"/>
<hasFeature obj="docImpl" var="xmlstate" feature='"XML"' version='"1.0"'/>
<hasFeature obj="docImpl" var="htmlstate" feature='"HTML"' version='"1.0"'/>
<if><contentType type="text/html"/>
<assertTrue actual="htmlstate" id="supports_HTML_1.0"/>
<else>
<assertTrue actual="xmlstate" id="supports_XML_1.0"/>
</else>
</if>
</test>
