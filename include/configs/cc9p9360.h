/*
 *  include/configs/cc9p9360.h
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

/* needed for digi_common.h */
#if defined(CONFIG_A9M9750DEV)
# define CFG_CONS_INDEX_SUB_1
#endif

#include <asm-arm/sizes.h>
#include <ns9750_sys.h>
#include <configs/digi_common.h>

/************************************************************
 * Definition of default options when not configured by user
 ************************************************************/
#ifndef CONFIG_DIGIEL_USERCONFIG
# define CONFIG_DISPLAYS_SUPPORT
# define CONFIG_UBOOT_SPLASH
# define CONFIG_UBOOT_CRT_VGA
# define CONFIG_UBOOT_LQ057Q3DC12I_TFT_LCD
# define CONFIG_UBOOT_LQ064V3DG01_TFT_LCD
# define CONFIG_AUTOLOAD_BOOTSCRIPT
# define CONFIG_UBOOT_CMD_BSP_COMPAT
#endif /* ifndef(CONFIG_DIGIEL_USERCONFIG) */

#define CONFIG_IDENT_STRING	" " VERSION_TAG "\nfor " MODULE_STRING " on " CONFIG_UBOOT_BOARDNAME_STR

#define CONFIG_GCC41		1

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM926EJS	1	/* This is an ARM926EJS Core	*/
#define CONFIG_NS9360		1	/* in an NetSilicon NS9360 SoC     */
#define CONFIG_CC9P9360		1       /* on a ConnectCore 9P 9360 module */
#define CONFIG_A9M9360		1       /* old module reference name (added for compatibility) */
#define CONFIG_IS_NETSILICON    1
#define CPU     		"NS9360"
#if defined(CONFIG_JSCC9P9360)
# define CONFIG_MACH_CC9P9360JS		/* Select board mach-type */
# define PLATFORM		"js"
#elif defined(CONFIG_A9M9750DEV)
# define CONFIG_MACH_CC9P9360DEV
# define PLATFORM		"dev"
#else
# define CONFIG_MACH_CC9P9360VAL	/* Select board mach-type */
# define PLATFORM		"val"
#endif
#define BOARD_LATE_INIT		1	/* Enables initializations before jumping to main loop */
#define CRYSTAL 		294912
#define PLLFS			((*get_sys_reg_addr( NS9750_SYS_PLL ) >>0x17 ) & 0x3)
#define PLLND			((*get_sys_reg_addr( NS9750_SYS_PLL ) >>0x10 ) & 0x1f)

#define CONFIG_BOOT_NAND	1
#define CFG_NO_FLASH		1	/* No NOR-Flash available */
#define EBOOTFLADDR		"280000"
#define WCEFLADDR		"380000"
#ifndef CONFIG_DOWNLOAD_BY_DEBUGGER
# define CONFIG_HAVE_SPI_LOADER  1       /* that initializes SDRAM and */
#endif  /* CONFIG_DOWNLOAD_BY_DEBUGGER */

#define CONFIG_MODULE_NAME	"cc9p9360"PLATFORM
#define CONFIG_MODULE_NAME_WCE  "CC9P9360"
#define CONFIG_MODULE_NAME_NETOS "connectcore9p9360_a"
#define NS9750_ETH_PHY_ADDRESS	(0x0001)

#define CONFIG_SYS_CLK_FREQ 	(100*CRYSTAL * (PLLND +1) / (1<<PLLFS))

#define CPU_CLK_FREQ		(CONFIG_SYS_CLK_FREQ/2)
#define AHB_CLK_FREQ		(CONFIG_SYS_CLK_FREQ/4)
#define BBUS_CLK_FREQ		(CONFIG_SYS_CLK_FREQ/8)

#undef CONFIG_USE_IRQ			/* we don't need IRQ/FIQ stuff */

/************************************************************
 * Base board specific configurations
 ************************************************************/
#if !defined(CONFIG_CONS_INDEX)
# define CONFIG_CONS_INDEX	1	/*0= Port B; 1= Port A; 2= Port C; 3=Port D */
#endif

#ifndef CFG_CONSOLE
#define CFG_CONSOLE            "ttyS"
#endif

