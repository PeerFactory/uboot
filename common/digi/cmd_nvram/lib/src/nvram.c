/*
 *  nvram/lib/src/nvram.c
 *
 *  Copyright (C) 2006 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
 */
/*
 *  !Revision:   $Revision$: 
 *  !Author:     Markus Pietrek
 *  !Descr:      Provides the NVRAM functions to access MAC, IP etc.
 */

#include "nvram.h"
#include "nvram_priv.h"
#include "nvram_version.h"
#define memcpy32(a,b,c)	memcpy(a,b,c)

#define NV_RESERVED_CRITICAL_SIZE 2048

#define NVRAM "NVRAM: "

#define FREE( pvPtr ) \
        do { \
                free( pvPtr );                  \
                pvPtr = NULL;                   \
        } while( 0 )

/* ********** local function declarations ********** */

static int NvPrivImagesRead( void );
static int NvPrivImageWrite( nv_critical_loc_e eType );
static int NvPrivImageRead( nv_critical_loc_e eType, /*@out@*/ void* pvTmp );
static int NvPrivImageUpdate( nv_critical_loc_e eImage );
static int NvPrivCriticalRead( /*@out@*/ nv_critical_t* pParamCrit,
                               nv_critical_loc_e eType );
static int NvPrivFlashDetermineOffs( nv_critical_loc_e eType,
                                     /*@out@*/ loff_t* piOffs );
static int NvPrivFlashProtectAll( void );

static int NvPrivFindInTable(
        /*@out@*/ int* piIdx,
        const char* szWhat, const char** aszTable,
        size_t iSize );

/* ********** local variables ********** */

#define MK(x) IDX(x) #x
static const char* l_aszErrors[] = {
        MK( NVE_GOOD ),
        MK( NVE_NO_DEV ),
        MK( NVE_IO ),
        MK( NVE_CRC ),
        MK( NVE_MIRROR ),
        MK( NVE_NO_SPACE ),
        MK( NVE_NOT_INITIALIZED ),
        MK( NVE_NOT_IMPLEMENTED ),
        MK( NVE_VERSION ),
        MK( NVE_MAGIC ),
        MK( NVE_MATCH ),
        MK( NVE_INVALID_ARG ),
        MK( NVE_WRONG_VALUE ),
        MK( NVE_NOT_FOUND ),
};
#undef MK

/* string tables with more human readable code */
#define MK(x,y) IDX(x) y

static const char* l_aszCritLoc[] = {
        MK( NVCL_ORIGINAL, "Original"  ),
        MK( NVCL_MIRROR,   "Mirror"    ),
        MK( NVCL_WORKCOPY, "Workcopy"  ),
};

static const char* l_aszPartTypes[] = {
        MK( NVPT_UBOOT,      "U-Boot" ),
        MK( NVPT_NVRAM,      "NVRAM" ),
        MK( NVPT_FPGA,       "FPGA" ),
        MK( NVPT_LINUX,      "Linux-Kernel" ),
        MK( NVPT_EBOOT,      "WinCE-EBoot" ),
        MK( NVPT_WINCE,      "WinCE-Kernel" ),
        MK( NVPT_NETOS,      "NET+OS-Kernel" ),
        MK( NVPT_FILESYSTEM, "Filesystem" ),
        MK( NVPT_WINCE_REGISTRY, "WinCE-Registry" ),
        MK( NVPT_UNKNOWN,    "Unknown" ),
        MK( NVPT_SPLASH_SCREEN,"Splash-Screen" ),
        MK( NVPT_NETOS_LOADER, "NET+OS-Loader"  ),
        MK( NVPT_NETOS_NVRAM, "NET+OS-NVRAM"  ),
};

static const char* l_aszFSTypes[] = {
        MK( NVFS_NONE,    "None"    ),
        MK( NVFS_JFFS2,   "JFFS2"   ),
        MK( NVFS_SQUASHFS,"SQUASHFS"),
	MK( NVFS_CRAMFS,  "CRAMFS"  ),
        MK( NVFS_INITRD,  "INITRD"  ),
	MK( NVFS_ROMFS,   "ROMFS"   ),
        MK( NVFS_FLASHFX, "FlashFX" ),
        MK( NVFS_EXFAT,   "ExFAT"   ),
        MK( NVFS_UNKNOWN, "Unknown" ),
        MK( NVFS_YAFFS,   "YAFFS"   ),
};

static const char* l_aszOSTypes[] = {
        MK( NVOS_NONE,    "None"   ),
        MK( NVOS_CRITICAL,"Critical" ),
        MK( NVOS_OS_META, "OS-Meta" ),
        MK( NVOS_UBOOT,   "U-Boot" ),
        MK( NVOS_LINUX,   "Linux"  ),
        MK( NVOS_EBOOT,   "EBoot"  ),
        MK( NVOS_WINCE,   "WinCE"  ),
        MK( NVOS_NETOS,   "NET+OS" ),
        MK( NVOS_UNKNOWN, "Unknown"  ),
        MK( NVOS_APPL,    "Application"  ),
        MK( NVOS_USER_DEFINED,  "User defined" ),
	MK( NVOS_NETOS_LOADER, "NET+OS-Loader"  ),
        MK( NVOS_WCAL_DATA, "Wireless Calibration"  ),
};
#undef MK

static struct {
        nv_error_e  eType;
        const char* szFunc;
        const char* szFile;
        int         iLine;
        char        szBuf[ 64 ];
} l_xError;

char           g_bInitialized = 0;
nv_info_t      g_xInfo;
nv_critical_t* g_pWorkcopy = NULL;

/*!\brief stores one erase block */
static void*       l_pvWorkcopy   = NULL;
static void*       l_pvFlashInput = NULL;
static char        l_bOutputEnabled = 1;
static size_t      l_iEraseSize     = 0;
static nv_os_meta_t* l_pWorkcopyOSMeta = NULL;

#ifdef WINCE
# define strnicmp( s1, s2, len ) _strnicmp( s1, s2, len )
#endif  /* WINCE */

/* ********** global functions ********** */

