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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapgetnameditemns02">
<metadata>
<title>namednodemapgetnameditemns02</title>
<creator>IBM</creator>
<description>
	The method getNamedItemNS retrieves a node specified by local name and namespace URI. 
	
	Using the method getNamedItemNS, retreive an attribute node having namespaceURI=http://www.nist.gov
	and localName=domestic, from a NamedNodeMap of attribute nodes, for the second element 
	whose namespaceURI=http://www.nist.gov and localName=address.  Verify if the attr node 
	has been retreived successfully by checking its nodeName atttribute.
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
<var name="attrName" type="DOMString"/>
<load var="doc" href="staffNS" willBeModified="false"/>
<getElementsByTagNameNS var="elementList" obj="doc" namespaceURI='"http://www.nist.gov"' localName='"address"' interface="Document"/>
<item var="element" obj="elementList" index="1" interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<getNamedItemNS var="attribute" obj="attributes" namespaceURI='"http://www.nist.gov"' localName='"domestic"'/>
<nodeName var="attrName" obj="attribute"/>
<assertEquals actual="attrName" expected='"emp:domestic"' id="namednodemapgetnameditemns02"  ignoreCase="false"/>
</test>
