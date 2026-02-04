#ifndef CPIO_H
#define CPIO_H

#include "../header/common.h"

//獲取所有檔案名稱
int CpioGetFilesHeaderName(void *file_header);

//獲取所有檔案內容
bool CpioGetFileContext(void *file_header, char* file_name);

#endif