/*! \brief initializes the NVRAM library */
/*! \return 0 on failure */
int NvInit( nv_repair_e eRepair )
{
        nv_priv_flash_status_t xStatus;
        int iRes = 0;

        if( g_bInitialized )
                NvFinish();

        /* some sanity checks, e.g. string table set up correctly */
        ASSERT( ARRAY_SIZE( l_aszErrors )    == NVE_LAST  );
        ASSERT( ARRAY_SIZE( l_aszPartTypes ) == NVPT_LAST );
        ASSERT( ARRAY_SIZE( l_aszFSTypes )   == NVFS_LAST );
        ASSERT( ARRAY_SIZE( l_aszOSTypes )   == NVOS_LAST );
        ASSERT( ARRAY_SIZE( l_aszCritLoc )   == NVCL_LAST );

        /* structure changes should be detected here */
        ASSERT( sizeof( nv_critical_t )           == 1328 );
        ASSERT( sizeof( nv_param_module_id_t )    == 64   );
        ASSERT( sizeof( nv_param_ip_t )           == 104  );
        ASSERT( sizeof( nv_param_part_table_t )   == 896  );
        ASSERT( sizeof( nv_param_part_t )         == 88   );
        ASSERT( sizeof( nv_param_os_cfg_table_t ) == 176  );
        ASSERT( sizeof( nv_param_os_cfg_t )       == 20  );
        ASSERT( sizeof( nv_os_meta_t )            == 128  );

        ASSERT( NV_RESERVED_CRITICAL_SIZE > sizeof( nv_critical_t ) );

        /* initialize static variables */
        CLEAR( l_xError );
        l_xError.eType = NVE_GOOD;

        CLEAR( g_xInfo );

        CE( NvPrivOSInit() );

        CE( NvPrivOSFlashOpen( 0 ) );
        iRes  = NvPrivOSFlashInfo( 0, &xStatus );
        l_iEraseSize = xStatus.iEraseSize;
        /* protect the whole NVRAM partition for unwanted access */
        iRes &= NvPrivFlashProtectAll();
        iRes &= NvPrivOSFlashClose();
        CE( iRes );

        /* we need a buffer to keep the workcopy page in NVRAM */
        l_pvWorkcopy = malloc( xStatus.iEraseSize );
        CE_SET_ERROR_EXT( NULL != l_pvWorkcopy, NVE_NO_SPACE, "malloc" );

        l_pWorkcopyOSMeta = (nv_os_meta_t*) l_pvWorkcopy;
        g_pWorkcopy       = (nv_critical_t*) ( l_pWorkcopyOSMeta + 1 );

        g_bInitialized = 1;

        if( NVR_IGNORE != eRepair ) {
                l_pvFlashInput = malloc( xStatus.iEraseSize );
                CE_SET_ERROR_EXT( NULL != l_pvFlashInput, NVE_NO_SPACE, "malloc (flash input)" );
                /* now we can read */
                CE( NvPrivImagesRead() );
                FREE( l_pvFlashInput );

                if( g_xInfo.bGood && g_xInfo.bAnyBad && ( eRepair == NVR_AUTO ) )
                        /* only when something is good and something is bad */
                        if(! NvPrivImageRepair() )
				eRepair = NVR_IGNORE;
        } 
	
	if( NVR_IGNORE == eRepair ) {
                /* the current content is ignored and contents not repaired */
                g_xInfo.bDefault = 1;
                CE( NvWorkcopyReset() );
                CE( NvPrintStatus() );
        }

        CE( NvPrivOSPostInit() );

        return 1;

error:
        FREE( l_pvFlashInput );

        return 0;
}

/*! \brief closes the NVRAM library */
/*! \return 0 on failure */
int NvFinish( void )
{
        int iRes = 0;

        FREE( l_pvWorkcopy );

        iRes = NvPrivOSFinish();

        g_bInitialized = 0;

        return iRes;
}

/*! \brief enables the output to the console. If disabled, then output is lost
 */
/*! \return always 1 */
int NvEnableOutput( char bEnable )
{
        l_bOutputEnabled = bEnable;

        return 1;
}

nv_error_e NvError( void )
{
        return l_xError.eType;
}

/*! \brief returns the error code as a number and can set a more descriptive
           strings in pszError */
/*!
  \param  pszError will be set to error message if not NULL
  \return the last error code.
*/

/*@-mustdefine@*/
/* there is a bug in splint that doesn't detect that NULL ptr can't be
   assigned. */
extern nv_error_e NvErrorMsg(
        const char** pszError,
        const char** pszWhat,
        const char** pszFunc,
        const char** pszFile,
        int* piLine )
{
        if( NULL != pszError )
                /* eError is used internally. So we are always within limits */
                *pszError = l_aszErrors[ l_xError.eType ];

        if( NULL != pszWhat )
                *pszWhat = l_xError.szBuf;

        if( NULL != pszFunc )
                *pszFunc = l_xError.szFunc;

        if( NULL != pszFile )
                *pszFile = l_xError.szFile;

        if( NULL != piLine )
                *piLine = l_xError.iLine;

        return l_xError.eType;
}
/*@+mustdefine@*/

void NvGetVersion( uint32_t* puiVerMajor, uint32_t* puiVerMinor )
{
        *puiVerMajor = NVV_MAJOR;
        *puiVerMinor = NVV_MINOR;
}

const struct nv_info* NvInfo( void )
{
        return ( g_bInitialized ? &g_xInfo : NULL );
}

/*! \brief Prints the status of the NVRAM. */
/*! May be used in delayed printing, e.g. in U-Boot, because baudrate in NVRAM
 *  may differ from default. */
int NvPrintStatus( void )
{
        if( !g_xInfo.bDefault ) {
                /* the flash contents is available */
                if( !g_xInfo.bOriginalGood )
                        NvPrivOSPrintfError( NVRAM "Original Critical is BAD\n" );
                if( !g_xInfo.bMirrorGood )
                        NvPrivOSPrintfError( NVRAM "Mirror Critical is BAD\n" );
                if( !g_xInfo.bFlashOriginalGood )
                        NvPrivOSPrintfError( NVRAM "Original Flash is BAD\n" );
                if( !g_xInfo.bFlashMirrorGood )
                        NvPrivOSPrintfError( NVRAM "Mirror Flash is BAD\n" );
                if( !g_xInfo.bFlashMirrorMatch )
                        NvPrivOSPrintfError( NVRAM "Original Flash and Mirror Flash don't match\n" );
                if( !g_xInfo.bMirrorMatch )
                        NvPrivOSPrintfError( NVRAM "Original and Mirror don't match\n" );
                if( !g_xInfo.bFlashMirrorMatch )
                        NvPrivOSPrintfError( NVRAM "Original and Mirror Flash don't match\n" );

                if( !g_xInfo.bGood )
                        NvPrivOSPrintfError( NVRAM "No NVRAM contents usable\n" );
                else if( !g_xInfo.bOriginalGood ) 
                        NvPrivOSPrintfError( NVRAM "Using Mirror\n" );
        } else
                NvPrivOSPrintfError( NVRAM "Using defaults\n" );

        return 1;
}

