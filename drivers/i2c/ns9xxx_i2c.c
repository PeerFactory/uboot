/***********************************************************************
 *
 * Copyright (C) 2004 by FS Forth-Systeme GmbH.
 * All rights reserved.
 *
 * @Author: Markus Pietrek
 * @Descr: I2C Driver for the NS9750. No multi-master support
 * @TODO: iprobe fails, i2c is unusable then
 * @References: [1] linux/drivers/i2c/busses/i2c_ns.c
 *              [2] drivers/s3c24x0_i2c.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *  
 ***********************************************************************/

#include <common.h>

#ifdef CONFIG_DRIVER_NS9750_I2C

#include <asm/errno.h>		/* -ETIMEDOUT */
#include <asm/byteorder.h>	/* cpu_to_le32 */

#if defined(CONFIG_CC9P9215) || defined(CONFIG_CCW9P9215) || \
    defined(CONFIG_CME9210) || defined(CONFIG_CC9P9210)
# include <asm-arm/arch-ns9xxx/ns921x_gpio.h>
# include <asm-arm/arch-ns9xxx/io.h>
#else
# include <ns9750_bbus.h>	/* NS9750_BBUS_MASTER_RESET */
# include <ns9750_sys.h>
#endif

#include <ns9750_i2c.h>		/* get_i2c_reg_addr */

#define I2C_TIMEOUT	1000 /* us */

#define DRIVER_NAME	"i2c_ns"

#define PRINTD(x)

#ifndef writel
/* linux porting helping functions */
# define writel(v,a)	do { PRINTD((" -> writel(0x%08x, 0x%08x)\n", v, a)); \
		*((a)) = (v); } while(0);

# define readl(a)	({ unsigned int __v; __v = *((a)); \
		PRINTD(("readl(0x%08x) -> 0x%08x\n", a, __v)); \
		__v; })
#endif

#define rmwl(a,op)	do { PRINTD((" -> rmwl(0x%08x, " #op ")\n", a)); \
		*(a) = ( *(a) op ); } while(0);

#define addr_i2c(a)	(get_i2c_reg_addr(a))
#define addr_bbus_utility(a)	(get_bbus_reg_addr(a))

#define I2C_M_NOSTART	1
#define I2C_M_RD	2

typedef enum
{
	I2C_NS_INT_AWAITING,
	I2C_NS_INT_OK,
	I2C_NS_INT_RETRY,
	I2C_NS_INT_ERROR,
	I2C_NS_INT_ABORT
} i2c_ns_int_status_t;

/* derived from linux/include/linux/i2c.h */
struct i2c_msg {
	__u16 addr;	/* slave address			*/
 	__u16 flags;		
 	__u16 len;		/* msg length				*/
 	__u8 *buf;		/* pointer to msg data			*/
};

static uchar* i2c_ns_buf = NULL;	/* where to store the incoming character */
static i2c_ns_int_status_t i2c_ns_int_status;

static int i2c_ns_master_xfer( struct i2c_msg msgs[], int num );
static int i2c_ns_read( int count );
static int i2c_ns_write( const uchar* buf, int count );
static int i2c_ns_send_cmd( unsigned int cmd );
static int i2c_ns_wait_cmd( void );

static int l_iSelectedSpeed = 0;

static void i2c_lowlevel_init( void )
{
        static char bAlreadyInitialized = 0;

        if( bAlreadyInitialized )
                return;

        bAlreadyInitialized = 1;
        
#ifdef CONFIG_NS921X
        /* enable clock and hold it out-of-reset */
        sys_rmw32( SYS_CLOCK, | SYS_CLOCK_I2C );
#endif
#ifdef REMOVE
#if defined (CONFIG_CC9P9360) || defined (CONFIG_CC9C) || (CONFIG_CCW9C)
	rmwl( addr_bbus_utility( NS9750_BBUS_MASTER_RESET ), & 0xFFFFFF7F );
        set_gpio_cfg_reg_val(I2C_SCL_GPIO, I2C_SCL_GPIO_FUNC);
        set_gpio_cfg_reg_val(I2C_SDA_GPIO, I2C_SDA_GPIO_FUNC);
	udelay(10);
#else
        gpio_cfg_set(I2C_SCL_GPIO, I2C_SCL_GPIO_FUNC);
        gpio_cfg_set(I2C_SDA_GPIO, I2C_SDA_GPIO_FUNC);
#endif /* (CONFIG_CC9P9360) || (CONFIG_CC9C) || (CONFIG_CCW9C) */
#endif /* REMOVE */
        gpio_cfg_set(I2C_SCL_GPIO, I2C_SCL_GPIO_FUNC);
        gpio_cfg_set(I2C_SDA_GPIO, I2C_SDA_GPIO_FUNC);
}


