#ifndef BASE_H
#define BASE_H

// Shared primitive definitions for cross-module use.
// Include migration from common.h to this file has started.

// NULL
#ifndef NULL
#define NULL ((void *)0)
#endif

// bool / true / false (for C, not C++)
#ifndef __cplusplus
typedef _Bool bool;

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#endif

#ifndef ALIGN4
#define ALIGN4(x) (((x) + 3) & ~3)
#endif

#ifndef ALIGN8
#define ALIGN8(x) (((x) + 7) & ~7)
#endif

#ifndef MAX_ARGS
#define MAX_ARGS 16
#endif

#ifndef MAX_STRING_SIZE
#define MAX_STRING_SIZE 1024
#endif

#endif