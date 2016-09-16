/*
 *  cpu/arm926ejs/ns921x/serial_ns921x.c
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

#ifdef CFG_NS921X_UART

#ifdef CONFIG_NS9210
# define SERIAL_PORT_B_TX	23
# define SERIAL_PORT_B_RX	19
# define SERIAL_PORT_D_TX	31
# define SERIAL_PORT_D_RX	27
# define GPIO_CFG_FUNC_UART	GPIO_CFG_FUNC_1
#else
# define SERIAL_PORT_B_TX	58
# define SERIAL_PORT_B_RX	54
# define SERIAL_PORT_D_TX	66
# define SERIAL_PORT_D_RX	62
# define GPIO_CFG_FUNC_UART	GPIO_CFG_FUNC_0
#endif

#include <serial.h>             /* serial_ns921x_devices */

#include <asm/errno.h>              /* EINVAL */

#include <asm-arm/arch-ns9xxx/ns921x_sys.h>
#include <asm-arm/arch-ns9xxx/ns921x_hub.h>
#include <asm-arm/arch-ns9xxx/ns921x_gpio.h>
#include <asm-arm/arch-ns9xxx/io.h>  /* gpio_readl */

#include <../common/digi/vscanf.h>  /* sscanf */

#define GPIO_TX		0
#define GPIO_RX		1
#define GPIO_LAST	1

DECLARE_GLOBAL_DATA_PTR;

/*
 * Order of GPIOs is GPIO_TX to GPIO_LAST and function
 * Use always function as last element of the array
 */
static const char l_aiGPIO[][3] = {
	{  7,  3,  GPIO_CFG_FUNC_0 }, /* Port A */
	{ SERIAL_PORT_B_TX, SERIAL_PORT_B_RX, GPIO_CFG_FUNC_UART }, /* Port B */
	{ 15, 11, GPIO_CFG_FUNC_0 }, /* Port C */
	{ SERIAL_PORT_D_TX, SERIAL_PORT_D_RX, GPIO_CFG_FUNC_UART }  /* Port D */
};

/* the port we are using */
static int l_iPort = -1;        /* not initialized yet */

/* for caching characters and l_xFIFO */
static struct {
        u32 uChars;
        int iCount;
} l_xFIFO = { 0, 0 };

/* some functions that map the HUB port  */
static inline u32 hub_readl( u32 uOffs )
{
        return hub_port_readl( l_iPort, uOffs );
}

static inline u8 hub_readb( u32 uOffs )
{
        return hub_port_readb( l_iPort, uOffs );
}

static inline void hub_writel( u32 uVal, u32 uOffs )
{
        hub_port_writel( l_iPort, uVal, uOffs );
}

static inline void hub_writeb( u8 ucVal, u32 uOffs )
{
        hub_port_writeb( l_iPort, ucVal, uOffs );
}

/**
 * ns_serial_setbrg - set baudrate register
 */
void ns_serial_setbrg( void )
{
        u32 uPeriod = NS_CPU_REF_CLOCK / gd->baudrate;
        u32 uDiv    = uPeriod / 16; /* 16 bit oversampling */
        /* [1], p. 359 by default we use 8N1. 8 character bit, one start, one
         * stopbit */
        static const u32 uCharLen = 10;

        hub_writel( UART_CGAP_CTRL_EN |
                       UART_CGAP_CTRL_VAL( uCharLen * uPeriod - 1 ), UART_CGAP_CTRL );
        /* [1], p. 360, 64 characters, 640 bit periods */
        hub_writel( UART_BGAP_CTRL_EN |
                       UART_BGAP_CTRL_VAL( 64 * uCharLen * uPeriod - 1 ), UART_BGAP_CTRL );

        /* baudrate divisor */
        hub_rmw32( UART_LINE_CTRL, | UART_LINE_CTRL_DLAB );
        hub_writel( UART_BRDL_VAL( uDiv ),      UART_BRDL );
        hub_writel( UART_BRDM_VAL( uDiv >> 8 ), UART_BRDM );
        hub_rmw32( UART_LINE_CTRL, & ~UART_LINE_CTRL_DLAB );
}

/**
 * ns_serial_init - initialises UART Module
 */
static int ns_serial_init( void )
{
        int i;

        /* configure GPIOs for special function */
         for( i = 0; i < ( ARRAY_SIZE( l_aiGPIO[ l_iPort ] ) - 1 ); i++ )
                gpio_cfg_set( l_aiGPIO[ l_iPort ][ i ],
                              l_aiGPIO[ l_iPort ][ ARRAY_SIZE( l_aiGPIO[ l_iPort ] ) - 1 ] );

        /* enable clock and hold it out-of-reset */
        sys_rmw32( SYS_CLOCK, | SYS_CLOCK_UART( l_iPort ) );
        sys_rmw32( SYS_RESET, | SYS_RESET_UART( l_iPort ) );

        /* toggle RX/TX FLUSH for resetting FIFO */
        hub_writel( UART_WRAPPER_CFG_RX_FLUSH |
                       UART_WRAPPER_CFG_TX_FLUSH,
                       UART_WRAPPER_CFG );
        hub_writel( 0, UART_WRAPPER_CFG );

        /* no DMA, we poke into FIFO */
        hub_writel( HUB_DMA_RX_CTRL_DIRECT, HUB_DMA_RX_CTRL );
        hub_writel( HUB_DMA_TX_CTRL_DIRECT, HUB_DMA_TX_CTRL );

        ns_serial_setbrg();

        /* set line control to 8 Data Bits, No Parity, 1 Stop Bit */
        hub_writel( UART_LINE_CTRL_PAR_NO |
                       UART_LINE_CTRL_STOP_1  |
                       UART_LINE_CTRL_WLS_8, UART_LINE_CTRL );

        /* enable UARTs internal Rx/Tx FIFO */
        hub_writel( UART_FIFO_CTRL_EN, UART_FIFO_CTRL );

        /* This interrupt is needed from HW (Hub) as transmitter holding
         * register. SW doesn't use it */
        hub_writel( UART_BAUD_INT_ETBEI, UART_BAUD_INT );

        /* enable wrapper */
        hub_writel( UART_WRAPPER_CFG_RX_EN      |
                       UART_WRAPPER_CFG_TX_EN      |
                       UART_WRAPPER_CFG_TX_FLOW_SW |
                       UART_WRAPPER_CFG_MODE_UART, UART_WRAPPER_CFG );

        return 0;
}

