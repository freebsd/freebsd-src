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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns02">
<metadata>
<title>namednodemapsetnameditemns02</title>
<creator>IBM</creator>
<description>
	The method setNamedItemNS adds a node using its namespaceURI and localName. If a node with 
	that namespace URI and that local name is already present in this map, it is replaced 
	by the new one.
	
	Create a new element and attribute Node and add the newly created attribute node to the elements 
	NamedNodeMap.  Verify if the new attr node has been successfully added to the map by checking 
	the nodeName of the retreived atttribute from the list of attribute nodes in this map.

</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-setNamedItemNS"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="element" type="Element"/>
<var name="attribute" type="Attr"/>
<var name="attribute1" type="Attr"/>
<var name="newNode" type="Node"/>
<var name="attrName" type="DOMString"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<createElementNS var="element" obj="doc" namespaceURI='"http://www.w3.org/DOM/Test"' qualifiedName='"root"'/>
<createAttributeNS var="attribute1" obj="doc" namespaceURI='"http://www.w3.org/DOM/L1"' qualifiedName='"L1:att"'/>
<attributes var="attributes" obj="element"/>
<setNamedItemNS var="newNode" obj="attributes" arg="attribute1"/>
<getNamedItemNS var="attribute" obj="attributes" namespaceURI='"http://www.w3.org/DOM/L1"' localName='"att"'/>
<nodeName var="attrName" obj="attribute"/>
<assertEquals actual="attrName" expected='"L1:att"' id="namednodemapsetnameditemns02" ignoreCase="false"/>
</test>
