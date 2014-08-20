<?xml version="1.0" encoding="iso-8859-1"?>
<?xml-stylesheet href="test-to-html.xsl" type="text/xml"?>
<!--
 Copyright (c) 2003 World Wide Web Consortium,

 (Massachusetts Institute of Technology, European Research Consortium for
 Informatics and Mathematics, Keio University). All Rights Reserved. This
 work is distributed under the W3C(r) Software License [1] in the hope that
 it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 [1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
-->

<!DOCTYPE test SYSTEM "dom3.dtd" [
  <!ENTITY % entities SYSTEM "dom3xpathents.ent">
  %entities;
]>
<test xmlns="&level3;" name="Conformance_isSupported_null">
  <metadata>
    <title>Conformance_isSupported_null</title>
    <creator>Philippe Le Hégaret</creator>
    <contributor>Bob Clary</contributor>
    <description>
      1.3 Conformance - Test if 
      Document.isSupported('XPath', null) returns true
    </description>
    <date qualifier="created">2003-11-29</date>
    <subject resource="&spec;#Conformance"/>
  </metadata>

  <var name="doc"       type="Document"/>
  <var name="state"     type="boolean"/>
  <var name="nullValue" type="DOMString" isNull="true"/>

  <load var="doc" href="staffNS" willBeModified="false"/>

  <isSupported obj="doc" 
               feature="&quot;xpATH&quot;"
               version="nullValue" 
               var="state"/>

  <assertTrue actual="state" id="isSupported-XPath-null"/>
  
</test>
