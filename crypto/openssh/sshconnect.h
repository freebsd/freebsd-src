#ifndef SSHCONNECT_H
#define SSHCONNECT_H

void
check_host_key(char *host, struct sockaddr *hostaddr, Key *host_key,
    const char *user_hostfile, const char *system_hostfile);

void	ssh_kex(char *host, struct sockaddr *hostaddr);
void
ssh_userauth(const char* local_user, const char* server_user, char *host,
    int host_key_valid, RSA *own_host_key);

void	ssh_kex2(char *host, struct sockaddr *hostaddr);
void	ssh_userauth2(const char *server_user, char *host);

#endif
