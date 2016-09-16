/*
 *  nvram/lib/src/nvram_cmdline.c
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
 *  !Descr:      Provides the NvCmdLine function and helper
 */

#include "nvram.h"
#include "nvram_priv.h"
#include "nvram_version.h"

#define FOR_MODULE    .eMode = NVPM_MODULE
#define FOR_IP        .eMode = NVPM_IP
#define FOR_PARTITION .eMode = NVPM_PARTITION
#define FOR_OS        .eMode = NVPM_OS

#define NEEDS_VAL \
        .bNeedsVal = 1

#define OFFS(x) \
        (((char*) &(((nv_critical_t*) NULL)->x)) - \
         ((char*) NULL))

#define OFFS_NONE \
        .iOffs = 0                             \
        
#define OFFS_HARDCODED \
        .bHardcoded = 1, \
        NEEDS_VAL,   \
        OFFS_NONE

#define OFFS_MODULE( x )   \
        FOR_MODULE, \
        NEEDS_VAL,                                          \
       .iOffs = OFFS( s.p.xID.x )

#define OFFS_IP( x )   \
        FOR_IP, \
        NEEDS_VAL,                                          \
       .iOffs = OFFS( s.p.xIP.x )

#define OFFS_PART( x )   \
        FOR_PARTITION,                                                \
        NEEDS_VAL,                                          \
       .iOffs = ( (char*) &((nv_critical_t*) NULL)->s.p.xPartTable.axEntries[ 0 ].x - \
                   (char* )&((nv_critical_t*) NULL)->s.p.xPartTable.axEntries[ 0 ] )
#define OFFS_OS( x )   \
        FOR_OS, \
        NEEDS_VAL,                                          \
       .iOffs = ( (char*) &((nv_critical_t*) NULL)->s.p.xOSTable.axEntries[ 0 ].x - \
                   (char* )&((nv_critical_t*) NULL)->s.p.xOSTable.axEntries[ 0 ] )

#define ALLOW_CHANGE_WHEN_FIXED \
        .bAllowChangeWhenFixed = 1

#define COPY_TO_SET( x, szStr )                 \
        ({                                      \
                int _iRes = 1; \
                if( strlen( szStr ) < sizeof( g_pWorkcopy->s.p.x ) ) { \
                        CLEAR( *g_pWorkcopy->s.p.x );             \
                        strncpy( g_pWorkcopy->s.p.x,             \
                                 szStr,                                 \
                                 sizeof( g_pWorkcopy->s.p.x ) ); \
                } else \
                        _iRes = NV_SET_ERROR( NVE_WRONG_VALUE, szStr ); \
                (_iRes);                                                \
        })                                                              \

#define PRINT_RESERVED( szLabel, auiReserved )                \
        do { \
                int _i;                         \
                const char* _pucRes = (const char*) auiReserved; \
                NvPrivOSPrintf( "%s", szLabel ); \
                for( _i = 0; _i < sizeof( auiReserved ); _i++ ) \
                        NvPrivOSPrintf( "%s%02x ", _i ? " " : "", \
                                _pucRes[ _i ] );      \
                NvPrivOSPrintf( "\n" ); \
        } while( 0 )


/* ********** local typedefs ********** */

typedef enum {
        NVPM_ANY,
        NVPM_MODULE,
        NVPM_IP,
        NVPM_PARTITION,
        NVPM_OS,
} nv_param_mode_e;
    
typedef enum {
        NVAT_ACTION_MODE_SEL,
        NVAT_ACTION_ADD,
        NVAT_ACTION_SEL,
        NVAT_ACTION_DEL,
        NVAT_BOOL,
        NVAT_UINT32,
        NVAT_UINT64,
        NVAT_IP,
        NVAT_MAC,
        NVAT_PRODUCT_TYPE,
        NVAT_SERIAL_NR,
        NVAT_PART_NAME,
        NVAT_OS_TYPE,
        NVAT_PART_TYPE,
        NVAT_FS_TYPE,
#ifdef CFG_HAS_WIRELESS
        NVAT_WCD_LOAD,
#endif
} nv_param_type_e;

typedef struct nv_param {
        const char*     szName;
        nv_param_type_e eType;
        void*           pvAddr;
        int             iOffs;
        nv_param_mode_e eMode;
        char            bHardcoded;
        char            bNeedsVal;
        char            bAllowChangeWhenFixed;
} nv_param_t;

/* ********** local function declarations ********** */

static int NvPrivCriticalPrintContents( char bReserved );
static int NvPrivCriticalPrintContentsFromArgV(
        int argc,
        const char* argv[] );
static int NvPrivPrintCritical(
        nv_critical_loc_e eType,
        char bPrintReserved );
static int NvPrivFindParam(
        /*@out@*/ const nv_param_t** ppParam,
        const char* szArg,
        /*@out@*/ /*@null@*/ const char** pszValue,
        const nv_param_mode_e eMode,
        char bForSet );
