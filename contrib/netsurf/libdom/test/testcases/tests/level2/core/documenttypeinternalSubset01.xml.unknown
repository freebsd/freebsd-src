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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="documenttypeinternalSubset01">
<metadata>
<title>documenttypeinternalSubset01</title>
<creator>IBM</creator>
<description>
    The method getInternalSubset() returns the internal subset as a string. 
  
    Create a new DocumentType node with null values for publicId and systemId.
    Verify that its internal subset is null.
</description>
<contributor>Neil Delima</contributor>
<date qualifier="created">2002-04-28</date>
<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-Core-DocType-internalSubset"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=259"/>
</metadata>
<var name="doc" type="Document"/>
<var name="docType" type="DocumentType"/>
<var name="domImpl" type="DOMImplementation"/>
<var name="internal" type="DOMString"/>
<var name="nullNS" type="DOMString" isNull="true"/>
<load var="doc" href="staffNS" willBeModified="false"/>
<implementation var="domImpl" obj="doc"/>
<createDocumentType var="docType" obj="domImpl" qualifiedName='"l2:root"' publicId="nullNS" systemId="nullNS" />
<internalSubset var="internal" obj="docType"/>
<assertNull actual="internal" id="internalSubsetNull"/>
</test>
