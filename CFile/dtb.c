#include "../header/dtb.h"
#include "../header/utils.h"
#include "../header/string.h"
#include "common.h"

#define MAX_MEM_REGIONS 256
#define MAX_SEGMENT 256
#define MAX_DEPTH 32
#define MAX_REG_SIZE 256
#define HANDLE_NODE_FUNCTION_MAX_SIZE 256

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

typedef enum NodeEvent
{
    ENTER_NODE,
    PROP_NODE,
    LEAVE_NODE,
} NodeEnum;

static void SaveChildCellAddr(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb);

static void SaveChildCellSize(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb);

static void Initrd_Handler(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb);

static void InitialDtbCtx();
static void dispacherCallbackFunction(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                                    char* nodeValueName, 
                                    unsigned long valuePtr, unsigned int valueLength);
static bool PathEqualsBase(char* nodeStack[MAX_DEPTH], int depth, string_t segments[MAX_SEGMENT], int compareDepth);
static unsigned long Decode_Initrd_Addr(unsigned long valuePtr, unsigned int valueLength);

typedef struct dtb_struct
{
    unsigned int magic; //固定魔數，用來驗證是否為 DTB（值為 0xd00dfeed，以 big-endian 儲存）
    unsigned int totalsize; //整個 DTB blob 的總長度（bytes），包含 header + 各 block + 空隙
    unsigned int off_dt_struct; //Structure Block 的起始 offset（相對 header 起點）
    unsigned int off_dt_strings;//Strings Block 的起始 offset
    unsigned int off_mem_rsvmap;//Memory Reservation Block 的起始 offset
    unsigned int version;//DTB 格式版本（規格文件對應的版本常見為 17）
    unsigned int last_comp_version; //最低相容版本（例如 version 17 常見為 16）
    unsigned int boot_cpuid_phys; //boot CPU 的 physical ID（對應 CPU node 的 reg）
    unsigned int size_dt_strings; //Strings Block 的長度（bytes）
    unsigned int size_dt_struct; //Structure Block 的長度（bytes）

} DtbT;

typedef struct mem_region
{
    unsigned long base;
    unsigned long size;
} MemRegionT;

typedef struct node_state 
{
    int matched_driver_id;
    MemRegionT reg_entries[MAX_REG_SIZE];
    unsigned int valid_reg_count;
} NodeStateT;

typedef struct ctx
{
    //initramfs的開頭和結尾,kernel 要用它當作 initramfs（cpio archive）的位置起點
    unsigned long initrd_start;
    unsigned long initrd_end;
    bool have_initrd_start;
    bool have_initrd_end;

    //獲得stdout 的目標節點路徑
    string_t stdout_target_segments[MAX_SEGMENT];
    unsigned int stdout_target_depth;
    bool have_stdout_target;

    //stdout 的目標節點內容(console UART 的 MMIO base/size)
    unsigned long uart_mmio_base;
    unsigned int uart_mmio_size;
    bool have_uart_reg;

    //記錄 RAM 的實體範圍
    MemRegionT mem_regions[MAX_MEM_REGIONS];


    unsigned int child_addr_cells[MAX_DEPTH];
    unsigned int child_size_cells[MAX_DEPTH];
    NodeStateT node_state[MAX_DEPTH];
} CtxT;

typedef void (*FdtHandleNodeFunction) (NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                                    char* nodeValueName, 
                                    unsigned long valuePtr, unsigned int valueLength, void* user_dtb);

static CtxT dtb_ctx;
static FdtHandleNodeFunction all_node_handler[HANDLE_NODE_FUNCTION_MAX_SIZE] = 
{
    SaveChildCellAddr, SaveChildCellSize, Initrd_Handler
};

bool ReadDTBFile(void* header)
{
    DtbT* dtb_header = (DtbT*)header;
    unsigned int magic = BigEndianToLittleEndian(&dtb_header->magic);

    if (magic != 0xD00DFEED)
    {
        return false;
    }

    unsigned int totalsize = BigEndianToLittleEndian(&dtb_header->totalsize);
    unsigned int off_dt_struct = BigEndianToLittleEndian(&dtb_header->off_dt_struct);
    unsigned int off_dt_string = BigEndianToLittleEndian(&dtb_header->off_dt_strings);
    unsigned int size_dt_struct = BigEndianToLittleEndian(&dtb_header->size_dt_struct);
    unsigned int size_dt_string = BigEndianToLittleEndian(&dtb_header->size_dt_strings);

    unsigned long struct_base = (unsigned long)header + off_dt_struct;
    unsigned long struct_end = struct_base + size_dt_struct;

    unsigned long string_base = (unsigned long)header + off_dt_string;
    unsigned long string_end = string_base + size_dt_string;

    if (string_end > (unsigned long)header + totalsize || struct_end > (unsigned long)header + totalsize)
    {
        return false;
    }

    int depth = 0;
    char* nodeStack[MAX_DEPTH];
    InitialDtbCtx();
    unsigned long  cursor = struct_base;

    while (cursor < struct_end)
    {
        unsigned int token = BigEndianToLittleEndian((void*)cursor);
        cursor += 4;

        if (token == FDT_BEGIN_NODE)
        {
            if (depth >= MAX_DEPTH || depth < 0)
            {
                return false;
            }

            nodeStack[depth] = (char*)(cursor);

            while (*(char*)cursor != '\0')
            {
                cursor += 1;

                if (cursor > struct_end)
                {
                    return false;
                }
            }

            //加上\0.
            cursor += 1;

            cursor = ALIGN4(cursor);

            depth += 1;

            continue;
        }
        else if (token == FDT_END_NODE)
        {
            depth -= 1;

            if (depth < 0)
            {
                return false;
            }
            else
            {
                continue;
            }
        }
        else if (token == FDT_PROP)
        {
            unsigned int valueLength = BigEndianToLittleEndian((void*)cursor);
            cursor += 4;
            unsigned int nameoff = BigEndianToLittleEndian((void*)cursor);
            cursor += 4;

            if (nameoff >= size_dt_string)
            {
                return false;
            }

            char* nodeValueName = (char*)(string_base + nameoff);

            bool findStringEnd = false;

            for (int i = 0; i < size_dt_string - nameoff; i++)
            {
                if (*(nodeValueName + i) == '\0')
                {
                    findStringEnd = true;
                    break;
                }
            }

            if (findStringEnd == false)
            {
                return false;
            }

            unsigned long valuePtr = cursor;

            if (valuePtr + valueLength > struct_end)
            {
                return false;
            }

            cursor += valueLength;
            cursor = ALIGN4(cursor);
            dispacherCallbackFunction(PROP_NODE, nodeStack, depth, nodeValueName, valuePtr, valueLength);
            continue;
        }
        else if (token == FDT_NOP)
        {
            continue;
        }
        else if (token == FDT_END)
        {
            break;
        }
        else
        {
            return false;
        }
    }

    return true;
}