static int NvPrivCriticalSetFromArgV( int argc, const char* argv[] );
static void NvPrivPrintParamModule(
        /*@in@*/ const struct nv_param_module_id* pModule,
        char bPrintReserved );
static void NvPrivPrintParamIP(
        /*@in@*/ const struct nv_param_ip* pIP,
        char bPrintReserved );
static void NvPrivPrintParamPartitionTable(
        /*@in@*/ const struct nv_param_part_table* pPartTable,
        char bPrintReserved );
static void NvPrivPrintParamOSTable(
        /*@in@*/ const struct nv_param_os_cfg_table* pOSTable,
        char bPrintReserved );

/* ********** local variables ********** */

/* uint32 are always hexadecimal */
static const nv_param_t l_axArg[] = {
        /* module */
        { "module",      NVAT_ACTION_MODE_SEL,   FOR_MODULE, OFFS_NONE, ALLOW_CHANGE_WHEN_FIXED },

        { "producttype", NVAT_PRODUCT_TYPE, FOR_MODULE, OFFS_MODULE( szProductType ) },
        { "serialnr",    NVAT_SERIAL_NR,    FOR_MODULE, OFFS_MODULE( szSerialNr ) }, 
        { "revision",    NVAT_UINT32,       OFFS_MODULE( uiRevision ) },
        { "patchlevel",  NVAT_UINT32,       OFFS_MODULE( uiPatchLevel ) },
        { "ethaddr1",    NVAT_MAC,          OFFS_MODULE( axMAC[ 0 ] ) },
        { "ethaddr2",    NVAT_MAC,          OFFS_MODULE( axMAC[ 1 ] ) },
#ifdef CFG_HAS_WIRELESS
        { "wifical",     NVAT_WCD_LOAD,     FOR_MODULE,  OFFS_NONE  },
#endif
        /* ip, it's default */
        { "network",     NVAT_ACTION_MODE_SEL,   FOR_IP, OFFS_NONE, ALLOW_CHANGE_WHEN_FIXED },

        { "gateway",     NVAT_IP,     OFFS_IP( uiIPGateway ) },
        { "dns1",        NVAT_IP,     OFFS_IP( auiIPDNS[ 0 ] ) },
        { "dns2",        NVAT_IP,     OFFS_IP( auiIPDNS[ 1 ] ) },
        { "server",      NVAT_IP,     OFFS_IP( uiIPServer ) },
        { "netconsole",  NVAT_IP,     OFFS_IP( uiIPNetConsole ) },
        { "ip1",         NVAT_IP,     OFFS_IP( axDevice[ 0 ].uiIP ) },
        { "netmask1",    NVAT_IP,     OFFS_IP( axDevice[ 0 ].uiNetMask ) },
        { "dhcp1",       NVAT_BOOL,   OFFS_IP( axDevice[ 0 ].flags.bDHCP ) },
        { "ip2",         NVAT_IP,     OFFS_IP( axDevice[ 1 ].uiIP ) },
        { "netmask2",    NVAT_IP,     OFFS_IP( axDevice[ 1 ].uiNetMask ) },
        { "dhcp2",       NVAT_BOOL,   OFFS_IP( axDevice[ 1 ].flags.bDHCP ) },

        /* partititon table */
        { "partition",   NVAT_ACTION_MODE_SEL,   FOR_PARTITION, OFFS_NONE, ALLOW_CHANGE_WHEN_FIXED },
        { "add",         NVAT_ACTION_ADD,    FOR_PARTITION, OFFS_NONE, ALLOW_CHANGE_WHEN_FIXED },
        { "del" ,        NVAT_ACTION_DEL,    FOR_PARTITION, OFFS_NONE },
        { "select",      NVAT_ACTION_SEL,    FOR_PARTITION, OFFS_HARDCODED, ALLOW_CHANGE_WHEN_FIXED },

        { "name",        NVAT_PART_NAME,     OFFS_PART( szName ) },
        { "chip",        NVAT_UINT32,        OFFS_PART( uiChip ) },
        { "start",       NVAT_UINT64,	     OFFS_PART( ullStart ) },
        { "size",        NVAT_UINT64,        OFFS_PART( ullSize ) },
        { "type",        NVAT_PART_TYPE,     OFFS_PART( eType ) },
        { "flag_fixed",  NVAT_BOOL,	     OFFS_PART( flags.bFixed ), ALLOW_CHANGE_WHEN_FIXED },
        { "flag_readonly", NVAT_BOOL,	     OFFS_PART(flags.bReadOnly)},
        { "flag_fs_mount_readonly", NVAT_BOOL, OFFS_PART( flags.fs.bMountReadOnly ) },
        { "flag_fs_root",    NVAT_BOOL,      OFFS_PART( flags.fs.bRoot ) },
        { "flag_fs_type",    NVAT_FS_TYPE,   OFFS_PART( flags.fs.eType ) },
        { "flag_fs_version", NVAT_UINT32,    OFFS_PART(flags.fs.uiVersion)},

        /* os configuration table */
        { "os",       NVAT_ACTION_MODE_SEL,   FOR_OS, OFFS_NONE, ALLOW_CHANGE_WHEN_FIXED },
        { "add",      NVAT_ACTION_ADD,    FOR_OS, OFFS_NONE },
        { "del",      NVAT_ACTION_DEL,    FOR_OS, OFFS_NONE },
        { "select",   NVAT_ACTION_SEL,    FOR_OS, OFFS_HARDCODED },

        { "type",     NVAT_OS_TYPE,       OFFS_OS( eType ) },
        { "start",    NVAT_UINT32,        OFFS_OS( uiStart ) },
        { "size",     NVAT_UINT32,        OFFS_OS( uiSize ) },
};

