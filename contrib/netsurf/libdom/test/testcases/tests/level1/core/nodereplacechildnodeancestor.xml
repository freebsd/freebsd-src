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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="nodereplacechildnodeancestor">
<metadata>
<title>nodeReplaceChildNodeAncestor</title>
<creator>NIST</creator>
<description>
    The "replaceChild(newChild,oldChild)" method raises a 
    HIERARCHY_REQUEST_ERR DOMException if the node to put
    in is one of this node's ancestors.
    
    Retrieve the second employee and attempt to replace
    one of its children with an ancestor node(root node).
    An attempt to make such a replacement should raise the 
    desired exception.
</description>
<contributor>Mary Brady</contributor>
<date qualifier="created">2001-08-17</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#xpointer(id('ID-258A00AF')/constant[@name='HIERARCHY_REQUEST_ERR'])"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-785887307"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#xpointer(id('ID-785887307')/raises/exception[@name='DOMException']/descr/p[substring-before(.,':')='HIERARCHY_REQUEST_ERR'])"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-785887307"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=249"/>
</metadata>
<var name="doc" type="Document"/>
<var name="newChild" type="Node"/>
<var name="elementList" type="NodeList"/>
<var name="employeeNode" type="Node"/>
<var name="childList" type="NodeList"/>
<var name="oldChild" type="Node"/>
<var name="replacedNode" type="Node"/>
<load var="doc" href="staff" willBeModified="true"/>
<documentElement obj="doc" var="newChild"/>
<getElementsByTagName interface="Document" obj="doc" tagname="&quot;employee&quot;" var="elementList"/>
<item interface="NodeList" obj="elementList" index="1" var="employeeNode"/>
<childNodes obj="employeeNode" var="childList"/>
<item interface="NodeList" obj="childList" index="0" var="oldChild"/>
<assertDOMException id="throw_HIERARCHY_REQUEST_ERR">
<HIERARCHY_REQUEST_ERR>
<replaceChild var="replacedNode" obj="employeeNode" newChild="newChild" oldChild="oldChild"/>
</HIERARCHY_REQUEST_ERR>
</assertDOMException>
</test>
