typedef struct _xFontName {
	char		Registry[256];
	char		Foundry[256];
	char		FamilyName[256];
	char		WeightName[256];
	char		Slant[3];
	char		SetwidthName[256];
	char		AddStyleName[256];
	unsigned int	PixelSize;
	unsigned int	PointSize;
	unsigned int	ResolutionX;
	unsigned int	ResolutionY;
	char		Spacing[2];
	unsigned int	AverageWidth;
	char		CharSetRegistry[256];
	char		CharSetEncoding[256];
} XFontName;

#define FontNameRegistry	(1<<0)
#define FontNameFoundry		(1<<1)
#define FontNameFamilyName	(1<<2)
#define FontNameWeightName	(1<<3)
#define FontNameSlant		(1<<4)
#define FontNameSetwidthName	(1<<5)
#define FontNameAddStyleName	(1<<6)
#define FontNamePixelSize	(1<<7)
#define FontNamePointSize	(1<<8)
#define FontNameResolutionX	(1<<9)
#define FontNameResolutionY	(1<<10)
#define FontNameSpacing		(1<<11)
#define FontNameAverageWidth	(1<<12)
#define FontNameCharSetRegistry	(1<<13)
#define FontNameCharSetEncoding	(1<<14)

#define SlantRoman		"R"
#define SlantItalic		"I"
#define SlantOblique		"O"
#define SlantReverseItalic	"RI"
#define SlantReverseOblique	"RO"

#define SpacingMonoSpaced	"M"
#define SpacingProportional	"P"
#define SpacingCharacterCell	"C"

typedef char	*XFontNameString;

Bool XParseFontName (XFontNameString, XFontName *, unsigned int *);
Bool XFormatFontName (XFontName *, unsigned int, XFontNameString);
Bool XCompareFontName (XFontName *, XFontName *, unsigned int);
Bool XCopyFontName (XFontName *, XFontName *, unsigned int);
