#ifndef COMMON_H
#define COMMON_H

//定義NULL
#ifndef NULL
#define NULL ((void *)0)
#endif

//定義BOOL類型
// 檢查是否已經有定義，避免跟編譯器內建的衝突
#ifndef __cplusplus  // 如果不是 C++ (C++ 內建 bool)
    
    // 使用 C99 標準的 _Bool
    // 如果編譯器版本很舊不支援 _Bool，可以改用 typedef int bool;
    typedef _Bool bool; 

    #ifndef true
    #define true 1
    #endif

    #ifndef false
    #define false 0
    #endif

#endif

#ifndef ALIGN4
#define ALIGN4(x) (((x) + 3) & ~3)
#endif

#ifndef ALIGN8
#define ALIGN8(x) (((x) + 7) & ~7)
#endif

#ifndef MAX_ARGS
#define MAX_ARGS 16
#endif

#ifndef MAX_STRING_SIZE
#define MAX_STRING_SIZE 1024
#endif

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