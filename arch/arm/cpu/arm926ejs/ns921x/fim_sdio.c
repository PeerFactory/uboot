/* -*- linux-c -*-
 *
 * cpu/arm926ejs/ns921x/fims/fim_sdio.c
 *
 * Copyright (C) 2008 by Digi International Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version2  as published by
 * the Free Software Foundation.
 *
 * !Revision:   $Revision: 1.1.1.1 $
 * !Author:     Luis Galdos
 * !References: Based on the MMC-driver of Kyle Harris (kharris@nexus-tech.net)
 *
 */

#include <config.h>
#include <common.h>

#include <configs/userconfig.h>

#if defined(CONFIG_NS921X_FIM_SDIO)

/* @XXX: This is not the correct place for this macro, need to move to another place */
#define CONFIG_SUPPORT_MMC_PLUS

#include <asm/arch/ns921x_sys.h>
#include <asm/arch/ns921x_hub.h>
#include <asm/arch/ns921x_gpio.h>
#include <asm/arch/io.h>
#include <asm/arch/mmc.h>
#include <asm/arch/ns921x_fim.h>
#include <asm/arch/ns921x_dma.h>

#include <mmc.h>
#include <asm/errno.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/protocol.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <part.h>

#include "fim_reg.h"
#include "fim_sdio.h"

#define AUTODETECTION	0
#define SD_CARD		1
#define MMC_CARD	2

/* When reading data, print a dot for each 64K read */
#define BYTES_PROGRESS_DOT	16384

/* Include only the needed firmware header */
#if defined(CONFIG_CC9P9215) || defined(CONFIG_CCW9P9215)
# if defined(CONFIG_UBOOT_FIM_ZERO_SD)
#  include "fim_sdio0.h"
# elif defined(CONFIG_UBOOT_FIM_ONE_SD)
#  include "fim_sdio1.h"
# else
#  error "FIM number is not valid!"
# endif
#elif defined(CONFIG_CME9210)
# if defined(CONFIG_UBOOT_FIM_ZERO_SD)
#  include "fim_sdio0_9210.h"
# elif defined(CONFIG_UBOOT_FIM_ONE_SD)
#  include "fim_sdio1_9210.h"
# else
#  error "FIM number is not valid!"
# endif
#endif

//#define FIM_SDIO_DEBUG
# define printk_info(fmt,args...)                   printf(fmt, ##args)
# define printk_err(fmt,args...)                   printf("[ ERROR ] " fmt, ##args)

#if defined(FIM_SDIO_DEBUG)
# define printk_debug(fmt,args...)                   printf(fmt, ##args)
#else
# define printk_debug(fmt, args...)                  do { } while (0);
#endif

/* Values for the block read state machine */
enum fim_blkrd_state {
	BLKRD_STATE_IDLE		= 0,
	BLKRD_STATE_WAIT_ACK		= 1, /* Waiting for the block read ACK */
	BLKRD_STATE_WAIT_DATA		= 2, /* Waiting for the block read data */
	BLKRD_STATE_WAIT_CRC		= 3, /* Waiting for the CRC */
	BLKRD_STATE_HAVE_DATA		= 4, /* Have block read data with the CRC */
	BLKRD_STATE_TIMEOUTED		= 5, /* Timeout response from the PIC */
	BLKRD_STATE_CRC_ERR		= 6, /* Compared CRC (PIC and card) differs */
};

/* Values for the command state machine */
enum fim_cmd_state {
	CMD_STATE_IDLE			= 0,
	CMD_STATE_WAIT_ACK		= 1, /* Waiting for the response ACK */
	CMD_STATE_WAIT_DATA		= 2, /* Waiting for the response data */
	CMD_STATE_HAVE_RSP		= 3, /* Have response data */
	CMD_STATE_TIMEOUTED		= 4, /* Timeout response from the PIC */
	CMD_STATE_CRC_ERR		= 5, /* Compared CRC (PIC and card) differs */
};

/*
 * Response receive structure from the Card
 * resp  : Card response, with a length of 5 or 17 as appropriate
 * stat  : Opcode of the executed command
 * crc   : CRC
 */
struct fim_sd_rx_resp_t {
        unsigned char stat;
        unsigned char resp[FIM_SD_MAX_RESP_LENGTH];
        unsigned char crc;
}__attribute__((__packed__));

/*
 * Transfer command structure for the card
 * opctl : Control byte for the PIC
 * blksz : Block size
 * cmd   : Command to send to the card
 */
struct fim_sd_tx_cmd_t {
	unsigned char opctl;
	unsigned char blksz_msb;
        unsigned char blksz_lsb;
        unsigned char cmd[FIM_SD_TX_CMD_LEN];
}__attribute__((__packed__));

struct fim_sdio_t {
	enum fim_cmd_state cmd_state;
	enum fim_blkrd_state blkrd_state;
	struct mmc_command *mmc_cmd;
	int trans_blocks;
	int trans_sg;
	int reg;

