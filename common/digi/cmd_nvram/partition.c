/*
 *  U-Boot/common/digi/cmd_nvram/partition.c
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
 *  !Descr:      Provides a GUI frontend to "internal_nvram".
 *               We don't use NvPrivOSFlashXXX functions, because they are
 *               intended only for the flash NVRAM partition.
*/

#include <common.h>

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
# include <nand.h>
#endif  /* CFG_CMD_NAND */
#ifdef USE_UBOOT_VERSION
#include <zlib.h>               /* inflate */
#endif
#include <u-boot/zlib.h>               /* inflate */

#include "nvram.h"
#include "env.h"                /* CW */
#include "mtd.h"
#include "partition.h"
#include "../arch/arm/include/asm/string.h"

/* ********** defines ********** */

#define CTRL_C 0x3
#define KiB(x) ((x)/1024)

#define MAX( a, b ) ( (a) > (b) ? (a) : (b) )

/* ********** typedefs ********** */

struct cmd_info;

typedef struct cmd_info {
        char        cKey;
        const char* szDescr;
/* a NULL function means exit */
        int (*pfFunc) ( const struct cmd_info* pCmdInfo );
        int         iInfo;
} cmd_info_t;

/* from cmd_bootm */
#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

#define DEFLATED	8
extern void *zalloc(void *x, unsigned items, unsigned size);
extern void zfree(void *x, void *addr, unsigned nb);


/* ********** local functions ********** */

static int  PartCmdAppend( const struct cmd_info* pCmdInfo );
static int  PartCmdDelete( const struct cmd_info* pCmdInfo );
static int  PartCmdModify( const struct cmd_info* pCmdInfo );
static int  PartCmdPrintPartTable( const struct cmd_info* pCmdInfo );
static int  PartCmdReset( const struct cmd_info* pCmdInfo );

/* helper commands to set local variables */
static int  PartCmdPTSelect( const struct cmd_info* pCmdInfo );
static int  PartCmdFSSelect( const struct cmd_info* pCmdInfo );
static int  PartCmdOSSelect( const struct cmd_info* pCmdInfo );

static int  PartCmdTableLoop(
        const char* szWhat,
        const struct cmd_info* pCmdInfoTable, size_t iSize,
        char bOnlyOnce, int iInit );
static void PartCmdTablePrint( const struct cmd_info* pCmdInfoTable, size_t iSize );

static int  PartAutoAdjust( unsigned int uiPartition, uint8_t* pbMoved );
static int  PartModifyProperties(
        /*@out@*/ struct nv_param_part* pPart, char bPrintCurrent,
        int iIndex );
static int  PartSelectPart( const char* szWhat, /*@out@*/ uint32_t* puiPart );
static int  PartGetSize( const char* szMsg,
                         /*@out@*/ uint64_t* pullVal, char bModify );
static int  PartGetUInt( const char* szMsg,
                         /*@out@*/ uint32_t* puiVal, char bModify );
static int  PartGetBool( const char* szMsg,
                         /*@out@*/ uint8_t*  pbVal, char bModify );
static int  getsn( /*@out@*/ char* szOut, size_t iSize, char bModify );
static int PartGetChip( const struct nv_param_part* pPart );
static int PartGetThrottle( const struct nv_param_part* pPart, uint64_t ullSize );
static void PrintProgress( int iPercentage, int iThrottle,
                           const char* szFmt, ... );

/* ********** local variables ********** */

/* function tables */
#define EXIT() \
        { .cKey = 'q', .szDescr = "Quit", .pfFunc = NULL }

#define MK( key, descr, func ) \
        { .cKey = key, .szDescr = descr, .pfFunc = func }

static const cmd_info_t l_axCmdGlobal[] = {
        MK( 'a', "Append partition",      PartCmdAppend     ),
        MK( 'd', "Delete partition",      PartCmdDelete     ),
        MK( 'm', "Modify partition",      PartCmdModify     ),
        MK( 'p', "Print partition table", PartCmdPrintPartTable ),
        MK( 'r', "Reset partition table", PartCmdReset      ),
        EXIT(),
};
#undef MK

#define MK( key, descr, info )                         \
        { .cKey = key, .szDescr = descr, .pfFunc = PartCmdPTSelect, .iInfo = info }
static cmd_info_t l_axCmdPartType[] = {
        MK( 'e', NULL, NVPT_EBOOT ),
        MK( 'f', NULL, NVPT_FILESYSTEM ),
        MK( 'l', NULL, NVPT_LINUX ),
        MK( 'n', NULL, NVPT_NVRAM ),
        MK( 'o', NULL, NVPT_NETOS ),
        MK( 'O', NULL, NVPT_NETOS_LOADER   ),
        MK( 'N', NULL, NVPT_NETOS_NVRAM    ),
        MK( 'r', NULL, NVPT_WINCE_REGISTRY ),
        MK( 's', NULL, NVPT_SPLASH_SCREEN  ),
        MK( 'u', NULL, NVPT_UBOOT ),
        MK( 'w', NULL, NVPT_WINCE ),
        MK( 'F', NULL, NVPT_FPGA  ),
        MK( '0', NULL, NVPT_UNKNOWN ),
};
#undef MK

#define MK( key, descr, info )                         \
        { .cKey = key, .szDescr = descr, .pfFunc = PartCmdFSSelect, .iInfo = info }
static cmd_info_t l_axCmdFSType[] = {
        /* NVFS_NONE not used */
        MK( 'j', NULL, NVFS_JFFS2   ),
        MK( 's', NULL, NVFS_SQUASHFS),
	MK( 'c', NULL, NVFS_CRAMFS  ),
        MK( 'i', NULL, NVFS_INITRD  ),
	MK( 'r', NULL, NVFS_ROMFS   ),
        MK( 'f', NULL, NVFS_FLASHFX ),
        MK( 'e', NULL, NVFS_EXFAT   ),
        MK( 'y', NULL, NVFS_YAFFS   ),
        MK( '0', NULL, NVFS_UNKNOWN ),
};
#undef MK

#define MK( key, descr, info )                         \
        { .cKey = key, .szDescr = descr, .pfFunc = PartCmdOSSelect, .iInfo = info }
static cmd_info_t l_axCmdOSType[] = {
        MK( 'n', NULL, NVOS_NONE   ),
#ifndef CONFIG_NETOS_BRINGUP
        MK( 'l', NULL, NVOS_LINUX  ),
#ifdef PART_WINCE_SIZE
        MK( 'w', NULL, NVOS_WINCE  ),
#endif
#endif
# ifdef PART_NETOS_KERNEL_SIZE
        MK( 'o', NULL, NVOS_NETOS  ),
# endif
#ifdef CONFIG_PARTITION
	        MK( 'u', NULL, NVOS_USER_DEFINED ),
#endif
};
#undef MK

static nv_param_part_table_t* l_pPartTable = NULL;
static nv_part_type_e l_ePartType          = NVPT_LAST;
static nv_fs_type_e   l_eFSType            = NVFS_LAST;
static nv_os_type_e   l_eOSType            = NVOS_LAST;
static char l_bAbort = 0;

/* ********** global functions ********** */

