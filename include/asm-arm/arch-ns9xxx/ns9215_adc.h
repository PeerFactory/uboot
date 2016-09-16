/*
 *  include/asm-arm/arch-ns9xxx/ns9215_adc.h
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

#ifndef __ASM_ARCH_NS9215_ADC_H
#define __ASM_ARCH_NS9215_ADC_H

#define ADC_BASE_PA		0x90039000

#define ADC_CFG			0x0000
#define ADC_CLOCK_CFG		0x0004
#define ADC_OUTPUT_BASE		0x0008

/* the vectored register addresses */

#define ADC_OUTPUT( ch )	( ADC_OUTPUT_BASE + ( ( ch ) & 0x7 ) * 4 )

/* register bit fields */

#define ADC_CFG_EN		0x80000000
#define ADC_CFG_INTSTAT( reg )	( ( ( reg ) >> 16 ) & 0x7 )
#define ADC_CFG_INTCLR		0x00000010
#define ADC_CFG_DMAEN		0x00000008
#define ADC_CFG_SEL_SET( ch )	( ( ch ) & 0x7 )
#define ADC_CFG_SEL_GET( reg )	( ( reg ) & 0x7 )

#define ADC_CLOCK_CFG_WAIT_MA       0xffff0000
#define ADC_CLOCK_CFG_WAIT_SET(val) ( ( (val) << 16 ) & ADC_CLOCK_CFG_WAIT_MA )
#define ADC_CLOCK_CFG_WAIT_GET(reg) ( ( (reg) & ADC_CLOCK_CFG_WAIT_MA ) >> 16 )
#define ADC_CLOCK_CFG_N_MA          0x000001ff
#define ADC_CLOCK_CFG_N_SET(val)    ( ( val ) & ADC_CLOCK_CFG_N_MA )
#define ADC_CLOCK_CFG_N_GET(reg)    ( ( reg ) & ADC_CLOCK_CFG_N_MA )

#endif /*__ASM_ARCH_NS9215_ADC_H*/
