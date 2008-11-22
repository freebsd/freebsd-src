/*
 * DviChar.h
 *
 * descriptions for mapping dvi names to
 * font indexes and back.  Dvi fonts are all
 * 256 elements (actually only 256-32 are usable).
 *
 * The encoding names are taken from X -
 * case insensitive, a dash separating the
 * CharSetRegistry from the CharSetEncoding
 */

# define DVI_MAX_SYNONYMS	10
# define DVI_MAP_SIZE		256
# define DVI_HASH_SIZE		256

typedef struct _dviCharNameHash {
	struct _dviCharNameHash	*next;
	const char		*name;
	int			position;
} DviCharNameHash;

typedef struct _dviCharNameMap {
    const char		*encoding;
    int			special;
    const char		*dvi_names[DVI_MAP_SIZE][DVI_MAX_SYNONYMS];
    DviCharNameHash	*buckets[DVI_HASH_SIZE];
} DviCharNameMap;

DviCharNameMap		*DviFindMap (char *);
void			DviRegisterMap (DviCharNameMap *);
#ifdef NOTDEF
char			*DviCharName (DviCharNameMap *, int, int);
#else
#define DviCharName(map,index,synonym)	((map)->dvi_names[index][synonym])
#endif
int			DviCharIndex (DviCharNameMap *, const char *);
