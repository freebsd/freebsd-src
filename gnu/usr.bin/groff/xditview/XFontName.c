/*
 * XFontName.c
 *
 * build/parse X Font name strings
 */

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include "XFontName.h"
#include <ctype.h>

static char *
extractStringField (name, buffer, size, attrp, bit)
	char		*name;
	char		*buffer;
	int		size;
	unsigned int	*attrp;
	unsigned int	bit;
{
	char	*buf = buffer;

	if (!*name)
		return 0;
	while (*name && *name != '-' && size > 0) {
		*buf++ = *name++;
		--size;
	}
	if (size <= 0)
		return 0;
	*buf = '\0';
	if (buffer[0] != '*' || buffer[1] != '\0')
		*attrp |= bit;
	if (*name == '-')
		return name+1;
	return name;
}

static char *
extractUnsignedField (name, result, attrp, bit)
	char		*name;
	unsigned int	*result;
	unsigned int	*attrp;
	unsigned int	bit;
{
	char		buf[256];
	char		*c;
	unsigned int	i;

	name = extractStringField (name, buf, sizeof (buf), attrp, bit);
	if (!name)
		return 0;
	if (!(*attrp & bit))
		return name;
	i = 0;
	for (c = buf; *c; c++) {
		if (!isdigit (*c))
			return 0;
		i = i * 10 + (*c - '0');
	}
	*result = i;
	return name;
}

Bool
XParseFontName (fontNameString, fontName, fontNameAttributes)
	XFontNameString	fontNameString;
	XFontName	*fontName;
	unsigned int	*fontNameAttributes;
{
	char		*name = fontNameString;
	XFontName	temp;
	unsigned int	attributes = 0;

#define GetString(field,bit)\
	if (!(name = extractStringField \
		(name, temp.field, sizeof (temp.field),\
		&attributes, bit))) \
		return False;

#define GetUnsigned(field,bit)\
	if (!(name = extractUnsignedField \
		(name, &temp.field, \
		&attributes, bit))) \
		return False;

	GetString (Registry, FontNameRegistry)
	GetString (Foundry, FontNameFoundry)
	GetString (FamilyName, FontNameFamilyName)
	GetString (WeightName, FontNameWeightName)
	GetString (Slant, FontNameSlant)
	GetString (SetwidthName, FontNameSetwidthName)
	GetString (AddStyleName, FontNameAddStyleName)
	GetUnsigned (PixelSize, FontNamePixelSize)
	GetUnsigned (PointSize, FontNamePointSize)
	GetUnsigned (ResolutionX, FontNameResolutionX)
	GetUnsigned (ResolutionY, FontNameResolutionY)
	GetString (Spacing, FontNameSpacing)
	GetUnsigned (AverageWidth, FontNameAverageWidth)
	GetString (CharSetRegistry, FontNameCharSetRegistry)
	if (!*name) {
		temp.CharSetEncoding[0] = '\0';
		attributes |= FontNameCharSetEncoding;
	} else {
		GetString (CharSetEncoding, FontNameCharSetEncoding)
	}
	*fontName = temp;
	*fontNameAttributes = attributes;
	return True;
}

static char *
utoa (u, s, size)
	unsigned int	u;
	char		*s;
	int		size;
{
	char	*t;

	t = s + size;
	*--t = '\0';
	do
		*--t = (u % 10) + '0';
	while (u /= 10);
	return t;
}

