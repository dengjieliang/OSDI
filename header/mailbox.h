#ifndef MAILBOX_H
#define MAILBOX_H

#include "../header/common.h"

// Mailbox 外設相對於 MMIO_BASE 的偏移量 (BCM2837/BCM2711)
#define MBOX_BASE_OFFSET 0xB880
#define MBOX_BASE (MMIO_BASE + MBOX_BASE_OFFSET)

// Mailbox 暫存器內部偏移量
#define MBOX_READ (MBOX_BASE + 0X00)
#define MBOX_PEEK (MBOX_BASE + 0X10)
#define MBOX_SENDER (MBOX_BASE + 0X14)
#define MBOX_STATUS (MBOX_BASE + 0X18)
#define MBOX_CONFIG (MBOX_BASE + 0X1C)
#define MBOX_WRITE (MBOX_BASE + 0X20)

void mailbox_call();

#endif