/*! \brief Erases the partition */
/*! \return 0 on failure otherwise 1 */

int PartErase( const struct nv_param_part* pPart )
{
        int               iChip   = PartGetChip( pPart );
        uint64_t          ullSize = PartSize( pPart );
        uint64_t          ullEnd  = pPart->ullStart + ullSize;
        int               iThrottle = PartGetThrottle( pPart, ullSize );
        uint64_t          ullAddr = pPart->ullStart;

        if( iChip < 0 )
                return 0;

        ullAddr = pPart->ullStart;
        while( ullAddr < ullEnd ) {
                size_t iEraseSize = MtdGetEraseSize( iChip, ullAddr );

                if( ctrlc() || had_ctrlc() )
                        goto error;

                if( !MtdBlockIsBad( iChip, ullAddr ) ) {
			if(ullSize >> 8)
				PrintProgress(((((ullAddr - pPart->ullStart) >> 8) * 100) / (ullSize >> 8)),
                                       iThrottle, "Erasing %i KiB @ 0x%08x:", KiB(iEraseSize), ullAddr);
			else
				PrintProgress((((ullAddr - pPart->ullStart) * 100) / ullSize),
                                       iThrottle, "Erasing %i KiB @ 0x%08x:", KiB(iEraseSize), ullAddr);

                        CE( MtdErase( iChip, ullAddr, iEraseSize ) );

                        /* clean markers are set in a separate function,
                           because that was being used with "nand erase" */
                }

                ullAddr += iEraseSize;
        }

        printf( "\rErasing:   complete                                      \n" );

        return 1;

error:
        printf( "\n" ERROR "Erase failed\n" );
        return 0;
}

/*! \brief Reads iSize bytes from pvBuf of flash partition */
/*!
  \param iSize if 0, the whole partition is read.
  \param bSilent if 0, no progress output is done
  \return 0 on failure otherwise 1 */

int PartRead(
        const struct nv_param_part* pPart,
        void*  pvBuf,
        size_t iSize,
        char   bSilent )
{
        int      iChip     = PartGetChip( pPart );
        uint64_t ullAddr   = pPart->ullStart;
        uint64_t ullEnd    = pPart->ullStart + PartSize( pPart );
        size_t   iRead     = 0;
        size_t   iSizeLeft;
        int      iThrottle;

        if( iChip < 0 )
                return 0;

        if( !iSize )
                /* read whole partition */
                iSize = ullEnd - ullAddr;
        iSizeLeft = iSize;

        iThrottle = PartGetThrottle( pPart, iSize );
        /* there is a small cosmetic failure. If there are bad sectors, our
         * progressbar never reaches 100% because we don't count them */

        while( iSizeLeft && ( ullAddr < ullEnd ) ) {
                size_t iBytesToRead = min( iSizeLeft, MtdGetEraseSize( iChip, ullAddr ) );

                if( ctrlc() || had_ctrlc() )
                        goto error;

                if( !MtdBlockIsBad( iChip, ullAddr ) ) {
                        if( !bSilent ) {
				if(iSize >> 8)
					PrintProgress((((iRead >> 8) * 100) / (iSize >> 8)),
							iThrottle, "Reading:");
				else
					PrintProgress(((iRead * 100) / iSize),
							iThrottle, "Reading:");
			}
                        CE( MtdRead( iChip, ullAddr, iBytesToRead, pvBuf ) );

                        iSizeLeft -= iBytesToRead;
                        pvBuf     += iBytesToRead;
                        iRead     += iBytesToRead;
                }

                ullAddr   += MtdGetEraseSize( iChip, ullAddr );
        } /* while( iSizeLeft */

        if( !bSilent )
                printf( "\rReading:   complete                                      \n" );

        return 1;

error:
        printf( "\n" ERROR "Read failed at block @ 0x%08qx\n", ullAddr );
        return 0;
}

/*! \brief Reads iSize bytes from pvBuf of flash partition with on-the-fly decompression*/
/*!
 * \param iSize if 0, the whole partition is read.
 * \return 0 on failure otherwise 1
 *
 * It is assumed that a U-Boot header is at the start of the partition
 * as it contains the information that the image is zipped.
 * That is not checked but skipped automatically
*/

int PartReadAndDecompress(
        const struct nv_param_part* pPart,
        void*  pvBuf,
        size_t iSize )
{
        int      iChip     = PartGetChip( pPart );
        /* skip U_Boot header telling to be compressed */
        uint64_t ullAddr   = pPart->ullStart + sizeof( image_header_t );
        uint64_t ullEnd    = pPart->ullStart + PartSize( pPart );
        size_t   iRead     = 0;
        char     bGzipInit = 0;
        int      iZipRes   = Z_STREAM_END;
        size_t   iSizeLeft;
        int      iThrottle;
        unsigned char* pucTmp = NULL;
        size_t   iBufSize = 0;
	z_stream xStream;

        if( iChip < 0 )
                return 0;

        if( !iSize )
                /* read whole partition */
                iSize = ullEnd - ullAddr;
        iSizeLeft = iSize;

        CLEAR( xStream );

        iThrottle = PartGetThrottle( pPart, iSize );
        /* there is a small cosmetic failure. If there are bad sectors, our
         * progressbar never reaches 100% because we don't count them */

        while( iSizeLeft && ( ullAddr < ullEnd ) ) {
                size_t iEraseSize = MtdGetEraseSize( iChip, ullAddr );
                size_t iBytesToRead = min( iSizeLeft, iEraseSize );

                if( iEraseSize > iBufSize ) {
			iBufSize = iEraseSize;

			if( NULL == pucTmp)
				free( pucTmp );
			pucTmp = (unsigned char*) malloc( 2 * iBufSize );

			if( NULL == pucTmp ) {
				eprintf( "No temporary memory for decompression\n" );
				goto error;
			}
                }

                if( ctrlc() || had_ctrlc() )
                        goto error;

                if( !MtdBlockIsBad( iChip, ullAddr ) ) {

			if(iSize >> 8)
				PrintProgress((((iRead >> 8) * 100) / (iSize >> 8)),
						iThrottle, "Unzipping:");
			else
				PrintProgress(((iRead * 100) / iSize),
						iThrottle, "Unzipping:");

                        CE( MtdRead( iChip, ullAddr, iBytesToRead, pucTmp ) );

                        if( !bGzipInit ) {
                                /* see common/cmd_bootm:gunzip */
                                int i, flags;

                                /* skip header */
                                i     = 10;
                                flags = pucTmp[3];
                                if( ( DEFLATED != pucTmp[2] ) ||
                                    (flags & RESERVED) ) {
                                        eprintf( "Error: Bad gzipped data\n" );
                                        goto error;
                                }
                                if( flags & EXTRA_FIELD )
                                        i = 12 + pucTmp[ 10 ] +
                                                ( pucTmp [ 11 ] << 8 );
                                if( flags & ORIG_NAME )
                                        i += strlen( (char*) &pucTmp[ i ] ) + 1;
                                if( flags & COMMENT )
                                        i += strlen( (char*) &pucTmp[ i ] ) + 1;
                                if( flags & HEAD_CRC )
                                        i += 2;

                                xStream.zalloc = zalloc;
                                xStream.zfree = zfree;
                                xStream.outcb = Z_NULL;

                                iZipRes = inflateInit2( &xStream, -MAX_WBITS );
                                if( Z_OK != iZipRes ) {
                                        eprintf( "Error: inflateInit2() returned %d\n", iZipRes );
                                        goto error;
                                }
                                xStream.avail_in  = iBytesToRead - i;
                                xStream.next_in   = pucTmp + i;
                                xStream.next_out  = pvBuf;
                                xStream.avail_out = 0x7fffffff;

                                bGzipInit = 1;
                        } else {
                                xStream.next_in = pucTmp;
                                xStream.avail_in  = iBytesToRead;
                        }

                        /* decompress */
                        iZipRes = inflate( &xStream, Z_FULL_FLUSH );
                        if( ( Z_OK != iZipRes ) &&
                            ( Z_STREAM_END != iZipRes ) ) {
                                eprintf( "Error: inflate() returned %d\n",
                                         iZipRes );
                                goto error;
                        }

                        /* update compressed statistics */

                        iSizeLeft -= iBytesToRead;
                        iRead     += iBytesToRead;
                }

                ullAddr   += MtdGetEraseSize( iChip, ullAddr );
        } /* while( iSizeLeft */

        printf( "\rUnzipping:   %s                                \n",
                ( Z_STREAM_END == iZipRes ) ? "complete" : "failed" );

        free( pucTmp );

        inflateEnd( &xStream );

        return 1;

error:
        if( bGzipInit )
                inflateEnd( &xStream );

        if( NULL != pucTmp )
                free( pucTmp );

        printf( "\n" ERROR "Read failed at block @ 0x%08qx\n", ullAddr );
        return 0;
}

