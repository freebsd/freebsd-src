<?xml version="1.0" encoding="UTF-8"?><?xml-stylesheet href="test-to-html.xsl" type="text/xml"?>
<!--

Copyright (c) 2001 World Wide Web Consortium, 
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
<test xmlns="http://www.w3.org/2001/DOM-Test-Suite/Level-2" name="domimplementationfeaturecore">
	<metadata>
		<title>domimplementationFeaturecore</title>
		<creator>NIST</creator>
		<description>
    The "feature" parameter in the
   "hasFeature(feature,version)" method is the package name
   of the feature.  Legal values are XML and HTML and CORE.
   (Test for feature core, lower case)
   
   Retrieve the entire DOM document and invoke its 
   "getImplementation()" method.  This should create a
   DOMImplementation object whose "hasFeature(feature,
   version)" method is invoked with feature equal to "core".
   The method should return a boolean "true".
</description>
		<contributor>Mary Brady</contributor>
		<date qualifier="created">2001-08-17</date>
		<subject resource="http://www.w3.org/TR/DOM-Level-2-Core/core#ID-5CED94D7"/>
	</metadata>
	<var name="doc" type="Document"/>
	<var name="domImpl" type="DOMImplementation"/>
	<var name="state" type="boolean"/>
	<load var="doc" href="staff" willBeModified="false"/>
	<implementation obj="doc" var="domImpl"/>
	<hasFeature obj="domImpl" var="state" feature='"core"' version='"2.0"'/>
	<assertTrue actual="state" id="domimplementationFeaturecoreAssert"/>
</test>
