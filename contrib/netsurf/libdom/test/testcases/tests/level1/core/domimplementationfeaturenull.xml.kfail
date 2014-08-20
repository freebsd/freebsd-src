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

<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-1" name="domimplementationfeaturenull">
<metadata>
<title>domimplementationFeatureNull</title>
<creator>NIST</creator>
<description>
hasFeature("XML", null) should return true for implementations that can read staff documents.
</description>
<contributor>Curt Arnold</contributor>
<date qualifier="created">2001-08-23</date>
<subject resource="http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-5CED94D7"/>
<subject resource="http://www.w3.org/2000/11/DOM-Level-2-errata#core-14"/>
</metadata>
<implementationAttribute name="hasNullString" value="true"/>
<var name="doc" type="Document"/>
<var name="domImpl" type="DOMImplementation"/>
<var name="state" type="boolean"/>
<var name="nullVersion" type="DOMString" isNull="true"/>
<load var="doc" href="staff" willBeModified="false"/>
<implementation obj="doc" var="domImpl"/>
<hasFeature obj="domImpl" var="state" feature='"XML"' version="nullVersion"/>
<assertTrue actual="state" id="hasXMLnull"/>
</test>
