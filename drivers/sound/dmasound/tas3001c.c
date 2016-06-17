/*
 * Driver for the i2c/i2s based TA3001C sound chip used
 * on some Apple hardware. Also known as "tumbler".
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 *
 * Modified by Christopher C. Chimelis <chris@debian.org>:
 *
 *   TODO:
 *   -----
 *   * Enable DRC since the TiBook speakers are less than good
 *   * Enable control over input line 2 (is this connected?)
 *   * Play with the dual six-stage cascading biquad filtering to see how
 *     we can use it to our advantage (currently not implemented)
 *   * Reorganise driver a bit to make it cleaner and easier to work with
 *     (read: use the header file more :-P)
 *   * Implement sleep support
 *
 *   Version 0.4:
 *   ------------
 *   * Balance control finally works (can someone document OSS better please?)
 *   * Moved to a struct for common values referenced in the driver
 *   * Put stubs in for sleep/wake-up support for now.  This will take some
 *     experimentation to make sure that the timing is right, since the
 *     TAS hardware requires specific timing while enabling low-power mode.
 *     I may cheat for now and just reset the chip on wake-up, but I'd rather
 *     not if I don't have to.
 *
 *   Version 0.3:
 *   ------------
 *   * Fixed volume control
 *   * Added bass and treble control
 *   * Added PCM line level control (mixer 1 in the TAS manual)
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/sysctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/prom.h>

#include "dmasound.h"
#include "tas3001c.h"

#define I2C_DRIVERID_TAS (0xFEBA)

#define TAS_VERSION	"0.3"
#define TAS_DATE	"20011214"

#define TAS_SETTING_MAX	100

#define VOL_DEFAULT	(((((TAS_SETTING_MAX*4)/5)<<0)<<8) | (((TAS_SETTING_MAX*4)/5)<<0))
#define INPUT_DEFAULT	(((TAS_SETTING_MAX*4)/5)<<0)
#define BASS_DEFAULT	((TAS_SETTING_MAX/2)<<0)
#define TREBLE_DEFAULT	((TAS_SETTING_MAX/2)<<0)

static struct i2c_client * tumbler_client = NULL;

int tumbler_enter_sleep(void);
int tumbler_leave_sleep(void);

static int tas_attach_adapter(struct i2c_adapter *adapter);
static int tas_detect_client(struct i2c_adapter *adapter, int address);
static int tas_detach_client(struct i2c_client *client);

/* Unique ID allocation */
static int tas_id;
static int tas_initialized;

static struct device_node* tas_node;
static u8 tas_i2c_address = 0x34;

struct tas_data_t {
	uint left_vol;		/* left volume */
	uint right_vol;		/* right volume */
	uint treble;		/* treble */
	uint bass;		/* bass */
	uint pcm_level;		/* pcm level */
};

struct i2c_driver tas_driver = {  
	name:		"TAS3001C driver  V 0.3",
	id:		I2C_DRIVERID_TAS,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	&tas_attach_adapter,
	detach_client:	&tas_detach_client,
	command:	NULL,
	inc_use:	NULL, /* &tas_inc_use, */
	dec_use:	NULL  /* &tas_dev_use  */
};

int
tumbler_get_volume(uint * left_vol, uint  *right_vol)
{
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);
	*left_vol = data->left_vol;
	*right_vol = data->right_vol;
	
	return 0;
}

int
tumbler_set_register(uint reg, uint size, char *block)
{
	if (i2c_smbus_write_block_data(tumbler_client, reg, size, block) < 0) {
		printk("tas3001c: I2C write failed \n");  
		return -1; 
	}
	return 0;
}

int
tumbler_get_pcm_lvl(uint *pcm_lvl)
{
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);
	*pcm_lvl = data->pcm_level;

	return 0;
}

int
tumbler_get_treble(uint *treble)
{
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);
	*treble = data->treble;
	
	return 0;
}

int
tumbler_get_bass(uint *bass)
{
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);
	*bass = data->bass;

	return 0;
}

int
tumbler_set_bass(uint bass)
{
	uint cur_bass_pers = bass;
	char block;
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);

	bass &= 0xff;
	if (bass > TAS_SETTING_MAX)
		bass = TAS_SETTING_MAX;
	bass = ((bass * 72) / TAS_SETTING_MAX) << 0;
	bass = tas_bass_table[bass];
	block = (bass >> 0)  & 0xff;

	if (tumbler_set_register(TAS_SET_BASS, &block) < 0) {
		printk("tas3001c: failed to set bass \n");  
		return -1; 
	}
	data->bass = cur_bass_pers;
	return 0;
}

int
tumbler_set_treble(uint treble)
{
	uint cur_treble_pers = treble;
	char block;
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);

	treble &= 0xff;
	if (treble > TAS_SETTING_MAX)
		treble = TAS_SETTING_MAX;
	treble = ((treble * 72) / TAS_SETTING_MAX) << 0;
	treble = tas_treble_table[treble];
	block = (treble >> 0)  & 0xff;

	if (tumbler_set_register(TAS_SET_TREBLE, &block) < 0) {
		printk("tas3001c: failed to set treble \n");  
		return -1; 
	}
	data->treble = cur_treble_pers;
	return 0;
}