	int waiting_process_next;
	int bus_width;
	struct mmc_card mmc_card;

	uint card_size;
	uint blk_size;
	uchar *blkrd_dst;
};

DECLARE_GLOBAL_DATA_PTR;

struct fim_sdio_t fim_sdio;
struct fim_sdio_t *port = &fim_sdio;

/* FIM SD control registers */
#define FIM_SD_REG_CLOCK_DIVISOR                0
#define FIM_SD_REG_INTERRUPT                    1
#define FIM_SDIO_MAIN_REG			5
#define FIM_SDIO_MAIN_START			(1 << 0)

#if defined(CONFIG_UBOOT_FIM_ZERO_SD)
static int pic_num = 0;
extern const unsigned char fim_sdio_firmware0[];
#define FIM_SDIO_FIRMWARE			(fim_sdio_firmware0)
#elif defined(CONFIG_UBOOT_FIM_ONE_SD)
static int pic_num = 1;
extern const unsigned char fim_sdio_firmware1[];
#define FIM_SDIO_FIRMWARE			(fim_sdio_firmware1)
#endif
#if (CONFIG_COMMANDS & CFG_CMD_FAT)
extern int fat_register_device(block_dev_desc_t *dev_desc, int part_no);
#endif
static block_dev_desc_t mmc_dev;
static uint  card;
static unsigned char sdhc = 0;

/*
 * This function is called for obtaining the device to a corresponding device number
 * (U-Boot) fatls mmc 1:1 /
 */
block_dev_desc_t * mmc_get_dev(int dev)
{
	return ((block_dev_desc_t *)&mmc_dev);
}

/* Internal function prototypes */
static void *fim_sd_dma_to_sg(struct fim_sdio_t *port, struct mmc_data *data,
			      unsigned char *dma_buf, int dma_len);

inline static void fim_sd_parse_resp(struct mmc_command *cmd,
				     struct fim_sd_rx_resp_t *resp)
{
	unsigned char *ptr;
	ptr = (unsigned char *)cmd->resp;
	if (cmd->flags & MMC_RSP_136) {
		*ptr++ = resp->resp[3];
		*ptr++ = resp->resp[2];
		*ptr++ = resp->resp[1];
		*ptr++ = resp->resp[0];
		*ptr++ = resp->resp[7];
		*ptr++ = resp->resp[6];
		*ptr++ = resp->resp[5];
		*ptr++ = resp->resp[4];
		*ptr++ = resp->resp[11];
		*ptr++ = resp->resp[10];
		*ptr++ = resp->resp[9];
		*ptr++ = resp->resp[8];
		*ptr++ = resp->resp[15];
		*ptr++ = resp->resp[14];
		*ptr++ = resp->resp[13];
		*ptr++ = resp->resp[12];
	} else {
		*ptr++ = resp->resp[3];
		*ptr++ = resp->resp[2];
		*ptr++ = resp->resp[1];
		*ptr++ = resp->resp[0];
		*ptr++ = resp->resp[4];
		*ptr++ = resp->stat;
	}
}

/*
 * This function checks the CRC by block read transfer
 * The information about the length and content of the CRC was obtained
 * from the firmware-source code (sd.asm)
 */
inline static int fim_sd_check_blkrd_crc(struct fim_sdio_t *port, unsigned char *data,
                                         int length)
{
        int crc_len;
        unsigned char *pic_crc;

        /*
         * The CRC length depends on the bus width (see sd.asm)
         * No CRC enabled : One byte (0x00)
         * One bit bus    : Four bytes
         * Four bit bus   : Eight bytes
         */
        if (!(port->mmc_cmd->flags & MMC_RSP_CRC)) {
                crc_len = 1;
                pic_crc = data;
        } else if (port->bus_width == MMC_BUS_WIDTH_1) {
                crc_len = 4;
                pic_crc = data + 2;
        } else {
                crc_len = 16;
                pic_crc = data + 8;
        }

        if (crc_len != length) {
                printk_err("Unexpected CRC length %i (expected %i)\n",
                       length, crc_len);
                return -EINVAL;
        }


        /*
         * Code for forcing a CRC-error and the behavior of the MMC-layer
         * crc_error = 10 : Error reading the partition table
         * crc_error = 40 : Error by a block read transfer
         */
#ifdef FIM_SD_FORCE_CRC
        static int crc_error = 0;
        if (crc_error == 40) {
                crc_error++;
                return 1;
        } else
                crc_error++;
#endif

        /* If the CRC is disabled, the PIC only appended a dummy Byte */
        if (crc_len == 1)
                return 0;

        return memcmp(data, pic_crc, crc_len >> 1);
}

static void fim_sd_process_next(struct fim_sdio_t *port)
{
	struct mmc_command *cmd;

	/* mmc0: req done (CMD41): 0: 00ff8000 00003fff 00000000 00000000 */
	cmd = port->mmc_cmd;
	printk_debug("mmc0: req done (CMD%u): %d: %08x %08x %08x %08x\n",
		     cmd->opcode, cmd->error,
		     cmd->resp[0], cmd->resp[1],
		     cmd->resp[2], cmd->resp[3]);

	port->waiting_process_next = 0;
}

