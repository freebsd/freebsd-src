/*
 * DECnet       An implementation of the DECnet protocol suite for the LINUX
 *              operating system.  DECnet is implemented using the  BSD Socket
 *              interface as the means of communication with the user level.
 *
 *              DECnet sysctl support functions
 *
 * Author:      Steve Whitehouse <SteveW@ACM.org>
 *
 *
 * Changes:
 *
 */
#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <net/neighbour.h>
#include <net/dst.h>

#include <asm/uaccess.h>

#include <net/dn.h>
#include <net/dn_dev.h>
#include <net/dn_route.h>


int decnet_debug_level;
int decnet_time_wait = 30;
int decnet_dn_count = 1;
int decnet_di_count = 3;
int decnet_dr_count = 3;
int decnet_log_martians = 1;
int decnet_no_fc_max_cwnd = NSP_MIN_WINDOW;

#ifdef CONFIG_SYSCTL
extern int decnet_dst_gc_interval;
static int min_decnet_time_wait[] = { 5 };
static int max_decnet_time_wait[] = { 600 };
static int min_state_count[] = { 1 };
static int max_state_count[] = { NSP_MAXRXTSHIFT };
static int min_decnet_dst_gc_interval[] = { 1 };
static int max_decnet_dst_gc_interval[] = { 60 };
static int min_decnet_no_fc_max_cwnd[] = { NSP_MIN_WINDOW };
static int max_decnet_no_fc_max_cwnd[] = { NSP_MAX_WINDOW };
static char node_name[7] = "???";

static struct ctl_table_header *dn_table_header = NULL;

/*
 * ctype.h :-)
 */
#define ISNUM(x) (((x) >= '0') && ((x) <= '9'))
#define ISLOWER(x) (((x) >= 'a') && ((x) <= 'z'))
#define ISUPPER(x) (((x) >= 'A') && ((x) <= 'Z'))
#define ISALPHA(x) (ISLOWER(x) || ISUPPER(x))
#define INVALID_END_CHAR(x) (ISNUM(x) || ISALPHA(x))

static void strip_it(char *str)
{
	for(;;) {
		switch(*str) {
			case ' ':
			case '\n':
			case '\r':
			case ':':
				*str = 0;
			case 0:
				return;
		}
		str++;
	}
}

/*
 * Simple routine to parse an ascii DECnet address
 * into a network order address.
 */
static int parse_addr(dn_address *addr, char *str)
{
	dn_address area, node;

	while(*str && !ISNUM(*str)) str++;

	if (*str == 0)
		return -1;

	area = (*str++ - '0');
	if (ISNUM(*str)) {
		area *= 10;
		area += (*str++ - '0');
	}

	if (*str++ != '.')
		return -1;

	if (!ISNUM(*str))
		return -1;

	node = *str++ - '0';
	if (ISNUM(*str)) {
		node *= 10;
		node += (*str++ - '0');
	}
	if (ISNUM(*str)) {
		node *= 10;
		node += (*str++ - '0');
	}
	if (ISNUM(*str)) {
		node *= 10;
		node += (*str++ - '0');
	}

	if ((node > 1023) || (area > 63))
		return -1;

	if (INVALID_END_CHAR(*str))
		return -1;

	*addr = dn_htons((area << 10) | node);

	return 0;
}


static int dn_node_address_strategy(ctl_table *table, int *name, int nlen,
				void *oldval, size_t *oldlenp,
				void *newval, size_t newlen,
				void **context)
{
	size_t len;
	dn_address addr;

	if (oldval && oldlenp) {
		if (get_user(len, oldlenp))
			return -EFAULT;
		if (len) {
			if (len != sizeof(unsigned short))
				return -EINVAL;
			if (put_user(decnet_address, (unsigned short *)oldval))
				return -EFAULT;
		}
	}
	if (newval && newlen) {
		if (newlen != sizeof(unsigned short))
			return -EINVAL;
		if (get_user(addr, (unsigned short *)newval))
			return -EFAULT;

		dn_dev_devices_off();

		decnet_address = addr;
		dn_dn2eth(decnet_ether_address, dn_ntohs(decnet_address));

		dn_dev_devices_on();
	}
	return 0;
}

