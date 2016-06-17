/*
 * EFI Variables - efivars.c
 *
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 *
 * This code takes all variables accessible from EFI runtime and
 *  exports them via /proc
 *
 * Reads to /proc/efi/vars/varname return an efi_variable_t structure.
 * Writes to /proc/efi/vars/varname must be an efi_variable_t structure.
 * Writes with DataSize = 0 or Attributes = 0 deletes the variable.
 * Writes with a new value in VariableName+VendorGuid creates
 * a new variable.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Changelog:
 *
 *  10 Dec 2002 - Matt Domsch <Matt_Domsch@dell.com>
 *   fix locking per Peter Chubb's findings
 * 
 *  25 Mar 2002 - Matt Domsch <Matt_Domsch@dell.com>
 *   move uuid_unparse() to include/asm-ia64/efi.h:efi_guid_unparse()
 *
 *  12 Feb 2002 - Matt Domsch <Matt_Domsch@dell.com>
 *   use list_for_each_safe when deleting vars.
 *   remove ifdef CONFIG_SMP around include <linux/smp.h>
 *   v0.04 release to linux-ia64@linuxia64.org
 *
 *  20 April 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   Moved vars from /proc/efi to /proc/efi/vars, and made
 *   efi.c own the /proc/efi directory.
 *   v0.03 release to linux-ia64@linuxia64.org
 *
 *  26 March 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   At the request of Stephane, moved ownership of /proc/efi
 *   to efi.c, and now efivars lives under /proc/efi/vars.
 *
 *  12 March 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   Feedback received from Stephane Eranian incorporated.
 *   efivar_write() checks copy_from_user() return value.
 *   efivar_read/write() returns proper errno.
 *   v0.02 release to linux-ia64@linuxia64.org
 *
 *  26 February 2001 - Matt Domsch <Matt_Domsch@dell.com>
 *   v0.01 release to linux-ia64@linuxia64.org
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>		/* for capable() */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/efi.h>

#include <asm/uaccess.h>

MODULE_AUTHOR("Matt Domsch <Matt_Domsch@Dell.com>");
MODULE_DESCRIPTION("/proc interface to EFI Variables");
MODULE_LICENSE("GPL");

#define EFIVARS_VERSION "0.06 2002-Dec-10"

static int
efivar_read(char *page, char **start, off_t off,
	    int count, int *eof, void *data);
static int
efivar_write(struct file *file, const char *buffer,
	     unsigned long count, void *data);


/*
 * The maximum size of VariableName + Data = 1024
 * Therefore, it's reasonable to save that much
 * space in each part of the structure,
 * and we use a page for reading/writing.
 */

typedef struct _efi_variable_t {
	efi_char16_t  VariableName[1024/sizeof(efi_char16_t)];
	efi_guid_t    VendorGuid;
	unsigned long DataSize;
	__u8          Data[1024];
	efi_status_t  Status;
	__u32         Attributes;
} __attribute__((packed)) efi_variable_t;


typedef struct _efivar_entry_t {
	efi_variable_t          var;
	struct proc_dir_entry   *entry;
	struct list_head        list;
} efivar_entry_t;

/*
  efivars_lock protects two things:
  1) efivar_list - adds, removals, reads, writes
  2) efi.[gs]et_variable() calls.
  It must not be held when creating proc entries or calling kmalloc.
  efi.get_next_variable() is only called from efivars_init(),
  which is protected by the BKL, so that path is safe.
*/
static spinlock_t efivars_lock = SPIN_LOCK_UNLOCKED;
static LIST_HEAD(efivar_list);
static struct proc_dir_entry *efi_vars_dir;

#define efivar_entry(n) list_entry(n, efivar_entry_t, list)

/* Return the number of unicode characters in data */
static unsigned long
utf8_strlen(efi_char16_t *data, unsigned long maxlength)
{
	unsigned long length = 0;
	while (*data++ != 0 && length < maxlength)
		length++;
	return length;
}

/* Return the number of bytes is the length of this string */
/* Note: this is NOT the same as the number of unicode characters */
static inline unsigned long
utf8_strsize(efi_char16_t *data, unsigned long maxlength)
{
	return utf8_strlen(data, maxlength/sizeof(efi_char16_t)) * sizeof(efi_char16_t);
}


