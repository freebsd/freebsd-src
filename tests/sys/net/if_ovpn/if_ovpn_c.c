//#include <sys/param.h>
#include <stdio.h>

#include <net/if.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/linker.h>
#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <atf-c.h>

#define OVPN_NEW_PEER		_IO  ('D', 1)

static nvlist_t *
fake_sockaddr()
{
	uint32_t addr = htonl(INADDR_LOOPBACK);
	nvlist_t *nvl;

	nvl = nvlist_create(0);

	nvlist_add_number(nvl, "af", AF_INET);
	nvlist_add_binary(nvl, "address", &addr, 4);
	nvlist_add_number(nvl, "port", 1024);

	return (nvl);
}

static char ovpn_ifname[IFNAMSIZ];
static int ovpn_fd;

static int
create_interface(int fd)
{
	int ret;
	struct ifreq ifr;

	bzero(&ifr, sizeof(ifr));

	/* Create ovpnx first, then rename it. */
	snprintf(ifr.ifr_name, IFNAMSIZ, "ovpn");
	ret = ioctl(fd, SIOCIFCREATE2, &ifr);
	if (ret)
		return (ret);

	snprintf(ovpn_ifname, IFNAMSIZ, "%s", ifr.ifr_name);
	printf("Created %s\n", ovpn_ifname);

	return (0);
}

static void
destroy_interface(int fd)
{
	int ret;
	struct ifreq ifr;

	if (ovpn_ifname[0] == 0)
		return;

	printf("Destroy %s\n", ovpn_ifname);

	bzero(&ifr, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ovpn_ifname);

	ret = ioctl(fd, SIOCIFDESTROY, &ifr);
	if (ret)
		atf_tc_fail("Failed to destroy interface");

	ovpn_ifname[0] = 0;
}

ATF_TC_WITH_CLEANUP(tcp);
ATF_TC_HEAD(tcp, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(tcp, tc)
{
	struct ifdrv drv;
	struct sockaddr_in sock_in;
	int ret;
	nvlist_t *nvl;

	/* Ensure the module is loaded. */
	(void)kldload("if_ovpn");

	ovpn_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	/* Kick off a connect so there's a local address set, which we need for
	 * ovpn_new_peer() to get to the critical point. */
	bzero(&sock_in, sizeof(sock_in));
	sock_in.sin_family = AF_INET;
	sock_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sock_in.sin_port = htons(1024);
	connect(ovpn_fd, (struct sockaddr *)&sock_in, sizeof(sock_in));

	ret = create_interface(ovpn_fd);
	if (ret)
		atf_tc_fail("Failed to create interface");

	nvl = nvlist_create(0);

	nvlist_add_number(nvl, "peerid", 0);
	nvlist_add_number(nvl, "fd", ovpn_fd);
	nvlist_add_nvlist(nvl, "remote", fake_sockaddr());

	bzero(&drv, sizeof(drv));
	snprintf(drv.ifd_name, IFNAMSIZ, "%s", ovpn_ifname);
	drv.ifd_cmd = OVPN_NEW_PEER;
	drv.ifd_data = nvlist_pack(nvl, &drv.ifd_len);

	ret = ioctl(ovpn_fd, SIOCSDRVSPEC, &drv);
	ATF_CHECK_EQ(ret, -1);
	ATF_CHECK_EQ(errno, EPROTOTYPE);
}

ATF_TC_CLEANUP(tcp, tc)
{
	destroy_interface(ovpn_fd);
	close(ovpn_fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, tcp);

	return (atf_no_error());
}
