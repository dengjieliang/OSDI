#ifndef COMMON_H
#define COMMON_H

typedef struct common_mmio_info
{
    unsigned long mmio_base;
    unsigned long gpio_base;
} CommonMmioInfoT;

extern CommonMmioInfoT common_mmio;
struct ctx;
void common_init_from_dtb(struct ctx* dtb_ctx);

//MMIO base address
#define MMIO_BASE       (common_mmio.mmio_base)

//GPIO registers
#define GPIO_BASE       (common_mmio.gpio_base)
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