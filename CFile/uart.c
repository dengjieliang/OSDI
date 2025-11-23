#include "../header/common.h"
#include "../header/uart.h"

//UART registers
#define AUX_BASE       (MMIO_BASE + 0x215000)
#define AUX_ENABLES    (AUX_BASE + 0x04)    //控制啟用 mini UART
#define AUX_MU_IO_REG  (AUX_BASE + 0x40)    //收/發資料
#define AUX_MU_IER_REG (AUX_BASE + 0x44)    //中斷 enable
#define AUX_MU_LCR_REG (AUX_BASE + 0x4C)    //資料格式，例如 8-bit
#define AUX_MU_IIR_REG (AUX_BASE + 0x48)    //中斷狀態/FIFO 控制
#define AUX_MU_MCR_REG (AUX_BASE + 0x50)    //modem control，關閉 flow control
#define AUX_MU_LSR_REG (AUX_BASE + 0x54)    //line status，確認 TX 可寫、RX 有資料
#define AUX_MU_CNTL_REG (AUX_BASE + 0x60)   //啟用/關閉 TX/RX
#define AUX_MU_STAT_REG (AUX_BASE + 0x64)   //存更多狀態資訊，很多人實作時只用 LSR 也行。也可以從這裡看 FIFO 狀態。
#define AUX_MU_BAUD_REG (AUX_BASE + 0x68)   //baud rate

void uart_init()
{
    //控制 mini UART啟用
    unsigned int aux_enables = mmio_read(AUX_ENABLES);
    unsigned int aux_enables_mask = 1;
    aux_enables |= aux_enables_mask;
    mmio_write(AUX_ENABLES, aux_enables);

    //關閉TX/RX(資料接收的設定先關閉，以防變更設定時寫入錯誤資料至mmio)
    unsigned int aux_mu_cntl_reg = mmio_read(AUX_MU_CNTL_REG);
    unsigned int aux_mu_cntl_reg_mask = ~(3);
    aux_mu_cntl_reg &= aux_mu_cntl_reg_mask;
    mmio_write(AUX_MU_CNTL_REG, aux_mu_cntl_reg);

    //關閉中斷，避免在動FIFO時CPU觸發這裡的INTERRUPT導致陷入無窮迴圈
    unsigned int aux_mu_ier_reg = mmio_read(AUX_MU_IER_REG);
    unsigned int aux_mu_ier_reg_mask = ~(3);
    aux_mu_ier_reg &= aux_mu_ier_reg_mask;
    mmio_write(AUX_MU_IER_REG, aux_mu_ier_reg);

    //控制 Flow Control (流量控制) 的訊號線，避免cpu受 flow control 干擾
    mmio_write(AUX_MU_MCR_REG, 0);

    //清空FIFO，這個暫存器讀出來的數值為中斷狀態。但寫進去的數值(只有第1、2bit可寫其他為READ-ONLY或Reserved Bits)
    //代表是否清空FIFObit 1 = 1 → 清 RX FIFO，bit 2 = 1 → 清 TX FIFO
    unsigned int aux_mu_iir_reg_value = 0x06;
    mmio_write(AUX_MU_IIR_REG, aux_mu_iir_reg_value);

    //決定mini UART資料格式
    unsigned int aux_mu_lcr_reg = mmio_read(AUX_MU_LCR_REG);
    unsigned int aux_mu_lcr_reg_mask = 3;
    aux_mu_lcr_reg |= aux_mu_lcr_reg_mask;
    mmio_write(AUX_MU_LCR_REG, aux_mu_lcr_reg);

    //設定傳送速度
    unsigned int aux_mu_baud_reg_value = 270;
    mmio_write(AUX_MU_BAUD_REG, aux_mu_baud_reg_value);

    //先將gppudclk0清零，預防之前有設定過
    mmio_write(GPPUDCLK0, 0);

    //disable pull-up/down
    unsigned int gppud = mmio_read(GPPUD);
    unsigned int gppud_mask = ~(3);
    gppud &= gppud_mask;
    mmio_write(GPPUD, gppud);

    //需delay 150 cycle以上之後再做將GPIO 14、15設為GPPUD的設定
    delay_cycles(150);

    //將GPIO 14、15設為GPPUD的設定
    unsigned int gppudclk0 = mmio_read(GPPUDCLK0);
    unsigned int gppudclk0_mask = 3 << 14;
    gppudclk0 |= gppudclk0_mask;
    mmio_write(GPPUDCLK0, gppudclk0);

    //需delay 150 cycle以上之後再清除GPPUD的設定
    delay_cycles(150);

    //清除GPPUD的設定
    mmio_write(GPPUD, 0);

    //清除GPIO 14、15的GPPUD設定
    mmio_write(GPPUDCLK0, 0);


    //將GPFSEL1清零後設為ALT5(GPIO14、GPIO15使用MINI UART)
    unsigned int gpfsel1= mmio_read(GPFSEL1);
    unsigned int gpfsel1_mask = ~(63 << 12);
    gpfsel1 = gpfsel1 & gpfsel1_mask;
    mmio_write(GPFSEL1, gpfsel1);
    gpfsel1_mask = 18 << 12;
    gpfsel1 = gpfsel1 | gpfsel1_mask;
    mmio_write(GPFSEL1, gpfsel1);

    //開啟TX/RX
    aux_mu_cntl_reg = mmio_read(AUX_MU_CNTL_REG);
    aux_mu_cntl_reg_mask = (3);
    aux_mu_cntl_reg = (aux_mu_cntl_reg | aux_mu_cntl_reg_mask);
    mmio_write(AUX_MU_CNTL_REG, aux_mu_cntl_reg);
}

void delay_cycles(unsigned int time)
{
    for (unsigned int i = 0; i < time; i++)
    {
        asm volatile("nop");
    }
}