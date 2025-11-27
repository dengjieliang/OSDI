#include "../header/string.h"

bool strcmp(const char* s1, const char* s2)
{
    bool equal = true;

    while (*s1 != '\0' && *s2 != '\0')
    {
        if (*s1 != *s2)
        {
            equal = false;
            break;
        }
        s1 += 1;
        s2 += 1;
    }

    return equal && (*s1 == '\0' && *s2 == '\0');
}

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