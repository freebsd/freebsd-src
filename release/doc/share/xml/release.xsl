<?xml version="1.0"?>
<!-- $FreeBSD$ -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                xmlns:db="http://docbook.org/ns/docbook"
                exclude-result-prefixes="db">

  <xsl:import href="http://www.FreeBSD.org/XML/share/xml/freebsd-xhtml.xsl"/>

  <xsl:import href="http://www.FreeBSD.org/release/XML/lang/share/xml/release.xsl"/>

  <xsl:param name="release.url"/>
  <xsl:param name="release.branch"/>

  <xsl:template name="paragraph">
    <xsl:param name="class" select="''"/>
    <xsl:param name="content"/>

    <xsl:variable name="p">
      <p>
	<xsl:choose>
          <xsl:when test="$class != ''">
            <xsl:call-template name="common.html.attributes">
              <xsl:with-param name="class" select="$class"/>
             </xsl:call-template>
           </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="locale.html.attributes"/>
           </xsl:otherwise>
	 </xsl:choose>
	<xsl:if test="@arch">
	  <xsl:value-of select="concat('[', @arch, ']')"/>
	  <xsl:value-of select='" "'/>
	</xsl:if>
	<xsl:copy-of select="$content"/>
	<xsl:value-of select='" "'/>
	<xsl:if test="@revision">
	  <xsl:element name="a">
	    <xsl:attribute name="href">
	      <xsl:value-of select="concat('http://svn.freebsd.org/viewvc/base?view=revision&#38;revision=', @revision)"/>
	    </xsl:attribute>
	    <xsl:value-of select="concat('[r', @revision, ']')"/>
	  </xsl:element>
	</xsl:if>
       </p>
     </xsl:variable>

    <xsl:choose>
      <xsl:when test="$html.cleanup != 0">
	<xsl:call-template name="unwrap.p">
          <xsl:with-param name="p" select="$p"/>
	 </xsl:call-template>
       </xsl:when>
      <xsl:otherwise>
	<xsl:copy-of select="$p"/>
       </xsl:otherwise>
     </xsl:choose>
   </xsl:template>
</xsl:stylesheet>
