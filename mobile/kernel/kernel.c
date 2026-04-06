/* Minimal kernel init for uOS(m) - User OS Mobile */

#define UART_BASE 0x10000000L
#define UART_TX 0x0

void uart_putc(char c) {
    volatile char *uart = (volatile char *)UART_BASE;
    while ((*(volatile unsigned char *)(UART_BASE + 0x5) & 0x20) == 0);
    *uart = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

const char boot_msg[] = "uOS(m) - User OS Mobile booting...\n";
const char hart_msg[] = "Hart ID: ";
const char hybrid_msg[] = "Hybrid kernel initialized\n";
const char posix_msg[] = "POSIX API ready\n";

/* Forward declarations for subsystems */
extern int mem_init(void);
extern int task_init(void);
extern int scheduler_init(void);
extern int ipc_init(void);
extern int vm_init(void);
extern int vfs_init(void);
extern void task_set_current(void *t);
extern void scheduler_run(void);

void kernel_init(unsigned long hartid, void *dtb) {
    /* Welcome message for uOS(m) */
    uart_puts(boot_msg);
    uart_puts(hart_msg);
    uart_putc('0' + hartid);
    uart_puts("\n");
    uart_puts(hybrid_msg);
    uart_puts(posix_msg);
    
    /* Initialize kernel subsystems */
    uart_puts("\n=== Initializing Kernel Subsystems ===\n");
    
    mem_init();
    vm_init();
    vfs_init();
    task_init();
    ipc_init();
    scheduler_init();
    
    uart_puts("=== All subsystems ready ===\n");
    uart_puts("uOS(m) kernel fully operational!\n");
    uart_puts("Architecture: Hybrid Kernel (Microkernel + Monolithic)\n");
    uart_puts("Features: POSIX API, IPC, Virtual Memory (SV39), VFS\n\n");
    
    /* Dummy kernel loop */
    while (1) {
        asm volatile("wfi");
    }
}

/* BSS section markers */
char bss_start[0];
char bss_end[0];