/**
 * i2c_update_speed - writes l_iSelectedSpeed to I2C
 *
 * If the speed is invalid, don't write it
 */
static void i2c_update_speed( void )
{
        char bIgnore = 0;
        
        u32 uiClkRef = CPU_CLK_FREQ / ( 4 * l_iSelectedSpeed );

        if( uiClkRef > NS_I2C_CFG_CLREF_MA )
                bIgnore = 1;

        uiClkRef &= NS_I2C_CFG_CLREF_MA;
        
        if( uiClkRef && !bIgnore ) {
                u32 uiCfg = uiClkRef | ((0xf << NS_I2C_CFG_SFW_SH) & NS_I2C_CFG_SFW_MA );
                writel( uiCfg, addr_i2c( NS_I2C_CFG ) );
        } else
                eprintf( "Wrong I2C Divisor %i, not changing register\n",
                         uiClkRef );
}

/**
 * i2c_init - initializes I2C
 * @speed: != 0 means new value, otherwise stick with old or default
 */
void i2c_init( int speed /* ignored */, int slaveadd /*ignored*/ )
{
	PRINTD(("%s\n", __func__));
        if( !speed && l_iSelectedSpeed )
		/* no speed change and already configured */
                return;

        i2c_lowlevel_init();

        l_iSelectedSpeed = ( speed ? speed : CFG_I2C_SPEED );

        i2c_update_speed();
}

/**
 * i2c_speed - determine real I2C speed
 */
int i2c_speed( void )
{
        u32 uiClkRef;
        
        i2c_init( 0, 0 );  /* in case it has not already been configured */

        uiClkRef = readl( addr_i2c( NS_I2C_CFG ) ) & NS_I2C_CFG_CLREF_MA;

        /* there seems to be a rounding issue on cc9p9215. It reports 100
           kHz, the scope says 96 kHz */
        return CPU_CLK_FREQ / ( 4 * uiClkRef );
}

int i2c_write( uchar chip, uint addr, int alen, uchar * buffer, int count )
{
	uchar xaddr[4];
	struct i2c_msg msg[ 2 ] = {
		{
			.addr = chip,
			.len = alen,
			.buf = &xaddr[4 - alen]
		}, {
			.addr = chip,
			.len = count,
			.buf = buffer
		}
        };

	PRINTD(("%s\n", __func__));

	if( alen > 4 ) {
		printf( "I2C read: addr len %d not supported\n", alen );
		return 1;
	}

	if( alen > 0 ) {
		xaddr[ 0 ] = (addr >> 24) & 0xFF;
		xaddr[ 1 ] = (addr >> 16) & 0xFF;
		xaddr[ 2 ] = (addr >> 8) & 0xFF;
		xaddr[ 3 ] = addr & 0xFF;

		msg[1].flags |= I2C_M_NOSTART;
	}

#ifdef CFG_I2C_EEPROM_ADDR_OVERFLOW
	/*
	 * EEPROM chips that implement "address overflow" are ones
	 * like Catalyst 24WC04/08/16 which has 9/10/11 bits of
	 * address and the extra bits end up in the "chip address"
	 * bit slots. This makes a 24WC08 (1Kbyte) chip look like
	 * four 256 byte chips.
	 *
	 * Note that we consider the length of the address field to
	 * still be one byte because the extra address bits are
	 * hidden in the chip address.
	 */
	if( alen > 0 )
		chip |= ((addr >> (alen * 8)) & CFG_I2C_EEPROM_ADDR_OVERFLOW);

	{
		int i;
		for( i = 0; i < 2; i++ )
			msgs[ i++ ].addr = chip;
	}
#endif

	/* if alen == 0, don't send the first message */
	if (i2c_ns_master_xfer(msg + !alen, 2 - !alen) != (2 - !alen))
		return -EIO;

	return 0;
}

