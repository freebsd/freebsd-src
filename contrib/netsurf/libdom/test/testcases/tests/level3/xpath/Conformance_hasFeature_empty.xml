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
<test xmlns="&level3;" name="Conformance_hasFeature_empty">
  <metadata>
    <title>Conformance_hasFeature_empty</title>
    <creator>Philippe Le Hégaret</creator>
    <contributor>Bob Clary</contributor>
    <description>
      1.3 Conformance - Test if 
      Document.implementation.hasFeature('XPath', "") returns true
    </description>
    <date qualifier="created">2003-11-29</date>
    <subject resource="&spec;#Interfaces"/>
  </metadata>

  <var name="doc"   type="Document"/>
  <var name="state" type="boolean"/>
  <var name="impl"  type='DOMImplementation'/>

  <load var="doc" href="staffNS" willBeModified="false"/>

  <implementation obj="doc" var="impl"/>

  <hasFeature obj="impl" 
              feature="&quot;xpATH&quot;" 
              version="&quot;&quot;" 
              var="state"/>

  <assertTrue actual="state" id="hasFeature-XPath-empty"/>

</test>