int
tumbler_set_pcm_lvl(uint pcm_lvl)
{
	uint pcm_lvl_pers = pcm_lvl;
	unsigned char block[3];
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);

	pcm_lvl &= 0xff;
	if (pcm_lvl > TAS_SETTING_MAX)
		pcm_lvl = TAS_SETTING_MAX;
	pcm_lvl = ((pcm_lvl * 176) / TAS_SETTING_MAX) << 0;

	pcm_lvl = tas_input_table[pcm_lvl];

	block[0] = (pcm_lvl >> 16) & 0xff;
	block[1] = (pcm_lvl >> 8)  & 0xff;
	block[2] = (pcm_lvl >> 0)  & 0xff;

	if (tumbler_set_register(TAS_SET_MIXER1, block) < 0) {
		printk("tas3001c: failed to set input level \n");  
		return -1; 
	}
	data->pcm_level = pcm_lvl_pers;

	return 0;
}

int
tumbler_set_volume(uint left_vol, uint right_vol)
{
	uint left_vol_pers = left_vol;
	uint right_vol_pers = right_vol;
	unsigned char block[6];
	struct tas_data_t *data;

	if (!tumbler_client)
		return -1;

	data = (struct tas_data_t *) (tumbler_client->data);

	left_vol &= 0xff;
	if (left_vol > TAS_SETTING_MAX)
		left_vol = TAS_SETTING_MAX;

	right_vol = (right_vol >> 8) & 0xff;
	if (right_vol > TAS_SETTING_MAX)
		right_vol = TAS_SETTING_MAX;

	left_vol = ((left_vol * 176) / TAS_SETTING_MAX) << 0;
	right_vol = ((right_vol * 176) / TAS_SETTING_MAX) << 0;

	left_vol = tas_volume_table[left_vol];
	right_vol = tas_volume_table[right_vol];

	block[0] = (left_vol >> 16) & 0xff;
	block[1] = (left_vol >> 8)  & 0xff;
	block[2] = (left_vol >> 0)  & 0xff;

	block[3] = (right_vol >> 16) & 0xff;
	block[4] = (right_vol >> 8)  & 0xff;
	block[5] = (right_vol >> 0)  & 0xff;

	if (tumbler_set_register(TAS_SET_VOLUME, block) < 0) {
		printk("tas3001c: failed to set volume \n");  
		return -1; 
	}
	data->left_vol = left_vol_pers;
	data->right_vol = right_vol_pers;

	return 0;
}

int
tumbler_leave_sleep(void)
{
	/* Stub for now, but I have the details on low-power mode */
	if (!tumbler_client)
		return -1;

	return 0;
}

int
tumbler_enter_sleep(void)
{
	/* Stub for now, but I have the details on low-power mode */
	if (!tumbler_client)
		return -1;

	return 0;
}

static int
tas_attach_adapter(struct i2c_adapter *adapter)
{
	if (!strncmp(adapter->name, "mac-io", 6))
		tas_detect_client(adapter, tas_i2c_address);

	return 0;
}

static int
tas_init_client(struct i2c_client * new_client)
{
	/* Make sure something answers on the i2c bus
	*/

	if (i2c_smbus_write_byte_data(new_client, 1, (1<<6)+(2<<4)+(2<<2)+0) < 0)
		return -1;

	tumbler_client = new_client;

	tumbler_set_volume(VOL_DEFAULT, VOL_DEFAULT);
	tumbler_set_pcm_lvl(INPUT_DEFAULT);
	tumbler_set_bass(BASS_DEFAULT);
	tumbler_set_treble(TREBLE_DEFAULT);

	return 0;
}

static int
tas_detect_client(struct i2c_adapter *adapter, int address)
{
	int rc = 0;
	struct i2c_client *new_client;
	struct tas_data_t *data;
	const char *client_name = "tas 3001c Digital Equalizer";

	new_client = kmalloc(
			     sizeof(struct i2c_client) + sizeof(struct tas_data_t),
			     GFP_KERNEL);
	if (!new_client) {
		rc = -ENOMEM;
		goto bail;
	}

	/* This is tricky, but it will set the data to the right value. */
	new_client->data = new_client + 1;
	data = (struct tas_data_t *) (new_client->data);

	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &tas_driver;
	new_client->flags = 0;

	strcpy(new_client->name,client_name);

	new_client->id = tas_id++; /* Automatically unique */

	if (tas_init_client(new_client)) {
		rc = -ENODEV;
		goto bail;
	}

	/* Tell the i2c layer a new client has arrived */
	if (i2c_attach_client(new_client)) {
		rc = -ENODEV;
		goto bail;
	}
bail:
	if (rc && new_client)
		kfree(new_client);
	return rc;
}

static int
tas_detach_client(struct i2c_client *client)
{
	if (client == tumbler_client)
		tumbler_client = NULL;

	i2c_detach_client(client);
	kfree(client);

	return 0;
}

int
tas_cleanup(void)
{
	if (!tas_initialized)
		return -ENODEV;
	i2c_del_driver(&tas_driver);
	tas_initialized = 0;

	return 0;
}

int
tas_init(void)
{
	int rc;
	u32* paddr;

	if (tas_initialized)
		return 0;

	tas_node = find_devices("deq");
	if (tas_node == NULL)
		return -ENODEV;

	printk(KERN_INFO "tas3001c driver version %s (%s)\n",TAS_VERSION,TAS_DATE);
	paddr = (u32 *)get_property(tas_node, "i2c-address", NULL);
	if (paddr) {
		tas_i2c_address = (*paddr) >> 1;
		printk(KERN_INFO "using i2c address: 0x%x from device-tree\n",
		       tas_i2c_address);
	} else    
		printk(KERN_INFO "using i2c address: 0x%x (default)\n", tas_i2c_address);

	if ((rc = i2c_add_driver(&tas_driver))) {
		printk("tas3001c: Driver registration failed, module not inserted.\n");
		tas_cleanup();
		return rc;
	}
	tas_initialized = 1;
	return 0;
}
