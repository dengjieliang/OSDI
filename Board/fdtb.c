#include "../Board/fdtb.h"
#include "../Board/common.h"
#include "../Lib/utils.h"
#include "../Lib/string.h"

#define AUX_MMIO_BASE_DEFAULT  (MMIO_BASE + 0x215000)
#define UART_MMIO_BASE_DEFAULT (MMIO_BASE + 0x215040)
#define GPIO_MMIO_BASE_DEFAULT (MMIO_BASE + 0x200000)
#define IRQ_MMIO_BASE_DEFAULT  (MMIO_BASE + 0xB200)

enum
{
    INTERRUPT_DRIVER_NONE = 0,
    INTERRUPT_DRIVER_ARM_LOCAL = 1,
    INTERRUPT_DRIVER_ARM_CTRL = 2,
    INTERRUPT_DRIVER_AUX_UART = 3,
    INTERRUPT_DRIVER_AUX = 4,
    INTERRUPT_DRIVER_GPIO = 5,
};

static void HandleChosenProps(char* nodeStack[MAX_DEPTH], int depth,
                            char* nodeValueName,
                            unsigned long valuePtr, unsigned int valueLength, CtxT* ctx_dtb)
{
    string_t chosenSegment[1];
    chosenSegment[0].string_ptr = "chosen";
    chosenSegment[0].size = 6;

    if (PathEqualsBase(nodeStack, depth, chosenSegment, 1) == false)
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

static void HandleSocPeripheralNode(char* nodeStack[MAX_DEPTH], int depth,
                            char* nodeValueName,
                            unsigned long valuePtr, unsigned int valueLength, CtxT* ctx_dtb)
{
    string_t interruptCtrlPath[2];
    interruptCtrlPath[0].string_ptr = "soc";
    interruptCtrlPath[0].size = 3;
    interruptCtrlPath[1].string_ptr = "interrupt-controller";
    interruptCtrlPath[1].size = 20;

    string_t serialPath[2];
    serialPath[0].string_ptr = "soc";
    serialPath[0].size = 3;
    serialPath[1].string_ptr = "serial";
    serialPath[1].size = 6;

    string_t auxPath[2];
    auxPath[0].string_ptr = "soc";
    auxPath[0].size = 3;
    auxPath[1].string_ptr = "aux";
    auxPath[1].size = 3;

    string_t gpioPath[2];
    gpioPath[0].string_ptr = "soc";
    gpioPath[0].size = 3;
    gpioPath[1].string_ptr = "gpio";
    gpioPath[1].size = 4;

    bool is_interrupt_ctrl = PathEqualsBase(nodeStack, depth, interruptCtrlPath, 2);
    bool is_serial = PathEqualsBase(nodeStack, depth, serialPath, 2);
    bool is_aux = PathEqualsBase(nodeStack, depth, auxPath, 2);
    bool is_gpio = PathEqualsBase(nodeStack, depth, gpioPath, 2);

    if (is_interrupt_ctrl == false && is_serial == false && is_aux == false && is_gpio == false)
    {
        return;
    }

    if (strcmp(nodeValueName, "compatible") == 0)
    {
        // Use strncmp with fixed lengths to avoid strcspn/strlen issues
        if (strncmp((const char*)valuePtr, "brcm,bcm2836-l1-intc", 20) == 0)
        {
            ctx_dtb->node_state[depth - 1].matched_driver_id = INTERRUPT_DRIVER_ARM_LOCAL;
            ctx_dtb->interrupt_info.debug_l1_intc_depth = depth;
        }
        else if (strncmp((const char*)valuePtr, "brcm,bcm2836-armctrl-ic", 23) == 0)
        {
            ctx_dtb->node_state[depth - 1].matched_driver_id = INTERRUPT_DRIVER_ARM_CTRL;
            ctx_dtb->interrupt_info.debug_armctrl_depth = depth;
        }
        else if (strncmp((const char*)valuePtr, "brcm,bcm2835-aux-uart", 21) == 0)
        {
            ctx_dtb->node_state[depth - 1].matched_driver_id = INTERRUPT_DRIVER_AUX_UART;
        }
        else if (strncmp((const char*)valuePtr, "brcm,bcm2835-aux", 16) == 0)
        {
            ctx_dtb->node_state[depth - 1].matched_driver_id = INTERRUPT_DRIVER_AUX;
        }
        else if (strncmp((const char*)valuePtr, "brcm,bcm2835-gpio", 17) == 0)
        {
            ctx_dtb->node_state[depth - 1].matched_driver_id = INTERRUPT_DRIVER_GPIO;
        }
    }
    else if (strcmp(nodeValueName, "reg") == 0)
    {
        unsigned int addr_cells = ctx_dtb->child_addr_cells[depth - 2];
        unsigned int size_cells = ctx_dtb->child_size_cells[depth - 2];
        if (addr_cells == 0) addr_cells = 1; // default
        if (size_cells == 0) size_cells = 1; // default

        unsigned long reg_base = Decode_Initrd_Addr(valuePtr, addr_cells * 4);
        unsigned long reg_size = Decode_Initrd_Addr(valuePtr + (addr_cells * 4), size_cells * 4);
        unsigned long cpu_mmio_base = reg_base;

        if ((reg_base & 0xFF000000UL) == 0x7E000000UL)
        {
            cpu_mmio_base = (reg_base & 0x00FFFFFFUL) | 0x3F000000UL;
        }
        else if ((reg_base & 0xFF000000UL) == 0x00000000UL)
        {
            cpu_mmio_base = (reg_base & 0x00FFFFFFUL) | 0x3F000000UL;
        }

        if (ctx_dtb->node_state[depth - 1].matched_driver_id == INTERRUPT_DRIVER_ARM_LOCAL)
        {
            ctx_dtb->interrupt_info.arm_local_intc_base = cpu_mmio_base;
            ctx_dtb->interrupt_info.have_arm_local_intc_base = true;
        }
        else if (ctx_dtb->node_state[depth - 1].matched_driver_id == INTERRUPT_DRIVER_ARM_CTRL)
        {
            ctx_dtb->interrupt_info.arm_ctrl_intc_base = cpu_mmio_base;
            ctx_dtb->interrupt_info.have_arm_ctrl_intc_base = true;
        }
        else if (ctx_dtb->node_state[depth - 1].matched_driver_id == INTERRUPT_DRIVER_AUX_UART)
        {
            ctx_dtb->uart_mmio_base = cpu_mmio_base;
            ctx_dtb->uart_mmio_size = (unsigned int)reg_size;
            ctx_dtb->have_uart_reg = true;
        }
        else if (ctx_dtb->node_state[depth - 1].matched_driver_id == INTERRUPT_DRIVER_AUX)
        {
            ctx_dtb->aux_mmio_base = cpu_mmio_base;
            ctx_dtb->have_aux_reg = true;
        }
        else if (ctx_dtb->node_state[depth - 1].matched_driver_id == INTERRUPT_DRIVER_GPIO)
        {
            ctx_dtb->gpio_mmio_base = cpu_mmio_base;
            ctx_dtb->have_gpio_reg = true;
        }
    }
}

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

    dtb_ctx->uart_mmio_base = UART_MMIO_BASE_DEFAULT;
    dtb_ctx->uart_mmio_size = 0;
    dtb_ctx->have_uart_reg = true;
    dtb_ctx->aux_mmio_base = AUX_MMIO_BASE_DEFAULT;
    dtb_ctx->have_aux_reg = true;
    dtb_ctx->gpio_mmio_base = GPIO_MMIO_BASE_DEFAULT;
    dtb_ctx->have_gpio_reg = true;

    dtb_ctx->interrupt_info.arm_local_intc_base = 0;
    dtb_ctx->interrupt_info.have_arm_local_intc_base = false;

    dtb_ctx->interrupt_info.arm_ctrl_intc_base = IRQ_MMIO_BASE_DEFAULT;
    dtb_ctx->interrupt_info.have_arm_ctrl_intc_base = true;

    dtb_ctx->interrupt_info.debug_l1_intc_depth = 0;
    dtb_ctx->interrupt_info.debug_armctrl_depth = 0;
    
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

void DtbCollectHandler(NodeEnum event, char* nodeStack[MAX_DEPTH], int depth, 
                            char* nodeValueName, 
                            unsigned long valuePtr, unsigned int valueLength, void* user_dtb)
{
    if (event != PROP_NODE)
    {
        return;
    }

    CtxT* ctx_dtb = (CtxT*)user_dtb;

    SaveChildCellAddr(event, nodeStack, depth, nodeValueName, valuePtr, valueLength, user_dtb);
    SaveChildCellSize(event, nodeStack, depth, nodeValueName, valuePtr, valueLength, user_dtb);
    HandleChosenProps(nodeStack, depth, nodeValueName, valuePtr, valueLength, ctx_dtb);
    HandleSocPeripheralNode(nodeStack, depth, nodeValueName, valuePtr, valueLength, ctx_dtb);
}