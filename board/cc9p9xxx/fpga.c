/***********************************************************************
 *
 * Copyright (C) 2008 by Digi International GmbH.
 * All rights reserved.
 *
 * $Id: fpga.c,v 1.1 2008-01-30 14:42:51 mludwig Exp $
 * @Author: Matthias Ludwig
 * @Descr: Defines helper functions for loading fpga firmware
 * @Usage: 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *  
 ***********************************************************************/
#include <common.h>
#include <command.h>

#ifdef CONFIG_IEEE1588

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
#include <nand.h>
#endif

#include <ns9750_bbus.h>
#include <ns9750_mem.h>

#include <fpga_checkbitstream.h>

#define PIN_RESET	23		/* reset fpga -> to programming-mode */
#define PIN_INIT	58		/* FPGA ready to program */
#define PIN_DONE	66		/* FPGA done with programing */

#define FPGA_BASE	0x60000000	/* use default base address for cs2 */
#define FPGA_MASK	0xF0000001	/* minimal size (4k) and enable bit */

#define PIO_INPUT(pin)	set_gpio_cfg_reg_val(pin, NS9750_GPIO_CFG_FUNC_GPIO|NS9750_GPIO_CFG_INPUT)
#define PIO_OUTPUT(pin)	set_gpio_cfg_reg_val(pin, NS9750_GPIO_CFG_FUNC_GPIO|NS9750_GPIO_CFG_OUTPUT)
#define PIO_GET(pin)	get_gpio_stat(pin)

ulong checksum_calc, checksum_read, fpgadatasize;

static unsigned char WaitForPIO(int pin)
{
	int i;

        /* 10ms max lock time, see Xilinux Spartan-3 datasheet */
	for (i=0; i<10000; i++)
		udelay(1);

	if (PIO_GET(pin) == 0)
		return 0;

	return 1;
}

int fpga_load(volatile u_char *buf, ulong bsize) 
{
	int i;

	/* setup GPIOs */
	PIO_INPUT(PIN_INIT);
	PIO_INPUT(PIN_DONE);

	/* already in programming mode? */
	if (!WaitForPIO(PIN_INIT)) {
		/* reset fpga to programming mode */
		*get_gpio_stat_reg_addr(PIN_RESET) = *get_gpio_stat_reg_addr(PIN_RESET)
			| (1 << (((PIN_RESET) % 32))) ^ (1 << (((PIN_RESET) % 32)));
		PIO_OUTPUT(PIN_RESET);
		for (i = 0; i<10000; i++)
			udelay(1);
		*get_gpio_stat_reg_addr(PIN_RESET) = *get_gpio_stat_reg_addr(PIN_RESET)
			| (1 << (((PIN_RESET) % 32)));

		/* wait for fpga to become ready */
		if (!WaitForPIO(PIN_INIT)) {
			printf("FPGA: PIN_INIT not set\n");
			return LOAD_FPGA_FAIL;
		}
	}

	/* CS2 is used for FPGA code download. */
	/* Map to FPGA_BASE address, 32 bit width, 1 read wait states, 2 write waitstates. */
	*get_mem_reg_addr(NS9750_MEM_STAT_CFG(2)) = NS9750_MEM_STAT_CFG_MW_32 | NS9750_MEM_STAT_CFG_PB;
	*get_mem_reg_addr(NS9750_MEM_STAT_WAIT_WEN(2))	= 0;
	*get_mem_reg_addr(NS9750_MEM_STAT_WAIT_OEN(2))	= 0;
	*get_mem_reg_addr(NS9750_MEM_STAT_RD(2))	= 0x1;
	*get_mem_reg_addr(NS9750_MEM_STAT_PAGE(2))	= 0;
	*get_mem_reg_addr(NS9750_MEM_STAT_WR(2))	= 0x2;
	*get_mem_reg_addr(NS9750_MEM_STAT_TURN(2))	= 0;
	*get_sys_reg_addr(NS9750_SYS_CS_STATIC_BASE(2)) = FPGA_BASE;
	*get_sys_reg_addr(NS9750_SYS_CS_STATIC_MASK(2)) = FPGA_MASK;

	/* send firmware */
	for(; bsize > 0; buf++, bsize--)
		*(volatile uchar *)FPGA_BASE = *buf;

	/* disable CS2 */
	*get_sys_reg_addr(NS9750_SYS_CS_STATIC_MASK(2)) = 0;

	/* wait for fpga to prcess data */
	if (!WaitForPIO(PIN_DONE)) {
		printf("FPGA: PIN_DONE not set\n");
		return LOAD_FPGA_FAIL;
	}

	return LOAD_FPGA_OK;
}

int fpga_checkbitstream(unsigned char* fpgadata, ulong size) {
	unsigned int length;
	unsigned int swapsize;
	char buffer[80];
	unsigned char *dataptr;
	unsigned int i;

	dataptr = (unsigned char *)fpgadata;

	/* skip the first bytes of the bitsteam, their meaning is unknown */
	length = (*dataptr << 8) + *(dataptr+1);
	dataptr+=2;
	dataptr+=length;

	/* get design name (identifier, length, string) */
	length = (*dataptr << 8) + *(dataptr+1);
	dataptr+=2;
	if (*dataptr++ != 0x61) {
		printf("\nFPGA:  Design name identifier not recognized in bitstream\n" );
		return LOAD_FPGA_FAIL;
	}

	length = (*dataptr << 8) + *(dataptr+1);
	dataptr+=2;
	for(i=0;i<length;i++)
		buffer[i]=*dataptr++;
        printf("FPGA: %s", buffer);

	/* get part number (identifier, length, string) */
	if (*dataptr++ != 0x62) {
		printf("\nFPGA: Part number identifier not recognized in bitstream\n");
		return LOAD_FPGA_FAIL;
	}

	length = (*dataptr << 8) + *(dataptr+1);
	dataptr+=2;
	for(i=0;i<length;i++)
		buffer[i]=*dataptr++;
	printf(", part = \"%s\"", buffer);

	/* get date (identifier, length, string) */
	if (*dataptr++ != 0x63) {
		printf("\nFPGA: Date identifier not recognized in bitstream\n");
		return LOAD_FPGA_FAIL;
	}

	length = (*dataptr << 8) + *(dataptr+1);
	dataptr+=2;
	for(i=0;i<length;i++)
		buffer[i]=*dataptr++;
        printf(", %s", buffer);

	/* get time (identifier, length, string) */
	if (*dataptr++ != 0x64) {
		printf("\nFPGA: Time identifier not recognized in bitstream\n");
		return LOAD_FPGA_FAIL;
	}

	length = (*dataptr << 8) + *(dataptr+1);
	dataptr+=2;
	for(i=0;i<length;i++)
		buffer[i]=*dataptr++;
        printf(", %s\n", buffer);

	/* get fpga data length (identifier, length) */
	if (*dataptr++ != 0x65) {
		printf("FPGA: Data length identifier not recognized in bitstream\n");
		return LOAD_FPGA_FAIL;
	}
	swapsize = ((unsigned int) *dataptr     <<24) +
	           ((unsigned int) *(dataptr+1) <<16) +
	           ((unsigned int) *(dataptr+2) <<8 ) +
	           ((unsigned int) *(dataptr+3)     ) ;
	dataptr+=4;

	/* check consistency of length obtained */
	int headersize = dataptr - (unsigned char *)fpgadata;

        /* fpgaloadsize has been checked by update command */
	if (size != (swapsize + headersize)) {
		printf("FPGA: Could not find right length of data in bitstream\n");
		return LOAD_FPGA_FAIL;
	}
	
	return LOAD_FPGA_OK;
}

#endif
