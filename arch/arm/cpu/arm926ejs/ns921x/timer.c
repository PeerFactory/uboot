/*
 *  cpu/arm926ejs/ns921x/timer.c
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
 *               [2] cc9x/timer.c
 *  !Descr:      U-Boot uses 4 timers.
 *               Timer 0 provides a 1ms timer. This is for long
 *               lasting test using get_timer().
 *               Timer 1 is concated to Timer 0 and counts the ms since reset.
 *               This gives a wraparound every 4 Mio. seconds or every 46 days
 *               Therefore there is no SW encounting of wraparounds
 *
 *               Timer 2 and Timer 3 are also concatenated and used for udelay.
 *               Timer 2 provides a pulse with about 1us resolution.
 *               Timer 3 is then loaded with usec value of udelay and
 *               polled for zero.
 *               This gives quite exact udelays.
 *               With 149.91360 MHz, udelay(1us) may be range from 1us to 1.7us
 *
 *               If only one timer is being used, divisions and multiplications
 *               are needed. This makes it impossible to achieve
 *               udelays < 25us because of the calculation overhead.
 *
 *               Calibrating an udelay loop is too unprecise, too. Depending on
 *               the environment it may differ (DMA transfer, framebuffer
 *               active, cache enabled/disabled ) etc.
 *
 *               A precise udelay brings advantage as U-Boot is also being used
 *               for HW tests.
*/

#include <common.h>

/* we are only compiled if CONFIG_NS9215 is present  */

#include <asm-arm/arch-ns9xxx/ns921x_sys.h>
#include <asm-arm/arch-ns9xxx/io.h>  /* gpio_readl */

#define MS_TIMER		0       /* for ms */
#define SYSTEM_TIMER		1       /* for get_timer */
#define US_TIMER		2       /* timer in about 1us resolution */
#define UDELAY_TIMER		3       /* loaded with usec delay */
#define SYSTEM_TIMER_RELOAD    	0

#define US_DISABLE \
        ( SYS_TIMER_CTRL_TCS_M2   | \
          SYS_TIMER_CTRL_TM_INT   | \
          SYS_TIMER_CTRL_DOWN     | \
          SYS_TIMER_CTRL_32       | \
          SYS_TIMER_CTRL_RELOAD )

#define UDELAY_DISABLE \
        ( SYS_TIMER_CTRL_TCS_M2   | \
          SYS_TIMER_CTRL_TM_CONC  | \
          SYS_TIMER_CTRL_DOWN     | \
          SYS_TIMER_CTRL_32 )

#define US_ENABLE      ( US_DISABLE     | SYS_TIMER_CTRL_TE )
#define UDELAY_ENABLE  ( UDELAY_DISABLE | SYS_TIMER_CTRL_TE )

/**
 * sys_clock_freq - determines system clock frequency based on PLL settings
 * @return: clock in Hz
 *
 * System clock is only calculated on the first call
 */
int sys_clock_freq( void )
{
        static u32 uiSysClkFreq = 0;

        if( !uiSysClkFreq ) {
                /* calculate it only once */
                static const u32 uiRefClk = NS_CPU_REF_CLOCK;
                u32 uiPLL    = sys_readl( SYS_PLL );
                u32 uiPLLVC0 = ( uiRefClk / ( SYS_PLL_NR( uiPLL ) + 1 ) ) *
                        ( SYS_PLL_NF( uiPLL ) + 1 );

                uiSysClkFreq  = uiPLLVC0 / ( SYS_PLL_OD( uiPLL ) + 1 );
        }

        return uiSysClkFreq;
}

/**
 * ahb_clock_freq - determines ahb clock frequency based on clock config settings
 * @return: clock in Hz
 */
int ahb_clock_freq( void )
{
        u32 uiClkScale = SYS_CLOCK_CSC( sys_readl( SYS_CLOCK ) );
        u32 uiAHBClockFreq;

        uiAHBClockFreq = ( sys_clock_freq() / 4 ) >> uiClkScale;

        return uiAHBClockFreq;
}

/**
 * cpu_clock_freq - determines cpu clock frequency based on clock config settings
 * @return: clock in Hz
 */
int cpu_clock_freq( void )
{
        u32 uiClkFac = ( sys_readl( SYS_CLOCK ) & SYS_CLOCK_CCSEL ) ? 2 : 1;
        u32 uiCPUClockFreq;

        uiCPUClockFreq = ahb_clock_freq() * uiClkFac;

        return uiCPUClockFreq;
}