#define CONFIG_CMDLINE_EDITING

#if defined(CONFIG_JSCC9P9360)
# define CFG_NS9750_UART		1	/* use on-chip UART */
# define USER_KEY1_GPIO			67
# define USER_KEY2_GPIO			68
# define USER_LED1_GPIO			46
# define USER_LED2_GPIO			66
#elif defined(CONFIG_A9M9750DEV)
# define USER_KEY1_GPIO			45
# define USER_KEY2_GPIO			46

/* serial stuff */
# define CFG_NS16550
# define CFG_NS16550_SERIAL
# define CFG_NS16550_REG_SIZE    1
# define CFG_NS16550_CLK         18432000
# define CFG_NS16550_COM1        0x40000000
# define CFG_NS16550_COM2        0x40000008
# define CFG_NS16550_COM3        0x40000010
# define CFG_NS16550_COM4        0x40000018
#endif

/* MMC */
#define MMC_MAX_DEVICE		0

/************************************************************
 * Activate LEDs while booting
 ************************************************************/
#define CONFIG_STATUS_LED
#define	CONFIG_SHOW_BOOT_PROGRESS 1	/* Show boot progress on LEDs	*/

#define CONFIG_DRIVER_NS9750_ETHERNET 1	/* use on-chip ethernet */

#define CONFIG_ZERO_BOOTDELAY_CHECK	/* check for keypress on bootdelay==0 */

#ifdef CONFIG_CMD_USB
/* USB related stuff */
# define LITTLEENDIAN		1	/* necessary for USB */
# define CONFIG_USB_OHCI 	1
# define CONFIG_USB_STORAGE 	1
# define CONFIG_DOS_PARTITION 	1
#endif

#define CONFIG_SILENT_RESCUE	1

#ifdef CONFIG_UBOOT_CMD_BSP_COMPAT
/* U-Boot 1.1.3 standard settings so we now where to look */
# define CMD_BSP_COMPAT_ENV_IS_IN_EEPROM 1
# define CMD_BSP_COMPAT_ENV_OFFSET       0x500
# define CMD_BSP_COMPAT_ENV_SIZE         0x800
# define CFG_CMD_BSP_COMPAT		 CFG_CMD_EEPROM
#endif  /* CONFIG_UBOOT_CMD_BSP_COMPAT */

#ifndef CFG_CMD_BSP_COMPAT
# define CFG_CMD_BSP_COMPAT 0
#endif  /* CFG_CMD_BSP_COMPAT */

/* Video settings */
#ifdef CONFIG_DISPLAYS_SUPPORT
# define CONFIG_LCD			1
# if defined(CONFIG_UBOOT_CRT_VGA) || defined(CONFIG_UBOOT_LQ057Q3DC12I_TFT_LCD) || defined(CONFIG_UBOOT_LQ064V3DG01_TFT_LCD)
#  define LCD_BPP       LCD_COLOR16
# else
#  error "Please, define LCD_BPP according to your display needs"
# endif
#endif

#ifdef CONFIG_UBOOT_SPLASH
# define CONFIG_SPLASH_SCREEN		1
# define CONFIG_SPLASH_WITH_CONSOLE	1
#endif  /* CONFIG_UBOOT_SPLASH */

/***********************************************************
 * Command definition
 ***********************************************************/
#ifndef CONFIG_COMMANDS
#define CONFIG_COMMANDS 	\
	( CONFIG_COMMANDS_DIGI	| \
	CFG_CMD_BSP_COMPAT	|  \
	CFG_CMD_DATE		| \
	CFG_CMD_I2C		| \
	CFG_CMD_FAT		| \
	CFG_CMD_MII		| \
	CFG_CMD_NAND    	| \
	CFG_CMD_SNTP		|   /* simple network time protocol */ \
	CFG_CMD_USB     	| \
	0 )
#endif
/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

#if !defined(CONFIG_UBOOT_PROMPT)
# define CFG_PROMPT 		"CC9P9360 # "	/* Monitor Command Prompt	*/
#else
# define CFG_PROMPT		CONFIG_UBOOT_PROMPT_STR
#endif

