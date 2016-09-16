/* -*- linux-c -*-
 * cpu/arm926ejs/ns921x/fims/fim.c
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

#include <asm/io.h>
#include <asm-arm/arch-ns9xxx/ns921x_fim.h>
#include <asm-arm/arch-ns9xxx/fim_firmware.h>
#include <asm-arm/arch-ns9xxx/io.h>
#include <asm-arm/arch-ns9xxx/ns921x_hub.h>
#include "fim_reg.h"

#define	FIM0_SHIFT			6

/* @XXX: Need a similar mechanism as under Linux for having customizable DMA-channels */
#define FIM_TOTAL_NUMBER                (FIM_MAX_PIC_INDEX + 1)
#define FIM_DMA_BUFFERS                 (21)
#define FIM_TOTAL_BUFFERS               (FIM_TOTAL_NUMBER * FIM_DMA_BUFFERS)
#define FIM_BUFFER_SIZE                 (1024)

//#define FIM_CORE_DEBUG

#define printk_info(fmt,args...)                   printf(fmt, ##args)
#define printk_err(fmt,args...)                   printf(fmt, ##args)

#if defined(FIM_CORE_DEBUG)
# define printk_debug(fmt,args...)                   printf(fmt, ##args)
#else
# define printk_debug(fmt, args...)
#endif

/* Data for the RX-DMA buffers and DMA-FIFOs too */
static volatile struct iohub_dma_desc_t fim_rxdma_buffers[FIM_TOTAL_BUFFERS];
static volatile struct iohub_dma_fifo_t fim_rxdma_fifos[FIM_TOTAL_NUMBER];

int fim_get_exp_reg(int pic_num, int nr)
{
	return readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_EXP_REG(nr));
}

/*
 * This function provides the access to the control registers of the PICs
 * reg : Number of the control register (from 0 to 15)
 * val : Value to write into the control register
 */
void fim_set_ctrl_reg(int pic_num, int reg, unsigned int val)
{
	writel(val, FIM_REG_ADDR(pic_num) + NS92XX_FIM_CTRL_REG(reg));
}

int fim_send_interrupt(int pic_num, unsigned int code)
{
	unsigned int stopcnt;
	u32 status;

	if ( !code || (code & ~0x7f))
		return 1;

	if (!pic_is_running(pic_num)) {
		return 1;
	}

	code = NS92XX_FIM_INT_MASK(code);
	status = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
	writel(status | code, FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);

	/* This loop is perhaps problematic, exit with a timeout */
	stopcnt = 0xFFFF;
	do {
		status = readl(FIM_REG_ADDR(pic_num)  + NS92XX_FIM_GEN_CTRL_REG);
		stopcnt--;
	} while (!(status & NS92XX_FIM_GEN_CTRL_INTACKRD) && stopcnt);

	if (!stopcnt) {
		return 1;
	}

	/* Reset the interrupt bits for the PIC acknowledge */
	status &= ~NS92XX_FIM_GEN_CTRL_INTTOPIC;
	writel(status, FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);

	stopcnt = 0xFFFF;
	do {
		status = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
		stopcnt--;
	} while ((status & NS92XX_FIM_GEN_CTRL_INTACKRD) && stopcnt);

	if (!stopcnt) {
		return 1;
	}

	return 0;
}

/* Provides the read access to the control registers of the PICs */
int fim_get_iohub_reg(int pic_num, int reg)
{
	return readl(FIM_IOHUB_ADDR(pic_num) + reg);
}

void fim_set_iohub_reg(int pic_num, int reg, unsigned int val)
{
	writel(val, FIM_IOHUB_ADDR(pic_num) + reg);
}

/* Provides the read access to the control registers of the PICs */
int fim_get_ctrl_reg(int pic_num, int reg)
{
	if (NS92XX_FIM_CTRL_REG_CHECK(reg))
		return -1;

	return readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_CTRL_REG(reg));
}

