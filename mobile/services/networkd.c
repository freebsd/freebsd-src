/*
 * Network Daemon - Implementation
 * BSD-style network management service
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include "networkd.h"

static int networkd_running = 0;
static int networkd_socket_fd = -1;
static struct iface_info iface_list[MAX_INTERFACES];
static int iface_count = 0;

static void
networkd_sigterm_handler(int sig __unused)
{
    networkd_running = 0;
}

static int
read_net_sysfs(const char *base, const char *attr, char *buf, size_t bufsz)
{
    char path[256];
    int fd;
    int len;

    snprintf(path, sizeof(path), "/sys/class/net/%s/%s", base, attr);
    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;
    len = read(fd, buf, bufsz - 1);
    if (len > 0)
        buf[len] = '\0';
    close(fd);
    return len;
}

int
networkd_enum_interfaces(struct iface_info **ifaces, int *count)
{
    DIR *dir;
    struct dirent *entry;
    int found = 0;
    struct iface_info *iface_arr;

    dir = opendir("/sys/class/net");
    if (!dir) {
        *ifaces = NULL;
        *count = 0;
        return -1;
    }

    iface_arr = malloc(MAX_INTERFACES * sizeof(struct iface_info));
    if (!iface_arr) {
        closedir(dir);
        return -1;
    }

    while ((entry = readdir(dir)) && found < MAX_INTERFACES) {
        if (entry->d_name[0] == '.')
            continue;

        strncpy(iface_arr[found].name, entry->d_name, sizeof(iface_arr[found].name) - 1);
        iface_arr[found].index = found;
        iface_arr[found].state = IFACE_STATE_DOWN;

        read_net_sysfs(entry->d_name, "address", iface_arr[found].mac_addr,
                      sizeof(iface_arr[found].mac_addr));

        iface_arr[found].ip_addr[0] = '\0';
        iface_arr[found].netmask[0] = '\0';
        found++;
    }
    closedir(dir);

    *ifaces = iface_arr;
    *count = found;
    return 0;
}

int
networkd_iface_up(const char *ifname)
{
    int fd;
    char path[256];

    snprintf(path, sizeof(path), "/sys/class/net/%s/flags", ifname);
    fd = open(path, O_RDWR);
    if (fd == -1)
        return -1;

    /* Set IFF_UP (0x1) and IFF_RUNNING (0x4000) */
    char flags[] = "0x4001";
    write(fd, flags, strlen(flags));
    close(fd);

    syslog(LOG_INFO, "Interface %s brought up", ifname);
    return 0;
}

int
networkd_iface_down(const char *ifname)
{
    int fd;
    char path[256];

    snprintf(path, sizeof(path), "/sys/class/net/%s/flags", ifname);
    fd = open(path, O_RDWR);
    if (fd == -1)
        return -1;

    char flags[] = "0x0";
    write(fd, flags, strlen(flags));
    close(fd);

    syslog(LOG_INFO, "Interface %s brought down", ifname);
    return 0;
}

int
networkd_dhcp_request(const char *ifname)
{
    pid_t pid;
    char *argv[] = { "/sbin/dhclient", "-v", (char *)ifname, NULL };

    syslog(LOG_INFO, "Starting DHCP client on %s", ifname);

    pid = fork();
    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    } else if (pid > 0) {
        return 0;
    }
    return -1;
}

int
networkd_dns_set(const char *server)
{
    int fd;
    char *nameservers[] = { "/etc/resolv.conf" };

    fd = open(nameservers[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
        return -1;

    dprintf(fd, "nameserver %s\n", server);
    close(fd);

    syslog(LOG_INFO, "DNS server set to %s", server);
    return 0;
}

int
networkd_route_add(const char *destination, const char *gateway)
{
    pid_t pid;
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "/sbin/route add default %s", gateway);

    pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
    return pid > 0 ? 0 : -1;
}

int
networkd_wifi_scan(struct wifi_network **networks, int *count)
{
    /* Mock implementation */
    *networks = NULL;
    *count = 0;
    return 0;
}

int
networkd_wifi_connect(const char *ssid, const char *password)
{
    /* Mock implementation */
    syslog(LOG_INFO, "WiFi connect to '%s' (mock)", ssid);
    return 0;
}

int
networkd_init(void)
{
    struct sigaction sa;
    struct sockaddr_un addr;

    openlog("networkd", LOG_PID, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = networkd_sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Enumerate interfaces */
    networkd_enum_interfaces((struct iface_info **)&iface_list, &iface_count);

    /* Create control socket */
    networkd_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (networkd_socket_fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, NETWORKD_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        unlink(NETWORKD_SOCKET_PATH);
        if (bind(networkd_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(networkd_socket_fd);
            networkd_socket_fd = -1;
        }
    }

    networkd_running = 1;
    syslog(LOG_INFO, "Network daemon initialized (found %d interfaces)", iface_count);
    return 0;
}

void
networkd_shutdown(void)
{
    int i;

    networkd_running = 0;

    /* Stop DHCP clients */
    for (i = 0; i < iface_count; i++) {
        if (iface_list[i].state == IFACE_STATE_CONFIG) {
            networkd_iface_down(iface_list[i].name);
        }
    }

    if (networkd_socket_fd >= 0) {
        close(networkd_socket_fd);
        unlink(NETWORKD_SOCKET_PATH);
    }

    closelog();
}

int
networkd_main(int argc __unused, char **argv __unused)
{
    struct pollfd pfd;

    if (networkd_init() < 0)
        return 1;

    pfd.fd = networkd_socket_fd;
    pfd.events = POLLIN;

    while (networkd_running) {
        poll(&pfd, 1, 5000);

        /* Periodic network monitoring */
        for (int i = 0; i < iface_count; i++) {
            if (iface_list[i].state == IFACE_STATE_UP) {
                /* Check link status periodically */
            }
        }
    }

    networkd_shutdown();
    return 0;
}