#ifndef CONFIG_UBOOT_BOARDNAME
# define CONFIG_UBOOT_BOARDNAME_STR	"Development Board"
#endif

#define MODULE_STRING		"ConnectCore 9P 9360"

#define CFG_MEMTEST_START	0x00200000	/* memtest works on	*/
#define CFG_MEMTEST_END		0x00f80000	/* 13,5 MB in DRAM	*/

#undef  CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

#define	CFG_LOAD_ADDR		0x00200000	/* default load address	*/
#define CFG_WCE_LOAD_ADDR	0x002C0000
#define CFG_INITRD_LOAD_ADDR    0x00600000      /* default initrd  load address */
#define CFG_NETOS_LOAD_ADDR	0x00500000

#define	CFG_HZ			1000

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1    		/* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		0x00000000	 /* SDRAM Bank #1 */
/* size will be defined in include/configs when you make the config */

#define PHYS_NAND_FLASH		0x50000000 /* NAND Flash Bank #1 */
#define CFG_NAND_BASE		PHYS_NAND_FLASH

#define CFG_NAND_BASE_LIST	{ CFG_NAND_BASE }

#define CFG_NAND_UNALIGNED	1
#define NAND_ECC_INFO		CFG_LOAD_ADDR	/* address in SDRAM is used */

#define NAND_BUSY_GPIO		49

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */

#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks */
#define PHYS_FLASH_1		0x50000000 /* Flash Bank #1 */
#define CFG_FLASH_BASE		PHYS_FLASH_1
#define CFG_MAX_FLASH_SECT	(71)	/* max number of sectors on one chip */


/* timeout values are in ticks */
#define CFG_FLASH_ERASE_TOUT	(5*CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(5*CFG_HZ) /* Timeout for Flash Write */

/* use external RTC */
#define	CONFIG_RTC_DS1337	1
#define CFG_I2C_RTC_ADDR	0x68	/* Dallas DS1337 RTC address */

/* i2c bus */
#define CONFIG_HARD_I2C		1	/* I2C with hardware support */
#define CONFIG_DRIVER_NS9750_I2C 1	/* I2C driver */
#define CFG_I2C_SPEED 		100000	/* Hz */
#define CFG_I2C_SLAVE 		0x7F	/* I2C slave addr */
#define I2C_SCL_GPIO		70
#define I2C_SDA_GPIO		71
#define I2C_SCL_GPIO_FUNC	NS9750_GPIO_CFG_FUNC_2
#define I2C_SDA_GPIO_FUNC	NS9750_GPIO_CFG_FUNC_2

#define CFG_I2C_EEPROM_ADDR	0x50	/* EEPROM address */
#define CFG_I2C_EEPROM_ADDR_LEN	2	/* 2 address bytes */

#undef CFG_I2C_EEPROM_ADDR_OVERFLOW
#define CFG_EEPROM_PAGE_WRITE_BITS    5	/* 32 bytes page write mode on M24LC64 */
#define CFG_EEPROM_PAGE_WRITE_DELAY_MS 10
#define CFG_EEPROM_PAGE_WRITE_ENABLE   1

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
# include <ns9750_nand.h>

/* they are calculated for 128kB sectors. So Kernel/FPGA starts at 1MB */
# define PART_UBOOT_SIZE	0x000c0000
# define PART_NVRAM_SIZE        0x00080000
# define PART_KERNEL_SIZE       0x00300000
# define PART_ROOTFS_SIZE       0x01000000
# define PART_WINCE_REG_SIZE	0x00100000
# define PART_SPLASH_SIZE    	0x00100000
# define PART_WINCE_SIZE        0x01800000
# define PART_WINCE_FS_SIZE	0x0
# define PART_WINCE_FLASHFX_SIZE 0x00200000
# define PART_NETOS_KERNEL_SIZE 0x004C0000

# define CONFIG_JFFS2_DEV	 "nand0"
# define CONFIG_JFFS2_NAND
#endif /* if (CONFIG_COMMANDS & CFG_CMD_NAND) */

#ifdef CONFIG_IEEE1588
# define CFG_FPGA_SIZE		344771
#endif

#include <configs/digi_common_post.h>

#endif	/* __CONFIG_H */