/*! \brief Writes iSize bytes of pvBuf to flash partition */
/*! Blocks are _NOT_ verified after writing
  \return 0 on failure otherwise 1 */

int PartWrite(
        const struct nv_param_part* pPart,
        const void* pvBuf,
        size_t iSize )
{
        int      iChip     = PartGetChip( pPart );
        uint64_t ullAddr   = pPart->ullStart;
        uint64_t ullEnd    = pPart->ullStart + PartSize( pPart );
        size_t   iWritten  = 0;
        size_t   iSizeLeft = iSize;
        int      iThrottle;

        if( iChip < 0 )
                return 0;

        iThrottle = PartGetThrottle( pPart, iSize );

        while( iSizeLeft && ( ullAddr < ullEnd ) ) {
                size_t iBytesToWrite = min( iSizeLeft, MtdGetEraseSize( iChip, ullAddr ) );

                if( ctrlc() || had_ctrlc() )
                        goto error;

                if( !MtdBlockIsBad( iChip, ullAddr ) ) {
			if( iSize >> 8 )
				PrintProgress((((iWritten >> 8) * 100) / (iSize >> 8)),
						iThrottle, "Writing:");
			else
				PrintProgress(((iWritten  * 100) / iSize),
						iThrottle, "Writing:");

                        if ((!MtdWrite( iChip, ullAddr, iBytesToWrite, pvBuf )) &&
				(!MtdRewrite(iChip, ullAddr, iBytesToWrite, pvBuf)))
				goto error;

                        iSizeLeft -= iBytesToWrite;
                        pvBuf     += iBytesToWrite;
                        iWritten  += iBytesToWrite;
                } /* if( !MtdBlockIsBad */

                ullAddr   += MtdGetEraseSize( iChip, ullAddr );
        } /* while( iSizeLeft ) */

        printf( "\rWriting:   complete                                      \n" );

        if( iSizeLeft ) {
                printf( "\n" ERROR "Partition was too small\n" );
                goto error;
        }

        return 1;

error:
        printf( "\n" ERROR "Write failed at block @ 0x%08qx\n", ullAddr );
        return 0;
}

/*! \brief Verifies iSize bytes of pvBuf to flash partition */
/*! \return 0 on failure otherwise 1 */

int PartVerify(
        const struct nv_param_part* pPart,
        const void* pvBuf,
        size_t iSize )
{
        int      iChip     = PartGetChip( pPart );
        uint64_t ullAddr   = pPart->ullStart;
        uint64_t ullEnd    = pPart->ullStart + PartSize( pPart );
        size_t   iVerified = 0;
        size_t   iSizeLeft = iSize;
        void*    pvTmp     = NULL;
        int      iThrottle;
        int      iEraseSize = MtdGetEraseSize( iChip, ullAddr );

        if( iChip < 0 )
                return 0;

        /* allocate temporay buffer for reading */
	/* FIXME NOR flash has not the same erasesize for all blocks.
	 * Instead of iEraseSize the max erasesize should be taken */
        pvTmp = MtdVerifyAllocBuf( iEraseSize );

        iThrottle = PartGetThrottle( pPart, iSize );

        /* all blocks */
        while( iSizeLeft && ( ullAddr < ullEnd ) ) {
                size_t iBytesToVerify = min( iSizeLeft, MtdGetEraseSize( iChip, ullAddr ));

                if( ctrlc() || had_ctrlc() )
                        goto error;

                if( !MtdBlockIsBad( iChip, ullAddr ) ) {
			if( iSize >> 8 )
				PrintProgress((((iVerified >> 8) * 100) / (iSize >> 8)),
						iThrottle, "Verifying:");
			else
				PrintProgress(((iVerified * 100) / iSize),
						iThrottle, "Verifying:");


                        /* verify it */
                        CE( MtdVerify( iChip, ullAddr, iBytesToVerify, pvBuf, pvTmp ) );

                        iSizeLeft -= iBytesToVerify;
                        pvBuf     += iBytesToVerify;
                        iVerified += iBytesToVerify;
                } /* if( !MtdBlockIsBad */

                ullAddr   += MtdGetEraseSize( iChip, ullAddr );
        } /* while( iSizeLeft ) */

        printf( "\rVerifying: complete                                      \n" );

        if( iSizeLeft ) {
                printf( "\n" ERROR "Partition was too small\n" );
                goto error;
        }

        MtdVerifyFreeBuf( pvTmp );

        return 1;

error:
        MtdVerifyFreeBuf( pvTmp );

        printf( ERROR "Verify failed\n" );

        return 0;
}

int PartProtect(
        const struct nv_param_part* pPart,
        char bProtect )
{
        return MtdProtect( pPart->uiChip, pPart->ullStart, pPart->ullSize, bProtect );
}

#ifdef CONFIG_PARTITION_SWAP
/*! \brief swaps the partition as needed for the partition/memory chip*/
/*!
 * Supported swaps from "ABCDEFGH" to
 *   o BADCFEHG (16bit flash connected on Bits 16:31)
 * \return 0 on failure, otherwise 1
 */
int PartSwap(
        const struct nv_param_part* pPart,
        void* pvBuf,
        size_t iSize )
{
        unsigned char* pucBuf = (unsigned char*) pvBuf;
        unsigned char* pucEnd = pucBuf + iSize;

        if( iSize & 0x3 ) {
                printf( ERROR "Wrong Size for Swap" );
                goto error;
        }

        /* "ABCDEFGH" -> "BADCFEHG" */
        for( ; pucBuf < pucEnd; pucBuf += 2 ) {
                unsigned char ch = *pucBuf;
                *pucBuf = *( pucBuf + 1 );
                *(pucBuf + 1) = ch;
        }

        printf( "Swapping:  complete\n" );

        return 1;

error:
        return 0;
}

