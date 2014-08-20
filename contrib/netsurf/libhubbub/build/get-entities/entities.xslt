<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method='text' version='1.0' encoding='UTF-8' indent='yes' />
<xsl:preserve-space elements="t" />

<xsl:template match="/table">
  <xsl:for-each select="tr">{ "<xsl:value-of select="td[1]/code" />", 0x<xsl:value-of select="substring(td[2],4)" />},
</xsl:for-each>
</xsl:template>

</xsl:stylesheet>
