/*
 * cpu/arm926ejs/ns921x/fims/fim_serial.c
 *
 * Copyright (C) 2008 by Digi International Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version2  as published by
 * the Free Software Foundation.
 *
 * !Revision:   $Revision$
 * !Author:     Luis Galdos
 * !References: [1] NS9215 Hardware Reference Manual, Preliminary January 2007
 */

#include <common.h>
#include <configs/userconfig.h>

#ifdef CONFIG_NS921X_FIM_UART

#include <serial.h>             /* serial_device */

#include <asm/arch/ns921x_sys.h>
#include <asm/arch/ns921x_hub.h>
#include <asm/arch/ns921x_gpio.h>
#include <asm/arch/io.h>  /* gpio_readl */
#include <asm/arch/ns921x_fim.h>

#include "fim_serial.h"

/* Depending on the processor we have different offset */
#if defined(CONFIG_CC9P9215) || defined(CONFIG_CCW9P9215)
# define FIM_GPIO_OFFSET	68
#elif defined(CONFIG_CME9210)
# define FIM_GPIO_OFFSET	0
#else
# error "Invalid platform. Couldn't define FIM_GPIO_OFFSET"
#endif

DECLARE_GLOBAL_DATA_PTR;

/* It must be a constant */
const static int pic_num = CONFIG_UBOOT_FIM_UART_PIC_NUM;
static unsigned int secByte = 0;

extern const unsigned char fim_serial_firmware[];

void fim_serial_setbrg( void)
{
	unsigned int div, prescale = 1;
	unsigned long clock;
	int cnt;
	unsigned int bit_time;

	clock = ahb_clock_freq();
	clock = (clock * 4) / gd->baudrate;
	div = (clock / 256) + 1;

	/* Must round up to next power of 2 (see NET+OS driver) */
	for (cnt = 1; cnt <= 8; cnt++) {
		if (div < (unsigned int)(1 << cnt)) {
			div = 1 << cnt;
			prescale = cnt - 1;
			break;
		}
	}

	/* The Net+OS driver has another calculation of the bit time */
	bit_time = (clock / div) - 1;

	/* Set the bit time */
	fim_set_ctrl_reg(pic_num, 0, bit_time);
	fim_send_interrupt(pic_num, FIM_SERIAL_INT_BIT_TIME);

	/* Set the prescale value */
	fim_set_ctrl_reg(pic_num, 0, prescale);
	fim_send_interrupt(pic_num, FIM_SERIAL_INT_PRESCALE);
}

static int fim_sw_flowctrl(void)
{
	unsigned short start_match, stop_match;

	start_match = 1;
	stop_match = 1;

	fim_set_ctrl_reg(pic_num, 0, 0xff);
	fim_set_ctrl_reg(pic_num, 1, 0xff);
	fim_set_ctrl_reg(pic_num, 2, 0xff);
	fim_set_ctrl_reg(pic_num, 3, 0xff);
	fim_set_ctrl_reg(pic_num, 4, 0x00);
	fim_set_ctrl_reg(pic_num, 5, 0x00);

	/* Now send the interrupt for the SW flow control */
	if(fim_send_interrupt(pic_num, FIM_SERIAL_INT_MATCH_CHAR))
		return 1;

	return 0;
}

/*
 * Check if there is new input data to get from the RX-FIFO
 */
int fim_serial_tstc(void)
{
	unsigned int regval;

	regval = fim_get_iohub_reg(pic_num, HUB_INT);

	return (regval & HUB_INT_RX_FIFO_EMPTY) ? 0 : 1;
}

int fim_serial_isr(int pic_num)
{
	unsigned int regval;
	int bytes;

	/* Was a second character read before from FIFO */
	if(secByte) {
		regval = secByte;
		secByte = 0;

		return regval;
	}

	/* If the RX-FIFO is empty then returns */
	do {
	  regval = fim_get_iohub_reg(pic_num, HUB_INT);
	} while (regval & HUB_INT_RX_FIFO_EMPTY);

	regval = fim_get_iohub_reg(pic_num, HUB_RX_FIFO_STAT);
	bytes = HUB_RX_FIFO_BYTE(regval);

	regval = fim_get_iohub_reg(pic_num, HUB_RX_FIFO);
	/* The FIM firmware always sends two bytes per data byte:
	 * - The first byte is the data itself
	 * - The second byte contains information about the serial
	 * communication (data bits, parity, stop bits).
	 * The direct IO HUB FIFO is 32 bits long, this means it
	 * can buffer 4 bytes. Considering the 1-byte overhead
	 * described above for each data byte, this means:
	 * if the 'bytes' field of FIFO stat register contains a
	 * value of 2, we have 1 data byte. If it contains a
	 * value of 4, we have 2 data bytes. */

	/* Get the second data byte and skip the FIM overhead byte */
	if (bytes == 4)
		secByte = (regval & 0xff0000) >> 16;

	/* Return the first data byte */
	return regval & 0xFF;
}

int fim_serial_getc(void)
{
	return fim_serial_isr(pic_num);
}

/*
 * Send a char over an interrupt
 */