#endif

/*! \return the size for the flash partition */
uint64_t PartSize( const struct nv_param_part* pPart )
{
        uint64_t ullSize = 0;
        int iChip = PartGetChip( pPart );

        if( iChip >= 0 ) {
                ullSize = pPart->ullSize;

                if( !ullSize )
                        /* to end of flash */
                        ullSize = MtdSize( iChip ) - pPart->ullStart;
        }

        return ullSize;
}

/*! \brief something like fdisk for flash partition */
/*! \return 0 on failure otherwise 1 */

int PartGUI( void )
{
        nv_critical_t* pCritical = NULL;
        int i;
        int iRes;
        nv_param_part_table_t xBackup;
        char bSave      = 0;
        char bKeepInMem = 0;

        l_pPartTable = NULL;
        l_bAbort     = 0;

        if( !CW( NvCriticalGet( &pCritical ) ) )
                return 0;
        l_pPartTable = &pCritical->s.p.xPartTable;

        /* make a copy so we can detect differences. Lazy solution, but we have
         * 128kB of stack, and part table is < 1KiB  */
        xBackup = *l_pPartTable;

        /* don't use ASSERT, PartGUI is an application that should work even if
         * we missed something */
        if( NVPT_LAST != ARRAY_SIZE( l_axCmdPartType ) )
                printf( ERROR "Missing partition entries, provide %i of %i ***\n",
                        ARRAY_SIZE( l_axCmdPartType ), NVPT_LAST );

        /* we don't have type NVFS_NONE in our l_axCmdFSType table.
           Our GUI is ensuring that fs types are only asked for FILESYSTEM
           partitions */
        if( NVFS_LAST != ( ARRAY_SIZE( l_axCmdFSType ) + 1 ) )
                printf( ERROR "Missing fs entries, provide %i of %i ***\n",
                        ARRAY_SIZE( l_axCmdFSType ), NVFS_LAST );

        /* OS is not checked for completeness */

        /* fill l_axCmdPartType and l_axCmdFSType.
         * we don't autogenerate everything, e.g. numbering keys, because 'u'
         * for U-Boot is more intuitive than '0' */
        for( i = 0; i < ARRAY_SIZE( l_axCmdPartType ); i++ )
                if( PartCmdPTSelect == l_axCmdPartType[ i ].pfFunc )
                        l_axCmdPartType[ i ].szDescr = strdup( NvToStringPart( (nv_part_type_e) l_axCmdPartType[ i ].iInfo ) );
        for( i = 0; i < ARRAY_SIZE( l_axCmdFSType ); i++ )
                if( PartCmdFSSelect == l_axCmdFSType[ i ].pfFunc )
                        l_axCmdFSType[ i ].szDescr = strdup( NvToStringFS( (nv_fs_type_e) l_axCmdFSType[ i ].iInfo ) );

        for( i = 0; i < ARRAY_SIZE( l_axCmdOSType ); i++ )
                if( PartCmdOSSelect == l_axCmdOSType[ i ].pfFunc )
                l_axCmdOSType[ i ].szDescr = strdup( NvToStringOS( (nv_os_type_e) l_axCmdOSType[ i ].iInfo ) );

        PartCmdPrintPartTable( NULL );

        /* execute commands */
        printf( "Commands: \n" );
        iRes = PartCmdTableLoop( "Cmd",
                                 l_axCmdGlobal, ARRAY_SIZE( l_axCmdGlobal ), 0, -1 );

        if( iRes &&
            memcmp( l_pPartTable, &xBackup, sizeof( xBackup ) ) ) {
                printf( "Partition table has been modified. " );

                {
                        uint8_t bQuerySave = 1;

                        if( PartGetBool( "Save? ",
                                         &bQuerySave, 1 ) && bQuerySave )
                                bSave = 1;
                }
        }

        /* free memory */
        for( i = 0; i < ARRAY_SIZE( l_axCmdPartType ); i++ )
                if( PartCmdPTSelect == l_axCmdPartType[ i ].pfFunc ) {
                        free( (void*) l_axCmdPartType[ i ].szDescr );
                        l_axCmdPartType[ i ].szDescr = NULL;
                }
        for( i = 0; i < ARRAY_SIZE( l_axCmdFSType ); i++ )
                if( PartCmdFSSelect == l_axCmdFSType[ i ].pfFunc ) {
                        free( (void*) l_axCmdFSType[ i ].szDescr );
                        l_axCmdFSType[ i ].szDescr = NULL;
                }

        for( i = 0; i < ARRAY_SIZE( l_axCmdOSType ); i++ )
                if( PartCmdOSSelect == l_axCmdOSType[ i ].pfFunc ) {
                        free( (void*) l_axCmdOSType[ i ].szDescr );
                        l_axCmdOSType[ i ].szDescr = NULL;
                }

        if( bSave )
                iRes = CW( NvSave() );
        else {
                if( !bKeepInMem ) {
                        /* restore */
                        pCritical->s.p.xPartTable = xBackup;
                        printf( "Partition table NOT changed!\n" );
                } else
                        printf( "Partition table NOT stored persistent!\n" );
        }

        return iRes;
}

#define APPEND_CMDLINE( szWhat ) \
        strncat( szCmdLine, szWhat, iMaxSize )

/*! \brief Appends mtdparts info to szCmdline */
/*! From linux/drivers/mtd/cmdlinepart.c
 * mtdparts=<mtddef>[;<mtddef]
 * <mtddef>  := <mtd-id>:<partdef>[,<partdef>]
 * <partdef> := <size>[@offset][<name>][ro]
 * <mtd-id>  := unique name used in mapping driver/device (mtd->name)
 * <size>    := standard linux memsize OR "-" to denote all remaining space
 * <name>    := '(' NAME ')'
\return 0 on failure otherwise 1 */

