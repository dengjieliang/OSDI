#ifndef PM_H
#define PM_H

#include "../header/common.h"

#define PM_PASSWORD (0x5A000000)    //Magic Password
#define PM_RSTC (MMIO_BASE + 0x0010001C)
#define PM_WDOG (MMIO_BASE + 0x00100024)

void reset(int tick);
void cancel_reset(void);

#endif