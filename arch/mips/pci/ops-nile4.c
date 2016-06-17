#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/bootinfo.h>

#include <asm/nile4.h>
#include <asm/lasat/lasat.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

#define LO(reg) (reg / 4)
#define HI(reg) (reg / 4 + 1)

static volatile unsigned long * const vrc_pciregs = (void *)Vrc5074_BASE;

static spinlock_t nile4_pci_lock;

static int nile4_pcibios_config_access(unsigned char access_type,
       struct pci_dev *dev, unsigned char reg, u32 *data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;
	u32 adr, mask, err;

	if ((bus == 0) && (PCI_SLOT(dev_fn) > 8))
		/* The addressing scheme chosen leaves room for just
		 * 8 devices on the first bus (besides the PCI
		 * controller itself) */
		return PCIBIOS_DEVICE_NOT_FOUND;

	if ((bus == 0) && (dev_fn == PCI_DEVFN(0,0))) {
		/* Access controller registers directly */
		if (access_type == PCI_ACCESS_WRITE) {
			vrc_pciregs[(0x200+reg) >> 2] = *data;
		} else {
			*data = vrc_pciregs[(0x200+reg) >> 2];
		}
	        return PCIBIOS_SUCCESSFUL;
	}

	/* Temporarily map PCI Window 1 to config space */
	mask = vrc_pciregs[LO(NILE4_PCIINIT1)];
	vrc_pciregs[LO(NILE4_PCIINIT1)] = 0x0000001a | (bus ? 0x200 : 0);

	/* Clear PCI Error register. This also clears the Error Type
	 * bits in the Control register */
	vrc_pciregs[LO(NILE4_PCIERR)] = 0;
	vrc_pciregs[HI(NILE4_PCIERR)] = 0;

	/* Setup address */
	if (bus == 0)
		adr = KSEG1ADDR(PCI_WINDOW1) +
		      ((1 << (PCI_SLOT(dev_fn) + 15)) |
		       (PCI_FUNC(dev_fn) << 8) | (reg & ~3));
	else
		adr = KSEG1ADDR(PCI_WINDOW1) | (bus << 16) | (dev_fn << 8) |
		      (reg & ~3);

	if (access_type == PCI_ACCESS_WRITE)
		*(u32 *)adr = *data;
	else
		*data = *(u32 *)adr;

	/* Check for master or target abort */
	err = (vrc_pciregs[HI(NILE4_PCICTRL)] >> 5) & 0x7;

	/* Restore PCI Window 1 */
	vrc_pciregs[LO(NILE4_PCIINIT1)] = mask;

	if (err)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int nile4_pcibios_read_config_byte(struct pci_dev *dev, int reg, u8 *val)
{
	unsigned long flags;
        u32 data = 0;
	int err;

	spin_lock_irqsave(&nile4_pci_lock, flags);
	err = nile4_pcibios_config_access(PCI_ACCESS_READ, dev, reg, &data);
	spin_unlock_irqrestore(&nile4_pci_lock, flags);

	if (err)
		return err;

	*val = (data >> ((reg & 3) << 3)) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}

static int nile4_pcibios_read_config_word(struct pci_dev *dev, int reg, u16 *val)
{
	unsigned long flags;
        u32 data = 0;
	int err;

	if (reg & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	spin_lock_irqsave(&nile4_pci_lock, flags);
	err = nile4_pcibios_config_access(PCI_ACCESS_READ, dev, reg, &data);
	spin_unlock_irqrestore(&nile4_pci_lock, flags);

	if (err)
		return err;

	*val = (data >> ((reg & 3) << 3)) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int nile4_pcibios_read_config_dword(struct pci_dev *dev, int reg, u32 *val)
{
	unsigned long flags;
        u32 data = 0;
	int err;

	if (reg & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	spin_lock_irqsave(&nile4_pci_lock, flags);
	err = nile4_pcibios_config_access(PCI_ACCESS_READ, dev, reg, &data);
	spin_unlock_irqrestore(&nile4_pci_lock, flags);

	if (err)
		return err;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}


static int nile4_pcibios_write_config_byte(struct pci_dev *dev, int reg, u8 val)
{
	unsigned long flags;
        u32 data = 0;
	int err;

	spin_lock_irqsave(&nile4_pci_lock, flags);
        err = nile4_pcibios_config_access(PCI_ACCESS_READ, dev, reg, &data);
        if (err)
		goto out;

	data = (data & ~(0xff << ((reg & 3) << 3))) | (val << ((reg & 3) << 3));
	err = nile4_pcibios_config_access(PCI_ACCESS_WRITE, dev, reg, &data);

out:
	spin_unlock_irqrestore(&nile4_pci_lock, flags);
	return err;
}

static int nile4_pcibios_write_config_word(struct pci_dev *dev, int reg, u16 val)
{
	unsigned long flags;
        u32 data = 0;
	int err;

	if (reg & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
       
	spin_lock_irqsave(&nile4_pci_lock, flags);
        err = nile4_pcibios_config_access(PCI_ACCESS_READ, dev, reg, &data);
        if (err)
	       goto out;

	data = (data & ~(0xffff << ((reg & 3) << 3))) | (val << ((reg&3) << 3));
	err = nile4_pcibios_config_access(PCI_ACCESS_WRITE, dev, reg, &data);

out:
	spin_unlock_irqrestore(&nile4_pci_lock, flags);
	return err;
}

static int nile4_pcibios_write_config_dword(struct pci_dev *dev, int reg, u32 val)
{
	unsigned long flags;
        u32 data = 0;
	int err;

	if (reg & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	spin_lock_irqsave(&nile4_pci_lock, flags);
	err = nile4_pcibios_config_access(PCI_ACCESS_WRITE, dev, reg, &val);
	spin_unlock_irqrestore(&nile4_pci_lock, flags);

	if (err)
		return -1;
	else
		return PCIBIOS_SUCCESSFUL;
out:
	return err;
}

struct pci_ops nile4_pci_ops = {
	nile4_pcibios_read_config_byte,
	nile4_pcibios_read_config_word,
	nile4_pcibios_read_config_dword,
	nile4_pcibios_write_config_byte,
	nile4_pcibios_write_config_word,
	nile4_pcibios_write_config_dword
};
