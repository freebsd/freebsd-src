<?xml version="1.0"?>
<!-- $FreeBSD$ -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                xmlns:db="http://docbook.org/ns/docbook"
                exclude-result-prefixes="db">

  <xsl:param name="release.url"/>
  <xsl:param name="release.branch"/>
  <xsl:param name="release.maillist"/>

  <xsl:template name="user.footer.content">
    <p align="center"><small>This file, and other release-related documents,
      can be downloaded from <a href="{$release.url}"><xsl:value-of select="$release.url"/></a>.</small></p>

    <p align="center"><small>For questions about FreeBSD, read the
      <a href="http://www.FreeBSD.org/docs.html">documentation</a> before
      contacting &lt;<a href="mailto:questions@FreeBSD.org">questions@FreeBSD.org</a>&gt;.</small></p>

    <p align="center"><small>All users of FreeBSD <xsl:value-of select="$release.branch"/> should
      subscribe to the &lt;<a href="mailto:{$release.maillist}@FreeBSD.org"><xsl:value-of select="$release.maillist"/>@FreeBSD.org</a>&gt;
      mailing list.</small></p>
  
    <p align="center"><small>For questions about this documentation,
      e-mail &lt;<a href="mailto:doc@FreeBSD.org">doc@FreeBSD.org</a>&gt;.</small></p>
  </xsl:template>
</xsl:stylesheet>
