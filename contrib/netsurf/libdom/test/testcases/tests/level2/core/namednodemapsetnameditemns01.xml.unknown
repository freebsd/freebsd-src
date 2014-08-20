<?xml version="1.0" encoding="UTF-8"?><?xml-stylesheet href="test-to-html.xsl" type="text/xml"?>

<!--

Copyright (c) 2001 World Wide Web Consortium, 
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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns01">
<metadata>
<title>namednodemapsetnameditemns01</title>
<creator>IBM</creator>
<description>
	The method setNamedItemNS adds a node using its namespaceURI and localName. If a node with 
	that namespace URI and that local name is already present in this map, it is replaced 
	by the new one.
	
	Retreive the first element whose localName is address and namespaceURI http://www.nist.gov", 
	and put its attributes into a named node map.  Create a new attribute node and add it to this map.  
	Verify if the attr node was successfully added by checking the nodeName of the retreived atttribute.
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
<var name="newAttribute" type="Attr"/>
<var name="newAttr1" type="Attr"/>
<var name="elementList" type="NodeList"/>
<var name="attrName" type="DOMString"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<getElementsByTagNameNS var="elementList" obj="doc" namespaceURI='"http://www.nist.gov"' localName='"address"' interface="Document"/>
<item var="element" obj="elementList" index="0" interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<createAttributeNS var="newAttr1" obj="doc" namespaceURI='"http://www.w3.org/DOM/L1"' qualifiedName='"streets"'/>
<setAttributeNodeNS var="newAttribute" obj="element" newAttr="newAttr1"/>
<getNamedItemNS var="attribute" obj="attributes" namespaceURI='"http://www.w3.org/DOM/L1"' localName='"streets"'/>
<nodeName var="attrName" obj="attribute"/>
<assertEquals actual="attrName" expected='"streets"' id="namednodemapsetnameditemns01" ignoreCase="false"/>
</test>
		