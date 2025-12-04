#include "../header/power_manager.h"

#define PM_RSTC_FULL_RESET (0X20)

#define TIMETICK_MASK (~(0XFFF << 20))

void reset(int tick)
{
    mmio_write(PM_RSTC, PM_PASSWORD | PM_RSTC_FULL_RESET);
    tick &= TIMETICK_MASK;
    mmio_write(PM_WDOG, PM_PASSWORD | tick);
}

void cancel_reset()
{
    mmio_write(PM_RSTC, PM_PASSWORD | 0);
    mmio_write(PM_WDOG, PM_PASSWORD | 0);
}