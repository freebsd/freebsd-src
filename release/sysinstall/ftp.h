typedef struct {
    enum {init, isopen, xfer} state;
    int		fd_ctrl;
    int		fd_xfer;
    int		fd_debug;
    int		binary;
    int		passive;
    int		addrtype;
    char	*host;
    char	*file;
} *FTP_t;

FTP_t		FtpInit();
int		FtpOpen(FTP_t, char *host, char *user, char *passwd);
#define 	FtpBinary(ftp,bool)	{ (ftp)->binary = (bool); }
#define 	FtpPassive(ftp,bool)	{ (ftp)->passive = (bool); }
#ifndef STANDALONE_FTP
#define 	FtpDebug(ftp, bool)	{ (ftp)->fd_debug = (bool); }
#endif
int		FtpChdir(FTP_t, char *);
int		FtpGet(FTP_t, char *);
int		FtpEOF(FTP_t);
void		FtpClose(FTP_t);