int PartStrAppendParts( char* szCmdLine, size_t iMaxSize )
{
        nv_critical_t* pCrit = NULL;
        const nv_param_part_table_t* pPartTable = NULL;
        uint32_t uiLastChip  = 0xffff;
        int      i;

        CE( 0 != iMaxSize );
        CE( NvCriticalGet( &pCrit ) );

        APPEND_CMDLINE( "mtdparts=" );

        pPartTable = &pCrit->s.p.xPartTable;

        /* append partitions */
        for( i = 0; i < pPartTable->uiEntries; i++ ) {
                char  acTmp[ 64 ];
                const nv_param_part_t* pPart = &pPartTable->axEntries[ i ];
                if( i )
                        APPEND_CMDLINE( "," );

                if( uiLastChip != pPart->uiChip ) {
                        /* new chip device, add mtddef */
                        uiLastChip = pPart->uiChip;
                        if( !pPart->uiChip )
#if defined(CONFIG_CC9P9215) || defined(CONFIG_CCW9P9215) || defined(CONFIG_CME9210) || \
    defined(CONFIG_INC20OTTER) || defined(CONFIG_CC9P9210)
				APPEND_CMDLINE( "physmap-flash.0" );
#else
				APPEND_CMDLINE( "onboard_boot" );
#endif
			else {
                                char szBuffer[ 20 ];
                                sprintf( szBuffer, "device_%u", pPart->uiChip );
                                APPEND_CMDLINE( szBuffer );
                        }
                        APPEND_CMDLINE( ":" );
                }
                /* size */
                if( pPart->ullSize ) {
                        sprintf( acTmp, "0x%" PRINTF_QUAD "x", pPart->ullSize );
                        APPEND_CMDLINE( acTmp );
                } else
                        APPEND_CMDLINE( "-" );
                /* start */
                if( pPart->ullStart ) {
                        sprintf( acTmp, "@0x%" PRINTF_QUAD "x", pPart->ullStart );
                        APPEND_CMDLINE( acTmp );
                }

                /* name */
                APPEND_CMDLINE( "(" );
                APPEND_CMDLINE( pPart->szName );
                APPEND_CMDLINE( ")" );

                /* flags */
                if( pPart->flags.bReadOnly )
                        APPEND_CMDLINE( "ro" );
        } /* for( i = 0) */

        return 1;

error:
        return 0;
}

/*! \brief Appends root=/dev/mtdblock??? and filesystem info to szCmdLine */
int PartStrAppendRoot( char* szCmdLine, size_t iMaxSize )
{
        nv_critical_t* pCrit = NULL;
        int iRootFSPart      = -1;
        int i;
        const nv_param_part_table_t* pPartTable;

        if( !CW( NvCriticalGet( &pCrit ) ) )
                return 0;

        pPartTable = &pCrit->s.p.xPartTable;

        /* find first rootfs partition */
        for( i = 0; i < pPartTable->uiEntries; i++ ) {
                const nv_param_part_t* pPart = &pPartTable->axEntries[ i ];
                if( ( NVPT_FILESYSTEM == pPart->eType ) && pPart->flags.fs.bRoot ) {
                        iRootFSPart = i;
                        break;
                }
        } /* for( i = 0; ) */

        /* append rootfs parameters for root partition */
        if( -1 != iRootFSPart ) {
                const nv_param_part_t* pPart = &pPartTable->axEntries[ iRootFSPart ];
                char szBuffer[ 30 ];
                const char* szFSType = NULL;
                char bOnMTD = 0;

                switch( pPart->flags.fs.eType ) {
                    case NVFS_JFFS2:
                        bOnMTD   = 1;
                        szFSType = "jffs2";
                        break;
		    case NVFS_SQUASHFS:
			bOnMTD   = 1;
			szFSType = "squashfs";
			break;
                    case NVFS_CRAMFS:
                        bOnMTD   = 1;
                        szFSType = "cramfs";
                        break;
                    case NVFS_INITRD:
                        bOnMTD   = 0;
                        szFSType = "cramfs";  /* we use cramfs instead of ext2, writing makes no sense on initrd*/
                        break;
		    case NVFS_ROMFS:
			bOnMTD   = 1;
			szFSType = "romfs";
			break;
                    default:
                        eprintf( "*** Unsupported filesystem 0x%x",
                                 pPart->flags.fs.eType );
                }

                APPEND_CMDLINE( "root=/dev/" );

                if( bOnMTD )
                        sprintf( szBuffer, "mtdblock%i",
                                 iRootFSPart );
                else
                        sprintf( szBuffer, "ram" );
                APPEND_CMDLINE( szBuffer );

                if( NULL != szFSType ) {
                        APPEND_CMDLINE( " rootfstype=" );
                        APPEND_CMDLINE( szFSType );
                }

                APPEND_CMDLINE( pPart->flags.fs.bMountReadOnly ? " ro" : " rw" );
        } /* if( -1 != iRootFSPart */

        return 1;
}

#undef APPEND_CMDLINE

/* ********** local functions ********** */

static int PartCmdAppend( const struct cmd_info* pCmdInfo )
{
        nv_param_part_t xPart;
        uint64_t        ullSizeLast = 0;

        if( l_pPartTable->uiEntries == ARRAY_SIZE( l_pPartTable->axEntries ) ) {
                printf( "*** Partition table full ***\n" );
                goto error;
        }

        if( l_pPartTable->uiEntries ) {
                /* check that we have room */
                /* !TODO. Add multiple chip support */
                nv_param_part_t* pPartLast = &l_pPartTable->axEntries[ l_pPartTable->uiEntries - 1 ];
                int iChip = PartGetChip( pPartLast );
                uint64_t ullSize;

                CE( iChip >= 0 );

                ullSize = MtdSize( iChip );
                ullSizeLast = pPartLast->ullSize;
                while( !ullSizeLast ||
                       ( ( pPartLast->ullStart + ullSizeLast ) >= ullSize ) ) {
                        /* allow only sizes that don't go beyond chip size */
                        char szBuffer[ 80 ];

                        printf( "Last partition %i had already maximum size.\n",
                                l_pPartTable->uiEntries - 1 );

                        sprintf( szBuffer, "  Size  (in MiB, 0 for auto, %s max) ",
                                 NvToStringSize64( ullSize - pPartLast->ullStart ) );

                        CE( PartGetSize( szBuffer, &ullSizeLast, 1 ) );
                        l_pPartTable->axEntries[ l_pPartTable->uiEntries - 1 ].ullSize = ullSizeLast;
                }
        } /* if( l_pPartTable->uiEntries */

        CLEAR( xPart );

        printf( "Adding partition # %i\n", l_pPartTable->uiEntries );

        CE( PartModifyProperties( &xPart, 0, l_pPartTable->uiEntries ) );

        CE( NvCriticalPartAdd( &xPart ) );

        printf( "Partition %u added\n", l_pPartTable->uiEntries - 1 );

        return 1;

error:
        return 0;
}

static int PartCmdDelete( const struct cmd_info* pCmdInfo )
{
        uint32_t uiPartition = 0xffff;
        uint8_t  bMoved = 0;
        const nv_param_part_t* pPart = NULL;

        CE( PartSelectPart( "Delete", &uiPartition ) );

        pPart = &l_pPartTable->axEntries[ uiPartition ];
        if( pPart->flags.bFixed ) {
                printf( "*** Partition is fixed, can't be deleted ***\n" );
                return 1;
        }

        CE( NvCriticalPartDelete( uiPartition ) );

        if( uiPartition < l_pPartTable->uiEntries )
                /* there exist partitions behind us */
                CE( PartAutoAdjust( uiPartition, &bMoved ) );

        PartCmdPrintPartTable( NULL );

        printf( "Partition %u deleted, start addresses%s adjusted\n",
                uiPartition,
                ( bMoved ? "" : " not" ) );

        /* !TODO. Add auto-adjust */
        return 1;

error:
        return 0;
}

