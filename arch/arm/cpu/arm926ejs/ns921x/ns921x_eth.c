/*
 *  cpu/arm926ejs/ns921x/ns921x_eth.c
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision: 1.2 $
 *  !Author:     Markus Pietrek
 *  !References: [1] derived from ns9750_eth.c, 1.29
*/

#include <common.h>
#include <net.h>		/* NetSendPacket */
#include <miiphy.h>		/* miiphy_register */

#include "ns9750_eth.h"		/* for Ethernet and PHY */
#include <asm-arm/arch-ns9xxx/ns921x_gpio.h>
#include <asm-arm/arch-ns9xxx/io.h>

#if defined(CONFIG_DRIVER_NS921X_ETHERNET) && \
	(CONFIG_COMMANDS & CFG_CMD_NET)

/* some definition to make transistion to linux easier */

#define NS921X_DRIVER_NAME	"eth"
#define KERN_WARNING 		"Warning:"
#define KERN_ERR 		"Error:"
#define KERN_INFO 		"Info:"

#if 0
# define DEBUG
#endif

#ifdef	DEBUG
# define printk			printf

# define DEBUG_INIT		0x0001
# define DEBUG_MINOR		0x0002
# define DEBUG_RX		0x0004
# define DEBUG_TX		0x0008
# define DEBUG_INT		0x0010
# define DEBUG_POLL		0x0020
# define DEBUG_LINK		0x0040
# define DEBUG_MII		0x0100
# define DEBUG_MII_LOW		0x0200
# define DEBUG_MEM		0x0400
# define DEBUG_ERROR		0x4000
# define DEBUG_ERROR_CRIT	0x8000

static int nDebugLvl = DEBUG_ERROR_CRIT;

# define DEBUG_ARGS0( FLG, a0 ) if( ( nDebugLvl & (FLG) ) == (FLG) ) \
		printf("%s: " a0, __FUNCTION__, 0, 0, 0, 0, 0, 0 )
# define DEBUG_ARGS1( FLG, a0, a1 ) if( ( nDebugLvl & (FLG) ) == (FLG)) \
		printf("%s: " a0, __FUNCTION__, (int)(a1), 0, 0, 0, 0, 0 )
# define DEBUG_ARGS2( FLG, a0, a1, a2 ) if( (nDebugLvl & (FLG)) ==(FLG))\
		printf("%s: " a0, __FUNCTION__, (int)(a1), (int)(a2), 0, 0,0,0 )
# define DEBUG_ARGS3( FLG, a0, a1, a2, a3 ) if((nDebugLvl &(FLG))==(FLG))\
		printf("%s: "a0,__FUNCTION__,(int)(a1),(int)(a2),(int)(a3),0,0,0)
# define DEBUG_FN( FLG ) if( (nDebugLvl & (FLG)) == (FLG) ) \
		printf("\r%s:line %d\n", (int)__FUNCTION__, __LINE__, 0,0,0,0);
