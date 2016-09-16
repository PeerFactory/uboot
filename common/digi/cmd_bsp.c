/*
 *  /targets/U-Boot.cvs/common/digi/cmd_bsp.c
 *
 *  Copyright (C) 2006 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

/*
 *  !Revision:   $Revision$
 *  !Author:     Markus Pietrek
 *  !References: [1] http://www.linux-mtd.infradead.org/doc/nand.html
*/

/*
 * Digi CC specific functions
 */

#include <common.h>
#include <asm/io.h>
#if (CONFIG_COMMANDS & CFG_CMD_BSP)
#include <command.h>
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
# include <nand.h>
#endif

#include <jffs2/jffs2.h>
#include <net.h>                /* DHCP */
#include <dvt.h>                /* DVTHadError */
#if defined(CONFIG_CC9M2443) || defined(CONFIG_CCW9M2443)
 #include <asm/arch/gpio.h>
#endif
#if defined(CONFIG_NS9360)
 #include <usb.h>	/* usb_stop */
#endif
#include "cmd_bsp.h"
#include "env.h"
#include "fpga_checkbitstream.h"
#include "nvram.h"
#include "partition.h"          /* MtdGetEraseSize */
#include "mtd.h"
#include "helper.h"
/* ------------*/
#include <configs/digi_common_post.h>
/* ----------- */
#define SNFS	"snfs"
#define SMTD	"smtd"
#define NPATH	"npath"
#define RIMG    "rimg"
#define USRIMG  "usrimg"
#define CONSOLE "console"

/* Constants for bootscript download timeouts */
#define AUTOSCRIPT_TFTP_MSEC	100
#define AUTOSCRIPT_TFTP_CNT	15
#define AUTOSCRIPT_START_AGAIN	100

/* to transform a value into a string, from environment.c */

#define CE( sCmd ) \
        do { \
                if( !(sCmd) )                   \
                        goto error; \
        } while( 0 )

#ifdef CFG_CONS_INDEX_SUB_1
# define CONSOLE_INDEX ((CONFIG_CONS_INDEX)-1)
#else
# define CONSOLE_INDEX (CONFIG_CONS_INDEX)
#endif

#if defined( CFG_APPEND_CRC32 )
# define UBOOT_IMG_HAS_CRC32 1
#else
# define UBOOT_IMG_HAS_CRC32 0
#endif

#ifdef CFG_NETOS_SWAP_ENDIAN
# define NETOS_SWAP_ENDIAN 1
#else
# define NETOS_SWAP_ENDIAN 0
#endif

DECLARE_GLOBAL_DATA_PTR;

typedef enum {
        IS_TFTP,
        IS_NFS,
        IS_FLASH,
        IS_USB,
        IS_MMC,
        IS_HSMMC,
} image_source_e;

/* ********** local typedefs ********** */

typedef struct {
        nv_os_type_e   eOSType;
        const char*    szName;       /*! short OS name */
        const char*    szEnvVar;
        nv_part_type_e ePartType;
        char           bForBoot;
        /* a compressed image can't be used for booting from TFTP/USB/NFS
         * because we can't decompress it on the fly */
        char           bForBootFromFlashOnly;
        char           bRootFS;
        char           bCRC32Appended;
        char           bSwapEndian;
} part_t;

typedef struct {
        const char*    szEnvVar;
        const char*    szEnvDflt;
} env_default_t;

typedef struct {
        image_source_e eType;
        const char*    szName;       /*! short image name */
} image_source_t;

/* ********** local functions ********** */

static int do_digi_dboot( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] );
static int do_digi_update( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] );
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
static int jffs2_mark_clean (long long offset, long long size);
#endif

static int WhatPart(
        const char* szPart, char bForBoot,
        const part_t** ppPart,
        const nv_param_part_t** ppPartEntry,
        char bPartEntryRequired );
static const image_source_t* WhatImageSource( const char* szSrc );
static int RunCmd( const char* szCmd );
static int GetIntFromEnvVar( /*@out@*/ int* piVal, const char* szVar,
                             char bSilent );
static int AppendPadding( const nv_param_part_t* pPart, void* pvImage,
                          /*@out@*/ int* piFileSize );
static size_t GetEraseSize( const nv_param_part_t* pPart );
static size_t GetPageSize( const nv_param_part_t* pPart );
static const nv_param_part_t* FindPartition( const char* szName );
static int GetDHCPEnabled( char* pcEnabled );
static int DoDHCP( void );
static int IsValidUImage( image_header_t* pHeader );
static int GetUImageSize( const nv_param_part_t* pPart,
                          size_t* piSize, char* pbCompressed );
int findpart_tableentry(  const part_t **ppPart,
	const nv_param_part_t *pPartEntry,
        int            iCount );
void setup_before_os_jump(nv_os_type_e eOSType, image_source_e eType);

extern image_header_t header;
extern int NetSilent;		/* Whether to silence the net commands output */
extern ulong TftpRRQTimeoutMSecs;
extern int TftpRRQTimeoutCountMax;
extern unsigned long NetStartAgainTimeout;

/* ********** local variables ********** */

static const env_default_t l_axEnvDynamic[] = {
	{ CONSOLE,       NULL                         	   },  /* auto-generated */
#ifdef CONFIG_MODULE_NAME_WCE
	{ "ebootaddr",   MK_STR( CFG_LOAD_ADDR )           },
        { "eimg",        "eboot-"CONFIG_MODULE_NAME_WCE    },
#endif
#ifdef CFG_FPGA_SIZE
        { "fimg",        "wifi.biu"                        },
#endif
	{ "ip",          "ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:" CONFIG_MODULE_NAME ":eth0:off" },
	{ "loadaddr",    MK_STR( CFG_LOAD_ADDR )           },
	{ "loadaddr_initrd",    MK_STR( CFG_INITRD_LOAD_ADDR )           },
        { "kimg",        CONFIG_LINUX_IMAGE_NAME           },
        { NPATH,         "/exports/nfsroot-"CONFIG_MODULE_NAME },
	{ "linuxloadaddr", MK_STR( CFG_LOAD_ADDR )           },
#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
	{ "loadbootsc",	"yes" },
	{ "bootscript", CONFIG_MODULE_NAME"-bootscript" },
#endif
#if (CONFIG_COMMANDS & CFG_CMD_MMC)
	{ "mmc_rpart",	DEFAULT_ROOTFS_MMC_PART		},
#endif
	{ "netosloadaddr", MK_STR( CFG_NETOS_LOAD_ADDR )   },
#ifdef CONFIG_IS_NETSILICON
        /* NET+OS exists only for these platforms */
        { "nimg",        "image-"CONFIG_MODULE_NAME_NETOS".bin"},
# ifdef PART_NETOS_LOADER_SIZE
        { "nloader",     "rom-"CONFIG_MODULE_NAME_NETOS".bin"},
# endif
#endif  /* CONFIG_IS_NETSILICON */
        { RIMG,          NULL                              }, /* auto-generated */
#if (CONFIG_COMMANDS & CFG_CMD_USB) || (CONFIG_COMMANDS & CFG_CMD_MMC)
	{ "rootdelay",	MK_STR(ROOTFS_DELAY)		},
#endif
	{ SMTD,          ""                                },
	{ SNFS,          "root=nfs nfsroot=${serverip}:"   },
#ifdef CONFIG_UBOOT_SPLASH
        { "simg",        "splash.bmp"                      },
#endif
        { "std_bootarg", ""                                },
        { "uimg",        CONFIG_UBOOT_IMAGE_NAME           },
        { USRIMG,        NULL                              }, /* auto-generated */
#if (CONFIG_COMMANDS & CFG_CMD_USB)
	{ "usb_rpart",	DEFAULT_ROOTFS_USB_PART		},
#endif
#ifdef VIDEO_DISPLAY
        { "video",       VIDEO_DISPLAY                     },
#endif
#ifdef CONFIG_MODULE_NAME_WCE
	{ "wceloadaddr",   MK_STR( CFG_WCE_LOAD_ADDR )     },
        { "wimg",        "wce-"CONFIG_MODULE_NAME_WCE      },
        { "wzimg",       "wcez-"CONFIG_MODULE_NAME_WCE     },
#endif
};