/*
 * Called when a receive DMA-buffer was closed.
 * Unfortunately the data received from the PIC has different formats. Sometimes it
 * contains a response, sometimes data of a block read request and sometimes the CRC
 * of the read data. In the case of a read transfer it is really amazing, then
 * the transfer consists in four DMA-buffers.
 */
static void fim_sd_rx_isr(/* struct fim_driver *driver, int irq, */
			  struct fim_buffer_t *pdata)
{
	/* struct fim_sdio_t *port; */
	struct mmc_command *mmc_cmd;
	struct fim_sd_rx_resp_t *resp;
	int len, crc_len;
	unsigned char *crc_ptr;
	int is_ack;

	/* Get the correct port from the FIM-driver structure */
	len = pdata->length;
	/* port = (struct fim_sdio_t *)driver->driver_data; */
	/* spin_lock(&port->mmc_lock); */

	/*
	 * The timeout function can set the command structure to NULL, for this reason
	 * check here is we can handle the response correctly
	 */
	if ((mmc_cmd = port->mmc_cmd) == NULL) {
		printk_err("Timeouted command response?\n");
		/* goto exit_unlock; */
	}

	/*
	 * Check the current state of the command and update it if required
	 * IMPORTANT: The buffer can contain response data or the data from a block
	 * read too, for this reason was implemented the state machine
	 */
	resp = (struct fim_sd_rx_resp_t *)pdata->data;
	is_ack = (pdata->length == 1) ? 1 : 0;

	printk_debug("CMD%i | PIC stat %x | CMD stat %i | BLKRD stat %i | Len %i\n",
	       mmc_cmd->opcode, resp->stat, port->cmd_state,
	       port->blkrd_state, pdata->length);

	/*
	 * By the ACKs the PIC will NOT send a timeout. Timeouts are only
	 * set by the response and and block read data
	 */
	if (is_ack && resp->stat & FIM_SD_RX_TIMEOUT) {
		mmc_cmd->error = -ETIMEDOUT;
		port->blkrd_state = BLKRD_STATE_HAVE_DATA;
		port->cmd_state = CMD_STATE_HAVE_RSP;

		/* Check the conditions for the BLOCK READ state machine */
	} else if (port->blkrd_state == BLKRD_STATE_WAIT_ACK && is_ack &&
		   resp->stat & FIM_SD_RX_BLKRD) {
		port->blkrd_state = BLKRD_STATE_WAIT_DATA;

		/* Check if the block read data has arrived */
	} else if (port->blkrd_state == BLKRD_STATE_WAIT_DATA && !is_ack) {
		crc_len = len - mmc_cmd->data->blksz;
		crc_ptr = pdata->data + mmc_cmd->data->blksz;
		port->blkrd_state = BLKRD_STATE_HAVE_DATA;

		if (fim_sd_check_blkrd_crc(port, crc_ptr, crc_len)) {
			mmc_cmd->error = -EILSEQ;
		} else {
			/*
			 * @UBOOT: Only pass the number of data bytes to the copy
			 * function, otherwise the CRC will be copied too!
			 */
			fim_sd_dma_to_sg(port, mmc_cmd->data,
					 pdata->data, pdata->length - crc_len);
 		}

		/* Check if we have a multiple transfer read */
		port->trans_blocks -= 1;
		if (port->trans_blocks > 0)
			port->blkrd_state = BLKRD_STATE_WAIT_DATA;

		/* Check the conditions for the COMMAND state machine */
	} else if (is_ack && port->cmd_state == CMD_STATE_WAIT_ACK &&
		   resp->stat & FIM_SD_RX_RSP) {
		port->cmd_state = CMD_STATE_WAIT_DATA;

	} else if (!is_ack && port->cmd_state == CMD_STATE_WAIT_DATA) {
		fim_sd_parse_resp(mmc_cmd, resp);
		port->cmd_state = CMD_STATE_HAVE_RSP;

		/* Check for unexpected acks or opcodes */
	} else {

		/* @FIXME: Need a correct errror handling for this condition */
		printk_err("Unexpected RX stat (CMD%i | PIC stat %x | Length %i)\n",
		       mmc_cmd->opcode, resp->stat, pdata->length);
	}

	/*
	 * By errors set the two states machines to the end position for sending
	 * the error to the MMC-layer
	 */
	if (mmc_cmd->error) {
		port->cmd_state = CMD_STATE_HAVE_RSP;
		port->blkrd_state = BLKRD_STATE_HAVE_DATA;
	}

	/*
	 * Now evaluate if need to wait for another RX-interrupt or
	 * can send the request done to the MMC-layer
	 */
	if (port->cmd_state == CMD_STATE_HAVE_RSP &&
	    port->blkrd_state == BLKRD_STATE_HAVE_DATA)
		fim_sd_process_next(port);

/*  exit_unlock: */
	/* spin_unlock(&port->mmc_lock); */
}

