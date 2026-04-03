#include "../Lib/base.h"
#include "../Lib/utils.h"
#include "../Lib/string.h"

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

bool is_all_digits(const char* string)
{
    if (string == NULL || *string == '\0')
    {
        return false;
    }

    while (*string != '\0')
    {
        if (*string < '0' || *string > '9')
        {
            return false;
        }

        string += 1;
    }

    return true;
}

unsigned int aoti(char* string)
{
    unsigned int result = 0;
    int digit_count = 0;

    while (*string != '\0' && digit_count < 10) // 10 digits is the max for 32-bit unsigned int
    {
        if (*string >= '0' && *string <= '9')    
        {
            result = result * 10 + (*string - '0');
            digit_count += 1;

        }
        else
        {
            break;
        }
        string += 1;
    }

    return result;
}

unsigned long strtoul(char* string)
{
    unsigned long result = 0;
    int digit_count = 0;

    while (*string != '\0' && digit_count < 20) // 20 digits is the max for 64-bit unsigned long
    {
        if (*string >= '0' && *string <= '9')    
        {
            result = result * 10 + (*string - '0');
            digit_count += 1;

        }
        else
        {
            break;
        }
        string += 1;
    }

    return result;
}

double atof(const char* string)
{
    double result = 0.0;
    double fraction_base = 0.1;
    bool in_fraction = false;

    while (*string != '\0')
    {
        if (*string == '.')
        {
            in_fraction = true;
        }
        else
        {
            int digit = *string - '0';

            if (in_fraction)
            {
                result += ((double)digit * fraction_base);
                fraction_base *= 0.1;
            }
            else
            {
                result = result * 10.0 + (double)digit;
            }
        }

        string += 1;
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

void uint_to_str(unsigned int val, char* buf, int buf_size)
{
    if (buf_size <= 0)
    {
        return;
    }

    if (val == 0)
    {
        buf[0] = '0';
        if (buf_size > 1)
        {
            buf[1] = '\0';
        }
        return;
    }

    int len = 0;
    unsigned int tmp = val;

    while (tmp > 0 && len < buf_size - 1)
    {
        buf[len] = (char)('0' + (tmp % 10));
        tmp /= 10;
        len++;
    }

    buf[len] = '\0';

    // 低位先寫，需反轉為正確的十進位順序
    for (int l = 0, r = len - 1; l < r; l++, r--)
    {
        char c = buf[l];
        buf[l] = buf[r];
        buf[r] = c;
    }
}