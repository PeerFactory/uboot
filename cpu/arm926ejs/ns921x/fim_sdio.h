/* FIM SD registers, flags, and macros */

/* Registers with status information */
#define FIM_SD_GPIOS_REG			0x02
#define FIM_SD_GPIOS_REG_CD			0x01
#define FIM_SD_GPIOS_REG_WP			0x02
#define FIM_SD_CARD_STATREG			0x00

/* Interrupts from the FIM to the driver */
#define FIM_SD_INTARM_CARD_DAT1			0x01
#define FIM_SD_INTARM_CARD_DETECTED		0x02


/* Macros for the SDIO-interface to the FIM-firmware */
#define SDIO_HOST_TX_HDR			0x40
#define SDIO_HOST_CMD_MASK			0x3f
#define SDIO_FIFO_TX_48RSP			0x01
#define	SDIO_FIFO_TX_136RSP			0x02
#define SDIO_FIFO_TX_BW4			0x04
#define SDIO_FIFO_TX_BLKWR			0x08
#define SDIO_FIFO_TX_BLKRD			0x10
#define SDIO_FIFO_TX_DISCRC			0x20


/* User specified macros */
#define FIM_SD_TIMEOUT_MS			2000
#define FIM_SD_TX_CMD_LEN			5
#define FIM_SD_MAX_RESP_LENGTH			17


/* Status bits from the PIC-firmware */
#define FIM_SD_RX_RSP				0x01
#define FIM_SD_RX_BLKRD				0x02
#define FIM_SD_RX_TIMEOUT			0x04


/* FIM SD control registers */
#define FIM_SD_REG_CLOCK_DIVISOR		0
#define FIM_SD_REG_INTERRUPT			1


/* Internal flags for the request function */
#define FIM_SD_REQUEST_NEW			0x00
#define FIM_SD_REQUEST_CMD			0x01
#define FIM_SD_REQUEST_STOP			0x02
#define FIM_SD_SET_BUS_WIDTH			0x04


/* Macros for the DMA-configuraton */
#define FIM_SD_DMA_BUFFER_SIZE			PAGE_SIZE
#define FIM_SD_DMA_RX_BUFFERS			21
#define FIM_SD_DMA_TX_BUFFERS			10

/*
 * Expansion/Status register with the revision number
 */
#define FIM_SDIO_VERSION_SREG			(0)
