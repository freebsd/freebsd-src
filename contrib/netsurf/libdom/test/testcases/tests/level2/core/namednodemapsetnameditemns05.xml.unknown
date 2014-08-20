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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="namednodemapsetnameditemns05">
<metadata>
<title>namednodemapsetnameditemns05</title>
<creator>IBM</creator>
<description>
	Retreive an entity and notation node and add the first notation to the 
	notation node map and first entity node to the entity map.  Since both these
	maps are for readonly node, a NO_MODIFICATION_ALLOWED_ERR should be raised.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-setNamedItemNS"/>
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
<var name="newNode" type="Node"/>
<var name="nullNS" type="DOMString" isNull="true"/>
<load var="doc" href="staffNS" willBeModified="true"/>
<doctype var="docType" obj="doc"/>
<entities var="entities" obj="docType"/>
<assertNotNull actual="entities" id="entitiesNotNull"/>
<notations var="notations" obj="docType"/>
<assertNotNull actual="notations" id="notationsNotNull"/>
<getNamedItem var="entity" obj="entities" name='"ent1"'/>
<getNamedItem var="notation" obj="notations" name='"notation1"'/>
<assertDOMException id="throw_NO_MODIFICATION_ALLOWED_ERR_entities">
<NO_MODIFICATION_ALLOWED_ERR>
<setNamedItemNS var="newNode" obj="entities" arg="entity"/>
</NO_MODIFICATION_ALLOWED_ERR>
</assertDOMException>
<assertDOMException id="throw_NO_MODIFICATION_ALLOWED_ERR_notations">
<NO_MODIFICATION_ALLOWED_ERR>
<setNamedItemNS var="newNode" obj="notations" arg="notation"/>
</NO_MODIFICATION_ALLOWED_ERR>
</assertDOMException>
</test>
