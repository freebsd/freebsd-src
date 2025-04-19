#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s family host protocol port\n", argv[0]);
        return 1;
    }
    int family = atoi(argv[1]);
    const char *host = argv[2];
    const char *protocol = argv[3];
    const char *port = argv[4];
    int sock_type;
    if (strcmp(protocol, "tcp") == 0)
        sock_type = SOCK_STREAM;
    else if (strcmp(protocol, "udp") == 0)
        sock_type = SOCK_DGRAM;
    else {
        fprintf(stderr, "Unsupported protocol: %s\n", protocol);
        return 1;
    }
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = sock_type;
    hints.ai_flags = AI_PASSIVE;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return 1;
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
        if (errno == EACCES || errno == EPERM)
            printf("bind_error: permission denied.\n");
        else
            printf("bind error: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return 1;
    }
    printf("ok\n");
    close(sock);
    freeaddrinfo(res);
    return 0;
}

