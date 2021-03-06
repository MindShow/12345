/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Tigertail board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "ina2xx.h"
#include "queue_policies.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "update_fw.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_i2c.h"
#include "usb-stream.h"
#include "util.h"

#include "gpio_list.h"


#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)


/******************************************************************************
 * Forward UARTs as a USB serial interface.
 */

#define USB_STREAM_RX_SIZE	16
#define USB_STREAM_TX_SIZE	16

/******************************************************************************
 * Forward USART1 as a simple USB serial interface.
 */
static struct usart_config const usart1;
struct usb_stream_config const usart1_usb;

static struct queue const usart1_to_usb = QUEUE_DIRECT(64, uint8_t,
	usart1.producer, usart1_usb.consumer);
static struct queue const usb_to_usart1 = QUEUE_DIRECT(64, uint8_t,
	usart1_usb.producer, usart1.consumer);

static struct usart_config const usart1 =
	USART_CONFIG(usart1_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		usart1_to_usb,
		usb_to_usart1);

USB_STREAM_CONFIG(usart1_usb,
	USB_IFACE_USART1_STREAM,
	USB_STR_USART1_STREAM_NAME,
	USB_EP_USART1_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart1,
	usart1_to_usb)


/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Tigertail"),
	[USB_STR_SERIALNO]     = 0,
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_I2C_NAME]     = USB_STRING_DESC("I2C"),
	[USB_STR_USART1_STREAM_NAME]  = USB_STRING_DESC("DUT UART"),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Tigertail Console"),
	[USB_STR_UPDATE_NAME]  = USB_STRING_DESC("Firmware update"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * ADC support for SBU flip detect.
 */
/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_SBU1] = {"SBU1", 3300, 4096, 0, STM32_AIN(6)},
	[ADC_SBU2] = {"SBU2", 3300, 4096, 0, STM32_AIN(7)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);



/******************************************************************************
 * Support I2C bridging over USB, this requires usb_i2c_board_enable and
 * usb_i2c_board_disable to be defined to enable and disable the SPI bridge.
 */

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
		GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

int usb_i2c_board_enable(void) {return EC_SUCCESS; }
void usb_i2c_board_disable(void) {}


/******************************************************************************
 * Support firmware upgrade over USB. We can update whichever section is not
 * the current section.
 */

/*
 * This array defines possible sections available for the firmware update.
 * The section which does not map the current executing code is picked as the
 * valid update area. The values are offsets into the flash space.
 */
const struct section_descriptor board_rw_sections[] = {
	{CONFIG_RO_MEM_OFF,
	 CONFIG_RO_MEM_OFF + CONFIG_RO_SIZE},
	{CONFIG_RW_MEM_OFF,
	 CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE},
};
const struct section_descriptor * const rw_sections = board_rw_sections;
const int num_rw_sections = ARRAY_SIZE(board_rw_sections);



/******************************************************************************
 * Console commands.
 */

/* State to indicate current GPIO config. */
static int uart_state = UART_OFF;
/* State to indicate current autodetect mode. */
static int uart_detect = UART_DETECT_AUTO;

/* Set GPIOs to configure UART mode. */
static void set_uart_gpios(int state)
{
	int uart = 0;
	int dir = 0;
	int enabled = (state == UART_ON) || (state == UART_FLIP);

	gpio_set_level(GPIO_ST_UART_LVL_DIS, 1);

	uart = GPIO_INPUT;

	if (state == UART_ON) {
		uart = GPIO_ALTERNATE;
		dir = 1;
	}
	if (state == UART_FLIP) {
		uart = GPIO_ALTERNATE;
		dir = 0;
	}

	/* Set level shifter direction. */
	gpio_set_level(GPIO_ST_UART_TX_DIR, dir);
	gpio_set_level(GPIO_ST_UART_TX_DIR_N, !dir);

	/* Enable STM pinmux */
	gpio_set_flags(GPIO_USART1_TX, uart);
	gpio_set_flags(GPIO_USART1_RX, uart);

	/* Flip uart orientation if necessary. */
	STM32_USART_CR1(STM32_USART1_BASE) &= ~(STM32_USART_CR1_UE);
	if (dir)
		STM32_USART_CR2(STM32_USART1_BASE) &= ~(STM32_USART_CR2_SWAP);
	else
		STM32_USART_CR2(STM32_USART1_BASE) |= (STM32_USART_CR2_SWAP);
	STM32_USART_CR1(STM32_USART1_BASE) |= STM32_USART_CR1_UE;

	/* Enable level shifter. */
	gpio_set_level(GPIO_ST_UART_LVL_DIS, !enabled);
}

/* Detect if a UART is plugged into SBU. Tigertail UART must be off
 * for this to return useful info.
 */
static int detect_uart_orientation(void)
{
	int sbu1 = adc_read_channel(ADC_SBU1);
	int sbu2 = adc_read_channel(ADC_SBU2);
	int state = UART_OFF;

	/*
	 * Here we check if one or the other SBU is 1.8v, as DUT
	 * TX should idle high.
	 */
	if ((sbu1 < 150) && (sbu2 > 1600) && (sbu2 < 1900))
		state = UART_ON;
	else if ((sbu2 < 150) && (sbu1 > 1600) && (sbu1 < 1900))
		state = UART_FLIP;
	else
		state = UART_OFF;

	return state;
}

/*
 * Detect if UART has been unplugged. Normal UARTs should
 * have both lines idling high at 1.8v.
 */
