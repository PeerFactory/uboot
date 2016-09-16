/*
 * (C) Copyright 2005-2006
 * Seung-Chull, Suh <sc.suh@samsung.com>
 *
 * Configuation settings for the Digi CC9M2443 board.
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
#include "digi_common.h"
#include <s3c2443_gpio.h>

/************************************************************
 * Definition of default options when not configured by user
 ************************************************************/
#if (!defined(CONFIG_DIGIEL_USERCONFIG))
# define CONFIG_DISPLAYS_SUPPORT
# define CONFIG_UBOOT_SPLASH
# define CONFIG_UBOOT_CRT_VGA
# define CONFIG_UBOOT_LQ057Q3DC12I_TFT_LCD
# define CONFIG_UBOOT_LQ064V3DG01_TFT_LCD
# define CONFIG_AUTOLOAD_BOOTSCRIPT
#endif /* if !defined(CONFIG_DIGIEL_USERCONFIG) */

/* example recover from silent mode by gpio
#define CONFIG_SILENT_CONSOLE
#define ENABLE_CONSOLE_GPIO		S3C_GPF0
#define CONSOLE_ENABLE_GPIO_STATE 	0
*/

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define	CONFIG_S3C2443		1		/* in a SAMSUNG S3C2443 SoC     */
#define	CONFIG_S3C24XX		1		/* in a SAMSUNG S3C24XX Family  */
#define CONFIG_CC9M2443		1		/* on a SAMSUNG SMDK2443 Board  */
#define CONFIG_ARM920T				/* This is an ARM920T Core */

#define PLATFORM "js"
#define CONFIG_MODULE_NAME "cc9m2443"PLATFORM
#define BOARD_LATE_INIT

#define CONFIG_BOOT_NAND        1
#define CFG_NO_FLASH		1
#undef	CONFIG_USE_NOR_BOOT

#define CONFIG_MODULE_NAME_WCE	"CCX9M2443"

/* input clock of PLL */
#define CONFIG_SYS_CLK_FREQ	12000000	/* the SMDK2443 has 12MHz input clock */

#define	CFG_LOAD_ADDR		0x30200000	/* default load address	*/
#define CFG_WCE_LOAD_ADDR 	0x30200000
#define CFG_INITRD_LOAD_ADDR    0x30600000
#define CFG_NETOS_LOAD_ADDR     0x30500000
#define EBOOTFLADDR             "280000"
#define virt_to_phys(x)	(x)

#undef CONFIG_USE_IRQ				/* we don't need IRQ/FIQ stuff */

#define CONFIG_ZIMAGE_BOOT
#define CONFIG_IMAGE_BOOT

/* Power Management is enabled */
#define CONFIG_PM
/* Generic U-Boot value 1000 */
#define	CFG_HZ		1000

/*
 * select serial console configuration
 */

#define CONFIG_SILENT_RESCUE	1

#define CONFIG_CMDLINE_EDITING

/* I2C */
#define CONFIG_S3C24XX_I2C
#define CONFIG_HARD_I2C		1
#define	CFG_I2C_SPEED		100000
#define	CFG_I2C_SLAVE		0xFE

/* USB and USB device related stuff */
#if defined(CONFIG_CMD_USB)
 #define LITTLEENDIAN
 #define CONFIG_USB_OHCI
 #define CONFIG_USB_STORAGE
#endif

/* not supported at the moment
#define CONFIG_S3C_USBD
#define USBD_DOWN_ADDR		0x30000000
*/

#if defined(CONFIG_CMD_USB) || defined(CONFIG_CMD_MMC)
 #define CONFIG_DOS_PARTITION
 #define CONFIG_SUPPORT_VFAT
#endif

/* Video settings */
#ifdef CONFIG_DISPLAYS_SUPPORT
# define CONFIG_LCD			1
# if defined(CONFIG_UBOOT_CRT_VGA) || defined(CONFIG_UBOOT_LQ057Q3DC12I_TFT_LCD) || \
	defined(CONFIG_UBOOT_LQ064V3DG01_TFT_LCD)
