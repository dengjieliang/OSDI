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

#define AUX_ENABLES_MASK (1)    //啟用 mini UART 的位元遮罩
#define AUX_TX_RX_DISABLE_MASK ~(3) // 關閉 TX/RX 的位元遮罩
#define AUX_TX_RX_ENABLE_MASK (3) // 開啟 TX/RX 的位元遮罩
#define AUX_INTERRUPT_DISABLE_MASK ~(3)  // 關閉中斷的位元遮罩
#define AUX_CLOSE_FLOW_CONTROL (0) // 關閉 flow control 的位元遮罩

//清空FIFO，這個暫存器讀出來的數值為中斷狀態。但寫進去的數值(只有第1、2bit可寫其他為READ-ONLY或Reserved Bits)
//代表是否清空FIFObit 1 = 1 → 清 RX FIFO，bit 2 = 1 → 清 TX FIFO
#define AUX_CLEAR_TX_RX_FIFO 0x06
#define AUX_TX_FIFO_EMPTY (1 << 5) // 或 0x20，表示 TX FIFO 為空，可以寫入資料
#define AUX_MINI_UART_DATA_TYPE 3 // 8-bit data
#define AUX_BAUD_RATE_115200 270 // 設定傳送速度為 115200 baud rate

#define GPPUD_SETUP_VALUE 0 // disable pull-up/down
#define GPPUD_CLEAR_VALUE 0 // 清除 GPPUD 設定
#define GPPUD_SETUP_TIME 150

#define GPPUDCLK0_CLEAR_VALUE 0 // 清除 GPPUDCLK0 設定
#define GPPUDCLK0_MASK_GPIO14_15 (3 << 14) // GPIO14、GPIO15 的遮罩

#define GPFSEL1_CLEAR_VALUE ~(63 << 12) // 清除 GPFSEL1 的 GPIO14、GPIO15 設定
#define GPFSEL1_ALT5_GPIO14_15 (18 << 12) // 設定 GPIO14、GPIO15 為 ALT5 (mini UART)

void uart_init()
{
    //控制 mini UART啟用
    unsigned int aux_enables = mmio_read(AUX_ENABLES);
    aux_enables |= AUX_ENABLES_MASK;
    mmio_write(AUX_ENABLES, aux_enables);

    //關閉TX/RX(資料接收的設定先關閉，以防變更設定時寫入錯誤資料至mmio)
    unsigned int aux_mu_cntl_reg = mmio_read(AUX_MU_CNTL_REG);
    aux_mu_cntl_reg &= AUX_TX_RX_DISABLE_MASK;
    mmio_write(AUX_MU_CNTL_REG, aux_mu_cntl_reg);

    //關閉中斷，避免在動FIFO時CPU觸發這裡的INTERRUPT導致陷入無窮迴圈
    unsigned int aux_mu_ier_reg = mmio_read(AUX_MU_IER_REG);
    aux_mu_ier_reg &= AUX_INTERRUPT_DISABLE_MASK;
    mmio_write(AUX_MU_IER_REG, aux_mu_ier_reg);

    //控制 Flow Control (流量控制) 的訊號線，避免cpu受 flow control 干擾
    mmio_write(AUX_MU_MCR_REG, AUX_CLOSE_FLOW_CONTROL);

    //清空FIFO，這個暫存器讀出來的數值為中斷狀態。但寫進去的數值(只有第1、2bit可寫其他為READ-ONLY或Reserved Bits)
    //代表是否清空FIFObit 1 = 1 → 清 RX FIFO，bit 2 = 1 → 清 TX FIFO
    mmio_write(AUX_MU_IIR_REG, AUX_CLEAR_TX_RX_FIFO);

    //決定mini UART資料格式
    mmio_write(AUX_MU_LCR_REG, AUX_MINI_UART_DATA_TYPE);

    //設定傳送速度
    mmio_write(AUX_MU_BAUD_REG, AUX_BAUD_RATE_115200);

    //disable pull-up/down
    mmio_write(GPPUD, GPPUD_SETUP_VALUE);

    //需delay 150 cycle以上之後再做將GPIO 14、15設為GPPUD的設定
    delay_cycles(GPPUD_SETUP_TIME);

    //先將gppudclk0清零，預防之前有設定過殘留
    unsigned int gppudclk0 = GPPUDCLK0_CLEAR_VALUE;

    //將GPIO 14、15設為GPPUD的設定
    gppudclk0 |= GPPUDCLK0_MASK_GPIO14_15;
    mmio_write(GPPUDCLK0, gppudclk0);

    //需delay 150 cycle以上之後再清除GPPUD的設定
    delay_cycles(GPPUD_SETUP_TIME);

    //清除GPPUD的設定
    mmio_write(GPPUD, GPPUD_CLEAR_VALUE);

    //清除GPIO 14、15的GPPUD設定
    mmio_write(GPPUDCLK0, GPPUDCLK0_CLEAR_VALUE);


    //將GPFSEL1清零後設為ALT5(GPIO14、GPIO15使用MINI UART)
    unsigned int gpfsel1= mmio_read(GPFSEL1);
    gpfsel1 &= GPFSEL1_CLEAR_VALUE;
    gpfsel1 |= GPFSEL1_ALT5_GPIO14_15;
    mmio_write(GPFSEL1, gpfsel1);

    //開啟TX/RX
    aux_mu_cntl_reg = mmio_read(AUX_MU_CNTL_REG);
    aux_mu_cntl_reg |= AUX_TX_RX_ENABLE_MASK;
    mmio_write(AUX_MU_CNTL_REG, aux_mu_cntl_reg);
}

void uart_send(char c)
{
    //bit 5: TX 可寫
    while ((mmio_read(AUX_MU_LSR_REG) & AUX_TX_FIFO_EMPTY) == 0)
    {
        asm volatile("nop");
    }
    //寫入資料
    mmio_write(AUX_MU_IO_REG, c);
}

void delay_cycles(unsigned int time)
{
    for (unsigned int i = 0; i < time; i++)
    {
        asm volatile("nop");
    }
}