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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns09">
<metadata>
<title>namednodemapsetnameditemns09</title>
<creator>IBM</creator>
<description>
	The method setNamedItemNS adds a node using its namespaceURI and localName and 
	raises a NO_MODIFICATION_ALLOWED_ERR if this map is readonly.
	
	Create a new attribute node and attempt to add it to the nodemap of entities and notations
	for this documenttype.  This should reaise a NO_MODIFICATION_ALLOWED_ERR.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-setNamedItemNS"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="docType" type="DocumentType"/>
<var name="entities" type="NamedNodeMap"/>
<var name="notations" type="NamedNodeMap"/>
<var name="attr" type="Attr"/>
<var name="newNode" type="Node"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<doctype var="docType" obj="doc"/>
<entities var="entities" obj="docType"/>
<notations var="notations" obj="docType"/>
<createAttributeNS var="attr" obj="doc" namespaceURI='"http://www.w3.org/DOM/Test"' qualifiedName='"test"'/>
<assertDOMException id="throw_NO_MODIFICATION_ALLOWED_ERR_entities">
<NO_MODIFICATION_ALLOWED_ERR>
<setNamedItemNS var="newNode" obj="entities" arg="attr"/>
</NO_MODIFICATION_ALLOWED_ERR>
</assertDOMException>
<assertDOMException id="throw_NO_MODIFICATION_ALLOWED_ERR_notations">
<NO_MODIFICATION_ALLOWED_ERR>
<setNamedItemNS var="newNode" obj="notations" arg="attr"/>
</NO_MODIFICATION_ALLOWED_ERR>
</assertDOMException>
</test>