static int dn_node_address_handler(ctl_table *table, int write, 
				struct file *filp,
				void *buffer, size_t *lenp)
{
	char addr[DN_ASCBUF_LEN];
	size_t len;
	dn_address dnaddr;

	if (!*lenp || (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}

	if (write) {
		int len = (*lenp < DN_ASCBUF_LEN) ? *lenp : (DN_ASCBUF_LEN-1);

		if (copy_from_user(addr, buffer, len))
			return -EFAULT;

		addr[len] = 0;
		strip_it(addr);

		if (parse_addr(&dnaddr, addr))
			return -EINVAL;

		dn_dev_devices_off();

		decnet_address = dnaddr;
		dn_dn2eth(decnet_ether_address, dn_ntohs(decnet_address));

		dn_dev_devices_on();

		filp->f_pos += len;

		return 0;
	}

	dn_addr2asc(dn_ntohs(decnet_address), addr);
	len = strlen(addr);
	addr[len++] = '\n';

	if (len > *lenp) len = *lenp;

	if (copy_to_user(buffer, addr, len))
		return -EFAULT;

	*lenp = len;
	filp->f_pos += len;

	return 0;
}


static int dn_def_dev_strategy(ctl_table *table, int *name, int nlen,
				void *oldval, size_t *oldlenp,
				void *newval, size_t newlen,
				void **context)
{
	size_t len;
	struct net_device *dev = decnet_default_device;
	char devname[17];
	size_t namel;

	devname[0] = 0;

	if (oldval && oldlenp) {
		if (get_user(len, oldlenp))
			return -EFAULT;
		if (len) {
			if (dev)
				strcpy(devname, dev->name);

			namel = strlen(devname) + 1;
			if (len > namel) len = namel;	

			if (copy_to_user(oldval, devname, len))
				return -EFAULT;

			if (put_user(len, oldlenp))
				return -EFAULT;
		}
	}

	if (newval && newlen) {
		if (newlen > 16)
			return -E2BIG;

		if (copy_from_user(devname, newval, newlen))
			return -EFAULT;

		devname[newlen] = 0;

		if ((dev = __dev_get_by_name(devname)) == NULL)
			return -ENODEV;

		if (dev->dn_ptr == NULL)
			return -ENODEV;

		decnet_default_device = dev;
	}

	return 0;
}


static int dn_def_dev_handler(ctl_table *table, int write, 
				struct file * filp,
				void *buffer, size_t *lenp)
{
	size_t len;
	struct net_device *dev = decnet_default_device;
	char devname[17];

	if (!*lenp || (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}

	if (write) {
		if (*lenp > 16)
			return -E2BIG;

		if (copy_from_user(devname, buffer, *lenp))
			return -EFAULT;

		devname[*lenp] = 0;
		strip_it(devname);

		if ((dev = __dev_get_by_name(devname)) == NULL)
			return -ENODEV;

		if (dev->dn_ptr == NULL)
			return -ENODEV;

		decnet_default_device = dev;
		filp->f_pos += *lenp;

		return 0;
	}

	if (dev == NULL) {
		*lenp = 0;
		return 0;
	}

	strcpy(devname, dev->name);
	len = strlen(devname);
	devname[len++] = '\n';

	if (len > *lenp) len = *lenp;

	if (copy_to_user(buffer, devname, len))
		return -EFAULT;

	*lenp = len;
	filp->f_pos += len;

	return 0;
}

static ctl_table dn_table[] = {
	{NET_DECNET_NODE_ADDRESS, "node_address", NULL, 7, 0644, NULL,
	dn_node_address_handler, dn_node_address_strategy, NULL,
	NULL, NULL},
	{NET_DECNET_NODE_NAME, "node_name", node_name, 7, 0644, NULL,
	&proc_dostring, &sysctl_string, NULL, NULL, NULL},
	{NET_DECNET_DEFAULT_DEVICE, "default_device", NULL, 16, 0644, NULL,
	dn_def_dev_handler, dn_def_dev_strategy, NULL, NULL, NULL},
	{NET_DECNET_TIME_WAIT, "time_wait", &decnet_time_wait,
	sizeof(int), 0644,
	NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	&min_decnet_time_wait, &max_decnet_time_wait},
	{NET_DECNET_DN_COUNT, "dn_count", &decnet_dn_count,
	sizeof(int), 0644,
	NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	&min_state_count, &max_state_count},
	{NET_DECNET_DI_COUNT, "di_count", &decnet_di_count,
	sizeof(int), 0644,
	NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	&min_state_count, &max_state_count},
	{NET_DECNET_DR_COUNT, "dr_count", &decnet_dr_count,
	sizeof(int), 0644,
	NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	&min_state_count, &max_state_count},
	{NET_DECNET_DST_GC_INTERVAL, "dst_gc_interval", &decnet_dst_gc_interval,
	sizeof(int), 0644,
	NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	&min_decnet_dst_gc_interval, &max_decnet_dst_gc_interval},
	{NET_DECNET_NO_FC_MAX_CWND, "no_fc_max_cwnd", &decnet_no_fc_max_cwnd,
	sizeof(int), 0644,
	NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	&min_decnet_no_fc_max_cwnd, &max_decnet_no_fc_max_cwnd},
	{NET_DECNET_DEBUG_LEVEL, "debug", &decnet_debug_level, 
	sizeof(int), 0644, 
	NULL, &proc_dointvec, &sysctl_intvec, NULL,
	NULL, NULL},
	{0}
};

static ctl_table dn_dir_table[] = {
	{NET_DECNET, "decnet", NULL, 0, 0555, dn_table},
	{0}
};

static ctl_table dn_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, dn_dir_table},
	{0}
};

void dn_register_sysctl(void)
{
	dn_table_header = register_sysctl_table(dn_root_table, 1);
}

void dn_unregister_sysctl(void)
{
	unregister_sysctl_table(dn_table_header);
}

#else  /* CONFIG_SYSCTL */
void dn_unregister_sysctl(void)
{
}
void dn_register_sysctl(void)
{
}

#endif
