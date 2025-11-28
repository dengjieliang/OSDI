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

    return *s1 - *s2;
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