int i2c_read( uchar chip, uint addr, int alen, uchar * buffer, int count )
{
	uchar xaddr[ 4 ];
	struct i2c_msg msg[ 2 ] = {
                {addr:chip,flags:0,       len: alen, buf:&xaddr[ 4-alen ] },
                {addr:chip,flags:I2C_M_RD,len:count, buf:buffer}
        };

	PRINTD(("%s\n", __func__));

	if( alen > 4 ) {
		printf( "I2C read: addr len %d not supported\n", alen );
		return 1;
	}

	if( alen > 0 ) {
		xaddr[ 0 ] = (addr >> 24) & 0xFF;
		xaddr[ 1 ] = (addr >> 16) & 0xFF;
		xaddr[ 2 ] = (addr >> 8) & 0xFF;
		xaddr[ 3 ] = addr & 0xFF;
	}

#ifdef CFG_I2C_EEPROM_ADDR_OVERFLOW
	/*
	 * EEPROM chips that implement "address overflow" are ones
	 * like Catalyst 24WC04/08/16 which has 9/10/11 bits of
	 * address and the extra bits end up in the "chip address"
	 * bit slots. This makes a 24WC08 (1Kbyte) chip look like
	 * four 256 byte chips.
	 *
	 * Note that we consider the length of the address field to
	 * still be one byte because the extra address bits are
	 * hidden in the chip address.
	 */
	if( alen > 0 )
		chip |= ((addr >> (alen * 8)) & CFG_I2C_EEPROM_ADDR_OVERFLOW);

	{
		int i;
		for( i = 0; i < 2; i++ )
			msgs[ i++ ].addr = chip;
	}
#endif

	/* if alen == 0, don't send the first message */
	if (i2c_ns_master_xfer(msg + !alen, 2 - !alen) != 2 - !alen)
		return -EIO;

	return 0;
}

int i2c_probe( uchar chip )
{
	uchar buf[ 1 ];
	struct i2c_msg msg[1] = {
                {addr:chip, flags:0, len: 0, buf:buf },
        };
	int len = sizeof( msg ) / sizeof( struct i2c_msg );

	PRINTD(("%s\n", __func__));

	buf[ 0 ] = 0;

	/*
	 * What is needed is to send the chip address and verify that the
	 * address was <ACK>ed (i.e. there was a chip at that address which
	 * drove the data line low).
	 */
        if( i2c_ns_master_xfer( msg, len ) != len )
                return -EIO;

	return 0;
}

uchar i2c_reg_read( uchar chip, uchar reg )
{
        uchar buf;

	PRINTD(("%s\n", __func__));

        i2c_read( chip, reg, 1, &buf, 1 );

        return (buf);
}

void i2c_reg_write( uchar chip, uchar reg, uchar val )
{
	PRINTD(("%s\n", __func__));

        i2c_write( chip, reg, 1, &val, 1 );
}

/***********************************************************************
 * @Function: i2c_ns_master_xfer
 * @Return: 0 on failure otherwise number messages sends (== num)
 * @Descr: performs action described in i2c_msg
 *         accepts I2C_M_NOSTART for continous transfer, I2C_M_RD and
 *         !I2C_M_RD (write). No buffer validity check is performed.
 ***********************************************************************/

