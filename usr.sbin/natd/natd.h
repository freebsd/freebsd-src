#define PIDFILE	"/var/run/natd.pid"

extern void Quit (char* msg);
extern void Warn (char* msg);
extern int SendNeedFragIcmp (int sock, struct ip* failedDgram, int mtu);
