#include "../header/common.h"
#include "../header/uart.h"
#include "../header/mailbox.h"

#define BUFFER_SIZE (0)
#define REQUEST_AND_RESPONSE (1)
#define TAG_ID (2)
#define VALUE_BUFFER_SIZE (3)
#define VALUE_LENGTH (4)
#define VALUE_BUFFER (5)
#define END_TAG (6)

#define MAILBOX_BUFFER_SIZE (7)

#define MBOX_CAN_WRITE 0X80000000
#define MBOX_CAN_READ 0X40000000

unsigned int mailbox_buffer[7] __attribute__((aligned(16)));

void mailbox_call(unsigned int channel)
{
    while((mmio_read(MBOX_STATUS) & MBOX_CAN_WRITE) != 0)
    {
        asm volatile("nop");
    }

    //轉換資料記憶體位置變成unsigned int，用mmio
    unsigned int buffer_address = (unsigned int)((unsigned long)mailbox_buffer);
    buffer_address = (buffer_address & ~(0xF)) | (0x8);
    mmio_write(MBOX_WRITE, buffer_address);

    while (1)
    {
        while ((mmio_read(MBOX_STATUS) & MBOX_CAN_READ) != 0)
        {
            asm volatile("nop");
        }

        unsigned int Mbox_Channel = mmio_read(MBOX_READ);

        if ((Mbox_Channel & 0xF) == 0x8)
        {
            break;
        }
    }
}

void prepare_board_revision_request()
{
    mailbox_buffer[BUFFER_SIZE] = MAILBOX_BUFFER_SIZE * sizeof(unsigned int);
    mailbox_buffer[REQUEST_AND_RESPONSE] = 0;
    mailbox_buffer[TAG_ID] = 0x00010002;
    mailbox_buffer[VALUE_BUFFER_SIZE] = 4;
    mailbox_buffer[VALUE_LENGTH] = 0;
    mailbox_buffer[VALUE_BUFFER] = 0;
    mailbox_buffer[END_TAG] = 0;
}

// 取得請求狀態碼 (通常在 index 1)
unsigned int mailbox_get_status() 
{
    return mailbox_buffer[REQUEST_AND_RESPONSE];
}

// 取得 Board Revision 的值 (根據您的定義，在 index 5)
unsigned int mailbox_get_board_revision() 
{
    return mailbox_buffer[VALUE_BUFFER];
}