int pic_get_exp_reg(int pic_num, int nr, unsigned int *value)
{
	*value = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_EXP_REG(nr));

	return 0;
}

int pic_is_running(int pic_num)
{
	unsigned int regval;

	regval = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);

	if (regval & NS92XX_FIM_GEN_CTRL_PROGMEM)
		return 1;
	else
		return 0;
}

/* Called when the PIC interrupts the ARM-processor */
void isr_from_pic(int pic_num, int irqnr)
{
	unsigned int status;
	unsigned int rx_fifo;
	unsigned int timeout;

	status = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
	rx_fifo = readl(FIM_IOHUB_ADDR(pic_num) + HUB_RX_FIFO_STAT);

	/* @TEST */
	writel(status, FIM_REG_ADDR(pic_num) + NS92XX_FIM_CTRL7_REG);

	writel(status | NS92XX_FIM_GEN_CTRL_INTACKWR,
	       FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);

	timeout = 0xFFFF;
	do {
		timeout--;
		status = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
	} while (timeout && (status & NS92XX_FIM_GEN_CTRL_INTFROMPIC));

	/* @XXX: Should we stop the PIC for avoiding more timeout errors? */
	if (!timeout) {
		return;
	}

	writel(status & ~NS92XX_FIM_GEN_CTRL_INTACKWR, FIM_REG_ADDR(pic_num) +
	       NS92XX_FIM_GEN_CTRL_REG);
}

/* This is the main ISR for the PIC-interrupts */
void pic_irq(int pic_num)
{
	unsigned int ifs;

	ifs = readl(FIM_IOHUB_ADDR(pic_num) + HUB_INT);

	if (ifs & HUB_INT_MODIP)
		isr_from_pic(pic_num, 0x00);

	writel(ifs, FIM_IOHUB_ADDR(pic_num) + HUB_INT);
}

/* Set the HWA PIC clock (see PIC module specification, page 19) */
static int pic_config_output_clock_divisor(int pic_num, struct fim_program_t *program)
{
	int div;
	int clkd;
	unsigned int val;

	if (!program)
		return 1;

	div = program->clkdiv;
	switch (div) {
	case FIM_CLK_DIV_2:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_2;
		break;

	case FIM_CLK_DIV_4:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_4;
		break;

	case FIM_CLK_DIV_8:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_8;
		break;

	case FIM_CLK_DIV_16:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_16;
		break;

	case FIM_CLK_DIV_32:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_32;
		break;

	case FIM_CLK_DIV_64:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_64;
		break;

	case FIM_CLK_DIV_128:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_128;
		break;

	case FIM_CLK_DIV_256:
		clkd = FIM_HWA_GEN_CFG_CLKSEL_DIVIDE_BY_256;
		break;

	default:
		return 1;
	}

	val = readl(FIM_HWA_ADDR(pic_num) + NS92XX_FIM_HWA_GEN_CONF_REG);
	writel(val | clkd, FIM_HWA_ADDR(pic_num) + NS92XX_FIM_HWA_GEN_CONF_REG);

	return 0;
}

