/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lwip/init.h"
#include "pico/stdlib.h"

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void) {
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void) pval;
}

/* lwip needs a millisecond time source, and the TinyUSB board support code has one available */
uint32_t sys_now(void) {
    return to_ms_since_boot(get_absolute_time());
}

u32_t sys_jiffies(void) {
    return time_us_32();
}