/*
 * Function called when RD data has arrived
 * Return value is the pointer of the last byte copied to the scatterlist, it can
 * be used for appending more data (e.g. in multiple block read transfers)
 */
static void *fim_sd_dma_to_sg(struct fim_sdio_t *port, struct mmc_data *data,
			      unsigned char *dma_buf, int dma_len)
{
/*         unsigned int len, cnt, process; */
/*         struct scatterlist *sg; */
/*         char *sg_buf; */

/*         sg = data->sg; */
/*         len = data->sg_len; */

/*         /\* Need a correct error handling *\/ */
/* #if !defined(FIM_SD_MULTI_BLOCK) */
/*         if (len > 1) { */
/*                 printk_err("The FIM-SD host driver only supports single block\n"); */
/*                 len = 1; */
/*         } */
/* #endif */

/*         /\* This loop was tested only with single block transfers *\/ */
/*         sg_buf = NULL; */
/*         for (cnt = port->trans_sg; cnt < len && dma_len > 0; cnt++) { */
/*                 process = dma_len > sg[cnt].length ? sg[cnt].length : dma_len; */
/*                 sg_buf = sg_virt(&sg[cnt]); */
/*                 memcpy(sg_buf, dma_buf, process); */
/*                 dma_buf += process; */
/*                 dma_len -= process; */
/*                 data->bytes_xfered += process; */
/*                 sg_buf += process; */
/*                 port->trans_sg += 1; */
/*         } */

/*         return sg_buf; */

	memcpy(port->blkrd_dst, dma_buf, dma_len);

	return NULL;
}

static struct fim_buffer_t *fim_sd_alloc_cmd(void)
{
	static struct fim_sd_tx_cmd_t fim_cmd;
	static struct fim_buffer_t fim_buf = {
		.length = sizeof(struct fim_sd_tx_cmd_t),
		.data = (uchar *)&fim_cmd,
		.sent = 0,
		.private = NULL,
	};

	return &fim_buf;
}

/* Send a buffer over the FIM-API */
static int fim_sd_send_buffer(struct fim_buffer_t *buf)
{
	int retval;

	if (!buf)
		return -EINVAL;

	if ((retval = fim_send_buffer(pic_num, buf)))
		printk_err("FIM send buffer request failed.\n");

	return retval;
}

