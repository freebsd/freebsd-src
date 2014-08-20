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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns06">
<metadata>
<title>namednodemapsetnameditemns06</title>
<creator>IBM</creator>
<description>
	Retreieve the first element whose localName is address and its attributes into a named node map.
	Retreiving the domestic attribute from the namednodemap.  
	Retreieve the second element whose localName is address and its attributes into a named node map.
	Invoke setNamedItemNS on the second NamedNodeMap specifying the first domestic attribute from
	the first map.  This should raise an INUSE_ATTRIBIUTE_ERR.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-setNamedItemNS"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="elementList" type="NodeList"/>
<var name="element" type="Element"/>
<var name="attr" type="Attr"/>
<var name="newNode" type="Node"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<getElementsByTagNameNS var="elementList" obj="doc" namespaceURI='"*"' localName='"address"'  interface="Document"/>
<item var="element" obj="elementList" index="0"  interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<getNamedItemNS var="attr" obj="attributes" namespaceURI='"http://www.usa.com"' localName='"domestic"'/>
<item var="element" obj="elementList" index="1" interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<assertDOMException id="namednodemapsetnameditemns06">
<INUSE_ATTRIBUTE_ERR>
<setNamedItemNS var="newNode" obj="attributes" arg="attr"/>
</INUSE_ATTRIBUTE_ERR>
</assertDOMException>
</test>

