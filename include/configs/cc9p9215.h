/*
 *  include/configs/cccp9215.h
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <../arch/arm/include/asm/sizes.h>
#include <ns921x.h>
#include <asm-arm/arch-ns9xxx/ns921x_sys.h>

#include <configs/digi_common.h>

#if !defined(CONFIG_DOWNLOAD_BY_DEBUGGER) && !defined(CONFIG_DIGIEL_USERCONFIG)
# define CONFIG_UBOOT_CHECK_CRC32_ON_BOOT
# define CONFIG_UBOOT_VERIFY_IN_SDRAM
#endif

/************************************************************
 * Definition of default options when not configured by user
 ************************************************************/
#if (!defined(CONFIG_DIGIEL_USERCONFIG))
# define CONFIG_AUTOLOAD_BOOTSCRIPT
#endif /* if !defined(CONFIG_DIGIEL_USERCONFIG) */

#define CONFIG_IDENT_STRING	" " VERSION_TAG "\nfor " MODULE_STRING " on " CONFIG_UBOOT_BOARDNAME_STR

#define CONFIG_GCC41		1

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM926EJS	1	/* This is an ARM926EJS Core	*/
#define CONFIG_NS921X		1	/* in an NetSilicon NS921x SoC */
#define CONFIG_NS9215		1	/* in an NetSilicon NS9215 SoC */
#define CONFIG_CC9P9215		1       /* on a ConnectCore 9P 9360 module */
#if defined(CONFIG_JSCC9P9215)
#define CONFIG_MACH_CC9P9215JS	1	/* Select board mach-type */
#else
#define CONFIG_MACH_CC9P9215	1	/* Select generic mach-type */
#endif
#define CONFIG_IS_NETSILICON    1
#define CPU     		"NS9215"

#if defined(CONFIG_JSCC9P9215)
# define PLATFORM		"js"
#else
#define PLATFORM "nor"
#endif

#define BOARD_LATE_INIT		1	/* Enables initializations before jumping to main loop */

#define CONFIG_MODULE_NAME	"cc9p9215"PLATFORM
#define CONFIG_MODULE_NAME_NETOS CONFIG_MODULE_NAME

#define CONFIG_SYS_CLK_FREQ 	sys_clock_freq()
#define CPU_CLK_FREQ		cpu_clock_freq()  /* for compat */
#define AHB_CLK_FREQ		ahb_clock_freq()  /* for compat */


/* @TODO */
/*#define	CFG_ENV_IS_IN_FLASH	1*/
#ifdef MISSING_MALLOC_SIZE_INFO_RLS

 * Defined in include/configs/digi_common.h 
 *
 * #define CFG_ENV_IS_NOWHERE
#define CFG_ENV_SIZE		0x10000	/* Total Size of Environment Sector */
#define CONFIG_ENV_SIZE     CFG_ENV_SIZE

/*
 * Size of malloc() pool
 */
#define CFG_MALLOC_LEN		(CFG_ENV_SIZE + 128*1024)
#define CFG_GBL_DATA_SIZE       128     /* size in bytes reserved for initial
					 * data */
#endif 

/************************************************************
 * Base board specific configurations
 ************************************************************/
#ifndef CONFIG_UBOOT_FIM_UART_PIC_NUM
# define CFG_NS921X_UART	1
# define CFG_CONSOLE            "ttyNS"
# if !defined(CONFIG_CONS_INDEX)
#  define CONFIG_CONS_INDEX	3	/*0= Port A; 1= Port B; 2= Port C; 3=Port D */
# endif
#else
# define CFG_CONSOLE            "ttyFIM"
# if defined(CONFIG_UBOOT_FIM_ZERO_SERIAL) && (CONFIG_UBOOT_FIM_UART_PIC_NUM == 0)
#  define CONFIG_CONS_INDEX	0
# elif defined(CONFIG_UBOOT_FIM_ONE_SERIAL) && (CONFIG_UBOOT_FIM_UART_PIC_NUM == 1)
#  define CONFIG_CONS_INDEX	1
# else
#  error "Bad configuration of FIM Console index"
# endif
#endif

#ifdef CONFIG_NS921X_FIM_UART
# if defined(CONFIG_UBOOT_FIM_ZERO_SERIAL) && (CONFIG_UBOOT_FIM_UART_PIC_NUM == 0)
#  define FIM_UART_TX			68
#  define FIM_UART_RX			69
#  define GPIO_CFG_FUNC_FIM_UART		GPIO_CFG_FUNC_0
# elif defined(CONFIG_UBOOT_FIM_ONE_SERIAL) && (CONFIG_UBOOT_FIM_UART_PIC_NUM == 1)
#  define FIM_UART_TX			72
#  define FIM_UART_RX			73
#  define GPIO_CFG_FUNC_FIM_UART		GPIO_CFG_FUNC_1
# endif
#endif

/* Configuration for the SDIO */
#if defined(CONFIG_CMD_MMC)
#define CONFIG_DOS_PARTITION 	1
#endif

