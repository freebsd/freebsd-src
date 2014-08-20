<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2001-2003 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
--><!DOCTYPE test SYSTEM "dom1.dtd">

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_elementcreatenewattribute">
<metadata>
<title>hc_elementCreateNewAttribute</title>
<creator>Curt Arnold</creator>
<description>
    The "setAttributeNode(newAttr)" method adds a new 
   attribute to the Element.  
   
   Retrieve first address element and add
   a new attribute node to it by invoking its         
   "setAttributeNode(newAttr)" method.  This test makes use
   of the "createAttribute(name)" method from the Document
   interface.
</description>

<date qualifier="created">2002-06-09</date>
<!--setAttributeNode-->
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-887236154"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=243"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="testAddress" type="Element"/>
<var name="newAttribute" type="Attr"/>
<var name="oldAttr" type="Attr"/>
<var name="districtAttr" type="Attr"/>
<var name="attrVal" type="DOMString"/>
<load var="doc" href="hc_staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc" tagname='"acronym"' var="elementList"/>
<item interface="NodeList" obj="elementList" index="0" var="testAddress"/>
<createAttribute obj="doc" var="newAttribute" name='"lang"'/>
<setAttributeNode obj="testAddress" var="oldAttr" newAttr="newAttribute"/>
<assertNull actual="oldAttr" id="old_attr_doesnt_exist"/>
<getAttributeNode obj="testAddress" var="districtAttr" name='"lang"'/>
<assertNotNull actual="districtAttr" id="new_district_accessible"/>
<getAttribute var="attrVal" obj="testAddress" name='"lang"'/>
<assertEquals actual="attrVal" expected='""' id="attr_value" ignoreCase="false"/>
</test>