/* ********** global functions ********** */

int NvCmdLine( int argc, const char* argv[] )
{
        REQUIRE_INITIALIZED();

        if( !argc )
                return NvPrintHelp();

        if( ( argc > 1 ) && !strcmp( "set", argv[ 0 ] ) ) {
                int i = 1;
                CE( NvPrivCriticalSetFromArgV( argc - i, &argv[ i ] ) );
        } else if( ( argc > 1 ) && !strcmp( "print", argv[ 0 ] ) ) {
                CE( NvPrivCriticalPrintContentsFromArgV( argc - 1,
                                                           &argv[ 1 ] ) );
        } else if( !strcmp( "printall", argv[ 0 ] ) ) {
                char bReserved = ( ( argc > 1 ) &&
                                   !strcmp( "--reserved", argv[ 1 ] ) );
                
                CE( NvPrivCriticalPrintContents( bReserved ) );
        } else if( !strcmp( "save", argv[ 0 ] ) ) {
                CE( NvSave() );
                /* reload */
                CE( NvInit( NVR_MANUAL ) );
        } else if( !strcmp( "repair", argv[ 0 ] ) ) {
                CE( NvPrivImageRepair() );
        } else if ( !strcmp( "reset", argv[ 0 ] ) ) {
                CE( NvWorkcopyReset() );
        } else if( !strcmp( "help", argv[ 0 ] ) ) {
                CE( NvPrintHelp() );
        } else
                CE( NvPrintHelp() );
                 
        return 1;

error:
        return 0;
}

int NvPrintHelp( void )
{
        int i;

        /* seems that U-Boot crashes when I concat just the lines.
         * !TODO. Do we have a hidden bug? */
        static const char* aszHelp[] = {
                " \n",
                "  help     : prints this\n",
                "  print    : prints selected parameters.\n",
                "             E.g.: print module mac serialnr\n",
                "  printall : prints complete contents and metainfo\n",
                "  repair   : Repairs the contents. If one image is\n",
                "             bad, the good one is copied onto it.\n",
                "             If both are good or bad, nothing happens.\n",
                "  reset    : resets everything to factory default values.\n",
                "  save     : saves the parameters\n",
                "  set      : sets parameters.\n",
                "\n",
        };

        NvPrivOSPrintf( "Arguments: help|print <params>|printall|repair|reset|save|set <params>\n" );

                
        for( i = 0; i < ARRAY_SIZE( aszHelp ); i++ )
                NvPrivOSPrintf( "%s", aszHelp[ i ] );

        NvPrivOSPrintf( "  params for \"set\" or \"print\" can be" );
        
        /* @TODO: Add Description */
        for( i = 0; i < ARRAY_SIZE( l_axArg ); i++ ) {
                const nv_param_t* pArg = &l_axArg[ i ];
                char bModeSel          = ( NVAT_ACTION_MODE_SEL == pArg->eType );
                const char* szAssign   = pArg->bNeedsVal ? "=" : "";

                if( bModeSel )
                        NvPrivOSPrintf( "\n     %-10s", pArg->szName );
                else
                        NvPrivOSPrintf( " [%s%s]", pArg->szName, szAssign );
        }
        NvPrivOSPrintf( "\n\nParams trailed with '=' require a value in the set command. In the print command, '=' mustn't be used.\n" );

        /* they user may want to know what strings he can use */
        NvPrivOSPrintf( "\nPossible Values are\n" );
        NvPrivOSPrintf( "  os type:        " );
        for( i = 0; i < NVOS_LAST; i++ )
                NvPrivOSPrintf( "%s%s", ( i > 0 ? "," : "" ), NvToStringOS( i ) );
        NvPrivOSPrintf( "\n" );

        NvPrivOSPrintf( "  partition type: " );
        for( i = 0; i < NVPT_LAST; i++ )
                NvPrivOSPrintf( "%s%s", ( i > 0 ? "," : "" ), NvToStringPart( i ) );
        NvPrivOSPrintf( "\n" );

        NvPrivOSPrintf( "  flag_fs_type:   " );
        for( i = 0; i < NVFS_LAST; i++ )
                NvPrivOSPrintf( "%s%s", ( i > 0 ? "," : "" ), NvToStringFS( i ) );
        NvPrivOSPrintf( "\n" );

        NvPrivOSPrintf( "\nExamples:\n"
                        "  %s print module ethaddr1 serialnr : prints mac address and serial number\n"
                        "  %s print partition select=0 name select=1 name: prints first and second partition name\n"
                        "  %s set network ip1=192.168.42.30 : changes the IP address\n", CMD_NAME , CMD_NAME , CMD_NAME );

        return 1;
}

