/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("Hello World! %s", CONFIG_BOARD_TARGET);

	return 0;
}


#include <zephyr/init.h>

static int init_config(void) {
    int ret;

    return 0;
}

SYS_INIT(init_config, EARLY, 0);