static int detect_uart_idle(void)
{
	int sbu1 = adc_read_channel(ADC_SBU1);
	int sbu2 = adc_read_channel(ADC_SBU2);
	int enabled = 0;

	if ((sbu1 > 1600) && (sbu1 < 1900) &&
	    (sbu2 > 1600) && (sbu2 < 1900))
		enabled = 1;

	return enabled;
}

/* Set the UART state and gpios, and autodetect if necessary. */
void set_uart_state(int state)
{
	if (state == UART_AUTO) {
		set_uart_gpios(UART_OFF);
		msleep(10);

		uart_detect = UART_DETECT_AUTO;
		state = detect_uart_orientation();
	} else {
		uart_detect = UART_DETECT_OFF;
	}

	uart_state = state;
	set_uart_gpios(state);
}

/*
 * Autodetect UART state:
 * We will check every 250ms, and change state if 1 second has passed
 * in the new state.
 */
void uart_sbu_tick(void)
{
	static int debounce;  /* = 0 */

	if (uart_detect != UART_DETECT_AUTO)
		return;

	if (uart_state == UART_OFF) {
		int state = detect_uart_orientation();

		if (state != UART_OFF) {
			debounce++;
			if (debounce > 4) {
				debounce = 0;
				CPRINTS("UART autoenable\n");
				set_uart_state(state);
			}
			return;
		}
	} else {
		int enabled = detect_uart_idle();

		if (!enabled) {
			debounce++;
			if (debounce > 4) {
				debounce = 0;
				CPRINTS("UART autodisable\n");
				set_uart_state(UART_OFF);
			}
			return;
		}
	}
	debounce = 0;
}
DECLARE_HOOK(HOOK_TICK, uart_sbu_tick, HOOK_PRIO_DEFAULT);

static int command_uart(int argc, char **argv)
{
	char *uart_state_str = "off";
	char *uart_detect_str = "manual";

	if (argc > 1) {
		if (!strcasecmp("off", argv[1]))
			set_uart_state(UART_OFF);
		else if (!strcasecmp("on", argv[1]))
			set_uart_state(UART_ON);
		else if (!strcasecmp("flip", argv[1]))
			set_uart_state(UART_FLIP);
		else if (!strcasecmp("auto", argv[1]))
			set_uart_state(UART_AUTO);
		else
			return EC_ERROR_PARAM1;
	}

	if (uart_state == UART_ON)
		uart_state_str = "on";
	if (uart_state == UART_FLIP)
		uart_state_str = "flip";
	if (uart_detect == UART_DETECT_AUTO)
		uart_detect_str = "auto";
	ccprintf("UART mux is: %s, setting: %s\n",
		uart_state_str, uart_detect_str);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(uart, command_uart,
	"[off|on|flip|auto]",
	"Get/set the flip and enable state of the SBU UART");

/* State we intend the mux GPIOs to be set. */
static int mux_state = MUX_OFF;

/* Set the state variable and GPIO configs to mux as requested. */
void set_mux_state(int state)
{
	int enabled = (state == MUX_A) || (state == MUX_B);
	/* dir: 0 -> A, dir: 1 -> B */
	int dir = (state == MUX_B);

	/* Disconnect first. */
	gpio_set_level(GPIO_USB_C_OE_N, 1);
	gpio_set_level(GPIO_SEL_RELAY_A, 0);
	gpio_set_level(GPIO_SEL_RELAY_B, 0);

	/* Reconnect in the requested direction. */
	gpio_set_level(GPIO_SEL_RELAY_A, !dir && enabled);
	gpio_set_level(GPIO_SEL_RELAY_B, dir && enabled);

	gpio_set_level(GPIO_USB_C_SEL_B, dir);
	gpio_set_level(GPIO_USB_C_OE_N, !enabled);

	if (!enabled)
		mux_state = MUX_OFF;
	else
		mux_state = state;
}

static int command_mux(int argc, char **argv)
{
	char *mux_state_str = "off";

	if (argc > 1) {
		if (!strcasecmp("off", argv[1]))
			set_mux_state(MUX_OFF);
		else if (!strcasecmp("a", argv[1]))
			set_mux_state(MUX_A);
		else if (!strcasecmp("b", argv[1]))
			set_mux_state(MUX_B);
		else
			return EC_ERROR_PARAM1;
	}

	if (mux_state == MUX_A)
		mux_state_str = "A";
	if (mux_state == MUX_B)
		mux_state_str = "B";
	ccprintf("TYPE-C mux is %s\n", mux_state_str);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(mux, command_mux,
	"[off|A|B]",
	"Get/set the mux and enable state of the TYPE-C mux");

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
	/* USB to serial queues */
	queue_init(&usart1_to_usb);
	queue_init(&usb_to_usart1);

	/* UART init */
	usart_init(&usart1);
	/* No default type-c mux. TODO: would we like this to be set? */
	set_mux_state(MUX_OFF);
	/* Note that we can't enable AUTO until after init. */
	set_uart_gpios(UART_OFF);

	/* Calibrate INA0 (VBUS) with 1mA/LSB scale */
	ina2xx_init(0, 0x8000, INA2XX_CALIB_1MA(15 /*mOhm*/));
	ina2xx_init(1, 0x8000, INA2XX_CALIB_1MA(15 /*mOhm*/));
	ina2xx_init(4, 0x8000, INA2XX_CALIB_1MA(15 /*mOhm*/));
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

