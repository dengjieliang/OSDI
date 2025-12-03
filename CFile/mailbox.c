#include "../header/common.h"
#include "../header/mailbox.h"

#define BOARD_BUFFER_SIZE (7)
#define MEMORY_BUFFER_SIZE (8)

#define MBOX_CAN_NOT_WRITE 0X80000000
#define MBOX_CAN_NOT_READ 0X40000000

typedef struct
{
    unsigned int buffer_size;
    unsigned int request_and_response;
    unsigned int tag;
    unsigned int value_buffer_size;
    unsigned int value_buffer_recv_size;
    unsigned int value_buffer;
    unsigned int end_tag;
} mbox_board_recv_t;

//當
_Static_assert(sizeof(mbox_board_recv_t) == sizeof(unsigned int) * BOARD_BUFFER_SIZE, "Error: mbox_board_recv_t size is not 32 bytes! Check for padding.");

typedef struct
{
    unsigned int buffer_size;
    unsigned int request_and_response;
    unsigned int tag;
    unsigned int value_buffer_size;
    unsigned int value_buffer_recv_size;
    unsigned int base_addr;
    unsigned int mem_size;
    unsigned int end_tag;
} mbox_memory_t;

_Static_assert(sizeof(mbox_memory_t) == sizeof(unsigned int) * MEMORY_BUFFER_SIZE, "Error: mbox_memory_t size is not 32 bytes! Check for padding.");

unsigned int mailbox_buffer[8] __attribute__((aligned(16)));

void mailbox_call(unsigned int channel)
{
    while((mmio_read(MBOX_STATUS) & MBOX_CAN_NOT_WRITE) != 0)
    {
        asm volatile("nop");
    }

    //轉換資料記憶體位置變成unsigned int，用mmio
    unsigned int buffer_address = (unsigned int)((unsigned long)mailbox_buffer);
    buffer_address = (buffer_address & ~(0xF)) | channel;
    mmio_write(MBOX_WRITE, buffer_address);

    while (1)
    {
        while ((mmio_read(MBOX_STATUS) & MBOX_CAN_NOT_READ) != 0)
        {
            asm volatile("nop");
        }

        unsigned int Mbox_Channel = mmio_read(MBOX_READ);

        if ((Mbox_Channel & 0xF) == channel)
        {
            break;
        }
    }
}

void prepare_board_revision_request()
{
    mbox_board_recv_t* req = (mbox_board_recv_t *)mailbox_buffer;
    req->buffer_size = sizeof(mbox_board_recv_t);
    req->request_and_response = 0;
    req->tag = 0x00010002;
    req->value_buffer_size = 4;
    req->value_buffer_recv_size = 0;
    req->value_buffer = 0;
    req->end_tag = 0;
}

// 取得GPU回傳狀態碼
unsigned int get_board_status() 
{
    mbox_board_recv_t* req = (mbox_board_recv_t *)mailbox_buffer;
    return req->request_and_response;
}

// 取得 Board Revision 的值
unsigned int get_board_revision() 
{
    mbox_board_recv_t* req = (mbox_board_recv_t *)mailbox_buffer;
    return req->value_buffer;
}
void prepare_memory_request()
{
    mbox_memory_t* req = (mbox_memory_t *)mailbox_buffer;
    req->buffer_size = sizeof(mbox_memory_t);
    req->request_and_response = 0;
    req->tag = 0x00010005;
    req->value_buffer_size = 8;
    req->value_buffer_recv_size = 0;
    req->base_addr = 0;
    req->mem_size = 0;
    req->end_tag = 0;
}

//取得memory回傳狀態
unsigned int get_memory_status() 
{
    mbox_memory_t* req = (mbox_memory_t *)mailbox_buffer;
    return req->request_and_response;
}

// 取得 memory 的大小
unsigned int get_memory_size() 
{
    mbox_memory_t* req = (mbox_memory_t *)mailbox_buffer;
    return req->mem_size;
}



