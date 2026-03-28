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

#endif