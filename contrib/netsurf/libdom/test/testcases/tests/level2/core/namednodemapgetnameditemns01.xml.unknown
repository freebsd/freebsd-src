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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapgetnameditemns01">
<metadata>
<title>namednodemapgetnameditemns01</title>
<creator>IBM</creator>
<description>
	Using the method getNamedItemNS, retreive the entity "ent1" and notation "notation1" 
	from a NamedNodeMap of this DocumentTypes entities and notations.
	Both should be null since entities and notations are not namespaced.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-getNamedItemNS"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=259"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=407"/>
<subject resource="http://lists.w3.org/Archives/Member/w3c-dom-ig/2003Nov/0016.html"/>
</metadata>
<implementationAttribute name="namespaceAware" value="true"/>
<var name="doc" type="Document"/>
<var name="docType" type="DocumentType"/>
<var name="entities" type="NamedNodeMap"/>
<var name="notations" type="NamedNodeMap"/>
<var name="entity" type="Entity"/>
<var name="notation" type="Notation"/>
<var name="entityName" type="DOMString"/>
<var name="notationName" type="DOMString"/>
<var name="nullNS" type="DOMString" isNull="true"/>
<load var="doc" href="staffNS" willBeModified="false"/>
<doctype var="docType" obj="doc"/>
<entities var="entities" obj="docType"/>
<assertNotNull actual="entities" id="entitiesNotNull"/>
<notations var="notations" obj="docType"/>
<assertNotNull actual="notations" id="notationsNotNull"/>
<getNamedItemNS var="entity" obj="entities" namespaceURI="nullNS" localName='"ent1"'/>
<assertNull actual="entity" id="entityNull"/>
<getNamedItemNS var="notation" obj="notations" namespaceURI="nullNS" localName='"notation1"'/>
<assertNull actual="notation" id="notationNull"/>
</test>