/* This function will send the command to the PIC using the TX-DMA buffers */
static int fim_sd_send_command(struct mmc_command *cmd)
{
	struct mmc_data *data;
	struct fim_buffer_t *buf;
	struct fim_sd_tx_cmd_t *txcmd;
	unsigned int block_length, blocks;
	int retval, length;

	/* @TODO: Send an error response to the MMC-core */
	buf = fim_sd_alloc_cmd();

	/* Use the buffer data for the TX-command */
	txcmd = (struct fim_sd_tx_cmd_t *)buf->data;
	txcmd->opctl = 0;

	/*
	 * Set the internal flags for the next response sequences
	 * Assume that we will wait for a command response (not block read).
	 * By block reads the flag will be modified inside the if-condition
	 */
	port->cmd_state = CMD_STATE_WAIT_ACK;
	port->blkrd_state = BLKRD_STATE_HAVE_DATA;
	if ((data = cmd->data) != NULL) {
		block_length = data->blksz;
		blocks = data->blocks;

#if !defined(FIM_SD_MULTI_BLOCK)
		if (blocks != 1) {
			printf("Only supports single block transfer (%i)\n", blocks);
			cmd->error = -EILSEQ;
			fim_sd_process_next(port);
			return -EILSEQ;
		}
#endif

		printk_debug("Transfer of %i blocks (len %i)\n", blocks, block_length);

/* 		/\* Reset the scatter list position *\/ */
/* 		port->trans_sg = 0; */
		port->trans_blocks = blocks;
		fim_set_ctrl_reg(pic_num, 2, blocks);

		/* Check if the transfer request is for reading or writing */
		if (cmd->data->flags & MMC_DATA_READ) {
			txcmd->opctl |= SDIO_FIFO_TX_BLKRD;
			port->blkrd_state = BLKRD_STATE_WAIT_ACK;
		} else
			txcmd->opctl |= SDIO_FIFO_TX_BLKWR;
	} else {
		block_length = 0;
		blocks = 0;
	}

	/* Set the correct expected response length */
	if (cmd->flags & MMC_RSP_136)
		txcmd->opctl |= SDIO_FIFO_TX_136RSP;
	else
		txcmd->opctl |= SDIO_FIFO_TX_48RSP;

	/* Set the correct CRC configuration */
	if (!(cmd->flags & MMC_RSP_CRC)) {
		printk_debug("CRC is disabled\n");
		txcmd->opctl |= SDIO_FIFO_TX_DISCRC;
	}

	/* Set the correct bus width for the FIM transfer */
	if (port->bus_width == MMC_BUS_WIDTH_4) {
		printk_debug("Bus width has four bits\n");
		txcmd->opctl |= SDIO_FIFO_TX_BW4;
	}

	txcmd->blksz_msb = (block_length >> 8);
	txcmd->blksz_lsb =  block_length;
	txcmd->cmd[0] = SDIO_HOST_TX_HDR | (cmd->opcode & SDIO_HOST_CMD_MASK);
	txcmd->cmd[1] = cmd->arg >> 24;
	txcmd->cmd[2] = cmd->arg >> 16;
	txcmd->cmd[3] = cmd->arg >> 8;
	txcmd->cmd[4] = cmd->arg;

	/*
	 * Store the private data for the callback function
	 * If an error ocurrs when sending the buffer, the timeout function will
	 * send the error to the MMC-layer
	 */
/* 	port->buf = buf; */
	port->mmc_cmd = cmd;
/* 	buf->private = port; */
/* 	mod_timer(&port->mmc_timer, jiffies + msecs_to_jiffies(FIM_SD_TIMEOUT_MS)); */
	if ((retval = fim_sd_send_buffer(buf))) {
		printk_err("MMC command %i (err %i)\n", cmd->opcode, retval);
		goto exit_ok;
	}

	if (fim_wait_txdma(pic_num)) {
		ulong ifs;

		/* If we have a timeout then try to get some infos about it */
		ifs = readl(FIM_IOHUB_ADDR(pic_num) + HUB_INT);
		printk_err("FIM%i: Timeout sending CMD%u [0x%08x]\n",
			   pic_num, cmd->opcode, ifs);
		retval = -EBUSY;
	}


	/*
	 * If we have a write command then fill a next buffer and send it
	 * @TODO: We need here an error handling, then otherwise we have started a
	 * WR-transfer but have no transfer data (perhaps not too critical?)
	 */
	if (data && data->flags & MMC_DATA_WRITE) {
		length = data->blksz * data->blocks;
/* 		if (!(buf = fim_sd_alloc_buffer(port, length))) { */
/* 			printk_err("Buffer alloc BLKWR failed, %i\n", length); */
/* 			goto exit_ok; */
/* 		} */

/* 		buf->private = port; */
/* 		fim_sd_sg_to_dma(port, data, buf); */
/* 		if ((retval = fim_sd_send_buffer(port, buf))) { */
/* 			printk_err("Send BLKWR-buffer failed, %i\n", retval); */
/* 			fim_sd_free_buffer(port, buf); */
/* 		} */
	}

 exit_ok:
	return retval;
}

static int mmc_cmd(u32 opcode, u32 arg, unsigned int flags, struct mmc_data *data)
{
	static struct mmc_command mmc_cmd;
	struct fim_buffer_t *rxbuf;

	/* Send a reset command first */
	mmc_cmd.opcode = opcode;
	mmc_cmd.arg = arg;
	mmc_cmd.data = data;
	mmc_cmd.flags = flags;
	mmc_cmd.error = 0;
	memset(mmc_cmd.resp, 0, sizeof(mmc_cmd.resp));
	if (fim_sd_send_command(&mmc_cmd))
		return -ENODEV;

	port->waiting_process_next = 1;
	while (port->waiting_process_next) {

		if (!(rxbuf = fim_wait_rxdma(pic_num, 1000))) {
			printk_err("Timeout by RX-DMA %i (INT 0x%08lx)\n",
				   pic_num, readl(FIM_IOHUB_ADDR(pic_num) + HUB_INT));
			return -ENODEV;
		}

		/* A new DMA-buffer is full, so call the handler */
		fim_sd_rx_isr(rxbuf);
	}

	return mmc_cmd.error;
}

static void set_bus_width(int width)
{
	u32 flags, arg;

	if(card == MMC_CARD) {
		flags = 0x0000049d;
		arg = 0x03b70000 | (width << 7);
		if (mmc_cmd(MMC_SWITCH, arg, flags, NULL)) {
			printk_err("Setting the bus width!\n");
			return;
		}
	} else  {
		/* CMD55: Prepare for the next application command */
		flags = 0x00000095;
		if (mmc_cmd(MMC_APP_CMD, port->mmc_card.rca << 16, flags, NULL)) {
			printk_err("Couldn't prepare the card for the APPCMD6\n");
			return;
		}

		/* APCMD6: Set the bus width */
		flags = 0x00000015;
		if (mmc_cmd(SD_APP_SET_BUS_WIDTH, width, flags, NULL)) {
			printk_err("Setting the bus width!\n");
			return;
		}
	}
}

