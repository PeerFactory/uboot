/*
 * include/asm/arch-ns9xxx/ns921x_sys.h
 *
 * Copyright (C) 2007 by Digi International Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
/***********************************************************************
 *
 * !Revision: $Revision$
 * !References: [1] NS9215 Hardware Reference Manual, Preliminary January 2007
 * !Author:   Markus Pietrek
 *
 ***********************************************************************/

#ifndef __ASM_ARCH_NS9XXX_SYS_H
#define __ASM_ARCH_NS9XXX_SYS_H
/* we use NS9XXX instead of NS921X_SYS.h to avoid accidently including both of
 * them in ns9xxx*.c. Some values are named identical, but values are different */

#define SYS_BASE_PA		0xA0900000

#define SYS_AHB_GEN		0x0000
#define SYS_BRC_BASE		0x0004
#define SYS_AHB_ERROR1		0x0018
#define SYS_AHB_ERROR2		0x001C
#define SYS_AHB_MON		0x0020
#define SYS_TIMER_MASTER_CTRL	0x0024
#define SYS_TIMER_RELOAD_BASE	0x0028
#define SYS_TIMER_READ_BASE	0x0050
#define SYS_TIMER_HIGH_BASE	0x0078
#define SYS_TIMER_STEP_BASE	0x0098
#define SYS_TIMER_RELOAD_STEP_BASE 0x00A8
#define SYS_INT_VEC_ADR_BASE	0x00C4
#define SYS_INT_CFG_BASE	0x0144
#define SYS_ISADDR		0x0164
#define SYS_INT_STAT_ACTIVE	0x0168
#define SYS_INT_STAT_RAW	0x016C
#define SYS_SW_WDOG_CFG		0x0174
#define SYS_SW_WDOG_TIMER	0x0178
#define SYS_CLOCK		0x017C
#define SYS_RESET		0x0180
#define SYS_MISC		0x0184
#define SYS_PLL			0x0188
#define SYS_ACT_INT_STAT	0x018C
#define SYS_TIMER_CTRL_BASE	0x0190
#define SYS_CS_DYN_BASE_BASE	0x01D0
#define SYS_CS_DYN_MASK_BASE	0x01D4
#define SYS_CS_STATIC_BASE_BASE	0x01F0
#define SYS_CS_STATIC_MASK_BASE	0x01F4
#define SYS_GEN_ID		0x0210
#define SYS_EXT_INT_CTRL_BASE	0x0214
#define SYS_RTC			0x0224
#define SYS_POWER		0x0228
#define SYS_AHB_BUS_ACTIVITY	0x022C

/* the vectored register addresses */

#define SYS_TIMER_RELOAD( c )	( SYS_TIMER_RELOAD_BASE   + ( c ) * 4 )
#define SYS_TIMER_READ( c )	( SYS_TIMER_READ_BASE     + ( c ) * 4 )
#define SYS_INT_VEC_ADR( c )	( SYS_INT_VEC_ADR_BASE    + ( c ) * 4 )
#define SYS_TIMER_CTRL( c )	( SYS_TIMER_CTRL_BASE     + ( c ) * 4 )
/* CS_DYN index 0..3 */
#define SYS_CS_DYN_BASE( c )	( SYS_CS_DYN_BASE_BASE    + ( c ) * 8 )
#define SYS_CS_DYN_MASK( c )	( SYS_CS_DYN_MASK_BASE    + ( c ) * 8 )
/* CS_STATIC start with 0 */
#define SYS_CS_STATIC_BASE( c ) ( SYS_CS_STATIC_BASE_BASE + ( c ) * 8 )
#define SYS_CS_STATIC_MASK( c ) ( SYS_CS_STATIC_MASK_BASE + ( c ) * 8 )
#define SYS_EXT_INT_CTRL( c )   ( SYS_EXT_INT_CTRL_BASE	+ ( c ) * 4 )

/* register bit fields */

#define SYS_AHB_GEN_EXMAM	0x00000001 /* use main arbiter */

/* need to be n*8bit to BRC channel */
#define SYS_BRC_CEB		0x00000080
#define SYS_BRC_BRF_MA		0x00000030
#define SYS_BRC_BRF_100		0x00000000
#define SYS_BRC_BRF_75		0x00000010
#define SYS_BRC_BRF_50		0x00000020
#define SYS_BRC_BRF_25		0x00000030

#define SYS_AHB_ERROR2_IE	0x00080000
#define SYS_AHB_ERROR2_DE	0x00040000
#define SYS_AHB_ERROR2_ER	0x00020000
#define SYS_AHB_ERROR2_HWR	0x00004000
#define SYS_AHB_ERROR2_HMSTR(reg) ( ( ( reg ) >> 10 ) & 0xf )
#define SYS_AHB_ERROR2_HPRE_MA 	0x000003C0
#define SYS_AHB_ERROR2_HSZ(reg) ( ( ( reg ) >> 3 ) & 0x7 )
#define SYS_AHB_ERROR2_HBRST_MA 0x00000007

