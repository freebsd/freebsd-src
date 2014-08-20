<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2001-2004 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
--><!DOCTYPE test SYSTEM "dom1.dtd">

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="nodeentityreferencenodevalue">
<metadata>
<title>nodeEntityReferenceNodeValue</title>
<creator>NIST</creator>
<description>
    The string returned by the "getNodeValue()" method for an 
    EntityReference Node is null.
    
    Retrieve the first Entity Reference node from the last
    child of the second employee and check the string 
    returned by the "getNodeValue()" method.   It should be 
    equal to null.
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-F68D080"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="entRefAddr" type="Element"/>
<var name="entRefNode" type="Node"/>
<var name="entRefValue" type="DOMString"/>
<var name="nodeType" type="int"/>
<load var="doc" href="staff" willBeModified="false"/>
<getElementsByTagName interface="Document" obj="doc" 
	var="elementList" tagname='"address"'/>
<item interface="NodeList" obj="elementList" index="1" var="entRefAddr"/>
<firstChild interface="Node" obj="entRefAddr" var="entRefNode"/>
<nodeType var="nodeType" obj="entRefNode"/>
<if><equals actual="nodeType" expected="3" ignoreCase="false"/>
	<createEntityReference var="entRefNode" obj="doc" name='"ent2"'/>
	<assertNotNull actual="entRefNode" id="createdEntRefNotNull"/>
</if>
<nodeValue obj="entRefNode" var="entRefValue"/>
<assertNull actual="entRefValue" id="entRefNodeValue"/>
</test>
