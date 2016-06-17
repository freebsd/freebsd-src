/*
 *   arch/ppc64/kernel/rtas-proc.c
 *   Copyright (C) 2000 Tilmann Bitterberg
 *   (tilmann@bitterberg.de)
 *
 *   RTAS (Runtime Abstraction Services) stuff
 *   Intention is to provide a clean user interface
 *   to use the RTAS.
 *
 *   TODO:
 *   Split off a header file and maybe move it to a different
 *   location. Write Documentation on what the /proc/rtas/ entries
 *   actually do.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/time.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/rtas.h>
#include <asm/machdep.h> /* for ppc_md */
#include <asm/time.h>

/* Token for Sensors */
#define KEY_SWITCH		0x0001
#define ENCLOSURE_SWITCH	0x0002
#define THERMAL_SENSOR		0x0003
#define LID_STATUS		0x0004
#define POWER_SOURCE		0x0005
#define BATTERY_VOLTAGE		0x0006
#define BATTERY_REMAINING	0x0007
#define BATTERY_PERCENTAGE	0x0008
#define EPOW_SENSOR		0x0009
#define BATTERY_CYCLESTATE	0x000a
#define BATTERY_CHARGING	0x000b

/* IBM specific sensors */
#define IBM_SURVEILLANCE	0x2328 /* 9000 */
#define IBM_FANRPM		0x2329 /* 9001 */
#define IBM_VOLTAGE		0x232a /* 9002 */
#define IBM_DRCONNECTOR		0x232b /* 9003 */
#define IBM_POWERSUPPLY		0x232c /* 9004 */
#define IBM_INTQUEUE		0x232d /* 9005 */

/* Status return values */
#define SENSOR_CRITICAL_HIGH	13
#define SENSOR_WARNING_HIGH	12
#define SENSOR_NORMAL		11
#define SENSOR_WARNING_LOW	10
#define SENSOR_CRITICAL_LOW	 9
#define SENSOR_SUCCESS		 0
#define SENSOR_HW_ERROR		-1
#define SENSOR_BUSY		-2
#define SENSOR_NOT_EXIST	-3
#define SENSOR_DR_ENTITY	-9000

/* Location Codes */
#define LOC_SCSI_DEV_ADDR	'A'
#define LOC_SCSI_DEV_LOC	'B'
#define LOC_CPU			'C'
#define LOC_DISKETTE		'D'
#define LOC_ETHERNET		'E'
#define LOC_FAN			'F'
#define LOC_GRAPHICS		'G'
/* reserved / not used		'H' */
#define LOC_IO_ADAPTER		'I'
/* reserved / not used		'J' */
#define LOC_KEYBOARD		'K'
#define LOC_LCD			'L'
#define LOC_MEMORY		'M'
#define LOC_NV_MEMORY		'N'
#define LOC_MOUSE		'O'
#define LOC_PLANAR		'P'
#define LOC_OTHER_IO		'Q'
#define LOC_PARALLEL		'R'
#define LOC_SERIAL		'S'
#define LOC_DEAD_RING		'T'
#define LOC_RACKMOUNTED		'U' /* for _u_nit is rack mounted */
#define LOC_VOLTAGE		'V'
#define LOC_SWITCH_ADAPTER	'W'
#define LOC_OTHER		'X'
#define LOC_FIRMWARE		'Y'
#define LOC_SCSI		'Z'

/* Tokens for indicators */
#define TONE_FREQUENCY		0x0001 /* 0 - 1000 (HZ)*/
#define TONE_VOLUME		0x0002 /* 0 - 100 (%) */
#define SYSTEM_POWER_STATE	0x0003 
#define WARNING_LIGHT		0x0004
#define DISK_ACTIVITY_LIGHT	0x0005
#define HEX_DISPLAY_UNIT	0x0006
#define BATTERY_WARNING_TIME	0x0007
#define CONDITION_CYCLE_REQUEST	0x0008
#define SURVEILLANCE_INDICATOR	0x2328 /* 9000 */
#define DR_ACTION		0x2329 /* 9001 */
#define DR_INDICATOR		0x232a /* 9002 */
/* 9003 - 9004: Vendor specific */
#define GLOBAL_INTERRUPT_QUEUE	0x232d /* 9005 */
/* 9006 - 9999: Vendor specific */

/* other */
#define MAX_SENSORS		 17  /* I only know of 17 sensors */    
#define MAX_LINELENGTH          256
#define SENSOR_PREFIX		"ibm,sensor-"
#define cel_to_fahr(x)		((x*9/5)+32)


