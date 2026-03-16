#ifndef FDTB_H
#define FDTB_H

#include "../header/common.h"
#include "../header/string.h"
#include "../header/dtb.h"

#define MAX_MEM_REGIONS 256
#define MAX_SEGMENT 256

#define MAX_REG_SIZE 256

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

typedef struct interrupt_info
{
    unsigned long arm_local_intc_base;
    bool have_arm_local_intc_base;

    unsigned long arm_ctrl_intc_base;
    bool have_arm_ctrl_intc_base;

    // debug: depth at which compatible was matched (0 = never)
    int debug_l1_intc_depth;
    int debug_armctrl_depth;
} InterruptInfoT;

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
    unsigned long aux_mmio_base;
    bool have_aux_reg;
    unsigned long gpio_mmio_base;
    bool have_gpio_reg;

    InterruptInfoT interrupt_info;


    //記錄 RAM 的實體範圍
    MemRegionT mem_regions[MAX_MEM_REGIONS];


    unsigned int child_addr_cells[MAX_DEPTH];
    unsigned int child_size_cells[MAX_DEPTH];
    NodeStateT node_state[MAX_DEPTH];
} CtxT;

void InitialDtbCtx(CtxT* dtb_ctx);

void DtbCollectHandler(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth,
                            char* nodeValueName,
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb);
void SaveChildCellAddr(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth,
                            char* nodeValueName,
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb);
void SaveChildCellSize(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth,
                            char* nodeValueName,
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb);

bool PathEqualsBase(char** nodeStack, int depth, string_t* segments, int compareDepth);
unsigned long Decode_Initrd_Addr(unsigned long valuePtr, unsigned int valueLength);

#endif