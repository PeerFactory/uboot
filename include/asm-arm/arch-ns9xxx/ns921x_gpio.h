/*
 * include/asm/arch-ns9xxx/ns921x_gpio.h
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

#ifndef __ASM_ARCH_NS921X_GPIO_H
#define __ASM_ARCH_NS921X_GPIO_H

#define GPIO_BASE_PA		0xA0902000

#define GPIO_GPIO_NR	        108
#define GPIO_CFG_BASE	   	0x0000
#define GPIO_CTRL_BASE		0x006C
#define GPIO_STAT_BASE		0x007C
#define GPIO_MEM		0x008C

/* register bit fields */

#define GPIO_CFG_MA		0xFF
#define GPIO_CFG_INPUT	   	0x00
#define GPIO_CFG_OUTPUT	   	0x04
#define GPIO_CFG_FUNC_4	   	0x20
#define GPIO_CFG_FUNC_GPIO	0x18
#define GPIO_CFG_FUNC_2	   	0x10
#define GPIO_CFG_FUNC_1	   	0x08
#define GPIO_CFG_FUNC_0	   	0x00
#define GPIO_CFG_PULLUP_DISABLE 0x01

#define GPIO_MEM_APUEN		0x02000000
#define GPIO_MEM_DHPUEN		0x01000000
#define GPIO_MEM_CS( cs, val )	( ( ( val ) & 0x7 ) << ( 3 * cs ) )
#define GPIO_MEM_DYN( cs )   	( cs )
#define GPIO_MEM_STAT( cs )  	( 4 + ( cs ) )

#endif /*__ASM_ARCH_NS921X_GPIO_H */
