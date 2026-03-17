#include "../header/common.h"
#include "../header/uart.h"
#include "../header/fdtb.h"

#define EARLY_AUX_BASE       (0x3F000000UL + 0x215000UL)
#define EARLY_AUX_MU_IO_REG  (EARLY_AUX_BASE + 0x40UL)
#define EARLY_AUX_MU_LSR_REG (EARLY_AUX_BASE + 0x54UL)

typedef struct uart_reg_info
{
    unsigned long aux_base;
    unsigned long uart_base;
    unsigned long irq_base;

    unsigned long aux_enables;
    unsigned long aux_mu_io_reg;
    unsigned long aux_mu_ier_reg;
    unsigned long aux_mu_iir_reg;
    unsigned long aux_mu_lcr_reg;
    unsigned long aux_mu_mcr_reg;
    unsigned long aux_mu_lsr_reg;
    unsigned long aux_mu_cntl_reg;
    unsigned long aux_mu_stat_reg;
    unsigned long aux_mu_baud_reg;
    unsigned long irq_enable1;

    bool is_ready;
} UartRegInfoT;

static UartRegInfoT uart_reg_info;

#define AUX_ENABLES_MASK (1)    //啟用 mini UART 的位元遮罩
#define AUX_TX_RX_DISABLE_MASK ~(3) // 關閉 TX/RX 的位元遮罩
#define AUX_TX_RX_ENABLE_MASK (3) // 開啟 TX/RX 的位元遮罩
#define AUX_INTERRUPT_DISABLE_MASK ~(3)  // 關閉中斷的位元遮罩
#define AUX_CLOSE_FLOW_CONTROL (0) // 關閉 flow control 的位元遮罩

//清空FIFO，這個暫存器讀出來的數值為中斷狀態。但寫進去的數值(只有第1、2bit可寫其他為READ-ONLY或Reserved Bits)
//代表是否清空FIFObit 1 = 1 → 清 RX FIFO，bit 2 = 1 → 清 TX FIFO
#define AUX_CLEAR_TX_RX_FIFO 0x06
#define AUX_MU_IIR_INT_RX 0X04 // bit 2 = 1 → RX FIFO 中有資料可讀
#define AUX_MU_IIR_INT_TX 0X02 // bit 1 = 1 → TX FIFO 可寫
#define AUX_TX_FIFO_EMPTY (1 << 5) // 或 0x20，表示 TX FIFO 為空，可以寫入資料
#define AUX_RX_DATA_READY 0X01 // LSR bit0=1 表示 RX FIFO 有資料可讀
#define AUX_MU_IER_RX_ENABLE 0X01 // Bit 0
#define AUX_MU_IER_TX_ENABLE 0X02 // Bit 1
#define AUX_CHAR_MASK 0xFF // 只取資料的低 8 bits讀取為CHAR傳回給CPU
#define AUX_MINI_UART_DATA_TYPE 3 // 8-bit data
#define AUX_BAUD_RATE_115200 270 // 設定傳送速度為 115200 baud rate
#define UART_RX_PUMP_BUDGET 16 // 每次 IRQ 最多搬運的 RX bytes 數（避免 RX 長時間佔用 IRQ，保留 TX 公平性）

#define GPPUD_SETUP_VALUE 0 // disable pull-up/down
#define GPPUD_CLEAR_VALUE 0 // 清除 GPPUD 設定
#define GPPUD_SETUP_TIME 150

#define GPPUDCLK0_CLEAR_VALUE 0 // 清除 GPPUDCLK0 設定
#define GPPUDCLK0_MASK_GPIO14_15 (3 << 14) // GPIO14、GPIO15 的遮罩

#define GPFSEL1_CLEAR_VALUE ~(63 << 12) // 清除 GPFSEL1 的 GPIO14、GPIO15 設定
#define GPFSEL1_ALT5_GPIO14_15 (18 << 12) // 設定 GPIO14、GPIO15 為 ALT5 (mini UART)