/**
 * read_timer - returns timer value
 */
static inline u32 read_timer( void )
{
        return sys_readl( SYS_TIMER_READ( SYSTEM_TIMER ) );
}

/**
 * set_timer - sets the timestamp counter
 */
void set_timer( ulong t )
{
        sys_writel( t, SYS_TIMER_RELOAD( SYSTEM_TIMER ) );
}

void reset_timer( void )
{
        set_timer( 0 );
}

/**
 * timer_init -
 * @return: always 0 on ok
 *
 * initializes Timer 0 as ms timer and Timer 1 concatenated as system timer (for msdelay).
 * initializes Timer 2 as us timer and Timer 3 concatenated for udelay
 */
int timer_init( void )
{
        u32 uiTicksPerMS = 0;

        sys_writel( 0, SYS_TIMER_CTRL( SYSTEM_TIMER ) );  /* disable all */

        /* configure ms timer */
        uiTicksPerMS = ( ahb_clock_freq() * 2 ) / 1000;
        sys_writel( uiTicksPerMS, SYS_TIMER_RELOAD( MS_TIMER ) );
        sys_writel( SYS_TIMER_CTRL_TE       |
                    SYS_TIMER_CTRL_TCS_M2   |
                    SYS_TIMER_CTRL_TM_INT   |
                    SYS_TIMER_CTRL_DOWN     |
                    SYS_TIMER_CTRL_32       |
                    SYS_TIMER_CTRL_RELOAD,
                    SYS_TIMER_CTRL( MS_TIMER ) );
        sys_rmw32( SYS_TIMER_MASTER_CTRL,
                   | SYS_TIMER_MASTER_CTRL_EN( MS_TIMER ) );

        /* configure system timer, concated to ms timer */
        reset_timer();
        sys_writel( SYS_TIMER_CTRL_TE       |
                    SYS_TIMER_CTRL_TCS_M2   |  /* don't divide input clock*/
                    SYS_TIMER_CTRL_TM_CONC  |
                    SYS_TIMER_CTRL_TM_INT   |
                    SYS_TIMER_CTRL_UP       |
                    SYS_TIMER_CTRL_32       |
                    SYS_TIMER_CTRL_RELOAD,
                    SYS_TIMER_CTRL( SYSTEM_TIMER ) );
        sys_rmw32( SYS_TIMER_MASTER_CTRL,
                   | SYS_TIMER_MASTER_CTRL_EN( SYSTEM_TIMER ) );

        /* configure us timer */
        sys_writel( uiTicksPerMS / 1000, SYS_TIMER_RELOAD( US_TIMER ) );
        sys_writel( US_DISABLE,
                    SYS_TIMER_CTRL( US_TIMER ) );
        sys_rmw32( SYS_TIMER_MASTER_CTRL,
                   | SYS_TIMER_MASTER_CTRL_EN( US_TIMER ) );

        /* configure udelay timer, concated to us timer */
        sys_writel( UDELAY_DISABLE,
                    SYS_TIMER_CTRL( UDELAY_TIMER ) );
        sys_rmw32( SYS_TIMER_MASTER_CTRL,
                   | SYS_TIMER_MASTER_CTRL_EN( UDELAY_TIMER ) );

        return 0;
}

ulong get_timer( ulong base )
{
	return read_timer() - base;
}

/* delay x useconds AND perserve advance timstamp value */
void udelay( unsigned long usec )
{
        if( !usec )
                return;

        /* disable us and udelay timer. us should be kicked from a known state */
        sys_writel( US_DISABLE, SYS_TIMER_CTRL( US_TIMER ) );
        sys_writel( UDELAY_DISABLE, SYS_TIMER_CTRL( UDELAY_TIMER ) );
        /* load timer 3 with the intended udelay to wait */
        sys_writel( usec, SYS_TIMER_RELOAD( UDELAY_TIMER ) );

        /* kick them */

        sys_writel( UDELAY_ENABLE, SYS_TIMER_CTRL( UDELAY_TIMER ) );
        sys_writel( US_ENABLE, SYS_TIMER_CTRL( US_TIMER ) );

        while( sys_readl( SYS_TIMER_READ( UDELAY_TIMER ) ) ) {
                /* wait for timer to expire, do nothing */
        }
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long get_ticks( void )
{
	return get_timer( 0 );
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong get_tbclk( void )
{
        return CFG_HZ;
}