void get_sd_scr(void)
{
	uint scr1, sd_spec;
	struct mmc_data data;

	/* ACMD51: Get the SCR */
	mmc_cmd(MMC_APP_CMD, port->mmc_card.rca << 16, 0x000000f5, NULL);
	data.blocks = 1;
	data.blksz = 8;
	data.flags = MMC_DATA_READ;
	mmc_cmd(SD_APP_SEND_SCR, 0x0, 0x000000b5, &data);

	udelay(1000);
	scr1 = port->mmc_cmd->resp[0];

	if (scr1 & 0x1<<24)
		sd_spec = 1;	/* Version 1.10, support CMD6 */
	else
		sd_spec = 0;	/* Version 1.0 ~ 1.01 */

	printf("sd_spec = 1.%d(0x%08x)\n", sd_spec, scr1);
}

uint check_sd_ocr(void)
{
	uint i, ret = 0;

	for(i = 0; i < 0x80; i++) {
		/* CMD55: Prepare for a next Application command (ACMD41) */
		mmc_cmd(MMC_APP_CMD, 0, 0x000000f5, NULL);
		/* CMD41: Get the OCR from the card */
		mmc_cmd(SD_APP_OP_COND, 0x40300000, 0x000000e1, NULL);
		/*
		 * If the busy bit is set, then the card informs us that the power up
		 * procedure is done.
		 */
		if (!port->mmc_cmd->error && (port->mmc_cmd->resp[0] & MMC_CARD_BUSY)) {
			if (port->mmc_cmd->resp[0] & MMC_HIGH_CAPACITY)
				sdhc = 1;
			ret = SD_CARD;
			break;
		}
		else
			udelay(10000);
	}

	return ret;
}

uint check_mmc_ocr(void)
{
	uint i, ret = 0;

	for(i = 0; i < 5; i++) {
		/* CMD1: Get the OCR from the card */
		mmc_cmd(MMC_SEND_OP_COND, 0x40300000, 0x000000e1, NULL);
		/*
		 * If the busy bit is set, then the card informs us that the power up
		 * procedure is done.
		 */
		if (!port->mmc_cmd->error && (port->mmc_cmd->resp[0] & MMC_CARD_BUSY)) {
			if (port->mmc_cmd->resp[0] & MMC_HIGH_CAPACITY)
				sdhc = 1;
			ret = MMC_CARD;
			break;
		}
		else
			udelay(100000);
	}

	return ret;
}

/*
 * Function for reading data from the card
 * dst : Memory address to transfer the read data to
 * src : Address inside the card to read from
 * len : Block size to read (must be configured first)
 */
int mmc_block_read(uchar *dst, ulong src, ulong len)
{
	struct mmc_data data;
	int retval;
	u32 flags;

	port->blkrd_dst = dst;

	/* CMD17: Read a single block from the card */
	data.blocks = 1;
	data.blksz = port->blk_size;
	data.flags = MMC_DATA_READ;
	flags = 0xb5 | MMC_RSP_CRC;

	if (sdhc) {
		/* Convert src to Block addressing */
		src /= port->blk_size;
	}
	retval = mmc_cmd(MMC_READ_SINGLE_BLOCK, src, flags, &data);

	return retval;
}

int mmc_read(ulong src, uchar *dst, int size)
{
	ulong blksz, blocks;
	ulong cnt;
	u8 *ptr;
	u32 progress = 0;

	/* @XXX: Sanity check probably not required */
	if (size == 0) {
		printk_err("Zero data to read?\n");
		return 0;
	}

	blksz = port->blk_size;
	blocks = (size) / blksz;

	printk_debug("Read: src %lx to dest %lx (Blocks %i x %luB)\n",
		    src, (ulong)dst, blocks, blksz);

	/* Main loop for reading the different blocks */
	for (ptr = dst, cnt = 0; cnt < blocks; cnt++) {
		if ((mmc_block_read((uchar *)ptr, src, blksz)) < 0) {
			printk_err("Block read error. Aborting\n");
			return -1;
		}
		if (++progress == BYTES_PROGRESS_DOT / blksz) {
			printf(".");
			progress = 0;
		}

		src += blksz;
		ptr += blksz;
	}

	return 0;
}

int mmc_write(uchar *src, ulong dst, int size)
{
	return 0;
}

ulong mmc_bread(int dev_num, ulong blknr, ulong blkcnt, ulong *dst)
{
	ulong src = (blknr * port->blk_size)/*  + CFG_MMC_BASE */;

	printk_debug("%s: Read %i block(s) from %i | Dest %p\n",
		     __func__, blkcnt, blknr, dst);

	/* In some cases the caller pass ZERO as number of blocks to read */
	blkcnt = (!blkcnt) ? (1) : blkcnt;

        mmc_read(src, (uchar *)dst, blkcnt * port->blk_size);
        return blkcnt;
}