/*! Both, mirror and original are written with this algorithm.
 *  a) No mirror present
       Write original and verify it.
    b) original is good or bad and mirror is good
       At first write original and verify it. If it is OK, do same for mirror
       If original failed, don't touch mirror.
    c) original is good and mirror is bad.
       At first write mirror and verify it. If it is OK, do same for original
       If mirror failed, don't touch original.
*/
int NvSave( void )
{
        nv_critical_loc_e eFirstToWrite = NVCL_ORIGINAL;

        REQUIRE_INITIALIZED();

        NvPrivOSPrintf( "Writing Parameters to NVRAM\n" );

        CE( NvWorkcopyUpdateCRC32() );

        if( g_xInfo.bFlashOriginalGood &&
            !g_xInfo.bFlashMirrorGood )
                eFirstToWrite = NVCL_MIRROR;

        CE( NvPrivImageUpdate( eFirstToWrite ) );
        CE( NvPrivImageUpdate( ( ( NVCL_ORIGINAL == eFirstToWrite ) ?
                                 NVCL_MIRROR : NVCL_ORIGINAL ) ) );

        /* only reached when written correctly */
        g_xInfo.bGood              = 1;
        g_xInfo.bAnyBad            = 0;
        g_xInfo.bFlashOriginalGood = 1;
        g_xInfo.bFlashMirrorGood   = 1;
        g_xInfo.bFlashMirrorMatch  = 1;
        g_xInfo.bOriginalGood      = 1;
        g_xInfo.bMirrorGood        = 1;
        g_xInfo.bMirrorMatch       = 1;
        g_xInfo.bOSGood            = 1;
        g_xInfo.bOSOriginalGood    = 1;
        g_xInfo.bDefault           = 0;  /* not any longer */

        return 1;

error:
        return 0;
}

/*! Updates the internals checksum */
/*  It is only used to NvWorkcopyUpdateCRC32 on the outside,
   when the workcopy in RAM is used instead of interchanged OS,
   e.g.
*/
int NvWorkcopyUpdateCRC32( void )
{
        REQUIRE_INITIALIZED();

        /* calculate CRC32 */
        g_pWorkcopy->uiCRC32 = crc32( 0,
                                      (const unsigned char*) &g_pWorkcopy->s,
                                      sizeof( g_pWorkcopy->s ) );

        l_pWorkcopyOSMeta->uiCRC32 = crc32( 0, (const unsigned char*) (&l_pWorkcopyOSMeta->uiCRC32 + 1),
                                          l_iEraseSize - sizeof( l_pWorkcopyOSMeta->uiCRC32 ) );

        return 1;

error:
        return 0;
}

int NvWorkcopyReset( void )
{
        REQUIRE_INITIALIZED();

        NvPrivOSPrintfError( NVRAM "Resetting Workcopy\n" );

        /* FLASH is initialized to 0xff */
        memset( l_pvWorkcopy, 0xff, l_iEraseSize );

        /*  g_pWorkcopy may be part of l_pvWorkcopy if pure NAND,
            otherwise it is separated */
        CLEAR( *g_pWorkcopy );
        strncpy( g_pWorkcopy->s.szMagic,
                 NV_MAGIC_CRITICAL,
                 sizeof( g_pWorkcopy->s.szMagic ) );

        g_pWorkcopy->s.uiVerMajor = NVV_MAJOR;
        g_pWorkcopy->s.uiVerMinor = NVV_MINOR;
        g_xInfo.bGood = 1;

        /* set defaults */
        CE( NvToIP( &g_pWorkcopy->s.p.xIP.axDevice[ 0 ].uiIP,
                    "192.168.42.30" ) );
        CE( NvToIP( &g_pWorkcopy->s.p.xIP.axDevice[ 0 ].uiNetMask,
                    "255.255.255.0" ) );
        CE( NvToIP( &g_pWorkcopy->s.p.xIP.axDevice[ 1 ].uiIP,
                    "192.168.43.30" ) );
        CE( NvToIP( &g_pWorkcopy->s.p.xIP.axDevice[ 1 ].uiNetMask,
                    "255.255.255.0" ) );
        CE( NvToIP( &g_pWorkcopy->s.p.xIP.uiIPServer,
                    "192.168.42.1" ) );
        CE( NvToMAC( &g_pWorkcopy->s.p.xID.axMAC[ 0 ],
                     "00:04:F3:FF:FF:FA" ) );
        CE( NvToMAC( &g_pWorkcopy->s.p.xID.axMAC[ 1 ],
                     "00:04:F3:FF:FF:FB" ) );

        /* it must be immediately after NV_RESERVED_CRITICAL_SIZE */
        NvOSCfgAdd( NVOS_OS_META, sizeof( nv_os_meta_t ) );

        /* add our own critical parameters here */
        NvOSCfgAdd( NVOS_CRITICAL, NV_RESERVED_CRITICAL_SIZE );

        CE( NvPrivOSCriticalPartReset( g_pWorkcopy, NVOS_NONE ) );

#ifdef CFG_HAS_WIRELESS
        NvOSCfgAdd( NVOS_WCAL_DATA, sizeof( wcd_data_t ) );
#endif
        /* the image is clean now. But maybe the OS wants to define some
         * standard values. */
        CE( NvPrivOSCriticalPostReset( g_pWorkcopy ) );

        return 1;

error:
        return 0;
}

int NvCriticalGet(
        /*@out@*/ struct nv_critical** ppCritical )
{
        *ppCritical = g_pWorkcopy;

        return 1;
}

