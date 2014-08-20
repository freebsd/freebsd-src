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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns03">
<metadata>
<title>namednodemapsetnameditemns03</title>
<creator>IBM</creator>
<description>
	The method setNamedItemNS adds a node using its namespaceURI and localName and 
	raises a WRONG_DOCUMENT_ERR if arg was created from a different document than the 
	one that created this map.
	
	Retreieve the second element whose local name is address and its attribute into a named node map.
	Do the same for another document and retreive its street attribute.  Call the setNamedItemNS
	using the first namedNodeMap and the retreive street attribute of the second.  This should
	raise a WRONG_DOCUMENT_ERR. 
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-setNamedItemNS"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=259"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=408"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="docAlt" type="Document"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="attributesAlt" type="NamedNodeMap"/>
<var name="elementList" type="NodeList"/>
<var name="elementListAlt" type="NodeList"/>
<var name="element" type="Element"/>
<var name="elementAlt" type="Element"/>
<var name="attr" type="Attr"/>
<var name="newNode" type="Node"/>
<var name="nullNS" type="DOMString" isNull="true"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<getElementsByTagNameNS var="elementList" obj="doc" namespaceURI='"*"' localName='"address"' interface="Document"/>
<item var="element" obj="elementList" index="1"  interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<load var="docAlt" href="staffNS" willBeModified="true"/>
<getElementsByTagNameNS var="elementListAlt" obj="docAlt" namespaceURI='"*"' localName='"address"'  interface="Document"/>
<item var="elementAlt" obj="elementListAlt" index="1"  interface="NodeList"/>
<attributes var="attributesAlt" obj="elementAlt"/>
<getNamedItemNS var="attr" obj="attributesAlt" namespaceURI="nullNS" localName='"street"'/>
<removeNamedItemNS var="newNode" obj="attributesAlt" namespaceURI="nullNS" localName='"street"'/>
<assertDOMException id="throw_WRONG_DOCUMENT_ERR">
<WRONG_DOCUMENT_ERR>
<setNamedItemNS var="newNode" obj="attributes" arg="attr"/>
</WRONG_DOCUMENT_ERR>
</assertDOMException>
</test>
