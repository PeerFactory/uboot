/*
 *  board/cc9p921x/board.c
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision$
 *  !Author:     Markus Pietrek
 *  !References: [1] NS9215 Hardware Reference Manual, Preliminary January 2007
*/

#include <common.h>
#include <nvram.h>
#include <net.h>                /* eth_use_mac_from_env */
#include <rtc.h>                /* rtc_get */

#include <asm-arm/arch-ns9xxx/ns9xxx_mem.h>
#include <asm-arm/arch-ns9xxx/ns921x_gpio.h>
#include <asm-arm/arch-ns9xxx/io.h>  /* gpio_readl */
#include <config.h>
#include <status_led.h>         /* also includes io.h */

#include <cmd_chkimg.h>         /* CheckCRC32OfImageInFlash */
#include <configs/cc9c.h>

#define BOOTSTRAP_CS		0x3
#define BOOTSTRAP_BASE_ADDRESS	0x30000000
#define MBitToByte( mbit ) ( ( ( mbit ) * 1024 * 1024 ) / 8 )

DECLARE_GLOBAL_DATA_PTR;

static char l_bBootFailed = 0;

#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
extern void run_auto_script(void);
#endif

/* The following are used to control the SPI chip selects for the SPI command */
#if (CONFIG_COMMANDS & CFG_CMD_SPI)
#include <spi.h>

void spi_chipsel_bitbang(int cs)
{
	if (cs)
		gpio_ctrl_set(SPI_EN_GPIO, 0);
	else
		gpio_ctrl_set(SPI_EN_GPIO, 1);
}

spi_chipsel_type spi_chipsel[] = {
	spi_chipsel_bitbang
};
int spi_chipsel_cnt = sizeof(spi_chipsel) / sizeof(spi_chipsel[0]);
#endif /* CFG_CMD_SPI */

/**
 * handle_user_keys - print's status of user keys and executes associated commands
 */

static void handle_user_keys( void )
{
#ifdef CONFIG_USER_KEY
        static const int aiGPIO[] = {
#ifdef USER_KEY1_GPIO
                USER_KEY1_GPIO,
#endif
#ifdef USER_KEY2_GPIO
                USER_KEY2_GPIO
#endif
        };

        int i;

        /* check what keys are pressed and whether there is a script */
        for( i = 0; i < ARRAY_SIZE( aiGPIO ); i++ ) {
                /* as input */
                gpio_cfg_set( aiGPIO[ i ],
                              GPIO_CFG_INPUT | GPIO_CFG_FUNC_GPIO );

                if( !gpio_stat_get( aiGPIO[ i ] ) ) {
                        char szCmd[ 30 ];

                        printf( "\nUser Key %i pressed\n", i + 1 );
                        sprintf( szCmd, "key%i", i + 1 );

                        if( getenv( szCmd ) != NULL ) {
                                sprintf(szCmd, "run key%i", i + 1);
                                run_command( szCmd, 0 );
                        }
                }
        } /* for( i = 0; ) */
#endif /* CONFIG_USER_KEY */
}

/**
 * board_print_cpu_status - print's some CPU and reset information
 */

static void print_board_cpu_status( void )
{
        const char*      szCause = NULL;
        u32 uSysClkFreq = sys_clock_freq();
        u32 uCPUClkFreq = cpu_clock_freq();
        u32 uAHBClkFreq = ahb_clock_freq();
        u32 uSysMisc    = sys_readl( SYS_MISC );
        u32 uReset      = sys_readl( SYS_RESET );
        u32 uAHB        = sys_readl( SYS_AHB_ERROR2 );

#define CLOCK( clock ) (clock) / 1000000, ( ( (clock) % 1000000 ) / 10 )
	printf( "CPU:   %s @ %i.%i MHz, SYS %i.%i MHz, AHB %i.%i MHz, Rev %i\n",
                CPU,
                CLOCK( uCPUClkFreq ),
                CLOCK( uSysClkFreq ),
                CLOCK( uAHBClkFreq ),
                SYS_MISC_REV( uSysMisc ) );
#undef CLOCK
        printf( "Strap: 0x%04x\n", sys_readl(  SYS_GEN_ID ) );

        /* Reset cause */
        switch( uReset & SYS_RESET_STAT_MA ) {
            case SYS_RESET_STAT_SW_WDOG:
                szCause = "CPU Watchdog";
                break;
            case SYS_RESET_STAT_AHB:
                szCause = "AHB";
                break;
            case SYS_RESET_STAT_SRESN:
                szCause = "SRESET";
                break;
            case SYS_RESET_STAT_PLL:
                /* this is the software reset command. This should be normal
		 * The AHB Error information is not reset during software reset
		 * and should be considered invalid */
		return;
            case SYS_RESET_STAT_RESN:
                /* normal poweron. The AHB Error information is not reset
		 * during power up and should be considered invalid */
		return;
            default:
                eprintf( "Unknown reset cause: 0x%08x\n", uReset );
                break;
        }
        if( NULL != szCause )
                printf( "Reset: %s\n", szCause );

        /* AHB Error */
        if( uAHB &
            ( SYS_AHB_ERROR2_IE |
              SYS_AHB_ERROR2_DE |
              SYS_AHB_ERROR2_ER ) ) {
                /* clear it */
                sys_rmw32( SYS_AHB_MON, | SYS_AHB_MON_EIC );
                sys_rmw32( SYS_AHB_MON, & ~SYS_AHB_MON_EIC );
        } /* if( uAHB ) */
}

static void inline user_led_init(void)
{
#ifndef CONFIG_CC9C
	gpio_cfg_set( USER_LED1_GPIO,
                              GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO );
	gpio_cfg_set( USER_LED2_GPIO,
                              GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO );
	gpio_ctrl_set(USER_LED1_GPIO, 0);
	gpio_ctrl_set(USER_LED2_GPIO, 0);
#endif
}