/*! \brief Locates the iCount'th partition table entry of ePartType.*/
/*! if ePartType is
 *  NVPT_FILESYSTEM, also eFSType is used for search.
 * \param pPart     where to store a copy of the partition table 
 * \param ePartType partition type to look forth.
 * \param eFSType   filesystem to look for if NVPT_FILESYSTEM. NVFS_NONE takes
 *                  any. 
 * \param bRoot     if 1, filesystem must be rootfs, too. Otherwise, also
 *                  non-roots are used.
 * \param iCount    (iCount + 1 )'th occurent
 * \return 1 if there is a JFFS2 partition in the partition table  */

int NvParamPartFind(
        const struct nv_param_part** ppPart,
        nv_part_type_e ePartType,
        nv_fs_type_e   eFSType,
        char           bRoot,
        int            iCount )
{
	unsigned int u = 0;	
        int iFound = 0;
        const nv_param_part_table_t* pPartTable = &g_pWorkcopy->s.p.xPartTable;

        REQUIRE_INITIALIZED();

	*ppPart = NULL;

        /* search the iCount'th partition entry */
        while( u < pPartTable->uiEntries ) {
                const nv_param_part_t* pPartEntry = &pPartTable->axEntries[ u];
                if( pPartEntry->eType == ePartType ) {
                        if( ( NVPT_FILESYSTEM != ePartType ) ||
                            ( ( ( eFSType == pPartEntry->flags.fs.eType ) ||
                                ( NVFS_NONE == eFSType ) ) &&
                              ( bRoot == pPartEntry->flags.fs.bRoot ) ) ) {
                                /* found one occurence */
                                if( iFound == iCount ) {
                                        /* it's the n'th occurence we look for */
                                        *ppPart = pPartEntry;
                                        return 1;
                                }
                                iFound++;
                        }
                } /* if( pPartEntry->eType */

                u++;
        } /* while( u ) */

        NV_SET_ERROR( NVE_NOT_FOUND, NvToStringPart( ePartType ) );

error:
        return 0;
}

int NvCriticalPartAdd( const struct nv_param_part* pNewPart )
{
        nv_param_part_table_t* pPartTable = &g_pWorkcopy->s.p.xPartTable;
        nv_param_part_t* pPart;

        REQUIRE_INITIALIZED();

        CE_SET_ERROR( pPartTable->uiEntries < ARRAY_SIZE( pPartTable->axEntries ),
                      NVE_NO_SPACE );

        pPart = &pPartTable->axEntries[ pPartTable->uiEntries ];

        /* add it */
        if( NULL == pNewPart ) {
                pPart->eType    = NVPT_UNKNOWN;
                CLEAR( *pPart );
        } else
                *pPart = *pNewPart;

        pPartTable->uiEntries++;

        return 1;

error:
        return 0;
}

int NvCriticalPartDelete( uint32_t uiPartition )
{
        nv_param_part_table_t* pPartTable = &g_pWorkcopy->s.p.xPartTable;

        REQUIRE_INITIALIZED();

        /* move partitions down */
        memcpy( &pPartTable->axEntries[ uiPartition ],
                &pPartTable->axEntries[ uiPartition + 1 ],
                (sizeof(pPartTable->axEntries[ 0 ] ) * ( ARRAY_SIZE( pPartTable->axEntries ) - uiPartition - 1 ) ) );
        /* reset last partition */
        CLEAR( pPartTable->axEntries[ ARRAY_SIZE( pPartTable->axEntries ) - 1 ] );
        pPartTable->uiEntries--;

        /* !TODO. Maybe we should move ullStart of all immediately following
         * partitions */
        return 1;

error:
        return 0;
}

int NvCriticalPartReset( nv_os_type_e eForOS )
{
        REQUIRE_INITIALIZED();

        g_pWorkcopy->s.p.xPartTable.uiEntries = 0;

        return NvPrivOSCriticalPartReset( g_pWorkcopy, eForOS );

error:
        return 0;
}

int NvOSCfgAdd( nv_os_type_e eType, size_t iSize )
{
        nv_param_os_cfg_t xCfg;
        nv_param_os_cfg_table_t* pOSTable = &g_pWorkcopy->s.p.xOSTable;
        nv_param_os_cfg_t* pCfg;
        uint32_t uiStart = 0;
        unsigned int u = 0;

        REQUIRE_INITIALIZED();

        /* check the sizes and the start addr */
        if( NvOSCfgFind( &xCfg, eType ) ) {
                /* partition already exists */
                if( iSize != xCfg.uiSize ) {
                        char szBuf[ 64 ];
                        sprintf( szBuf,
                                 "Can't set OS Configuration to %i bytes, it already exists with %i bytes\n",
                                 (int) iSize, xCfg.uiSize );
                        NV_SET_ERROR( NVE_NO_SPACE, szBuf );
                        goto error;
                }
        }

        CE_SET_ERROR( pOSTable->uiEntries < ARRAY_SIZE( pOSTable->axEntries ),
                      NVE_NO_SPACE );

        pCfg = &pOSTable->axEntries[ pOSTable->uiEntries ];

        /* determine start addr */

        /* append it after other configurations. */
        /* !TODO. Detect overlapps */
        for( u = 0; u < pOSTable->uiEntries; u++ )
                uiStart += pOSTable->axEntries[ u ].uiSize;

        CE_SET_ERROR_EXT( ( ( uiStart + iSize ) < l_iEraseSize ),
                          NVE_NO_SPACE,
                          "OS config beyond erase block" );

        /* add it */
        CLEAR( *pCfg );
        pCfg->eType   = eType;
        pCfg->uiStart = uiStart;
        pCfg->uiSize  = iSize;
        pOSTable->uiEntries++;

        return 1;

error:
        return 0;
}

int NvOSCfgGet( nv_os_type_e eType, void* pvBuf, size_t iMaxSize,
                size_t* piSize )
{
        nv_param_os_cfg_t xCfg;

        REQUIRE_INITIALIZED();

        CE( NvOSCfgFind( &xCfg, eType ) );
        CE_SET_ERROR( ( xCfg.uiSize <= iMaxSize ), NVE_NO_SPACE );
        *piSize = xCfg.uiSize;

        memcpy32( pvBuf, (char*) l_pvWorkcopy + xCfg.uiStart, xCfg.uiSize );
        *piSize = xCfg.uiSize;

        return 1;

error:
        return 0;
}