static int NvPrivCriticalPrintContents( char bReserved )
{
        REQUIRE_INITIALIZED();

        /* dump meta information */
        NvPrivOSPrintf( "NVRAM Critical Parameter Status: \n" );
        if( !g_xInfo.bDefault ) {
                /* flash is present */
                NvPrivOSPrintf( "  Original is %s\n", g_xInfo.bOriginalGood ? "OK " : "NOT OK" );
                NvPrivOSPrintf( "  Mirror is %s\n", g_xInfo.bMirrorGood ? "OK " : "NOT OK" );
                NvPrivOSPrintf( "  Mirror %s\n",
                                g_xInfo.bMirrorMatch ? "matches" : "mismatches" );
                
                NvPrivOSPrintf( "NVRAM Flash (OS Cfg) Status: \n" );
                NvPrivOSPrintf( "  Original is %s\n", g_xInfo.bFlashOriginalGood ? "OK " : "NOT OK" );
                NvPrivOSPrintf( "  Mirror is %s\n", g_xInfo.bFlashMirrorGood ? "OK " : "NOT OK" );
                NvPrivOSPrintf( "  Mirror %s\n",
                                g_xInfo.bFlashMirrorMatch ? "matches" : "mismatches" );
        } else
                NvPrivOSPrintf( "  Using defaults\n" );
        
        NvPrivOSPrintf( "Workcopy: \n" );
        CE( NvPrivPrintCritical( NVCL_WORKCOPY, bReserved ) );
        
        return 1;

error:
        return 0;
}

static int NvPrivPrintCritical(
        nv_critical_loc_e eType,
        char bPrintReserved )
{
        uint32_t uiLibVerMajor;
        uint32_t uiLibVerMinor;
        const nv_critical_t* pParam = g_pWorkcopy;
        
        NvGetVersion( &uiLibVerMajor, &uiLibVerMinor );

        NvPrivOSPrintf( "  Magic: %s\n", pParam->s.szMagic );

        /* version and do some checks */
        NvPrivOSPrintf( "  Version: %02u.%02u",
                pParam->s.uiVerMajor,
                pParam->s.uiVerMinor );

        if( pParam->s.uiVerMajor != uiLibVerMajor )
                return NV_SET_ERROR( NVE_VERSION, "" );
                
        if( pParam->s.uiVerMinor != uiLibVerMinor ) {
                bPrintReserved = 1;
                NvPrivOSPrintf( "  (differs to library %02u.%02u)",
                        uiLibVerMajor, uiLibVerMinor );
        }
        
        NvPrivOSPrintf( "\n" );

        NvPrivPrintParamModule( &pParam->s.p.xID,  bPrintReserved );
        NvPrivPrintParamIP( &pParam->s.p.xIP,      bPrintReserved );
        NvPrivPrintParamPartitionTable( &pParam->s.p.xPartTable, bPrintReserved );
        NvPrivPrintParamOSTable( &pParam->s.p.xOSTable,          bPrintReserved );

        return 1;
}

