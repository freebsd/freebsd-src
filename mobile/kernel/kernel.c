/* Minimal kernel init for uOS(m) - User OS Mobile */

#include "vfs.h"
#include "hybridfs.h"
#include "task.h"
#include "chardev.h"
#include "interrupt.h"
#include "../drivers/uart.h"
#include "../drivers/virtio_net.h"

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
extern int chardev_init(void);
extern int uart_chardev_init(void);
extern int interrupt_init(void);
extern int pmp_init(void);
extern int syscall_security_init(void);
extern int aslr_init(void);
extern int stack_canary_init(void);
extern int mobile_ui_init(void);
extern int mobile_ui_start(void);
extern void mobile_ui_event_loop(void);
extern void mobile_ui_handle_touch(int x, int y, int action);
extern filesystem_t *get_hybrid_fs(void);
extern void task_set_current(void *t);
extern void scheduler_run(void);
extern void (*irq_handlers[256])(void);

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
    pmp_init();                    /* Physical Memory Protection */
    syscall_security_init();       /* System call security */
    aslr_init();                   /* Address Space Layout Randomization */
    stack_canary_init();           /* Stack overflow protection */
    vfs_init();
    vfs_register_fs(get_hybrid_fs());
    vfs_mount(get_hybrid_fs(), "/");
    chardev_init();
    uart_chardev_init();
    interrupt_init();
    virtio_net_init();
    mobile_ui_init();              /* Mobile UI system */
    mobile_ui_start();             /* Start UI */
    task_init();
    ipc_init();
    scheduler_init();
    
    uart_puts("=== All subsystems ready ===\n");
    uart_puts("uOS(m) kernel fully operational!\n");
    uart_puts("Architecture: Hybrid Kernel (Microkernel + Monolithic)\n");
    uart_puts("Features: POSIX API, IPC, Virtual Memory (SV39), VFS\n\n");

    /* Hand over control to the scheduler */
    scheduler_run();
}

/* Trap handler */
void trap_handler(void) {
    uint64_t cause, epc, tval;
    asm volatile("csrr %0, scause" : "=r"(cause));
    asm volatile("csrr %0, sepc" : "=r"(epc));
    asm volatile("csrr %0, stval" : "=r"(tval));

    if (cause & (1UL << 63)) {
        /* Interrupt */
        uint64_t intr_cause = cause & ~(1UL << 63);
        if (intr_cause == 9) {  /* Supervisor external interrupt */
            handle_external_interrupt();
        } else {
            uart_puts("Unhandled interrupt: ");
            uart_putc('0' + (intr_cause % 10));
            uart_puts("\n");
        }
    } else {
        /* Exception */
        uart_puts("Exception: ");
        uart_putc('0' + (cause % 10));
        uart_puts(" at ");
        uart_putc('0' + ((epc >> 4) % 16));
        uart_putc('0' + (epc % 16));
        uart_puts("\n");
        while (1);  /* Halt on exception */
    }
}

/* Handle external interrupts (PLIC) */
void handle_external_interrupt(void) {
    volatile uint32_t *plic_claim = (volatile uint32_t *)0x0c200004;
    uint32_t irq = *plic_claim;
    
    if (irq_handlers[irq]) {
        irq_handlers[irq]();
    } else {
        uart_puts("Unhandled IRQ: ");
        uart_putc('0' + (irq % 10));
        uart_puts("\n");
    }
    
    /* Complete the interrupt */
    *plic_claim = irq;
}

/* BSS section markers */
char bss_start[0];
char bss_end[0];