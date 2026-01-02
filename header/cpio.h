#ifndef CPIO_H
#define CPIO_H

#include "../header/common.h"

#define FILE_HEADER 0x8000000

/* 宣告解析函式 */
bool CpioGetFilesHeaderName(void *file_header);

#endif