/* Globals */
static struct rtas_sensors sensors;
static struct device_node *rtas_node = NULL;
static unsigned long power_on_time = 0; /* Save the time the user set */
static char progress_led[MAX_LINELENGTH];

static unsigned long rtas_tone_frequency = 1000;
static unsigned long rtas_tone_volume = 0;
static unsigned int open_token = 0;

static int set_time_for_power_on = RTAS_UNKNOWN_SERVICE;
static int set_time_of_day = RTAS_UNKNOWN_SERVICE;
static int get_sensor_state = RTAS_UNKNOWN_SERVICE;
static int set_indicator = RTAS_UNKNOWN_SERVICE;

extern struct proc_dir_entry *proc_ppc64_root;
extern struct proc_dir_entry *rtas_proc_dir;
extern spinlock_t proc_ppc64_lock;

/* ****************STRUCTS******************************************* */
struct individual_sensor {
	unsigned int token;
	unsigned int quant;
};

struct rtas_sensors {
        struct individual_sensor sensor[MAX_SENSORS];
	unsigned int quant;
};

/* ****************************************************************** */
/* Declarations */
static int ppc_rtas_sensor_read(char * buf, char ** start, off_t off,
		int count, int *eof, void *data);
static ssize_t ppc_rtas_clock_read(struct file * file, char * buf, 
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_clock_write(struct file * file, const char * buf, 
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_progress_read(struct file * file, char * buf,
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_progress_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_poweron_read(struct file * file, char * buf,
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_poweron_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos);

static ssize_t ppc_rtas_tone_freq_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_tone_freq_read(struct file * file, char * buf,
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_tone_volume_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos);
static ssize_t ppc_rtas_tone_volume_read(struct file * file, char * buf,
		size_t count, loff_t *ppos);
static int ppc_rtas_errinjct_open(struct inode *inode, struct file *file);
static int ppc_rtas_errinjct_release(struct inode *inode, struct file *file);
static ssize_t ppc_rtas_errinjct_write(struct file * file, const char * buf,
				   size_t count, loff_t *ppos);
static ssize_t ppc_rtas_errinjct_read(struct file *file, char *buf,
				      size_t count, loff_t *ppos);

struct file_operations ppc_rtas_poweron_operations = {
	.read =		ppc_rtas_poweron_read,
	.write =	ppc_rtas_poweron_write
};
struct file_operations ppc_rtas_progress_operations = {
	.read =		ppc_rtas_progress_read,
	.write =	ppc_rtas_progress_write
};

struct file_operations ppc_rtas_clock_operations = {
	.read =		ppc_rtas_clock_read,
	.write =	ppc_rtas_clock_write
};

struct file_operations ppc_rtas_tone_freq_operations = {
	.read =		ppc_rtas_tone_freq_read,
	.write =	ppc_rtas_tone_freq_write
};
struct file_operations ppc_rtas_tone_volume_operations = {
	.read =		ppc_rtas_tone_volume_read,
	.write =	ppc_rtas_tone_volume_write
};

struct file_operations ppc_rtas_errinjct_operations = {
    .open =		ppc_rtas_errinjct_open,
    .read = 		ppc_rtas_errinjct_read,
    .write = 		ppc_rtas_errinjct_write,
    .release = 		ppc_rtas_errinjct_release
};

int ppc_rtas_find_all_sensors (void);
int ppc_rtas_process_sensor(struct individual_sensor s, int state, 
		int error, char * buf);
char * ppc_rtas_process_error(int error);
int get_location_code(struct individual_sensor s, char * buf);
int check_location_string (char *c, char * buf);
int check_location (char *c, int idx, char * buf);

/* ****************************************************************** */
/* MAIN                                                               */
/* ****************************************************************** */
void proc_rtas_init(void)
{
	struct proc_dir_entry *entry;
	int display_character;
	int errinjct_token;

	rtas_node = find_devices("rtas");
	if ((rtas_node == NULL) || (systemcfg->platform == PLATFORM_ISERIES_LPAR)) {
		return;
	}
	
	spin_lock(&proc_ppc64_lock);
	if (proc_ppc64_root == NULL) {
		proc_ppc64_root = proc_mkdir("ppc64", 0);
		if (!proc_ppc64_root) {
			spin_unlock(&proc_ppc64_lock);
			return;
		}		
	}
	spin_unlock(&proc_ppc64_lock);
	
	if (rtas_proc_dir == NULL) {
		rtas_proc_dir = proc_mkdir("rtas", proc_ppc64_root);
	}

	if (rtas_proc_dir == NULL) {
		printk(KERN_ERR "Failed to create /proc/ppc64/rtas in rtas_init\n");
		return;
	}

	/*
	 * /proc/rtas entries
	 * only create entries if rtas token exists for desired function
	 */

	set_time_of_day = rtas_token("set-time-of-day");
	if (set_time_of_day != RTAS_UNKNOWN_SERVICE) {
		entry=create_proc_entry("clock",S_IRUGO|S_IWUSR,rtas_proc_dir);
		if (entry) entry->proc_fops = &ppc_rtas_clock_operations;
	}

	set_time_for_power_on = rtas_token("set-time-for-power-on");
	if (set_time_for_power_on != RTAS_UNKNOWN_SERVICE) {
		entry=create_proc_entry("poweron",S_IWUSR|S_IRUGO,rtas_proc_dir);
		if (entry) entry->proc_fops = &ppc_rtas_poweron_operations;
	}

	get_sensor_state = rtas_token("get-sensor-state");
	if (get_sensor_state != RTAS_UNKNOWN_SERVICE) {
		create_proc_read_entry("sensors", S_IRUGO, rtas_proc_dir,
				       ppc_rtas_sensor_read, NULL);
	}

	set_indicator = rtas_token("set-indicator");
	if (set_indicator != RTAS_UNKNOWN_SERVICE) {
		entry=create_proc_entry("frequency",S_IWUSR|S_IRUGO,rtas_proc_dir);
		if (entry) entry->proc_fops = &ppc_rtas_tone_freq_operations;

		entry=create_proc_entry("volume",S_IWUSR|S_IRUGO,rtas_proc_dir);
		if (entry) entry->proc_fops = &ppc_rtas_tone_volume_operations;
	}

	display_character = rtas_token("display-character");
	if ((display_character != RTAS_UNKNOWN_SERVICE) ||
	    (set_indicator != RTAS_UNKNOWN_SERVICE)) {
		entry=create_proc_entry("progress",S_IRUGO|S_IWUSR,rtas_proc_dir);
		if (entry) entry->proc_fops = &ppc_rtas_progress_operations;
	}

#ifdef CONFIG_RTAS_ERRINJCT
	errinjct_token = rtas_token("ibm,errinjct");
	if (errinjct_token != RTAS_UNKNOWN_SERVICE) {
		entry=create_proc_entry("errinjct",S_IWUSR|S_IRUGO,rtas_proc_dir);
		if (entry) entry->proc_fops = &ppc_rtas_errinjct_operations;
	}
#endif

}

/* ****************************************************************** */
/* POWER-ON-TIME                                                      */
/* ****************************************************************** */
static ssize_t ppc_rtas_poweron_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos)
{
	char stkbuf[40];  /* its small, its on stack */
	struct rtc_time tm;
	unsigned long nowtime;
	char *dest;
	int error;

	if (39 < count)
		count = 39;
	if (copy_from_user(stkbuf, buf, count))
		return -EFAULT;

	stkbuf[count] = 0;
	nowtime = simple_strtoul(stkbuf, &dest, 10);
	if (*dest != '\0' && *dest != '\n') {
		printk("ppc_rtas_poweron_write: Invalid time\n");
		return count;
	}
	power_on_time = nowtime; /* save the time */

	to_tm(nowtime, &tm);

	error = rtas_call(set_time_for_power_on, 7, 1, NULL,
			tm.tm_year, tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, 0 /* nano */);
	if (error != 0)
		printk(KERN_WARNING "error: setting poweron time returned: %s\n",
				ppc_rtas_process_error(error));
	return count;
}
/* ****************************************************************** */
static ssize_t ppc_rtas_poweron_read(struct file * file, char * buf,
		size_t count, loff_t *ppos)
{
	char stkbuf[40];  /* its small, its on stack */
	int n;

	if (power_on_time == 0)
		n = snprintf(stkbuf, 40, "Power on time not set\n");
	else
		n = snprintf(stkbuf, 40, "%lu\n", power_on_time);

	int sn = strlen(stkbuf) +1;
	if (*ppos >= sn)
		return 0;
	if (n > sn - *ppos)
		n = sn - *ppos;
	if (n > count)
		n = count;
	if (copy_to_user(buf, stkbuf + (*ppos), n))
		return -EFAULT;
	*ppos += n;
	return n;
}

/* ****************************************************************** */
/* PROGRESS                                                           */
/* ****************************************************************** */
static ssize_t ppc_rtas_progress_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos)
{
	unsigned long hex;

	if (count >= MAX_LINELENGTH)
		count = MAX_LINELENGTH -1;
	if (copy_from_user(progress_led, buf, count))
		return -EFAULT;

	progress_led[count] = 0;

	/* Lets see if the user passed hexdigits */
	hex = simple_strtoul(progress_led, NULL, 10);

	ppc_md.progress((char *)progress_led, hex);
	return count;

	/* clear the line */ /* ppc_md.progress("                   ", 0xffff);*/
}
/* ****************************************************************** */
static ssize_t ppc_rtas_progress_read(struct file * file, char * buf,
		size_t count, loff_t *ppos)
{
	int n = 0, sn;
	
	if (progress_led == NULL)
		return 0;

	char * tmpbuf = kmalloc(MAX_LINELENGTH, GFP_KERNEL);
	if (!tmpbuf) {
		printk(KERN_ERR "error: kmalloc failed\n");
		return -ENOMEM;
	}
	n = sprintf (tmpbuf, "%s\n", progress_led);

	sn = strlen (tmpbuf) +1;
	if (*ppos >= sn) {
		kfree(tmpbuf);
		return 0;
	}
	if (n > sn - *ppos)
		n = sn - *ppos;
	if (n > count)
		n = count;
	if (copy_to_user(buf, tmpbuf + (*ppos), n)) {
		kfree(tmpbuf);
		return -EFAULT;
	}
	kfree(tmpbuf);
	*ppos += n;
	return n;
}

/* ****************************************************************** */
/* CLOCK                                                              */
/* ****************************************************************** */
static ssize_t ppc_rtas_clock_write(struct file * file, const char * buf, 
		size_t count, loff_t *ppos)
{
	char stkbuf[40];  /* its small, its on stack */
	struct rtc_time tm;
	unsigned long nowtime;
	char *dest;
	int error;

	if (39 < count)
		count = 39;
	if (copy_from_user(stkbuf, buf, count))
		return -EFAULT;

	stkbuf[count] = 0;
	nowtime = simple_strtoul(stkbuf, &dest, 10);
	if (*dest != '\0' && *dest != '\n') {
		printk("ppc_rtas_clock_write: Invalid time\n");
		return count;
	}

	to_tm(nowtime, &tm);
	error = rtas_call(rtas_token("set-time-of-day"), 7, 1, NULL, 
			tm.tm_year, tm.tm_mon, tm.tm_mday, 
			tm.tm_hour, tm.tm_min, tm.tm_sec, 0);
	if (error != 0)
		printk(KERN_WARNING "error: setting the clock returned: %s\n", 
				ppc_rtas_process_error(error));
	return count;
}
/* ****************************************************************** */
static ssize_t ppc_rtas_clock_read(struct file * file, char * buf, 
		size_t count, loff_t *ppos)
{
	unsigned int year, mon, day, hour, min, sec;
	unsigned long *ret = kmalloc(4*8, GFP_KERNEL);
	int n, error;

	error = rtas_call(rtas_token("get-time-of-day"), 0, 8, ret);
	
	year = ret[0]; mon  = ret[1]; day  = ret[2];
	hour = ret[3]; min  = ret[4]; sec  = ret[5];

	char stkbuf[40];  /* its small, its on stack */

	if (error != 0){
		printk(KERN_WARNING "error: reading the clock returned: %s\n", 
				ppc_rtas_process_error(error));
		n = snprintf(stkbuf, 40, "0");
	} else { 
		n = snprintf(stkbuf, 40, "%lu\n", mktime(year, mon, day, hour, min, sec));
	}
	kfree(ret);

	int sn = strlen(stkbuf) +1;
	if (*ppos >= sn)
		return 0;
	if (n > sn - *ppos)
		n = sn - *ppos;
	if (n > count)
		n = count;
	if (copy_to_user(buf, stkbuf + (*ppos), n))
		return -EFAULT;

	*ppos += n;
	return n;
}

/* ****************************************************************** */
/* SENSOR STUFF                                                       */
/* ****************************************************************** */
static int ppc_rtas_sensor_read(char * buf, char ** start, off_t off,
		int count, int *eof, void *data)
{
	int i,j,n;
	unsigned long ret;
	int state, error;
	char *buffer;

	if (count < 0)
		return -EINVAL;

	/* May not be enough */
	buffer = kmalloc(MAX_LINELENGTH*MAX_SENSORS, GFP_KERNEL);

	if (!buffer)
		return -ENOMEM;

	memset(buffer, 0, MAX_LINELENGTH*MAX_SENSORS);

	n  = sprintf ( buffer  , "RTAS (RunTime Abstraction Services) Sensor Information\n");
	n += sprintf ( buffer+n, "%-17s\t%-15s\t%-15s\tLocation\n", "Sensor", "Value", "Condition");
	n += sprintf ( buffer+n, "***************************************************************************\n");

	if (ppc_rtas_find_all_sensors() != 0) {
		n += sprintf ( buffer+n, "\nNo sensors are available\n");
		goto return_string;
	}

	for (i=0; i<sensors.quant; i++) {
		j = sensors.sensor[i].quant;
		/* A sensor may have multiple instances */
		while (j >= 0) {

			error =	rtas_call(get_sensor_state, 2, 2, &ret, 
				  	  sensors.sensor[i].token, 
				  	  sensors.sensor[i].quant - j);

			state = (int) ret;
			n += ppc_rtas_process_sensor(sensors.sensor[i], state, 
					     	     error, buffer+n );
			n += sprintf (buffer+n, "\n");
			j--;
		} /* while */
	} /* for */

return_string:
	if (off >= strlen(buffer)) {
		*eof = 1;
		kfree(buffer);
		return 0;
	}
	if (n > strlen(buffer) - off)
		n = strlen(buffer) - off;
	if (n > count)
		n = count;
	else
		*eof = 1;

	memcpy(buf, buffer + off, n);
	*start = buf;
	kfree(buffer);
	return n;
}

/* ****************************************************************** */

int ppc_rtas_find_all_sensors (void)
{
	unsigned int *utmp;
	int len, i;

	utmp = (unsigned int *) get_property(rtas_node, "rtas-sensors", &len);
	if (utmp == NULL) {
		printk (KERN_ERR "error: could not get rtas-sensors\n");
		return 1;
	}

	sensors.quant = len / 8;      /* int + int */

	for (i=0; i<sensors.quant; i++) {
		sensors.sensor[i].token = *utmp++;
		sensors.sensor[i].quant = *utmp++;
	}
	return 0;
}

/* ****************************************************************** */
/*
 * Builds a string of what rtas returned
 */
char * ppc_rtas_process_error(int error)
{
	switch (error) {
		case SENSOR_CRITICAL_HIGH:
			return "(critical high)";
		case SENSOR_WARNING_HIGH:
			return "(warning high)";
		case SENSOR_NORMAL:
			return "(normal)";
		case SENSOR_WARNING_LOW:
			return "(warning low)";
		case SENSOR_CRITICAL_LOW:
			return "(critical low)";
		case SENSOR_SUCCESS:
			return "(read ok)";
		case SENSOR_HW_ERROR:
			return "(hardware error)";
		case SENSOR_BUSY:
			return "(busy)";
		case SENSOR_NOT_EXIST:
			return "(non existant)";
		case SENSOR_DR_ENTITY:
			return "(dr entity removed)";
		default:
			return "(UNKNOWN)";
	}
}

/* ****************************************************************** */
/*
 * Builds a string out of what the sensor said
 */

int ppc_rtas_process_sensor(struct individual_sensor s, int state, 
		int error, char * buf) 
{
	/* Defined return vales */
	const char * key_switch[]        = { "Off", "Normal", "Secure", "Maintenance" };
	const char * enclosure_switch[]  = { "Closed", "Open" };
	const char * lid_status[]        = { " ", "Open", "Closed" };
	const char * power_source[]      = { "AC", "Battery", "AC & Battery" };
	const char * battery_remaining[] = { "Very Low", "Low", "Mid", "High" };
	const char * epow_sensor[]       = { 
		"EPOW Reset", "Cooling warning", "Power warning",
		"System shutdown", "System halt", "EPOW main enclosure",
		"EPOW power off" };
	const char * battery_cyclestate[]  = { "None", "In progress", "Requested" };
	const char * battery_charging[]    = { "Charging", "Discharching", "No current flow" };
	const char * ibm_drconnector[]     = { "Empty", "Present" };
	const char * ibm_intqueue[]        = { "Disabled", "Enabled" };

	int temperature = 0;
	int unknown = 0;
	int n = 0;
	char *label_string = NULL;
	const char **value_arr = NULL;
	int value_arrsize = 0;

	/* What kind of sensor do we have here? */
	
	switch (s.token) {
		case KEY_SWITCH:
			label_string = "Key switch:";
			value_arrsize = sizeof(key_switch)/sizeof(char *);
			value_arr = key_switch;
			break;
		case ENCLOSURE_SWITCH:
			label_string = "Enclosure switch:";
			value_arrsize = sizeof(enclosure_switch)/sizeof(char *);
			value_arr = enclosure_switch;
			break;
		case THERMAL_SENSOR:
			label_string = "Temp. (°C/°F):";
			temperature = 1;
			break;
		case LID_STATUS:
			label_string = "Lid status:";
			value_arrsize = sizeof(lid_status)/sizeof(char *);
			value_arr = lid_status;
			break;
		case POWER_SOURCE:
			label_string = "Power source:";
			value_arrsize = sizeof(power_source)/sizeof(char *);
			value_arr = power_source;
			break;
		case BATTERY_VOLTAGE:
			label_string = "Battery voltage:";
			break;
		case BATTERY_REMAINING:
			label_string = "Battery remaining:";
			value_arrsize = sizeof(battery_remaining)/sizeof(char *);
			value_arr = battery_remaining;
			break;
		case BATTERY_PERCENTAGE:
			label_string = "Battery percentage:";
			break;
		case EPOW_SENSOR:
			label_string = "EPOW Sensor:";
			value_arrsize = sizeof(epow_sensor)/sizeof(char *);
			value_arr = epow_sensor;
			break;
		case BATTERY_CYCLESTATE:
			label_string = "Battery cyclestate:";
			value_arrsize = sizeof(battery_cyclestate)/sizeof(char *);
			value_arr = battery_cyclestate;
			break;
		case BATTERY_CHARGING:
			label_string = "Battery Charging:";
			value_arrsize = sizeof(battery_charging)/sizeof(char *);
			value_arr = battery_charging;
			break;
		case IBM_SURVEILLANCE:
			label_string = "Surveillance:";
			break;
		case IBM_FANRPM:
			label_string = "Fan (rpm):";
			break;
		case IBM_VOLTAGE:
			label_string = "Voltage (mv):";
			break;
		case IBM_DRCONNECTOR:
			label_string = "DR connector:";
			value_arrsize = sizeof(ibm_drconnector)/sizeof(char *);
			value_arr = ibm_drconnector;
			break;
		case IBM_POWERSUPPLY:
			label_string = "Powersupply:";
			break;
		case IBM_INTQUEUE:
			label_string = "Interrupt queue:";
			value_arrsize = sizeof(ibm_intqueue)/sizeof(char *);
			value_arr = ibm_intqueue;
			break;
		default:
			n += sprintf(buf+n,  "Unkown sensor (type %d), ignoring it\n",
					s.token);
			unknown = 1;
			break;
	}

	if (label_string)
		n += sprintf(buf+n, "%-17s\t", label_string);

	if (value_arr && state >= 0 && state < value_arrsize) {
		n += sprintf(buf+n, "%-15s\t", value_arr[state]);
	} else {
		if (temperature) {
			n += sprintf(buf+n, "%2d / %2d  \t", state, cel_to_fahr(state));
		} else
			n += sprintf(buf+n, "%-10d\t", state);
	}
	if (unknown == 0) {
		n += sprintf ( buf+n, "%-15s\t", ppc_rtas_process_error(error));
		n += get_location_code(s, buf+n);
	}
	return n;
}

/* ****************************************************************** */

int check_location (char *c, int idx, char * buf)
{
	int n = 0;

	switch (*(c+idx)) {
		case LOC_PLANAR:
			n += sprintf ( buf, "Planar #%c", *(c+idx+1));
			break;
		case LOC_CPU:
			n += sprintf ( buf, "CPU #%c", *(c+idx+1));
			break;
		case LOC_FAN:
			n += sprintf ( buf, "Fan #%c", *(c+idx+1));
			break;
		case LOC_RACKMOUNTED:
			n += sprintf ( buf, "Rack #%c", *(c+idx+1));
			break;
		case LOC_VOLTAGE:
			n += sprintf ( buf, "Voltage #%c", *(c+idx+1));
			break;
		case LOC_LCD:
			n += sprintf ( buf, "LCD #%c", *(c+idx+1));
			break;
		case '.':
			n += sprintf ( buf, "- %c", *(c+idx+1));
		default:
			n += sprintf ( buf, "Unknown location");
			break;
	}
	return n;
}


/* ****************************************************************** */
/* 
 * Format: 
 * ${LETTER}${NUMBER}[[-/]${LETTER}${NUMBER} [ ... ] ]
 * the '.' may be an abbrevation
 */
int check_location_string (char *c, char *buf)
{
	int n=0,i=0;

	while (c[i]) {
		if (isalpha(c[i]) || c[i] == '.') {
			 n += check_location(c, i, buf+n);
		}
		else if (c[i] == '/' || c[i] == '-')
			n += sprintf(buf+n, " at ");
		i++;
	}
	return n;
}


/* ****************************************************************** */

int get_location_code(struct individual_sensor s, char * buffer)
{
	char rstr[512], tmp[10], tmp2[10];
	int n=0, i=0, llen, len;
	/* char *buf = kmalloc(MAX_LINELENGTH, GFP_KERNEL); */
	char *ret;

	static int pos = 0; /* remember position where buffer was */

	/* construct the sensor number like 0003 */
	/* fill with zeros */
	n = sprintf(tmp, "%d", s.token);
	len = strlen(tmp);
	while (strlen(tmp) < 4)
		n += sprintf (tmp+n, "0");
	
	/* invert the string */
	while (tmp[i]) {
		if (i<len)
			tmp2[4-len+i] = tmp[i];
		else
			tmp2[3-i] = tmp[i];
		i++;
	}
	tmp2[4] = '\0';

	sprintf (rstr, SENSOR_PREFIX"%s", tmp2);

	ret = (char *) get_property(rtas_node, rstr, &llen);

	n=0;
	if (ret == NULL || ret[0] == '\0') {
		n += sprintf ( buffer+n, "--- ");/* does not have a location */
	} else {
		char t[50];
		ret += pos;

		n += check_location_string(ret, buffer + n);
		n += sprintf ( buffer+n, " ");
		/* see how many characters we have printed */
		snprintf( t, 50, "%s ", ret);

		pos += strlen(t);
		if (pos >= llen) pos=0;
	}
	return n;
}
/* ****************************************************************** */
/* INDICATORS - Tone Frequency                                        */
/* ****************************************************************** */
static ssize_t ppc_rtas_tone_freq_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos)
{
	char stkbuf[40];  /* its small, its on stack */
	unsigned long freq;
	char *dest;
	int error;

	if (39 < count)
		count = 39;
	if (copy_from_user(stkbuf, buf, count))
		return -EFAULT;

	stkbuf[count] = 0;
	freq = simple_strtoul(stkbuf, &dest, 10);
	if (*dest != '\0' && *dest != '\n') {
		printk("ppc_rtas_tone_freq_write: Invalid tone freqency\n");
		return count;
	}
	if (freq < 0) freq = 0;
	rtas_tone_frequency = freq; /* save it for later */
	error = rtas_call(set_indicator, 3, 1, NULL,
			TONE_FREQUENCY, 0, freq);
	if (error != 0)
		printk(KERN_WARNING "error: setting tone frequency returned: %s\n", 
				ppc_rtas_process_error(error));
	return count;
}
/* ****************************************************************** */
static ssize_t ppc_rtas_tone_freq_read(struct file * file, char * buf,
		size_t count, loff_t *ppos)
{
	int n, sn;
	char stkbuf[40];  /* its small, its on stack */

	n = snprintf(stkbuf, 40, "%lu\n", rtas_tone_frequency);

	sn = strlen(stkbuf) +1;
	if (*ppos >= sn)
		return 0;
	if (n > sn - *ppos)
		n = sn - *ppos;
	if (n > count)
		n = count;
	if (copy_to_user(buf, stkbuf + (*ppos), n))
		return -EFAULT;

	*ppos += n;
	return n;
}
/* ****************************************************************** */
/* INDICATORS - Tone Volume                                           */
/* ****************************************************************** */
static ssize_t ppc_rtas_tone_volume_write(struct file * file, const char * buf,
		size_t count, loff_t *ppos)
{
	char stkbuf[40];  /* its small, its on stack */
	unsigned long volume;
	char *dest;
	int error;

	if (39 < count)
		count = 39;
	if (copy_from_user(stkbuf, buf, count))
		return -EFAULT;

	stkbuf[count] = 0;
	volume = simple_strtoul(stkbuf, &dest, 10);
	if (*dest != '\0' && *dest != '\n') {
		printk("ppc_rtas_tone_volume_write: Invalid tone volume\n");
		return count;
	}
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	
        rtas_tone_volume = volume; /* save it for later */
	error = rtas_call(set_indicator, 3, 1, NULL,
			TONE_VOLUME, 0, volume);
	if (error != 0)
		printk(KERN_WARNING "error: setting tone volume returned: %s\n", 
				ppc_rtas_process_error(error));
	return count;
}
/* ****************************************************************** */
static ssize_t ppc_rtas_tone_volume_read(struct file * file, char * buf,
		size_t count, loff_t *ppos)
{
	int n, sn;
	char stkbuf[40];  /* its small, its on stack */

	n = snprintf(stkbuf, 40, "%lu\n", rtas_tone_volume);
	sn = strlen(stkbuf) +1;
	if (*ppos >= sn)
		return 0;
	if (n > sn - *ppos)
		n = sn - *ppos;
	if (n > count)
		n = count;
	if (copy_to_user(buf, stkbuf + (*ppos), n))
		return -EFAULT;

	*ppos += n;
	return n;
}

