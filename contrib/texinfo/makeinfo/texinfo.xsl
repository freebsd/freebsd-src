<?xml version='1.0'?>
<!-- $Id: texinfo.xsl,v 1.1 2002/08/25 23:38:39 karl Exp $ -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:output method="html" indent="yes"/>

<!-- root rule -->
<xsl:template match="/">
   <html>
    <head><title>
     <xsl:apply-templates select="TEXINFO/SETTITLE" mode="head"/>
    </title></head>
     <body bgcolor="#FFFFFF"><xsl:apply-templates/>
</body></html>
</xsl:template>


<xsl:template match="TEXINFO">
  <xsl:apply-templates/>
</xsl:template>


<xsl:template match="TEXINFO/SETFILENAME">
</xsl:template>

<xsl:template match="TEXINFO/SETTITLE" mode="head">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="TEXINFO/SETTITLE">
  <h1><xsl:apply-templates/></h1>
</xsl:template>


<xsl:template match="TEXINFO/DIRCATEGORY">
</xsl:template>

<xsl:template match="//PARA">
  <p><xsl:apply-templates/></p>
</xsl:template>

<xsl:template match="//EMPH">
  <i><xsl:apply-templates/></i>
</xsl:template>

<!-- The node -->
<xsl:template match="TEXINFO/NODE">
 <hr/>
 <p>
 <xsl:apply-templates select="NODENAME" mode="select"/>
 <xsl:apply-templates select="NODEPREV" mode="select"/>
 <xsl:apply-templates select="NODEUP" mode="select"/>
 <xsl:apply-templates select="NODENEXT" mode="select"/>
 <xsl:apply-templates/>
  <h2>Footnotes</h2>
  <ol>
  <xsl:apply-templates select=".//FOOTNOTE" mode="footnote"/>
   </ol>
 </p>
</xsl:template>

<xsl:template match="TEXINFO/NODE/NODENAME" mode="select">
<h2>
 <a>
 <xsl:attribute name="name">
  <xsl:apply-templates/>
 </xsl:attribute>
 <xsl:apply-templates/>
 </a>
</h2>
</xsl:template>

<xsl:template match="TEXINFO/NODE/NODENAME"/>


<xsl:template match="TEXINFO/NODE/NODEPREV" mode="select">
 [ <b>Previous: </b>
 <a>
 <xsl:attribute name="href">
  <xsl:text>#</xsl:text>
  <xsl:apply-templates/>
 </xsl:attribute>
 <xsl:apply-templates/>
 </a> ]
</xsl:template>

<xsl:template match="TEXINFO/NODE/NODEPREV"/>
	
<xsl:template match="TEXINFO/NODE/NODEUP" mode="select">
 [ <b>Up: </b>
 <a>
 <xsl:attribute name="href">
  <xsl:text>#</xsl:text>
  <xsl:apply-templates/>
 </xsl:attribute>
 <xsl:apply-templates/>
 </a> ]
</xsl:template>

<xsl:template match="TEXINFO/NODE/NODEUP"/>

<xsl:template match="TEXINFO/NODE/NODENEXT" mode="select">
 [ <b>Next: </b>
 <a>
 <xsl:attribute name="href">
  <xsl:text>#</xsl:text>
  <xsl:apply-templates/>
 </xsl:attribute>
 <xsl:apply-templates/>
 </a> ]
</xsl:template>

<xsl:template match="TEXINFO/NODE/NODENEXT"/>

<!-- Menu -->
<xsl:template match="//MENU">
 <h3>Menu</h3>
 <xsl:apply-templates/>
</xsl:template> 

<xsl:template match="//MENU/MENUENTRY">
 <a>
 <xsl:attribute name="href">
  <xsl:text>#</xsl:text>
  <xsl:apply-templates select="MENUNODE"/>
 </xsl:attribute>
 <xsl:apply-templates select="MENUTITLE"/>
 </a>: 
 <xsl:apply-templates select="MENUCOMMENT"/>
 <br/>
</xsl:template>

<xsl:template match="//MENU/MENUENTRY/MENUNODE">
 <xsl:apply-templates/>
</xsl:template>

<xsl:template match="//MENU/MENUENTRY/MENUTITLE">
 <xsl:apply-templates/>
</xsl:template>

<xsl:template match="//MENU/MENUENTRY/MENUCOMMENT">
 <xsl:apply-templates mode="menucomment"/>
</xsl:template>

<xsl:template match="PARA" mode="menucomment">
 <xsl:apply-templates/>
</xsl:template>

<xsl:template match="//PARA">
 <p><xsl:apply-templates/></p>
</xsl:template>

<!-- LISTS -->
<xsl:template match="//ITEMIZE">
 <ul>
  <xsl:apply-templates/>
 </ul>
</xsl:template>

<xsl:template match="//ITEMIZE/ITEM">
 <li>
  <xsl:apply-templates/>
 </li>
</xsl:template>

<xsl:template match="//ENUMERATE">
 <ol>
  <xsl:apply-templates/>
 </ol>
</xsl:template>

<xsl:template match="//ENUMERATE/ITEM">
 <li>
  <xsl:apply-templates/>
 </li>
</xsl:template>

<!-- INLINE -->
<xsl:template match="//CODE">
 <tt>
  <xsl:apply-templates/>
 </tt>
</xsl:template>

<xsl:template match="//DFN">
 <i><b>
  <xsl:apply-templates/>
 </b></i>
</xsl:template>

<xsl:template match="//STRONG">
 <b>
  <xsl:apply-templates/>
 </b>
</xsl:template>

<xsl:template match="//CENTER">
 <center>
  <xsl:apply-templates/>
 </center>
</xsl:template>

<xsl:template match="//VAR">
 <i>
  <xsl:apply-templates/>
 </i>
</xsl:template>

<xsl:template match="//KBD">
 <tt>
  <xsl:apply-templates/>
 </tt>
</xsl:template>

<xsl:template match="//KEY">
 <b>
  <xsl:apply-templates/>
 </b>
</xsl:template>

<!-- BLOCKS -->
<xsl:template match="//DISPLAY">
 <pre>
  <xsl:apply-templates/>
 </pre>
</xsl:template>


<!-- INDEX -->
<xsl:template match="//INDEXTERM">
</xsl:template>

<!-- FOOTNOTE -->
<xsl:template match="//FOOTNOTE">
</xsl:template>

<xsl:template match="//FOOTNOTE" mode="footnote">
 <li><xsl:apply-templates/></li>
</xsl:template>

</xsl:stylesheet>
