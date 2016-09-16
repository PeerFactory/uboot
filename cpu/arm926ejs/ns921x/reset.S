/*
 *  cpu/arm926ejs/ns921x/reset.S
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
 *  !Reference:  [1] NS9215 Hardware Reference Manual, Preliminary January 2007
*/

#include <config.h>
#include <asm-arm/arch-ns9xxx/ns921x_sys.h>

	.align	4
.globl reset_cpu
reset_cpu:
          ldr   r1, =(SYS_BASE_PA + SYS_PLL)
          ldr   r2, [r1]
          str   r2, [r1]

_loop_forever:
	b	_loop_forever
        
