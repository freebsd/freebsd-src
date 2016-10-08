<?xml version="1.0" encoding="utf-8"?>
<!--
	Sccsid @(#)odt2tr.xsl	1.14 (gritter) 10/10/06

	A simplistic OpenDocument to troff converter in form of
	an XSLT stylesheet. See the usage instructions below.
-->

<!--
	Copyright (c) 2006 Gunnar Ritter
	
	This software is provided 'as-is', without any express or
	implied warranty. In no event will the authors be held liable
	for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for
	any purpose, including commercial applications, and to
	alter it and redistribute it freely, subject to the following
	restrictions:

	1. The origin of this software must not be misrepresented; you
	   must not claim that you wrote the original software. If you
	   use this software in a product, an acknowledgment in the
	   product documentation would be appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and
	   must not be misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	   distribution.
-->

<!--
	The main purpose of this stylesheet is to extract texts
	from office documents for typesetting, a typical task
	when producing a book: Most authors use word processors
	and hand in (be it proprietary or open format) office
	documents. When converting them to troff, some formatting
	elements (italics, headings, footnotes, etc.) should be
	preserved, although the general layout is a decision of
	the typesetter. The stylesheet thus makes an attempt to
	save the former but discards the latter. This is not
	always possible - an office document is organized as a
	set of "styles" which have only restricted significance
	as generic markup elements. Since the styles are typically
	useless for typesetting, they are discarded, but some
	of their markup information is directly included in the
	generated troff document.

	Since OpenOffice can read many word processor formats,
	the first conversion step usually is to open the author's
	document in OpenOffice and to save it as OpenDocument text
	(.odt).

	Now use cpio or unzip to extract the .odt file (which is
	actually a zip archive). This will produce, amongst others,
	a file named content.xml. Then (using xsltproc from libxml2,
	or perhaps another XSLT processor), execute:

		xsltproc odt2tr.xsl content.xml >converted.tr

	No macro set is supplied with the generated troff document,
	but the generated macros are roughly compatible with -mm,
	which makes it possible to obtain a preview by

		tbl converted.tr | troff -mm | dpost >converted.ps

	It is normally necessary (and part of the process for which
	the stylesheet is intended) to further edit the output.

	The generated troff document contains the following markup:

	\fX		The following fonts are expected: R, I,
			B, BI, and SC (for small capitals). All
			other font information is discarded.

	.H		A heading of with four arguments: The first
			argument is the heading level (1, 2, 3, ...);
			the second argument contains the heading text.
			The third argument contains markup for the
			heading, and the third contains markup reset
			instructions. These are separated from the
			text to ease the creation of table of contents
			entries.

	.P		A paragraph. Actually, almost any <text:p>
			tag results in a .P call, although <text:p>
			may also have been generated in place of a
			heading. There is no way to automatically
			distinguish between them, so some handwork
			is to be expected.

	.IP		An indented paragraph. The indent amount is
			added as a comment but is not passed. This
			is because it is the typesetter's, not the
			author's, decision.

			Temporary indents result in a comment after
			a .P or .IP call but there is no code emitted
			for them.

	.br		A hard line break.

	line break	A line break is generated instead of a space
			following a "." or another sentence-ending
			character under most circumstances. Since
			there is no markup for such characters in the
			original document, some handwork is to be
			expected. This issue can be ignored if troff
			is configured not to insert an additional
			space following a sentence-ending character.

	.ML		Begin a marked list. The single argument is
			the mark character.

	.AL		Begin a numbered list.

	.LE		End a list (bulleted or numbered).

	.LI		Start a list item.

	.LP		Paragraph (any <text:p> tag) inside a list.

	\*F		The numbering for the next footnote.

	.FS/.FE		Footnote text start/end macros.

	.TS/.TE/T{/T}	Table structure for tbl. All table markup is
			discarded; all columns are left-aligned in
			the generated document. It is expected that
			the typesetter will apply the exact format
			by hand.

	Of course, it is easy to modify the stylesheet if other macro
	names are more suitable.
-->
<stylesheet
	xmlns="http://www.w3.org/1999/XSL/Transform"
	xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
	xmlns:date="http://exslt.org/dates-and-times"
	xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
	xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
	xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
	xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
	version="1.0">

<output method="text" omit-xml-declaration="yes" encoding="utf-8"/>

<strip-space elements="*"/>
<preserve-space elements="text:h text:p text:span"/>

<template name="println">
  <param name="t"/>
  <if test='starts-with($t, ".") or starts-with($t, "&apos;")'>\&amp;</if>
  <value-of select="$t"/>
</template>

<template name="fold">
  <param name="t"/>
  <choose>
    <when test="contains($t, '&#10;')">
      <call-template name="fold">
        <with-param name="t" select="substring-before($t, '&#10;')"/>
      </call-template>
      <text>&#10;</text>
      <call-template name="fold">
        <with-param name="t" select="substring-after($t, '&#10;')"/>
      </call-template>
    </when>
    <otherwise>
      <choose>
        <when test="string-length($t) > 66">
          <variable name="p" select="substring($t, 1, 50)"/>
          <variable name="s" select="substring($t, 51)"/>
          <choose>
            <when test="contains($s, ' ')">
              <variable name="m" select="substring-before($s, ' ')"/>
              <value-of select="$p"/><value-of select="$m"/><text>&#10;</text>
              <call-template name="fold">
                <with-param name="t" select="substring-after($s, ' ')"/>
              </call-template>
            </when>
            <otherwise>
              <call-template name="println">
                <with-param name="t" select="$t"/>
              </call-template>
            </otherwise>
          </choose>
        </when>
        <otherwise>
          <call-template name="println">
            <with-param name="t" select="$t"/>
          </call-template>
        </otherwise>
      </choose>
    </otherwise>
  </choose>
</template>

<template name="lastword">
  <param name="t"/>
  <variable name="u" select="translate($t, ' .', '  ')"/>
  <choose>
    <when test="contains($u, ' ')">
      <call-template name="lastword">
        <with-param name="t">
          <value-of select="substring-after($u, ' ')"/>
        </with-param>
      </call-template>
    </when>
    <otherwise>
      <value-of select="$t"/>
    </otherwise>
  </choose>
</template>

<template name="spconv">
  <param name="t"/>
  <variable name="u" select="translate($t, '.:!?', '....')"/>
  <choose>
    <when test="contains($u, '. ')">
      <variable name="p" select="substring-before($u, '. ')"/>
      <variable name="n" select="string-length($p)"/>
      <value-of select="substring($t, 1, $n)"/><value-of select="substring($t, $n+1, 1)"/>
      <choose>
        <when test="$n &lt; string-length($t) - 2">
          <variable name="w">
            <call-template name="lastword">
              <with-param name="t">
                <value-of select="$p"/>
              </with-param>
            </call-template>
          </variable>
          <choose>
          <when test="string-length($w) > 2">
            <text>&#10;</text>
          </when>
          <otherwise>
            <text> </text>
          </otherwise>
          </choose>
          <call-template name="spconv">
            <with-param name="t">
              <value-of select="substring($t, $n + 3)"/>
            </with-param>
          </call-template>
        </when>
        <otherwise>
          <text> </text>
        </otherwise>
      </choose>
    </when>
    <otherwise>
      <value-of select="$t"/>
    </otherwise>
  </choose>
</template>

<template name="dqconv">
  <param name="t"/>
  <choose>
    <when test="contains($t, '&quot;')">
        <value-of select="substring-before($t, '&quot;')"/>\(dq<call-template name="dqconv">
        <with-param name="t">
          <value-of select="substring-after($t, '&quot;')"/>
        </with-param>
      </call-template>
    </when>
    <otherwise>
      <value-of select="$t"/>
    </otherwise>
  </choose>
</template>

<template name="rsconv">
  <param name="t"/>
  <choose>
    <when test="contains($t, '\')">
      <call-template name="dqconv">
        <with-param name="t">
          <value-of select="substring-before($t, '\')"/>
        </with-param>
      </call-template>\(rs<call-template name="rsconv">
        <with-param name="t">
          <value-of select="substring-after($t, '\')"/>
        </with-param>
      </call-template>
    </when>
    <otherwise>
      <call-template name="dqconv">
        <with-param name="t">
          <value-of select="$t"/>
        </with-param>
      </call-template>
    </otherwise>
  </choose>
</template>

<template match="text()">
  <choose>
    <when test='starts-with(., ". ")'>
      <text>\&amp;</text>
      <value-of select="substring(., 1, 1)"/><text>&#10;</text>
      <call-template name="text">
        <with-param name="t" select="substring(., 3)"/>
      </call-template>
    </when>
    <otherwise>
      <call-template name="text">
        <with-param name="t" select="."/>
      </call-template>
    </otherwise>
  </choose>
</template>

<template name="text">
  <param name="t"/>
  <variable name="n" select="string-length($t)"/>
  <variable name="z" select="substring($t, $n)"/>
  <choose>
    <when test="$z = ' ' and $n > 8 and following-sibling::node() and
        (following::node()[position() = 1][self::text()] or
         following::node()[position() = 1][self::text:span])">
      <call-template name="foldtext">
        <with-param name="t" select="substring($t, 1, $n - 1)"/>
      </call-template>
      <text>&#10;</text>
    </when>
    <otherwise>
      <call-template name="foldtext">
        <with-param name="t" select="$t"/>
      </call-template>
    </otherwise>
  </choose>
</template>

<template name="foldtext">
  <param name="t"/>
  <call-template name="fold">
    <with-param name="t">
      <call-template name="spconv">
        <with-param name="t">
          <call-template name="rsconv">
            <with-param name="t">
              <value-of select="$t"/>
            </with-param>
          </call-template>
        </with-param>
      </call-template>
    </with-param>
  </call-template>
</template>

<template match="text:h//text()">
  <if test='starts-with(., ".") or starts-with(., "&apos;")'>\&amp;</if>
  <call-template name="rsconv">
    <with-param name="t">
      <value-of select="."/>
    </with-param>
  </call-template>
</template>

<template match="text:s"/>

<template match="text:tab[preceding-sibling::text() or
    following-sibling::text()]">
  <text>&#9;</text>
</template>

<template name="parastyle">
  <choose>
    <when test="number(translate(//style:style[@style:name =
        current()/@text:style-name]/style:paragraph-properties/@fo:margin-left,
        'cimnptx', '       ')) > 0">
      <text>.IP \" indent </text>
      <value-of select="//style:style[@style:name =
          current()/@text:style-name]/style:paragraph-properties/@fo:margin-left"/>
      <text>&#10;</text>
    </when>
    <otherwise>
      <text>.P&#10;</text>
    </otherwise>
  </choose>
  <choose>
    <when test="//style:style[@style:name =
        current()/@text:style-name]/style:paragraph-properties/@fo:text-indent">
      <text>.\" temporary indent </text>
      <value-of select="//style:style[@style:name =
        current()/@text:style-name]/style:paragraph-properties/@fo:text-indent"/>
      <text>&#10;</text>
    </when>
  </choose>
