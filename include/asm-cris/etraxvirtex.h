#ifndef _LINUX_ETRAXVIRTEX_FPGA_H
#define _LINUX_ETRAXVIRTEX_FPGA_H

/* etraxvirtex_fpga _IOC_TYPE, bits 8 to 15 in ioctl cmd */

#define ETRAXVIRTEX_FPGA_IOCTYPE 45

/* supported ioctl _IOC_NR's */

/* in write operations, the argument contains both virtex
 * register and value.
 */

#define VIRTEX_FPGA_WRITEARG(reg, value) (((reg) << 16) | (value))
#define VIRTEX_FPGA_READARG(reg) ((reg) << 16)

#define VIRTEX_FPGA_ARGREG(arg) (((arg) >> 16) & 0x0fff)
#define VIRTEX_FPGA_ARGVALUE(arg) ((arg) & 0xffff)

#define VIRTEX_FPGA_WRITEREG 0x1   /* write to an (FPGA implemented) register */
#define VIRTEX_FPGA_READREG  0x2   /* read from an (FPGA implemented register */

/*
EXAMPLE usage:

    virtex_arg = VIRTEX_FPGA_WRITEARG( reg, val);
    ioctl(fd, _IO(ETRAXVIRTEX_FPGA_IOCTYPE, VIRTEX_FPGA_WRITEREG), virtex_arg);

    virtex_arg = VIRTEX_FPGA_READARG( reg);
    val = ioctl(fd, _IO(ETRAXVIRTEX_FPGA_IOCTYPE, VIRTEX_FPGA_READREG), virtex_arg);

*/
#endif
