/* open.h */

#ifndef _open_h_
#define _open_h_ 1

/* Variables for Open() that can be changed from the command line. */
typedef struct OpenOptions {
	int				openmode;
	int				ignore_rc;
	unsigned int	port;
	int				redial_delay;
	int				max_dials;
	int				ftpcat;
	Hostname		hostname;
	longstring		cdpath;
	longstring		colonmodepath;
} OpenOptions;

typedef struct RemoteSiteInfo {
	int				hasSIZE;
	int				hasMDTM;
} RemoteSiteInfo;

/* Open modes. */
#define openImplicitAnon 1
#define openImplicitUser 4
#define openExplicitAnon 3
#define openExplicitUser 2

#define ISUSEROPEN(a) ((a==openImplicitUser)||(a==openExplicitUser))
#define ISANONOPEN(a) (!ISUSEROPEN(a))
#define ISEXPLICITOPEN(a) ((a==openExplicitAnon)||(a==openExplicitUser))
#define ISIMPLICITOPEN(a) (!ISEXPLICITOPEN(a))

/* ftpcat modes. */
#define NO_FTPCAT	0
#define FTPCAT		1
#define FTPMORE		2

/* Protos: */
void InitOpenOptions(OpenOptions *openopt);
int GetOpenOptions(int argc, char **argv, OpenOptions *openopt);
int CheckForColonMode(OpenOptions *openopt, int *login_verbosity);
int HookupToRemote(OpenOptions *openopt);
void CheckRemoteSystemType(int);
void ColonMode(OpenOptions *openopt);
int Open(OpenOptions *openopt);
int cmdOpen(int argc, char **argv);

#endif	/* _open_h_ */