static void InitialDtbCtx()
{
    dtb_ctx.initrd_start = 0;
    dtb_ctx.initrd_end = 0;
    dtb_ctx.have_initrd_start = false;
    dtb_ctx.have_initrd_end = false;

    for (int i = 0; i < MAX_SEGMENT; i++)
    {
        dtb_ctx.stdout_target_segments[i].string_ptr = 0;
        dtb_ctx.stdout_target_segments[i].size = 0;
    }
    dtb_ctx.stdout_target_depth = 0;
    dtb_ctx.have_stdout_target = false;

    dtb_ctx.uart_mmio_base = 0;
    dtb_ctx.uart_mmio_size = 0;
    dtb_ctx.have_uart_reg = false;
    
    for (int i = 0; i < MAX_MEM_REGIONS; i++)
    {
        dtb_ctx.mem_regions[i].base = 0;
        dtb_ctx.mem_regions[i].size = 0;
    }

    for (int i = 0; i < MAX_DEPTH; i++)
    {
        dtb_ctx.child_addr_cells[i] = 0;
        dtb_ctx.child_size_cells[i] = 0;
        dtb_ctx.node_state[i].matched_driver_id = 0;

        for (int j = 0; j < MAX_REG_SIZE; j++)
        {
            dtb_ctx.node_state[i].reg_entries[j].base = 0;
            dtb_ctx.node_state[i].reg_entries[j].size = 0;
        }

        dtb_ctx.node_state[i].valid_reg_count = 0;
    }
}

static void dispacherCallbackFunction(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                                    char* nodeValueName, 
                                    unsigned long valuePtr, unsigned int valueLength)
{
    for (int i = 0; i < HANDLE_NODE_FUNCTION_MAX_SIZE; i++)
    {
        if (all_node_handler[i] != NULL)
        {
            all_node_handler[i](event, nodeStack, depth, nodeValueName, valuePtr, valueLength, &dtb_ctx);
        }
    }
}

static void SaveChildCellAddr(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb)
{
    if (strcmp(nodeValueName, "#address-cells") != 0)
    {
        return;
    }

    unsigned int childCellAddr = BigEndianToLittleEndian((void*)valuePtr);
    ((CtxT*)user_dtb)->child_addr_cells[depth] = childCellAddr;
}

static void SaveChildCellSize(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb)
{
    if (strcmp(nodeValueName, "#size-cells") != 0)
    {
        return;
    }

    unsigned int childCellSize = BigEndianToLittleEndian((void*)valuePtr);
    dtb_ctx.child_size_cells[depth] = childCellSize;
}

static void Initrd_Handler(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
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
    if (PathEqualsBase(nodeStack, depth, ctx_dtb->stdout_target_segments, 0) == false)
    {
        return;
    }

    if (strcmp(nodeValueName, "linux,initrd-start") == 0)
    {
        ctx_dtb->initrd_start = Decode_Initrd_Addr(valuePtr, valueLength);
        ctx_dtb->have_initrd_start = true;
    }
    else if (strcmp(nodeValueName, "linux,initrd-start") == 0)
    {
        ctx_dtb->initrd_end = Decode_Initrd_Addr(valuePtr, valueLength);
        ctx_dtb->have_initrd_end = true;
    }
}

static bool PathEqualsBase(char* nodeStack[MAX_DEPTH], int depth, string_t segments[MAX_SEGMENT], int compareDepth)
{
    if (depth < compareDepth)
    {
        return false;
    }

    for (int i = 0; i <= compareDepth; i++)
    {
        if (strncmp(nodeStack[i], segments[i].string_ptr, 
            strcspn(nodeStack[i], '@', segments[i].size)) == false)
        {
            return false;
        }
    }
}

static unsigned long Decode_Initrd_Addr(unsigned long valuePtr, unsigned int valueLength)
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