/*
 *  include/asm-arm/arch-ns9xxx/ns921x_rtc.h
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

#ifndef __ASM_ARCH_NS921X_RTC_H
#define __ASM_ARCH_NS921X_RTC_H

#define RTC_BASE_PA		0x90060000

#define RTC_CTRL		0x0000
#define RTC_24H			0x0004
#define RTC_TIME		0x0008
#define RTC_DATE		0x000C
#define RTC_ALARM_TIME		0x0010
#define RTC_ALARM_CALENDAR	0x0014
#define RTC_ALARM_EN		0x0018
#define RTC_EVENT		0x001C
#define RTC_INT_EN		0x0020
#define RTC_INT_DIS		0x0024
#define RTC_INT_STAT		0x0028
#define RTC_GEN_STAT		0x002C

/* register bit fields */

#define RTC_CTRL_CAL		0x00000002
#define RTC_CTRL_TIME		0x00000001

#define RTC_24H_12		0x00000001
#define RTC_24H_24		0x00000000

#define RTC_TIME_PM		0x40000000

/* GET returns BCD code, SET expects BCD */
#define RTC_TIME_HR_GET( reg )	( ( ( reg ) >> 24  ) & 0x3f )
#define RTC_TIME_HR_SET( val )	( ( ( val ) & 0x3f ) << 24  )
#define RTC_TIME_M_GET( reg  )	( ( ( reg ) >> 16  ) & 0x7f )
#define RTC_TIME_M_SET( val  )	( ( ( val ) & 0x7f ) << 16  )
#define RTC_TIME_S_GET( reg  )	( ( ( reg ) >> 8   ) & 0x7f )
#define RTC_TIME_S_SET( val  )	( ( ( val ) & 0x7f ) << 8   )
#define RTC_TIME_H_GET( reg  )	( ( ( reg ) & 0xff ) )
#define RTC_TIME_H_SET( val  )	( ( ( val ) & 0xff ) )

#define RTC_DATE_C_GET( reg )	( ( ( reg ) >> 24  ) & 0x3f )
#define RTC_DATE_C_SET( val )	( ( ( val ) & 0x3f ) << 24  )
#define RTC_DATE_Y_GET( reg )	( ( ( reg ) >> 16  ) & 0xff )
#define RTC_DATE_Y_SET( val )	( ( ( val ) & 0xff ) << 16  )
#define RTC_DATE_D_GET( reg )	( ( ( reg ) >> 8   ) & 0x3f )
#define RTC_DATE_D_SET( val )	( ( ( val ) & 0x3f ) << 8   )
#define RTC_DATE_M_GET( reg )	( ( ( reg ) >> 3   ) & 0x1f )
#define RTC_DATE_M_SET( val )	( ( ( val ) & 0x1f ) << 3   )
#define RTC_DATE_DAY_GET( reg )	( ( ( reg ) & 0x7  ) )
#define RTC_DATE_DAY_SET( val )	( ( ( val ) & 0x7  ) )

#define RTC_GEN_STAT_VCAC	0x00000008
#define RTC_GEN_STAT_VTAC	0x00000004
#define RTC_GEN_STAT_VCC	0x00000002
#define RTC_GEN_STAT_VTC	0x00000001
#define RTC_GEN_STAT_ALL_VALID	( RTC_GEN_STAT_VCAC | \
                                  RTC_GEN_STAT_VTAC | \
                                  RTC_GEN_STAT_VCC  | \
                                  RTC_GEN_STAT_VTC )

#define RTC_EVENT_ALARM		0x00000040
#define RTC_EVENT_MONTH		0x00000020
#define RTC_EVENT_DATE		0x00000010
#define RTC_EVENT_HOUR		0x00000008
#define RTC_EVENT_MINUTE	0x00000004
#define RTC_EVENT_SEC		0x00000002
#define RTC_EVENT_HSEC		0x00000001

#endif  /* __ASM_ARCH_NS921X_RTC_H */
