#include "../header/uart.h"
#include "../header/shell.h"
#include "../header/mailbox.h"

void kernel_main(void)
{
    //將GPIO接到 mini uart
    uart_init();

    //寫入資料到MailBox Buffer
    prepare_board_revision_request();

    //呼叫mailbox_call確認GPU
    mailbox_call(MBOX_CH_PROP);

    if (mailbox_get_status() == 0x80000000)
    {
        uart_puts("Board Revision: ");
        uart_send_hex(mailbox_get_board_revision());
    }

    //獲取使用者輸入
    shell_main();
}