#ifdef CONFIG_NS921X_FIM_SDIO
# define CONFIG_MMC		1
# if defined(CONFIG_UBOOT_FIM_ZERO_SD)
#  define FIM_SDIO_D0			68
#  define FIM_SDIO_D1			69
#  define FIM_SDIO_D2			70
#  define FIM_SDIO_D3			71
#  define FIM_SDIO_CLK			76
#  define FIM_SDIO_CMD			77
#  define FIM_SDIO_WP			100
#  define FIM_SDIO_CD			101
#  define GPIO_CFG_FUNC_FIM_SDIO_D0	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_D1	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_D2	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_D3	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_CLK	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_CMD	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_WP	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_CD	(GPIO_CFG_FUNC_0 | GPIO_CFG_PULLUP_DISABLE)
# elif defined(CONFIG_UBOOT_FIM_ONE_SD)
#  define FIM_SDIO_D0			72
#  define FIM_SDIO_D1			73
#  define FIM_SDIO_D2			74
#  define FIM_SDIO_D3			75
#  define FIM_SDIO_CLK			78
#  define FIM_SDIO_CMD			79
#  define FIM_SDIO_WP			100
#  define FIM_SDIO_CD			101
#  define GPIO_CFG_FUNC_FIM_SDIO_D0	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_D1	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_D2	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_D3	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_CLK	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_CMD	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_WP	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
#  define GPIO_CFG_FUNC_FIM_SDIO_CD	(GPIO_CFG_FUNC_1 | GPIO_CFG_PULLUP_DISABLE)
# else
#  error "No FIM SDIO number defined! Aborting."
# endif
#endif

/* MMC */
#ifdef CONFIG_CMD_MMC
# define CONFIG_MMC		1
#endif
#define CFG_MMC_BASE		0xf0000000
#define MMC_MAX_DEVICE		1
#define HSMMC_MAX_DEVICE	0

#define CONFIG_CMDLINE_EDITING

#define CONFIG_SERIAL_MULTI
#if defined(CONFIG_JSCC9P9215)
# define USER_KEY1_GPIO			81
# define USER_KEY2_GPIO			84
# define USER_LED1_GPIO			82
# define USER_LED2_GPIO			85
#endif

#define	CONFIG_RTC_NS921X		1

#define GPIO_ETH_PHY_RESET		90  /* GPIO that holds PHY in reset  */

/* some strapping values. Only used if defined here */
#define CFG_STRAP_PUT_ETH_OUT_OF_RESET	0x80

/************************************************************
 * Activate LEDs while booting
 ************************************************************/

#define CONFIG_STATUS_LED
#define CONFIG_SHOW_BOOT_PROGRESS

#define CONFIG_DRIVER_NS921X_ETHERNET 1	/* use on-chip ethernet */
#define NS921X_ETH_PHY_ADDRESS	 	(0x0007)

#define CONFIG_ZERO_BOOTDELAY_CHECK	/* check for keypress on bootdelay==0 */

#define CONFIG_SILENT_RESCUE	1

/* Video settings */
#ifdef CONFIG_DISPLAYS_SUPPORT
# define CONFIG_LCD			1
# if defined(CONFIG_UBOOT_EDTQVGA_TFT_LCD)
#  define LCD_BPP       LCD_COLOR16
# else
#  error "Please, define LCD_BPP accordingly to your display needs"
# endif
#endif

#ifdef CONFIG_UBOOT_SPLASH
# define CONFIG_SPLASH_SCREEN           1
# define CONFIG_SPLASH_WITH_CONSOLE     1
#endif  /* CONFIG_UBOOT_SPLASH */

#ifdef CONFIG_UBOOT_CMD_BSP_TESTHW
# define TEST_HW_EXTRA_CMDS		CFG_CMD_SPI
#endif

/***********************************************************
 * Command definition
 ***********************************************************/
#ifndef CONFIG_COMMANDS
#define CONFIG_COMMANDS \
	( CONFIG_COMMANDS_DIGI	| \
	TEST_HW_EXTRA_CMDS	| \
	CFG_CMD_DATE		| \
	CFG_CMD_I2C		| \
	CFG_CMD_FLASH		| \
	CFG_CMD_MII		| \
	CFG_CMD_SNTP		|  /* simple network time protocol */ \
	0 )
#endif
/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

#if !defined(CONFIG_UBOOT_PROMPT)
# define CFG_PROMPT 		"CC9P9215 # "	/* Monitor Command Prompt	*/
#else
# define CFG_PROMPT		CONFIG_UBOOT_PROMPT_STR
#endif

#ifndef CONFIG_UBOOT_BOARDNAME
# define CONFIG_UBOOT_BOARDNAME_STR "Development Board"
#endif

#define MODULE_STRING		"ConnectCore 9P 9215"

/* TODO */
#define CFG_MEMTEST_START	0x00200000	/* memtest works from    */
#define CFG_MEMTEST_END		0x00780000	/* 2Mb to 7,5 MB in DRAM */

