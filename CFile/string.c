#include "../header/string.h"

// 比較兩個字串是否相等
int strcmp(const char* s1, const char* s2)
{
    while (*s1 != '\0' && *s2 != '\0')
    {
        if (*s1 != *s2)
        {
            break;
        }

        s1 += 1;
        s2 += 1;
    }

    //防止overflow轉成unsigned char
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, unsigned long read_byte)
{
    char s1_tmp;
    char s2_tmp;

    for (unsigned long i = 0; i < read_byte; i++)
    {
        s1_tmp = *(s1 + i);
        s2_tmp = *(s2 + i);

        if (s1_tmp != s2_tmp)
        {
            break;
        }
    }

    //防止overflow轉成unsigned char
    return (unsigned char)s1_tmp - (unsigned char)s2_tmp;
}

// 計算字串長度
int strlen(const char *s)
{
    int length = 0;
    while (*s != '\0')
    {
        length++;
        s++;
    }
    return length;
}