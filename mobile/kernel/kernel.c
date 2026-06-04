/* Minimal kernel init for UOS(m) - User OS Mobile
 *
 * Serial-only early boot (works with QEMU -nographic).
 * The framebuffer console is NOT used here because -nographic provides
 * no VGA device; use --vnc mode instead for graphical output.
 */

#include "config.h"
#include "framebuffer_console.h"

#define UART_BASE   0x10000000UL
#define UART_TX     0x0
#define UART_LSR    0x5
#define UART_LSR_TX 0x20

static volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;

static void uart_putc(char c) {
    while ((uart[UART_LSR] & UART_LSR_TX) == 0) { }
    uart[UART_TX] = (unsigned char)c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void early_putc(char c) { uart_putc(c); }
static void early_puts(const char *s) { while (*s) uart_putc(*s++); }

#define CHECK(n, label) do { \
    early_puts("[" #n "] " label "\r\n"); \
} while(0)

#define HALT(msg) do { \
    early_puts("HALT: " msg "\r\n"); \
    for (;;) { asm volatile("wfi"); } \
} while(0)

extern int mem_init(void);
extern int task_init(void);
extern int scheduler_init(void);
extern int ipc_init(void);
extern int vm_init(void);
extern int vfs_init(void);
extern int chardev_init(void);
extern int uart_chardev_init(void);
extern int interrupt_init(void);
extern int pmp_init(void);
extern int syscall_security_init(void);
extern int aslr_init(void);
extern int stack_canary_init(void);
extern int virtio_net_init(void);
extern int virtio_blk_init(void);

void kernel_init(unsigned long hartid, void *dtb) {
    (void)hartid;
    (void)dtb;

    uart_puts("\r\nuOS(m) boot start\r\n");

    CHECK(1, "uart");
    early_putc('0' + (int)hartid);
    uart_puts("\r\n");

    CHECK(2, "mem_init");
    if (mem_init())     HALT("mem_init");

    CHECK(3, "vm_init");
    if (vm_init())      HALT("vm_init");

    CHECK(4, "pmp_init");
    if (pmp_init())     HALT("pmp_init");

    CHECK(5, "syscall_security");
    if (syscall_security_init()) HALT("syscall_security");

    CHECK(6, "aslr_init");
    if (aslr_init())    HALT("aslr_init");

    CHECK(7, "stack_canary");
    if (stack_canary_init()) HALT("stack_canary");

    CHECK(8, "vfs_init");
    if (vfs_init())     HALT("vfs_init");

    CHECK(9, "chardev_init");
    if (chardev_init()) HALT("chardev_init");

    CHECK(10, "uart_chardev");
    if (uart_chardev_init()) HALT("uart_chardev");

    CHECK(11, "interrupt_init");
    if (interrupt_init()) HALT("interrupt_init");

    CHECK(12, "virtio_blk");
    if (virtio_blk_init()) HALT("virtio_blk");

    CHECK(13, "virtio_net");
    if (virtio_net_init()) HALT("virtio_net");

    uart_puts("[14] subsystems ready\r\n");

    CHECK(15, "task_init");
    task_init();

    CHECK(16, "ipc_init");
    ipc_init();

    CHECK(17, "scheduler_init");
    scheduler_init();

    uart_puts("[18] entering scheduler (should not return)\r\n");

    for (;;) {
        asm volatile("wfi");
    }
}
