/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(TOUCHPAD_INT, PIN(B, 8), GPIO_INT_FALLING, elan_tp_interrupt)

/* Keyboard inputs */
#define GPIO_KB_INPUT  (GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_BOTH)
#define GPIO_KB_OUTPUT GPIO_ODR_HIGH

GPIO_INT(KB_IN00, PIN(A, 4),  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN01, PIN(B, 3),  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN02, PIN(B, 0),  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN03, PIN(A, 7),  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN04, PIN(B, 12), GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN05, PIN(B, 2),  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN06, PIN(B, 14), GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN07, PIN(B, 15), GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)

/* Do not forget to update KB_OUT_PORT_LIST to match this. */
GPIO(KB_OUT00,			PIN(B, 1),  GPIO_KB_OUTPUT)
GPIO(KB_OUT01,			PIN(A, 5),  GPIO_KB_OUTPUT)
GPIO(KB_OUT02,			PIN(A, 3),  GPIO_KB_OUTPUT)
GPIO(KB_OUT03,			PIN(A, 2),  GPIO_KB_OUTPUT)
GPIO(KB_OUT04,			PIN(A, 6),  GPIO_KB_OUTPUT)
GPIO(KB_OUT05,			PIN(A, 0),  GPIO_KB_OUTPUT)
GPIO(KB_OUT06,			PIN(A, 1),  GPIO_KB_OUTPUT)
GPIO(KB_OUT07,			PIN(B, 13), GPIO_KB_OUTPUT)
GPIO(KB_OUT08,			PIN(C, 14), GPIO_KB_OUTPUT)
GPIO(KB_OUT09,			PIN(C, 15), GPIO_KB_OUTPUT)
GPIO(KB_OUT10,			PIN(F, 1),  GPIO_KB_OUTPUT)
GPIO(KB_OUT11,			PIN(F, 0),  GPIO_KB_OUTPUT)
GPIO(KB_OUT12,			PIN(C, 13), GPIO_KB_OUTPUT)

/* I2C pins should be configured as inputs until I2C module is    */
/* initialized. This will avoid driving the lines unintentionally.*/
GPIO(MASTER_I2C_SCL,		PIN(B, 6), GPIO_INPUT)
GPIO(MASTER_I2C_SDA,		PIN(B, 7), GPIO_INPUT)
/* TODO(crosbug.com/p/59083): Disable trackpad when appropriate */
GPIO(EN_PP3300_TP_ODL,		PIN(A, 14), GPIO_OUT_LOW)

GPIO(KEYBOARD_BACKLIGHT,	PIN(B, 9), GPIO_OUT_LOW)

GPIO(WP_L,			PIN(A, 13), GPIO_INPUT)

/* Unimplemented signals since we are not an EC */
UNIMPLEMENTED(ENTERING_RW)

ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART, GPIO_PULL_UP) /* USART1: PA09/PA10 - Servo stm32 console UART */

ALTERNATE(PIN_MASK(B, 0x00c0), 1, MODULE_I2C, 0) /* I2C MASTER:PB6/7 GPIO_ODR_HIGH */

ALTERNATE(PIN_MASK(B, 0x0200), 2, MODULE_PWM, GPIO_DEFAULT) /* PWM: PB9 */
