/* Minimal kernel init for UOS(m) - User OS Mobile
 *
 * Early boot uses both serial and framebuffer console.
 * Framebuffer at physical 0x40000000 (QEMU stdvga / virtio-gpu).
 */

#include "config.h"
#include "../ui/mobile_ui.h"

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

/* Framebuffer console (defined in framebuffer_console.c) */
void fb_console_init(void);
void fb_console_write(const char *s);

static void fb_puts(const char *s) {
    fb_console_write(s);
}

#define CHECK(n, label) do { \
    early_puts("[" #n "] " label "\r\n"); \
    fb_puts("[" #n "] " label "\n"); \
} while(0)

#define HALT(msg) do { \
    early_puts("HALT: " msg "\r\n"); \
    fb_puts("HALT: " msg "\n"); \
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
extern void timer_init(uint32_t freq_hz);
extern int pmp_init(void);
extern int syscall_security_init(void);
extern int aslr_init(void);
extern int stack_canary_init(void);
extern int virtio_net_init(void);
extern int virtio_blk_init(void);
extern int gpu_init(void);
extern int mobile_ui_init(void);
extern int mobile_ui_start(void);

void kernel_init(unsigned long hartid, void *dtb) {
    (void)hartid;
    (void)dtb;

    uart_puts("\r\nuOS(m) boot start\r\n");

    CHECK(0, "uart");
    early_putc('0' + (int)hartid);
    uart_puts("\r\n");

    CHECK(1, "fb_console_init");
    fb_console_init();
    fb_puts("uOS(m) boot start\n");

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

    CHECK(8, "gpu_init");
    if (gpu_init())     HALT("gpu_init");

    CHECK(9, "mobile_ui_init");
    if (mobile_ui_init()) HALT("mobile_ui_init");

    CHECK(10, "mobile_ui_start");
    if (mobile_ui_start()) HALT("mobile_ui_start");

    CHECK(11, "vfs_init");
    if (vfs_init())     HALT("vfs_init");

    CHECK(12, "chardev_init");
    if (chardev_init()) HALT("chardev_init");

    CHECK(13, "uart_chardev");
    if (uart_chardev_init()) HALT("uart_chardev");

    CHECK(14, "interrupt_init");
    if (interrupt_init()) HALT("interrupt_init");

    CHECK(15, "timer_init");
    timer_init(TIMER_FREQ);


    CHECK(16, "virtio_blk");
    if (virtio_blk_init()) HALT("virtio_blk");

    CHECK(17, "virtio_net");
    if (virtio_net_init()) HALT("virtio_net");

    uart_puts("[18] subsystems ready\n");
    fb_puts("[18] subsystems ready\n");

    CHECK(19, "task_init");
    task_init();

    CHECK(20, "ipc_init");
    ipc_init();

    CHECK(21, "scheduler_init");
    scheduler_init();

    uart_puts("[22] entering scheduler (should not return)\n");
    fb_puts("[22] entering scheduler\n");

    for (;;) {
        asm volatile("wfi");
    }
}