#define SYS_AHB_MON_EIC	 	0x00800000
#define SYS_AHB_MON_SERDC	0x00000010

#define SYS_TIMER_MASTER_CTRL_EN( timer ) 	( 1 << ( timer ) )
#define SYS_TIMER_MASTER_CTRL_HSTEP_EN( timer ) ( 1 << ( 3 * ( ( timer ) - 6 ) ) + 10 )
#define SYS_TIMER_MASTER_CTRL_LSTEP_EN( timer ) ( 1 << ( 3 * ( ( timer ) - 6 ) ) + 11 )
#define SYS_TIMER_MASTER_CTRL_RSTEP_EN( timer ) ( 1 << ( 3 * ( ( timer ) - 6 ) ) + 12 )

/* need to be n*8bit to Int Level */

#define SYS_INT_CFG_IE		0x00000080
#define SYS_INT_CFG_INV		0x00000040
#define SYS_INT_CFG_IRQ		0x00000000
#define SYS_INT_CFG_FIQ		0x00000020
#define SYS_INT_CFG_ISD(irq)	( ( irq ) & 0x1f )

/* interrupt source ids */
#define SYS_ISD_WDOG		0x00
#define SYS_ISD_AHB		0x01
#define SYS_ISD_EXT_DMA		0x02
#define SYS_ISD_WAKEUP		0x03
#define SYS_ISD_ETH_RX		0x04
#define SYS_ISD_ETH_TX		0x05
#define SYS_ISD_ETH_PHY		0x06
#define SYS_ISD_UART( port )	( 0x07 + ( ( port ) & 0x3 ) )
#define SYS_ISD_SPI		0x0B
#define SYS_ISD_ADC		0x0E
#define SYS_ISD_EARLY_POWER	0x0F
#define SYS_ISD_I2C		0x10
#define SYS_ISD_RTC		0x11
#define SYS_ISD_TIMER( timer )	( 0x12 + ( timer ) )
#define SYS_ISD_EXT( int )	( 0x1C + ( ( int ) & 0x3 ) )

#define SYS_SW_WDOG_CFG_DEBUG 	0x00000100
#define SYS_SW_WDOG_CFG_SWWE 	0x00000080
#define SYS_SW_WDOG_CFG_SWWI 	0x00000020
#define SYS_SW_WDOG_CFG_SWWIC	0x00000010
#define SYS_SW_WDOG_CFG_SWTCS_MA 0x00000007
#define SYS_SW_WDOG_CFG_SWTCS_2	 0x00000000
#define SYS_SW_WDOG_CFG_SWTCS_4	 0x00000001
#define SYS_SW_WDOG_CFG_SWTCS_8	 0x00000002
#define SYS_SW_WDOG_CFG_SWTCS_16 0x00000003
#define SYS_SW_WDOG_CFG_SWTCS_32 0x00000004
#define SYS_SW_WDOG_CFG_SWTCS_64 0x00000005

#define SYS_CLOCK_CSC( reg )	( ( reg >> 29 ) & 0x7 )
#define SYS_CLOCK_CSC_1		0x00000000
#define SYS_CLOCK_CSC_2		0x20000000
#define SYS_CLOCK_CSC_4		0x40000000
#define SYS_CLOCK_CSC_8		0x60000000
#define SYS_CLOCK_CSC_16	0x80000000
#define SYS_CLOCK_MAX_CSC_1	0x00000000
#define SYS_CLOCK_MAX_CSC_2	0x04000000
#define SYS_CLOCK_MAX_CSC_4	0x08000000
#define SYS_CLOCK_MAX_CSC_8	0x0C000000
#define SYS_CLOCK_MAX_CSC_16	0x10000000
#define SYS_CLOCK_CCSEL		0x02000000
#define SYS_CLOCK_MA		0x00ffffff
#define SYS_CLOCK_MCCOUT( cs )	( 1 << ( ( ( cs & 3 ) ) + 16 ) )
#define SYS_CLOCK_EXT_DMA	0x00004000
#define SYS_CLOCK_IO		0x00002000
#define SYS_CLOCK_RTC		0x00001000
#define SYS_CLOCK_I2C		0x00000800
#define SYS_CLOCK_AES		0x00000200
#define SYS_CLOCK_ADC		0x00000100
#define SYS_CLOCK_SPI		0x00000020
#define SYS_CLOCK_UART( port )	( 1 << ( ( ( port ) & 3 ) + 1 ) )
#define SYS_CLOCK_ETH		0x00000001

#define SYS_RESET_STAT_MA	0xE0000000
#define SYS_RESET_STAT_RESN	0x20000000
#define SYS_RESET_STAT_SRESN	0x40000000
#define SYS_RESET_STAT_PLL	0x60000000
#define SYS_RESET_STAT_SW_WDOG	0x80000000
#define SYS_RESET_STAT_AHB	0x90000000
#define SYS_RESET_EXT_DMA	0x00004000
#define SYS_RESET_IO		0x00002000
#define SYS_RESET_I2C		0x00000800
#define SYS_RESET_AES		0x00000200
#define SYS_RESET_ADC		0x00000100
#define SYS_RESET_SPI		0x00000020
#define SYS_RESET_UART( port )	( 1 << ( ( ( port ) & 3 ) + 1 ) )
#define SYS_RESET_ETH		0x00000001