# define ASSERT( expr, func ) if( !( expr ) ) { \
        	printf( "Assertion failed! %s:line %d %s\n", \
        	(int)__FUNCTION__,__LINE__,(int)(#expr),0,0,0); \
        	func }
#else /* DEBUG */
# define printk(...)
# define DEBUG_ARGS0( FLG, a0 )
# define DEBUG_ARGS1( FLG, a0, a1 )
# define DEBUG_ARGS2( FLG, a0, a1, a2 )
# define DEBUG_ARGS3( FLG, a0, a1, a2, a3 )
# define DEBUG_FN( n )
# define ASSERT(expr, func)
#endif /* DEBUG */

#define NS921X_MII_CHECK_FOR_NEG      	(3*CFG_HZ) /* in s */
#define NS921X_MII_NEG_DELAY      	(5*CFG_HZ) /* in s */
#define TX_TIMEOUT			(5*CFG_HZ) /* in s */
#define SPREAD_SPECTRUM_PERCENTAGE	(4)	   /* 4% */

/* @TODO move it to eeprom.h */
#define FS_EEPROM_AUTONEG_MASK		0x7
#define FS_EEPROM_AUTONEG_SPEED_MASK	0x1
#define FS_EEPROM_AUTONEG_SPEED_10	0x0
#define FS_EEPROM_AUTONEG_SPEED_100	0x1
#define FS_EEPROM_AUTONEG_DUPLEX_MASK	0x2
#define FS_EEPROM_AUTONEG_DUPLEX_HALF	0x0
#define FS_EEPROM_AUTONEG_DUPLEX_FULL	0x2
#define FS_EEPROM_AUTONEG_ENABLE_MASK	0x4
#define FS_EEPROM_AUTONEG_DISABLE	0x0
#define FS_EEPROM_AUTONEG_ENABLE	0x4

/* buffer descriptors taken from [1] p.306 */
typedef struct
{
	unsigned int* punSrc;
	unsigned int unLen;	/* 11 bits */
	unsigned int* punDest;	/* unused */
	union {
		unsigned int unReg;
		struct {
			unsigned uStatus : 16;
			unsigned uRes : 12;
			unsigned uFull : 1;
			unsigned uEnable : 1;
			unsigned uInt : 1;
			unsigned uWrap : 1;
		} bits;
	} s;
} rx_buffer_desc_t;

typedef struct
{
	unsigned int* punSrc;
	unsigned int unLen;	/* 10 bits */
	unsigned int* punDest;	/* unused */
	union {
		unsigned int unReg; /* only 32bit accesses may done to NS921X
				     * eth engine */
		struct {
			unsigned uStatus : 16;
			unsigned uRes : 12;
			unsigned uFull : 1;
			unsigned uLast : 1;
			unsigned uInt : 1;
			unsigned uWrap : 1;
		} bits;
	} s;
} tx_buffer_desc_t;

static int eth_lowlevel_init( void );
static int ns921x_eth_init_mac( void );
static void ns921x_eth_reset_mac( void );

static void ns921x_link_force( void );
static void ns921x_link_auto_negotiate( void );
static void ns921x_link_update_egcr( void );
static void ns921x_link_print_changed( void );

/* the PHY stuff */

static char ns921x_mii_identify_phy( void );
static unsigned short ns921x_mii_read( unsigned short uiRegister );
static void ns921x_mii_write( unsigned short uiRegister, unsigned short uiData );
static unsigned int ns921x_mii_get_clock_divisor( unsigned int unMaxMDIOClk );
static unsigned int ns921x_mii_poll_busy( void );

static unsigned int nPhyMaxMdioClock = PHY_MDIO_ASS_CLK;
static unsigned char ucLinkMode =      FS_EEPROM_AUTONEG_ENABLE;
static unsigned int uiLastLinkStatus;
static PhyType phyDetected = PHY_NONE;

/* we use only one tx buffer descriptor */
static tx_buffer_desc_t* pTxBufferDesc =
	(tx_buffer_desc_t*) get_eth_reg_addr( NS9750_ETH_TXBD );

/* we use only one rx buffer descriptor of the 4 */
static rx_buffer_desc_t rxBufferDesc[PKTBUFSRX];
static uchar rxBuffer[PKTBUFSRX][(1522 + (0x1f)) & ~0x1f];

DECLARE_GLOBAL_DATA_PTR;

/***********************************************************************
 * @Function: ns921x_miiphy_read based on ns921x_mii_read
 * @Return: the data read from PHY register reg
 * @Descr: the data read may be invalid if timed out. If so, a message
 *         is printed but the invalid data is returned.
 *         The fixed device address is being used.
 ***********************************************************************/

static int ns921x_miiphy_read( char* devname, unsigned char addr, unsigned char reg, unsigned short *value )
{
        /* so MII functions can be used independently whether we did
         * something with ethernet */
        if( !eth_lowlevel_init() )
                return -1;
        
	/* write MII register to be read */
	*get_eth_reg_addr(NS9750_ETH_MADR) = addr<<8|reg;

	*get_eth_reg_addr( NS9750_ETH_MCMD ) = NS9750_ETH_MCMD_READ;

	if( !ns921x_mii_poll_busy() )
	    printk( KERN_WARNING NS921X_DRIVER_NAME
		    ": MII still busy in read\n" );
	/* continue to read */

	*get_eth_reg_addr( NS9750_ETH_MCMD ) = 0;

	*value = (unsigned short) (*get_eth_reg_addr( NS9750_ETH_MRDD ) );

	return 0;
}


/***********************************************************************
 * @Function: ns921x_miiphy_write based on ns921x_mii_write
 * @Return: != 0 on error
 * @Descr: writes the data to the PHY register. In case of a timeout,
 *         no special handling is performed but a message printed
 *         The fixed device address is being used.
 ***********************************************************************/

static int ns921x_miiphy_write (char* devname, unsigned char addr, unsigned char reg, unsigned short value)
{
        if( !eth_lowlevel_init() )
                return -1;

	/* write MII register to be written */
	*get_eth_reg_addr( NS9750_ETH_MADR)=addr<<8|reg;

	*get_eth_reg_addr( NS9750_ETH_MWTD ) = value;

	if( !ns921x_mii_poll_busy() )
		printk( KERN_WARNING NS921X_DRIVER_NAME
			": MII still busy in write\n");
	return 0;
}

/***********************************************************************
 * @Function: ns921x_miiphy_initialize
 * @Return: always 0
 ***********************************************************************/
int ns921x_miiphy_initialize( bd_t *bis )
{
        miiphy_register( "ns921x", ns921x_miiphy_read, ns921x_miiphy_write );
        return 0;
}


/***********************************************************************
 * @Function: ns921x_mii_read
 * @Return: the data read from PHY register uiRegister
 * @Descr: the data read may be invalid if timed out. If so, a message
 *         is printed but the invalid data is returned.
 *         The fixed device address is being used.
 ***********************************************************************/

static unsigned short ns921x_mii_read( unsigned short uiRegister )
{
        unsigned short val;
        
        ns921x_miiphy_read( NULL, NS921X_ETH_PHY_ADDRESS, uiRegister, &val );
        return val;
}

/***********************************************************************
 * @Function: ns921x_mii_write
 * @Return: nothing
 * @Descr: writes the data to the PHY register. In case of a timeout,
 *         no special handling is performed but a message printed
 *         The fixed device address is being used.
 ***********************************************************************/

static void ns921x_mii_write( unsigned short uiRegister, unsigned short uiData )
{
        ns921x_miiphy_write( NULL, NS921X_ETH_PHY_ADDRESS, uiRegister, uiData );
}

/***********************************************************************
 * @Function: eth_lowlevel_init
 * @Return: 0 on failure otherwise 1
 * @Descr: Initializes the GPIO and low level register not for individual
 *         frames
 ***********************************************************************/

static int eth_lowlevel_init( void )
{
        static int bAlreadyInitialized = 0;
	int i;

	DEBUG_FN( DEBUG_INIT );

        if( bAlreadyInitialized )
                return 1;
        
        bAlreadyInitialized = 1;

        /* enable clock and hold it out-of-reset */
        sys_rmw32( SYS_CLOCK, | SYS_CLOCK_ETH );
        /* we don't need ETH Phy Int */
	for( i = 32; i <= 49; i++ )
                gpio_cfg_set( i, GPIO_CFG_FUNC_0 );

#ifdef GPIO_ETH_PHY_RESET
        /* take PHY out of reset. Needs to be done after GPIO 42
           (TX_EN), otherwise the PHY lights yellow activity and blinks
           green link LED.
           xmit of frames will start after 35...500ms, [4] 9.5.16.
           If already done by platform.S due to S4-8, it has no effect */
        gpio_ctrl_set( GPIO_ETH_PHY_RESET, 1 );
        gpio_cfg_set( GPIO_ETH_PHY_RESET,
                      GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO );
#endif
        
	/* no need to check for hardware */

	if( !ns921x_eth_init_mac() )
		return 0;

	*get_eth_reg_addr( NS9750_ETH_MAC2 ) = NS9750_ETH_MAC2_CRCEN |
		NS9750_ETH_MAC2_PADEN |
		NS9750_ETH_MAC2_FULLD;

        return 1;
}

/***********************************************************************
 * @Function: eth_init
 * @Return: -1 on failure otherwise 0
 * @Descr: Initializes the ethernet engine and uses either FS Forth's default
 *         MAC addr or the one in environment
 ***********************************************************************/

int eth_init( bd_t* pbis )
{
	int i;

	/* enable hardware */
        if( !eth_lowlevel_init() )
                return -1;

	/* prepare DMA descriptors */
	/* NetRxPackets[ 0 ] is initialized before eth_init is called and never
	   changes. NetRxPackets is 32bit aligned */

	for (i = 0; i < PKTBUFSRX; ++i) {
		rxBufferDesc[i].punSrc = (unsigned int*) (rxBuffer[i]);
		rxBufferDesc[i].unLen = 1522;
		rxBufferDesc[i].s.bits.uWrap = !(i < PKTBUFSRX - 1);
		rxBufferDesc[i].s.bits.uInt = 1;
		rxBufferDesc[i].s.bits.uEnable = 1;
		rxBufferDesc[i].s.bits.uFull = 0;
	}

        flush_cache_all();
        
	*get_eth_reg_addr( NS9750_ETH_RXAPTR ) = (unsigned int)rxBufferDesc;
	*get_eth_reg_addr( NS9750_ETH_RXBPTR ) = 0;
	*get_eth_reg_addr( NS9750_ETH_RXCPTR ) = 0;
	*get_eth_reg_addr( NS9750_ETH_RXDPTR ) = 0;

	udelay(1); /* This seems to be only needed when compiled under cygwin?? */

	/* pTxBufferDesc is the first possible buffer descriptor */
	*get_eth_reg_addr( NS9750_ETH_TXPTR ) = 0x0;
	
	/* set first descriptor to wrap and disable to avoid unwanted
	   transmissions */
	((tx_buffer_desc_t*) get_eth_reg_addr( NS9750_ETH_TXBD ))->s.unReg=0x80000000;
        
	/* set the next buffer descriptor empty so the tx engine stops on that descriptor */
	((tx_buffer_desc_t*) get_eth_reg_addr( NS9750_ETH_TXBD1 ))->s.unReg=0x0;

	/* enable receive and transmit FIFO, use 10/100 Mbps MII */
	*get_eth_reg_addr( NS9750_ETH_EGCR1 ) =
		NS9750_ETH_EGCR1_ERX |
		NS9750_ETH_EGCR1_ETX |
		NS9750_ETH_EGCR1_ERXINIT;

	/* [1] Tab. 221 states less than 5us */
	udelay( 5 );
	while( !(*get_eth_reg_addr(NS9750_ETH_EGSR) & NS9750_ETH_EGSR_RXINIT))
		/* wait for finish */
		udelay( 1 );

        *get_eth_reg_addr(NS9750_ETH_EGSR) = NS9750_ETH_EGSR_RXINIT;

	*get_eth_reg_addr( NS9750_ETH_EGCR1 ) &= ~NS9750_ETH_EGCR1_ERXINIT;
	*get_eth_reg_addr( NS9750_ETH_EGCR2 ) = NS9750_ETH_EGCR2_STEN;


	*get_eth_reg_addr( NS9750_ETH_MAC1 ) = NS9750_ETH_MAC1_RXEN;
	*get_eth_reg_addr( NS9750_ETH_SUPP ) &= ~NS9750_ETH_SUPP_RPERMII;

	*get_eth_reg_addr( NS9750_ETH_EGCR1 ) =
		NS9750_ETH_EGCR1_ERX |
		NS9750_ETH_EGCR1_ETX |
		NS9750_ETH_EGCR1_ERXDMA |
		NS9750_ETH_EGCR1_ETXDMA |
		NS9750_ETH_EGCR1_ERXSHT;

	*get_eth_reg_addr(NS9750_ETH_EINTR) |= *get_eth_reg_addr(NS9750_ETH_EINTR);

	return 0;
}

/***********************************************************************
 * @Function: eth_send
 * @Return: -1 on timeout otherwise 1
 * @Descr: sends one frame by DMA
 ***********************************************************************/

int eth_send( volatile void* pPacket, int nLen )
{
        unsigned int uiTries = 3;
        
	DEBUG_FN( DEBUG_TX );

        /* in case a Tx Error happened, retry transmission a few times */
        do {
                ulong ulTimeout;
                
                uiTries--;
                
                /* clear old status values */
                *get_eth_reg_addr(NS9750_ETH_EINTR) &= NS9750_ETH_EINTR_TX_MA;

                /* prepare Tx Descriptors */
                
                pTxBufferDesc->punSrc = (unsigned int*) pPacket; /* pPacket is 32bit
                                                                  * aligned */
                pTxBufferDesc->unLen = nLen;
                /* only 32bit accesses allowed. wrap, full, interrupt and enabled to 1 */
                pTxBufferDesc->s.unReg = 0xf0000000;
                
                flush_cache_all();
                
                /* pTxBufferDesc is the first possible buffer descriptor */
                *get_eth_reg_addr( NS9750_ETH_TXPTR ) = 0x0;

                /* enable processor for next frame */

                *get_eth_reg_addr( NS9750_ETH_EGCR2 ) &= ~(NS9750_ETH_EGCR2_TCLER | NS9750_ETH_EGCR2_TKICK );
                *get_eth_reg_addr( NS9750_ETH_EGCR2 ) |= ( NS9750_ETH_EGCR2_TCLER | NS9750_ETH_EGCR2_TKICK );

                DEBUG_ARGS0(DEBUG_TX|DEBUG_MINOR,"Waiting for transmission to finish\n");
	
                ulTimeout = get_timer( 0 );

                while( 1 ) {
                        u32 uiIntr = *get_eth_reg_addr( NS9750_ETH_EINTR );

                        if( get_timer( ulTimeout ) >= TX_TIMEOUT ) {
                                printf( "Tx Timeout %x %x\n",
                                        uiIntr, pTxBufferDesc->s.unReg );
                                break;
                        }

                        if( uiIntr & NS9750_ETH_EINTR_TXERR ) {
                                printf( "Tx Error: %08x\n",
                                        pTxBufferDesc->s.unReg );
                                break;
                        } else if( uiIntr & NS9750_ETH_EINTR_TXDONE ) {
                                uiTries = 0;
                                break;
                        }
                }

                *get_eth_reg_addr( NS9750_ETH_EINTR ) &= NS9750_ETH_EINTR_TX_MA;
        } while( uiTries );
        
        DEBUG_ARGS0( DEBUG_TX|DEBUG_MINOR, "transmitted...\n");

        return 0;
}

/***********************************************************************
 * @Function: eth_rx
 * @Return: size of last frame in bytes or 0 if no frame available
 * @Descr: gives one frame to U-Boot which has been copied by DMA engine already
 *         to NetRxPackets[ 0 ].
 ***********************************************************************/

int eth_rx( void )
{
	unsigned int unStatus;
	int i;

	unStatus = *get_eth_reg_addr(NS9750_ETH_EINTR) & NS9750_ETH_EINTR_RX_MA;

	if( !unStatus )
		/* no packet available, return immediately */
		return 0;

	DEBUG_FN( DEBUG_RX );

	/* acknowledge status register */
	*get_eth_reg_addr(NS9750_ETH_EINTR) = unStatus;

	if (unStatus & NS9750_ETH_EINTR_RXDONEA)  {
		for (i = 0; i < PKTBUFSRX; ++i) {
                        invalidate_cache_all();

			if (rxBufferDesc[i].s.bits.uFull) {
				int len = rxBufferDesc[i].unLen - 4;
				NetReceive(rxBuffer[i], len);

				rxBufferDesc[i].unLen = 1522;
				rxBufferDesc[i].s.bits.uFull = 0;
				*get_eth_reg_addr(NS9750_ETH_RXFREE) |= 0x1;
			}
		}
	}

	return 0;
}

/***********************************************************************
 * @Function: eth_halt
 * @Return: n/a
 * @Descr: we don't do anything here to avoid unnecessary initialization
 *         again on next command
 ***********************************************************************/

void eth_halt( void )
{
	DEBUG_FN( DEBUG_INIT );

	*get_eth_reg_addr( NS9750_ETH_MAC1 ) &= ~NS9750_ETH_MAC1_RXEN;
}

/***********************************************************************
 * @Function: ns921x_eth_init_mac
 * @Return: 0 on failure otherwise 1
 * @Descr: initializes the PHY layer,
 *         performs auto negotiation or fixed modes
 ***********************************************************************/

static int ns921x_eth_init_mac( void )
{
	DEBUG_FN( DEBUG_MINOR );

	/* initialize PHY */
	*get_eth_reg_addr( NS9750_ETH_SUPP ) = NS9750_ETH_SUPP_RPERMII;

	*get_eth_reg_addr( NS9750_ETH_MAC1 ) = 0;  /* take it out of SoftReset */
	/* we don't support hot plugging of PHY, therefore we don't reset
	   phyDetected and nPhyMaxMdioClock here. The risk is if the setting is
	   incorrect the first open
	   may detect the PHY correctly but succeding will fail
	   For reseting the PHY and identifying we have to use the standard
	   MDIO CLOCK value 2.5 MHz.2.5 MHz are assured by the hardware
	   reference ns9215 and ns9210. Higher speed can be working. We
	   will not use higher speed. */

	*get_eth_reg_addr( NS9750_ETH_MCFG ) =
	    ns921x_mii_get_clock_divisor( nPhyMaxMdioClock );

	/* MII clock has been setup to default, ns9750_mii_identify_phy should
	   work for all */

	if( !ns921x_mii_identify_phy() ) {
	    printf( "Unsupported PHY, aborting\n");
	    return 0;
	}

	/* PHY has been detected, so there can be no abort reason and we can
	   finish initializing ethernet */

	uiLastLinkStatus = 0xff; /* undefined */

	if((ucLinkMode&FS_EEPROM_AUTONEG_ENABLE_MASK)==FS_EEPROM_AUTONEG_DISABLE)
		/* use parameters defined */
		ns921x_link_force();
	else
		ns921x_link_auto_negotiate();

	return 1;
}

/***********************************************************************
 * @Function: ns921x_eth_reset_mac
 * @Return: 0 on failure otherwise 1
 * @Descr: resets the MAC 
 ***********************************************************************/

static void ns921x_eth_reset_mac( void )
{
	DEBUG_FN( DEBUG_MINOR );

	/* Reset MAC */
	*get_eth_reg_addr( NS9750_ETH_EGCR1 ) |= (NS9750_ETH_EGCR1_MAC_HRST |
							NS9750_ETH_EGCR1_ERX |
							NS9750_ETH_EGCR1_ETX);
	udelay( 5 ); 		/* according to [1], p.322 */
	*get_eth_reg_addr( NS9750_ETH_EGCR1 ) &= ~NS9750_ETH_EGCR1_MAC_HRST;
}

/***********************************************************************
 * @Function: ns921x_link_force
 * @Return: void
 * @Descr: configures eth and MII to use the link mode defined in
 *         ucLinkMode
 ***********************************************************************/

static void ns921x_link_force( void )
{
	unsigned short uiControl;

	DEBUG_FN( DEBUG_LINK );

	uiControl = ns921x_mii_read( PHY_COMMON_CTRL );
	uiControl &= ~( PHY_COMMON_CTRL_SPD_MA |
			PHY_COMMON_CTRL_AUTO_NEG |
			PHY_COMMON_CTRL_DUPLEX );

	uiLastLinkStatus = 0;

	if( ( ucLinkMode & FS_EEPROM_AUTONEG_SPEED_MASK ) ==
	    FS_EEPROM_AUTONEG_SPEED_100 ) {
	    uiControl |= PHY_COMMON_CTRL_SPD_100;
#ifdef CONFIG_PHY_ICS1893
	    uiLastLinkStatus |= PHY_ICS1893_QPSTAT_100BTX;
#endif
	} else
	    uiControl |= PHY_COMMON_CTRL_SPD_10;

	if( ( ucLinkMode & FS_EEPROM_AUTONEG_DUPLEX_MASK ) ==
	    FS_EEPROM_AUTONEG_DUPLEX_FULL ) {
	    uiControl |= PHY_COMMON_CTRL_DUPLEX;
#ifdef CONFIG_PHY_ICS1893
	    uiLastLinkStatus |= PHY_ICS1893_QPSTAT_DUPLEX;
#endif
	}

	ns921x_mii_write( PHY_COMMON_CTRL, uiControl );

	ns921x_link_print_changed();
	ns921x_link_update_egcr();
}

/***********************************************************************
 * @Function: ns921x_link_auto_negotiate
 * @Return: void
 * @Descr: performs auto-negotation of link.
 ***********************************************************************/

static void ns921x_link_auto_negotiate( void )
{
	unsigned long ulStartJiffies;
	unsigned short uiStatus;
        unsigned long uiCheckStart;
        
	DEBUG_FN( DEBUG_LINK );

        uiCheckStart = get_timer( 0 );
        
        /* determination whether an auto-negotiation is in progress */
        do {
                uiStatus = ns921x_mii_read( PHY_COMMON_STAT );
                if( ( uiStatus &
                      PHY_COMMON_STAT_LNK_STAT) == PHY_COMMON_STAT_LNK_STAT) {
                        /* we have a link, so no need to trigger auto-negotiation */
                        ns921x_link_print_changed();
                        ns921x_link_update_egcr();
                        return;
                }
        } while( get_timer( uiCheckStart ) < NS921X_MII_CHECK_FOR_NEG );

        /* no auto-negotiation seemed to be in progress, and there is no link.
           run auto-negotation */
	/* define what we are capable of */
	ns921x_mii_write( PHY_COMMON_AUTO_ADV,
			  PHY_COMMON_AUTO_ADV_100BTXFD |
			  PHY_COMMON_AUTO_ADV_100BTX |
			  PHY_COMMON_AUTO_ADV_10BTFD |
			  PHY_COMMON_AUTO_ADV_10BT |
			  PHY_COMMON_AUTO_ADV_802_3 );
	/* start auto-negotiation */
	ns921x_mii_write( PHY_COMMON_CTRL,
			  PHY_COMMON_CTRL_AUTO_NEG |
			  PHY_COMMON_CTRL_RES_AUTO );

	/* wait for completion */

	ulStartJiffies = get_ticks();
	while( get_ticks() < ulStartJiffies + NS921X_MII_NEG_DELAY ) {
		uiStatus = ns921x_mii_read( PHY_COMMON_STAT );
		if( ( uiStatus &
		      (PHY_COMMON_STAT_AN_COMP | PHY_COMMON_STAT_LNK_STAT)) ==
		    (PHY_COMMON_STAT_AN_COMP | PHY_COMMON_STAT_LNK_STAT) ) {
			/* lucky we are, auto-negotiation succeeded */
			ns921x_link_print_changed();
			ns921x_link_update_egcr();

			return;
		}
	}

	printf( KERN_WARNING NS921X_DRIVER_NAME
		":auto-negotiation timed out, forcing 10Mbps/Half\n" );

	/* force mode */
	ucLinkMode = FS_EEPROM_AUTONEG_SPEED_10 | FS_EEPROM_AUTONEG_DUPLEX_HALF;
	ns921x_link_force();
}

/***********************************************************************
 * @Function: ns921x_link_update_egcr
 * @Return: void
 * @Descr: updates the EGCR and MAC2 link status after mode change or
 *         auto-negotation
 ***********************************************************************/

static void ns921x_link_update_egcr( void )
{
	unsigned int unEGCR;
	unsigned int unMAC2;
	unsigned int unIPGT;

	DEBUG_FN( DEBUG_LINK );

	unEGCR = *get_eth_reg_addr( NS9750_ETH_EGCR1 );
	unMAC2 = *get_eth_reg_addr( NS9750_ETH_MAC2 );
	unIPGT = *get_eth_reg_addr( NS9750_ETH_IPGT ) & ~NS9750_ETH_IPGT_MA;

	unMAC2 &= ~NS9750_ETH_MAC2_FULLD;

#ifdef CONFIG_PHY_ICS1893
	if( (uiLastLinkStatus & PHY_ICS1893_QPSTAT_DUPLEX)
	    == PHY_ICS1893_QPSTAT_DUPLEX ) {
		unMAC2 |= NS9750_ETH_MAC2_FULLD;
		unIPGT |= 0x15;	/* see [1] p. 339 */
	}
	else
#endif
		unIPGT |= 0x12;	/* see [1] p. 339 */

	*get_eth_reg_addr( NS9750_ETH_MAC2 ) = unMAC2;
	*get_eth_reg_addr( NS9750_ETH_EGCR1 ) = unEGCR;
	*get_eth_reg_addr( NS9750_ETH_IPGT ) = unIPGT;
}

/***********************************************************************
 * @Function: ns921x_link_print_changed
 * @Return: void
 * @Descr: checks whether the link status has changed and if so prints
 *         the new mode
 ***********************************************************************/

static void ns921x_link_print_changed( void )
{
	unsigned short uiStatus;
	unsigned short uiControl;

	DEBUG_FN( DEBUG_LINK );

	uiControl = ns921x_mii_read( PHY_COMMON_CTRL );

	if( (uiControl & PHY_COMMON_CTRL_AUTO_NEG) == PHY_COMMON_CTRL_AUTO_NEG) {
		/* PHY_COMMON_STAT_LNK_STAT is only set on autonegotiation */
		uiStatus = ns921x_mii_read( PHY_COMMON_STAT );

		if( !( uiStatus & PHY_COMMON_STAT_LNK_STAT) ) {
			printk( KERN_WARNING NS921X_DRIVER_NAME ": link down\n");
			/* @TODO Linux: carrier_off */
		} else {
			/* @TODO Linux: carrier_on */
#ifdef CONFIG_PHY_ICS1893
	unsigned short uiStatus_bak;
			if( phyDetected == PHY_ICS1893BK ) {
				uiStatus = ns921x_mii_read( PHY_COMMON_AUTO_ADV );
				uiStatus &= (PHY_ICS1893_QPSTAT_100BTX);
				uiStatus_bak = uiStatus;
				uiStatus = ns921x_mii_read( PHY_COMMON_CTRL );
				uiStatus &= (PHY_ICS1893_QPSTAT_DUPLEX );
				uiStatus |= uiStatus_bak;

				/* mask out all uninteresting parts */
			}
#endif
			/* other PHYs must store there link information in
			   uiStatus as PHY_LXT971 */
		}
	} else {
		/* mode has been forced, so uiStatus should be the same as the
		   last link status, enforce printing */
		uiStatus = uiLastLinkStatus;
		uiLastLinkStatus = 0xff;
	}

	if( uiStatus != uiLastLinkStatus ) {
		/* save current link status */
		uiLastLinkStatus = uiStatus;
	}
}

/***********************************************************************
 * the MII low level stuff
 ***********************************************************************/

/***********************************************************************
 * @Function: ns921x_mii_identify_phy
 * @Return: 1 if supported PHY has been detected otherwise 0
 * @Descr: checks for supported PHY and prints the IDs.
 ***********************************************************************/

static char ns921x_mii_identify_phy( void )
{
	unsigned short uiID1;
	unsigned short uiID2;
	char* szName;
	char cRes = 0;
        
	DEBUG_FN( DEBUG_MII );

	uiID1 = ns921x_mii_read( PHY_COMMON_ID1 );
	phyDetected = (PhyType)uiID1;

	switch( phyDetected ) {
#ifdef CONFIG_PHY_ICS1893
	    case PHY_ICS1893BK:
		szName = "ICS1893BK";
		uiID2 = ns921x_mii_read( PHY_COMMON_ID2 );
		cRes = 1;
		break;
#endif
#ifdef CONFIG_PHY_HIRSCHMANN
	    case PHY_HIRSCHMANN:
		szName = "Hirschmann";
		nPhyMaxMdioClock = 0x25000000;
		cRes = 1;
		break;
#endif
	    case PHY_NONE:
	    default:
		/* in case uiID1 == 0 && uiID2 == 0 we may have the wrong
		   address or reset sets the wrong NS921X_ETH_MCFG_CLKS */

		uiID2 = 0;
		szName = "unknown";
		phyDetected = PHY_NONE;

                printf( "Unknown PHY @ %i (0x%x, 0x%x) detected\n",
                        NS921X_ETH_PHY_ADDRESS,
                        uiID1,
                        uiID2 );

	}

	return cRes;
}

/***********************************************************************
 * @Function: ns921x_mii_get_clock_divisor
 * @Return: the clock divisor that should be used in NS9750_ETH_MCFG_CLKS
 * @Descr: if no clock divisor can be calculated for the
 *         current SYSCLK and the maximum MDIO Clock, a warning is printed
 *         and the greatest divisor is taken
 ***********************************************************************/

static unsigned int ns921x_mii_get_clock_divisor( unsigned int unMaxMDIOClk )
{
	struct
	{
		unsigned int unSysClkDivisor;
		unsigned int unClks; /* field for NS9750_ETH_MCFG_CLKS */
	} PHYClockDivisors[] = {
		{  4, NS9750_ETH_MCFG_CLKS_4 },
		{  6, NS9750_ETH_MCFG_CLKS_6 },
		{  8, NS9750_ETH_MCFG_CLKS_8 },
		{ 10, NS9750_ETH_MCFG_CLKS_10 },
		{ 20, NS9750_ETH_MCFG_CLKS_20 },
		{ 30, NS9750_ETH_MCFG_CLKS_30 },
		{ 40, NS9750_ETH_MCFG_CLKS_40 }
	};

	int nIndexSysClkDiv;
	int nArraySize = sizeof(PHYClockDivisors) / sizeof(PHYClockDivisors[0]);
	unsigned int unClks = NS9750_ETH_MCFG_CLKS_40; /* defaults to
							  greatest div */

	DEBUG_FN( DEBUG_INIT );

	for( nIndexSysClkDiv=0; nIndexSysClkDiv < nArraySize;nIndexSysClkDiv++) {
		/* find first sysclock divisor that isn't higher than 2.5 MHz
		   clock */
		if( AHB_CLK_FREQ /
		    PHYClockDivisors[ nIndexSysClkDiv ].unSysClkDivisor <=
		    unMaxMDIOClk ) {
			unClks = PHYClockDivisors[ nIndexSysClkDiv ].unClks;
			break;
		}
	}

	DEBUG_ARGS2( DEBUG_INIT,
		     "Taking MDIO Clock bit mask 0x%0x for max clock %i\n",
		     unClks,
		     unMaxMDIOClk );

	/* return greatest divisor */
	return unClks;
}

/***********************************************************************
 * @Function: ns921x_mii_poll_busy
 * @Return: 0 if timed out otherwise the remaing timeout
 * @Descr: waits until the MII has completed a command or it times out
 *         code may be interrupted by hard interrupts.
 *         It is not checked what happens on multiple actions when
 *         the first is still being busy and we timeout.
 ***********************************************************************/

static unsigned int ns921x_mii_poll_busy( void )
{
	unsigned int unTimeout = 10000;

	DEBUG_FN( DEBUG_MII_LOW );

	while( (( *get_eth_reg_addr( NS9750_ETH_MIND ) & NS9750_ETH_MIND_BUSY)
		== NS9750_ETH_MIND_BUSY ) &&
	       unTimeout )
		unTimeout--;

	return unTimeout;
}

/***********************************************************************
 * @Function: eth_use_mac_from_env
 * @Return: 0 if ok, < 0 on failure
 * @Descr: Stores MAC address in chip. Might be called for a not fully
 * initialized network stack when booting Linux from Flash.
 ***********************************************************************/

int eth_use_mac_from_env( bd_t* pbis )
{
	unsigned char aucMACAddr[ 6 ] = { 0x00,0x04,0xf3,0x00,0x06,0x35 };

	char* pcTmp = getenv( "ethaddr" );
	char* pcEnd;
        int   i;

        /* clock is not on when used directly from board_late_init() */
        sys_rmw32( SYS_CLOCK, | SYS_CLOCK_ETH );

        ns921x_eth_reset_mac();

	if( pcTmp != NULL ) {
                /* copy MAC address */
                for( i = 0; i < 6; i++ ) {
                        aucMACAddr[ i ] = pcTmp ? simple_strtoul( pcTmp, &pcEnd, 16 ) : 0;
                        /* next char or terminating zero */
                        pcTmp = (*pcTmp) ? ( pcEnd + 1 ) : pcEnd;
                }
        } else
                eprintf( "Couldn't read MAC address from environment, using default\n" );


        /* now set it, so linux can use it */
	*get_eth_reg_addr( NS9750_ETH_SA1 ) = aucMACAddr[ 5 ]<<8 | aucMACAddr[ 4 ];
	*get_eth_reg_addr( NS9750_ETH_SA2 ) = aucMACAddr[ 3 ]<<8 | aucMACAddr[ 2 ];
	*get_eth_reg_addr( NS9750_ETH_SA3 ) = aucMACAddr[ 1 ]<<8 | aucMACAddr[ 0 ];

        sys_rmw32( SYS_CLOCK, & ( ~SYS_CLOCK_ETH ) );

	return 0;
}

/***********************************************************************
* @Function: set_mac_from_env
* @Descr: Stores MAC address in chip. Call careful,because no check is
* done.
***********************************************************************/

void set_mac_from_env( void )
{
	unsigned char aucMACAddr[ 6 ];

	char* pcTmp = getenv( "ethaddr" );
	char* pcEnd;
        int   i;

	if( pcTmp != NULL ) {
                /* copy MAC address */
                for( i = 0; i < 6; i++ ) {
                        aucMACAddr[ i ] = pcTmp ? simple_strtoul( pcTmp, &pcEnd, 16 ) : 0;
                        /* next char or terminating zero */
                        pcTmp = (*pcTmp) ? ( pcEnd + 1 ) : pcEnd;
                }
        } else
                eprintf( "Couldn't read MAC address from environment, using default\n" );


        /* now set it, so linux can use it */
	*get_eth_reg_addr( NS9750_ETH_SA1 ) = aucMACAddr[ 5 ]<<8 | aucMACAddr[ 4 ];
	*get_eth_reg_addr( NS9750_ETH_SA2 ) = aucMACAddr[ 3 ]<<8 | aucMACAddr[ 2 ];
	*get_eth_reg_addr( NS9750_ETH_SA3 ) = aucMACAddr[ 1 ]<<8 | aucMACAddr[ 0 ];
}

#endif /* CONFIG_DRIVER_NS921X_ETHERNET */