#define	CFG_LOAD_ADDR		0x00200000	/* default load address	*/
#define	CFG_INITRD_LOAD_ADDR	0x00600000	/* default initrd  load address	*/
#define CFG_NETOS_LOAD_ADDR	0x00300000

#define	CFG_HZ			1000

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1    		/* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		0x00000000	 /* SDRAM Bank #1 */
/* size will be defined in include/configs when you make the config */


/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CFG_FLASH_CFI		1
#define	CFG_FLASH_CFI_DRIVER	1
#define CFG_FLASH_CFI_WIDTH	FLASH_CFI_16BIT
#define CFG_FLASH_PROTECTION    1

#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks */
#define PHYS_FLASH_1		0x50000000 /* Flash Bank #1 */
#define CFG_FLASH_BASE		PHYS_FLASH_1
#define CFG_MAX_FLASH_SECT	(256)	/* max number of sectors on one chip */

/* i2c bus */
#define CONFIG_DRIVER_NS9750_I2C 1	/* I2C driver */
#define CFG_I2C_SPEED 		100000	/* Hz */
#define CFG_I2C_SLAVE 		0x7F	/* I2C slave addr */
#define I2C_SCL_GPIO		102
#define I2C_SDA_GPIO		103
#define I2C_SCL_GPIO_FUNC	GPIO_CFG_FUNC_2
#define I2C_SDA_GPIO_FUNC	GPIO_CFG_FUNC_2

#define CFG_I2C_EEPROM_ADDR	0x50	/* EEPROM address */
#define CFG_I2C_EEPROM_ADDR_LEN	2	/* 2 address bytes */

#undef CFG_I2C_EEPROM_ADDR_OVERFLOW
#define CFG_EEPROM_PAGE_WRITE_BITS    5	/* 32 bytes page write mode on M24LC64 */
#define CFG_EEPROM_PAGE_WRITE_DELAY_MS 10
#define CFG_EEPROM_PAGE_WRITE_ENABLE   1

#define PART_UBOOT_SIZE	        0x00040000
#define PART_NVRAM_SIZE         0x00020000
#define PART_KERNEL_SIZE        0x00180000
#define PART_ROOTFS_SIZE        0x00000000  /* the rest */
#define PART_SPLASH_SIZE    	0x00100000

#ifdef CONFIG_NETOS_BRINGUP
/* NET+OS runs in big-endian, 9600 */
# undef CONFIG_BAUDRATE
# define CONFIG_BAUDRATE	9600  /* they take it patient */

# define CONFIG_PARTITION_SWAP
# define CFG_NETOS_SWAP_ENDIAN
# define PART_NETOS_LOADER_SIZE_VARIANT1	0x00010000
# define PART_NETOS_LOADER_SIZE_VARIANT2	0x00020000
# define PART_NETOS_LOADER_SIZE		PART_NETOS_LOADER_SIZE_VARIANT1
# define PART_NETOS_KERNEL_SIZE 0x002C0000
# define PART_NETOS_NVRAM_SIZE  0x00010000
#endif

#if (CONFIG_COMMANDS & CFG_CMD_BSP ) && defined(CONFIG_UBOOT_CMD_BSP_TESTHW)
# define CONFIG_USE_IRQ         /* for testhw powersave */

/* Select SPI support for hardware test purposes */
# define CONFIG_SOFT_SPI		/* Enable SPI driver */

/* Software (bit-bang) SPI driver configuration */
# define SPI_TX_GPIO		7
# define SPI_RX_GPIO		3
# define SPI_CLK_GPIO		5
# define SPI_EN_GPIO		0

# define SPI_INIT								\
{										\
	gpio_cfg_set(SPI_TX_GPIO, GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO);	\
	gpio_cfg_set(SPI_EN_GPIO, GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO);	\
	gpio_cfg_set(SPI_CLK_GPIO, GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO);	\
	gpio_cfg_set(SPI_RX_GPIO, GPIO_CFG_INPUT | GPIO_CFG_FUNC_GPIO);		\
	gpio_ctrl_set(SPI_EN_GPIO, 1);						\
	gpio_ctrl_set(SPI_CLK_GPIO, 1);						\
}

# define SPI_READ	(gpio_stat_get(SPI_RX_GPIO))
# define SPI_SDA(bit)	if(bit)  gpio_ctrl_set(SPI_TX_GPIO, 1);		\
			else     gpio_ctrl_set(SPI_TX_GPIO, 0)
# define SPI_SCL(bit)	if(bit)  gpio_ctrl_set(SPI_CLK_GPIO, 1);	\
			else     gpio_ctrl_set(SPI_CLK_GPIO, 0)
# define SPI_DELAY	udelay(spi_delay)
#endif /* CONFIG_UBOOT_CMD_BSP_TESTHW */

#include <configs/digi_common_post.h>

#endif	/* __CONFIG_H */