static int NvPrivCriticalPrintContentsFromArgV(
        int argc, const char* argv[] )
{
        int i     = 0;
        int iEntry = -1;
        const nv_param_part_table_t*   pPartTable = &g_pWorkcopy->s.p.xPartTable;
        const nv_param_os_cfg_table_t* pOSTable   = &g_pWorkcopy->s.p.xOSTable;
        nv_param_mode_e eMode = NVPM_IP;

        REQUIRE_INITIALIZED();
        
        while( i < argc ) {
                const nv_param_t* pParam = NULL;
                const char* szArg = NULL;
                const void*       pvAddr = NULL;
                char bIntValid  = 0;
                int  iIntArg    = 0;
                
                CE( NvPrivFindParam( &pParam, argv[ i ], &szArg, eMode, 0 ) );

                eMode = pParam->eMode;

                if( !pParam->bNeedsVal ) {
                        i++;
                        continue;
                }
                bIntValid = ( sscanf( szArg, "%i", &iIntArg ) == 1 );

                if( NVAT_ACTION_MODE_SEL != pParam->eType ) {
                        /* get base address parameters */
                        switch( eMode ) {
                            case NVPM_PARTITION:
                                if( ( NVAT_ACTION_SEL != pParam->eType ) && 
                                    ( NVAT_ACTION_ADD != pParam->eType ) )
                                        CE_WRONG_VALUE( -1 != iEntry, "no partition selected" );
                                pvAddr = &pPartTable->axEntries[ iEntry ];
                                break;
                            case NVPM_OS:
                                if( ( NVAT_ACTION_SEL != pParam->eType ) && 
                                    ( NVAT_ACTION_ADD != pParam->eType ) )
                                        CE_WRONG_VALUE( -1 != iEntry, "no os selected" );
                                pvAddr = &pOSTable->axEntries[ iEntry ];
                                break;
                            case NVPM_MODULE:  /* no break */
                            case NVPM_IP:
                                pvAddr = g_pWorkcopy;
                                break;
                            case NVPM_ANY:
                                ASSERT( eMode != NVPM_ANY );
                                break;
                        }
                }
                
                pvAddr += pParam->iOffs;

                if( pParam->eType == NVAT_ACTION_MODE_SEL ) {
                        i++;
                        continue;
                }
                
                if( !pParam->bHardcoded )
                        NvPrivOSPrintf( "%s=", pParam->szName );

                /* do some action */
                switch( pParam->eType ) {
                    case NVAT_ACTION_MODE_SEL:
                        CE_WRONG_VALUE( bIntValid, szArg );
                        
                        switch( eMode ) {
                            case NVPM_PARTITION:
                                CE_WRONG_VALUE(
                                        ( iIntArg >= 0 ) &&
                                        ( iIntArg < pPartTable->uiEntries ), szArg );
                                break;
                            case NVPM_OS:
                                CE_WRONG_VALUE(
                                        ( iIntArg >= 0 ) &&
                                        ( iIntArg < pPartTable->uiEntries ), szArg );
                                break;
                            default:
                                CE_WRONG_VALUE( 0, argv[ i ] );
                        }
                        eMode = pParam->eMode;
                        break;

                    case NVAT_BOOL:
                        NvPrivOSPrintf( "%i", *((uint8_t*) pvAddr ) );
                        break;

                    case NVAT_MAC:
                        NvPrivOSPrintf( "%s",
                                        NvToStringMAC( *(nv_mac_t*) pvAddr ) );
                        break;

                    case NVAT_PRODUCT_TYPE:  /* no break */
                    case NVAT_SERIAL_NR:  /* no break */
                    case NVAT_PART_NAME:  /* no break */
                        NvPrivOSPrintf( "%s", (const char*) pvAddr );
                        break;

                    case NVAT_IP:
                        NvPrivOSPrintf( "%s",
                                        NvToStringIP( *(nv_ip_t*) pvAddr ) );
                        break;

                    case NVAT_FS_TYPE:
                        NvPrivOSPrintf( "%s",
                                        NvToStringFS( *(nv_fs_type_e*) pvAddr ) );
                        break;

                    case NVAT_PART_TYPE:
                        NvPrivOSPrintf( "%s",
                                        NvToStringPart( *(nv_part_type_e*) pvAddr ) );
                        break;

                    case NVAT_OS_TYPE:
                        NvPrivOSPrintf( "%s",
                                        NvToStringOS( *(nv_os_type_e*) pvAddr ) );
                        break;

                    case NVAT_UINT32:
                        NvPrivOSPrintf( "0x%x", *(uint32_t*) pvAddr );
                        break;
                        
                    case NVAT_UINT64:
                        NvPrivOSPrintf( "0x%" PRINTF_QUAD "x",
                                        *(uint64_t*) pvAddr );
                        break;
                        
                    case NVAT_ACTION_SEL: 
                    {
                            CE_WRONG_VALUE( bIntValid, szArg );
                            
                            switch( eMode ) {
                                case NVPM_PARTITION:
                                    CE_WRONG_VALUE(
                                            ( iIntArg >= 0 ) &&
                                            ( iIntArg < pPartTable->uiEntries ), szArg );
                                    break;
                                case NVPM_OS:
                                    CE_WRONG_VALUE(
                                            ( iIntArg >= 0 ) &&
                                            ( iIntArg < pOSTable->uiEntries ), szArg );
                                    break;
                                default:
                                    CE_WRONG_VALUE( 0, argv[ i ] );
                            }
                            
                            iEntry = iIntArg;
                            break;
                    }

                    case NVAT_ACTION_ADD:  /* no break */
                    case NVAT_ACTION_DEL:
#ifdef CFG_HAS_WIRELESS
                    case NVAT_WCD_LOAD:
#endif
                        /* will not be reached, satisfy compiler */
                        break;
                } /* switch */

                if( !pParam->bHardcoded )
                        NvPrivOSPrintf( "\n" );

                i++;
        } /* while( i < argc ) */

        return 1;

error:
        return 0;
}

