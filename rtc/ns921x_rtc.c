/*
 *  rtc/ns921x_rtc.c
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision: 1.9 $
 *  !Author:     Markus Pietrek
 *  !Descr:      Some system registers (SYS_CLOCK and SYS_RTC) have been
 *               changed. Therefore, cc9c_rtc.c is not merged into it.
 *  !References: [1] NS9215 Hardware Reference Manual, Preliminary January 2007
 *               [2] netos:rtc_drv.c 1.4
*/

#include <common.h>

#if defined(CONFIG_RTC_NS921X) && (CONFIG_COMMANDS & CFG_CMD_DATE)

#include <rtc.h>                /* rtc_time */
#include <bcd.h>                /* bcd2bin */

#include <asm-arm/arch-ns9xxx/ns921x_rtc.h>
#include <asm-arm/arch-ns9xxx/ns921x_sys.h>
#include <asm-arm/arch-ns9xxx/io.h>  /* rtc_readl */

#define CLEAR( x ) memset( &x, 0, sizeof( x ) )

#define TIMEOUT		1000000 /* us */

static void rtc_cold_boot( void )
{
        unsigned int uiTimeout = TIMEOUT;

        /* configure RTC for CPU access */
        sys_rmw32( SYS_RTC, |  SYS_RTC_INT_READY_CLEAR );
        sys_rmw32( SYS_RTC, & ~SYS_RTC_INT_READY_CLEAR );

        /* [2] */
        sys_rmw32( SYS_RTC, | SYS_RTC_NORMAL );
        while( ! (sys_readl( SYS_RTC ) & SYS_RTC_INT_READY ) ) {
                /* wait for RTC to become ready */
                if( !uiTimeout ) {
                        eprintf( "RTC Timed Out\n" );
                        break;
                }
                uiTimeout--;
                udelay( 1 );
        }

        while( ! (sys_readl( SYS_RTC ) & SYS_RTC_NORMAL_STAT ) ) {
                /* wait for RTC to become accessable */
                if( !uiTimeout ) {
                        eprintf( "RTC Timed Out\n" );
                        break;
                }
                uiTimeout--;
                udelay( 1 );
        }
}

/**
 * rtc_lowlevel_init - setups the hardware
 */
void rtc_lowlevel_init( void )
{
        static char bAlreadyInitialized = 0;

        if( !bAlreadyInitialized ) {
                sys_rmw32( SYS_CLOCK, | SYS_CLOCK_RTC );

                if( ! (sys_readl( SYS_RTC ) & SYS_RTC_NORMAL_STAT ) )
                        /* RTC has not been started yet, e.g. reset */
                        rtc_cold_boot();

                sys_rmw32( SYS_RTC,  | SYS_RTC_INT_READY_CLEAR );
                sys_rmw32( SYS_RTC, &~ SYS_RTC_INT_READY_CLEAR );

                rtc_writel( RTC_24H_24, RTC_24H );

                /* clear events */
                rtc_readl( RTC_EVENT );

                /* kick it if not running */
                rtc_rmw32( RTC_CTRL, & ~( RTC_CTRL_CAL | RTC_CTRL_TIME ) );

                bAlreadyInitialized = 1;
        }
}

/**
 * rtc_reset - bring rtc into known state (same as battery of)
 */
void rtc_reset( void )
{
        rtc_lowlevel_init();
}

/**
 * rtc_get - returns current time/date
 */
void rtc_get( struct rtc_time* pTime )
{
        u32 uiDate;
        u32 uiDateDetectWrap;
        u32 uiTime;
        u32 uiStatus;

        rtc_lowlevel_init();

        uiDate = rtc_readl( RTC_DATE );
        uiTime = rtc_readl( RTC_TIME );

        /* date could have wrapped around just after a read, so time is
         * 00:00:00 but date is from yesterday. Detect this and read again  */
        uiDateDetectWrap = rtc_readl( RTC_DATE );
        if( uiDate != uiDateDetectWrap ) {
                /* wrapped, use new date and get new time */
                uiDate = uiDateDetectWrap;
                uiTime = rtc_readl( RTC_TIME );
        }

        uiStatus = rtc_readl( RTC_GEN_STAT );
        if( ( uiStatus & RTC_GEN_STAT_ALL_VALID ) != RTC_GEN_STAT_ALL_VALID )
                eprintf( "RTC reports invalid configuration: 0x%08x\n",
                         uiStatus );

        /* convert to U-Boot */
        CLEAR( *pTime );

        pTime->tm_sec  = bcd2bin( RTC_TIME_S_GET( uiTime ) );
        pTime->tm_min  = bcd2bin( RTC_TIME_M_GET( uiTime ) );
        pTime->tm_hour = bcd2bin( RTC_TIME_HR_GET( uiTime ) );
        pTime->tm_mday = bcd2bin( RTC_DATE_D_GET( uiDate ) );
        pTime->tm_mon  = bcd2bin( RTC_DATE_M_GET( uiDate ) );
        pTime->tm_year = bcd2bin( RTC_DATE_C_GET( uiDate ) ) * 100 +
                bcd2bin( RTC_DATE_Y_GET( uiDate ) );
        pTime->tm_wday = bcd2bin( RTC_DATE_DAY_GET( uiDate ) - 1 );
        /* yday and isdst not set */
}

/**
 * rtc_set - updates battery powered RTC
 */
void rtc_set( struct rtc_time* pTime )
{
        u32 uiDate;
        u32 uiTime;
        u32 uiStatus;

        rtc_lowlevel_init();

        uiTime =
                RTC_TIME_S_SET( bin2bcd( pTime->tm_sec ) ) |
                RTC_TIME_M_SET( bin2bcd( pTime->tm_min ) ) |
                RTC_TIME_HR_SET( bin2bcd( pTime->tm_hour ) );
        uiDate =
                RTC_DATE_D_SET( bin2bcd( pTime->tm_mday ) ) |
                RTC_DATE_M_SET( bin2bcd( pTime->tm_mon  ) )  |
                RTC_DATE_Y_SET( bin2bcd( pTime->tm_year % 100 ) ) |
                RTC_DATE_C_SET( bin2bcd( pTime->tm_year / 100 ) ) |
		/* NET+OS saves Sunday as 1, so let's do that, too */
                RTC_DATE_DAY_SET( bin2bcd( pTime->tm_wday ) + 1 );

        /* disable RTC operation while we update */
        rtc_rmw32( RTC_CTRL, | ( RTC_CTRL_CAL | RTC_CTRL_TIME ) );

        rtc_writel( uiTime, RTC_TIME );
        /* msecs are 0. This gives us a whole second to write date */
        rtc_writel( uiDate, RTC_DATE );

        /* check for valid settings. do_date() performs a get() after a set(),
         * but* someone else might not */
        uiStatus = rtc_readl( RTC_GEN_STAT );
        if( ( uiStatus & RTC_GEN_STAT_ALL_VALID ) != RTC_GEN_STAT_ALL_VALID )
                eprintf( "RTC reports invalid configuration: 0x%08x\n",
                         uiStatus );

        /* let it continue */
        rtc_rmw32( RTC_CTRL, & ~( RTC_CTRL_CAL | RTC_CTRL_TIME ) );
}

#endif  /* defined(CONFIG_RTC_NS921X) && (CONFIG_COMMANDS & CFG_CMD_DATE) */
