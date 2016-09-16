/*
 * include/asm/arch-ns9xxx/ns9xxx_mem.h
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

#ifndef __ASM_ARCH_NS9XXX_MEM_H
#define __ASM_ARCH_NS9XXX_MEM_H

#define MEM_BASE_PA		0xA0700000

#define MEM_CTRL     		0x0000 
#define MEM_STATUS     		0x0004
#define MEM_CFG     		0x0008
#define MEM_DYN_CTRL 		0x0020
#define MEM_DYN_REFRESH 	0x0024
#define MEM_DYN_READ_CFG	0x0028
#define MEM_DYN_TRP     	0x0030
#define MEM_DYN_TRAS    	0x0034
#define MEM_DYN_TSREX   	0x0038
#define MEM_DYN_TAPR   		0x003C
#define MEM_DYN_TDAL    	0x0040
#define MEM_DYN_TWR     	0x0044
#define MEM_DYN_TRC     	0x0048
#define MEM_DYN_TRFC    	0x004C
#define MEM_DYN_TXSR    	0x0050
#define MEM_DYN_TRRD    	0x0054
#define MEM_DYN_TMRD    	0x0058
#define MEM_STAT_EXT_WAIT	0x0080
#define MEM_DYN_CFG_BASE	0x0100
#define MEM_DYN_RAS_CAS_BASE	0x0104
#define MEM_STAT_CFG_BASE	0x0200
#define MEM_STAT_WAIT_WEN_BASE	0x0204
#define MEM_STAT_WAIT_OEN_BASE	0x0208
#define MEM_STAT_WAIT_RD_BASE	0x020C
#define MEM_STAT_WAIT_PAGE_BASE	0x0210
#define MEM_STAT_WAIT_WR_BASE	0x0214
#define MEM_STAT_WAIT_TURN_BASE	0x0218

/* the vectored register addresses */

#define MEM_DYN_CFG(c)		( MEM_DYN_CFG_BASE        + (c) * 0x20 )
#define MEM_DYN_RAS_CAS(c)	( MEM_DYN_RAS_CAS_BASE    + (c) * 0x20 )
#define MEM_STAT_CFG(c)		( MEM_STAT_CFG_BASE       + (c) * 0x20 )
#define MEM_STAT_WAIT_WEN(c)	( MEM_STAT_WAIT_WEN_BASE  + (c) * 0x20 )
#define MEM_STAT_WAIT_OEN(c)	( MEM_STAT_WAIT_OEN_BASE  + (c) * 0x20 )
#define MEM_STAT_RD(c)		( MEM_STAT_WAIT_RD_BASE   + (c) * 0x20 )
#define MEM_STAT_PAGE(c)	( MEM_STAT_WAIT_PAGE_BASE + (c) * 0x20 )
#define MEM_STAT_WR(c)		( MEM_STAT_WAIT_WR_BASE   + (c) * 0x20 )
#define MEM_STAT_TURN(c)	( MEM_STAT_WAIT_TURN_BASE + (c) * 0x20 )

/* register bit fields */

#define MEM_CTRL_L		0x00000004
#define MEM_CTRL_M		0x00000002
#define MEM_CTRL_E		0x00000001

#define MEM_STAT_SA		0x00000004
#define MEM_STAT_S		0x00000002
#define MEM_STAT_B		0x00000001

#define MEM_CFG_CLK		0x00000010
#define MEM_CFG_N		0x00000001

#define MEM_DYN_CTRL_NRP	0x00004000
#define MEM_DYN_CTRL_DP		0x00002000
#define MEM_DYN_CTRL_I_MA	0x00000180
#define MEM_DYN_CTRL_I_NORMAL 	0x00000000
#define MEM_DYN_CTRL_I_MODE	0x00000080
#define MEM_DYN_CTRL_I_PALL	0x00000100
#define MEM_DYN_CTRL_I_NOP	0x00000180
#define MEM_DYN_CTRL_BIT5	0x00000020
#define MEM_DYN_CTRL_SR		0x00000004
#define MEM_DYN_CTRL_BIT1	0x00000002
#define MEM_DYN_CTRL_CE		0x00000001