/* Below function is coming from the S3C24XX machine code */
static int fim_sdio_fill_csd(u32 *resp)
{
        uint c_size, c_size_multi, read_bl_len;
	uint csd_struct;
	uint mb;

	if (card == SD_CARD) {
		/* CSD struct is at bits [127:126] i.e. R0[31:30]
		* At this point Response is: R0-R1-R2-R3 where
		* R0.31 is the MSB and R3.0 the LSB (unshifted) */
		csd_struct = resp[0] >> 30;
		switch (csd_struct) {
			case 0:
				sdhc = 0;
				break;
			case 1:
				/* SDHC card > 2Gb.
				* This should have already been discovered
				* in the OCR data (bit 30), but just in case */
				sdhc = 1;
				break;
			default:
				printf("ERROR: incorrect CSD Structure version. Card not supported\n");
				return -1;
		}
	}

	/* Common fields for SD/SDHC/MMC */
        read_bl_len = (resp[1] >> 16) & 0xf;
        port->blk_size = (1 << read_bl_len);
        printf("Block Size: %d Bytes\n", port->blk_size);

	if (sdhc) {
		/* Special size calculation procedure for
		 * SDHC cards: In theory, they are 22 bits,
		 * but the specification says that only 16 bits
		 * are valid. c_size is given in blocks */
		c_size = resp[2] >> 16;
		port->card_size = (c_size + 1) << 10;	/* in blocks */
	} else {
		/* Standard size calculation procedure for
		 * SD and MMC cards */
		c_size = ((resp[1] & 0x3ff) << 2 ) | ((resp[2] >> 30) & 0x3);
		c_size_multi = ((resp[2] >> 15) & 0x7);
		port->card_size = (c_size + 1) *
			    (1 << (c_size_multi + 2)); /* in blocks */
	}
	/* Card size in MB + security area */
	mb = (port->card_size * port->blk_size) / 1048576 + 1;
	if (mb / 1024 > 0)
		printf("Card Size: %d.%d GBytes\n",
			mb / 1024,
			(mb % 1024)*100/1024);
	else
		printf("Card Size: %d MBytes\n", mb);

	return 0;
}

/*
 * This function is called over the command "mmcinit"
 */
