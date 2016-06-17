/*
    sensors.h - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef SENSORS_SENSORS_H
#define SENSORS_SENSORS_H

#ifdef __KERNEL__

/* Next two must be included before sysctl.h can be included, in 2.0 kernels */
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysctl.h>

/* The type of callback functions used in sensors_{proc,sysctl}_real */
typedef void (*i2c_real_callback) (struct i2c_client * client,
				       int operation, int ctl_name,
				       int *nrels_mag, long *results);

/* Values for the operation field in the above function type */
#define SENSORS_PROC_REAL_INFO 1
#define SENSORS_PROC_REAL_READ 2
#define SENSORS_PROC_REAL_WRITE 3

/* These funcion reads or writes a 'real' value (encoded by the combination
   of an integer and a magnitude, the last is the power of ten the value
   should be divided with) to a /proc/sys directory. To use these functions,
   you must (before registering the ctl_table) set the extra2 field to the
   client, and the extra1 field to a function of the form:
      void func(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
   This last function can be called for three values of operation. If
   operation equals SENSORS_PROC_REAL_INFO, the magnitude should be returned
   in nrels_mag. If operation equals SENSORS_PROC_REAL_READ, values should
   be read into results. nrels_mag should return the number of elements
   read; the maximum number is put in it on entry. Finally, if operation
   equals SENSORS_PROC_REAL_WRITE, the values in results should be
   written to the chip. nrels_mag contains on entry the number of elements
   found.
   In all cases, client points to the client we wish to interact with,
   and ctl_name is the SYSCTL id of the file we are accessing. */
extern int i2c_sysctl_real(ctl_table * table, int *name, int nlen,
			       void *oldval, size_t * oldlenp,
			       void *newval, size_t newlen,
			       void **context);
extern int i2c_proc_real(ctl_table * ctl, int write, struct file *filp,
			     void *buffer, size_t * lenp);



/* These rather complex functions must be called when you want to add or
   delete an entry in /proc/sys/dev/sensors/chips (not yet implemented). It
   also creates a new directory within /proc/sys/dev/sensors/.
   ctl_template should be a template of the newly created directory. It is
   copied in memory. The extra2 field of each file is set to point to client.
   If any driver wants subdirectories within the newly created directory,
   these functions must be updated! */
extern int i2c_register_entry(struct i2c_client *client,
				  const char *prefix,
				  ctl_table * ctl_template,
				  struct module *controlling_mod);

extern void i2c_deregister_entry(int id);


/* A structure containing detect information.
   Force variables overrule all other variables; they force a detection on
   that place. If a specific chip is given, the module blindly assumes this
   chip type is present; if a general force (kind == 0) is given, the module
   will still try to figure out what type of chip is present. This is useful
   if for some reasons the detect for SMBus or ISA address space filled
   fails.
   probe: insmod parameter. Initialize this list with SENSORS_I2C_END values.
     A list of pairs. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second is the address. 
   kind: The kind of chip. 0 equals any chip.
*/
struct i2c_force_data {
	unsigned short *force;
	unsigned short kind;
};

/* A structure containing the detect information.
   normal_i2c: filled in by the module writer. Terminated by SENSORS_I2C_END.
     A list of I2C addresses which should normally be examined.
   normal_i2c_range: filled in by the module writer. Terminated by 
     SENSORS_I2C_END
     A list of pairs of I2C addresses, each pair being an inclusive range of
     addresses which should normally be examined.
   normal_isa: filled in by the module writer. Terminated by SENSORS_ISA_END.
     A list of ISA addresses which should normally be examined.
   normal_isa_range: filled in by the module writer. Terminated by 
     SENSORS_ISA_END
     A list of triples. The first two elements are ISA addresses, being an
     range of addresses which should normally be examined. The third is the
     modulo parameter: only addresses which are 0 module this value relative
     to the first address of the range are actually considered.
   probe: insmod parameter. Initialize this list with SENSORS_I2C_END values.
     A list of pairs. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second is the address. These
     addresses are also probed, as if they were in the 'normal' list.
   probe_range: insmod parameter. Initialize this list with SENSORS_I2C_END 
     values.
     A list of triples. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second and third are addresses. 
     These form an inclusive range of addresses that are also probed, as
     if they were in the 'normal' list.
   ignore: insmod parameter. Initialize this list with SENSORS_I2C_END values.
     A list of pairs. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second is the I2C address. These
     addresses are never probed. This parameter overrules 'normal' and 
     'probe', but not the 'force' lists.
   ignore_range: insmod parameter. Initialize this list with SENSORS_I2C_END 
      values.
     A list of triples. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second and third are addresses. 
     These form an inclusive range of I2C addresses that are never probed.
     This parameter overrules 'normal' and 'probe', but not the 'force' lists.
   force_data: insmod parameters. A list, ending with an element of which
     the force field is NULL.
*/
struct i2c_address_data {
	unsigned short *normal_i2c;
	unsigned short *normal_i2c_range;
	unsigned int *normal_isa;
	unsigned int *normal_isa_range;
	unsigned short *probe;
	unsigned short *probe_range;
	unsigned short *ignore;
	unsigned short *ignore_range;
	struct i2c_force_data *forces;
};

/* Internal numbers to terminate lists */
#define SENSORS_I2C_END 0xfffe
#define SENSORS_ISA_END 0xfffefffe

/* The numbers to use to set an ISA or I2C bus address */
#define SENSORS_ISA_BUS 9191
#define SENSORS_ANY_I2C_BUS 0xffff