#define SYS_MISC_REV( reg )	( ( ( reg ) >> 24 ) & 0xFF )
#define SYS_MISC_ID_MA		0x00FF0000
#define SYS_MISC_ID_NS9215_SP	0x00020000
#define SYS_MISC_ID_NS9215_LP	0x00030000
#define SYS_MISC_RTC		0x00000040
#define SYS_MISC_BOOT_MODE	0x00000020
#define SYS_MISC_BOOT_WIDTH_MA	0x00000018
#define SYS_MISC_ENDM		0x00000004
#define SYS_MISC_MIS		0x00000002
#define SYS_MISC_INT		0x00000001

#define SYS_PLL_NF( reg )	( ( ( reg ) >> 8 ) & 0x1ff )
#define SYS_PLL_BP		0x00000080
#define SYS_PLL_OD( reg )	( ( ( reg ) >> 5 ) & 0x3 )
#define SYS_PLL_NR( reg )	( ( reg ) & 0x1f )

#define SYS_ACT_INT_STAT_MA	0x0000FFFF

#define SYS_CS_BASE_VAL( base )	( ( base ) << 11 )
#define SYS_CS_MASK_VAL( mask )	( ( mask ) << 11 )
#define SYS_CS_MASK_EN		0x00000001

/* only for timer 0-4 */
#define SYS_TIMER_CTRL_TE	0x00008000
#define SYS_TIMER_CTRL_CAP_CMP_MA 0x00007000
#define SYS_TIMER_CTRL_DBG_HALT 0x00000800
#define SYS_TIMER_CTRL_INTC	0x00000400
#define SYS_TIMER_CTRL_TCS_MA	0x000003C0
#define SYS_TIMER_CTRL_TCS_M2	0x00000000
#define SYS_TIMER_CTRL_TCS_1	0x00000040
#define SYS_TIMER_CTRL_TCS_2	0x00000080
#define SYS_TIMER_CTRL_TCS_4	0x000000C0
#define SYS_TIMER_CTRL_TCS_8	0x00000100
#define SYS_TIMER_CTRL_TCS_16	0x00000140
#define SYS_TIMER_CTRL_TCS_32	0x00000180
#define SYS_TIMER_CTRL_TCS_64	0x000001C0
#define SYS_TIMER_CTRL_TCS_128	0x00000200
#define SYS_TIMER_CTRL_TCS_EXT	0x000003C0
#define SYS_TIMER_CTRL_TM_MA	0x00000030
#define SYS_TIMER_CTRL_TM_INT	0x00000000
#define SYS_TIMER_CTRL_TM_EXT_L 0x00000010
#define SYS_TIMER_CTRL_TM_EXT_H	0x00000020
#define SYS_TIMER_CTRL_TM_CONC	0x00000030
#define SYS_TIMER_CTRL_INT	0x00000008
#define SYS_TIMER_CTRL_DOWN	0x00000004
#define SYS_TIMER_CTRL_UP	0x00000000
#define SYS_TIMER_CTRL_32	0x00000002
#define SYS_TIMER_CTRL_RELOAD	0x00000001

#define SYS_CS_DYN_MASK_EN	0x00000001

#define SYS_EXT_INT_CTRL_STS 	0x00000008
#define SYS_EXT_INT_CTRL_CLR 	0x00000004
#define SYS_EXT_INT_CTRL_PLTY_H	0x00000000
#define SYS_EXT_INT_CTRL_PLTY_L	0x00000002
#define SYS_EXT_INT_CTRL_EDGE	0x00000001
#define SYS_EXT_INT_CTRL_LEVEL	0x00000000

#define SYS_RTC_NORMAL_STAT	0x00000010
#define SYS_RTC_INT_READY	0x00000008
#define SYS_RTC_INT	 	0x00000004
#define SYS_RTC_NORMAL		0x00000002
#define SYS_RTC_STANDBY		0x00000000
#define SYS_RTC_INT_READY_CLEAR	0x00000001

#define SYS_POWER_SLEEP		0x80000000
#define SYS_POWER_HW_CLOCK	0x40000000
#define SYS_POWER_SELF_REFRESH	0x00200000
#define SYS_POWER_WAKE_INT_CLR	0x00100000
#define SYS_POWER_WAKE_INT_EXT( irq)	( 1 << ( ( irq ) + 16 ) )
#define SYS_POWER_WAKE_RTC	0x00001000
#define SYS_POWER_WAKE_I2C	0x00000800
#define SYS_POWER_WAKE_SPI	0x00000020
#define SYS_POWER_WAKE_UART( port )	( 1 << ( ( port ) + 1 ) )
#define SYS_POWER_WAKE_ETH	0x00000001

#endif /*__ASM_ARCH_NS9XXX_SYS_H*/