static int i2c_ns_master_xfer( struct i2c_msg msgs[], int num )
{
	int i = 0;
	int ret = 0;
	int len;
	uchar* buf;
	int retry = 1;		/* allow only some retries */

	PRINTD(("%s\n", __func__));

        i2c_init(0, 0);
        
	i2c_ns_int_status = I2C_NS_INT_OK;
	while( i < num ) {
		if( i2c_ns_int_status == I2C_NS_INT_RETRY ) {
			/* resend complete message, stop previous access first */
			/* i is already set to 0 due to while condition */
			ret = i2c_ns_send_cmd( NS_I2C_DATA_TX_CMD_M_STOP );
			if( !(retry--) || ret ) {
				/* @TODO */

				break;
			}
		}
		
		len = msgs[ i ].len;
		buf = msgs[ i ].buf;

		i2c_ns_buf = buf;

		if( !(msgs[ i ].flags & I2C_M_NOSTART ) ) {
			unsigned int cmd;

			/* set device address */
			writel( (msgs[i].addr << NS_I2C_MASTER_MDA_SH) &
				NS_I2C_MASTER_MDA_MA, addr_i2c( NS_I2C_MASTER ));

			if( msgs[ i ].flags & I2C_M_RD )
				cmd = NS_I2C_DATA_TX_CMD_M_READ;
			else {
				/* we need to send first byte with the command */
				cmd = NS_I2C_DATA_TX_CMD_M_WRITE |
					NS_I2C_DATA_TX_VAL |
					*buf;
				len--;
				buf++;
			}

			/* send start, device address and read/write command */
			ret = i2c_ns_send_cmd( cmd );
			if( ret ) {
				if( i2c_ns_int_status == I2C_NS_INT_RETRY ) {
					/* try the message again */
					i = 0;
					continue;
				} else if( i2c_ns_int_status == I2C_NS_INT_ABORT )
                                        break;
				
				printf( DRIVER_NAME 
					":Setting address failed for 0x%x "
					"with errno %i\n",
					msgs[ i ].addr, ret  );
				break;
			}
		}
		
		if( len > 0 ) {
			/* either read mode or send more than one byte */
			if( msgs[ i ].flags & I2C_M_RD )
				ret = i2c_ns_read( len );
			else
				ret = i2c_ns_write( buf, len );
			
			if( ret ) {
				if( i2c_ns_int_status == I2C_NS_INT_RETRY ) {
					/* try the message again */
					i = 0;
					continue;
				}

				printf( DRIVER_NAME,
					":Operation 0x%x failed for 0x%x "
					"with errno %i\n",
					msgs[ i ].flags,msgs[ i ].addr,ret);
				break;
			}
		}

		i++;
	}

	/* message block transmitted, send stop */
	if( i2c_ns_send_cmd( NS_I2C_DATA_TX_CMD_M_STOP ) ) {
		/* something locked up, maybe devices thinks lost clock. */
		if( i2c_ns_send_cmd( NS_I2C_DATA_TX_CMD_M_NOP ) ||
		    i2c_ns_send_cmd( NS_I2C_DATA_TX_CMD_M_STOP ) ) {
#ifndef CONFIG_NS921X			
			/* maybe netsilicon's I2C state machine got stuck,
			   reset it. */
			rmwl( addr_bbus_utility( NS9750_BBUS_MASTER_RESET ),
			      | NS9750_BBUS_MASTER_RESET_I2C );
			rmwl( addr_bbus_utility( NS9750_BBUS_MASTER_RESET ),
			      & ~NS9750_BBUS_MASTER_RESET_I2C );
#endif

                        i2c_update_speed();
		}
	}

	return ret ? 0 : i;
}

/***********************************************************************
 * @Function: i2c_ns_read
 * @Return: <0 in case of failure otherwise 0
 * @Descr: Must be called after an M_READ has been executed and the 
 *         interrupt has happened. Byte has already been stored by i2c_ns_int in
 *         i2c_ns_buf but not incremented. Don't do read command in i2c_ns_int
 *         so that we still use I2C_TIMEOUT
 ***********************************************************************/

static int i2c_ns_read( int count )
{
	int ret = 0;

	PRINTD(("%s\n", __func__));

	/* first character has already been stored in buffer by interrupt
	 * handler */
	while( count-- > 1 ) {
		i2c_ns_buf++;

		ret = i2c_ns_send_cmd( NS_I2C_DATA_TX_CMD_M_NOP );
		if( ret )
			return ret;
	}

	return 0;
}

