/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED1_NODE DT_ALIAS(led1)

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	if (!gpio_is_ready_dt(&led1)) {
		return 0;
	}

	int ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	while(1) {
		gpio_pin_set_dt(&led1, 0);
		k_msleep(SLEEP_TIME_MS);

		gpio_pin_set_dt(&led1, 1);
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