static int PartCmdModify( const struct cmd_info* pCmdInfo )
{
        uint32_t uiPartition = 0xffff;
        nv_param_part_t xPart;

        CE( PartSelectPart( "Modify", &uiPartition ) );

        /* rmw of partition */
        xPart = l_pPartTable->axEntries[ uiPartition ];
        CE( PartModifyProperties( &xPart, 1, uiPartition ) );
        l_pPartTable->axEntries[ uiPartition ] = xPart;

        if( ( uiPartition < ( l_pPartTable->uiEntries - 1 ) ) &&
            ( ( xPart.ullStart + xPart.ullSize ) !=
              l_pPartTable->axEntries[ uiPartition + 1 ].ullStart ) ) {
                /* adjust partitions to fit behind changed entry */
                uint8_t bMoved = 0;

                CE( PartAutoAdjust( uiPartition + 1, &bMoved ) );
                if( bMoved )
                        printf( "Start addresses adjusted\n" );
        }

        PartCmdPrintPartTable( NULL );

        return 1;

error:
        return 0;
}

static int PartCmdReset( const struct cmd_info* pCmdInfo )
{
        l_eOSType = NVOS_UNKNOWN;
        printf( "  OS Types: \n" );
        CE( PartCmdTableLoop( "OS", l_axCmdOSType, ARRAY_SIZE( l_axCmdOSType ), 1, -1 ) );

        CE( NvCriticalPartReset( l_eOSType ) );

        printf( "Partition table reset\n" );
        PartCmdPrintPartTable( NULL );

        return 1;

error:
        return 0;
}

static int PartCmdPrintPartTable( const struct cmd_info* pCmdInfo )
{
        int i;
        int iPrinted     = 0;
        int iMaxNameLen  = 0;
        int iMaxPTLen    = 0;
        int iMaxFSLen    = 0;
        char bAllOnChip0 = 1;

        /* determine max sizes for best fit on screen */
        for( i = 0; i < l_pPartTable->uiEntries; i++ ) {
                const nv_param_part_t* pPart = &l_pPartTable->axEntries[ i ];
                iMaxNameLen = MAX( iMaxNameLen, strlen( pPart->szName ) );
                iMaxPTLen   = MAX( iMaxPTLen, strlen( NvToStringPart( pPart->eType ) ) );
                iMaxFSLen   = MAX( iMaxFSLen, strlen( NvToStringFS( pPart->flags.fs.eType ) ) );
                bAllOnChip0 &= (!pPart->uiChip);
        }

        /* print header */
        printf( "Nr | Name%-*s |%s Start     | Size       | Type%-*s | FS%-*s | Flags%n\n",
                iMaxNameLen - 4 /* strlen ("Name") */ , "", /* for * alignment */
                ( bAllOnChip0 ? " " : " Chip |" ),
                iMaxPTLen - 4 /* strlen( "Type" ) */, "", /* for * alignment */
                iMaxFSLen - 2 /* strlen( "FS" ) */, "", /* for * alignment */
                &iPrinted );

        /* print separator */
        while( iPrinted-- )
                printf( "-" );
        printf( "\n" );

        for( i = 0; i < l_pPartTable->uiEntries; i++ ) {
                /* print one partition entry */
                const nv_param_part_t* pPart = &l_pPartTable->axEntries[ i ];
                char szTmp1[ 20 ];
                char szTmp2[ 20 ];

                printf( "%2i | %-*s | ", i, iMaxNameLen, pPart->szName );
                if( !bAllOnChip0 )
                        printf( " %-3i | ", pPart->uiChip );

                /* they use a static buffer, can't have two calls in one printf */
                strcpy( szTmp1, NvToStringSize64( pPart->ullStart ) );
                strcpy( szTmp2, NvToStringSize64( pPart->ullSize ) );

                printf( "%10" PRINTF_QUAD "s | %10" PRINTF_QUAD "s | %-*s | %-*s | ",
                        szTmp1,
                        szTmp2,
                        iMaxPTLen, NvToStringPart( pPart->eType ),
                        iMaxFSLen, ( ( NVPT_FILESYSTEM == pPart->eType ) ?
                                     NvToStringFS( pPart->flags.fs.eType ) :
                                     "" ) );

                if( pPart->flags.bFixed )
                        printf( "fixed " );
                if( pPart->flags.bReadOnly )
                        printf( "readonly " );

                if( NVPT_FILESYSTEM == pPart->eType ) {
                        /* this information is only used for filesysystems.
                           They are not unset on modify, to easily switch back
                        */
                        /* they don't make sense if not FS, but print them always */
                        if( pPart->flags.fs.bMountReadOnly )
                                printf( "mounted readonly " );
                        if( pPart->flags.fs.bRoot )
                                printf( "rootfs " );

                        if( pPart->flags.fs.uiVersion )
                                printf( " FS Version: %u",
                                        pPart->flags.fs.uiVersion );
                } /* if( NVPT_FILESYSTEM ) */

                printf( "\n" );
        } /* for( i = 0; */

        return 1;
}

static int PartCmdPTSelect( const struct cmd_info* pCmdInfo )
{
        l_ePartType = (nv_part_type_e) pCmdInfo->iInfo;
        return 1;
}

static int PartCmdFSSelect( const struct cmd_info* pCmdInfo )
{
        l_eFSType = (nv_part_type_e) pCmdInfo->iInfo;
        return 1;
}

static int PartCmdOSSelect( const struct cmd_info* pCmdInfo )
{
        l_eOSType = (nv_os_type_e) pCmdInfo->iInfo;
        return 1;
}

/*! \brief processes user input on pCmdTable */
/*! \return 0 on failure otherwise 1 */
static int PartCmdTableLoop(
        const char* szWhat,
        const struct cmd_info* pCmdInfoTable, size_t iSize,
        char bOnlyOnce, int iInit )
{
        int iRes;

        if( iInit < 0 )
                /* so we know what we can enter */
                PartCmdTablePrint( pCmdInfoTable, iSize );

        do {
                char bFound = 0;
                char cKey;
                int i;
                const char* szDescr = "";
                const cmd_info_t* pCmdInfo = NULL;
                const cmd_info_t* pDfltCmdInfo = NULL;

                iRes = 1;

                if( iInit >= 0 ) {
                        /* find command for that key */
                        for( i = 0, pCmdInfo = pCmdInfoTable; i < iSize;
                             i++, pCmdInfo++ ) {
                                if( iInit == pCmdInfo->iInfo ) {
                                        /* found */
                                        pDfltCmdInfo = pCmdInfo;
                                        szDescr      = pDfltCmdInfo->szDescr;
                                        break;
                                }
                        } /* for( i = 0 */
                        printf( "%s (%s, ? for help)> ", szWhat, szDescr );
                } else /* if( iInit */
                        printf( "%s (? for help)> ", szWhat );

                while( !tstc() ) {
                        /* wait for key to be pressed */
                }

                cKey = getc();
                if( CTRL_C == cKey ) {
                        l_bAbort = 1;
                        iRes = 0;
                        break;
                } else if( ( '\r' == cKey ) && ( NULL != pDfltCmdInfo ) ) {
                        /* use initial one */
                        printf( "\n" );
                        pDfltCmdInfo->pfFunc( pDfltCmdInfo );
                        break;
                }

                printf( "%c\n", cKey );

                /* find command for that key */
                for( i = 0, pCmdInfo = pCmdInfoTable; i < iSize;
                     i++, pCmdInfo++ ) {
                        if( cKey == pCmdInfo->cKey ) {
                                /* found */
                                bFound = 1;
                                break;
                        }
                } /* for( i = 0 */

                if( bFound ) {
                        if( NULL == pCmdInfo->pfFunc )
                                /* NULL is exit */
                                break;
                        else {
                                /* execute func */
                                iRes = pCmdInfo->pfFunc( pCmdInfo );
                                CW( iRes );
                                if( iRes && bOnlyOnce )
                                        break;
                               /* l_bAbort may be set */
                        }
                } else
                        /* anything else */
                        PartCmdTablePrint( pCmdInfoTable, iSize );
	} while( !l_bAbort && ( iRes || !bOnlyOnce ) );

        return iRes;
}