</template>

<template name="textstyle">
  <choose>
    <when test="//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-variant =
        'small-caps'">
      <text>\f(SC</text>
    </when>
    <when test="//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-style =
        'italic' and
        //style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-weight =
        'bold'">
      <text>\f(BI</text>
    </when>
    <when test="//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-style =
        'italic'">
      <text>\fI</text>
    </when>
    <when test="//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-weight =
        'bold'">
      <text>\fB</text>
    </when>
  </choose>
  <!-- discard font sizes
  <choose>
    <when test="contains(//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-size,
        'pt')">
      <text>\s[<value-of select="substring-before(//style:style[@style:name =
          current()/@text:style-name]/style:text-properties/@fo:font-size,
          'p')"/>]</text>
    </when>
    <when test="contains(//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-size,
        '%')">
      <text>\s[\n(PS*<value-of
          select="substring-before(//style:style[@style:name =
          current()/@text:style-name]/style:text-properties/@fo:font-size,
          '%')"/>/100]</text>
    </when>
  </choose>
  -->
</template>

<template name="endtextstyle">
  <choose>
    <when test="//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-variant =
        'small-caps' or
        //style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-style =
        'italic' or
        //style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-weight =
        'bold'">
    <text>\fR</text>
  </when>
  </choose>
  <!-- discard font sizes
  <choose>
    <when test="contains(//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-size,
        'pt')">
      <text>\s[\n(PS]</text>
    </when>
    <when test="contains(//style:style[@style:name =
        current()/@text:style-name]/style:text-properties/@fo:font-size,
        '%')">
      <text>\s[\n(PS]</text>
    </when>
  </choose>
  -->
</template>

<template name="liststyle">
  <choose>
    <when test="//text:list-style[@style:name =
        current()/@text:style-name]/text:list-level-style-bullet">
      <text>.ML </text>
      <value-of select="//text:list-style[@style:name =
          current()/@text:style-name]/text:list-level-style-bullet[1]/@text:bullet-char"/>
      <text>&#10;</text>
    </when>
    <when test="//text:list-style[@style:name =
        current()/@text:style-name]/text:list-level-style-number">
      <text>.AL&#10;</text>
    </when>
    <otherwise>
      <text>.AL&#10;</text>
    </otherwise>
    </choose>
