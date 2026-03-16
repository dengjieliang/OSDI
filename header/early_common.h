#ifndef EARLY_COMMON_H
#define EARLY_COMMON_H

#define KERNEL_LOAD_ADDRESS 0x80000UL

//MMIO base address
#define MMIO_BASE       0x3F000000

//GPIO registers
#define GPIO_BASE       (MMIO_BASE + 0x200000)
#define GPFSEL1        (GPIO_BASE + 0x04)
#define GPPUD         (GPIO_BASE + 0x94)
#define GPPUDCLK0     (GPIO_BASE + 0x98)

static inline void mmio_write(unsigned long reg, unsigned int data)
{
    *(volatile unsigned int *)reg = data;
}

static inline unsigned int mmio_read(unsigned long reg)
{
    return *(volatile unsigned int *)reg;
}

#endif