static int NvPrivCriticalSetFromArgV( int argc, const char* argv[] )
{
        int i     = 0;
        int iEntry = -1;
        nv_param_part_table_t* pPartTable = &g_pWorkcopy->s.p.xPartTable;
        nv_param_os_cfg_table_t* pOSTable = &g_pWorkcopy->s.p.xOSTable;
        nv_param_mode_e eMode = NVPM_IP;
        char bFixed = 0;
        
        REQUIRE_INITIALIZED();

        while( i < argc ) {
                const nv_param_t* pParam = NULL;
                const char* szArg;
                char bIntValid  = 0;
                int  iIntArg    = 0;
                void* pvAddr    = NULL;
                
                CE( NvPrivFindParam( &pParam, argv[ i ], &szArg, eMode, 1 ) );

                eMode     = pParam->eMode;
                bIntValid = ( sscanf( szArg, "%i", &iIntArg ) == 1 );

                if( NVAT_ACTION_MODE_SEL != pParam->eType ) {
                        /* get base address parameters */
                        switch( eMode ) {
                            case NVPM_PARTITION:
                                if( ( NVAT_ACTION_SEL != pParam->eType ) && 
                                    ( NVAT_ACTION_ADD != pParam->eType ) )
                                        CE_WRONG_VALUE( -1 != iEntry, "no partition selected" );
                                pvAddr = &pPartTable->axEntries[ iEntry ];
                                break;
                            case NVPM_OS:
                                if( ( NVAT_ACTION_SEL != pParam->eType ) && 
                                    ( NVAT_ACTION_ADD != pParam->eType ) )
                                        CE_WRONG_VALUE( -1 != iEntry, "no os selected" );
                                pvAddr = &pOSTable->axEntries[ iEntry ];
                                break;
                            case NVPM_MODULE:  /* no break */
                            case NVPM_IP:
                                pvAddr = g_pWorkcopy;
                                break;
                            case NVPM_ANY:
                                ASSERT( eMode != NVPM_ANY );
                                break;
                        }
                }

                pvAddr += pParam->iOffs;

                if( NVAT_ACTION_SEL != pParam->eType ) {
                        char szBuf[ 64 ];
                        sprintf( szBuf,
                                 "parameter '%s' is fixed", argv[ i ] );
                        CE_WRONG_VALUE( !bFixed || pParam->bAllowChangeWhenFixed, szBuf );
                }
                
                /* do some action */
                switch( pParam->eType ) {
                    case NVAT_ACTION_MODE_SEL:
                        eMode = pParam->eMode;
                        bFixed = 0;
                        break;

                    case NVAT_BOOL:
			if (!strcmp(szArg, "on") || !strcmp(szArg, "yes"))
				*((uint8_t *) pvAddr) = 1;
			else
				*((uint8_t *) pvAddr) = atoi(szArg);
                        break;

                    case NVAT_MAC:
                        CE_WRONG_VALUE( NvToMAC( (nv_mac_t*) pvAddr, szArg ), szArg );
                        break;

                    case NVAT_PRODUCT_TYPE:
                        CE_WRONG_VALUE( COPY_TO_SET( xID.szProductType, szArg ), szArg );
                        break;

                    case NVAT_SERIAL_NR:
                        CE_WRONG_VALUE( COPY_TO_SET( xID.szSerialNr, szArg ), szArg );
                        break;
#ifdef CFG_HAS_WIRELESS
                    case NVAT_WCD_LOAD:
                        if ( !NvPrivWCDGetFromFlashAndSetInNvram() ) {
                                NvPrivOSPrintfError( "*** Error: loading Wifi Calibration Data from flash\n" );
                        }
                        break;
#endif
                    case NVAT_IP:
                        CE_WRONG_VALUE( NvToIP( (nv_ip_t*) pvAddr, szArg ), szArg );
                        break;

                    case NVAT_FS_TYPE:
                        CE_WRONG_VALUE( NvToFS( (nv_fs_type_e*) pvAddr, szArg ), szArg );
                        break;

                    case NVAT_PART_TYPE:
                        CE_WRONG_VALUE( NvToPart( (nv_part_type_e*) pvAddr, szArg ), szArg );
                        break;

                    case NVAT_OS_TYPE:
                        CE_WRONG_VALUE( NvToOS( (nv_os_type_e*) pvAddr, szArg ), szArg );
                        break;

                    case NVAT_UINT32:  /* no break */
                    case NVAT_UINT64: 
                    {
                            unsigned long long ullVal;
                            CE_WRONG_VALUE( 1 == sscanf( szArg, "%Lx", &ullVal ), szArg );

                            if( NVAT_UINT32 == pParam->eType )
                                    *((uint32_t*) pvAddr ) = ullVal;
                            else if( NVAT_UINT64 == pParam->eType )
                                    *((uint64_t*) pvAddr ) = ullVal;
                            else
                                    CE_WRONG_VALUE( 0, szArg );
                            
                            break;
                    } /* case NVAT_UINT* */

                    case NVAT_ACTION_SEL: 
                    {
                            bFixed = 0;
                            
                            CE_WRONG_VALUE( bIntValid, szArg );
                            
                            switch( eMode ) {
                                case NVPM_PARTITION:
                                    CE_WRONG_VALUE(
                                            ( iIntArg >= 0 ) &&
                                            ( iIntArg < pPartTable->uiEntries ), szArg );
                                    bFixed = pPartTable->axEntries[ iIntArg ].flags.bFixed;
                                    break;
                                case NVPM_OS:
                                    CE_WRONG_VALUE(
                                            ( iIntArg >= 0 ) &&
                                            ( iIntArg < pOSTable->uiEntries ), szArg );
                                    break;
                                default:
                                    CE_WRONG_VALUE( 0, argv[ i ] );
                            }
                            
                            iEntry = iIntArg;
                            break;
                    }

                    case NVAT_ACTION_ADD:
                        switch( eMode ) {
                            case NVPM_PARTITION:
                                iEntry = pPartTable->uiEntries;
                                CE( NvCriticalPartAdd( NULL ) );
                                break;
                            case NVPM_OS:
                                iEntry = pOSTable->uiEntries;
                                CE( NvOSCfgAdd( NVOS_NONE, 0 ) );
                                break;
                            default:
                                CE_WRONG_VALUE( 0, argv[ i ] );
                        }
                        
                        break; /* NVAT_ACTION_ADD */
                        
                    case NVAT_ACTION_DEL:
                        switch( eMode ) {
                            case NVPM_PARTITION:
                                CE_WRONG_VALUE( pPartTable->uiEntries > 0, argv[ i ] );
                                CE( NvCriticalPartDelete( iEntry ) );
                                if( pPartTable->uiEntries <= iEntry )
                                        bFixed = 0;
                                else
                                        bFixed = pPartTable->axEntries[ iEntry ].flags.bFixed;
                                break;
                            case NVPM_OS:
                                CE_WRONG_VALUE( pOSTable->uiEntries > 0, argv[ i ] );
                                /* move partitions down */
                                memcpy( &pOSTable->axEntries[ iEntry ],
                                        &pOSTable->axEntries[ iEntry + 1 ],
                                        ARRAY_SIZE( pOSTable->axEntries ) - iEntry - 1 );
                                /* reset last partition */
                                CLEAR( pOSTable->axEntries[ ARRAY_SIZE( pOSTable->axEntries ) - 1 ] );
                                pOSTable->uiEntries--;
                                break;
                            default:
                                CE_WRONG_VALUE( 0, argv[ i ] );
                        }

                        iEntry = -1; /* invalidate it */
                        break;  /* NVAT_ACTION_DEL */

                    case NVAT_PART_NAME: 
                        CE_WRONG_VALUE( COPY_TO_SET( xPartTable.axEntries[ iEntry ].szName,
                                                     szArg ), szArg );
                        break;

                } /* switch */
                
                i++;
        } /* while( i < argc ) */

        return 1;

error:
        return 0;
}