int NvOSCfgSet( nv_os_type_e eType, const void*  pvBuf,  size_t iSize )
{
        nv_param_os_cfg_t xCfg;

        REQUIRE_INITIALIZED();

        CE( NvOSCfgFind( &xCfg, eType ) );
        CE_SET_ERROR( ( xCfg.uiSize >= iSize ), NVE_NO_SPACE );

        memcpy32( (char*) l_pvWorkcopy + xCfg.uiStart, pvBuf, iSize );

        return 1;

error:
        return 0;
}

int NvOSCfgFind( /*@out@*/ struct nv_param_os_cfg* pCfg, nv_os_type_e eType )
{
        unsigned int u = 0;
        const nv_param_os_cfg_table_t* pOSTable = &g_pWorkcopy->s.p.xOSTable;

        REQUIRE_INITIALIZED();

        CLEAR( *pCfg );

        /* search the iCount'th partition entry */
        while( u < pOSTable->uiEntries ) {
                const nv_param_os_cfg_t* pOSEntry = &pOSTable->axEntries[ u ];
                if( pOSEntry->eType == eType ) {
                        /* it's the n'th occurence we look for */
                        *pCfg = *pOSEntry;
                        return 1;
                } /* if( pOSEntry */

                u++;
        } /* while( u ) */

        NV_SET_ERROR( NVE_NOT_FOUND, NvToStringOS( eType ) );

error:
        return 0;
}


/* ********** private functions ********** */

/*! \brief set's the error code */
/*!
  \param  eError the error code
  \return always 0 so it can be used like return NvPrivSetError()
*/
int NvPrivSetError( nv_error_e eError, const char* szWhat,
                    const char* szFunc,
                    const char* szFile, int iLine )
{
        l_xError.eType  = eError;
        l_xError.szFunc = szFunc;
        l_xError.szFile = szFile;
        strncpy( l_xError.szBuf, szWhat, sizeof( l_xError.szBuf ) );
        l_xError.iLine  = iLine;

        return 0;
}

/*! \brief returns whether output is enabled */
/*!
  \return on true, NvPrivOSPrintf shouldn't output anything.
*/
char NvPrivIsOutputEnabled( void )
{
        return l_bOutputEnabled;
}

const char* NvToStringMAC( const nv_mac_t xMAC )
{
        static char acBuffer[ 20 ];

        sprintf( acBuffer, "%02X:%02X:%02X:%02X:%02X:%02X",
                 xMAC.u.c[ 0 ],
                 xMAC.u.c[ 1 ],
                 xMAC.u.c[ 2 ],
                 xMAC.u.c[ 3 ],
                 xMAC.u.c[ 4 ],
                 xMAC.u.c[ 5 ] );

        return acBuffer;
}

int NvToMAC( nv_mac_t* pMAC, const char* szMAC )
{
        uint32_t auiMAC[ 6 ];
        int i;
	int format_ok = 0, consumed_chars = 0;

	/*
	 * While on U-Boot the format of the MAC address is correctly detected,
	 * in Linux it is not. The reason is the different behaviour of the
	 * implementation of sscanf. For exactly the same string with a
	 * correct MAC (szMAC) the last parameter (consumed_chars) is 0 in
	 * U-Boot but is the string length in Linux. So this workaround to
	 * overcome the problem.
	 */
#ifdef LINUX
	format_ok = strlen(szMAC);
#endif

	/* %n is not reached, if the format is correct */
	if( sscanf( szMAC, "%02X:%02X:%02X:%02X:%02X:%02X%n",
	    &auiMAC[ 0 ], &auiMAC[ 1 ], &auiMAC[ 2 ], &auiMAC[ 3 ],
	    &auiMAC[ 4 ], &auiMAC[ 5 ], &consumed_chars) != 6 ||
	    consumed_chars != format_ok)
		return NV_SET_ERROR( NVE_WRONG_VALUE, szMAC );

        for( i = 0; i < 6; i++ ) {
                if( auiMAC[ i ] > 255 )
                        return NV_SET_ERROR( NVE_WRONG_VALUE, szMAC );
                else
                        pMAC->u.c[ i ] = auiMAC[ i ];
        }

        return 1;
}

const char* NvToStringIP( nv_ip_t uiIP )
{
        static char acBuffer[ 20 ];

        sprintf( acBuffer, "%u.%u.%u.%u",
                 (uiIP) & 0xff,
                 (uiIP >> 8)  & 0xff,
                 (uiIP >> 16) & 0xff,
                 (uiIP >> 24) & 0xff );

        return acBuffer;
}

int NvToIP( nv_ip_t* pIP, const char* szIP )
{
        uint32_t auiIP[ 4 ];
        int i;

        if( sscanf( szIP, "%u.%u.%u.%u",
                    &auiIP[ 0 ],
                    &auiIP[ 1 ],
                    &auiIP[ 2 ],
                    &auiIP[ 3 ] ) != 4 )
                return NV_SET_ERROR( NVE_WRONG_VALUE, szIP );

        for( i = 0; i < 4; i++ )
                if( auiIP[ i ] > 255 )
                        return NV_SET_ERROR( NVE_WRONG_VALUE, szIP );

        *pIP = ( auiIP[ 0 ] |
                 ( auiIP[ 1 ] << 8 ) |
                 ( auiIP[ 2 ] << 16 ) |
                 ( auiIP[ 3 ] << 24 ) );

        return 1;
}

const char* NvToStringPart( nv_part_type_e ePartType )
{
        /* can be out-of-range when reading from NVRAM */
        return ( ePartType < ARRAY_SIZE( l_aszPartTypes ) ) ?
                l_aszPartTypes[ ePartType ] : 
                "Out-Of-Range Partition Type";
}

int NvToPart( nv_part_type_e* pePart, const char* szPart )
{
        return NvPrivFindInTable( (int*) pePart, szPart, l_aszPartTypes,
                                  ARRAY_SIZE( l_aszPartTypes ) );
}

const char* NvToStringFS( nv_fs_type_e eFS )
{
        /* can be out-of-range when reading from NVRAM */
        return ( eFS < ARRAY_SIZE( l_aszFSTypes ) ) ?
                l_aszFSTypes[ eFS ] : 
                "Out-Of-Range FS Type";
}