int mmc_init(uint card_type, uint width, uint highspeed)
{
 	int rc = -ENODEV;
	unsigned int flags;
	unsigned long clkdiv;
	int fwver, rca;

	//printk_info("Firmware address 0x%08x\n", FIM_SDIO_FIRMWARE);

	/* Reset sdhc flag */
	sdhc = 0;

	/* Check if the instruction cache is enabled, otherwise return at this point */
	if (!icache_status()) {
		printk_err("Instruction cache required for this operation (icache on)\n");
		return -EAGAIN;
	}

	/* Install the FIM-firmware for the SDIO-port */
        if(fim_core_init(pic_num, FIM_SDIO_FIRMWARE)) {
		printf("Couldn't install the FIM-firmware\n");
                return -ENODEV;
	}

	fwver = fim_get_exp_reg(pic_num, FIM_SDIO_VERSION_SREG);
	printk_debug("FIM%u: Firmware revision rev%02x\n", pic_num, fwver);

	/* Configure the GPIOs */
	printk_debug("Configuring GPIOs: %d,%d,%d,%d,%d,%d,%d\n",
		FIM_SDIO_D0,
		FIM_SDIO_D1,
		FIM_SDIO_D2,
		FIM_SDIO_D3,
		FIM_SDIO_CD,
		FIM_SDIO_CLK,
		FIM_SDIO_CMD);
	gpio_cfg_set(FIM_SDIO_D0, GPIO_CFG_FUNC_FIM_SDIO_D0);
#if defined(CONFIG_CC9P9215) || defined(CONFIG_CCW9P9215)
	gpio_cfg_set(FIM_SDIO_D1, GPIO_CFG_FUNC_FIM_SDIO_D1);
	gpio_cfg_set(FIM_SDIO_D2, GPIO_CFG_FUNC_FIM_SDIO_D2);
	gpio_cfg_set(FIM_SDIO_D3, GPIO_CFG_FUNC_FIM_SDIO_D3);
#endif
	gpio_cfg_set(FIM_SDIO_CD, GPIO_CFG_FUNC_FIM_SDIO_CD);
	gpio_cfg_set(FIM_SDIO_CLK, GPIO_CFG_FUNC_FIM_SDIO_CLK);
	gpio_cfg_set(FIM_SDIO_CMD, GPIO_CFG_FUNC_FIM_SDIO_CMD);

	/* Configure the write protect GPIO as input IO */
	gpio_cfg_set(FIM_SDIO_WP,
		     GPIO_CFG_INPUT | GPIO_CFG_PULLUP_DISABLE | GPIO_CFG_FUNC_GPIO);

	/* turn on the power */
	udelay(1000);

	fim_init_rxdma(pic_num);

	/* Configure the clock with 320kHz */
	clkdiv = 0x70;
	fim_set_ctrl_reg(pic_num, FIM_SD_REG_CLOCK_DIVISOR, clkdiv);

	/* Start the code execution on the FIM */
	fim_set_ctrl_reg(pic_num, FIM_SDIO_MAIN_REG, FIM_SDIO_MAIN_START);

	port->bus_width = MMC_BUS_WIDTH_1;

	/* A timeout will happen by this reset-command (but isn't a failure) */
	mmc_cmd(MMC_GO_IDLE_STATE, 0, 0x000000c0, NULL);

	/* CMD8: Get the Card Specific Data from the card */
	if ((rc = mmc_cmd(MMC_SEND_EXT_CSD, 0x000001aa, 0x000002f5, NULL))) {
		/* printk_err("FIM%u: Couldn't get the CSD\n", pic_num); */
		/* return rc; */
	}

	switch(card_type) {
		case AUTODETECTION:
		case MMC_CARD:
			if(check_mmc_ocr()) {
				card = MMC_CARD;
				printf("MMC card is detected\n");
				break;
			}
		case SD_CARD:
			if(check_sd_ocr()) {
				card = SD_CARD;
				printf("SD%s card is detected\n",
					sdhc ? "HC" : "" );
				break;
			}
		default:
			return 1;
	}

	/* CMD2: Send the CID to the MMC-host */
	flags = MMC_RSP_R2 | MMC_CMD_BCR;
	if ((rc = mmc_cmd(MMC_ALL_SEND_CID, 0x0, flags, NULL))) {
		printk_err("Reading the CID\n");
		return rc;
	}

	/* CMD3: Get the relative card address (RCA) */
	rca = (card & MMC_CARD) ? 0x1 : 0x0;
	if ((rc = mmc_cmd(MMC_SET_RELATIVE_ADDR, rca << 16, 0x00000075, NULL))) {
		printk_err("Setting the relative address\n");
		return rc;
	}
	/* Read RCA only on SD cards */
	if (!rca)
		rca = port->mmc_cmd->resp[0] >> 16;
	port->mmc_card.rca = rca;

	/* CMD9: Get the card specific data (CSD) */
	if ((rc = mmc_cmd(MMC_SEND_CSD, port->mmc_card.rca << 16, 0x00000087, NULL))) {
		printk_err("Error on CMD9\n");
		return rc;
	}
	fim_sdio_fill_csd(port->mmc_cmd->resp);

	/* CMD7: Select the card for the next operations */
	if ((rc = mmc_cmd(MMC_SELECT_CARD, port->mmc_card.rca << 16, 0x00000015, NULL))) {
		printk_err("Error on CMD7\n");
		return rc;
	}

#if defined(CONFIG_CC9P9215) || defined(CONFIG_CCW9P9215)
	/* Set bus width to 4 bits in cc(w)9p9215 */
	port->bus_width = MMC_BUS_WIDTH_4;
#endif
	set_bus_width(port->bus_width);

	/* Now, the card can be driven at full speed */
	fim_set_ctrl_reg(pic_num, FIM_SD_REG_CLOCK_DIVISOR, 0x4);

	/* FAT and ext2 require a block size of 512.
	 * it needs to be forced despite of the value in CSD */
	if (MMC_BLOCK_SIZE != port->blk_size)
		port->blk_size = MMC_BLOCK_SIZE;

	/* CMD16: Set the correct block length to read */
	flags = 0x00000095; //MMC_RSP_CRC | MMC_CMDAT_R1;
	while(mmc_cmd(MMC_CMD_SET_BLOCKLEN, port->blk_size, flags, NULL));

	/* Setup the mmc device in order to register a FAT device */
        mmc_dev.if_type = IF_TYPE_MMC;
        mmc_dev.part_type = PART_TYPE_DOS;
        mmc_dev.dev = 0;
        mmc_dev.lun = 0;
        mmc_dev.type = 0;
        mmc_dev.blksz = port->blk_size;
        mmc_dev.lba = port->card_size;
        sprintf((char*) mmc_dev.vendor,"Man %02x%02x%02x Snr %02x%02x%02x",
                        0x00, 0x1, 0x2, 0x1, 0x2, 0x3);
        sprintf((char*) mmc_dev.product,"%s","SD/MMC");
        sprintf((char*) mmc_dev.revision,"%x %x",0x1, 0x1);
        mmc_dev.removable = 0;
        mmc_dev.block_read = mmc_bread;
#if (CONFIG_COMMANDS & CFG_CMD_FAT)
	fat_register_device(&mmc_dev, MMC_MAX_DEVICE);
#endif
	return 0;
}

int hs_mmc_get_dev(uint card, uint buswidth, uint highspeed)
{

	return -1;
}

int hs_mmc_init(uint card, uint buswidth, uint highspeed)
{

	return -1;
}

int mmc_ident(block_dev_desc_t *dev)
{
	return 0;
}

int mmc2info(ulong addr)
{
	/* FIXME hard codes to 32 MB device */
/* 	if (addr >= CFG_MMC_BASE && addr < CFG_MMC_BASE + 0x02000000) { */
/* 		return 1; */
/* 	} */
	return 0;
}

#endif /* CONFIG_NS921X_FIM_SDIO && CONFIG_MMC */
