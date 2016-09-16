/* -*- linux-c -*-
 * include/asm-arm/arch-ns9xxx/fim-ns921x.h
 *
 * Copyright (C) 2008 by Digi International Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 *  !Revision:   $Revision$
 *  !Author:     Silvano Najera, Luis Galdos
 *  !Descr:      
 *  !References:
 */


#ifndef _NS921X_FIM_API_H
#define _NS921X_FIM_API_H


#include <asm-arm/arch-ns9xxx/ns921x_dma.h>


#define FIM_MAX_FIRMWARE_NAME			32


/* For DMA handling... */
#define	FIM_DMA_NCIP				(0x1)
#define FIM_DMA_NRIP				(0x2)
#define FIM_DMA_ECIP				(0x4)
#define FIM_DMA_CAIP				(0x8)
#define FIM_DMA_CANCELLED			FIM_DMA_CAIP
#define FIM_DMA_FLUSHED				(0x10)
#define FIM_DMA_SUCCESS		         	(FIM_DMA_NCIP | FIM_DMA_NRIP)


#define FIM_MAX_PIC_INDEX			(1)
#define FIM_MIN_PIC_INDEX			(0)
#define FIM_NR_PICS				(FIM_MAX_PIC_INDEX-FIM_MIN_PIC_INDEX+1)


/* @XXX: Place this macros in another place? */
#define NS92XX_FIM_GEN_CTRL_STOP_PIC		~NS92XX_FIM_GEN_CTRL_PROGMEM
#define NS92XX_FIM_GEN_CTRL_START_PIC		NS92XX_FIM_GEN_CTRL_PROGMEM



/* Please note that the maximal DMA-buffer size is 64kB */
/* @FIXME: Check that the maximal size of the descriptors is littler than one page */
#define PIC_DMA_RX_BUFFERS			(10)
#define PIC_DMA_TX_BUFFERS			(10)
#define PIC_DMA_BUFFER_SIZE			(1 * PAGE_SIZE)

/*
 * Internal structure for handling with the DMA-buffer descriptors
 * p_desc : Physical descriptors address
 * v_desc : Virtual access address for the descriptors
 * v_buf  : Virtual address of the memory buffers
 * length : Configured length of this buffer
 * tasked : Used for locking the descriptor
 */
struct pic_dma_desc_t {
	dma_addr_t src;
	size_t length;
//	atomic_t tasked;
	void *private;
	int total_length;
};


/*
 * Structure used by the FIM-API to configure the DMA-buffer and buffer-descriptors.
 * rxnr : Number of RX-DMA-buffers
 * rxsz : Size of each DMA-buffer (in Bytes)
 * txnr : Number of TX-DMA-buffers
 * txsz : Size for each TX-buffer (in Bytes)
 */
struct fim_dma_cfg_t {
	int rxnr;
	int rxsz;
	int txnr;
	int txsz;
};


/*
 * This structure should be used for transferring data with the API
 * length  : Date length to transfer
 * data    : Data buffer
 * private : The API will not touch this pointer
 * sent    : The external driver can use it for waking up sleeping processes
 */
struct fim_buffer_t {
	int length;
	unsigned char *data;
	void *private;
	int sent;
};


/* @TODO: We need perhaps another PIC-structure for the U-Boot */
struct pic_t {
	int irq;
	struct device *dev;
	u32 reg_addr;
	u32 instr_addr;
	u32 hwa_addr;
	u32 iohub_addr;
//	spinlock_t lock;
	int index;
//	atomic_t irq_enabled;
	int requested;
	
	/* RX-DMA structures */
	struct iohub_dma_fifo_t rx_fifo;
//	spinlock_t rx_lock;
	struct fim_dma_cfg_t dma_cfg;
	
	/* Variables for the DMA-memory buffers */
	dma_addr_t dma_phys;
//	void __iomem *dma_virt;
	size_t dma_size;
	
	/* Data for the handling of the TX-DMA buffers */
//	spinlock_t tx_lock;
	struct pic_dma_desc_t *tx_desc;
	struct iohub_dma_fifo_t tx_fifo;
//	atomic_t tx_tasked;
//	atomic_t tx_aborted;
	
	/* Info data for the sysfs */
	char fw_name[FIM_MAX_FIRMWARE_NAME];
	int fw_length;

	/* Functions for a low level access to the PICs */
	int (* is_running)(struct pic_t *);
	int (* start_at_zero)(struct pic_t *);
	int (* stop_and_reset)(struct pic_t *);
	int (* download_firmware)(struct pic_t *, const unsigned char *);
	int (* get_ctrl_reg)(struct pic_t *, int , unsigned int *);
	void (* set_ctrl_reg)(struct pic_t *, int , unsigned int );
	int (* get_exp_reg)(struct pic_t *, int , unsigned int *);
	int (* send_interrupt)(struct pic_t *, u32 );
};


/*
 * Structure with the GPIOs to use for the driver to be initialized
 * nr     : GPIO number
 * name   : Name to use for the GPIO
 * picval : Value to pass to the PIC-firmware
 * All GPIOs will be configured with NS921X_GPIO_CFG_FUNC_0 for the FIM-support
 */
struct fim_gpio_t {
	int nr;
	char *name;
	unsigned char picval;  /* Value to pass to firmware */
};

#define FIM_LAST_GPIO			-1


/* These are the functions of the FIM-API */
int fim_core_init(int pic_num, const unsigned char *fwbuf);
int pic_is_running(int pic_num);
int fim_send_interrupt(int pic_num, unsigned int code);

int fim_get_iohub_reg(int pic_num, int reg);
int fim_get_ctrl_reg(int pic_num, int reg);
int fim_get_exp_reg(int pic_num, int nr);
void fim_set_iohub_reg(int pic_num, int reg, unsigned int val);
void fim_set_ctrl_reg(int pic_num, int reg, unsigned int val);


struct fim_buffer_t *fim_wait_rxdma(int pic, uint timeout);
int fim_wait_txdma(int pic);
int fim_send_buffer(int pic, struct fim_buffer_t *buffer);
int fim_init_rxdma(int pic);

#endif /* ifndef _NS921X_FIM_API_H */