/* The length of the option lists */
#define SENSORS_MAX_OPTS 48

/* Default fill of many variables */
#define SENSORS_DEFAULTS {SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END}

/* This is ugly. We need to evaluate SENSORS_MAX_OPTS before it is 
   stringified */
#define SENSORS_MODPARM_AUX1(x) "1-" #x "h"
#define SENSORS_MODPARM_AUX(x) SENSORS_MODPARM_AUX1(x)
#define SENSORS_MODPARM SENSORS_MODPARM_AUX(SENSORS_MAX_OPTS)

/* SENSORS_MODULE_PARM creates a module parameter, and puts it in the
   module header */
#define SENSORS_MODULE_PARM(var,desc) \
  static unsigned short var[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM(var,SENSORS_MODPARM); \
  MODULE_PARM_DESC(var,desc)

/* SENSORS_MODULE_PARM creates a 'force_*' module parameter, and puts it in
   the module header */
#define SENSORS_MODULE_PARM_FORCE(name) \
  SENSORS_MODULE_PARM(force_ ## name, \
                      "List of adapter,address pairs which are unquestionably" \
                      " assumed to contain a `" # name "' chip")


/* This defines several insmod variables, and the addr_data structure */
#define SENSORS_INSMOD \
  SENSORS_MODULE_PARM(probe, \
                      "List of adapter,address pairs to scan additionally"); \
  SENSORS_MODULE_PARM(probe_range, \
                      "List of adapter,start-addr,end-addr triples to scan " \
                      "additionally"); \
  SENSORS_MODULE_PARM(ignore, \
                      "List of adapter,address pairs not to scan"); \
  SENSORS_MODULE_PARM(ignore_range, \
                      "List of adapter,start-addr,end-addr triples not to " \
                      "scan"); \
  static struct i2c_address_data addr_data = \
                                       {normal_i2c, normal_i2c_range, \
                                        normal_isa, normal_isa_range, \
                                        probe, probe_range, \
                                        ignore, ignore_range, \
                                        forces}

/* The following functions create an enum with the chip names as elements. 
   The first element of the enum is any_chip. These are the only macros
   a module will want to use. */

#define SENSORS_INSMOD_0 \
  enum chips { any_chip }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  static struct i2c_force_data forces[] = {{force,any_chip},{NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_1(chip1) \
  enum chips { any_chip, chip1 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  static struct i2c_force_data forces[] = {{force,any_chip},\
                                                 {force_ ## chip1,chip1}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_2(chip1,chip2) \
  enum chips { any_chip, chip1, chip2 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  static struct i2c_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_3(chip1,chip2,chip3) \
  enum chips { any_chip, chip1, chip2, chip3 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  static struct i2c_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_4(chip1,chip2,chip3,chip4) \
  enum chips { any_chip, chip1, chip2, chip3, chip4 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  static struct i2c_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_5(chip1,chip2,chip3,chip4,chip5) \
  enum chips { any_chip, chip1, chip2, chip3, chip4, chip5 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  SENSORS_MODULE_PARM_FORCE(chip5); \
  static struct i2c_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {force_ ## chip5,chip5}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_6(chip1,chip2,chip3,chip4,chip5,chip6) \
  enum chips { any_chip, chip1, chip2, chip3, chip4, chip5, chip6 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  SENSORS_MODULE_PARM_FORCE(chip5); \
  SENSORS_MODULE_PARM_FORCE(chip6); \
  static struct i2c_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {force_ ## chip5,chip5}, \
                                                 {force_ ## chip6,chip6}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_7(chip1,chip2,chip3,chip4,chip5,chip6,chip7) \
  enum chips { any_chip, chip1, chip2, chip3, chip4, chip5, chip6, chip7 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  SENSORS_MODULE_PARM_FORCE(chip5); \
  SENSORS_MODULE_PARM_FORCE(chip6); \
  SENSORS_MODULE_PARM_FORCE(chip7); \
  static struct i2c_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {force_ ## chip5,chip5}, \
                                                 {force_ ## chip6,chip6}, \
                                                 {force_ ## chip7,chip7}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

typedef int i2c_found_addr_proc(struct i2c_adapter *adapter,
				    int addr, unsigned short flags,
				    int kind);

/* Detect function. It iterates over all possible addresses itself. For
   SMBus addresses, it will only call found_proc if some client is connected
   to the SMBus (unless a 'force' matched); for ISA detections, this is not
   done. */
extern int i2c_detect(struct i2c_adapter *adapter,
			  struct i2c_address_data *address_data,
			  i2c_found_addr_proc * found_proc);


/* This macro is used to scale user-input to sensible values in almost all
   chip drivers. */
extern inline int SENSORS_LIMIT(long value, long low, long high)
{
	if (value < low)
		return low;
	else if (value > high)
		return high;
	else
		return value;
}

#endif				/* def __KERNEL__ */


/* The maximum length of the prefix */
#define SENSORS_PREFIX_MAX 20

/* Sysctl IDs */
#ifdef DEV_HWMON
#define DEV_SENSORS DEV_HWMON
#else				/* ndef DEV_HWMOM */
#define DEV_SENSORS 2		/* The id of the lm_sensors directory within the
				   dev table */
#endif				/* def DEV_HWMON */

#define SENSORS_CHIPS 1
struct i2c_chips_data {
	int sysctl_id;
	char name[SENSORS_PREFIX_MAX + 13];
};

#endif				/* def SENSORS_SENSORS_H */