static const part_t l_axPart[] = {
        { NVOS_LINUX, "linux",  "kimg",   NVPT_LINUX,      .bForBoot = 1 },
#ifdef CONFIG_MODULE_NAME_WCE
        { NVOS_EBOOT, "eboot",  "eimg",   NVPT_EBOOT,      .bForBoot = 1 },
        { NVOS_WINCE, "wce",    "wimg",   NVPT_WINCE,      .bForBoot = 1 },
        { NVOS_WINCE, "wcez",   "wzimg",  NVPT_WINCE,      .bCRC32Appended = 1 },
#endif

#ifdef CONFIG_IS_NETSILICON
# ifdef PART_NETOS_LOADER_SIZE
        { NVOS_NETOS_LOADER, "netos_loader", "nloader", NVPT_NETOS_LOADER,
          .bSwapEndian = NETOS_SWAP_ENDIAN },
# endif
        { NVOS_NETOS, "netos",  "nimg",   NVPT_NETOS,
          .bSwapEndian = NETOS_SWAP_ENDIAN, .bForBoot = 1 },
#endif  /* CONFIG_IS_NETSILICON */

        { NVOS_UBOOT, "uboot",  "uimg",   NVPT_UBOOT,
          .bCRC32Appended = UBOOT_IMG_HAS_CRC32                          },
#ifdef CFG_FPGA_SIZE
        { NVOS_NONE,  "fpga",   "fimg",   NVPT_FPGA,                     },
#endif
#ifdef CONFIG_UBOOT_SPLASH
	{ NVOS_NONE,  "splash",  "simg",  NVPT_SPLASH_SCREEN,            },
#endif
        { NVOS_NONE,  "rootfs", RIMG,     NVPT_FILESYSTEM, .bRootFS = 1  },
        { NVOS_NONE,  "userfs", USRIMG,   NVPT_FILESYSTEM,               },
};

/* use MK because we later use l_axImgSrc[ IS_FLASH ] */
#define MK(x,y) [x] = { .eType = x, .szName = y }
static const image_source_t l_axImgSrc[] = {
        MK( IS_TFTP,  "tftp"  ),
        MK( IS_NFS,   "nfs"   ),
        MK( IS_FLASH, "flash" ),
        MK( IS_USB,   "usb"   ),
        MK( IS_MMC,   "mmc"   ),
        MK( IS_HSMMC, "hsmmc" ),
};
#undef MK

/* struct for wce bootargs */
const static struct {
	char* arg;
	int   val;
} wce_args[] = {
	{ "kitl_ttyS0",		0x01000000 },
	{ "kitl_ttyS1",		0x02000000 },
	{ "kitl_ttyUSB",	0x03000000 },
	{ "kitl_ethUSB",	0x04000000 },
	{ "kitl_eth",		0x05000000 },
	{ "cleanhive",		0x00010000 },
	{ "cleanboot",		0x00000100 },
	{ "formatpart",		0x00000001 },
};

/* ********** local functions ********** */

static size_t GetEraseSize( const nv_param_part_t* pPart )
{
        if( NULL == pPart )
                /* no specific sector, take the first one which might be wrong
                 * for NOR */
                return MtdGetEraseSize( 0, 0 );
        else
                return MtdGetEraseSize( pPart->uiChip, pPart->ullStart );
}

static size_t GetPageSize( const nv_param_part_t* pPart )
{
        size_t iPageSize;

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
        iPageSize = nand_info[ 0 ].writesize;
#else
        /* NOR doesn't know anthing about pages */
        iPageSize = GetEraseSize( pPart );
#endif

        return iPageSize;
}

static int SetBootargs( int argc, char* args[] )
{
	int i, index = 0;
	/* default clean boot */
	int commands = 0x00000100;
	bd_t *bdinfo;

	if(argc > 3) {
		for( i=3; i < argc; i++ )
			for( index = 0; index < ARRAY_SIZE( wce_args); index++ )
				if( !strcmp( args[i], wce_args[index].arg ) )
					commands |= wce_args[index].val;
	}

	bdinfo = (bd_t *) gd->bd->bi_boot_params;
	bdinfo->bi_boot_params = commands;

	return 0;
}

