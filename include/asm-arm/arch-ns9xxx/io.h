/*
 * include/asm/arch-ns9xxx/io.h
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
 * !Descr:    Provides XXX_readl and XXX_writel. These are OS specific
 *            Include the header file first
 * !Author:   Markus Pietrek
 *
 ***********************************************************************/

#ifndef __ASM_ARCH_IO_H
#define __ASM_ARCH_IO_H

#include "asm-arm/arch-ns9xxx/ns921x_hub.h"

#include <asm/io.h>
/* 
 * u-boot is compiled with -Os -g by default. These functions are so small,
 * and sometimes should run as fast as possible (e.g. in udelay), they should
 * be always inlined. */


#ifdef MEM_BASE_PA
static inline u32  mem_readl( u32 uOffs ) __attribute__ ((always_inline));
static inline void mem_writel( u32 uVal, u32 uOffs ) __attribute__ ((always_inline));

# define mem_rmw32( offs, op ) mem_writel( mem_readl( offs ) op, (offs))
static inline u32 mem_readl( u32 uOffs )
{
        u32 val = readl( MEM_BASE_PA + uOffs );

        return val;
}
static inline void mem_writel( u32 uVal, u32 uOffs )
{
        writel( uVal, MEM_BASE_PA + uOffs );
}
#endif  /* MEM_BASE_PA */

#ifdef SYS_BASE_PA
static inline u32  sys_readl( u32 uOffs ) __attribute__ ((always_inline));
static inline void sys_writel( u32 uVal, u32 uOffs ) __attribute__ ((always_inline));

# define sys_rmw32( offs, op ) sys_writel( sys_readl( offs ) op, (offs))

static inline u32 sys_readl( u32 uOffs )
{
        u32 uVal = readl( SYS_BASE_PA + uOffs );

        return uVal;
}
static inline void sys_writel( u32 uVal, u32 uOffs )
{
        writel( uVal, SYS_BASE_PA + uOffs );
}
#endif  /* SYS_BASE_PA */

#ifdef HUB_BASE_PA
static inline u32  hub_port_readl( u32 uPort, u32 uOffs ) __attribute__ ((always_inline));
static inline u8   hub_port_readb( u32 uPort, u32 uOffs ) __attribute__ ((always_inline));
static inline void hub_port_writel( u32 uPort, u32 uVal, u32 uOffs ) __attribute__ ((always_inline));
static inline void hub_port_writeb( u32 uPort, u8 ucVal, u32 uOffs ) __attribute__ ((always_inline));
# define hub_rmw32( offs, op ) hub_writel( hub_readl( offs ) op, (offs))
static inline u32 hub_port_readl( u32 uPort, u32 uOffs )
{
        u32 uVal = readl( HUB_BASE_PA + ( HUB_MODULE_OFFSET * uPort ) + uOffs );

        return uVal;
}
static inline u8 hub_port_readb( u32 uPort, u32 uOffs )
{
        u8 ucVal = readb( HUB_BASE_PA + ( HUB_MODULE_OFFSET * uPort ) + uOffs );

        return ucVal;
}
static inline void hub_port_writel( u32 uPort, u32 uVal, u32 uOffs )
{
        writel( uVal, HUB_BASE_PA + ( HUB_MODULE_OFFSET * uPort ) + uOffs );
}
static inline void hub_port_writeb( u32 uPort, u8 ucVal, u32 uOffs )
{
        writeb( ucVal, HUB_BASE_PA + ( HUB_MODULE_OFFSET * uPort ) + uOffs );
}
#endif  /* HUB_BASE_PA */

#ifdef GPIO_BASE_PA
static inline u32  gpio_readl( u32 uOffs ) __attribute__ ((always_inline));
static inline void gpio_writel( u32 uVal, u32 uOffs ) __attribute__ ((always_inline));
static inline uint gpio_cfg_bit_pos( uint uPin ) __attribute__ ((always_inline));
static inline uint gpio_cfg_reg_offs( uint uPin ) __attribute__ ((always_inline));
static inline void gpio_cfg_set( uint uPin, uint uFunction ) __attribute__ ((always_inline));
static inline int  gpio_cfg_get( uint uPin ) __attribute__ ((always_inline));
static inline uint gpio_ctrl_reg_offs ( uint uPin ) __attribute__ ((always_inline));
static inline uint gpio_ctrl_bit_pos ( uint uPin ) __attribute__ ((always_inline));
static inline void gpio_ctrl_set( uint uPin, uchar uVal ) __attribute__ ((always_inline));
static inline uchar gpio_ctrl_get( uint uPin ) __attribute__ ((always_inline));
static inline uchar gpio_stat_get( uint uPin ) __attribute__ ((always_inline));

