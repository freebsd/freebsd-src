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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="elementretrieveallattributes">
<metadata>
<title>elementRetrieveAllAttributes</title>
<creator>NIST</creator>
<description>
    The "getAttributes()" method(Node Interface) may
   be used to retrieve the set of all attributes of an
   element. 
   
   Create a list of all the attributes of the last child
   of the first employee by using the "getAttributes()"
   method.  Examine the length of the attribute list.  
   This test uses the "getLength()" method from the        
   NameNodeMap interface.
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<!--attributes attribute -->
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-84CF096"/>
<!--  DOM WG opinion on default attributes -->
<subject resource="http://lists.w3.org/Archives/Public/www-dom-ts/2002Mar/0002.html"/>
</metadata>
<implementationAttribute name="validating" value="true"/>
<var name="doc" type="Document"/>
<var name="addressList" type="NodeList"/>
<var name="testAddress" type="Node"/>
<var name="attributes" type="NamedNodeMap"/>
<load var="doc" href="staff" willBeModified="false"/>
<getElementsByTagName interface="Document" obj="doc" tagname="&quot;address&quot;" var="addressList"/>
<item interface="NodeList" obj="addressList" index="0" var="testAddress"/>
<attributes obj="testAddress" var="attributes"/>
<assertSize collection="attributes" size="2" id="elementRetrieveAllAttributesAssert"/>
</test>