static int
proc_calc_metrics(char *page, char **start, off_t off,
		  int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

/*
 * efivar_create_proc_entry()
 * Requires:
 *    variable_name_size = number of bytes required to hold
 *                         variable_name (not counting the NULL
 *                         character at the end.
 *    efivars_lock is not held on entry or exit.
 * Returns 1 on failure, 0 on success
 */
static int
efivar_create_proc_entry(unsigned long variable_name_size,
			 efi_char16_t *variable_name,
			 efi_guid_t *vendor_guid)
{
	int i, short_name_size = variable_name_size / sizeof(efi_char16_t) + 38;
	char *short_name;
	efivar_entry_t *new_efivar;

	short_name = kmalloc(short_name_size+1, GFP_KERNEL);
	new_efivar = kmalloc(sizeof(efivar_entry_t), GFP_KERNEL);

	if (!short_name || !new_efivar)  {
		if (short_name)        kfree(short_name);
		if (new_efivar)        kfree(new_efivar);
		return 1;
	}
	memset(short_name, 0, short_name_size+1);
	memset(new_efivar, 0, sizeof(efivar_entry_t));

	memcpy(new_efivar->var.VariableName, variable_name,
	       variable_name_size);
	memcpy(&(new_efivar->var.VendorGuid), vendor_guid, sizeof(efi_guid_t));

	/* Convert Unicode to normal chars (assume top bits are 0),
	   ala UTF-8 */
	for (i=0; i< (int) (variable_name_size / sizeof(efi_char16_t)); i++) {
		short_name[i] = variable_name[i] & 0xFF;
	}

	/* This is ugly, but necessary to separate one vendor's
	   private variables from another's.         */

	*(short_name + strlen(short_name)) = '-';
	efi_guid_unparse(vendor_guid, short_name + strlen(short_name));

	/* Create the entry in proc */
	new_efivar->entry = create_proc_entry(short_name, 0600, efi_vars_dir);
	kfree(short_name); short_name = NULL;
	if (!new_efivar->entry) return 1;

	new_efivar->entry->data = new_efivar;
	new_efivar->entry->read_proc = efivar_read;
	new_efivar->entry->write_proc = efivar_write;

	spin_lock(&efivars_lock);
	list_add(&new_efivar->list, &efivar_list);
	spin_unlock(&efivars_lock);

	return 0;
}



/***********************************************************
 * efivar_read()
 * Requires:
 * Modifies: page
 * Returns: number of bytes written, or -EINVAL on failure
 ***********************************************************/

static int
efivar_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = sizeof(efi_variable_t);
	efivar_entry_t *efi_var = data;
	efi_variable_t *var_data = (efi_variable_t *)page;

	if (!page || !data) return -EINVAL;

	spin_lock(&efivars_lock);
	MOD_INC_USE_COUNT;

	memcpy(var_data, &efi_var->var, len);

	var_data->DataSize = 1024;
	var_data->Status = efi.get_variable(var_data->VariableName,
					    &var_data->VendorGuid,
					    &var_data->Attributes,
					    &var_data->DataSize,
					    var_data->Data);

	MOD_DEC_USE_COUNT;
	spin_unlock(&efivars_lock);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/***********************************************************
 * efivar_write()
 * Requires: data is an efi_setvariable_t data type,
 *           properly filled in, possibly by a call
 *           first to efivar_read().
 *           Caller must have CAP_SYS_ADMIN
 * Modifies: NVRAM
 * Returns: var_data->DataSize on success, errno on failure
 *
 ***********************************************************/
static int
efivar_write(struct file *file, const char *buffer,
	     unsigned long count, void *data)
{
	unsigned long strsize1, strsize2;
	int found=0;
	struct list_head *pos, *n;
	unsigned long size = sizeof(efi_variable_t);
	efi_status_t status;
	efivar_entry_t *efivar = data, *search_efivar = NULL;
	efi_variable_t *var_data;
	if (!data || count != size) {
		printk(KERN_WARNING "efivars: improper struct of size 0x%lx passed.\n", count);
		return -EINVAL;
	}
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	MOD_INC_USE_COUNT;

	var_data = kmalloc(size, GFP_KERNEL);
	if (!var_data) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	if (copy_from_user(var_data, buffer, size)) {
		MOD_DEC_USE_COUNT;
		kfree(var_data);
		return -EFAULT;
	}

	spin_lock(&efivars_lock);

	/* Since the data ptr we've currently got is probably for
	   a different variable find the right variable.
	   This allows any properly formatted data structure to
	   be written to any of the files in /proc/efi/vars and it will work.
	*/
	list_for_each_safe(pos, n, &efivar_list) {
		search_efivar = efivar_entry(pos);
		strsize1 = utf8_strsize(search_efivar->var.VariableName, 1024);
		strsize2 = utf8_strsize(var_data->VariableName, 1024);
		if ( strsize1 == strsize2 &&
		     !memcmp(&(search_efivar->var.VariableName),
			     var_data->VariableName, strsize1) &&
		     !efi_guidcmp(search_efivar->var.VendorGuid,
				  var_data->VendorGuid)) {
			found = 1;
			break;
		}
	}
	if (found) efivar = search_efivar;

	status = efi.set_variable(var_data->VariableName,
				  &var_data->VendorGuid,
				  var_data->Attributes,
				  var_data->DataSize,
				  var_data->Data);

	if (status != EFI_SUCCESS) {
		printk(KERN_WARNING "set_variable() failed: status=%lx\n", status);
		kfree(var_data);
		MOD_DEC_USE_COUNT;
		spin_unlock(&efivars_lock);
		return -EIO;
	}


	if (!var_data->DataSize || !var_data->Attributes) {
		/* We just deleted the NVRAM variable */
		remove_proc_entry(efivar->entry->name, efi_vars_dir);
		list_del(&efivar->list);
		kfree(efivar);
	}

	spin_unlock(&efivars_lock);

	/* If this is a new variable, set up the proc entry for it. */
	if (!found) {
		efivar_create_proc_entry(utf8_strsize(var_data->VariableName,
						      1024),
					 var_data->VariableName,
					 &var_data->VendorGuid);
	}

	kfree(var_data);
	MOD_DEC_USE_COUNT;
	return size;
}

/*
 * The EFI system table contains pointers to the SAL system table,
 * HCDP, ACPI, SMBIOS, etc, that may be useful to applications.
 */
static ssize_t
efi_systab_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	void *data;
	u8 *proc_buffer;
	ssize_t size, length;
	int ret;
	const int max_nr_entries = 7; 	/* num ptrs to tables we could expose */
	const int max_line_len = 80;

	if (!efi.systab)
		return 0;

	proc_buffer = kmalloc(max_nr_entries * max_line_len, GFP_KERNEL);
	if (!proc_buffer)
		return -ENOMEM;

	length = 0;
	if (efi.mps)
		length += sprintf(proc_buffer + length, "MPS=0x%lx\n", __pa(efi.mps));
	if (efi.acpi20)
		length += sprintf(proc_buffer + length, "ACPI20=0x%lx\n", __pa(efi.acpi20));
	if (efi.acpi)
		length += sprintf(proc_buffer + length, "ACPI=0x%lx\n", __pa(efi.acpi));
	if (efi.smbios)
		length += sprintf(proc_buffer + length, "SMBIOS=0x%lx\n", __pa(efi.smbios));
	if (efi.sal_systab)
		length += sprintf(proc_buffer + length, "SAL=0x%lx\n", __pa(efi.sal_systab));
	if (efi.hcdp)
		length += sprintf(proc_buffer + length, "HCDP=0x%lx\n", __pa(efi.hcdp));
	if (efi.boot_info)
		length += sprintf(proc_buffer + length, "BOOTINFO=0x%lx\n", __pa(efi.boot_info));

	if (*ppos >= length) {
		ret = 0;
		goto out;
	}

	data = proc_buffer + file->f_pos;
	size = length - file->f_pos;
	if (size > count)
		size = count;
	if (copy_to_user(buffer, data, size)) {
		ret = -EFAULT;
		goto out;
	}

	*ppos += size;
	ret = size;

out:
	kfree(proc_buffer);
	return ret;
}

