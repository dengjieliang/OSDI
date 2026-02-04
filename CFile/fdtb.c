#include "../header/fdtb.h"
#include "../header/utils.h"

bool PathEqualsBase(char** nodeStack, int depth, string_t* segments, int compareDepth)
{
    if (depth != compareDepth + 1)
    {
        return false;
    }

    for (int i = 0; i < compareDepth; i++)
    {
        int unit_size = strcspn(nodeStack[i + 1], '@', strlen(nodeStack[i + 1]));

        if (unit_size != segments[i].size)
        {
            return false;
        }

        if (strncmp(nodeStack[i + 1], segments[i].string_ptr, segments[i].size) != 0)
        {
            return false;
        }
    }

    return true;
}

unsigned long Decode_Initrd_Addr(unsigned long valuePtr, unsigned int valueLength)
{
    if (valueLength == 4)
    {
        return (unsigned long)CombineByte((void*)valuePtr, 1);
    }

    if (valueLength == 8)
    {
        return (unsigned long)CombineByte((void*)valuePtr, 2);
    }

    return 0;
}

void InitialDtbCtx(CtxT* dtb_ctx)
{
    dtb_ctx->initrd_start = 0;
    dtb_ctx->initrd_end = 0;
    dtb_ctx->have_initrd_start = false;
    dtb_ctx->have_initrd_end = false;

    for (int i = 0; i < MAX_SEGMENT; i++)
    {
        dtb_ctx->stdout_target_segments[i].string_ptr = 0;
        dtb_ctx->stdout_target_segments[i].size = 0;
    }
    dtb_ctx->stdout_target_depth = 0;
    dtb_ctx->have_stdout_target = false;

    dtb_ctx->uart_mmio_base = 0;
    dtb_ctx->uart_mmio_size = 0;
    dtb_ctx->have_uart_reg = false;
    
    for (int i = 0; i < MAX_MEM_REGIONS; i++)
    {
        dtb_ctx->mem_regions[i].base = 0;
        dtb_ctx->mem_regions[i].size = 0;
    }

    for (int i = 0; i < MAX_DEPTH; i++)
    {
        dtb_ctx->child_addr_cells[i] = 0;
        dtb_ctx->child_size_cells[i] = 0;
        dtb_ctx->node_state[i].matched_driver_id = 0;

        for (int j = 0; j < MAX_REG_SIZE; j++)
        {
            dtb_ctx->node_state[i].reg_entries[j].base = 0;
            dtb_ctx->node_state[i].reg_entries[j].size = 0;
        }

        dtb_ctx->node_state[i].valid_reg_count = 0;
    }
}

void SaveChildCellAddr(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb)
{
    if (strcmp(nodeValueName, "#address-cells") != 0)
    {
        return;
    }

    unsigned int childCellAddr = BigEndianToLittleEndian((void*)valuePtr);
    ((CtxT*)user_dtb)->child_addr_cells[depth - 1] = childCellAddr;
}

void SaveChildCellSize(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb)
{
    if (strcmp(nodeValueName, "#size-cells") != 0)
    {
        return;
    }

    unsigned int childCellSize = BigEndianToLittleEndian((void*)valuePtr);
    ((CtxT*)user_dtb)->child_size_cells[depth - 1] = childCellSize;
}

void Initrd_Handler(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb)
{
    if (event != PROP_NODE)
    {
        return;
    }

    CtxT* ctx_dtb = (CtxT*)user_dtb;
    ctx_dtb->stdout_target_segments[0].string_ptr = "chosen";
    ctx_dtb->stdout_target_segments[0].size = 6;
    if (PathEqualsBase(nodeStack, depth, ctx_dtb->stdout_target_segments, 1) == false)
    {
        return;
    }

    if (strcmp(nodeValueName, "linux,initrd-start") == 0)
    {
        ctx_dtb->initrd_start = Decode_Initrd_Addr(valuePtr, valueLength);
        ctx_dtb->have_initrd_start = true;
    }
    else if (strcmp(nodeValueName, "linux,initrd-end") == 0)
    {
        ctx_dtb->initrd_end = Decode_Initrd_Addr(valuePtr, valueLength);
        ctx_dtb->have_initrd_end = true;
    }
}