static int do_digi_dboot( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] )
{
        int iLoadAddr    = -1;
        char bAppendMtdRootToBootargs = 1;
        const part_t* pPart = NULL;
        const char*   szTmp           = NULL;
        const image_source_t* pImgSrc = NULL;
        const nv_param_part_t* pPartEntry = NULL;
        char bCanDetectImageSize = 0;
	char szCmd[ 80 ]  = "";
	char szImg[ 60 ]  = "";
        char bIsNFSRoot   = 0;
        char bDHCPEnabled = 0;
	int iLoadAddrInitRD = 0;
        nv_os_type_e eOSType = NVOS_NONE;

        if( ( argc < 2 ) || ( argc > 6 ) )
                goto usage;

        clear_ctrlc();

        /* check what to boot */

        if( argc > 2 )
                pImgSrc = ( ( NVOS_EBOOT != pPart->eOSType ) ?
                            WhatImageSource( argv[ 2 ] ) :
                            &l_axImgSrc[ IS_FLASH ] );
        else
                pImgSrc = &l_axImgSrc[ IS_TFTP ];

        if( NULL == pImgSrc )
                goto usage;

        /* determine OS and/or partition */
        CE( WhatPart( argv[ 1 ], 1, &pPart, &pPartEntry, 0 ) );
        if( NULL != pPartEntry ) {
                switch( pPartEntry->eType ) {
                    case NVPT_LINUX:
                        eOSType = NVOS_LINUX;
                        bCanDetectImageSize = 1;
                        break;
                    case NVPT_WINCE:
                        eOSType = NVOS_WINCE;
                        bCanDetectImageSize = 1;
                        break;
                    case NVPT_EBOOT: eOSType = NVOS_EBOOT; break;
                    case NVPT_NETOS:
                        eOSType = NVOS_NETOS;
                        bCanDetectImageSize = 1;
                        break;
                    default: break;    /* to avoid compiler warnings */
                }
        } else if( NULL != pPart )
                /* no partition available, e.g. dboot linux tftp */
                eOSType = pPart->eOSType;

        if( NVOS_NONE == eOSType ) {
                eprintf( "OS Type not detected for %s\n", argv[ 1 ] );
                goto error;
        }

        /* determine file to boot */
        if (argc == 4)
		szTmp = argv[3];
	else if (argc == 6)
		szTmp = argv[5];
        else if( NULL != pPart ) {
                /* not present, but we have a partition definition */
                szTmp = GetEnvVar( pPart->szEnvVar, 0 );
                CE( NULL != szTmp );
        } else {
                eprintf( "Require filename\n" );
                goto error;
        }
        strncat( szImg, szTmp, sizeof( szImg ) );

        CE( GetDHCPEnabled( &bDHCPEnabled ) );

        /* user input processed, determine addresses */
        switch( eOSType ) {
            case NVOS_LINUX:
                CE( GetIntFromEnvVar( &iLoadAddr, "linuxloadaddr", 0 ) );
                break;
            case NVOS_WINCE:
                CE( GetIntFromEnvVar( &iLoadAddr, "wceloadaddr", 0 ) );
                break;
            case NVOS_EBOOT:
                CE( GetIntFromEnvVar( &iLoadAddr, "ebootaddr", 0 ) );
                break;
            case NVOS_NETOS:
                CE( GetIntFromEnvVar( &iLoadAddr, "netosloadaddr", 0 ) );
                break;
            default:
                (void) GetIntFromEnvVar( &iLoadAddr, "loadaddr", 1 );
                eprintf( "Operating system not supported\n" );
        }

        if( -1 == iLoadAddr ) {
                eprintf( "variable loadaddr does not exist\n" );
                goto error;
        }

	if( bDHCPEnabled &&
	    (( IS_TFTP == pImgSrc->eType ) || ( IS_NFS == pImgSrc->eType )) )
		/* makes no sense for USB or FLASH download to need DHCP */
		CE( DoDHCP() );

        /* run boot scripts and get images into RAM */
        switch( pImgSrc->eType ) {
            case IS_TFTP:
                bAppendMtdRootToBootargs = 0;  /* everything is loaded by network */
                sprintf( szCmd, "tftp 0x%x %s", iLoadAddr, szImg );
                CE( RunCmd( szCmd ) );
                bIsNFSRoot = 1;
                break;
            case IS_NFS:
                bAppendMtdRootToBootargs = 0;  /* everything is loaded by network */
                sprintf( szCmd, "nfs 0x%x %s/%s",
                         iLoadAddr, GetEnvVar( NPATH, 0 ), szImg );
                CE( RunCmd( szCmd ) );
                bIsNFSRoot = 1;
                break;
            case IS_USB:
	    {
		char kdevpart[8];
		char kfs[8];

		bAppendMtdRootToBootargs = 0;  /* everything is loaded from media */

		/* Device number can be given as argument
		 * Otherwise, default value is used */
		if (argc > 4)
			strcpy(kdevpart, argv[3]);
		else
			strcpy(kdevpart, DEFAULT_KERNEL_DEVPART);

		/* File system can be given as argument
		* Otherwise, default value is used */
		if (argc > 4)
			strcpy(kfs, argv[4]);
		else
			strcpy(kfs, DEFAULT_KERNEL_FS);

		if (!strcmp(kfs, "fat") || !strcmp(kfs, "vfat"))
			sprintf(szCmd, "fatload usb %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else if (!strcmp(kfs, "ext2") || !strcmp(kfs, "ext3"))
			sprintf(szCmd, "ext2load usb %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else {
			printf("error: invalid value for filesystem (must be 'fat', 'vfat', 'ext2', 'ext3'\n");
			goto error;
		}

		CE(RunCmd("usb reset"));
		CE(RunCmd(szCmd));
	        break;
	    }
	    case IS_MMC:
	    {
		char kdevpart[8];
		char kfs[8];

                bAppendMtdRootToBootargs = 0;  /* everything is loaded from media */

		/* Device number can be given as argument
		 * Otherwise, default value is used */
		if (argc > 4)
			strcpy(kdevpart, argv[3]);
		else
			strcpy(kdevpart, DEFAULT_KERNEL_DEVPART);

		/* File system can be given as argument
		* Otherwise, default value is used */
		if (argc > 4)
			strcpy(kfs, argv[4]);
		else
			strcpy(kfs, DEFAULT_KERNEL_FS);

		if (!strcmp(kfs, "fat") || !strcmp(kfs, "vfat"))
			sprintf(szCmd, "fatload mmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else if (!strcmp(kfs, "ext2") || !strcmp(kfs, "ext3"))
			sprintf(szCmd, "ext2load mmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else {
			printf("error: invalid value for filesystem (must be 'fat', 'vfat', 'ext2', 'ext3'\n");
			goto error;
		}

		CE(RunCmd("mmcinit"));
		CE(RunCmd(szCmd));
	        break;
	    }
	    case IS_HSMMC:
	    {
		char kdevpart[8];
		char kfs[8];

                bAppendMtdRootToBootargs = 0;  /* everything is loaded from media */

		/* Device number can be given as argument
		 * Otherwise, default value is used */
		if (argc > 4)
			strcpy(kdevpart, argv[3]);
		else
			strcpy(kdevpart, DEFAULT_KERNEL_DEVPART);

		/* File system can be given as argument
		* Otherwise, default value is used */
		if (argc > 4)
			strcpy(kfs, argv[4]);
		else
			strcpy(kfs, DEFAULT_KERNEL_FS);

		if (!strcmp(kfs, "fat") || !strcmp(kfs, "vfat"))
			sprintf(szCmd, "fatload hsmmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else if (!strcmp(kfs, "ext2") || !strcmp(kfs, "ext3"))
			sprintf(szCmd, "ext2load hsmmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else {
			printf("error: invalid value for filesystem (must be 'fat', 'vfat', 'ext2', 'ext3'\n");
			goto error;
		}

		CE(RunCmd("hsmmcinit"));
		CE(RunCmd(szCmd));
	        break;
	    }

            case IS_FLASH:
            {
                    size_t iSize = 0;
                    char   bCompressed = 0;

                    if( NULL == pPartEntry ) {
                            eprintf( "No partition to boot from\n" );
                            goto error;
                    }

                    if( bCanDetectImageSize )
                            CE( GetUImageSize( pPartEntry, &iSize, &bCompressed ) );

                    /* copy kernel partition */
                    if( bCompressed ) {
                            iLoadAddr = ntohl( header.ih_load );
                            CE( PartReadAndDecompress( pPartEntry,
                                                       (void*) iLoadAddr,
                                                       iSize ) );
                    } else
                            CE( PartRead( pPartEntry, (void*) iLoadAddr, iSize, 0 ) );

                    /* !TODO. Should only be done for linux
                     * !TODO. Should also be able to use uncompression */
                    if( NvParamPartFind( &pPartEntry, NVPT_FILESYSTEM,
                                         NVFS_INITRD, 1, 0 ) ) {
                            /* we have a bootable initrd, copy it, too */
                            if( GetIntFromEnvVar( &iLoadAddrInitRD,
                                                  "loadaddr_initrd", 0 ) )
                                    CE( PartRead( pPartEntry, (void*) iLoadAddrInitRD, 0, 0 ) );
                    }

                    break;
            }
        } /* switch( pImgSrc->eType ) */

        CE( !DVTError() );

	/* Platform setup before jumping to the OS */
	setup_before_os_jump(eOSType, pImgSrc->eType);

        /* boot operating system */
        switch( eOSType ) {
            case NVOS_LINUX:
            {
                    char szBootargs[ 2048 ];
                    strcpy( szBootargs, "setenv bootargs " );
                     /* no snprintf, therefore strncat */
                    strncat( szBootargs, GetEnvVar( "std_bootarg", 0 ), sizeof( szBootargs ) );

                    if( !bDHCPEnabled || bIsNFSRoot ) {
                            /* Userspace can't do DHCP then. Therefore we give
                             * the DHCP settings here. Otherwise they are the
                             * values from NVRAM */
                            strncat( szBootargs, " ", sizeof( szBootargs ) );
                            strncat( szBootargs, GetEnvVar( "ip", 0 ), sizeof( szBootargs ) );
                    }

                    strncat( szBootargs, " ", sizeof( szBootargs ) );
                    strncat( szBootargs, GetEnvVar( "console", 0 ), sizeof( szBootargs ) );

                    strncat( szBootargs, " ", sizeof( szBootargs ) );
                    if( bIsNFSRoot ) {
                            strncat( szBootargs, GetEnvVar( SNFS, 0 ), sizeof( szBootargs ) );
                            strncat( szBootargs, GetEnvVar( NPATH, 0 ), sizeof( szBootargs ) );
                    } else
                            strncat( szBootargs, GetEnvVar( SMTD, 0 ), sizeof( szBootargs ) );

                    /*
                     * When booting from USB or MMC the rootfs is expected in
                     * a partition of that media. The partition number to use
                     * must be stored in variable 'rpart' otherwise, the
                     * default partition is used (DEFAULT_ROOTFS_PARTITION)
                     */
                    if (pImgSrc->eType == IS_MMC || pImgSrc->eType == IS_HSMMC) {
			strncat(szBootargs, " root=", sizeof( szBootargs ) );
			if (GetEnvVar("mmc_rpart", 1))
				strncat(szBootargs, GetEnvVar("mmc_rpart", 1), sizeof(szBootargs));
			else
				strncat(szBootargs, DEFAULT_ROOTFS_MMC_PART, sizeof(szBootargs));
			/* A delay is needed to allow the media to
			 * be properly initialized before being able
			 * to mount the rootfs
			 */
			strncat(szBootargs, " rootdelay=", sizeof(szBootargs));
			if (GetEnvVar("rootdelay", 1))
				strncat(szBootargs, GetEnvVar("rootdelay", 1), sizeof(szBootargs));
			else
				sprintf(szBootargs, "%s%d", szBootargs, ROOTFS_DELAY);
                    }
                    else if (pImgSrc->eType == IS_USB) {
			strncat(szBootargs, " root=", sizeof( szBootargs ) );
			if (GetEnvVar("usb_rpart", 1))
				strncat(szBootargs, GetEnvVar("usb_rpart", 1), sizeof(szBootargs));
			else
				strncat(szBootargs, DEFAULT_ROOTFS_USB_PART, sizeof(szBootargs));
			/* A delay is needed to allow the media to
			 * be properly initialized before being able
			 * to mount the rootfs
			 */
			strncat(szBootargs, " rootdelay=", sizeof(szBootargs));
			if (GetEnvVar("rootdelay", 1))
				strncat(szBootargs, GetEnvVar("rootdelay", 1), sizeof(szBootargs));
			else
				sprintf(szBootargs, "%s%d", szBootargs, ROOTFS_DELAY);
                    }
                    else {
                            if( bAppendMtdRootToBootargs ) {
                                    strncat( szBootargs, " ", sizeof( szBootargs ) );
                                    CE( PartStrAppendRoot( szBootargs, sizeof( szBootargs ) ) );
                            }
                    }
		    /* MTD partitions */
                    strncat( szBootargs, " ", sizeof( szBootargs ) );
                    CE( PartStrAppendParts( szBootargs, sizeof( szBootargs ) ) );

                    if(GetEnvVar( "video", 1 ) ) {
                            strncat( szBootargs, " video=", sizeof( szBootargs ) );
                            strncat( szBootargs, GetEnvVar( "video", 1 ), sizeof( szBootargs ) );
                    }

                    CE( RunCmd( szBootargs ) );
		    if(iLoadAddrInitRD)
			    sprintf( szCmd, "bootm 0x%x 0x%x", iLoadAddr, iLoadAddrInitRD);
		    else
			    sprintf( szCmd, "bootm 0x%x", iLoadAddr);
                    break;
            }
            case NVOS_WINCE:
		    SetBootargs( argc, argv );
	    case NVOS_EBOOT:
                sprintf( szCmd, "go 0x%x", iLoadAddr );
                break;
            case NVOS_NETOS:
                sprintf( szCmd, "bootm 0x%x", iLoadAddr );
                break;
            default:
                eprintf( "Operating system not supported\n" );
        } /* switch( eOSType ) */

        printf( "%s will be booted now\n", argv[ 1 ] );

        /* maybe an OS (e.g. WinCE) checks the Workcopys CRC32 */
	/* TODO this is a waste of time, optimize... */
        NvWorkcopyUpdateCRC32();

        /* check from what source to boot */
        return !RunCmd( szCmd );

usage:
        printf( "Usage:\n%s\n%s\n", cmdtp->usage, cmdtp->help );
        return 1;

error:
        return 1;
}

static int do_digi_update( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] )
{
        int iLoadAddr  = -1;
        const image_source_t* pImgSrc = NULL;
        const part_t* pPart           = NULL;
        const char*   szTmp           = NULL;
        const nv_param_part_t* pPartEntry = NULL;
        int iFileSize  = 0;
        int iCRCSize   = 0;
        crc32_t uiCRC;
	char szCmd[ 80 ] = "";
	char szImg[ 60 ] = "";
        char bDHCPEnabled = 0;
        static const char* szUpdating = "Updating";
#ifdef CFG_HAS_WIRELESS
	wcd_data_t *pWCal;
#endif

        if( ( argc < 2 ) || ( argc > 6 ) )
                goto usage;

        clear_ctrlc();

        /* check what to update */
        CE( WhatPart( argv[ 1 ], 0, &pPart, &pPartEntry, 1 ) );

        if( pPartEntry->flags.bReadOnly || pPartEntry->flags.bFixed )
                /* U-Boot is always marked read-only, but has been checked
                 * above already  */
                CE( WaitForYesPressed( "Partition marked read-only / fixed."
			" Do you want to continue?", szUpdating ) );

        if( argc >= 3 )
                pImgSrc = WhatImageSource( argv[ 2 ] );
        else
                pImgSrc = &l_axImgSrc[ IS_TFTP ];

        if( NULL == pImgSrc )
                goto usage;

        /* determine file to update */
        if( argc == 4 )
                szTmp = argv[ 3 ];
	else if (argc == 6)
		szTmp = argv[5];
        else if( NULL != pPart ) {
                /* not present, but we have a partition definition */
                szTmp = GetEnvVar( pPart->szEnvVar, 0 );
                CE( NULL != szTmp );
        } else {
                eprintf( "Require filename\n" );
                goto error;
        }
        strncat( szImg, szTmp, sizeof( szImg ) );

        CE( GetDHCPEnabled( &bDHCPEnabled ) );

        /* we check result later, it may be set otherwise */
        (void) GetIntFromEnvVar( &iLoadAddr, "loadaddr", 1 );

        /* we require it being set from download tool */
        setenv( "filesize", "" );

	if( bDHCPEnabled &&
	    (( IS_TFTP == pImgSrc->eType ) || ( IS_NFS == pImgSrc->eType )) )
		/* makes no sense for USB or FLASH download to need DHCP */
		CE( DoDHCP() );

        /* get images into RAM */
        switch( pImgSrc->eType ) {
#if (CONFIG_COMMANDS & CFG_CMD_NET)
            case IS_TFTP:
                sprintf( szCmd, "tftp 0x%x %s", iLoadAddr, szImg );
                CE( RunCmd( szCmd ) );
                break;
            case IS_NFS:
                sprintf( szCmd, "nfs 0x%x %s/%s",
                         iLoadAddr, GetEnvVar( NPATH, 0 ), szImg );
                CE( RunCmd( szCmd ) );
                break;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_USB)
            case IS_USB:
            {
		char kdevpart[8];
		char kfs[8];

		/* Device number can be given as argument
		 * Otherwise, default value is used */
		if (argc > 4)
			strcpy(kdevpart, argv[3]);
		else
			strcpy(kdevpart, DEFAULT_KERNEL_DEVPART);

		/* File system can be given as argument
		* Otherwise, default value is used */
		if (argc > 4)
			strcpy(kfs, argv[4]);
		else
			strcpy(kfs, DEFAULT_KERNEL_FS);

		if (!strcmp(kfs, "fat") || !strcmp(kfs, "vfat"))
			sprintf(szCmd, "fatload usb %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else if (!strcmp(kfs, "ext2") || !strcmp(kfs, "ext3"))
			sprintf(szCmd, "ext2load usb %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else {
			printf("error: invalid value for filesystem (must be 'fat', 'vfat', 'ext2', 'ext3'\n");
			goto error;
		}

		CE(RunCmd("usb reset"));
		CE(RunCmd(szCmd));
	        break;
	    }
#endif
#if (CONFIG_COMMANDS & CFG_CMD_MMC)
            case IS_MMC:
            {
		char kdevpart[8];
		char kfs[8];

		/* Device number can be given as argument
		 * Otherwise, default value is used */
		if (argc > 4)
			strcpy(kdevpart, argv[3]);
		else
			strcpy(kdevpart, DEFAULT_KERNEL_DEVPART);

		/* File system can be given as argument
		* Otherwise, default value is used */
		if (argc > 4)
			strcpy(kfs, argv[4]);
		else
			strcpy(kfs, DEFAULT_KERNEL_FS);

		if (!strcmp(kfs, "fat") || !strcmp(kfs, "vfat"))
			sprintf(szCmd, "fatload mmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else if (!strcmp(kfs, "ext2") || !strcmp(kfs, "ext3"))
			sprintf(szCmd, "ext2load mmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else {
			printf("error: invalid value for filesystem (must be 'fat', 'vfat', 'ext2', 'ext3'\n");
			goto error;
		}

		CE(RunCmd("mmcinit"));
		CE(RunCmd(szCmd));
	        break;
	    }

	    case IS_HSMMC:
	    {
		char kdevpart[8];
		char kfs[8];

		/* Device number can be given as argument
		 * Otherwise, default value is used */
		if (argc > 4)
			strcpy(kdevpart, argv[3]);
		else
			strcpy(kdevpart, DEFAULT_KERNEL_DEVPART);

		/* File system can be given as argument
		* Otherwise, default value is used */
		if (argc > 4)
			strcpy(kfs, argv[4]);
		else
			strcpy(kfs, DEFAULT_KERNEL_FS);

		if (!strcmp(kfs, "fat") || !strcmp(kfs, "vfat"))
			sprintf(szCmd, "fatload hsmmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else if (!strcmp(kfs, "ext2") || !strcmp(kfs, "ext3"))
			sprintf(szCmd, "ext2load hsmmc %s 0x%x %s", kdevpart, iLoadAddr, szImg);
		else {
			printf("error: invalid value for filesystem (must be 'fat', 'vfat', 'ext2', 'ext3'\n");
			goto error;
		}

		CE(RunCmd("hsmmcinit"));
		CE(RunCmd(szCmd));
		break;
	    }
#endif
            case IS_FLASH:
	    default:
                /* makes really no sense in the update func */
                goto usage;
        }

        CE( !DVTError() );

        /* should be set by download tool */
        CE( GetIntFromEnvVar( &iFileSize, "filesize", 0 ) );

        iCRCSize = iFileSize - ( (NULL != pPart && pPart->bCRC32Appended) ?
		   sizeof( uiCRC ) : 0 );
	if (iCRCSize <= 0) {
		printf("Partition requires checksum but provided file is to small \
			to contain a checksum\n");
		goto error;
	}
        uiCRC = crc32( 0, (uchar*) iLoadAddr, iCRCSize );
        printf( "Calculated checksum = 0x%x\n", uiCRC );

        if (NULL != pPart && pPart->bCRC32Appended) {
                /* check CRC32 in File */
                crc32_t uiCRCFile;
                /* works independent whether the CRC is aligned or not. We
                   * don't know what the image does. */
                memcpy( &uiCRCFile, (const crc32_t*) ( iLoadAddr + iCRCSize ),
                        sizeof( uiCRCFile ) );
                if( uiCRCFile != uiCRC ) {
                        eprintf( "CRC32 mismatch: Image reports 0x%0x - ",
                                 uiCRCFile );
                        CE( WaitForYesPressed( "Continue", szUpdating ) );
                }
        }

        /* run some checks based on the image */

        switch( pPartEntry->eType ) {
            case NVPT_UBOOT:
                CE( WaitForYesPressed( "Do you really want to overwrite U-Boot flash partition", szUpdating ) );
                break;
#ifdef CFG_HAS_WIRELESS
            case NVPT_NVRAM:
                if( !strcmp( argv[ 1 ], "wifical" ) ) {
                        CE( WaitForYesPressed(
			    "Do you really want to update the Wireless Calibration Information", szUpdating ) );
                        pWCal = (wcd_data_t *)iLoadAddr;
                        if ( !NvPrivWCDSetInNvram( pWCal ) ) {
                                eprintf( "Invalid calibration data file\n" );
                                goto error;
                        }
                        saveenv();
                        goto done;
                }
                break;
#endif
            case NVPT_FPGA:
            {
#ifdef CFG_FPGA_SIZE
                    if( CFG_FPGA_SIZE != ( iFileSize - 4 ) ) {
                            /* +4 because of checksum added */
                            eprintf( "Expecting FPGA to have size 0x%x\n",
                                     CFG_FPGA_SIZE );
                            goto error;
                    }

                    printf( "Updating FPGA firmware ...\n" );
                    if( LOAD_FPGA_FAIL == fpga_checkbitstream( (uchar*) iLoadAddr, iFileSize ) ) {
                            eprintf( "Updating FPGA firmware failed\n" );
                            goto error;
                    }

                    /* fpga_checkbitstream doesn't print \0 */
                    printf( "\n" );
                    CE( WaitForYesPressed( "Do you really want to overwrite FPGA firmware", szUpdating ) );
#else  /* CONFIG_FPGA_SIZE */
                    printf( "No FPGA available\n" );
#endif  /* CONFIG_FPGA_SIZE */
                    break;
            }
            default: break;  /* avoid compiler warnings */
        }

#ifdef CONFIG_PARTITION_SWAP
        if( ( NULL != pPart ) && pPart->bSwapEndian ) {
                /* swapping is done on 16/32bit. Ensure that there is space */
                int iFileSizeAligned = ( iFileSize + 0x3 ) & ~0x3;
                /* fill swapped areas with empty character */
                memset( (void*) iLoadAddr + iFileSize, 0xff, iFileSizeAligned - iFileSize );
                iFileSize = iFileSizeAligned;
        }
#endif

        /* update images */

        /* fit we into it? */
        if( PartSize( pPartEntry ) < iFileSize ) {
                /* 0 means to end of flash */
                eprintf( "Partition too small\n" );
                goto error;
        }

        CE( PartProtect( pPartEntry, 0 ) );

        /* erase complete partition */
        CE( PartErase( pPartEntry ) );
        CE( AppendPadding( pPartEntry, (void*) iLoadAddr, &iFileSize ) );

#ifdef CONFIG_PARTITION_SWAP
        if( ( NULL != pPart ) && pPart->bSwapEndian )
                /* do it before writing. Swapping not supported for user
                 * named partitions */
                CE( PartSwap( pPartEntry, (void*) iLoadAddr, iFileSize ) );
#endif

        /* write partition */
        CE( PartWrite( pPartEntry, (void*) iLoadAddr, iFileSize ) );
        /* verify it */
        CE( PartVerify( pPartEntry, (void*) iLoadAddr, iFileSize ) );

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
        if( pPartEntry->uiChip )
                /* !TODO */
                eprintf( "*** Chip %i not supported yet\n" );

        if( ( NVPT_FILESYSTEM == pPartEntry->eType ) &&
            ( NVFS_JFFS2      == pPartEntry->flags.fs.eType ) ) {
                printf( "Writing cleanmarkers\n" );
                CE( jffs2_mark_clean( pPartEntry->ullStart,
                                      pPartEntry->ullSize ) );
        }
#endif

        if( pPartEntry->flags.bFixed || pPartEntry->flags.bReadOnly)
                CE( PartProtect( pPartEntry, 1 ) );

        switch( pPartEntry->eType ) {
            case NVPT_FPGA:
            case NVPT_UBOOT:
                /* be user friendly. fpga is not loaded */
                printf( "Reboot system so update takes effect\n" );
                break;
            default:
                break;
        }
#ifdef CFG_HAS_WIRELESS
done:
#endif
        printf( "Update successful\n" );

        return 0;
usage:
        printf( "Usage:\n%s\n%s\n", cmdtp->usage, cmdtp->help );

error:
        if( ( NULL != pPartEntry ) &&
            ( pPartEntry->flags.bFixed || pPartEntry->flags.bReadOnly ) )
                /* it might have been unprotected */
                PartProtect( pPartEntry, 1 );

        return 1;
}

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
static int jffs2_mark_clean (long long offset, long long size)
{
	struct mtd_info *nand;
	struct mtd_oob_ops oob_ops;
	int i, magic_ofs, magic_len;
	long long end;
	unsigned char magic[] = {0x85, 0x19, 0x03, 0x20, 0x08, 0x00, 0x00, 0x00};

	for (i = 0; i < CFG_MAX_NAND_DEVICE; i++) {
		if (nand_info[i].name)
			break;
	}
	nand = &nand_info[i];
        /* !see [1] */
	switch(nand->oobsize) {
		case 8:
			magic_ofs = 0x06;
			magic_len = 0x02;
                        break;
		case 16:
			magic_ofs = 0x08;
			magic_len = 0x08;
			break;
		case 64:
			magic_ofs = 0x10;
			magic_len = 0x08;
			break;
		default:
			printf("Cannot set markers on this oobsize!\n");
                        goto error;
	}

	oob_ops.mode = MTD_OOB_PLACE;
	oob_ops.ooblen = magic_len;
	oob_ops.oobbuf = magic;
	oob_ops.datbuf = NULL;
	oob_ops.ooboffs = magic_ofs;

	/* calculate end */
	for(end = offset + size; offset < end; offset += nand->erasesize) {
		/* skip if block is bad */
		if(nand->block_isbad(nand, offset))
			continue;

		/* modify oob */
		 if(nand->write_oob(nand, offset, &oob_ops))
			 goto error;

	} /* for( end=offset) */

	return 1;

error:
	return 0;
}
#endif  /* CONFIG_COMMANDS & CFG_CMD_NAND */

static int do_envreset( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] )
{
	switch( argc ) {
		case 1:
		break;

            default:
		printf( "Usage:\n%s\n", cmdtp->usage );
		return -1;
	}

	printf( "Environment will be set to Default now!\n" );

        NvEnvUseDefault();

        return saveenv();
}

static int do_printenv_dynamic( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] )
{
        int i;

        for( i = 0; i < ARRAY_SIZE( l_axEnvDynamic ); i++ ) {
                const char* szVar = l_axEnvDynamic[ i ].szEnvVar;
                printf( "%s=%s\n", szVar,  GetEnvVar( szVar, 0 ) );
        }

        return 0;
}

static int do_erase_pt( cmd_tbl_t* cmdtp, int flag, int argc, char* argv[] )
{
        const nv_param_part_t* pPartEntry = NULL;

        if( argc != 2 )
                goto usage;

        clear_ctrlc();

        /* determine OS and/or partition */
        CE( WhatPart( argv[ 1 ], 0, NULL, &pPartEntry, 1 ) );

        switch( pPartEntry->eType ) {
            case NVPT_UBOOT:
            case NVPT_NVRAM:
                /* protect against silly stuff. Use update/envreset */
                eprintf( "Can't erase a critical partition\n" );
                goto error;
            default:
                break;
        }

        if( pPartEntry->flags.bReadOnly || pPartEntry->flags.bFixed )
                CE( WaitForYesPressed( "Partition marked read-only /fixed. Do you want to continue?", "Erasing" ) );

        CE( PartProtect( pPartEntry, 0 ) );
        CE( PartErase( pPartEntry ) );
        if( pPartEntry->flags.bFixed || pPartEntry->flags.bReadOnly)
                CE( PartProtect( pPartEntry, 1 ) );

        return 0;

usage:
        printf( "Usage:\n%s\n%s\n", cmdtp->usage, cmdtp->help );

error:
        return 1;
}

static int WhatPart(
        const char* szPart, char bForBoot,
        const part_t** ppPart,
        const nv_param_part_t** ppPartEntry,
        char bPartEntryRequired )
{
        int iPartition = 0;  /* later for multi-partition support */
        int i = 0;
        const part_t* pPart = NULL;
        const nv_param_part_t* pPartEntry = NULL;

	/* Search in l_axPart by the abreviated name */
        while( i < ARRAY_SIZE( l_axPart ) ) {
                if( !strcmp( szPart, l_axPart[ i ].szName ) &&
                    ( !bForBoot || l_axPart[ i ].bForBoot ) ) {
                        pPart = &l_axPart[ i ];

                        /* abbreviated name found, does it exists? */
                        /* find first rootfs partition */
                        NvParamPartFind( &pPartEntry, pPart->ePartType,
                                         NVFS_NONE, pPart->bRootFS, iPartition );
                        break;
                }
		pPart = NULL;	/* Reset if not found */
                i++;
        } /* !while */

        if( NULL == pPart ) {
                /* internal abbrevation name userfs/rootfs/u-boot/not found,
                   try it with szPart as partition name.
		   wifical lives inside the NVRAM partition. Tweak that if
                   that is what we are looking for */
#ifdef CFG_HAS_WIRELESS
		if( !strcmp( szPart, "wifical" ) )
                        pPartEntry = FindPartition( "NVRAM" );
                else
#endif
                        pPartEntry = FindPartition( szPart );
                if( NULL == pPartEntry ) {
                        eprintf( "Partition %s not found\n", szPart );
                        goto error;
                }
		else {
			/* Find the first partition in l_axPart array that
			* matches the partition type of the partition
			* located in NVRAM */
			findpart_tableentry(&pPart, pPartEntry, 0);
		}
        }

        if( bPartEntryRequired && ( NULL == pPartEntry ) ) {
                eprintf( "Partition %s not found\n", szPart );
                goto error;
        }

	if( NULL != pPart )
		*ppPart = pPart;

	if( NULL != pPartEntry)
		*ppPartEntry = pPartEntry;

	return 1;

error:
        return 0;
}

static const image_source_t* WhatImageSource( const char* szSrc )
{
        int i = 0;

        while( i < ARRAY_SIZE( l_axImgSrc ) ) {
                if( !strcmp( szSrc, l_axImgSrc[ i ].szName ) )
                        return &l_axImgSrc[ i ];

                i++;
        } /* !while */

        return NULL;
}

/*! \brief Runs command and prints error on failure */
/*! \return 0 on failure otherwise 1
 */
static int RunCmd( const char* szCmd )
{
        int iRes;

        clear_ctrlc();

        iRes = ( run_command( szCmd, 0 ) >= 0 );

	if( ctrlc() || had_ctrlc() )
                iRes = 0;

        if( !iRes )
                eprintf( "command %s failed\n", szCmd );

        return iRes;
}

/*! \brief Returns dynamic or normal environment variable */
/*! \param bSilent true if not existant should not be reported
 */
const char* GetEnvVar( const char* szVar, char bSilent )
{
        const char* szTmp = NULL;
        static char szTmpBuf[ 128 ];
	static char sEraseSize[ 8 ];
        static char ending[ 10 ];

	szTmp = getenv( (char*) szVar );
        if( ( NULL == szTmp ) ) {
                /* variable not yet defined, try to read it in l_axPart */
                if( !strcmp( szVar, RIMG ) || !strcmp( szVar, USRIMG ) ) {
                        char bRIMG = !strcmp( szVar, RIMG );
                        const nv_param_part_t* pPartEntry;
                        size_t iEraseSize = GetEraseSize( NULL );

                        /* find first rootfs partition */
                        if( !NvParamPartFind( &pPartEntry, NVPT_FILESYSTEM,
                                              NVFS_NONE, bRIMG, 0 ) )
                                return "";

                        iEraseSize = GetEraseSize( pPartEntry );
			memset(sEraseSize,0,sizeof(sEraseSize));

			if( bRIMG ) {
				switch( pPartEntry->flags.fs.eType) {
					case NVFS_JFFS2:
						sprintf(sEraseSize, "-%i", iEraseSize / 1024);
						strcpy( ending, "jffs2");
						break;
					case NVFS_CRAMFS:
						strcpy( ending, "cramfs");
						break;
					case NVFS_INITRD:
						strcpy( ending, "initrd");
						break;
					case NVFS_SQUASHFS:
						strcpy( ending, "squashfs");
						break;
					case NVFS_ROMFS :
						strcpy( ending, "romfs");
						break;
					default:
						sprintf(sEraseSize, "-%i", iEraseSize / 1024);
						strcpy( ending, "jffs2");
						break;
				}
				sprintf( szTmpBuf, "%s%s.%s",
					( (CONFIG_IMAGE_JFFS2_BASENAME == NULL) ? "rootfs-"CONFIG_MODULE_NAME :
					  CONFIG_IMAGE_JFFS2_BASENAME ), sEraseSize, ending );
			} else
				sprintf( szTmpBuf, "userfs-%s-%i.jffs2",
						CONFIG_MODULE_NAME, iEraseSize / 1024 );
			szTmp = szTmpBuf;
		} else if( !strcmp( szVar, CONSOLE ) ) {
			sprintf( szTmpBuf, "console="CFG_CONSOLE"%i,%s",
					CONSOLE_INDEX,
					getenv( "baudrate" ) );
			szTmp = szTmpBuf;
		} else {
			int i = 0;

			while( i < ARRAY_SIZE( l_axEnvDynamic ) ) {
				if( !strcmp( l_axEnvDynamic[ i ].szEnvVar, szVar ) ) {
					szTmp = l_axEnvDynamic[ i ].szEnvDflt;
					break;
				}
				i++;
			} /* while */
		}

		if( ( NULL == szTmp ) && !bSilent )
			eprintf( "variable %s does not exists\n", szVar );
	}

        return szTmp;
}

/*! \brief Converts an environment variable to an integer */
/*! \return 0 on failure otherwise 1
 */
static int GetIntFromEnvVar( int* piVal, const char* szVar, char bSilent )
{
        int iRes = 0;
        const char* szVal = GetEnvVar( szVar, bSilent );

        if( NULL != szVal ) {
                *piVal = simple_strtoul( szVal, NULL, 16 );
                iRes = 1;
        }

        return iRes;
}

/*! \brief padds the image, also adds JFFS2 Padding if it's a jffs2 partition. */
/*! \return 0 on failure otherwise wise 1
 */
static int AppendPadding( const nv_param_part_t* pPart, void* pvImage,
                          int* piFileSize )
{
        int iPageSize = GetPageSize( pPart );
        int iBytesFreeInBlock;

        if( !iPageSize )
                return 0;

        iBytesFreeInBlock = iPageSize - (*piFileSize % iPageSize);

        if( ( NVPT_FILESYSTEM == pPart->eType ) &&
            ( NVFS_JFFS2      == pPart->flags.fs.eType ) &&
            ( iBytesFreeInBlock >= sizeof( struct jffs2_unknown_node ) ) ) {
                /* JFFS CRC32 starts from 0xfffffff, our crc32 from 0x0 */
                uint32_t uiStart = 0xffffffff;
                /* write padding to avoid Empty block messages.
                   see linux/fs/jffs2/wbuf.c:flush_wbuf */
                struct jffs2_unknown_node* pNode = (struct jffs2_unknown_node*) ( pvImage + *piFileSize );

                printf( "Padding last sector\n" );

                /* 0 for JFFS2 PADDING Type, see wbuf.c */
                memset( pvImage + *piFileSize, 0x0, iBytesFreeInBlock );

                *piFileSize       += iBytesFreeInBlock;
                iBytesFreeInBlock -= sizeof( *pNode );

                CLEAR( *pNode );

                /* no cpu_to_je16, it's always host endianess in U-Boot */
                pNode->magic    = JFFS2_MAGIC_BITMASK;
                pNode->nodetype = JFFS2_NODETYPE_PADDING;

                pNode->totlen   = iBytesFreeInBlock;
                /* don't CRC32 the hdr_crc itself */
                pNode->hdr_crc  = crc32( uiStart, (uchar*) pNode, sizeof( *pNode ) - 4 ) ^ uiStart;
        } else {/* if( JFFS2 ) */
                /* clear remaining stuff */
                memset( pvImage + *piFileSize, 0xff, iBytesFreeInBlock );
        }

        return 1;
}

static const nv_param_part_t* FindPartition( const char* szName )
{
        nv_critical_t*               pCrit      = NULL;
        const nv_param_part_table_t* pPartTable = NULL;
        unsigned int u;

        if( !CW( NvCriticalGet( &pCrit ) ) )
                return NULL;

        pPartTable = &pCrit->s.p.xPartTable;

        for( u = 0; u < pPartTable->uiEntries; u++ )
		if( !strcmp( szName, pPartTable->axEntries[ u ].szName) )
                        return &pPartTable->axEntries[ u ];

        return NULL;
}

static int GetDHCPEnabled( char* pcEnabled )
{
        nv_critical_t* pCrit = NULL;

        *pcEnabled = 0;
        CE( NvCriticalGet( &pCrit ) );

        *pcEnabled = pCrit->s.p.xIP.axDevice[ 0 ].flags.bDHCP;

        return 1;

error:
        return 0;
}

/*! \brief performs a DHCP/BOOTP request without loading any file */
/*! \return 0 on failure otherwise 1
 */
static int DoDHCP( void )
{
        int  iRes = 0;
        char szTmp[ 80 ];

        /* get current value */
        strncpy( szTmp, getenv( "autoload" ), sizeof( szTmp ) );
        /* autoload is used in BOOTP to do TFTP itself.
           We do tftp ourself because we don't need bootfile */
        setenv( "autoload", "n" );
#if (CONFIG_COMMANDS & CFG_CMD_NET)
        if( NetLoop( DHCP ) >= 0 ) {
                /* taken from netboot_common. But we don't need autostart */
                netboot_update_env();
                iRes = 1;
        }
#endif
        /* restore old autoload */
        setenv( "autoload", szTmp );

        return iRes;
}

static int IsValidUImage( image_header_t* pHeader )
{
        int iRes = 0;

        /* check image */
        if( ( ntohl( pHeader->ih_magic ) == IH_MAGIC ) ) {
                ulong ulCRC32 = ntohl( pHeader->ih_hcrc );
                pHeader->ih_hcrc = 0;
                if( crc32( 0, (uchar*)  pHeader, sizeof( *pHeader ) ) == ulCRC32 )
                        /* image header is correct */
                        iRes = 1;
                pHeader->ih_hcrc = htonl( ulCRC32 );
        }

        return iRes;
}

static int GetUImageSize( const nv_param_part_t* pPart,
                          size_t* piSize, char* pbCompressed )
{
        *piSize = 0;
        *pbCompressed = 0;

        if( pPart->ullSize >= sizeof( header ) ) {
                CE( PartRead( pPart, &header, sizeof( header ), 1 ) );

                if( IsValidUImage( &header ) ) {
                        *piSize = ntohl( header.ih_size ) + sizeof( header );
                        /* only support for gzip */
                        *pbCompressed = ( IH_COMP_GZIP == header.ih_comp );
                }

                /* when image is not correct, dboot will read whole partition
                 * and boot. This is more safe in respect of
                 * unsupported images than complaining here */
        }

        return 1;

error:
        return 0;
}

#if (CONFIG_COMMANDS & CFG_CMD_AUTOSCRIPT) && defined(CONFIG_AUTOLOAD_BOOTSCRIPT)
void run_auto_script(void)
{
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	int iLoadAddr = -1, ret;
	char szCmd[ 80 ]  = "";
	char *s, *bootscript;
	/* Save original timeouts */
        ulong saved_rrqtimeout_msecs = TftpRRQTimeoutMSecs;
        int saved_rrqtimeout_count = TftpRRQTimeoutCountMax;
	ulong saved_startagain_timeout = NetStartAgainTimeout;

	/* Check if we really have to try to run the bootscript */
	s = (char *)GetEnvVar("loadbootsc", 1);
	bootscript = (char *)GetEnvVar("bootscript", 1 );
	if (s && !strcmp(s, "yes") && bootscript) {
		CE( GetIntFromEnvVar( &iLoadAddr, "loadaddr", 1 ) );
		printf("Autoscript from TFTP... ");

		/* set timeouts for bootscript */
		TftpRRQTimeoutMSecs = AUTOSCRIPT_TFTP_MSEC;
		TftpRRQTimeoutCountMax = AUTOSCRIPT_TFTP_CNT;
		NetStartAgainTimeout = AUTOSCRIPT_START_AGAIN;

		/* Silence net commands during the bootscript download */
		NetSilent = 1;
		sprintf( szCmd, "tftp 0x%x %s", iLoadAddr, bootscript );
		ret = run_command( szCmd, 0 );
		/* First restore original values of global variables
		 * and then evaluate the result of the run_command */
		NetSilent = 0;
		/* Restore original timeouts */
		TftpRRQTimeoutMSecs = saved_rrqtimeout_msecs;
		TftpRRQTimeoutCountMax = saved_rrqtimeout_count;
		NetStartAgainTimeout = saved_startagain_timeout;

		if (ret < 0)
			goto error;

		printf("[ready]\nRunning bootscript...\n");
		/* Launch bootscript */
		ret = autoscript( iLoadAddr );
	}
	return;

error:
	printf( "[not available]\n" );
#endif
}
#else
void run_auto_script(void) {}
#endif

/* Looks in the l_axPart table, the iCount'th entry
 * that matches the given partition type */
int findpart_tableentry(
        const part_t **ppPart,
	const nv_param_part_t *pPartEntry,
        int            iCount )
{
	unsigned int u = 0;
	int iFound = 0;

	*ppPart = NULL;

	/* search the iCount'th partition entry */
	while( u < ARRAY_SIZE( l_axPart ) ) {
		if( pPartEntry->eType == l_axPart[u].ePartType ) {
			/* found one occurence */
			if( iFound == iCount ) {
				/* it's the n'th occurence we look for */
				*ppPart = (part_t *)&l_axPart[u];
				return 1;
			}
			iFound++;
		} /* if( pPartEntry->eType */

		u++;
	} /* while( u ) */

	return 0;
}

/**
 * Setup function for initialization/deinitialization needed
 * for each platform before jumping to the OS
 **/
void setup_before_os_jump(nv_os_type_e eOSType, image_source_e eType)
{
#if defined(CONFIG_NS9360)
	if (NVOS_LINUX == eOSType) {
		/* If booting Linux stop USB */
		usb_stop();
	}
#endif

#if defined(CONFIG_CC9M2443) || defined(CONFIG_CCW9M2443)
	/* Enable USB_POWEREN line before booting the OS */
	s3c_gpio_setpin(S3C_GPA14, 0);
#endif

	/* Halt Ethernet controller to avoid interrupts
	 * before the OS has fully initialized */
	eth_halt();
}

U_BOOT_CMD(
	dboot,	6,	0,	do_digi_dboot,
	"dboot   - Digi modules boot commands\n",
	"OS [source [device:part filesystem] [file]]\n"
	"  - boots 'OS' of 'source' of 'device' with 'filesystem'\n"
	"    values for 'OS': linux, wce, eboot, netos or any partition name\n"
	"    values for 'source': flash, tftp, nfs, usb, mmc, hsmmc\n"
	"    'device[:part]': number of device [and partition], for 'usb', 'mmc', 'hsmmc' sources\n"
	"    values for 'filesystem': fat|vfat, ext2|ext3\n"
	"    values for 'file'  : the file to be used for booting\n"
	"\n"
	"For wce following bootargs are possible:\n"
/* hide for user */
/*	"    ktil_ttyS0,kitl_ttyS1, kitl_ttyUSB, kitl_ethUSB, kitl_eth\n"
	"    cleanhive, cleanboot, formatpart\n"
*/
	"    cleanhive\n"
);

U_BOOT_CMD(
	update,	6,	0,	do_digi_update,
	"update  - Digi modules update commands\n",
	"partition [source [device:part filesystem] [file]]\n"
	"  - updates 'partition' via 'source'\n"
	"    values for 'partition': uboot, linux, rootfs, userfs, eboot, wce, wcez, netos,\n"
        "                            netos_loader, splash, or any partition name\n"
	"    values for 'source': tftp, nfs, usb, mmc, hsmmc\n"
	"    'device:part': number of device and partition, for 'usb', 'mmc', 'hsmmc' sources\n"
	"    values for 'filesystem': fat|vfat, ext2|ext3\n"
	"    values for 'file'  : the file to be used for updating\n"
);

U_BOOT_CMD(
	envreset,	1,	0,	do_envreset,
	"envreset- Sets environment variables to default setting\n",
	"\n"
	"  - overwrites all current variables\n"
);

U_BOOT_CMD(
	printenv_dynamic,	1,	0,	do_printenv_dynamic,
	"printenv_dynamic- Prints all dynamic variables\n",
	"\n"
	"  - Prints all dynamic variables\n"
);

U_BOOT_CMD(
	erase_pt,	2,	0,	do_erase_pt,
	"erase_pt- Erases the partition\n",
	"\npartition - the name of the partition\n"
);

#endif	/* (CONFIG_COMMANDS & CFG_CMD_BSP) */