static void PartCmdTablePrint( const struct cmd_info* pCmdInfoTable, size_t iSize )
{
        int i;

        for( i = 0; i < iSize; i++, pCmdInfoTable++ )
                printf( "   %c) %s\n",
                        pCmdInfoTable->cKey,
                        pCmdInfoTable->szDescr );
}

static int PartAutoAdjust( unsigned int uiPartition, uint8_t* pbMoved )
{
        uint8_t bAutoAdjust = 1;
        uint8_t bAnyFixedAfter = 0;
        uint32_t u;

        *pbMoved = 0;

        /* are we allowed to change the partitions? */
        for( u = uiPartition; u < l_pPartTable->uiEntries; u++ )
                bAnyFixedAfter |= l_pPartTable->axEntries[ u ].flags.bFixed;

        if( !bAnyFixedAfter ) {
                CE( PartGetBool( "  Auto-Adjust start of succeeding partition? ",
                                 &bAutoAdjust, 1 ) );

                if( bAutoAdjust ) {
                        uint64_t ullStart = 0;

                        /* move existing partitions so they follow each other*/
                        if( uiPartition )
                                ullStart = l_pPartTable->axEntries[ uiPartition - 1 ].ullStart + l_pPartTable->axEntries[ uiPartition -1 ].ullSize;

                        for( u = uiPartition; u < l_pPartTable->uiEntries; u++ ) {
                                if( ( ullStart + l_pPartTable->axEntries[ u ].ullSize ) > MtdSize( PartGetChip( &l_pPartTable->axEntries[ u ] ) ) ) {
                                        eprintf( "*** Warning: Partition %u would expand behind chip size, don't adjusting it \n", u );
                                        break;
                                }

                                l_pPartTable->axEntries[ u ].ullStart = ullStart;
                                ullStart += l_pPartTable->axEntries[ u ].ullSize;
                                *pbMoved = 1;
                        }
                }
        } else {
                printf( "*** Can't adjust start addresses because of fixed partitions ***\n" );
        }

        return 1;

error:
        return 0;
}