static struct proc_dir_entry *efi_systab_entry;
static struct file_operations efi_systab_fops = {
	.read = efi_systab_read,
};

static int __init
efivars_init(void)
{
	efi_status_t status;
	efi_guid_t vendor_guid;
	efi_char16_t *variable_name = kmalloc(1024, GFP_KERNEL);
	unsigned long variable_name_size = 1024;

	printk(KERN_INFO "EFI Variables Facility v%s\n", EFIVARS_VERSION);

	/* Since efi.c happens before procfs is available,
	   we create the directory here if it doesn't
	   already exist.  There's probably a better way
	   to do this.
	*/
	if (!efi_dir)
		efi_dir = proc_mkdir("efi", NULL);

	efi_systab_entry = create_proc_entry("systab", S_IRUSR | S_IRGRP, efi_dir);
	if (efi_systab_entry)
		efi_systab_entry->proc_fops = &efi_systab_fops;

	efi_vars_dir = proc_mkdir("vars", efi_dir);

	/* Per EFI spec, the maximum storage allocated for both
	   the variable name and variable data is 1024 bytes.
	*/

	memset(variable_name, 0, 1024);

	do {
		variable_name_size=1024;

		status = efi.get_next_variable(&variable_name_size,
					       variable_name,
					       &vendor_guid);


		switch (status) {
		case EFI_SUCCESS:
			efivar_create_proc_entry(variable_name_size,
						 variable_name,
						 &vendor_guid);
			break;
		case EFI_NOT_FOUND:
			break;
		default:
			printk(KERN_WARNING "get_next_variable: status=%lx\n", status);
			status = EFI_NOT_FOUND;
			break;
		}

	} while (status != EFI_NOT_FOUND);

	kfree(variable_name);
	return 0;
}

static void __exit
efivars_exit(void)
{
	struct list_head *pos, *n;
	efivar_entry_t *efivar;

	spin_lock(&efivars_lock);
	if (efi_systab_entry)
		remove_proc_entry(efi_systab_entry->name, efi_dir);
	list_for_each_safe(pos, n, &efivar_list) {
		efivar = efivar_entry(pos);
		remove_proc_entry(efivar->entry->name, efi_vars_dir);
		list_del(&efivar->list);
		kfree(efivar);
	}
	spin_unlock(&efivars_lock);

	remove_proc_entry(efi_vars_dir->name, efi_dir);
}

module_init(efivars_init);
module_exit(efivars_exit);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