Bool
XFormatFontName (fontName, fontNameAttributes, fontNameString)
	XFontName	*fontName;
	unsigned int	fontNameAttributes;
	XFontNameString	fontNameString;
{
	XFontNameString	tmp;
	char		*name = tmp, *f;
	int		left = sizeof (tmp) - 1;
	char		number[32];

#define PutString(field, bit)\
	f = (fontNameAttributes & bit) ? \
		fontName->field \
		: "*"; \
	if ((left -= strlen (f)) < 0) \
		return False; \
	while (*f) \
		if ((*name++ = *f++) == '-') \
			return False;
#define PutHyphen()\
	if (--left < 0) \
		return False; \
	*name++ = '-';

#define PutUnsigned(field, bit) \
	f = (fontNameAttributes & bit) ? \
		utoa (fontName->field, number, sizeof (number)) \
		: "*"; \
	if ((left -= strlen (f)) < 0) \
		return False; \
	while (*f) \
		*name++ = *f++;

	PutString (Registry, FontNameRegistry)
	PutHyphen ();
	PutString (Foundry, FontNameFoundry)
	PutHyphen ();
	PutString (FamilyName, FontNameFamilyName)
	PutHyphen ();
	PutString (WeightName, FontNameWeightName)
	PutHyphen ();
	PutString (Slant, FontNameSlant)
	PutHyphen ();
	PutString (SetwidthName, FontNameSetwidthName)
	PutHyphen ();
	PutString (AddStyleName, FontNameAddStyleName)
	PutHyphen ();
	PutUnsigned (PixelSize, FontNamePixelSize)
	PutHyphen ();
	PutUnsigned (PointSize, FontNamePointSize)
	PutHyphen ();
	PutUnsigned (ResolutionX, FontNameResolutionX)
	PutHyphen ();
	PutUnsigned (ResolutionY, FontNameResolutionY)
	PutHyphen ();
	PutString (Spacing, FontNameSpacing)
	PutHyphen ();
	PutUnsigned (AverageWidth, FontNameAverageWidth)
	PutHyphen ();
	PutString (CharSetRegistry, FontNameCharSetRegistry)
	PutHyphen ();
	PutString (CharSetEncoding, FontNameCharSetEncoding)
	*name = '\0';
	strcpy (fontNameString, tmp);
	return True;
}

Bool
XCompareFontName (name1, name2, fontNameAttributes)
	XFontName	*name1, *name2;
	unsigned int	fontNameAttributes;
{
#define CompareString(field,bit) \
	if (fontNameAttributes & bit) \
		if (strcmp (name1->field, name2->field)) \
			return False;

#define CompareUnsigned(field,bit) \
	if (fontNameAttributes & bit) \
		if (name1->field != name2->field) \
			return False;

	CompareString (Registry, FontNameRegistry)
	CompareString (Foundry, FontNameFoundry)
	CompareString (FamilyName, FontNameFamilyName)
	CompareString (WeightName, FontNameWeightName)
	CompareString (Slant, FontNameSlant)
	CompareString (SetwidthName, FontNameSetwidthName)
	CompareString (AddStyleName, FontNameAddStyleName)
	CompareUnsigned (PixelSize, FontNamePixelSize)
	CompareUnsigned (PointSize, FontNamePointSize)
	CompareUnsigned (ResolutionX, FontNameResolutionX)
	CompareUnsigned (ResolutionY, FontNameResolutionY)
	CompareString (Spacing, FontNameSpacing)
	CompareUnsigned (AverageWidth, FontNameAverageWidth)
	CompareString (CharSetRegistry, FontNameCharSetRegistry)
	CompareString (CharSetEncoding, FontNameCharSetEncoding)
	return True;
}

XCopyFontName (name1, name2, fontNameAttributes)
	XFontName	*name1, *name2;
	unsigned int	fontNameAttributes;
{
#define CopyString(field,bit) \
	if (fontNameAttributes & bit) \
		strcpy (name2->field, name1->field);

#define CopyUnsigned(field,bit) \
	if (fontNameAttributes & bit) \
		name2->field = name1->field;

	CopyString (Registry, FontNameRegistry)
	CopyString (Foundry, FontNameFoundry)
	CopyString (FamilyName, FontNameFamilyName)
	CopyString (WeightName, FontNameWeightName)
	CopyString (Slant, FontNameSlant)
	CopyString (SetwidthName, FontNameSetwidthName)
	CopyString (AddStyleName, FontNameAddStyleName)
	CopyUnsigned (PixelSize, FontNamePixelSize)
	CopyUnsigned (PointSize, FontNamePointSize)
	CopyUnsigned (ResolutionX, FontNameResolutionX)
	CopyUnsigned (ResolutionY, FontNameResolutionY)
	CopyString (Spacing, FontNameSpacing)
	CopyUnsigned (AverageWidth, FontNameAverageWidth)
	CopyString (CharSetRegistry, FontNameCharSetRegistry)
	CopyString (CharSetEncoding, FontNameCharSetEncoding)
	return True;
}