#  define LCD_BPP       LCD_COLOR16
# else
#  error "Please, define LCD_BPP accordingly to your display needs"
# endif
#endif

#ifdef CONFIG_UBOOT_SPLASH
# define CONFIG_SPLASH_SCREEN		1
# define CONFIG_SPLASH_WITH_CONSOLE	1
#endif  /* CONFIG_UBOOT_SPLASH */


/************************************************************
 * Base board specific configurations
 ************************************************************/
#if !defined(CONFIG_CONS_INDEX)
# define CONFIG_CONS_INDEX	0 	/* 0 Port A; 2 Port B on 2410 dev 1 Port B on 2443JS */
#endif

#ifndef CFG_CONSOLE
# define CFG_CONSOLE            "ttySAC"
#endif

#define USER_KEY1_GPIO		S3C_GPF0
#define USER_KEY2_GPIO		S3C_GPF6
#define USER_LED1_GPIO		S3C_GPL10
#define USER_LED2_GPIO		S3C_GPL11

/************************************************************
 * RTC
 ************************************************************/
//#define CONFIG_RTC_S3C24XX	1 	/* internal RTC */
#define CONFIG_RTC_DS1337	1 	/* RTC on jupstartboard */
#define CFG_I2C_RTC_ADDR    	0x68

/***********************************************************
 * Command definition
 ***********************************************************/
#ifndef CONFIG_COMMANDS
#define CONFIG_COMMANDS \
	( CONFIG_COMMANDS_DIGI | \
	CFG_CMD_DATE	| \
	CFG_CMD_I2C	| \
	CFG_CMD_FAT	| \
	CFG_CMD_MMC	| \
	CFG_CMD_NAND	| \
	CFG_CMD_USB     | \
	CFG_CMD_SNTP	|  /* simple network time protocol */ \
	0 )
#endif
/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

/************************************************************
 * Default environment settings
 ************************************************************/
#define CONFIG_EXTRA_ENV_SETTINGS			\
	"silent="CFG_SET_SILENT"\0"

/* ethernet */
#define CONFIG_DRIVER_SMSC9118
#define CONFIG_SMSC9118_BASE (0x29000000)

#define CONFIG_ETHADDR		00:40:5c:26:0a:5b
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_IPADDR		192.168.42.30
#define CONFIG_SERVERIP		192.168.42.1
#define CONFIG_GATEWAYIP	192.168.0.1

#define CONFIG_ZERO_BOOTDELAY_CHECK

/* MMC */
#if defined(CONFIG_CMD_MMC)
# define CONFIG_MMC		1
# define CONFIG_SUPPORT_MMC_PLUS
#endif
#define CFG_MMC_BASE		0xf0000000
#define MMC_MAX_DEVICE		1
#define HSMMC_MAX_DEVICE	1

/*
 * CC9M2443 default environment settings
 */

/*
 * Miscellaneous configurable options
 */
#if !defined(CONFIG_UBOOT_PROMPT)
#define	CFG_PROMPT		"CC9M2443 # "	/* Monitor Command Prompt	*/
#else
# define CFG_PROMPT             CONFIG_UBOOT_PROMPT_STR
#endif

#define CFG_MEMTEST_START	0x30200000	/* memtest works on	*/
#define CFG_MEMTEST_END		0x30400000	/* 60 MB in DRAM	*/

#undef  CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CFG_BANK_CFG_VAL_128            0x0006D25D
#define	CFG_BANK_CFG_VAL_64		0x0004925D
#define	CFG_BANK_CFG_VAL_32             0x0004895D

#define	CFG_BANK_CON1_VAL		0x44000040

#define	CFG_BANK_CON2_VAL_100		0x007b003f
#define	CFG_BANK_CON2_VAL_133		0x009e003f

#define	CFG_BANK_CON3_VAL		0x80000030