/**
 * ns_serial_deinit - deinitialises UART Module
 */
static int ns_serial_deinit( void )
{
        int i;

        /* configure GPIOs as INPUT */
        for( i = 0; i < ( ARRAY_SIZE( l_aiGPIO[ l_iPort ] ) - 1 ); i++ )
                gpio_cfg_set( l_aiGPIO[ l_iPort ] [ i ], GPIO_CFG_INPUT );

        /* disable wrapper */
        hub_writel( 0, UART_WRAPPER_CFG );

        /* disable clock and put in reset */
        sys_rmw32( SYS_CLOCK, & ~SYS_CLOCK_UART( l_iPort ) );
        sys_rmw32( SYS_RESET, & ~SYS_RESET_UART( l_iPort ) );

        return 0;
}

/**
 * serial_disable - disables UART Module
 */
int serial_disable( void )
{
#ifdef CONFIG_ALLOW_SERIAL_DISABLE
	int i;

	for(i=0; i < SERIAL_PORTS_NR; i++) {
		l_iPort = i;
		/* disable UARTs internal Rx/Tx FIFO */
		hub_writel( 0, UART_WRAPPER_CFG );
	}
#endif
	return 0;
}

/**
 * ns_serial_start - starts the console.
 */
static int ns_serial_start( int iPort )
{
        int iRes = -EINVAL;

        if( ( iPort >= 0 ) && ( iPort < 4 ) ) {
                /* switch to new console */
                if( -1 != l_iPort )
                                /* not initialized yet */
                        ns_serial_deinit();

                l_iPort = iPort;
                ns_serial_init();

                iRes = 0;
        } else {
                /* eprintf may not be available yet */
                printf( "*** ERROR: Unsupported port\n" );
                iRes = -EINVAL;
        }

        return iRes;
}

/**
 * ns_serial_putc - outputs one character
 */

static void ns_serial_putc( const char c )
{
        /* generate CR/LF */
        if( '\n' == c )
                ns_serial_putc( '\r' );

        /* wait for space available */
        while( ( hub_readl( HUB_INT ) & HUB_INT_TX_FIFO_FULL ) ) {
                /* do nothing, wait for character to be sent */
        }

        hub_writeb( c, HUB_TX_FIFO );
}

/**
 * ns_serial_puts - outputs a zero terminated string
 */
static void ns_serial_puts( const char* szMsg )
{
	while( *szMsg )
		ns_serial_putc( *szMsg++ );
}

/**
 * ns_serial_tstc - checks for input available
 * @return: 0 if no input available, otherwise != 0
 */
int ns_serial_tstc( void )
{
        if( !l_xFIFO.iCount ) {
                /* don't use HUB_RX_FIFO_STAT for checking of empty, it doesn't
                 * handle buffer closed well. */
                if( !( hub_readl( HUB_INT ) & HUB_INT_RX_FIFO_EMPTY ) ) {
                        /* both reads (status and data) must be together */
                        l_xFIFO.iCount = HUB_RX_FIFO_BYTE( hub_readl( HUB_RX_FIFO_STAT ) );

	                l_xFIFO.uChars = hub_readl( HUB_RX_FIFO );
                }

                /* uChars may still be undefined if iCount == 0 */
        }

        return ( l_xFIFO.iCount != 0 );
}


/**
 * ns_serial_getc - returns one character
 */
static int ns_serial_getc( void )
{
        u8 ucChar;

        while( !ns_serial_tstc() ) {
                /* do nothing, wait for incoming character */
        }

        l_xFIFO.iCount--;
        ucChar = l_xFIFO.uChars & 0xff;
        l_xFIFO.uChars >>= 8;
        return ucChar;
}

/**
 * ns_serial_tx_flush - returns when all characters have been flushed out
 */
static void ns_serial_tx_flush( void )
{
        while( !( hub_readl( HUB_INT ) & HUB_INT_TX_FIFO_EMPTY ) ) {
                /* do nothing, wait for all characters to be sent */
        }

        /* wait for character to be really out */
        while( !( hub_readl( UART_LINE_STAT ) & UART_LINE_STAT_TEMT ) ) {
                /* do nothing, wait for character to be sent */
        }
}

/* some stuff to provide serial0...serial3 */
#define MK( port ) \
static int ns_serial_start_##port( void ) \
{ \
        ns_serial_start( port ); \
        return 0; \
}
MK( 0 )
MK( 1 )
MK( 2 )
MK( 3 )
#undef MK

#define MK( port ) \
        { .name     = "serial"#port,            \
          .init     = ns_serial_start_##port,   \
          .setbrg   = ns_serial_setbrg, \
          .getc     = ns_serial_getc,   \
          .tstc     = ns_serial_tstc,   \
          .putc     = ns_serial_putc,   \
          .puts     = ns_serial_puts,     \
          .tx_flush = ns_serial_tx_flush    \
        }

struct serial_device serial_ns921x_devices[ 4 ] = {
        MK( 0 ),
        MK( 1 ),
        MK( 2 ),
        MK( 3 )
};
#undef MK

#endif  /* CFG_NS921X_UART */