static int NvPrivFindParam(
        const nv_param_t** ppParam,
        const char* szArg,
        const char** pszValue,
        const nv_param_mode_e eMode,
        char bForSet )
{
        int i = 0;
        int iRes = 1;
        const nv_param_t* pParamFound = NULL;

        if( NULL != pszValue )
                *pszValue = NULL;

        for( i = 0; i < ARRAY_SIZE( l_axArg ); i++ ) {
                const nv_param_t* pParam = &l_axArg[ i ];
                int iLen = strlen( pParam->szName );
                
                if( !strncmp( pParam->szName, szArg, iLen ) &&
                    ( ( NVPM_ANY == eMode ) ||
                      ( NVAT_ACTION_MODE_SEL == pParam->eType ) ||
                      ( pParam->eMode == eMode ) ) ) {
                        /* szArg can be longer than szName, ensure that it has
                         * either "=" or nothing */
                        char bHasAssign = ( '=' == szArg[ iLen ] );
                        char bIsEOS = !szArg[ iLen ];/* has at least len iLen*/
                        char bFound = 0;

                        if( bHasAssign || bIsEOS ) {
                                /* everything for set */
                                bFound |= ( bForSet &&
                                            ( ( pParam->bNeedsVal  && bHasAssign ) ||
                                              ( !pParam->bNeedsVal && bIsEOS ) ) );
                                /* mac= or mac */
                                bFound |= ( !bForSet && !bHasAssign && bIsEOS );
                                /* partition selection=5 */
                                bFound |= ( !bForSet &&
                                            ( pParam->eType == NVAT_ACTION_SEL ) &&
                                            bHasAssign );
                                if( bFound ) {
                                        if( NULL != pszValue )
                                                *pszValue = &szArg[ iLen + bHasAssign];
                                        pParamFound = pParam;
                        
                                        break;
                                }
                        }
                }
        }

        *ppParam = pParamFound;

        if( NULL == pParamFound )
                iRes = NV_SET_ERROR( NVE_INVALID_ARG, szArg );

        return iRes;
}

static void NvPrivPrintParamModule(
        /*@in@*/ const struct nv_param_module_id* pModule,
        char bPrintReserved )
{
        int i;
        
        NvPrivOSPrintf( "  Module ID\n" );
        NvPrivOSPrintf( "    P/N:          %s\n", pModule->szProductType );    
        NvPrivOSPrintf( "    S/N:          %s\n", pModule->szSerialNr );     
        NvPrivOSPrintf( "    Revision:     %x\n", pModule->uiRevision );
        NvPrivOSPrintf( "    Patchlevel:   %u\n", pModule->uiPatchLevel );

        /* printf MAC for each device */
        for( i = 0; i < ARRAY_SIZE( pModule->axMAC ); i++ )
                NvPrivOSPrintf( "    MAC %2i:       %s\n", i + 1,
                        NvToStringMAC( pModule->axMAC[ i ] ) );

        if( bPrintReserved )
                PRINT_RESERVED( "    Reserved: ",
                                pModule->auiReserved );
}

