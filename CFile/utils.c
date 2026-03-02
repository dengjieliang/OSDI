#include "../header/common.h"
#include "../header/utils.h"
#include "../header/string.h"

static unsigned int makemask(int size, int shiftnumber);

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

unsigned long hex2UnsignedLong(char *hex, int n)
{
    unsigned long result = 0;

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

unsigned int reverseint(unsigned int number)
{
    unsigned int number_size = sizeof(number) * 8;
    for (unsigned int bit = 1; bit < number_size; bit *= 2)
    {
        unsigned int mask = makemask(number_size, bit);
        number = ((number >> bit) & mask) | ((number & mask) << bit);
    }

    return number;
}

static unsigned int makemask(int size, int shiftnumber)
{
    if (shiftnumber == 0 || size <= shiftnumber)
    {
        return 0;
    }

    unsigned int mask = 0;
    unsigned int low_ones = (1u << shiftnumber) - 1u;

    for(int i = 0; i < size; i += (shiftnumber * 2))
    {
        mask = mask | (low_ones << i);
    }

    return mask;
}

unsigned int BigEndianToLittleEndian(void* byte)
{
    unsigned int result = 0;

    for (unsigned int i = 0; i < 4; i++)
    {
        result = result << 8;
        unsigned char* ptr = (unsigned char*)byte;
        result += *(ptr + i);
    }

    return result;
}

unsigned long long CombineByte(void* byte, unsigned int n)
{
    unsigned long long result = 0;

    for (unsigned int i = 0; i < n; i += 1)
    {
        byte = (void*)((char*)byte + (i * 4));
        result = (result << 32) | BigEndianToLittleEndian(byte);
    }

    return result;
}