#ifndef UTILS_H
#define UTILS_H

void *memcpy(void * dest, const void *src, unsigned long n);
int hex2int(char *hex, int n);
unsigned int reverseint(unsigned int number);
unsigned int BigEndianToLittleEndian(void* byte);
unsigned long long CombineByte(void* byte, unsigned int n);
#endif