/* ****************************************************************** */
/* ERRINJCT			                                      */
/* ****************************************************************** */
static int ppc_rtas_errinjct_open(struct inode *inode, struct file *file)
{
	int rc;

	/* We will only allow one process to use error inject at a
	   time.  Since errinjct is usually only used for testing,
	   this shouldn't be an issue */
	if (open_token) {
		return -EAGAIN;
	}
	rc = rtas_errinjct_open();
	if (rc < 0) {
		return -EIO;
	}
	open_token = rc;

	return 0;
}

static ssize_t ppc_rtas_errinjct_write(struct file * file, const char * buf,
				       size_t count, loff_t *ppos)
{
 	char * tmpbuf;
	char * ei_token;
	char * workspace = NULL;
	size_t max_len;
	int token_len;
	int rc;

	/* Verify the errinjct token length */
	if (count < ERRINJCT_TOKEN_LEN) {
		max_len = count;
	} else {
		max_len = ERRINJCT_TOKEN_LEN;
	}

	tmpbuf = (char *) kmalloc(max_len, GFP_KERNEL);
	if (!tmpbuf) {
		printk(KERN_WARNING "error: kmalloc failed\n");
		return -ENOMEM;
	}
	if (copy_from_user (tmpbuf, buf, max_len)) {
		kfree(tmpbuf);
		return -EFAULT;
	}
	token_len = strnlen(tmpbuf, max_len);
	token_len++; /* Add one for the null termination */
    
	ei_token = (char *)kmalloc(token_len, GFP_KERNEL);
	if (!ei_token) {
		printk(KERN_WARNING "error: kmalloc failed\n");
		kfree(tmpbuf);
		return -ENOMEM;
	}

	strncpy(ei_token, tmpbuf, token_len);
    
	if (count > token_len + WORKSPACE_SIZE) {
		count = token_len + WORKSPACE_SIZE;
	}
    
	buf += token_len;

	/* check if there is a workspace */
	if (count > token_len) {
		/* Verify the workspace size */
		if ((count - token_len) > WORKSPACE_SIZE) {
			max_len = WORKSPACE_SIZE;
		} else {
			max_len = count - token_len;
		}

		workspace = (char *)kmalloc(max_len, GFP_KERNEL);
		if (!workspace) {
			printk(KERN_WARNING "error: failed kmalloc\n");
			kfree(tmpbuf);
			kfree(ei_token);
			return -ENOMEM;
		}
	
		memcpy(workspace, tmpbuf, max_len);
	}

	rc = rtas_errinjct(open_token, ei_token, workspace);

	if (count > token_len) {
		kfree(workspace);
	}
	kfree(ei_token);
	kfree(tmpbuf);

	return rc < 0 ? rc : count;
}

