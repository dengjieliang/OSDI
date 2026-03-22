#include "../Memory/allocator.h"
#include "../Board/common.h"

extern char __bss_end;

static void* heap_ptr = NULL;

void* SimpleAllocator(unsigned long size)
{
    if (heap_ptr == NULL)
    {
        heap_ptr = &__bss_end;
    }

    void* result_ptr = heap_ptr;

    size = ALIGN8(size);

    if ((unsigned long)result_ptr + size > MMIO_BASE)
    {
        return NULL;
    }

    heap_ptr = (void*)((unsigned long)result_ptr + size);

    return result_ptr;
}