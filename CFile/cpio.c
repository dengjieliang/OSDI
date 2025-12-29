#include "../header/cpio.h"
#include "../header/string.h"
#include "common.h"
#include "utils.h"

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

bool cpio_parse_newc_header(void *file_header)
{
    cpio_header_t* header = (struct cpio_header_t *)file_header;

    if (strncmp(header->c_magic, "070701", 6) != 0)
    {
        return false;
    }

    // 讀取 filesize
    int size = hex2int(header->c_namesize, 8);

    return true;
}