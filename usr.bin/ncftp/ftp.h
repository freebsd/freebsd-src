/* ftp.h */

#ifndef _ftp_h_
#define _ftp_h_

/*  $RCSfile: ftp.h,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/07/09 11:04:12 $
 */

#define IS_FILE 1
#define IS_STREAM 0
#define IS_PIPE -1

/* Progress-meter types. */
#define pr_none 0
#define pr_percent 1
#define pr_philbar 2
#define pr_kbytes 3
#define pr_dots 4
#define pr_last pr_dots

/* Values sent to CommandWithFlags() to determine whether to read a reply
 * from the remote host after sending the command.
 */
#define DONT_WAIT_FOR_REPLY 0
#define WAIT_FOR_REPLY 1

/* Expect EOF values for getreply() */
#define DONT_EXPECT_EOF		0
#define EXPECT_EOF			1

int hookup(char *, unsigned int);
int Login(char *userNamePtr, char *passWordPtr, char *accountPtr, int doInit);
void cmdabort SIG_PARAMS;
int CommandWithFlags(char *, int);
int command(char *);
int command_noreply(char *);
int quiet_command(char *);
int verbose_command(char *);
int getreply(int);
int start_progress(int, char *);
int progress_report(int);
void end_progress(char *, char *, char *);
void close_file(FILE **, int);
void abortsend SIG_PARAMS;
int sendrequest(char *, char *, char *);
void abortrecv SIG_PARAMS;
void GetLSRemoteDir(char *, char *);
int AdjustLocalFileName(char *);
int SetToAsciiForLS(int, int);
int IssueCommand(char *, char *);
FILE *OpenOutputFile(int, char *, char *, Sig_t *);
void ReceiveBinary(FILE *, FILE *, int *, char *);
void AddRedirLine(char *);
void ReceiveAscii(FILE *, FILE *, int *, char *, int);
void CloseOutputFile(FILE *, int, char *, time_t);
void ResetOldType(int);
int FileType(char *);
void CloseData(void);
int recvrequest(char *, char *, char *, char *);
int initconn(void);
FILE *dataconn(char *);

#endif /* _ftp_h_ */

/* eof ftp.h */
