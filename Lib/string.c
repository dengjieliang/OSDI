#include "../Lib/string.h"
#include "../Lib/base.h"

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

    if (read_byte == 0)
    {
        return 0;
    }

    for (unsigned long i = 0; i < read_byte; i++)
    {
        s1_tmp = *(s1 + i);
        s2_tmp = *(s2 + i);

        if (s1_tmp != s2_tmp)
        {
            break;
        }

        if (s1_tmp == '\0')
        {
            return 0;
        }
    }

    //防止overflow轉成unsigned char
    return (unsigned char)s1_tmp - (unsigned char)s2_tmp;
}

unsigned int strcspn(const char *s, const char reject, int max_len)
{
    int i = 0;

    while (i < max_len)
    {
        if (s[i] == '\0')
        {
            return i;
        }

        if (s[i] == reject)
        {
            return i;
        }

        i += 1;
    }

    return max_len;
}

// 計算字串長度
unsigned long strlen(const char *s)
{
    int max_size = MAX_STRING_SIZE;
    unsigned long length = 0;
    const char* char_ptr = s;
    const unsigned long* long_ptr;

    if (char_ptr == 0)
    {
        return length;
    }

    while(((unsigned long)char_ptr & 7) != 0)
    {
        if (*char_ptr == '\0') 
        {
            return length;
        }

        char_ptr += 1;
        length += 1;
    }

    long_ptr = (const unsigned long *)char_ptr;

    // 準備 64-bit 的魔法數字
    unsigned long himagic = 0x8080808080808080UL;
    unsigned long lomagic = 0x0101010101010101UL;
    
    for (; length < max_size; length += 8)
    {
        int has_end = (*long_ptr - lomagic) & (~(*long_ptr)) & himagic;

        if (has_end)
        {
            break;
        }

        long_ptr += 1;
    }

    char_ptr = (const char *)long_ptr;

    if (length < max_size)
    {
        while (length < max_size) 
        {
            if (*char_ptr == '\0')
            {
                return length;
            } 
            
            char_ptr++;
            length++;
        }
    }

    return length;
}