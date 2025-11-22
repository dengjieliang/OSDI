#include "common.h"
#include "uart.h"

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
    aux_enables = aux_enables | aux_enables_mask;
    mmio_write(AUX_ENABLES, aux_enables);

    //關閉TX/RX(資料接收的設定先關閉，以防變更設定時寫入錯誤資料至mmio)
    unsigned int now_aux_mu_cntl_reg = mmio_read(AUX_MU_CNTL_REG);
    unsigned int aux_mu_cntl_reg_mask = ~(3);
    now_aux_mu_cntl_reg = (now_aux_mu_cntl_reg & aux_mu_cntl_reg_mask);
    mmio_write(AUX_MU_CNTL_REG, now_aux_mu_cntl_reg);

    //關閉中斷，避免FIFO是空的，讓CPU不會觸發這裡的INTERRUPT導致陷入無窮迴圈
    unsigned int aux_mu_ier_reg = mmio_read(AUX_MU_IER_REG);
    unsigned int aux_mu_ier_reg_mask = ~(3);
    aux_mu_ier_reg = aux_mu_ier_reg & aux_mu_ier_reg_mask;
    mmio_write(AUX_MU_IER_REG, aux_mu_ier_reg);

    

    //清空FIFO
    unsigned int aux_mu_iir_reg = mmio_read(AUX_MU_IIR_REG);
    unsigned int aux_mu_iir_reg_mask = 3 << 1;
    aux_mu_iir_reg = aux_mu_iir_reg & aux_mu_iir_reg_mask;
    mmio_write(AUX_MU_IIR_REG, aux_mu_iir_reg);

    //決定mini UART資料格式
    unsigned int aux_mu_lcr_reg = mmio_read(AUX_MU_LCR_REG);
    unsigned int aux_mu_lcr_reg_mask = 3;
    aux_mu_lcr_reg = aux_mu_lcr_reg | aux_mu_lcr_reg_mask;
    mmio_write(AUX_MU_LCR_REG, aux_mu_lcr_reg);


}