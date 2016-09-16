/*
 *  include/configs/digi_common.h
 *
 *  Copyright (C) 2006 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision: 1.4 $:
 *  !Author:     Markus Pietrek
 *  !Descr:      Defines all definitions that are common to all DIGI platforms
*/

#ifndef __DIGI_COMMON_H
#define __DIGI_COMMON_H

/* global helper stuff */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/* may undefine settings */

#include <digi_version.h>
#include <configs/parse_user_definitions.h>

/* stuff that may be undefined in userconfig.h */

/* ********** user key configuration ********** */

#ifndef CONFIG_UBOOT_DISABLE_USER_KEYS
# define CONFIG_USER_KEY
#endif

/* ********** console configuration ********** */

#ifdef CONFIG_SILENT_CONSOLE
# define CFG_SET_SILENT		"yes"
#else
# define CFG_SET_SILENT		"no"
#endif

/* ********** System Initialization ********** */
#ifdef CONFIG_DOWNLOAD_BY_DEBUGGER
# define CONFIG_SKIP_RELOCATE_UBOOT
#endif /* CONFIG_DOWNLOAD_BY_DEBUGGER */

/*
 * If we are developing, we might want to start armboot from ram
 * so we MUST NOT initialize critical regs like mem-timing ...
 */
/* define for developing */
#undef	CONFIG_SKIP_LOWLEVEL_INIT

/* ********** Booting ********** */

#ifndef CONFIG_BOOTDELAY
# define CONFIG_BOOTDELAY		4
#endif  /* CONFIG_BOOTDELAY */

/* ********** Rootfs *********** */
/* Delay before trying to mount the rootfs from a media */
#define ROOTFS_DELAY		10

#ifndef CONFIG_ZERO_BOOTDELAY_CHECK
# define CONFIG_ZERO_BOOTDELAY_CHECK	/* check for keypress on bootdelay==0 */
#endif  /* CONFIG_ZERO_BOOTDELAY_CHECK */

/* ********** Commands supported ********** */

#define CONFIG_COMMANDS_DIGI (  \
	( CONFIG_CMD_DFL & ~CFG_CMD_FLASH) | \
	CFG_CMD_BSP       |  /* DIGI commands like dboot and update */ \
	CFG_CMD_CACHE     |  /* icache, dcache */ \
	CFG_CMD_DHCP      | \
	CFG_CMD_PING      | \
	CFG_CMD_REGINFO   | \
	CFG_CMD_AUTOSCRIPT | \
	0 )

#define	CONFIG_DIGI_CMD	1		/* enables DIGI board specific commands */

/* ********** serial configuration ********** */
#define CONFIG_BAUDRATE		38400

/* ********** network ********** */
#ifndef CONFIG_TFTP_RETRIES_ON_ERROR
# define CONFIG_TFTP_RETRIES_ON_ERROR	5
#endif

#define CONFIG_CMDLINE_TAG	1 /* passing of ATAGs */
#define CONFIG_INITRD_TAG	1
#define CONFIG_SETUP_MEMORY_TAGS 1

/* ********** usb/mmc ********** */
#define DEFAULT_KERNEL_FS		"fat"
#define DEFAULT_KERNEL_DEVPART		"0:1"
#define DEFAULT_ROOTFS_MMC_PART		"/dev/mmcblk0p2"
#define DEFAULT_ROOTFS_USB_PART		"/dev/sda2"

/* ********** memory sizes ********** */
#define SPI_LOADER_SIZE		8192

/* NVRAM */

#define CFG_ENV_IS_IN_DIGI_NVRAM
#define CFG_ENV_SIZE		0x00002000 /* some space for U-Boot */


/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
/*
 * Size of malloc() pool
 */
#define CFG_MALLOC_LEN		(768*1024)  /*  we need 2 erase blocks with at
                                             *  max 128kB for NVRAM compares */
#define CFG_GBL_DATA_SIZE       256     /* size in bytes reserved for initial data */
#define CONFIG_SYS_MALLOC_LEN    CFG_MALLOC_LEN
#define CONFIG_SYS_GBL_DATA_SIZE CFG_GBL_DATA_SIZE

/* ********** misc stuff ********** */

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERRIDE
#define CONFIG_ENV_OVERWRITE

#define	CFG_LONGHELP            /* undef to save memory */

#define	CFG_CBSIZE		2048		/* Console I/O Buffer Size	*/
#define	CFG_PBSIZE 		(CFG_CBSIZE+sizeof(CFG_PROMPT)+16) /* Print Buffer Size */
#define	CFG_MAXARGS		16		/* max number of command args	*/
#define	CFG_BARGSIZE		CFG_CBSIZE	/* Boot Argument Buffer Size	*/

/* valid baudrates */
#define CFG_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

/* In: serial, Out: serial etc. */
#define CFG_CONSOLE_INFO_QUIET

/* stuff for DVTs and special information */

#define CONFIG_DVT_PROVIDED
#define CONFIG_MTD_DEBUG
#define CONFIG_MTD_DEBUG_VERBOSE 1

/* compilation */

#define CFG_64BIT_VSPRINTF      /* we need if for NVRAM */
#define CFG_64BIT_STRTOUL       /* we need if for NVRAM */

/************************************************************
 * Default environment settings
 ************************************************************/
#define CONFIG_EXTRA_ENV_SETTINGS			\
	"silent="CFG_SET_SILENT"\0"

#endif /* __DIGI_COMMON_H */
