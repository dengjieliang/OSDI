#include "../header/utils.h"

void *memcpy(void * dest, const void *src, unsigned long n)
{
    char *src_char = (char *)src;
    char *dest_char = (char *)dest;

    for (unsigned long i = 0; i < n; i += 1)
    {
        dest_char[i] = src_char[i];
    }

    return (void *)dest_char;
}

int hex2int(char *hex, int n)
{
    int result = 0;

    for (int i = 0; i < n; i++)
    {
        result *= 16;

        if (hex[i] >= 'a')
        {
            result += (10 + (hex[i] - 'a'));
        }
        else if (hex[i] >= 'A')
        {
            result += (10 + (hex[i] - 'A'));
        }
        else
        {
            result += (hex[i] - '0');
        }
    }

    return result;
}