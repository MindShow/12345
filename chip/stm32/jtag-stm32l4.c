/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Settings to enable JTAG debugging */

#include "jtag.h"
#include "registers.h"

void jtag_pre_init(void)
{
	/*
	 * Stop all timers we might use (TIM1-8) and watchdogs when
	 * the JTAG stops the CPU.
	 */
	STM32_DBGMCU_APB1FZ |=
		STM32_RCC_PB1_TIM2 | STM32_RCC_PB1_TIM3 | STM32_RCC_PB1_TIM4 |
		STM32_RCC_PB1_TIM5 | STM32_RCC_PB1_TIM6 | STM32_RCC_PB1_TIM7 |
		STM32_RCC_PB1_WWDG | STM32_RCC_PB1_IWDG;
	STM32_DBGMCU_APB2FZ |= STM32_RCC_PB2_TIM1 | STM32_RCC_PB2_TIM8;
}