int NvToFS( nv_fs_type_e* peFS, const char* szFS )
{
        return NvPrivFindInTable( (int*) peFS, szFS, l_aszFSTypes,
                                  ARRAY_SIZE( l_aszFSTypes ) );
}

const char* NvToStringOS( nv_os_type_e eOS )
{
        /* can be out-of-range when reading from NVRAM */
        return ( eOS < ARRAY_SIZE( l_aszOSTypes ) ) ?
                l_aszOSTypes[ eOS ] : 
                "Out-Of-Range OS Type";
}

int NvToOS( nv_os_type_e* peOS, const char* szOS )
{
        return NvPrivFindInTable( (int*) peOS, szOS, l_aszOSTypes,
                                  ARRAY_SIZE( l_aszOSTypes ) );
}

const char* NvToStringLoc( nv_critical_loc_e eLoc )
{
        /* can be out-of-range when reading from NVRAM */
        return ( eLoc < ARRAY_SIZE( l_aszCritLoc ) ) ?
                l_aszCritLoc[ eLoc ] : 
                "Out-Of-Range Loc Type";
}

const char* NvToStringSize64( const uint64_t ullVal )
{
        static char szBuffer[ 20 ];

        if( ullVal && !(ullVal & 0xfffff ) )
                sprintf( szBuffer, "%i MiB", (int) ( ullVal >> 20 ) );
        else if( ullVal && !(ullVal & 0x3ff) )
                sprintf( szBuffer, "%i KiB", (int) ( ullVal >> 10 ) );
        else if( !ullVal )
                /* no hex prefix */
                sprintf( szBuffer, "0" );
        else
                sprintf( szBuffer, "0x%09" PRINTF_QUAD "x", ullVal );

        return szBuffer;
}

/*! \brief retreives a 64bit Value */
/*!
  \param  pullVal the resulting number
  \param  szVal   can be hex number, number in MiB, Number K[iB], Number M[iB]
  \return 0 on failure
*/
int NvToSize64( /*@out@*/ uint64_t* pullVal, const char* szVal )
{
        int iRes = 1;

        /* when 0x is present, use the number */
        if( sscanf( szVal, "0x%Lx", pullVal ) != 1 ) {
                unsigned int uVal = 0;
                char         szBuffer[ 20 ];
                int          iNum = sscanf( szVal, "%u%19s", &uVal, szBuffer );

                if( 1 <= iNum ) {
                        const char* szUnit = szBuffer;
                        int         iFac   = 1;

                        /* skip spaces characters */
                        while( ' ' == *szUnit )
                                szUnit++;

                        if( ( 1 == iNum ) ||
                            !strnicmp( "MiB", szUnit, 3 ) ||
                            !strnicmp( "M",   szUnit, 1 ) )
                                /* decimal number without unit means MiB */
                                iFac = 1024*1024;
                        else if( !strnicmp( "KiB", szUnit, 3 ) ||
                                 !strnicmp( "K",   szUnit, 1 ) )
                                iFac = 1024;
                        else
                                iRes = 0;

                        if( iRes )
                                *pullVal = uVal * iFac;
                } else
                        iRes = 0;
        } /* if( sscanf() */

        return iRes;
}

/* ********** local functions ********** */

static int NvPrivImageUpdate( nv_critical_loc_e eImage )
{
        void* pvTmp = NULL;

        pvTmp = malloc( l_iEraseSize );
        CE_SET_ERROR_EXT( NULL != pvTmp, NVE_NO_SPACE, "malloc" );

        CE( NvPrivImageWrite( eImage ) );
        /* read back */
        CE( NvPrivImageRead( eImage, pvTmp ) );
        CE_SET_ERROR_EXT( !memcmp( l_pvWorkcopy, pvTmp, l_iEraseSize ),
                          NVE_MATCH, NvToStringLoc( eImage ) );

        FREE( pvTmp );
        return 1;

error:
        FREE( pvTmp );
        return 0;
}

static int NvPrivImagesRead( void )
{
        int iRes = 0;
        nv_critical_loc_e eLoc = NVCL_ORIGINAL;
        nv_critical_t axCritical[ 2 ];

        REQUIRE_INITIALIZED();

        /* initialize both parameters with different values to detect any
         * read errors */
        memset( &axCritical[ NVCL_ORIGINAL ], 0,
                sizeof( axCritical[ NVCL_ORIGINAL ] ) );
        memset( &axCritical[ NVCL_MIRROR ], 1,
                sizeof( axCritical[ NVCL_MIRROR ] ) );

        /* process original image */
        iRes = NvPrivImageRead( NVCL_ORIGINAL, l_pvFlashInput );
        g_xInfo.bFlashOriginalGood = iRes;
        if( iRes )
                /* everything ok, copy it */
                memcpy32( l_pvWorkcopy, l_pvFlashInput, l_iEraseSize );


        if( iRes )
                iRes = NvPrivCriticalRead( &axCritical[ NVCL_ORIGINAL ],
                                           NVCL_ORIGINAL );
        g_xInfo.bOriginalGood   = iRes;

        /* process mirror image */
        iRes = NvPrivImageRead( NVCL_MIRROR, l_pvFlashInput );
        g_xInfo.bFlashMirrorGood = iRes;
        if( iRes && !g_xInfo.bFlashOriginalGood )
                /* everything ok and we don't have a workcopy yet */
                memcpy32( l_pvWorkcopy, l_pvFlashInput, l_iEraseSize );

        if( iRes )
                iRes = NvPrivCriticalRead( &axCritical[ NVCL_MIRROR ],
                                           NVCL_MIRROR );

        g_xInfo.bMirrorGood   = iRes;
        g_xInfo.bFlashMirrorMatch = 0;
        if( g_xInfo.bFlashMirrorGood && g_xInfo.bFlashOriginalGood )
                g_xInfo.bFlashMirrorMatch = !memcmp(
                        l_pvWorkcopy,
                        l_pvFlashInput,
                        l_iEraseSize );

        g_xInfo.bMirrorMatch = 0;
        if( g_xInfo.bMirrorGood && g_xInfo.bOriginalGood )
                g_xInfo.bMirrorMatch = !memcmp(
                        &axCritical[ NVCL_ORIGINAL ],
                        &axCritical[ NVCL_MIRROR ],
                        sizeof( axCritical[ 0 ] ) );

        g_xInfo.bGood   = (  g_xInfo.bFlashOriginalGood ||  g_xInfo.bFlashMirrorGood );
        g_xInfo.bAnyBad = ( !g_xInfo.bFlashOriginalGood || !g_xInfo.bFlashMirrorGood );

        if( g_xInfo.bGood && !g_xInfo.bOriginalGood )
                eLoc = NVCL_MIRROR;

        CE( NvPrintStatus() );

        if( !g_xInfo.bGood ) {
                /* no data available, clear everything */
                NvWorkcopyReset();
        } else {
                /* use existing and checked image */
                *g_pWorkcopy = axCritical[ eLoc ];

                /* increase minor number because reserved entries may change */
                if( g_pWorkcopy->s.uiVerMinor <= NVV_MINOR )
                        /* <= to get rid of a compiler warning */
                        g_pWorkcopy->s.uiVerMinor = NVV_MINOR;

        }

        return 1;

error:
        return 0;
}