</template>

<template match="text:span">
  <call-template name="textstyle"/>
    <apply-templates/>
  <call-template name="endtextstyle"/>
</template>

<template match="text:p//text:line-break">
  <if test="preceding-sibling::node()">
    <if test="preceding::node()[position() = 1][self::text()] or
        preceding::node()[position() = 1][self::text:span] or
        preceding::node()[position() = 1][self::text:s]">
      <text>&#10;</text>
    </if>
    <text>.br</text>
    <if test="following-sibling::node()">
      <text>&#10;</text>
    </if>
  </if>
</template>

<template match="text:p[descendant-or-self::text()]">
  <call-template name="parastyle"/>
  <call-template name="textstyle"/>
  <apply-templates/>
  <call-template name="endtextstyle"/>
  <text>&#10;</text>
</template>

<template match="text:list-item/text:p[descendant-or-self::text()]">
  <text>.LP&#10;</text>
  <call-template name="textstyle"/>
  <apply-templates/>
  <call-template name="endtextstyle"/>
</template>

<template match="text:list">
  <call-template name="liststyle"/>
  <apply-templates/>
  <text>.LE&#10;</text>
</template>

<template match="text:list-item">
  <text>.LI&#10;</text>
  <apply-templates/>
  <text>&#10;</text>
</template>

