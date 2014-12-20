#ifndef WIN32_IO_H
#define WIN32_IO_H

extern	void	InitSockets(void);
extern	void	connection_reset_fix(SOCKET fd, sockaddr_u *addr);

#endif /* WIN32_IO_H */