/*! \brief Reads the original or the mirror image. */
/*! The image is checked whether the CRC is ok, the magic matches and the
    version is supported.
  \param  eType the image to read
  \return 0 on failure otherwise 1
*/
static int NvPrivCriticalRead(
        nv_critical_t* pParamCrit,
        nv_critical_loc_e eType )
{
        crc32_t uiCRC32;

        REQUIRE_INITIALIZED();

        CLEAR( *pParamCrit );

        /* take it from our flash copy */
        memcpy32( pParamCrit, (char*) l_pvFlashInput + sizeof( nv_os_meta_t ),
                  sizeof( *pParamCrit ) );

        /* verify checksum */
        uiCRC32 = crc32( 0,
                         (const unsigned char*) &pParamCrit->s,
                         sizeof( pParamCrit->s ) );
        if( uiCRC32 == pParamCrit->uiCRC32 ) {
                /* check magic */
                if( !strncmp( pParamCrit->s.szMagic,
                              NV_MAGIC_CRITICAL,
                              sizeof( pParamCrit->s.szMagic ) ) ) {

                        /* check version */
                        if( NVV_MAJOR != pParamCrit->s.uiVerMajor ) {
                                NV_SET_ERROR( NVE_VERSION, "" );
                                goto error;
                        }
                } else {
                        NV_SET_ERROR( NVE_MAGIC, pParamCrit->s.szMagic );
                        goto error;
                }
        } else {
                NV_SET_ERROR( NVE_CRC, "" );
                goto error;
        }

        return 1;

error:
        return 0;
}

static int NvPrivImageWrite( nv_critical_loc_e eType )
{
        loff_t iOffs = 0;
        int    iRes  = 0;

        REQUIRE_INITIALIZED();

        CE( NvPrivOSFlashOpen( 1 ) );
        if( NvPrivFlashDetermineOffs( eType, &iOffs ) &&
            NvPrivOSFlashProtect( iOffs, l_iEraseSize, 0 ) ) {
                iRes = NvPrivOSFlashErase( iOffs );
                if( iRes )
                        iRes = NvPrivOSFlashWrite( l_pvWorkcopy, iOffs, l_iEraseSize );
                iRes &= NvPrivOSFlashProtect( iOffs, l_iEraseSize, 1 );
        }

        iRes &= NvPrivOSFlashClose();
        CE( iRes );

        return 1;

error:
        return 0;
}

static int NvPrivImageRead( nv_critical_loc_e eType, /*@out@*/ void* pvImage )
{
        crc32_t uiCRC32;
        loff_t  iOffsBase = 0;
        int     iRes = 0;

        REQUIRE_INITIALIZED();

        CE( NvPrivOSFlashOpen( 0 ) );
        iRes = NvPrivFlashDetermineOffs( eType, &iOffsBase );
        if( iRes )
                iRes = NvPrivOSFlashRead( pvImage, iOffsBase, l_iEraseSize );
        iRes &= NvPrivOSFlashClose();
        CE( iRes );
		uiCRC32 = crc32( 0, sizeof( crc32_t ) + (uchar_t*) pvImage,
                                 l_iEraseSize - sizeof( crc32_t ) );
        CE_SET_ERROR( uiCRC32 == *(const crc32_t*) pvImage, NVE_CRC );

        return 1;

error:
        return 0;
}

/*! \brief Determines the offset for an image of eType in the NVRAM partition. */
/*!
  \param  eType the image to determine the offset in the
  \param  piOffs the offset will be stored here when no error happens.
          Otherwise it is undefined
  \return 0 on failure otherwise 1
*/
static int NvPrivFlashDetermineOffs( nv_critical_loc_e eType, loff_t* piOffs )
{
        nv_priv_flash_status_t xStatus;
        int  iGood  = 0;

        ASSERT( ( eType == NVCL_ORIGINAL ) || ( eType == NVCL_MIRROR ) );

        *piOffs = 0;

        /* The parameters stored on the first good block. The mirrors is
         * stored on the next good block */
        do {
                /* NvPrivOSFlashInfo will return 0 if out of range */
                if( !NvPrivOSFlashInfo( *piOffs, &xStatus ) )
                        /* error is set */
                        return 0;

                /* check whether our algorithm can work, the critical
                 * parameters can fit into one erase size */
                if( xStatus.iEraseSize < sizeof( nv_critical_t ) )
                        return NV_SET_ERROR( NVE_IO, "NvFlashDetermineOffs: erase size too small" );

                if( !xStatus.bBad )
                        iGood++;

                if( ( ( 1 == iGood ) && ( NVCL_ORIGINAL ) == eType ) ||
                    ( ( 2 == iGood ) && ( NVCL_MIRROR ) == eType ) )
                        return 1;

                *piOffs += xStatus.iEraseSize;
        } while( 1 );

        /* algorithm will always terminate because the flash partition is not
         * endless */ 
        return NV_SET_ERROR( NVE_NO_SPACE, "" );
}

