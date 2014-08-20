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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapgetnameditemns03">
<metadata>
<title>namednodemapgetnameditemns03</title>
<creator>IBM</creator>
<description>
    The method getNamedItemNS retrieves a node specified by local name and namespace URI. 
  
    Create a new Element node and add 2 new attribute nodes having the same local name but different
    namespace names and namespace prefixes to it.  Using the getNamedItemNS retreive the second attribute node.  
    Verify if the attr node has been retreived successfully by checking its nodeName atttribute.
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
<var name="newAttr1" type="Attr"/>
<var name="newAttr2" type="Attr"/>
<var name="newAttribute" type="Attr"/>
<var name="attrName" type="DOMString"/>
<load var="doc" href="staffNS" willBeModified="false"/>
<createElementNS var="element" obj="doc" namespaceURI='"http://www.w3.org/DOM/Test"' qualifiedName='"root"'/>
<createAttributeNS var="newAttr1" obj="doc" namespaceURI='"http://www.w3.org/DOM/L1"' qualifiedName='"L1:att"'/>
<setAttributeNodeNS var="newAttribute" obj="element" newAttr="newAttr1"/>
<createAttributeNS var="newAttr2" obj="doc" namespaceURI='"http://www.w3.org/DOM/L2"' qualifiedName='"L2:att"'/>
<setAttributeNodeNS var="newAttribute" obj="element" newAttr="newAttr2"/>
<attributes var="attributes" obj="element"/>
<getNamedItemNS var="attribute" obj="attributes" namespaceURI='"http://www.w3.org/DOM/L2"' localName='"att"'/>
<nodeName var="attrName" obj="attribute"/>
<assertEquals actual="attrName" expected='"L2:att"' id="namednodemapgetnameditemns03"  ignoreCase="false"/>
</test>
