/*
 * Header file defaults.h - assorted default values for character strings in
 * the volume descriptor.
 */

#define  PREPARER_DEFAULT 	NULL
#define  PUBLISHER_DEFAULT	NULL
#define  APPID_DEFAULT 		NULL
#define  COPYRIGHT_DEFAULT 	NULL
#define  BIBLIO_DEFAULT 	NULL
#define  ABSTRACT_DEFAULT 	NULL
#define  VOLSET_ID_DEFAULT 	NULL
#define  VOLUME_ID_DEFAULT 	"CDROM"
#ifdef __FreeBSD__
#define  SYSTEM_ID_DEFAULT 	"FreeBSD"
#else
#ifdef __QNX__
#define  SYSTEM_ID_DEFAULT 	"QNX"
#else
#define  SYSTEM_ID_DEFAULT 	"LINUX"
#endif
#endif
