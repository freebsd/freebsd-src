/* -*- linux-c -*-
 * sysctl_net_ax25.c: sysctl interface to net AX.25 subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/ax25 directory entry (empty =) ). [MS]
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <net/ax25.h>

static int min_ipdefmode[] = {0},	max_ipdefmode[] = {1};
static int min_axdefmode[] = {0},	max_axdefmode[] = {1};
static int min_backoff[] = {0},		max_backoff[] = {2};
static int min_conmode[] = {0},		max_conmode[] = {2};
static int min_window[] = {1},		max_window[] = {7};
static int min_ewindow[] = {1},		max_ewindow[] = {63};
static int min_t1[] = {1},		max_t1[] = {30 * HZ};
static int min_t2[] = {1},		max_t2[] = {20 * HZ};
static int min_t3[] = {0},		max_t3[] = {3600 * HZ};
static int min_idle[] = {0},		max_idle[] = {65535 * HZ};
static int min_n2[] = {1},		max_n2[] = {31};
static int min_paclen[] = {1},		max_paclen[] = {512};
static int min_proto[] = {0},		max_proto[] = {3};
static int min_ds_timeout[] = {0},	max_ds_timeout[] = {65535 * HZ};

static struct ctl_table_header *ax25_table_header;

static ctl_table *ax25_table;
static int ax25_table_size;

static ctl_table ax25_dir_table[] = {
	{NET_AX25, "ax25", NULL, 0, 0555, NULL},
	{0}
};

static ctl_table ax25_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, ax25_dir_table},
	{0}
};

static const ctl_table ax25_param_table[] = {
	{NET_AX25_IP_DEFAULT_MODE, "ip_default_mode",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_ipdefmode, &max_ipdefmode},
	{NET_AX25_DEFAULT_MODE, "ax25_default_mode",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_axdefmode, &max_axdefmode},
	{NET_AX25_BACKOFF_TYPE, "backoff_type",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_backoff, &max_backoff},
	{NET_AX25_CONNECT_MODE, "connect_mode",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_conmode, &max_conmode},
	{NET_AX25_STANDARD_WINDOW, "standard_window_size",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_window, &max_window},
	{NET_AX25_EXTENDED_WINDOW, "extended_window_size",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_ewindow, &max_ewindow},
	{NET_AX25_T1_TIMEOUT, "t1_timeout",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_t1, &max_t1},
	{NET_AX25_T2_TIMEOUT, "t2_timeout",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_t2, &max_t2},
	{NET_AX25_T3_TIMEOUT, "t3_timeout",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_t3, &max_t3},
	{NET_AX25_IDLE_TIMEOUT, "idle_timeout",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_idle, &max_idle},
	{NET_AX25_N2, "maximum_retry_count",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_n2, &max_n2},
	{NET_AX25_PACLEN, "maximum_packet_length",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_paclen, &max_paclen},
	{NET_AX25_PROTOCOL, "protocol",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_proto, &max_proto},
	{NET_AX25_DAMA_SLAVE_TIMEOUT, "dama_slave_timeout",
	 NULL, sizeof(int), 0644, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 &min_ds_timeout, &max_ds_timeout},
	{0}	/* that's all, folks! */
};

void ax25_register_sysctl(void)
{
	ax25_dev *ax25_dev;
	int n, k;

	for (ax25_table_size = sizeof(ctl_table), ax25_dev = ax25_dev_list; ax25_dev != NULL; ax25_dev = ax25_dev->next)
		ax25_table_size += sizeof(ctl_table);

	if ((ax25_table = kmalloc(ax25_table_size, GFP_ATOMIC)) == NULL)
		return;

	memset(ax25_table, 0x00, ax25_table_size);

	for (n = 0, ax25_dev = ax25_dev_list; ax25_dev != NULL; ax25_dev = ax25_dev->next) {
		ctl_table *child = kmalloc(sizeof(ax25_param_table), GFP_ATOMIC);
		if (!child) {
			while (n--)
				kfree(ax25_table[n].child);
			kfree(ax25_table);
			return;
		}
		memcpy(child, ax25_param_table, sizeof(ax25_param_table));
		ax25_table[n].child = ax25_dev->systable = child;
		ax25_table[n].ctl_name     = n + 1;
		ax25_table[n].procname     = ax25_dev->dev->name;
		ax25_table[n].mode         = 0555;

#ifndef CONFIG_AX25_DAMA_SLAVE
		/* 
		 * We do not wish to have a representation of this parameter
		 * in /proc/sys/ when configured *not* to include the
		 * AX.25 DAMA slave code, do we?
		 */

		child[AX25_VALUES_DS_TIMEOUT].procname = NULL;
#endif

		child[AX25_MAX_VALUES].ctl_name = 0;	/* just in case... */

		for (k = 0; k < AX25_MAX_VALUES; k++)
			child[k].data = &ax25_dev->values[k];

		n++;
	}

	ax25_dir_table[0].child = ax25_table;

	ax25_table_header = register_sysctl_table(ax25_root_table, 1);
}

void ax25_unregister_sysctl(void)
{
	ctl_table *p;
	unregister_sysctl_table(ax25_table_header);

	ax25_dir_table[0].child = NULL;
	for (p = ax25_table; p->ctl_name; p++)
		kfree(p->child);
	kfree(ax25_table);
}
