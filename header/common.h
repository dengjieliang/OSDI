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