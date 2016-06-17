#ifndef _INCLUDE_GUARD_PROTOTYPES_H
#define _INCLUDE_GUARD_PROTOTYPES_H


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <net/sock.h>
#include <asm/uaccess.h>

#include "structure.h"


/* General defines and stuff */


#define CONFIG_KHTTPD_NUMCPU 16    /* Maximum number of threads */

#ifdef OOPSTRACE
#define EnterFunction(x)   printk("Enter: %s, %s line %i\n",x,__FILE__,__LINE__)
#define LeaveFunction(x)   printk("Leave: %s, %s line %i\n",x,__FILE__,__LINE__)
#else
#define EnterFunction(x)   do {} while (0)
#define LeaveFunction(x)   do {} while (0)
#endif



/* sockets.c */
int  StartListening(const int Port);
void StopListening(void);

extern struct socket *MainSocket;


/* sysctl.c */
void StartSysctl(void);
void EndSysctl(void);

extern int sysctl_khttpd_stop;


/* main.c */


extern struct khttpd_threadinfo threadinfo[CONFIG_KHTTPD_NUMCPU];
extern char CurrentTime[];
extern atomic_t ConnectCount;
extern struct wait_queue main_wait[CONFIG_KHTTPD_NUMCPU];

/* misc.c */

void CleanUpRequest(struct http_request *Req);
int SendBuffer(struct socket *sock, const char *Buffer,const size_t Length);
int SendBuffer_async(struct socket *sock, const char *Buffer,const size_t Length);
void Send403(struct socket *sock);
void Send304(struct socket *sock);
void Send50x(struct socket *sock);

/* accept.c */

int AcceptConnections(const int CPUNR,struct socket *Socket);

/* waitheaders.c */

int WaitForHeaders(const int CPUNR);
void StopWaitingForHeaders(const int CPUNR);
int InitWaitHeaders(int ThreadCount);

/* datasending.c */

int DataSending(const int CPUNR);
void StopDataSending(const int CPUNR);
int InitDataSending(int ThreadCount);


/* userspace.c */

int Userspace(const int CPUNR);
void StopUserspace(const int CPUNR);
void InitUserspace(const int CPUNR);


/* rfc_time.c */

void time_Unix2RFC(const time_t Zulu,char *Buffer);
void UpdateCurrentDate(void);
time_t mimeTime_to_UnixTime(char *Q);
extern int CurrentTime_i;

/* rfc.c */

void ParseHeader(char *Buffer,const int length, struct http_request *Head);
char *ResolveMimeType(const char *File,__kernel_size_t *Len);
void AddMimeType(const char *Ident,const char *Type);
void SendHTTPHeader(struct http_request *Request);



/* security.c */

struct file *OpenFileForSecurity(char *Filename);
void AddDynamicString(const char *String);
void GetSecureString(char *String);


/* logging.c */

int Logging(const int CPUNR);
void StopLogging(const int CPUNR);


/* Other prototypes */



#endif