static int ppc_rtas_errinjct_release(struct inode *inode, struct file *file)
{
	int rc;
    
	rc = rtas_errinjct_close(open_token);
	if (rc) {
		return rc;
	}
	open_token = 0;
	return 0;
}

static ssize_t ppc_rtas_errinjct_read(struct file *file, char *buf,
				      size_t count, loff_t *ppos) 
{
	char * buffer;
	int i, sn;
	int n = 0;

	int m = MAX_ERRINJCT_TOKENS * (ERRINJCT_TOKEN_LEN+1);
	buffer = (char *)kmalloc(m, GFP_KERNEL);
	if (!buffer) {
		printk(KERN_ERR "error: kmalloc failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < MAX_ERRINJCT_TOKENS && ei_token_list[i].value; i++) {
		n += snprintf(buffer+n, m-n, ei_token_list[i].name);
		n += snprintf(buffer+n, m-n, "\n");
	}

	sn = strlen(buffer) +1;
	if (*ppos >= sn) {
		kfree(buffer);
		return 0;
	}
	if (n > sn - *ppos)
		n = sn - *ppos;

	if (n > count)
		n = count;

	if (copy_to_user(buf, buffer + *ppos, n)) {
		kfree(buffer);
		return -EFAULT;
	}

	*ppos += n;

	kfree(buffer);
	return n;
}
