#ifndef STRING_H
#define STRING_H

typedef struct string
{
    char* string_ptr;
    unsigned int size;
} string_t;

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned long read_byte);
unsigned int strcspn(const char *s, const char reject, int max_len);
unsigned long strlen(const char *s);
char* strncpy(const char *s1, const char *s2, unsigned long read_byte);

#endif