int pic_stop_and_reset(int pic_num)
{
	unsigned int regval = 0;

	if (pic_is_running(pic_num)) {

		regval = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
		writel(regval & NS92XX_FIM_GEN_CTRL_STOP_PIC,
		       FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
	}

	if (pic_is_running(pic_num)) {
		return 1;
	}

	/* Reset the HWA generial register too */
	writel(0x00, FIM_HWA_ADDR(pic_num) + NS92XX_FIM_HWA_GEN_CONF_REG);

	return 0;
}

int pic_start_at_zero(int pic_num)
{
	unsigned int regval;

	regval = readl(FIM_REG_ADDR(pic_num) + NS92XX_FIM_GEN_CTRL_REG);
	writel(regval | NS92XX_FIM_GEN_CTRL_START_PIC, FIM_REG_ADDR(pic_num) +
	       NS92XX_FIM_GEN_CTRL_REG);

	/* Check if the PIC is really running */
	if (!pic_is_running(pic_num)) {
		return 1;
	}

	return 0;
}

int pic_download_firmware(int pic_num, const unsigned char *buffer)
{
	int mode;
	unsigned int status;
	struct fim_program_t *program = (struct fim_program_t *)buffer;
	int offset;

	if (!program )
		return 1;

	if (!(FORMAT_TYPE_VALID(program->format))) {
		printf("Invalid format type\n");
		return 1;
	}

	/* Check if the PIC is running, before starting the firmware update */
	if (pic_is_running(pic_num)) {
		return 1;
	}

	/* Check if the firmware has the correct header */
	if (!(PROCESSOR_TYPE_VALID(program->processor))) {
		printf("Invalid processor type. Aborting firmware download.\n");
		return 1;
	}

	/* Enable the clock to IO processor and reset the module */
	status = sys_readl(SYS_CLOCK);
	sys_writel(status | (1 << (pic_num + FIM0_SHIFT)), SYS_CLOCK);
	status = sys_readl(SYS_RESET);
	sys_writel(status & ~(1 << (pic_num + FIM0_SHIFT)), SYS_RESET);
	sys_writel(status | (1 << (pic_num + FIM0_SHIFT)), SYS_RESET);

	/* Configure the output clock */
	if(pic_config_output_clock_divisor(pic_num, program)) {
		printf("Couldn't set the clock output divisor.\n");
		return 1;
	}

	switch (program->hw_mode) {
	case FIM_HW_ASSIST_MODE_NONE:
		mode = 0x00;
		break;
	case FIM_HW_ASSIST_MODE_GENERIC:
		mode = 0x01;
		break;
	case FIM_HW_ASSIST_MODE_CAN:
		mode = 0x02;
		break;
	default:
		printf("Invalid HWA mode %i\n", program->hw_mode);
		return 1;
	}

	status = readl(FIM_HWA_ADDR(pic_num) + NS92XX_FIM_HWA_GEN_CONF_REG);
	writel(mode | status, FIM_HWA_ADDR(pic_num) + NS92XX_FIM_HWA_GEN_CONF_REG);

	/* Update the HW assist config registers */
	for (offset = 0; offset < FIM_NUM_HWA_CONF_REGS; offset++) {
		status = program->hwa_cfg[offset];
		writel(status, FIM_HWA_ADDR(pic_num) + NS92XX_FIM_HWA_SIGNAL(offset));
	}

	/* Check for the maximal supported number of instructions */
	if (program->length > FIM_NS9215_MAX_INSTRUCTIONS)
		return 1;

	/* Start programming the PIC (the program size is in 16bit-words) */
	for (offset = 0; offset < program->length; offset++)
		writel(program->data[offset] & NS92XX_FIM_INSTRUCTION_MASK,
		       FIM_INSTR_ADDR(pic_num) + 4*offset);

	return 0;
}

/* This is for a hard reset of the FIMs */
void fim_force_hard_reset(int pic)
{
	unsigned int status;
	ulong regval;
	int cnt;

	status = sys_readl(SYS_RESET);
	for (cnt = 0; cnt < FIM_MAX_PIC_INDEX; cnt++) {

		if (cnt != pic)
			continue;
		
		sys_writel(status & ~(1 << (cnt + FIM0_SHIFT)), SYS_RESET);
		sys_writel(status | (1 << (cnt + FIM0_SHIFT)), SYS_RESET);
		regval = readl(FIM_IOHUB_ADDR(cnt) + HUB_INT);
		writel(regval, FIM_IOHUB_ADDR(cnt) + HUB_INT);
        }
}

/* Dont use any lock, otherwise it would be required to free the PIC by failures */
int fim_core_init(int pic, const unsigned char *fwbuf)
{
	fim_force_hard_reset(pic);

        /* Stop the PIC before any other action */
	if(pic_stop_and_reset(pic))
		goto exit_free_pic;

	if(pic_download_firmware(pic, fwbuf))
		goto exit_free_pic;

	/* Start the PIC at zero */
	if(pic_start_at_zero(pic))
		goto exit_free_pic;

	return 0;

 exit_free_pic:
	return 1;
}



/*
 * Functions for using the DMA-channels of the FIMs. In some cases it is absolutely
 * required to use DMA for exchanging data with the FIMs (like in the case of the MMC)
 * for this reason we need the support.
 */


int fim_init_rxdma(int pic)
{
	/* DMA-buffer descriptors */
	static uchar rxbuf[FIM_TOTAL_BUFFERS][FIM_BUFFER_SIZE];
	volatile struct iohub_dma_desc_t *picbuf, *lstbuf;
	volatile struct iohub_dma_fifo_t *picfifo;
	int cnt;

	/* Init the last buffer pointer too */
	picbuf = &fim_rxdma_buffers[pic * FIM_DMA_BUFFERS];

	picfifo = &fim_rxdma_fifos[pic];
	picfifo->first = (struct iohub_dma_desc_t *)picbuf;
	picfifo->dma_last = (struct iohub_dma_desc_t *)picbuf;
	picfifo->last = (struct iohub_dma_desc_t *)(picbuf + FIM_DMA_BUFFERS - 1);

	printk_debug("DMA buffers for the FIM %i [%p to %p]\n", pic,
		     picfifo->first, picfifo->last);

	for (cnt = 0; cnt < FIM_DMA_BUFFERS; cnt++, picbuf++) {
		picbuf->src = (unsigned int)rxbuf[cnt];
		picbuf->length = FIM_BUFFER_SIZE;
		picbuf->control = IOHUB_DMA_DESC_CTRL_INT;
		printk_debug("  %2i. %p : 0x%08x | %u Bytes\n", cnt+1, picbuf,
			     picbuf->src, picbuf->length);

		lstbuf = picbuf;
	}

	picbuf = &fim_rxdma_buffers[pic * FIM_DMA_BUFFERS];
	lstbuf->control = IOHUB_DMA_DESC_CTRL_LAST | IOHUB_DMA_DESC_CTRL_INT |
		IOHUB_DMA_DESC_CTRL_WRAP;

	writel(0x0, FIM_IOHUB_ADDR(pic) + HUB_DMA_RX_CTRL);

	writel((unsigned long)picbuf, FIM_IOHUB_ADDR(pic) + HUB_DMA_RX_DESCR);

	writel(HUB_RX_INT_NCIE | HUB_RX_INT_NRIE | HUB_RX_INT_ECIE,
		FIM_IOHUB_ADDR(pic) + HUB_RX_INT);

	writel(HUB_DMA_RX_CTRL_CE, FIM_IOHUB_ADDR(pic) + HUB_DMA_RX_CTRL);
	return 0;
}

/* Send a buffer by using the DMA-channel */
int fim_send_buffer(int pic, struct fim_buffer_t *buffer)
{
	static volatile struct iohub_dma_desc_t fims_txdma_bufdesc[2];
	volatile struct iohub_dma_desc_t *picbuf;

	/* @TODO: Sanity check for the passed PIC number! */
	picbuf = &fims_txdma_bufdesc[pic];

	/* Configure the buffer first */
	picbuf->src = (unsigned int)buffer->data;
	picbuf->length = buffer->length;
	picbuf->control = IOHUB_DMA_DESC_CTRL_LAST | IOHUB_DMA_DESC_CTRL_FULL |
		IOHUB_DMA_DESC_CTRL_INT | IOHUB_DMA_DESC_CTRL_WRAP;

	/* Configure the TX DMA-channel */
	writel(0x0, FIM_IOHUB_ADDR(pic) + HUB_DMA_TX_CTRL);

	flush_cache_all();
	writel((unsigned long)picbuf, FIM_IOHUB_ADDR(pic) + HUB_DMA_TX_DESCR);

	writel(HUB_TX_INT_NCIE | HUB_TX_INT_NRIE | HUB_TX_INT_ECIE,
		FIM_IOHUB_ADDR(pic) + HUB_TX_INT);

	writel(HUB_DMA_TX_CTRL_CE, FIM_IOHUB_ADDR(pic) + HUB_DMA_TX_CTRL);

	return 0;
}

/* Wait until the DMA-buffer was closed by the FIM */
int fim_wait_txdma(int pic)
{
	unsigned long ifs;
	unsigned long timeout = 0xfffff;

	do {
		timeout--;
		ifs = readl(FIM_IOHUB_ADDR(pic) + HUB_INT);
	} while (timeout && !(ifs & HUB_INT_TX_NCIP));

	if (timeout)
		writel(ifs | HUB_INT_TX_NCIP, FIM_IOHUB_ADDR(pic) + HUB_INT);

	return (timeout) ? (0) : (-1);
}

/* The timeout is in microseconds */
struct fim_buffer_t *fim_wait_rxdma(int pic, uint timeout)
{
	struct fim_buffer_t *retval;
	struct iohub_dma_desc_t *picbuf;
	unsigned long ifs;
	volatile static struct fim_buffer_t rxdma[FIM_TOTAL_NUMBER];
	volatile struct iohub_dma_fifo_t *picfifo;
	int cnt;

	/* First check if there is an already fulled DMA-buffer */
	picfifo = &fim_rxdma_fifos[pic];

	do {
		picbuf = picfifo->dma_last;
		for (cnt = 0; cnt < FIM_DMA_BUFFERS; cnt++, picbuf++) {

			/* Check that the pointer is correct */
			if (picbuf > picfifo->last)
				picbuf = picfifo->first;

			invalidate_cache_all();
			if (picbuf->control & IOHUB_DMA_DESC_CTRL_FULL) {

				rxdma[pic].data = (uchar *)picbuf->src;
				rxdma[pic].length = picbuf->length;
				retval = (struct fim_buffer_t *)&rxdma[pic];
				picbuf->control &= ~(IOHUB_DMA_DESC_CTRL_FULL |
						     IOHUB_DMA_DESC_CTRL_LAST);
				picbuf->length = FIM_BUFFER_SIZE;

				/*
				 * Update the last pointer. If we overflow the DMA-buffer,
				 * then the above sanity check will correct the pointer
				 */
				picfifo->dma_last = picbuf + 1;
				if (picfifo->dma_last > picfifo->last)
					picfifo->dma_last = picfifo->first;

				ifs = readl(FIM_IOHUB_ADDR(pic) + HUB_DMA_RX_CTRL);
				printk_debug("  ==> RX-DMA %p (%i) | Last is now %p\n",
					     picbuf, ifs & 0x3ff, picfifo->dma_last);
				
				return retval;
			}
		}

		udelay(1);
		timeout--;
	} while (timeout);

	return NULL;
}

/*
 * There seems to be a problem when we try to boot the Linux-kernel with
 * a running FIM. The failure is reproducible by resetting the target console
 * stopped and then started with a command).
 * At this moment let us reset the FIMs for being able to boot the system, but
 * in the future we will need a better workaround.
 * Luis Galdos
 */
void shutdown_fims(void)
{
        unsigned int status;
	ulong regval;
        int cnt;

        status = sys_readl(SYS_RESET);
        for (cnt = 0; cnt < FIM_MAX_PIC_INDEX; cnt++) {
                sys_writel(status & ~(1 << (cnt + FIM0_SHIFT)), SYS_RESET);
                sys_writel(status | (1 << (cnt + FIM0_SHIFT)), SYS_RESET);
		regval = readl(FIM_IOHUB_ADDR(cnt) + HUB_INT);
		writel(regval, FIM_IOHUB_ADDR(cnt) + HUB_INT);
        }
}