static inline u32 gpio_readl( u32 uOffs )
{
        u32 uVal = readl( GPIO_BASE_PA + uOffs );

        return uVal;
}
static inline void gpio_writel( u32 uVal, u32 uOffs )
{
        writel( uVal, GPIO_BASE_PA + uOffs );
}

static inline uint gpio_cfg_bit_pos( uint uPin )
{
        return ( uPin & 0x3 ) * 8;
}

static inline uint gpio_cfg_reg_offs( uint uPin )
{
        return ( uPin / 4 ) * 4;
}

static inline void gpio_cfg_set( uint uPin, uint uFunction )
{
        u32  uOffs   = GPIO_CFG_BASE + gpio_cfg_reg_offs( uPin );
        u32  uBitpos = gpio_cfg_bit_pos( uPin );
        u32  uVal    = ( ( gpio_readl( uOffs ) & ~( 0xff << uBitpos ) ) |
                        ( uFunction << uBitpos ) );

        gpio_writel( uVal, uOffs );
}

static inline int gpio_cfg_get( uint uPin )
{
        u32 uOffs   = GPIO_CFG_BASE + gpio_cfg_reg_offs( uPin );
        u32 uBitpos = gpio_cfg_bit_pos( uPin );
        u32 uVal    = ( gpio_readl( uOffs ) >> uBitpos ) & 0xff;

        return uVal;
}

static inline uint gpio_ctrl_reg_offs ( uint uPin )
{
        return ( uPin / 32 ) * 4;
}

static inline uint gpio_ctrl_bit_pos ( uint uPin )
{
        return uPin & 0x1f;
}

static inline void gpio_ctrl_set( uint uPin, uchar uVal )
{
        u32 uOffs   = GPIO_CTRL_BASE + gpio_ctrl_reg_offs( uPin );
        u32 uBitpos = gpio_ctrl_bit_pos( uPin );
        
        gpio_writel( ( (gpio_readl( uOffs ) & ~( 1 << uBitpos ) ) |
                          ( ( ( uVal & 0x1 ) << uBitpos ) ) ),
                        uOffs );
}

static inline uchar gpio_ctrl_get( uint uPin )
{
        u32 uOffs   = GPIO_CTRL_BASE + gpio_ctrl_reg_offs( uPin );
        u32 uBitpos = gpio_ctrl_bit_pos( uPin );
        u32 uVal    = gpio_readl( uOffs );

        return ( uVal >> uBitpos ) & 0x1;
}

static inline uchar gpio_stat_get( uint uPin )
{
        u32 uOffs   = GPIO_STAT_BASE + gpio_ctrl_reg_offs( uPin );
        u32 uBitpos = gpio_ctrl_bit_pos( uPin );
        uchar ucVal = ( gpio_readl( uOffs ) >> uBitpos ) & 0x1;

        return ucVal;
}
#endif  /* GPIO_BASE_PA */

#ifdef RTC_BASE_PA
static inline u32  rtc_readl( u32 uOffs ) __attribute__ ((always_inline));
static inline void rtc_writel( u32 uVal, u32 uOffs ) __attribute__ ((always_inline));

# define rtc_rmw32( offs, op ) rtc_writel( rtc_readl( offs ) op, (offs))

static inline u32 rtc_readl( u32 uOffs )
{
        u32 uVal = readl( RTC_BASE_PA + uOffs );

        return uVal;
}
static inline void rtc_writel( u32 uVal, u32 uOffs )
{
        writel( uVal, RTC_BASE_PA + uOffs );
}
#endif  /* RTC_BASE_PA */

#ifdef ADC_BASE_PA
static inline u32  adc_readl( u32 uOffs ) __attribute__ ((always_inline));
static inline void adc_writel( u32 uVal, u32 uOffs ) __attribute__ ((always_inline));

# define adc_rmw32( offs, op ) adc_writel( adc_readl( offs ) op, (offs))

static inline u32 adc_readl( u32 uOffs )
{
        u32 uVal = readl( ADC_BASE_PA + uOffs );

        return uVal;
}
static inline void adc_writel( u32 uVal, u32 uOffs )
{
        writel( uVal, ADC_BASE_PA + uOffs );
}
#endif  /* ADC_BASE_PA */

#endif /* __ASM_ARCH_IO_H */