int NvPrivFlashProtectAll( void )
{
        loff_t iOffs = 0;
        int    iCount = 0;

        /* flash every block of NVRAM partition. Depending on the type of
         * FLASH, there can be only 2 blocks (NOR) or more than 2 blocks for
         * NAND bad block management */
        while( 1 ) {
                /* for every block */
                nv_priv_flash_status_t xStatus;
                
		int iRes = NvPrivOSFlashInfo(iOffs, &xStatus);
		if (xStatus.type == MTD_NORFLASH)
			break;
		if( !iRes ) {
                        if( iCount < 2 )
                                /* we need at least two pages for NVRAM */
                                CE( !iOffs );
                        else
                                /* no more blocks for NVRAM, this is ok */
                                break;
                } /* if( NvPrivOSFlashInfo */

                CE( NvPrivOSFlashProtect( iOffs, l_iEraseSize, 1 ) );

                iOffs += l_iEraseSize;
                iCount++;
        } /* while( 1 ) */

        return 1;

error:
        printf( "error %i\n", iCount );
        return 0;
}

int NvPrivAnyFlashImageGood(void)
{
	return (g_xInfo.bFlashMirrorGood || g_xInfo.bFlashOriginalGood);
}

int NvPrivImageRepair( void )
{
        REQUIRE_INITIALIZED();

        if(  !g_xInfo.bFlashMirrorGood || !g_xInfo.bFlashOriginalGood ) {
                nv_critical_loc_e eToWrite = ( g_xInfo.bFlashOriginalGood ? NVCL_MIRROR : NVCL_ORIGINAL );

                NvPrivOSPrintf( NVRAM "Repairing %s\n",
                                l_aszCritLoc[ eToWrite ] );
                /* at least one is good */
                CE( NvPrivImageUpdate( eToWrite ) );
        }

        return 1;
error:
        return 0;
}

static int NvPrivFindInTable(
        /*@out@*/ int* piIdx,
        const char* szWhat, const char** aszTable,
        size_t iSize )
{
        int iRes = 1;
	size_t i = 0;
        char bFound = 0;

        while( i < iSize ) {
                if( !strcmp( szWhat, aszTable[ i ] ) ) {
                        bFound = 1;
                        break;
                }

                i++;
        }

        *piIdx = i;
        if( !bFound )
                iRes = NV_SET_ERROR( NVE_WRONG_VALUE, szWhat );

        return iRes;
}


#ifdef CFG_HAS_WIRELESS

#include <mtd.h>
#define MAX_SECTORS_TO_CHECK_4_WCD	10
int NvPrivWirelessCalDataCheck( wcd_data_t *pWCal )
{
	nv_wcd_header_t *pH = &pWCal->header;
        crc32_t uiCRC32;
  
        /* Start with the sanity checks */
        if( !strncmp( pH->magic_string, WCD_MAGIC, sizeof( pH->magic_string ) ) ) {
                /* check version */
                if( ( (pH->ver_major >= '1') && (pH->ver_major <= '9') ) &&
                    ( (pH->ver_minor >= '0') && (pH->ver_minor <= '9') ) ) {
                        uiCRC32 = crc32( 0, 
                                        (const unsigned char*) pWCal->cal_curves_bg,
                                        pH->wcd_len );
                        if( uiCRC32 == pH->wcd_crc )
                                return 1;                        
                }
        }
        return 0;
}

/*! \brief Sets the Wireless Calibration inside the nvram structure */
/*! \return 0 on failure */
int NvPrivWCDSetInNvram( wcd_data_t *pWCal )
{
        /* Check if valid and then set in nvram */
        if ( NvPrivWirelessCalDataCheck( pWCal ) ) {
                return NvOSCfgSet( NVOS_WCAL_DATA, pWCal, sizeof( wcd_data_t ) );
        }
        return 0;
}

/*! \brief Gets the Wireless Calibration from the last Sectors of the Flash */
/*! \return 0 on failure */
int NvPrivWCDGetFromFlashAndSetInNvram( void )
{
	wcd_data_t *pWCal;
	nv_priv_flash_status_t xStatus;
	size_t l_iFlashSize = 0;
	int iRes, j;

	/* Calibration data is stored at the end of the flash */
	if( NvPrivOSFlashInfo( 0, &xStatus ) == 0 )
		return 0;

	pWCal = malloc( xStatus.iEraseSize );
	if( pWCal == NULL )
		return 0;

	l_iFlashSize = MtdSize( 0 );

	for ( j = 1; j < MAX_SECTORS_TO_CHECK_4_WCD; j++ ) {
		if( MtdBlockIsBad( 0, l_iFlashSize - (xStatus.iEraseSize * j) ) )
			continue;	/* Skip bad blocks */

		iRes = MtdRead( 0, l_iFlashSize - (xStatus.iEraseSize * j),
				xStatus.iEraseSize, pWCal );
		if ( iRes ) {
			if ( NvPrivWCDSetInNvram( pWCal ) ) {
				FREE( pWCal );
				return 1;
			}
		}
	}

	FREE( pWCal );
	return 0;
}

/*! \brief Writes the Wireless Calibration data into the last sectors
           of the flash, 2 times */
/*! \return 0 on failure */
int NvPrivWCDSaveInFlash( wcd_data_t *pWCal )
{
	nv_priv_flash_status_t xStatus;
	size_t l_iFlashSize = 0;
	int iRes, j, i = 0;

	if ( (pWCal != NULL) && NvPrivWirelessCalDataCheck( pWCal ) ) {
		if( NvPrivOSFlashInfo( 0, &xStatus ) == 0 )
			return 0;

		l_iFlashSize = MtdSize( 0 );

		for ( j = 1; j < MAX_SECTORS_TO_CHECK_4_WCD; j++ ) {
			if( MtdBlockIsBad( 0, l_iFlashSize - (xStatus.iEraseSize * j) ) )
				continue;	/* Skip bad blocks */

			if( !MtdErase( 0, l_iFlashSize - (xStatus.iEraseSize * j),
			    xStatus.iEraseSize ) ) {
				char szErr[ 64 ];
		      		sprintf( szErr, "Flash Erase @ 0x%08qx, length = %i",
				         l_iFlashSize - (xStatus.iEraseSize * j),
					 xStatus.iEraseSize );
				goto error;
			}

			iRes = MtdWrite( 0, l_iFlashSize - (xStatus.iEraseSize * j),
					 xStatus.iEraseSize, pWCal );
			if ( iRes ) {
				/* The data is written in 2 different sectors */
				i++;
				if ( i >= 2 )
					break;
			}
		}
	}
error:
	return (i >= 2);
}
#endif
