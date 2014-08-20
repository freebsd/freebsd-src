<?xml version="1.0" encoding="UTF-8"?><?xml-stylesheet href="test-to-html.xsl" type="text/xml"?>

<!--

Copyright (c) 2001-2004 World Wide Web Consortium, 
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University).  All 
Rights Reserved.  This program is distributed under the W3C's Software
Intellectual Property License.  This program is distributed in the 
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
PURPOSE.  

See W3C License http://www.w3.org/Consortium/Legal/ for more details.

-->
<!DOCTYPE test SYSTEM "dom2.dtd">
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapgetnameditemns05">
<metadata>
<title>namednodemapgetnameditemns05</title>
<creator>IBM</creator>
<description>
	The method getNamedItemNS retrieves a node specified by local name and namespace URI. 
	
	Retreieve the second address element and its attribute into a named node map.
	Try retreiving the street attribute from the namednodemap using the
	default namespace uri and the street attribute name.  Since the default
	namespace doesnot apply to attributes this should return null.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-getNamedItemNS"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="element" type="Node"/>
<var name="attribute" type="Attr"/>
<var name="elementList" type="NodeList"/>
<load var="doc" href="staffNS" willBeModified="false"/>
<getElementsByTagNameNS var="elementList" obj="doc" namespaceURI='"*"' localName='"address"' interface="Document"/>
<item var="element" obj="elementList" index="1" interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<getNamedItemNS var="attribute" obj="attributes" namespaceURI='"*"' localName='"street"'/>
<assertNull actual="attribute" id="namednodemapgetnameditemns05"/>
</test>