#define MAX_BUFFER_SIZE 1024
static char rx_buffer[MAX_BUFFER_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

static char tx_buffer[MAX_BUFFER_SIZE];
static volatile int tx_head = 0;
static volatile int tx_tail = 0;

// 將硬體 RX FIFO 中的資料搬到 software ring buffer
// budget: 本次 IRQ 最多搬運的字節數，用於 RX/TX fairness
static void uart_rx_pump(unsigned int budget)
{
    while (budget > 0 && (mmio_read(uart_reg_info.aux_mu_lsr_reg) & AUX_RX_DATA_READY))
    {
        unsigned int read_byte = mmio_read(uart_reg_info.aux_mu_io_reg);
        char c = (char)(read_byte & AUX_CHAR_MASK);

        int next_tail = (rx_tail + 1) % MAX_BUFFER_SIZE;
        if (next_tail == rx_head)
        {
            rx_head = (rx_head + 1) % MAX_BUFFER_SIZE;
        }

        rx_buffer[rx_tail] = c;
        rx_tail = next_tail;
        budget -= 1;
    }
}

static void uart_assemble_registers(void)
{
    extern CtxT dtb_ctx;

    uart_reg_info.aux_base = dtb_ctx.aux_mmio_base;
    uart_reg_info.uart_base = dtb_ctx.uart_mmio_base;
    uart_reg_info.irq_base = dtb_ctx.interrupt_info.arm_ctrl_intc_base;

    uart_reg_info.aux_enables = uart_reg_info.aux_base + 0x04;

    uart_reg_info.aux_mu_io_reg = uart_reg_info.uart_base + 0x00;
    uart_reg_info.aux_mu_ier_reg = uart_reg_info.uart_base + 0x04;
    uart_reg_info.aux_mu_iir_reg = uart_reg_info.uart_base + 0x08;
    uart_reg_info.aux_mu_lcr_reg = uart_reg_info.uart_base + 0x0C;
    uart_reg_info.aux_mu_mcr_reg = uart_reg_info.uart_base + 0x10;
    uart_reg_info.aux_mu_lsr_reg = uart_reg_info.uart_base + 0x14;
    uart_reg_info.aux_mu_cntl_reg = uart_reg_info.uart_base + 0x20;
    uart_reg_info.aux_mu_stat_reg = uart_reg_info.uart_base + 0x24;
    uart_reg_info.aux_mu_baud_reg = uart_reg_info.uart_base + 0x28;

    uart_reg_info.irq_enable1 = uart_reg_info.irq_base + 0x10;

    uart_reg_info.is_ready = true;
}

void uart_init()
{
    if (uart_reg_info.is_ready == false)
    {
        uart_assemble_registers();
    }

    //控制 mini UART啟用
    unsigned int aux_enables = mmio_read(uart_reg_info.aux_enables);
    aux_enables |= AUX_ENABLES_MASK;
    mmio_write(uart_reg_info.aux_enables, aux_enables);

    //關閉TX/RX(資料接收的設定先關閉，以防變更設定時寫入錯誤資料至mmio)
    unsigned int aux_mu_cntl_reg = mmio_read(uart_reg_info.aux_mu_cntl_reg);
    aux_mu_cntl_reg &= AUX_TX_RX_DISABLE_MASK;
    mmio_write(uart_reg_info.aux_mu_cntl_reg, aux_mu_cntl_reg);

    //關閉中斷，避免在動FIFO時CPU觸發這裡的INTERRUPT導致陷入無窮迴圈
    unsigned int aux_mu_ier_reg = mmio_read(uart_reg_info.aux_mu_ier_reg);
    aux_mu_ier_reg &= AUX_INTERRUPT_DISABLE_MASK;
    mmio_write(uart_reg_info.aux_mu_ier_reg, aux_mu_ier_reg);

    //控制 Flow Control (流量控制) 的訊號線，避免cpu受 flow control 干擾
    mmio_write(uart_reg_info.aux_mu_mcr_reg, AUX_CLOSE_FLOW_CONTROL);

    //清空FIFO，這個暫存器讀出來的數值為中斷狀態。但寫進去的數值(只有第1、2bit可寫其他為READ-ONLY或Reserved Bits)
    //代表是否清空FIFObit 1 = 1 → 清 RX FIFO，bit 2 = 1 → 清 TX FIFO
    mmio_write(uart_reg_info.aux_mu_iir_reg, AUX_CLEAR_TX_RX_FIFO);

    //決定mini UART資料格式
    mmio_write(uart_reg_info.aux_mu_lcr_reg, AUX_MINI_UART_DATA_TYPE);

    //設定傳送速度
    mmio_write(uart_reg_info.aux_mu_baud_reg, AUX_BAUD_RATE_115200);

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
}

void uart_init_dynamic()
{
    uart_assemble_registers();
    uart_init();
}

void uart_aux_mu_cntl_reg()
{
    unsigned int aux_mu_cntl_reg = mmio_read(uart_reg_info.aux_mu_cntl_reg);
    aux_mu_cntl_reg |= AUX_TX_RX_ENABLE_MASK;
    mmio_write(uart_reg_info.aux_mu_cntl_reg, aux_mu_cntl_reg);
}

void uart_open_ier_reg()
{
    unsigned int ier = mmio_read(uart_reg_info.aux_mu_ier_reg);
    ier |= AUX_MU_IER_RX_ENABLE; // 0x01
    mmio_write(uart_reg_info.aux_mu_ier_reg, ier);

    // [新增] 啟用第二層中斷控制器的 AUX IRQ (Bit 29)
    unsigned int enable_irq1 = mmio_read(uart_reg_info.irq_enable1);
    enable_irq1 |= (1 << 29);
    mmio_write(uart_reg_info.irq_enable1, enable_irq1);
}

void uart_interrupt_handler()
{
    // IIR 是當下快照：可用來判斷這次 IRQ 主要來源
    unsigned int iir = mmio_read(uart_reg_info.aux_mu_iir_reg);

    bool rx_iir_hit = ((iir & AUX_CLEAR_TX_RX_FIFO) == AUX_MU_IIR_INT_RX);
    bool tx_iir_hit = ((iir & AUX_CLEAR_TX_RX_FIFO) == AUX_MU_IIR_INT_TX);

    // 不能只依賴 rx_iir_hit：IIR 讀完到分支判斷之間，新的 RX byte 仍可能到達。
    // 因此用「rx_iir_hit || LSR data-ready」提高穩定性，避免延後讀取造成堆積。
    if (rx_iir_hit || (mmio_read(uart_reg_info.aux_mu_lsr_reg) & AUX_RX_DATA_READY))
    {
        uart_rx_pump(UART_RX_PUMP_BUDGET);
    }
    
    // TX 每次 IRQ 只送一個 byte；其餘交給下一次 TX IRQ 持續送
    if (tx_iir_hit)
    {
        //寫出至螢幕
        if (tx_head != tx_tail)
        {
            char c = tx_buffer[tx_head];
            tx_head = (tx_head + 1) % MAX_BUFFER_SIZE;
            mmio_write(uart_reg_info.aux_mu_io_reg, c);
        }
        else
        {
            // 寫出時是寫出完成觸發中斷，因此若無東西可寫需清除 TX 中斷狀態，避免重複觸發
            unsigned int ier = mmio_read(uart_reg_info.aux_mu_ier_reg);
            ier &= ~AUX_MU_IER_TX_ENABLE;
            mmio_write(uart_reg_info.aux_mu_ier_reg, ier);
        }
    }
}

void async_uart_send(char c)
{
    int next_tail = (tx_tail + 1) % MAX_BUFFER_SIZE;
    while (next_tail == tx_head)
    {
        asm volatile("nop");
    }

    bool queue_was_empty = (tx_head == tx_tail);

    tx_buffer[tx_tail] = c;
    tx_tail = next_tail;

    if (queue_was_empty && (mmio_read(uart_reg_info.aux_mu_lsr_reg) & AUX_TX_FIFO_EMPTY))
    {
        char first_byte = tx_buffer[tx_head];
        tx_head = (tx_head + 1) % MAX_BUFFER_SIZE;
        mmio_write(uart_reg_info.aux_mu_io_reg, first_byte);
    }

    if (tx_head != tx_tail)
    {
        unsigned int ier = mmio_read(uart_reg_info.aux_mu_ier_reg);
        ier |= AUX_MU_IER_TX_ENABLE;
        mmio_write(uart_reg_info.aux_mu_ier_reg, ier);
    }

    return;
}

void async_uart_puts(const char *s)
{
    while (*s != '\0')
    {
        //換行字元前先加上回車字元
        if (*s == '\n')
        {
            async_uart_send('\r');
        }

        async_uart_send(*s++);
    }
}

char async_uart_recv()
{
    while (rx_head == rx_tail)
    {
        asm volatile("nop");
    }

    char c = rx_buffer[rx_head];
    rx_head = (rx_head + 1) % MAX_BUFFER_SIZE;
    return c;
}

unsigned int async_uart_recv_uint()
{
    unsigned int size = 0;

    //因為python是Little Endian
    for (int i = 0; i < 4; i++)
    {
        char tmp = async_uart_recv();
        size |= (((unsigned char)tmp) << i * 8);
    }

    return size;
}

void async_uart_send_integer(int number)
{
    if (number == 0)
    {
        async_uart_send('0');
        return;
    }

    if (number < 0)
    {
        async_uart_send('-');
        number = -number;
    }

    char buffer[100];
    unsigned int digit_size = 0;

    while(number > 0)
    {
        buffer[digit_size] = (number % 10) + '0';
        number /= 10;
        digit_size += 1;
    }

    for (int i = digit_size - 1; i >= 0; i--)
    {
        async_uart_send(buffer[i]);
    }
}

void async_uart_send_unsigned_long_integer(unsigned long number)
{
    if (number == 0)
    {
        async_uart_send('0');
        return;
    }

    if (number < 0)
    {
        async_uart_send('-');
        number = -number;
    }

    char buffer[100];
    unsigned int digit_size = 0;

    while(number > 0)
    {
        buffer[digit_size] = (number % 10) + '0';
        number /= 10;
        digit_size += 1;
    }

    for (int i = digit_size - 1; i >= 0; i--)
    {
        async_uart_send(buffer[i]);
    }
}

void async_uart_send_decimal_part(int number, unsigned int digit_size)
{
    char buffer[100];
    unsigned int has_number_size = 0;

    while(number > 0)
    {
        buffer[has_number_size] = (number % 10) + '0';
        number /= 10;
        has_number_size += 1;
    }

    while (has_number_size < digit_size)
    {
        buffer[has_number_size] = '0';
        has_number_size += 1;
    }

    for (int i = has_number_size - 1; i >= 0; i--)
    {
        async_uart_send(buffer[i]);
    }
}

void async_uart_send_hex(unsigned int number)
{
    for (int hex_section = 28; hex_section >= 0; hex_section -= 4)
    {
        unsigned int hex_number = number & (0xF << hex_section);
        hex_number = (hex_number >> hex_section);
        char output;

        if (hex_number >= 10)
        {
            output = 'A';
            output = output + (hex_number - 10);
        }
        else
        {
            output = hex_number + '0';
        }

        async_uart_send(output);
    }

    async_uart_puts("\n");
}

void delay_cycles(unsigned int time)
{
    for (unsigned int i = 0; i < time; i++)
    {
        asm volatile("nop");
    }
}