#define	CFG_BANK_REFRESH_VAL_100_128	0x00000186
#define	CFG_BANK_REFRESH_VAL_100_64	0x00000186
#define	CFG_BANK_REFRESH_VAL_100_32	0x00000186
#define	CFG_BANK_REFRESH_VAL_133_128	0x00000200
#define	CFG_BANK_REFRESH_VAL_133_64	0x00000200
#define	CFG_BANK_REFRESH_VAL_133_32	0x00000200

#define	CFG_BANK_TIMEOUT_VAL_133_128	CFG_BANK_REFRESH_VAL_133_128
#define	CFG_BANK_TIMEOUT_VAL_133_64	CFG_BANK_REFRESH_VAL_133_64
#define	CFG_BANK_TIMEOUT_VAL_133_32	CFG_BANK_REFRESH_VAL_133_32

#define CONFIG_NR_DRAM_BANKS		2	   /* we have 2 bank of DRAM */
#define PHYS_SDRAM_1			0x30000000 /* SDRAM Bank #1 */
#define PHYS_SDRAM_2			0x38000000 /* SDRAM Bank #2 */
#define CFG_FLASH_BASE			0x08000000

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks */
#define CFG_MAX_FLASH_SECT	1024
#define CONFIG_AMD_LV800
#define PHYS_FLASH_SIZE		0x100000

/* timeout values are in ticks */
#define CFG_FLASH_ERASE_TOUT	(5*CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(5*CFG_HZ) /* Timeout for Flash Write */


#define CFG_ENV_ADDR		0

/*
 * CC9M2443 board specific data
 */
#ifndef CONFIG_UBOOT_BOARDNAME
#define CONFIG_UBOOT_BOARDNAME_STR	"Development Board"
#endif
#define MODULE_STRING           	"ConnectCore 9M 2443"
#define CONFIG_IDENT_STRING		" " VERSION_TAG "\nfor " MODULE_STRING " on " \
       					CONFIG_UBOOT_BOARDNAME_STR

#define CFG_UBOOT_SIZE  	(768*1024)
/* base address for u-boot */
#define CFG_UBOOT_BASE		0x30100000
#define CFG_PHY_UBOOT_BASE	0x30100000

/* NAND configuration */
#define S3C_NAND_CFG_HWECC
#define CFG_MAX_NAND_DEVICE	1
#define CFG_NAND_BASE		(0x4e000010)
#define NAND_MAX_CHIPS		1

#define NAND_DISABLE_CE(nand)		(NFCONT_REG |= (1 << 1))
#define NAND_ENABLE_CE(nand)		(NFCONT_REG &= ~(1 << 1))
#define NF_TRANSRnB()		do { while(!(NFSTAT_REG & (1 << 0))); } while(0)

#define CFG_NAND_SKIP_BAD_DOT_I		1  /* ".i" read skips bad blocks   */
#define	CFG_NAND_WP			1
#define CFG_NAND_YAFFS_WRITE		1  /* support yaffs write */
#define CFG_ENV_OFFSET 0x000C0000

#define CONFIG_JFFS2_NAND	1	/* jffs2 on nand support */
#define NAND_CACHE_PAGES	16	/* size of nand cache in 512 bytes pages */
/*
 * JFFS2 partitions
 */
#undef CONFIG_JFFS2_CMDLINE
#define CONFIG_JFFS2_DEV		"nand0"
#define CONFIG_JFFS2_PART_SIZE		0xFFFFFFFF
#define CONFIG_JFFS2_PART_OFFSET	0x00200000

#define PART_UBOOT_SIZE	       	0x000c0000
#define PART_NVRAM_SIZE        	0x00040000	/* nvram 256 kb because 2k pagesize nands */
#define PART_KERNEL_SIZE       	0x00300000
#define PART_ROOTFS_SIZE       	0x01000000
#define PART_WINCE_REG_SIZE	0x00100000
#define PART_SPLASH_SIZE       	0x00100000
#define PART_WINCE_SIZE        	0x01800000
#define PART_WINCE_FS_SIZE    	0x0
#define PART_NETOS_KERNEL_SIZE 	0x002C0000
#endif	/* __CONFIG_H */
