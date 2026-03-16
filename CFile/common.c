#include "../header/common.h"
#include "../header/fdtb.h"

#define MMIO_BASE_DEFAULT 0x3F000000UL
#define GPIO_BASE_OFFSET  0x200000UL
#define AUX_BASE_OFFSET   0x215000UL
#define IRQ_BASE_OFFSET   0x0000B200UL

CommonMmioInfoT common_mmio = {
    .mmio_base = MMIO_BASE_DEFAULT,
    .gpio_base = MMIO_BASE_DEFAULT + GPIO_BASE_OFFSET,
};

void common_init_from_dtb(struct ctx* dtb_ctx)
{
    if (dtb_ctx == NULL)
    {
        return;
    }

    if (dtb_ctx->have_gpio_reg)
    {
        common_mmio.gpio_base = dtb_ctx->gpio_mmio_base;
        common_mmio.mmio_base = dtb_ctx->gpio_mmio_base - GPIO_BASE_OFFSET;
        return;
    }

    if (dtb_ctx->have_aux_reg)
    {
        common_mmio.mmio_base = dtb_ctx->aux_mmio_base - AUX_BASE_OFFSET;
        common_mmio.gpio_base = common_mmio.mmio_base + GPIO_BASE_OFFSET;
        return;
    }

    if (dtb_ctx->interrupt_info.have_arm_ctrl_intc_base)
    {
        common_mmio.mmio_base = dtb_ctx->interrupt_info.arm_ctrl_intc_base - IRQ_BASE_OFFSET;
        common_mmio.gpio_base = common_mmio.mmio_base + GPIO_BASE_OFFSET;
    }
}