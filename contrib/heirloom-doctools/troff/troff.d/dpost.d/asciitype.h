/*
 * Copyright (c) 2003 Gunnar Ritter
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */
/*	Sccsid @(#)asciitype.h	1.6 (gritter) 9/9/05	*/

/*
 * Locale-independent character classes.
 */
enum {
	C_CNTRL	= 0000,
	C_BLANK	= 0001,
	C_WHITE = 0002,
	C_SPACE	= 0004,
	C_PUNCT	= 0010,
	C_OCTAL	= 0020,
	C_DIGIT	= 0040,
	C_UPPER	= 0100,
	C_LOWER	= 0200
};

extern const unsigned char	class_char[];

#define	asciichar(c) ((unsigned)(c) <= 0177)
#define	alnumchar(c) (asciichar(c)&&(class_char[c]&\
			(C_DIGIT|C_OCTAL|C_UPPER|C_LOWER)))
#define	alphachar(c) (asciichar(c)&&(class_char[c]&(C_UPPER|C_LOWER)))
#define	blankchar(c) (asciichar(c)&&(class_char[c]&(C_BLANK)))
#define	cntrlchar(c) (asciichar(c)&&(class_char[c]==C_CNTRL)
#define	digitchar(c) (asciichar(c)&&(class_char[c]&(C_DIGIT|C_OCTAL)))
#define	lowerchar(c) (asciichar(c)&&(class_char[c]&(C_LOWER)))
#define	punctchar(c) (asciichar(c)&&(class_char[c]&(C_PUNCT)))
#define	spacechar(c) (asciichar(c)&&(class_char[c]&(C_BLANK|C_SPACE|C_WHITE)))
#define	upperchar(c) (asciichar(c)&&(class_char[c]&(C_UPPER)))
#define	whitechar(c) (asciichar(c)&&(class_char[c]&(C_BLANK|C_WHITE)))
#define	octalchar(c) (asciichar(c)&&(class_char[c]&(C_OCTAL)))
#define	graphchar(c) (asciichar(c)&&(class_char[c]&\
			(C_UPPER|C_LOWER|C_DIGIT|C_OCTAL|C_PUNCT)))
#define	printchar(c) ((c)==' ' || (asciichar(c)&&(class_char[c]&\
			(C_UPPER|C_LOWER|C_DIGIT|C_OCTAL|C_PUNCT))))

#define	upperconv(c) (lowerchar(c) ? (c)-'a'+'A' : (c))
#define	lowerconv(c) (upperchar(c) ? (c)-'A'+'a' : (c))
