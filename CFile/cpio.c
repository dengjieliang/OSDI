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

static void* GetNextHeader(void *file_header);
static bool CompareFileName(cpio_header_t* file, char* cmp_name, int cmp_size);

int CpioGetFilesHeaderName(void *file_header)
{
    cpio_header_t* header = (cpio_header_t *)file_header;

    int file_count = 0;

    if (strncmp(header->c_magic, "070701", 6) != 0)
    {
        return file_count;
    }

    while (1)
    {
        int file_name_size = hex2int(header->c_namesize, 8);

        if (file_name_size == 11 && 
            CompareFileName(header, "TRAILER!!!", 
                strlen("TRAILER!!!") ) == true)
        {
            break;
        }

        char* filename_ptr = (char *)header + 110;
        
        for (int i = 0; i < file_name_size - 1; i++)
        {
            uart_send(*((char*)filename_ptr));
            filename_ptr += 1;
        }

        file_count += 1;
        file_header = GetNextHeader(file_header);

        if (file_header == NULL)
        {
            break;
        }

        header = (cpio_header_t *)file_header;
    }

    return file_count;
}

bool CpioGetFileContext(void *file_header, char* file_name)
{
    cpio_header_t* header = (cpio_header_t *)file_header;

    if (strncmp(header->c_magic, "070701", 6) != 0)
    {
        return false;
    }

    while(1)
    {
        //確保file的檔名和使用者輸入的完全相符合
        if (CompareFileName(header, file_name, strlen(file_name) + 1 ))
        {
            int file_context_size = hex2int(header->c_filesize, 8);
            int file_header_size = hex2int(header->c_namesize, 8);

            header = (cpio_header_t*)((char *)header + 110);
            header = (cpio_header_t*)((char *)header + file_header_size);

            unsigned long current_ptr = (unsigned long)header;
            current_ptr = ALIGN4(current_ptr);
            char* file_content_ptr = (char *)current_ptr;

            for (int i = 0; i < file_context_size; i++)
            {
                uart_send(*file_content_ptr);
                file_content_ptr++;
            }
            uart_puts("\n");
            return true;
        }
        else
        {
            file_header = GetNextHeader((void*)file_header);
            
            if (file_header == NULL)
            {
                break;
            }
            header = (cpio_header_t *)file_header;
        }
    }

    return false;
}

static void* GetNextHeader(void *file_header)
{
    cpio_header_t* header = (cpio_header_t *)file_header;

    // 讀取 檔名 size
    int file_name_size = hex2int(header->c_namesize, 8);
    int file_size = hex2int(header->c_filesize, 8);
    
    header = (cpio_header_t *)((char*)header + 110);

    //加上檔案名稱長度
    header = (cpio_header_t *)((char*)header + file_name_size);

    unsigned long current_ptr = (unsigned long)header;
    current_ptr = ALIGN4(current_ptr);
    current_ptr = ALIGN4(current_ptr + file_size);
    header = (void *)current_ptr;

    if (strncmp(header->c_magic, "070701", 6) != 0)
    {
        return NULL;
    }
    else
    {
        return (void *)header;
    }
}

static bool CompareFileName(cpio_header_t* file, char* cmp_name, int cmp_size)
{    
    file = (cpio_header_t *)((char*)file + 110);
    return (strncmp((char*)file, cmp_name, cmp_size) == 0);
}