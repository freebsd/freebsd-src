/*
 * UART Character Device Driver
 * uOS(m) - User OS Mobile
 */

#include "../kernel/chardev.h"
#include <stdint.h>

#define UART_BUFFER_SIZE 1024

/* UART registers (QEMU virt machine) */
#define UART_BASE 0x10000000
#define UART_TX 0x0
#define UART_LSR 0x5
#define UART_LSR_TX_READY 0x20

/* UART buffer */
static char uart_tx_buffer[UART_BUFFER_SIZE];
static volatile uint32_t uart_tx_head = 0;
static volatile uint32_t uart_tx_tail = 0;

/* UART operations */
static int uart_open(void) {
    return 0;  /* UART is always ready */
}

static int uart_close(void) {
    return 0;
}

static ssize_t uart_read(void *buf, size_t count) {
    /* For now, UART read is not implemented (polling would block) */
    (void)buf;
    (void)count;
    return 0;
}

static ssize_t uart_write(const void *buf, size_t count) {
    const char *data = (const char *)buf;
    size_t written = 0;
    
    for (size_t i = 0; i < count; i++) {
        /* Add to buffer */
        uint32_t next_head = (uart_tx_head + 1) % UART_BUFFER_SIZE;
        if (next_head != uart_tx_tail) {
            uart_tx_buffer[uart_tx_head] = data[i];
            uart_tx_head = next_head;
            written++;
        } else {
            /* Buffer full, wait or drop */
            break;
        }
    }
    
    /* Start transmission if not already */
    uart_start_tx();
    
    return written;
}

static int uart_ioctl(unsigned long cmd, void *arg) {
    (void)cmd;
    (void)arg;
    return -1;  /* Not implemented */
}

/* UART device operations */
static chardev_ops_t uart_ops = {
    .open = uart_open,
    .close = uart_close,
    .read = uart_read,
    .write = uart_write,
    .ioctl = uart_ioctl,
};

/* UART device */
static chardev_t uart_device = {
    .name = "uart0",
    .ops = &uart_ops,
    .private_data = NULL,
};

/* Start UART transmission */
void uart_start_tx(void) {
    if (uart_tx_head != uart_tx_tail) {
        /* Enable TX interrupt */
        volatile uint8_t *ier = (volatile uint8_t *)(UART_BASE + 0x1);
        *ier |= 0x02;  /* IER_THRI */
    }
}

/* UART TX interrupt handler */
void uart_tx_interrupt(void) {
    while (uart_tx_head != uart_tx_tail) {
        if ((*(volatile uint8_t *)(UART_BASE + UART_LSR) & UART_LSR_TX_READY) == 0) {
            break;  /* TX not ready */
        }
        *(volatile uint8_t *)(UART_BASE + UART_TX) = uart_tx_buffer[uart_tx_tail];
        uart_tx_tail = (uart_tx_tail + 1) % UART_BUFFER_SIZE;
    }
    
    if (uart_tx_head == uart_tx_tail) {
        /* Disable TX interrupt */
        volatile uint8_t *ier = (volatile uint8_t *)(UART_BASE + 0x1);
        *ier &= ~0x02;  /* Clear IER_THRI */
    }
}

/* Initialize UART character device */
int uart_chardev_init(void) {
    /* Initialize UART */
    volatile uint8_t *lcr = (volatile uint8_t *)(UART_BASE + 0x3);
    *lcr = 0x03;  /* 8N1 */
    
    volatile uint8_t *fcr = (volatile uint8_t *)(UART_BASE + 0x2);
    *fcr = 0x07;  /* Enable FIFOs */
    
    return chardev_register(&uart_device);
}