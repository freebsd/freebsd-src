<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2001-2003 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
--><!DOCTYPE test SYSTEM "dom1.dtd">

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="namednodemapwrongdocumenterr">
<metadata>
<title>namednodemapWrongDocumentErr</title>
<creator>NIST</creator>
<description>
    The "setNamedItem(arg)" method raises a 
   WRONG_DOCUMENT_ERR DOMException if "arg" was created
   from a different document than the one that created
   the NamedNodeMap. 
   
   Create a NamedNodeMap object from the attributes of the
   last child of the third employee and attempt to add
   another Attr node to it that was created from a 
   different DOM document.  This should raise the desired
   exception.  This method uses the "createAttribute(name)"
   method from the Document interface.
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#xpointer(id('ID-258A00AF')/constant[@name='WRONG_DOCUMENT_ERR'])"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-1025163788"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#xpointer(id('ID-1025163788')/raises/exception[@name='DOMException']/descr/p[substring-before(.,':')='WRONG_DOCUMENT_ERR'])"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=249"/>
</metadata>
<var name="doc1" type="Document"/>
<var name="doc2" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="testAddress" type="Node"/>
<var name="attributes" type="NamedNodeMap"/>
<var name="newAttribute" type="Node"/>
<var name="setNode" type="Node"/>
<load var="doc1" href="staff" willBeModified="true"/>
<load var="doc2" href="staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc1" var="elementList" tagname="&quot;address&quot;"/>
<item interface="NodeList" obj="elementList" var="testAddress" index="2"/>
<createAttribute obj="doc2" var="newAttribute" name="&quot;newAttribute&quot;"/>
<attributes obj="testAddress" var="attributes"/>
<assertDOMException id="throw_WRONG_DOCUMENT_ERR">
<WRONG_DOCUMENT_ERR>
<setNamedItem var="setNode" obj="attributes" arg="newAttribute"/>
</WRONG_DOCUMENT_ERR>
</assertDOMException>
</test>