void fim_serial_putc( const char ch)
{
	unsigned int status;
	unsigned short data = 1;

	if( '\n' == ch ) {
		fim_serial_putc('\r');
	}

	/* Check if the PIC is tasked with another send-char request */
	do {
		status = fim_get_exp_reg(pic_num, 0);
	} while (status & FIM_SERIAL_INT_INSERT_CHAR);

	data = (data << FIM_SERIAL_DATA_BITS) | (ch & ((1 << FIM_SERIAL_DATA_BITS) - 1));

	/* And send the char using the interrupt function */
	fim_set_ctrl_reg(pic_num, 0, data & 0xFF);
	fim_set_ctrl_reg(pic_num, 1, (data >> 8) & 0xFF);
	fim_send_interrupt(pic_num, FIM_SERIAL_INT_INSERT_CHAR);

}

int fim_serial_init(void)
{
	unsigned int regval;
	printf("Loading serial firmware\n");
	if(fim_core_init(pic_num, fim_serial_firmware))
		return 1;

	/* Init the register of the PIC */
	fim_set_ctrl_reg(pic_num, FIM_SERIAL_CTRL_REG, 0x00);

	/* Set the GPIOs offset */
	fim_set_ctrl_reg(pic_num, FIM_SERIAL_TXIO_REG,
			 1 << (FIM_UART_TX - FIM_GPIO_OFFSET));
	fim_set_ctrl_reg(pic_num, FIM_SERIAL_RXIO_REG,
			 1 << (FIM_UART_RX - FIM_GPIO_OFFSET));
	fim_send_interrupt(pic_num, FIM_SERIAL_INT_BIT_POS);

	fim_set_ctrl_reg(pic_num, 0, 0xFF);
	fim_set_ctrl_reg(pic_num, 1, 0xFF);
	fim_set_ctrl_reg(pic_num, 2, 0xFF);
	fim_set_ctrl_reg(pic_num, 3, 0xFF);
	fim_set_ctrl_reg(pic_num, 4, 0x00);
	fim_set_ctrl_reg(pic_num, 5, 0x00);
	fim_send_interrupt(pic_num, FIM_SERIAL_INT_MATCH_CHAR);

	/* Configure the GPIOs */
	gpio_cfg_set(FIM_UART_RX, GPIO_CFG_FUNC_FIM_UART);
	gpio_cfg_set(FIM_UART_TX, GPIO_CFG_FUNC_FIM_UART);

	fim_set_ctrl_reg(pic_num, 0, FIM_SERIAL_TOTAL_BITS);
	if(fim_send_interrupt(pic_num, FIM_SERIAL_INT_BITS_CHAR))
		return 1;

	/* Configure the software flow control */
	if(fim_sw_flowctrl())
		return 1;

	/* Now set the control status register to the correct value */
	regval = fim_get_ctrl_reg(pic_num, FIM_SERIAL_CTRL_REG);
	if(regval < 0)
		return 1;
	regval &= ~FIM_SERIAL_STAT_HW_FLOW;
	fim_set_ctrl_reg(pic_num, FIM_SERIAL_CTRL_REG, regval);


	/* After each reconfiguration we need to re-init the FIM-firmware */
	regval = fim_get_ctrl_reg(pic_num, FIM_SERIAL_CTRL_REG);
	if(regval < 0)
		return 1;
	fim_set_ctrl_reg(pic_num, FIM_SERIAL_CTRL_REG,
			 regval | FIM_SERIAL_STAT_TX_ENABLE | FIM_SERIAL_STAT_COMPLETE);

	/* baudrate configure before get lost */
	fim_serial_setbrg();

	/*
	 * IMPORTANT: Configure the FIM in direct mode, otherwise the other FIM
	 * probably doesn't work when it tries to use the DMA-engine!
	 */
	fim_set_iohub_reg(pic_num, HUB_DMA_RX_CTRL, HUB_DMA_RX_CTRL_DIRECT);
	fim_set_iohub_reg(pic_num, HUB_DMA_TX_CTRL, HUB_DMA_TX_CTRL_DIRECT);
	/* fim_set_iohub_reg(pic_num, HUB_RX_INT, 0x20000000); */
	/* regval = fim_get_iohub_reg(pic_num, HUB_DMA_RX_CTRL); */
	/* fim_set_iohub_reg(pic_num, HUB_DMA_RX_CTRL, regval | 0x10000000); */

	return 0;
}

static void fim_serial_tx_flush( void )
{
	/* Dummy */
}

/**
 * fim_serial_puts - outputs a zero terminated string
 */
static void fim_serial_puts( const char* szMsg )
{
	while( *szMsg )
		fim_serial_putc( *szMsg++ );
}

struct serial_device fim_serial_device = {

	.name     = "fim_serial",
	.init     = fim_serial_init ,
	.setbrg   = fim_serial_setbrg,
	.getc     = fim_serial_getc,
	.tstc     = fim_serial_tstc,
	.putc     = fim_serial_putc,
	.puts     = fim_serial_puts,
	.tx_flush = fim_serial_tx_flush
};

#endif  /* CONFIG_NS921X_FIM_UART */
