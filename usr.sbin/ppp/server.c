#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "vars.h"
#include "server.h"

int server = UNKNOWN_SERVER;
static struct sockaddr_un ifsun;
static char *rm;

int
ServerLocalOpen(const char *name, mode_t mask)
{
  int s;

  ifsun.sun_len = strlen(name);
  if (ifsun.sun_len > sizeof ifsun.sun_path - 1) {
    LogPrintf(LogERROR, "Local: %s: Path too long\n", name);
    return 1;
  }
  ifsun.sun_family = AF_LOCAL;
  strcpy(ifsun.sun_path, name);

  s = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "Local: socket: %s\n", strerror(errno));
    return 2;
  }
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &s, sizeof s);
  mask = umask(mask);
  if (bind(s, (struct sockaddr *) & ifsun, sizeof(ifsun)) < 0) {
    umask(mask);
    LogPrintf(LogERROR, "Local: bind: %s\n", strerror(errno));
    if (errno == EADDRINUSE && VarTerm)
      fprintf(VarTerm, "Wait for a while, then try again.\n");
    close(s);
    unlink(name);
    return 3;
  }
  umask(mask);
  if (listen(s, 5) != 0) {
    LogPrintf(LogERROR, "Local: Unable to listen to socket - OS overload?\n");
    close(s);
    unlink(name);
    return 4;
  }
  ServerClose();
  server = s;
  rm = ifsun.sun_path;
  LogPrintf(LogPHASE, "Listening at local socket %s.\n", name);
  return 0;
}

int
ServerTcpOpen(int port)
{
  struct sockaddr_in ifsin;
  int s;

  s = socket(PF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "Tcp: socket: %s\n", strerror(errno));
    return 5;
  }
  ifsin.sin_family = AF_INET;
  ifsin.sin_addr.s_addr = INADDR_ANY;
  ifsin.sin_port = htons(port);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &s, sizeof s);
  if (bind(s, (struct sockaddr *) & ifsin, sizeof(ifsin)) < 0) {
    LogPrintf(LogERROR, "Tcp: bind: %s\n", strerror(errno));
    if (errno == EADDRINUSE && VarTerm)
      fprintf(VarTerm, "Wait for a while, then try again.\n");
    close(s);
    return 6;
  }
  if (listen(s, 5) != 0) {
    LogPrintf(LogERROR, "Tcp: Unable to listen to socket - OS overload?\n");
    close(s);
    return 7;
  }
  ServerClose();
  server = s;
  LogPrintf(LogPHASE, "Listening at port %d.\n", port);
  return 0;
}

void
ServerClose()
{
  if (server >= 0) {
    close(server);
    if (rm) {
      unlink(rm);
      rm = 0;
    }
  }
  server = -1;
}

int
ServerType()
{
  if (server == UNKNOWN_SERVER)
    return UNKNOWN_SERVER;
  else if (server == NO_SERVER)
    return NO_SERVER;
  else if (rm)
    return LOCAL_SERVER;
  else
    return INET_SERVER;
}
