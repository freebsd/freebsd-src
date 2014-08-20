/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#include <limits.h>
#include "oslib/font.h"
#include "rufl.h"
#ifdef __CC_NORCROFT
#include "strfuncs.h"
#endif


/** The available characters in a font. The range which can be represented is
 * 0x0000 to 0xffff. The size of the structure is 4 + 256 + 32 * blocks. A
 * typical * 200 glyph font might have characters in 10 blocks, giving 580
 * bytes. The maximum possible size of the structure is 8388 bytes. Note that
 * since two index values are reserved, fonts with 65280-65024 glyphs may be
 * unrepresentable, if there are no full blocks. This is unlikely. The primary
 * aim of this structure is to make lookup fast. */
struct rufl_character_set {
	/** Size of structure / bytes. */
	size_t size;

	/** Index table. Each entry represents a block of 256 characters, so
	 * i[k] refers to characters [256*k, 256*(k+1)). The value is either
	 * BLOCK_EMPTY, BLOCK_FULL, or an offset into the block table. */
	unsigned char index[256];
	/** The block has no characters present. */
#	define BLOCK_EMPTY 254
	/** All characters in the block are present. */
#	define BLOCK_FULL 255

	/** Block table. Each entry is a 256-bit bitmap indicating which
	 * characters in the block are present and absent. */
	unsigned char block[254][32];
};


/** Part of struct rufl_unicode_map. */
struct rufl_unicode_map_entry {
	/** Unicode value. */
	unsigned short u;
	/** Corresponding character. */
	unsigned char c;
};


/** Old font manager: mapping from Unicode to character code. This is simply
 * an array sorted by Unicode value, suitable for bsearch(). If a font has
 * support for multiple encodings, then it will have multiple unicode maps.
 * The encoding field contains the name of the encoding to pass to the 
 * font manager. This will be NULL if the font is a Symbol font. */
struct rufl_unicode_map {
	/** Corresponding encoding name */
	char *encoding;
	/** Number of valid entries in map. */
	unsigned int entries;
	/** Map from Unicode to character code. */
	struct rufl_unicode_map_entry map[256];
};


/** An entry in rufl_font_list. */
struct rufl_font_list_entry {
	/** Font identifier (name). */
	char *identifier;
	/** Character set of font. */
	struct rufl_character_set *charset;
	/** Number of Unicode mapping tables */
	unsigned int num_umaps;
	/** Mappings from Unicode to character code. */
	struct rufl_unicode_map *umap;
	/** Family that this font belongs to (index in rufl_family_list and
	 * rufl_family_map). */
	unsigned int family;
	/** Font weight (0 to 8). */
	unsigned int weight;
	/** Font slant (0 or 1). */
	unsigned int slant;
};
/** List of all available fonts. */
extern struct rufl_font_list_entry *rufl_font_list;
/** Number of entries in rufl_font_list. */
extern size_t rufl_font_list_entries;


/** An entry in rufl_family_map. */
struct rufl_family_map_entry {
	/** This style does not exist in this family. */
#	define NO_FONT UINT_MAX
	/** Map from weight and slant to index in rufl_font_list, or NO_FONT. */
	unsigned int font[9][2];
};
/** Map from font family to fonts, rufl_family_list_entries entries. */
extern struct rufl_family_map_entry *rufl_family_map;


/** No font contains this character. */
#define NOT_AVAILABLE 65535
/** Font substitution table. */
extern unsigned short *rufl_substitution_table;


/** Number of slots in recent-use cache. This is the maximum number of RISC OS
 * font handles that will be used at any time by the library. */
#define rufl_CACHE_SIZE 10

/** An entry in rufl_cache. */
struct rufl_cache_entry {
	/** Font number (index in rufl_font_list), or rufl_CACHE_*. */
	unsigned int font;
	/** No font cached in this slot. */
#define rufl_CACHE_NONE UINT_MAX
	/** Font for rendering hex substitutions in this slot. */
#define rufl_CACHE_CORPUS (UINT_MAX - 1)
	/** Font size. */
	unsigned int size;
	/** Font encoding */
	const char *encoding;
	/** Value of rufl_cache_time when last used. */
	unsigned int last_used;
	/** RISC OS font handle. */
	font_f f;
};
/** Cache of rufl_CACHE_SIZE most recently used font handles. */
extern struct rufl_cache_entry rufl_cache[rufl_CACHE_SIZE];
/** Counter for measuring age of cache entries. */
extern int rufl_cache_time;

/** Font manager does not support Unicode. */
extern bool rufl_old_font_manager;

/** Font manager supports background blending */
extern bool rufl_can_background_blend;

rufl_code rufl_find_font_family(const char *family, rufl_style font_style,
		unsigned int *font, unsigned int *slanted,
		struct rufl_character_set **charset);
rufl_code rufl_find_font(unsigned int font, unsigned int font_size,
		const char *encoding, font_f *fhandle);
bool rufl_character_set_test(struct rufl_character_set *charset,
		unsigned int c);


#define rufl_utf8_read(s, l, u)						       \
	if (4 <= l && ((s[0] & 0xf8) == 0xf0) && ((s[1] & 0xc0) == 0x80) &&    \
			((s[2] & 0xc0) == 0x80) && ((s[3] & 0xc0) == 0x80)) {  \
		u = ((s[0] & 0x7) << 18) | ((s[1] & 0x3f) << 12) |	       \
				((s[2] & 0x3f) << 6) | (s[3] & 0x3f);	       \
		s += 4; l -= 4;						       \
	} else if (3 <= l && ((s[0] & 0xf0) == 0xe0) &&			       \
			((s[1] & 0xc0) == 0x80) &&			       \
			((s[2] & 0xc0) == 0x80)) {			       \
		u = ((s[0] & 0xf) << 12) | ((s[1] & 0x3f) << 6) |	       \
				(s[2] & 0x3f);				       \
		s += 3; l -= 3;						       \
	} else if (2 <= l && ((s[0] & 0xe0) == 0xc0) &&			       \
			((s[1] & 0xc0) == 0x80)) {			       \
		u = ((s[0] & 0x3f) << 6) | (s[1] & 0x3f);		       \
		s += 2; l -= 2;						       \
	} else if ((s[0] & 0x80) == 0) {				       \
		u = s[0];						       \
		s++; l--;						       \
	} else {							       \
		u = 0xfffd;						       \
		s++; l--;						       \
	}

#define rufl_CACHE "<Wimp$ScrapDir>.RUfl_cache"
#define rufl_CACHE_VERSION 3


struct rufl_glyph_map_entry {
	const char *glyph_name;
	unsigned short u;
};

extern const struct rufl_glyph_map_entry rufl_glyph_map[];
extern const size_t rufl_glyph_map_size;


#ifndef NDEBUG
#ifdef __CC_NORCROFT
#define __PRETTY_FUNCTION__ __func__
#endif
#include <time.h>
bool log_got_start_time;
time_t log_start_time;
#define LOG(format, ...)						\
	do {								\
		if (log_got_start_time == false) {			\
			log_start_time = time(NULL);			\
			log_got_start_time = true;			\
		}							\
									\
		fprintf(stderr,"(%.6fs) " __FILE__ " %s %i: ",		\
				difftime(time(NULL), log_start_time),	\
				__PRETTY_FUNCTION__, __LINE__);		\
		fprintf(stderr, format, __VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	} while (0)
#else
#define LOG(format, ...) ((void) 0)
#endif