#define MEM_DYN_REFRESH_VAL( val ) ( ( val ) & 0x000007FF )

#define MEM_DYN_READ_CFG_MA	0x00000003
#define MEM_DYN_READ_CFG_DELAY0 0x00000001
#define MEM_DYN_READ_CFG_DELAY1 0x00000002
#define MEM_DYN_READ_CFG_DELAY2 0x00000003

#define MEM_DYN_TRP_VAL( val )   ( ( val ) & 0x0000000F )
#define MEM_DYN_TRAS_VAL( val )  ( ( val ) & 0x0000000F )
#define MEM_DYN_TSREX_VAL( val ) ( ( val ) & 0x0000000F )
#define MEM_DYN_TAPR_VAL( val )  ( ( val ) & 0x0000000F )
#define MEM_DYN_TDAL_VAL( val )	 ( ( val ) & 0x0000000F )
#define MEM_DYN_TWR_VAL( val )   ( ( val ) & 0x0000000F )
#define MEM_DYN_TRC_VAL( val )	 ( ( val ) & 0x0000001F )
#define MEM_DYN_TRFC_VAL( val )  ( ( val ) & 0x0000001F )
#define MEM_DYN_TXSR_VAL( val )  ( ( val ) & 0x0000001F )
#define MEM_DYN_TRRD_VAL( val )  ( ( val ) & 0x0000000F )
#define MEM_DYN_TMRD_VAL( val )	 ( ( val ) & 0x0000000F )

#define MEM_STAT_EXTW_WAIT_VAL( val ) ( ( val ) & 0x0000003F )

#define MEM_DYN_CFG_P	   	0x00100000
#define MEM_DYN_CFG_BDMC	0x00080000
#define MEM_DYN_CFG_AM	   	0x00004000
#define MEM_DYN_CFG_AM1( val )  ( (val) & 0x00001F80 )
#define MEM_DYN_CFG_AM1_SIZE( reg )  ( ( ( reg ) >> 9 ) & 0x7 )
#define MEM_DYN_CFG_MD	   	0x00000018

#define MEM_DYN_RAS_CAS_CAS( val ) ( ( ( val ) & 0x3 ) << 8 )
#define MEM_DYN_RAS_CAS_RAS( val ) ( ( ( val ) & 0x3 ) )

#define MEM_STAT_CFG_PSMC	0x00100000
#define MEM_STAT_CFG_BSMC	0x00080000
#define MEM_STAT_CFG_EW	   	0x00000100
#define MEM_STAT_CFG_PB	   	0x00000080
#define MEM_STAT_CFG_PC	   	0x00000040
#define MEM_STAT_CFG_PM	   	0x00000008
#define MEM_STAT_CFG_BMODE	0x00000004
#define MEM_STAT_CFG_MW_MA	0x00000003
#define MEM_STAT_CFG_MW_8	0x00000000
#define MEM_STAT_CFG_MW_16      0x00000001
#define MEM_STAT_CFG_MW_32	0x00000002

#define MEM_STAT_WAIT_WEN_VAL( val ) ( ( val ) & 0x0000000F )
#define MEM_STAT_WAIT_OEN_VAL( val ) ( ( val ) & 0x0000000F )
#define MEM_STAT_RD_VAL( val )       ( ( val ) & 0x0000001F )
#define MEM_STAT_PAGE_VAL( val )     ( ( val ) & 0x0000001F )
#define MEM_STAT_WR_VAL( val )       ( ( val ) & 0x0000001F )
#define MEM_STAT_TURN_VAL( val )     ( ( val ) & 0x0000000F )

#endif /*__ASM_ARCH_NS9XXX_MEM_H*/
