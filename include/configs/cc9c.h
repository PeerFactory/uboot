/*
 * Copyright (C) 2005 by FS Forth-Systeme GmbH.
 * All rights reserved.
 * Jonas Dietsche <jdietsche@fsforth.de>
 *
 * Configuation settings for the FS Forth-Systeme GmbH's A9M9750 Module
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 */

#ifndef __CONFIG_H
#define __CONFIG_H

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
#endif /* ifndef(CONFIG_DIGIEL_USERCONFIG) */

#define CONFIG_IDENT_STRING	" " VERSION_TAG "\nfor " MODULE_STRING " on " CONFIG_UBOOT_BOARDNAME_STR

#define CONFIG_GCC41		1

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM926EJS	1	/* This is an ARM926EJS Core	*/
#define CONFIG_NS9360		1	/* in an NetSilicon NS9360 SoC     */
#define CONFIG_CC9C		1
#define CONFIG_IS_NETSILICON    1
#define CPU			"NS9360"
#define CONFIG_MACH_CC9C		/* Select board mach-type */
#define BOARD_LATE_INIT		1	/* Enables initializations before jumping to main loop */
#define CRYSTAL 		294912
#define PLLFS			((*get_sys_reg_addr( NS9750_SYS_PLL ) >>0x17 ) & 0x3)
#define PLLND			((*get_sys_reg_addr( NS9750_SYS_PLL ) >>0x10 ) & 0x1f)

#define CONFIG_HAVE_BOOTMUX     1

/* CONFIG_CC9C_NAND will be defined in include/configs.h */
#ifdef CONFIG_CC9C_NAND
# define CONFIG_BOOT_NAND	1
# define CFG_NO_FLASH		1	/* No NOR-Flash available */
# define PLATFORM		"nand"
# define EBOOTFLADDR		"280000"
# define WCEFLADDR		"380000"
# define NAND_WAIT_READY( nand ) udelay(25) /* 2KiB flash have 25 us */
# ifndef CONFIG_DOWNLOAD_BY_DEBUGGER
#  define CONFIG_HAVE_SPI_LOADER  1       /* that initializes SDRAM and */
# endif  /* CONFIG_DOWNLOAD_BY_DEBUGGER */
#else
# define CFG_FLASH_CFI		/* The flash is CFI compatible	*/
# define CFG_FLASH_CFI_DRIVER	/* Use common CFI driver	*/
# define PLATFORM		"nor"
# define EBOOTFLADDR		"501C0000"
# define WCEFLADDR		"502C0000"
#endif /*CONFIG_CC9C_NAND*/
#define CONFIG_MODULE_NAME	"cc9cjs"PLATFORM
#define CONFIG_MODULE_NAME_WCE  "CCX9C"
#define CONFIG_MODULE_NAME_NETOS "connectcore9c_a"
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
# define CFG_CONSOLE            "ttyS"
#endif

#define CONFIG_CMDLINE_EDITING

# define USER_KEY1_GPIO		72
# define USER_KEY2_GPIO		69
# define USER_LED1_GPIO		48
# define USER_LED2_GPIO		49

/* MMC */
#define MMC_MAX_DEVICE		0

/************************************************************
 * Activate LEDs while booting
 ************************************************************/
#define CONFIG_STATUS_LED	1
#define	CONFIG_SHOW_BOOT_PROGRESS 1	/* Show boot progress on LEDs	*/

#define CFG_NS9750_UART		1	/* use on-chip UART */
#define CONFIG_DRIVER_NS9750_ETHERNET 1	/* use on-chip ethernet */

#ifdef CONFIG_CMD_USB
/* USB related stuff */
# define LITTLEENDIAN		1	/* necessary for USB */
# define CONFIG_USB_OHCI 	1
# define CONFIG_USB_STORAGE 	1
# define CONFIG_DOS_PARTITION 	1
#endif

#define CONFIG_SILENT_RESCUE	1

/* Video settings */
#ifdef CONFIG_DISPLAYS_SUPPORT
# define CONFIG_LCD			1
# if defined(CONFIG_UBOOT_CRT_VGA) || defined(CONFIG_UBOOT_LQ057Q3DC12I_TFT_LCD) || defined(CONFIG_UBOOT_LQ064V3DG01_TFT_LCD)
#  define LCD_BPP       LCD_COLOR16
# else
#  error "Please, define LCD_BPP accordingly to your display needs"
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
#ifdef CONFIG_CC9C_NAND
# define CONFIG_COMMANDS \
	( CONFIG_COMMANDS_DIGI	| \
	CFG_CMD_DATE		| \
	CFG_CMD_I2C		| \
	CFG_CMD_FAT		| \
	CFG_CMD_MII		| \
	CFG_CMD_NAND    	| \
	CFG_CMD_SNTP		|   /* simple network time protocol */ \
	CFG_CMD_USB     	| \
	0 )
#else
# define CONFIG_COMMANDS \
	( CONFIG_COMMANDS_DIGI	| \
	CFG_CMD_DATE		| \
	CFG_CMD_I2C		| \
	CFG_CMD_FAT		| \
	CFG_CMD_FLASH    	| \
	CFG_CMD_MII		| \
	CFG_CMD_SNTP		|   /* simple network time protocol */ \
	CFG_CMD_USB     	| \
	0 )
#endif	/* CONFIG_CC9C_NAND */
#endif
/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

#if !defined(CONFIG_UBOOT_PROMPT)
# define CFG_PROMPT 		"CC9C # "	/* Monitor Command Prompt	*/
#else
# define CFG_PROMPT		CONFIG_UBOOT_PROMPT_STR
#endif

#ifndef CONFIG_UBOOT_BOARDNAME
# define CONFIG_UBOOT_BOARDNAME_STR	"Development Board"
#endif

#define MODULE_STRING		"ConnectCore 9C"

#define CFG_MEMTEST_START	0x00200000	/* memtest works on	*/
#define CFG_MEMTEST_END		0x00f80000	/* 13,5 MB in DRAM	*/

#undef  CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

#define	CFG_LOAD_ADDR		0x00200000	/* default load address	*/
#define CFG_WCE_LOAD_ADDR	0x002C0000
#define CFG_INITRD_LOAD_ADDR    0x00600000      /* default initrd  load address */
#define CFG_NETOS_LOAD_ADDR	0x00300000

#define	CFG_HZ			1000

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1    		/* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		0x00000000	 /* SDRAM Bank #1 */
/* size will be defined in include/configs when you make the config */

#ifdef CONFIG_CC9C_NAND
# define PHYS_NAND_FLASH	0x50000000 /* NAND Flash Bank #1 */
# define CFG_NAND_BASE		PHYS_NAND_FLASH
# define CFG_NAND_BASE_LIST	{ CFG_NAND_BASE }
# define CFG_NAND_UNALIGNED	1
# define NAND_ECC_INFO		CFG_LOAD_ADDR	/* address in SDRAM is used */
# define NAND_NO_RB
#endif /*CONFIG_CC9C_NAND*/

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

/* use internal RTC */
#define	CONFIG_RTC_NS9360	1

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
# define PART_WINCE_FS_SIZE     0x0
# define PART_WINCE_FLASHFX_SIZE 0x00300000 /* Even when the FFX demo only
					       supports 2MB, some additional space is
					       required by FFX */

# define PART_NETOS_KERNEL_SIZE 0x002C0000
# define CONFIG_JFFS2_DEV	 "nand0"
# define CONFIG_JFFS2_NAND
#endif /* if (CONFIG_COMMANDS & CFG_CMD_NAND) */

#include <configs/digi_common_post.h>

#endif	/* __CONFIG_H */
