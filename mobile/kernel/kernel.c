/* Minimal kernel init for UOS(m) - User OS Mobile
 *
 * This is a minimal bootstrap that:
 *  - Initializes the framebuffer console (VGA-style, 1280x720x32)
 *  - Prints boot messages to BOTH serial and framebuffer
 *  - Initializes core subsystems
 *  - Hands off to the scheduler
 */

#include "config.h"
#include "framebuffer_console.h"

/* UART for serial console */
#define UART_BASE   0x10000000UL
#define UART_TX     0x0
#define UART_LSR    0x5
#define UART_LSR_TX 0x20

static inline void uart_putc(char c) {
    volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;
    while ((uart[UART_LSR] & UART_LSR_TX) == 0) { /* wait */ }
    uart[UART_TX] = (unsigned char)c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void early_putc(char c) { uart_putc(c); }
static void early_puts(const char *s) { while (*s) uart_putc(*s++); }

/* External subsystem init functions */
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

void kernel_init(unsigned long hartid, void *dtb) {
    const char boot_msg[]     = "\n\ruOS(m) - User OS Mobile booting...\r\n";
    const char hart_msg[]     = "Hart ID: ";
    const char hybrid_msg[]   = "Hybrid kernel initialized\r\n";
    const char posix_msg[]    = "POSIX API ready\r\n";
    const char subsys_msg[]   = "\r\n=== Initializing Kernel Subsystems ===\r\n";
    const char ready_msg[]    = "=== All subsystems ready ===\r\n";
    const char arch_msg[]     = "Arch: Hybrid Kernel | POSIX | IPC | VM (SV39) | VFS\r\n\n";

    (void)hartid;
    (void)dtb;

    /* Boot log to serial */
    early_puts(boot_msg);
    early_puts(hart_msg);
    early_putc('0' + (int)hartid);
    early_puts("\r\n");
    early_puts(hybrid_msg);
    early_puts(posix_msg);

    /* Initialize framebuffer console (visible in VNC) */
    fb_console_init();

    /* Mirror boot log to framebuffer */
    fb_console_write(boot_msg);
    fb_console_write(hart_msg);
    fb_console_putc('0' + (int)hartid);
    fb_console_write("\r\n");
    fb_console_write(hybrid_msg);
    fb_console_write(posix_msg);

    /* Initialize kernel subsystems */
    fb_console_write(subsys_msg);
    early_puts(subsys_msg);

    mem_init();       early_puts("  [OK] memory\r\n");       fb_console_write("  [OK] memory\r\n");
    vm_init();        early_puts("  [OK] virtual memory\r\n"); fb_console_write("  [OK] virtual memory\r\n");
    pmp_init();       early_puts("  [OK] PMP\r\n");           fb_console_write("  [OK] PMP\r\n");
    syscall_security_init(); early_puts("  [OK] syscall security\r\n"); fb_console_write("  [OK] syscall security\r\n");
    aslr_init();      early_puts("  [OK] ASLR\r\n");          fb_console_write("  [OK] ASLR\r\n");
    stack_canary_init(); early_puts("  [OK] stack canary\r\n"); fb_console_write("  [OK] stack canary\r\n");
    vfs_init();       early_puts("  [OK] VFS\r\n");           fb_console_write("  [OK] VFS\r\n");
    chardev_init();   early_puts("  [OK] character devices\r\n"); fb_console_write("  [OK] character devices\r\n");
    uart_chardev_init(); early_puts("  [OK] UART driver\r\n"); fb_console_write("  [OK] UART driver\r\n");
    interrupt_init(); early_puts("  [OK] interrupt controller\r\n"); fb_console_write("  [OK] interrupt controller\r\n");

    fb_console_write("\r\n");
    early_puts("\r\n");

    fb_console_write(ready_msg);
    early_puts(ready_msg);
    fb_console_write(arch_msg);
    early_puts(arch_msg);

    /* Initialize task subsystem and start scheduler */
    task_init();
    ipc_init();
    scheduler_init();

    /* Should never reach here */
    early_puts("HALT\r\n");
    fb_console_write("HALT\r\n");
    while (1) {
        /* Wait for interrupt */
        asm volatile("wfi");
    }
}