static int PartModifyProperties(
        struct nv_param_part* pPart,
        char bPrintCurrent,
        int iIndex )
{
        int      iChip = PartGetChip( pPart );
        uint64_t ullSize;
	uint64_t erase_block;
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
	struct nand_chip *chip = nand_info[0].priv;
#endif
#if (CONFIG_COMMANDS & CFG_CMD_FLASH)
	extern flash_info_t flash_info[];
#endif
        if( iChip < 0 )
                return 0;

        ullSize = MtdSize( iChip );
        if( pPart->flags.bFixed ) {
                uint8_t bModify = 0;

                CE( PartGetBool( "Partition is marked fixed, do you still"
				       " want to modify it? ", &bModify, 0 ) );

                if( !bModify )
                        return 1;
        }

        printf( "  Name  " );
        CE( getsn( pPart->szName, ARRAY_SIZE( pPart->szName ), 1 ) );
        CE( PartGetUInt( "  Chip  ", &pPart->uiChip, 1 ) );
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
	if(chip->numchips <= pPart->uiChip) {
#endif
#if (CONFIG_COMMANDS & CFG_CMD_FLASH)
	if( (pPart->uiChip + 1) > CFG_MAX_FLASH_BANKS ||
		flash_info[pPart->uiChip].size < 2) {
#endif
		printf("Invalid chip\n");
		return 0;
	}

        /* enter start address */
        do {
                /* allow only start addresses < chip size */
                CE( PartGetSize( "  Start (in MiB, 0 for auto) ",
                                 &pPart->ullStart, 1 ) );
		/* allow only start addresses which are
		 * aligened to erase blocks */
		erase_block = MtdGetEraseSize(iChip, pPart->ullStart);
		if (pPart->ullStart % erase_block)
			printf("Must be a multiple of Flash erase block size (%lu KiB)\n",
				(erase_block >> 10));
        } while( pPart->ullStart >= ullSize ||
		 pPart->ullStart % erase_block );

        if( iIndex > 0 ) {
                if( !pPart->ullStart ) {
                        /* auto-calc start */
                        nv_param_part_t* pPartLast = &l_pPartTable->axEntries[ iIndex - 1 ];

                        if( pPart->uiChip == pPartLast->uiChip ) {
                                /* same chip, place us behind */
                                pPart->ullStart = pPartLast->ullStart + pPartLast->ullSize;
				erase_block = MtdGetEraseSize(iChip, pPart->ullStart);
				/* align to erase blocks */
				if (pPart->ullStart % erase_block)
					pPart->ullStart += erase_block -
						(pPart->ullStart % erase_block);
				/* Check that it fits in the flash */
				if (pPart->ullStart >= ullSize) {
					printf("Calculated address is out of bounds\n");
					return 0;
				}
                                printf( "   --> Set to %s\n", NvToStringSize64( pPart->ullStart ) );
                        }
                }

                if( ( l_pPartTable->axEntries[ iIndex - 1 ].ullStart + l_pPartTable->axEntries[ iIndex - 1 ].ullSize ) > pPart->ullStart ) {
                        eprintf( "*** Warning: Partition %u intersects partition %u\n",
                                 iIndex, iIndex - 1 );
                }
        }
        /* enter size */
        do {
                /* allow only sizes that don't go beyond chip size */
                char szBuffer[ 80 ];

                sprintf( szBuffer, "  Size  (in MiB, 0 for auto, %s max) ",
                         NvToStringSize64( MtdSize( iChip ) - pPart->ullStart ) );

                CE( PartGetSize( szBuffer, &pPart->ullSize, 1 ) );
		if (pPart->ullSize % erase_block)
			printf("Must be a multiple of Flash erase block size (%lu KiB)\n",
				(erase_block >> 10));
        } while( pPart->ullSize > ( ullSize - pPart->ullStart ) ||
		pPart->ullSize % erase_block );

        if( !pPart->ullSize ) {
                /* write the fixed size, makes reading it easier */
                pPart->ullSize = ullSize - pPart->ullStart;
                printf( "   --> Set to %s\n", NvToStringSize64( pPart->ullSize ) );
        }

        l_ePartType = pPart->eType;
        printf( "  Partition Types\n" );
        CE( PartCmdTableLoop( "Partition Type",
                              l_axCmdPartType, ARRAY_SIZE( l_axCmdPartType ), 1, l_ePartType ) );
        pPart->eType = l_ePartType;

        CE( PartGetBool( "  Fixed  ", &pPart->flags.bFixed, 1 ) );

        CE( PartGetBool( "  Readonly  ", &pPart->flags.bReadOnly, 1 ) );

        if( NVPT_FILESYSTEM == pPart->eType ) {
                l_eFSType = pPart->flags.fs.eType;
                printf( "  Filesystem Types \n" );
                CE( PartCmdTableLoop( "Filesystem", l_axCmdFSType, ARRAY_SIZE( l_axCmdFSType ), 1, l_eFSType ) );
                pPart->flags.fs.eType = l_eFSType;

                CE( PartGetBool( "  Root-FS  ", &pPart->flags.fs.bRoot, 1 ) );

                CE( PartGetBool( "  Mount Readonly  ",
                                 &pPart->flags.fs.bMountReadOnly, 1 ) );

                /* we skip version as it is not used yet and can be
                 * modified with intnvram */
        }

        return 1;

error:
        return 0;
}

static int PartSelectPart( const char* szWhat, /*@out@*/ uint32_t* puiPart )
{
        char szMsg[ 64 ] = "";

        if( !l_pPartTable->uiEntries ) {
                printf( "*** No partitions available ***\n" );
                goto error;
        }

        sprintf( szMsg, "%s Which Partition? ", szWhat );

        do {
                CE( PartGetUInt( szMsg, puiPart, 0 ) );
                if( *puiPart >= l_pPartTable->uiEntries )
                        printf( "*** Partition %u not available ***\n",
                                *puiPart );
        } while( *puiPart >= l_pPartTable->uiEntries );

        return 1;
error:
        return 0;
}

static int PartGetSize( const char* szMsg, uint64_t* pullVal, char bModify )
{
        int iRes = 0;

        do {
                char szBuffer[ 20 ] = "";

                printf( "%s", szMsg );

                if( bModify )
                        strncpy( szBuffer, NvToStringSize64( *pullVal ), ARRAY_SIZE( szBuffer ) );
                if( getsn( szBuffer, ARRAY_SIZE( szBuffer ), bModify ) )
                        iRes = NvToSize64( pullVal, szBuffer );
                else
                        printf( "\n" );

                if( !iRes && !l_bAbort )
                        printf( "*** Wrong value, try again\n" );
        } while( !iRes && !l_bAbort );

        return iRes;
}

static int PartGetUInt( const char* szMsg, uint32_t* puiVal, char bModify )
{
        int iRes = 0;

        do {
                char szBuffer[ 20 ] = "";

                printf( "%s", szMsg );

                if( bModify )
                        sprintf( szBuffer, "%u", *puiVal );
                if( getsn( szBuffer, ARRAY_SIZE( szBuffer ), bModify ) )
                        iRes = ( 1 == sscanf( szBuffer, "%u", puiVal ) );
                else
                        printf( "\n" );

                if( !iRes && !l_bAbort )
                        printf( "*** Wrong value, try again\n" );
        } while( !iRes && !l_bAbort );

        return iRes;
}

static int PartGetBool( const char* szMsg, uint8_t* pbVal, char bModify )
{
        int iRes = 0;

        do {
                char szBuffer[ 20 ] = "";

                printf( "%s", szMsg );

                if( bModify )
                        sprintf( szBuffer, "%s", *pbVal ? "y" : "n" );
                if( getsn( szBuffer, ARRAY_SIZE( szBuffer ), bModify ) ) {
                        char szTmp[ 4 ];
                        if( sscanf( szBuffer, "%4s", &szTmp ) == 1 ) {
                                if( !strcmp( "y", szBuffer ) ) {
                                        *pbVal = 1;
                                        iRes   = 1;
                                } else if( !strcmp( "n", szBuffer ) ) {
                                        *pbVal = 0;
                                        iRes   = 1;
                                }
                        }
                } else
                        printf( "\n" );

                if( !iRes && !l_bAbort )
                        printf( "*** Wrong value, try again\n" );
        } while( !iRes && !l_bAbort );

        return iRes;
}

/*! \brief returns a string of max iSize.*/
/*! if ePartType is
 *  NVPT_FILESYSTEM, also eFSType is used for search.
 * \param szOut     output string
 * \param iSize max size with 0
 * \param bModify if 1, s is reused
 * \return 0 on Ctrl-C otherwise 1
 */
static int getsn( char* szOut, size_t iSize, char bModify )
{
        int   i       = 0;
        int   iRes    = 1;
        char  bBreak  = 0;
        char  acBuffer[ 256 ];
        char* pcStart = acBuffer;
        char* s       = pcStart;

        acBuffer[ 0 ] = 0;

        if( bModify )
                printf( "(%s): ", szOut );

        while( !bBreak ) {
                char cKey;

                while( !tstc() ) {
                        /* wait for key to be pressed */
                }

                cKey = getc();

                switch( cKey ) {
                    case '\r':
                        /* enter */
                        *s     = 0;
                        bBreak = 1;
                        break;

                    case CTRL_C:
                        *pcStart = 0;
                        bBreak   = 1;
                        l_bAbort = 1;
                        iRes     = 0;
                        break;

                    case 0x7f:
                        /* backspace */
                        printf( "\b \b" );
                        /* backspace */
                        if( i ) {
                                i--;
                                s--;
                        }
                        break;

                    default:
                        printf( "%c", cKey );
                        *s = cKey;
                        i++;
                        s++;
                        break;
                }

                if( i == ( iSize - 1 ) ) {
                        /* terminate it */
                        *(s-1) = 0;
                        bBreak = 1;
                }
        }

        if( iRes ) {
                if( !bModify || ( s != pcStart ) )
                        strncpy( szOut, acBuffer, iSize );
                else
                        printf( "%s", szOut );

                /* if bModify and nothing entered, take default */
                printf( "\n" );
        }

        return iRes;
}

/*! \return the chip for the flash partition */
static int PartGetChip( const struct nv_param_part* pPart )
{
        return pPart->uiChip;
}

/*! \brief throttles output to avoid to many serial messages */
/*! If we have much to read, print more messages. If we have little to read,
 *  just print a few lines. */
static int PartGetThrottle(
        const struct nv_param_part* pPart,
        uint64_t ullSize )
{
        int iChip   = PartGetChip( pPart );
        int iBlocks = ullSize / MtdGetEraseSize( iChip, 0 );  /* not exact for top/bottom sectors */
        int iThrottle;

        /* just some numbers, finetuned for linux  */
        if( iBlocks > 80 )
                iThrottle = 10;
        else if( iBlocks > 20 )
                iThrottle = 20;
        else
                iThrottle = 50;

        return iThrottle;
}

/*! \brief print progress if changed */
static void PrintProgress( int iPercentage, int iThrottle,
                           const char* szFmt, ... )
{
        static int iLastPercentage = -1;
        int iThrottled = iPercentage / iThrottle;

        if( iThrottled != iLastPercentage ) {
                va_list args;

                iLastPercentage = iThrottled;

                va_start( args, szFmt );
                vprintf( szFmt, args );
                printf( "% 3i%%          \r", iPercentage );
                va_end( args );
        }
}

