#ifndef DTB_H
#define DTB_H

#include "../Board/common.h"


#define MAX_DEPTH 32
typedef enum NodeEvent
{
    ENTER_NODE,
    PROP_NODE,
    LEAVE_NODE,
} NodeEnum;

typedef void (*FdtHandleNodeFunction) (NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                                    char* nodeValueName, 
                                    unsigned long valuePtr, unsigned int valueLength, void* user_dtb);

bool ReadDTBFile(void* header, FdtHandleNodeFunction callBack, void* user_data);

#endif