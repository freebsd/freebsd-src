#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.0.1"
#define DRIVER_AUTHOR "Josh Myer <josh@joshisanerd.com>"
#define DRIVER_DESC "KB Gear Jam Studio Tablet Driver"

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define USB_VENDOR_ID_KBTAB 0x84e

static int       kb_pressure_click = 0x10;
MODULE_PARM     (kb_pressure_click,"i");
MODULE_PARM_DESC(kb_pressure_click,
		 "pressure threshold for clicks");

struct kbtab {
	signed char data[8];
	struct input_dev dev;
	struct usb_device *usbdev;
	struct urb irq;
	int open;
	int x, y;
	int button;
	int pressure;
};

static void kbtab_irq(struct urb *urb)
{

	struct kbtab *tab = urb->context;
	unsigned char *data = tab->data;
	struct input_dev *dev = &tab->dev;

	if(urb->status)
		return;

	tab->x = (data[2] << 8) + data[1];
	tab->y = (data[4] << 8) + data[3];

	tab->pressure = (data[5]);

	/* XXX: don't report unless actual change */

	input_report_abs(dev, ABS_X, tab->x);
	input_report_abs(dev, ABS_Y, tab->y);
	input_report_abs(dev, ABS_PRESSURE, tab->pressure);

	input_report_key(dev, BTN_STYLUS, (data[0] & 2));
	input_report_key(dev, BTN_TOUCH, (data[0] & 1));
	input_report_key(dev, BTN_LEFT, (tab->pressure > kb_pressure_click) ? 1 : 0);

	input_event(dev, EV_MSC, MSC_SERIAL, 0);
}

struct usb_device_id kbtab_ids[] = {
	{ USB_DEVICE(USB_VENDOR_ID_KBTAB, 0x1001), driver_info : 0 },
	{ }
};
  
MODULE_DEVICE_TABLE(usb, kbtab_ids);

static int kbtab_open(struct input_dev *dev)
{
	struct kbtab *kbtab = dev->private;

	if(kbtab->open++)
		return 0;

	kbtab->irq.dev = kbtab->usbdev;
	if(usb_submit_urb(&kbtab->irq))
		return -EIO;

	return 0;
}

static void kbtab_close(struct input_dev *dev)
{
	struct kbtab *kbtab = dev->private;

	if(!--kbtab->open)
		usb_unlink_urb(&kbtab->irq);
}

static void *kbtab_probe(struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *endpoint;
	struct kbtab *kbtab;

	if(!(kbtab = kmalloc(sizeof(struct kbtab), GFP_KERNEL)))
		return NULL;

	memset(kbtab, 0, sizeof(struct kbtab));

	kbtab->dev.evbit[0] |= BIT(EV_KEY) | BIT(EV_ABS) | BIT(EV_MSC);
	kbtab->dev.absbit[0] |= BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);

	kbtab->dev.keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT) | BIT(BTN_RIGHT) | BIT(BTN_MIDDLE);
	kbtab->dev.keybit[LONG(BTN_DIGI)] |= BIT(BTN_STYLUS);

	kbtab->dev.mscbit[0] |= BIT(MSC_SERIAL);

	kbtab->dev.absmax[ABS_X] = 0x2000;
	kbtab->dev.absmax[ABS_Y] = 0x1750;

	kbtab->dev.absmax[ABS_PRESSURE] = 0xff;
  
	kbtab->dev.absfuzz[ABS_X] = 4;
	kbtab->dev.absfuzz[ABS_Y] = 4;
  
	kbtab->dev.private = kbtab;

	kbtab->dev.open = kbtab_open;
	kbtab->dev.close = kbtab_close;

	kbtab->dev.name = "KB Gear Tablet";
	kbtab->dev.idbus = BUS_USB;
  
	kbtab->dev.idvendor = dev->descriptor.idVendor;
	kbtab->dev.idproduct = dev->descriptor.idProduct;
	kbtab->dev.idversion = dev->descriptor.bcdDevice;
	kbtab->usbdev = dev;


	endpoint = dev->config[0].interface[ifnum].altsetting[0].endpoint + 0;

	usb_set_idle(dev, dev->config[0].interface[ifnum].altsetting[0].bInterfaceNumber, 0, 0);

	FILL_INT_URB(&kbtab->irq, dev, usb_rcvintpipe(dev, endpoint->bEndpointAddress),
		     kbtab->data, 8, kbtab_irq, kbtab, endpoint->bInterval);

	input_register_device(&kbtab->dev);

	printk(KERN_INFO "input%d: KB Gear Tablet on usb%d:%d.%d\n",
	       kbtab->dev.number, dev->bus->busnum, dev->devnum, ifnum);

	return kbtab;

}

static void kbtab_disconnect(struct usb_device *dev, void *ptr)
{
	struct kbtab *kbtab = ptr;
	usb_unlink_urb(&kbtab->irq);
	input_unregister_device(&kbtab->dev);
	kfree(kbtab);
}

static struct usb_driver kbtab_driver = {
	name:		"kbtab",
	probe:		kbtab_probe,
	disconnect:	kbtab_disconnect,
	id_table:	kbtab_ids,
};

static int __init kbtab_init(void)
{
	usb_register(&kbtab_driver);
	info(DRIVER_VERSION " " DRIVER_AUTHOR);
	info(DRIVER_DESC);
	return 0;
}

static void __exit kbtab_exit(void)
{
	usb_deregister(&kbtab_driver);
}

module_init(kbtab_init);
module_exit(kbtab_exit);
