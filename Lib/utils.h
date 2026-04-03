#ifndef UTILS_H
#define UTILS_H

#include "../Lib/base.h"

void *memcpy(void * dest, const void *src, unsigned long n);
int hex2int(char *hex, int n);
unsigned long hex2UnsignedLong(char *hex, int n);
bool is_all_digits(const char* string);
unsigned int aoti(char* string);
unsigned long strtoul(char* string);
double atof(const char* string);
unsigned int reverseint(unsigned int number);
unsigned int BigEndianToLittleEndian(void* byte);
unsigned long long CombineByte(void* byte, unsigned int n);

// 將非負整數轉為十進位字串，結果寫入 buf（含 null terminator）。
// buf_size 須計入 '\0'；對值域 0..MAX_TIMERS-1，大小 4 即已足夠。
void uint_to_str(unsigned int val, char* buf, int buf_size);

#endif