/***********************************************************************
 * @Function: i2c_ns_write
 * @Return: <0 in case of failure otherwise 0
 * @Descr: Must be called after an M_WRITE has been executed and the 
 *         first character has been sent already with M_WRITE.
 ***********************************************************************/

static int i2c_ns_write( const uchar* buf, int count )
{
	int ret = 0;

	PRINTD(("%s\n", __func__));

	while( count-- ) {
		ret = i2c_ns_send_cmd( (*buf & NS_I2C_DATA_TX_DATA_MA ) |
				       NS_I2C_DATA_TX_CMD_M_NOP |
				       NS_I2C_DATA_TX_VAL );
		if( ret )
			return ret;

		buf++;
	}
	
	return ret;
}

/***********************************************************************
 * @Function: i2c_ns_send_cmd
 * @Return: -ETIMEDOUT if no interrupt happened in I2C_TIMEOUT or -EIO on
 *          an I2C failure
 * @Descr: sends command and waits signal safe for I2C interrupt acknowleding
 *         command or timeout.
 ***********************************************************************/

static int i2c_ns_send_cmd( unsigned int cmd )
{
	int ret = 0;

	PRINTD(("%s\n", __func__));

	i2c_ns_int_status = I2C_NS_INT_AWAITING;
	do {
		writel( cmd, addr_i2c( NS_I2C_DATA_TX ) );
		ret = i2c_ns_wait_cmd();
	} while( !ret && (i2c_ns_int_status == I2C_NS_INT_AWAITING) );
	
	if( i2c_ns_int_status != I2C_NS_INT_OK )
		ret = -EIO;
	
	return ret;
}

/***********************************************************************
 * @Function: i2c_ns_wait_cmd
 * @Return: 0 or -ETIMEDOUT
 * @Descr: waits for status flag of transmission
 ***********************************************************************/

static int i2c_ns_wait_cmd( void )
{
	int ret = 0;
	unsigned int status;
	int timeout = I2C_TIMEOUT;

	PRINTD(("%s\n", __func__));

	while( (i2c_ns_int_status == I2C_NS_INT_AWAITING) && --timeout ) {
		udelay( 1 );
		
		/* read and acknowledge (by read) */
		status = readl( addr_i2c( NS_I2C_DATA_RX ) );
		switch( status & NS_I2C_DATA_RX_IRQCD_MA ) {
		    case NS_I2C_DATA_RX_IRQCD_NO_IRQ:
                        /* no status available. continue to wait */
                        break;
                        
		    case NS_I2C_DATA_RX_IRQCD_M_RX_DATA:
			*i2c_ns_buf = status & NS_I2C_DATA_RX_DATA_MA;
			i2c_ns_int_status = I2C_NS_INT_OK;
                        break;
                        
		    case NS_I2C_DATA_RX_IRQCD_M_CMD_ACK:
			i2c_ns_int_status = I2C_NS_INT_OK;
			break;
			
		    case NS_I2C_DATA_RX_IRQCD_M_TX_DATA:
			i2c_ns_int_status = I2C_NS_INT_OK;
			break;
			
		    case NS_I2C_DATA_RX_IRQCD_M_NO_ACK:
                        /* send stop */
                        writel( NS_I2C_DATA_TX_CMD_M_STOP, addr_i2c( NS_I2C_DATA_TX ) );
                        /* no ACK is not necessary an error, e.g. in probe */
			i2c_ns_int_status = I2C_NS_INT_ABORT;
                        break;
                        
                    case NS_I2C_DATA_RX_IRQCD_M_ARBIT:
			/* maybe device didn't listen closely,try again*/
			i2c_ns_int_status = I2C_NS_INT_RETRY;
			break;
			
		    default:
			printf(DRIVER_NAME ": wait failed with 0x%x\n", status);
			i2c_ns_int_status = I2C_NS_INT_ERROR;
		}
	}
	if( !timeout ) {
		printf( DRIVER_NAME ": timed out\n" );
		
		/* not interrupted */
		ret = -ETIMEDOUT;
	}

	return ret;
}

#endif /* CONFIG_DRIVER_NS9750_I2C */
