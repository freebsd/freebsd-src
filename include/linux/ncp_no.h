#ifndef _NCP_NO
#define _NCP_NO

/* these define the attribute byte as seen by NCP */
#define aRONLY			(ntohl(0x01000000))
#define aHIDDEN			(__constant_ntohl(0x02000000))
#define aSYSTEM			(__constant_ntohl(0x04000000))
#define aEXECUTE		(ntohl(0x08000000))
#define aDIR			(ntohl(0x10000000))
#define aARCH			(ntohl(0x20000000))
#define aSHARED			(ntohl(0x80000000))
#define aDONTSUBALLOCATE	(ntohl(1L<<(11+8)))
#define aTRANSACTIONAL		(ntohl(1L<<(12+8)))
#define aPURGE			(ntohl(1L<<(16-8)))
#define aRENAMEINHIBIT		(ntohl(1L<<(17-8)))
#define aDELETEINHIBIT		(ntohl(1L<<(18-8)))
#define aDONTCOMPRESS		(nothl(1L<<(27-24)))

#endif /* _NCP_NO */
