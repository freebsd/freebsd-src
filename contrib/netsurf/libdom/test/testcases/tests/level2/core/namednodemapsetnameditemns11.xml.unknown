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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns11">
<metadata>
<title>namednodemapsetnameditemns11</title>
<creator>IBM</creator>
<description>
    The method setNamedItemNS adds a node using its namespaceURI and localName and 
    raises a HIERARCHY_REQUEST_ERR if an attempt is made to add a node doesn't belong 
    in this NamedNodeMap.

	 Attempt to add a notation node to a NamedNodeMap of attribute nodes,
	 Since notations nodes do not belong in the attribute node map a HIERARCHY_REQUEST_ERR
	 should be raised.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-setNamedItemNS"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=259"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="docType" type="DocumentType"/>
<var name="notations" type="NamedNodeMap"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="notation" type="Notation"/>
<var name="element" type="Element"/>
<var name="elementList" type="NodeList"/>
<var name="newNode" type="Node"/>
<var name="nullNS" type="DOMString" isNull="true"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<doctype var="docType" obj="doc"/>
<notations var="notations" obj="docType"/>
<assertNotNull actual="notations" id="notationsNotNull"/>
<getNamedItem var="notation" obj="notations" name='"notation1"'/>
<getElementsByTagNameNS var="elementList" obj="doc" namespaceURI='"http://www.nist.gov"' localName='"address"' interface="Document"/>
<item var="element" obj="elementList" index="0" interface="NodeList"/>
<attributes var="attributes" obj="element"/>
<assertDOMException id="throw_HIERARCHY_REQUEST_ERR">
<HIERARCHY_REQUEST_ERR>
<setNamedItemNS var="newNode" obj="attributes" arg="notation"/>
</HIERARCHY_REQUEST_ERR>
</assertDOMException>
</test>
