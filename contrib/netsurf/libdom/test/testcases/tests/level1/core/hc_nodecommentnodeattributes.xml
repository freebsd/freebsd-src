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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="hc_nodecommentnodeattributes">
<metadata>
<title>hc_nodeCommentNodeAttributes</title>
<creator>Curt Arnold</creator>
<description>
    The "getAttributes()" method invoked on a Comment 
    Node returns null.

    Find any comment that is an immediate child of the root
    and assert that Node.attributes is null.  Then create
    a new comment node (in case they had been omitted) and
    make the assertion.    
</description>

<date qualifier="created">2002-06-09</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-84CF096"/>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-1728279322"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=248"/>
<subject resource="http://www.w3.org/Bugs/Public/show_bug.cgi?id=263"/>
</metadata>
<var name="doc" type="Document"/>
<var name="commentNode" type="Node"/>
<var name="nodeList" type="NodeList"/>
<var name="attrList" type="NamedNodeMap"/>
<var name="nodeType" type="int"/>
<load var="doc" href="hc_staff" willBeModified="false"/>
<childNodes obj="doc" var="nodeList"/>
<for-each collection="nodeList" member="commentNode">
<nodeType obj="commentNode" var="nodeType"/>
<if>
<equals actual="nodeType" expected="8" ignoreCase="false"/>
<attributes obj="commentNode" var="attrList"/>
<assertNull actual="attrList" id="existingCommentAttributesNull"/>
</if>
</for-each>
<createComment var="commentNode" obj="doc" data='"This is a comment"'/>
<attributes obj="commentNode" var="attrList"/>
<assertNull actual="attrList" id="createdCommentAttributesNull"/>
</test>