static void NvPrivPrintParamIP(
        /*@in@*/ const struct nv_param_ip* pIP,
        char bPrintReserved )
{
        int i;
        
        NvPrivOSPrintf( "  IP\n" );
        for( i = 0; i < ARRAY_SIZE( pIP->axDevice ); i++ ) {
                const nv_param_ip_device_t* pDev = &pIP->axDevice[ i ];
                NvPrivOSPrintf( "    Device %i\n", i );
                NvPrivOSPrintf( "      IP:       %s\n",
                        NvToStringIP( pDev->uiIP ) );
                NvPrivOSPrintf( "      NetMask:  %s\n",
                        NvToStringIP( pDev->uiNetMask ) );
                NvPrivOSPrintf( "      DHCP:     %s\n",
                        pDev->flags.bDHCP ? "yes" : "no" );
                if( bPrintReserved )
                        PRINT_RESERVED( "      Reserved: ",pDev->auiReserved );
        }
        
        NvPrivOSPrintf( "    Gateway:    %s\n", NvToStringIP( pIP->uiIPGateway ) );
        for( i = 0; i < ARRAY_SIZE( pIP->auiIPDNS ); i++ )
                NvPrivOSPrintf( "    DNS %i:      %s\n",
                                i,
                                NvToStringIP( pIP->auiIPDNS[ i ] ) );

        NvPrivOSPrintf( "    Server:     %s\n", NvToStringIP( pIP->uiIPServer ) );
        NvPrivOSPrintf( "    NetConsole: %s\n", NvToStringIP( pIP->uiIPNetConsole ) );
        if( bPrintReserved )
                PRINT_RESERVED( "    Reserved: ", pIP->auiReserved );
}

static void NvPrivPrintParamPartitionTable(
        /*@in@*/ const struct nv_param_part_table* pPartTable,
        char bPrintReserved )
{
        int i;
        
        NvPrivOSPrintf( "  Partition Table\n" );
        if( !pPartTable->uiEntries )
                return;

        NvPrivOSPrintf( "   Nr | Name                 | Chip | Start       | Size        | Type         | FS      | Flags\n" );
        NvPrivOSPrintf( "   ---|----------------------|------|-------------|-------------|--------------|---------|------\n" );
        for( i = 0; i < pPartTable->uiEntries; i++ ) {
                const nv_param_part_t* pPart = &pPartTable->axEntries[ i ];

                NvPrivOSPrintf( "   %2i | %-20s | %-4i | 0x%09" PRINTF_QUAD "x | 0x%09" PRINTF_QUAD "x | %-12s | %-7s | ",
                                i, pPart->szName,
                                pPart->uiChip,
                                pPart->ullStart, pPart->ullSize,
                                NvToStringPart( pPart->eType ),
                                NvToStringFS( pPart->flags.fs.eType ) );

                if( pPart->flags.bFixed )
                        NvPrivOSPrintf( "fixed " );
                if( pPart->flags.bReadOnly )
                        NvPrivOSPrintf( "readonly " );

                /* they don't make sense if not FS, but print them always */
                if( pPart->flags.fs.bMountReadOnly )
                        NvPrivOSPrintf( "mount-readonly " );
                if( pPart->flags.fs.bRoot )
                        NvPrivOSPrintf( "rootfs " );

                if( pPart->flags.fs.uiVersion )
                        NvPrivOSPrintf( " FS Version: %u",
                                        pPart->flags.fs.uiVersion );

                NvPrivOSPrintf( "\n" );

                if( bPrintReserved )
                        PRINT_RESERVED( "        Reserved: ",pPart->auiReserved);
        }

        if( bPrintReserved )
                PRINT_RESERVED( "    Reserved: ", pPartTable->auiReserved );
}

static void NvPrivPrintParamOSTable(
        /*@in@*/ const struct nv_param_os_cfg_table* pOSTable,
        char bPrintReserved )
{
        int i;

        NvPrivOSPrintf( "  OS Configuration Table\n" );
        if( !pOSTable->uiEntries )
                return;
        
        NvPrivOSPrintf( "   Type        | Start      | Size     \n" );
        NvPrivOSPrintf( "   ------------|------------|-----------\n" );
        
        for( i = 0; i < pOSTable->uiEntries; i++ ) {
                const nv_param_os_cfg_t* pOS = &pOSTable->axEntries[ i ];
                NvPrivOSPrintf( "   %-11s | 0x%08x | 0x%08x\n",
                                NvToStringOS( pOS->eType ),
                                pOS->uiStart, pOS->uiSize );
                if( bPrintReserved )
                        PRINT_RESERVED( "    Reserved: ", pOS->auiReserved );
        }
        
        if( bPrintReserved )
                PRINT_RESERVED( "    Reserved: ", pOSTable->auiReserved );
}