<template match="text:h">
  <text>.H </text>
  <value-of select="@text:outline-level"/>
  <text> "</text>
  <apply-templates/>
  <text>" "</text>
  <call-template name="textstyle"/>
  <text>" "</text>
  <call-template name="endtextstyle"/>
  <text>"&#10;</text>
</template>

<template match="text:note">
  <text>\*F\c&#10;</text>
  <apply-templates/>
</template>

<template match="text:note-citation">
  <text>.\" footnote </text>
  <apply-templates/>
  <text>&#10;</text>
</template>

<template match="text:note-body">
  <text>.FS&#10;</text>
  <apply-templates mode="footnote"/>
  <text>.FE&#10;</text>
</template>

<template match="text:p[1]" mode="footnote">
  <apply-templates/>
  <text>&#10;</text>
</template>

<template match="table:table">
  <text>.TS&#10;</text>
  <apply-templates/>
  <text>.TE&#10;</text>
</template>

<template match="table:table-column">
  <call-template name="tablekey">
    <with-param name="n" select="@table:number-columns-repeated"/>
  </call-template>
  <if test="not(following-sibling::table:table-column)">
    <text>.&#10;</text>
  </if>
</template>

<template name="tablekey">
  <text>l </text>
  <if test="$n > 1">
    <call-template name="tablekey">
      <with-param name="n" select="$n - 1"/>
    </call-template>
  </if>
</template>

<template match="table:table-row">
  <apply-templates/>
</template>

<template match="table:table-cell">
  <text>T{&#10;</text>
  <apply-templates/>
  <text>T}</text>
  <choose>
    <when test="following-sibling::table:table-cell">
      <text>&#9;</text>
    </when>
    <otherwise>
      <text>&#10;</text>
    </otherwise>
  </choose>
</template>

<template match="table:table-cell/text:p[1][descendant::text()]">
  <call-template name="textstyle"/>
  <apply-templates/>
  <call-template name="endtextstyle"/>
  <text>&#10;</text>
</template>

<template match="/">
  <text>.\" Converted by odt2tr.xsl 1.14 (gritter) 10/10/06 on </text>
  <value-of select="date:date-time()"/>
  <text>&#10;</text>
  <apply-templates/>
</template>

</stylesheet>
