extern int server;

extern int ServerLocalOpen(const char *name, mode_t mask);
extern int ServerTcpOpen(int);
extern void ServerClose(void);

#define UNKNOWN_SERVER (-2)
#define NO_SERVER      (-1)
#define LOCAL_SERVER   (1)
#define INET_SERVER    (2)

extern int ServerType(void);