#if defined(CONFIG_SILENT_CONSOLE) && defined(ENABLE_CONSOLE_GPIO)
static void init_console_gpio(void)
{
	gpio_cfg_set( ENABLE_CONSOLE_GPIO, GPIO_CFG_INPUT | GPIO_CFG_FUNC_GPIO);
}
#endif

/**
 * board_init - early board initialization
 * @return: always 0
 */
int board_init( void )
{
        /* icache already initialized in ASM */

        /* we want an interrupt in case of an AHB error */
        sys_writel( SYS_AHB_MON_SERDC, SYS_AHB_MON );

	gd->bd->bi_arch_number = machine_arch_type;
	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x00000100;

	user_led_init();

#if defined(CONFIG_SILENT_CONSOLE) && defined(ENABLE_CONSOLE_GPIO)
	init_console_gpio();
#endif

#ifdef	SPI_INIT
	SPI_INIT;
#endif

	return 0;
}

/**
 * misc_init_r -
 */
int misc_init_r( void )
{
	/* nothing to do */
	return 0;
}

void dev_board_info( void )
{
	/* Nothing to do */
}


/**
 * dram_init - initializes board SDRAM information
 *
 * @return: always 0
 *
 * Either the Debugger or platform.S:lowlevel_init have already setup the
 * Memory Controller CS0 settings. We take the size information and setup the
 * system controller's CS0 memory mask. The memory mask is reduced to detect
 * more easily SW faults using wrong addresses.
 *
 * Only CS0 is probed.
 */
int dram_init( void )
{
        u32 uiSize;             /* in bytes */
        u32 uiMask;
        u32 uiMem = mem_readl( MEM_DYN_CFG( 0 ) );

        /* [1], p. 208. Bits 11:9 encodes the size of SDRAM for all kinds of
           memory. We use this as an index into auiMemSize */
        static const u32 auiMemSize[ 8 ] = {
                MBitToByte( 16 ),
                MBitToByte( 64 ),
                MBitToByte( 128 ),
                MBitToByte( 256 ),
                MBitToByte( 512 ),
                0,
                0,
                0,
        };

        uiSize = auiMemSize[ MEM_DYN_CFG_AM1_SIZE( uiMem ) ];

        /* set mask for detecting wrong accesses */
        uiMask = ~(uiSize - 1) & 0xFFFFF000;
        sys_writel( uiMask | SYS_CS_DYN_MASK_EN, SYS_CS_DYN_MASK( 0 ) );

        /* tell U-Boot what we have */
	gd->bd->bi_dram[ 0 ].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[ 0 ].size  = uiSize;

        /* dcache depends on bi_dram */
        dcache_enable();

	return 0;
}

#if (COMMANDS & CFG_CMD_DATE)
/**
 * print_date - prints date/time
 */
void print_date( void )
{
	struct rtc_time tm;

         /* rtc_get should be called in any case as it kicks a not running
          * clock */
      	rtc_get( &tm );

        printf( "Date:  %02d:%02d:%02d - %04d-%02d-%02d\n",
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                tm.tm_year, tm.tm_mon, tm.tm_mday );
}
#else
void print_date( void )
{
}
#endif

int eth_use_mac_from_env( bd_t* pbis );
/**
 * board_late_init - last step before console
 */
int board_late_init( void )
{
        const nv_info_t* pInfo  = NULL;
	nv_critical_t*   pNvram = NULL;

        print_board_cpu_status();
        print_date();

	if( NvCriticalGet( &pNvram ) )
                pInfo = NvInfo();

        if( ( pInfo == NULL ) || pInfo->bAnyBad )
                /* we should really boot with a valid NVRAM. Default Workcopy
                 * is always present, but shouldn't never happend */
                l_bBootFailed |= 1;

        /* copy bdinfo to start of boot-parameter block */
 	memcpy( (int*)gd->bd->bi_boot_params, gd->bd, sizeof( bd_t ) );
#if (CONFIG_COMMANDS & CFG_CMD_NET)
        eth_use_mac_from_env( gd->bd );
#endif

#if defined(CONFIG_UBOOT_CHECK_CRC32_ON_BOOT) && defined(CFG_APPEND_CRC32) && !defined(CFG_NO_FLASH)
        if( !CheckCRC32OfImageInFlash( 1 ) )
                l_bBootFailed |= 1;
#endif
#ifdef CONFIG_STATUS_LED
        status_led_set( STATUS_LED_BOOT, STATUS_LED_ON );
        status_led_set( STATUS_LED_ERROR,
                        ( l_bBootFailed ? STATUS_LED_ON : STATUS_LED_OFF ) );
#endif
        /* handle_user_keys might never return */
        handle_user_keys();

#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
	run_auto_script();
#endif

	return 0;
}

#ifdef CONFIG_SHOW_BOOT_PROGRESS
void show_boot_progress( int status )
{
	if( status < 0 ) {
#ifdef CONFIG_STATUS_LED
        	/* ready to transfer to kernel, make sure LED is proper state */
        	status_led_set( STATUS_LED_BOOT, STATUS_LED_ON );
#endif
	}
}
#endif /* CONFIG_SHOW_BOOT_PROGRESS */

#if defined(CONFIG_SILENT_CONSOLE)
int test_console_gpio (void)
{
#if defined(ENABLE_CONSOLE_GPIO) && defined(CONSOLE_ENABLE_GPIO_STATE)
	if (gpio_stat_get(ENABLE_CONSOLE_GPIO) == CONSOLE_ENABLE_GPIO_STATE)
		return 1;
	else
		return 0;
#else
	return 0;
#endif
}
#endif
