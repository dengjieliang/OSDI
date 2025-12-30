#include "../header/cpio.h"
#include "../header/string.h"
#include "../header/utils.h"
#include "../header/uart.h"
#include "../header/common.h"

typedef struct cpio_header
{
    char c_magic[6];    //魔術數字。固定為字串 "070701"。這是你用來判斷「這裡是不是一個新檔案開頭」的依據。
    char c_ino[8];  //檔案的 unique id
    char c_mode[8]; //檔案權限與類型。判斷它是「檔案」還是「目錄」
    char c_uid[8];  //User ID
    char c_gid[8];  //Group ID
    char c_nlink[8];    //有多少個檔名指向這個 Inode
    char c_mtime[8];    //修改時間
    char c_filesize[8]; //檔案內容大小
    char c_devmajor[8]; //存放在哪個硬碟
    char c_devminor[8]; //存放在哪個分區
    char c_rdevmajor[8]; //這原本是哪一種驅動程式
    char c_rdevminor[8]; //這是該類別下的第幾個裝置
    char c_namesize[8]; //檔名長度。包含結尾的 \0
    char c_check[8];    //Checksum，通常是 0

} cpio_header_t;

bool cpio_get_header_name(void *file_header)
{
    while (1)
    {
        cpio_header_t* header = (cpio_header_t *)file_header;

        if (strncmp(header->c_magic, "070701", 6) != 0)
        {
            return false;
        }

        // 讀取 檔名 size
        int file_name_size = hex2int(header->c_namesize, 8);
        int file_size = hex2int(header->c_filesize, 8);
        char* file_name_ptr = (char*)file_header;
        
        file_name_ptr = file_name_ptr + 110;

        if (file_name_size == 11 && strncmp(file_name_ptr, "TRAILER!!!", 11) == 0)
        {
            break;
        }
        
        for (int i = 0; i < file_name_size - 1; i++)
        {
            uart_send(*file_name_ptr);
            file_name_ptr += 1;
        }

        //加上'/0'的位置
        file_name_ptr += 1;
        uart_puts("\n");

        file_header = file_name_ptr;
        unsigned long current_ptr = (unsigned long)file_header;
        current_ptr = ALIGN4(current_ptr);
        current_ptr = ALIGN4(current_ptr + file_size);
        file_header = (void *)current_ptr;
    }

    return true;
}