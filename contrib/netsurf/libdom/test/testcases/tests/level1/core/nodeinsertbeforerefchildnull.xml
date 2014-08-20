<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2001 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
--><!DOCTYPE test SYSTEM "dom1.dtd">

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="nodeinsertbeforerefchildnull">
<metadata>
<title>nodeInsertBeforeRefChildNull</title>
<creator>NIST</creator>
<description>
    If the "refChild" is null then the
    "insertBefore(newChild,refChild)" method inserts the
    node "newChild" at the end of the list of children. 
    
    Retrieve the second employee and invoke the
    "insertBefore(newChild,refChild)" method with
    refChild=null.   Since "refChild" is null the "newChild"
    should be added to the end of the list.   The last item
    in the list is checked after insertion.   The last Element
    node of the list should be "newChild".
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-952280727"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="employeeNode" type="Node"/>
<var name="childList" type="NodeList"/>
<var name="refChild" type="Node" isNull="true"/>
<var name="newChild" type="Node"/>
<var name="child" type="Node"/>
<var name="childName" type="DOMString"/>
<var name="insertedNode" type="Node"/>
<load var="doc" href="staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc" tagname="&quot;employee&quot;" var="elementList"/>
<item interface="NodeList" obj="elementList" index="1" var="employeeNode"/>
<childNodes obj="employeeNode" var="childList"/>
<createElement obj="doc" tagName="&quot;newChild&quot;" var="newChild"/>
<insertBefore var="insertedNode" obj="employeeNode" newChild="newChild" refChild="refChild"/>
<lastChild interface="Node" obj="employeeNode" var="child"/>
<nodeName obj="child" var="childName"/>
<assertEquals actual="childName" expected="&quot;newChild&quot;" id="nodeInsertBeforeRefChildNullAssert1" ignoreCase="false"/>
</test>
