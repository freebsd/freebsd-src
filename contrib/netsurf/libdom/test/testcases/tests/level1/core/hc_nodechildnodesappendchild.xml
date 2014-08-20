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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_nodechildnodesappendchild">
<metadata>
<title>hc_nodeChildNodesAppendChild</title>
<creator>Curt Arnold</creator>
<description>
    The NodeList returned by the "getChildNodes()" method
    is live.   Changes on the node's children are immediately
    reflected on the nodes returned in the NodeList.
    
    Create a NodeList of the children of the second employee
    and then add a newly created element that was created
    by the "createElement()" method(Document Interface) to
    the second employee by using the "appendChild()" method.
    The length of the NodeList should reflect this new
    addition to the child list.   It should return the value 14.
</description>

<date qualifier="created">2002-06-09</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-1451460987"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-184E7107"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=246"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=247"/>
</metadata>
<var name="doc" type="Document"/>
<var name="elementList" type="NodeList"/>
<var name="employeeNode" type="Node"/>
<var name="childList" type="NodeList"/>
<var name="createdNode" type="Node"/>
<var name="childNode" type="Node"/>
<var name="childName" type="DOMString"/>
<var name="childType" type="int"/>
<var name="textNode" type="Node"/>
<var name="actual" type="List"/>
<var name="expected" type="List">
<member>"em"</member>
<member>"strong"</member>
<member>"code"</member>
<member>"sup"</member>
<member>"var"</member>
<member>"acronym"</member>
<member>"br"</member>
</var>
<load var="doc" href="hc_staff" willBeModified="true"/>
<getElementsByTagName interface="Document" obj="doc" tagname='"p"' var="elementList"/>
<item interface="NodeList" obj="elementList" index="1" var="employeeNode"/>
<childNodes obj="employeeNode" var="childList"/>
<createElement obj="doc" var="createdNode" tagName='"br"'/>
<appendChild obj="employeeNode" newChild="createdNode" var="employeeNode"/>
<for-each collection="childList" member="childNode">
    <nodeName var="childName" obj="childNode"/>
    <nodeType var="childType" obj="childNode"/>
    <if><equals actual="childType" expected="1"/>
        <append collection="actual" item="childName"/>
        <else>
            <assertEquals id="textNodeType" actual="childType" expected="3" ignoreCase="false"/>
        </else>
    </if>
</for-each> 
<assertEquals actual="actual" expected="expected" id="childElements" ignoreCase="auto"/>
</test>
