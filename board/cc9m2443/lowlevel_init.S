/*
 * Memory Setup stuff - taken from blob memsetup.S
 *
 * Copyright (C) 1999 2000 2001 Erik Mouw (J.A.K.Mouw@its.tudelft.nl) and
 *                     Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * Modified for the Samsung SMDK2410 by
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 */


#include <config.h>
#include <version.h>

#include <regs.h>

_TEXT_BASE:
	.word	TEXT_BASE

	.globl lowlevel_init
lowlevel_init:
	mov	r12, lr

ldr     r0, =ELFIN_GPIO_BASE
        ldr     r1, =0x10
        str     r1, [r0, #GPBCON_OFFSET]

        ldr     r1, =0x04
        str     r1, [r0, #GPBDAT_OFFSET]
/* Disable Watchdog */
	ldr	r0, =ELFIN_WATCHDOG_BASE
	mov	r1, #0
	str	r1, [r0]

	/* Disable main and sub interrupts */
	ldr	r0, =ELFIN_INTERRUPT_BASE
	mvn	r1, #0x0
	str	r1, [r0, #INTMSK_OFFSET]
	str	r1, [r0, #INTSUBMSK_OFFSET]

	/* Set all interrupts as IRQ */
	mov	r1, #0x0
	str	r1, [r0, #INTMOD_OFFSET]

#ifndef CONFIG_DOWNLOAD_BY_DEBUGGER
	/* init system clock */
	bl system_clock_init
#endif

	/* simple init for NAND */
	bl nand_asm_init

	/* when we already run in ram, we don't need to relocate U-Boot.
	 * and actually, memory controller must be configured before U-Boot
	 * is running in ram.
	 */
	ldr	r0, =0xff000fff
	bic	r1, pc, r0		/* r0 <- current base addr of code */
	ldr	r2, _TEXT_BASE		/* r1 <- original base addr in ram */
	bic	r2, r2, r0		/* r0 <- current base addr of code */
	cmp     r1, r2                  /* compare r0, r1                  */
	beq     2f			/* r0 == r1 then skip sdram init   */
#ifndef CONFIG_DOWNLOAD_BY_DEBUGGER
	ldr     r1, =GPFDAT_REG
	ldr     r4, [r1]
	tst     r4, #(0x1 << 3)
	bne     64f
	/* 16 or 32 MB ram */
	tst     r4, #(0x1 << 4)
	ldrne   r0, =sdram_bank_set_val_32
	b       4f
	/* 64 or 128 MB ram */
64:	tst     r4, #(0x1 << 4)
	ldreq   r0, =sdram_bank_set_val_64
	ldrne   r0, =sdram_bank_set_val_128

4:	bl sdr_ctrl_asm_init
#endif
2:	ldr     r0, =RSTSTAT_REG
	ldr 	r1, [r0]
	ldr	r2, =0x8
	cmp     r1, r2
	ldrne	r2, =0x20
	cmpne   r1, r2
	bne   	2f
	/* recover from sleep mode */

	/* PM check */
	@ Check if this is a wake-up from sleep

	ldr 	r0, =INFORM1_REG	/* INFORM1 register */
        ldr 	r1, [r0]
        mov 	pc, r1 			 /* end PM check */

2:	mov	lr, r12
	mov	pc, lr

/*
 * system_clock_init: Initialize core clock and bus clock.
 * void system_clock_init(void)
 */
system_clock_init:
	ldr	r0, =ELFIN_CLOCK_POWER_BASE

	ldr	r1, [r0, #CLKDIV0CON_OFFSET]
	bic	r1, r1, #0x37	/* clear HCLKDIV, PREDIV, PCLKDIV */
	bic	r1, r1, #(0xf<<9) /* clear ARMCLKDIV */
	ldr     r3, =GPHDAT_REG
        ldr     r4, [r3]
        tst     r4, #(1 << 12)
        ldrne   r2, =0xf    /* FCLK CPU HCLK PCLK Mhz 533 533 133 66 */
        ldreq   r2, =0x102D  /* FCLK CPU HCLK PCLK MHz 800 400 133 66 */
	orr	r1, r1, r2
	str	r1, [r0, #CLKDIV0CON_OFFSET]

	/*Set MPLL Locktime*/
	ldr	r1, =0xe10
	str	r1, [r0, #LOCKCON0_OFFSET]

	ldr 	r1, =GPHDAT_REG
	ldr     r2, [r1]
	tst     r2, #(1 << 12)
	ldrne 	r1, =0x00510201    /* CPU Mhz 533 133 66*/
	ldreq	r1, =0x005c0300    /* CPU MHz 400 133 66 */
	str	r1, [r0, #MPLLCON_OFFSET]

#if 1
	ldr	r1, =0x00280101		/*Set EPLL*/
	str	r1, [r0, #EPLLCON_OFFSET]
#endif

	/* set CLKSRC to fit MPLL and USB*/
	ldr	r1, [r0, #CLKSRCCON_OFFSET]
	orr	r1, r1, #0x50
	str	r1, [r0, #CLKSRCCON_OFFSET]

#if 0
	/* wait at least 200us to stablize all clock */
	mov	r1, #0x10000
1:	subs	r1, r1, #1
	bne	1b
#endif

	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0xc0000000	/* clock setting in MMU */
	mcr	p15, 0, r0, c1, c0, 0

	mov	pc, lr

/*
 * Nand Interface Init for smdk2443
 */
nand_asm_init:
	ldr	r0, =ELFIN_GPIO_BASE
	ldr	r1, [r0]
	orr	r1, r1, #(0x3f<<17)
	str     r1, [r0]
	/* EVT1 has modification in NAND */
#if 0
	ldr	r1, =0x1aa8a
	str     r1, [r0, #GPADAT_OFFSET]
#endif

	ldr	r0, =ELFIN_NAND_BASE
	ldr	r1, [r0, #NFCONF_OFFSET]
	orr	r1, r1, #0x70
	orr	r1, r1, #0x7700
	str     r1, [r0, #NFCONF_OFFSET]

	ldr	r1, [r0, #NFCONT_OFFSET]
	orr	r1, r1, #0x03
	str     r1, [r0, #NFCONT_OFFSET]

	mov	pc, lr

	.ltorg
/*
 * struct sdram_val {
 *	ulong bankcfg_val;	r5, val of bankcfg
 *	ulong bankcon1_val;	r6, val of bankcon1
 *	ulong bankcon2_val;	r7, val of bankcon2
 *	ulong bankcon3_val;	r8, val of bankcon3
 *	ulong refresh_val;	r9, val of refresh
 *	ulong wbtimout_val;	r10, val of write buffer timeout
 * }
 */
sdram_bank_set_val_128:
       .word   CFG_BANK_CFG_VAL_128
       .word   CFG_BANK_CON1_VAL
       .word   CFG_BANK_CON2_VAL_133
       .word   CFG_BANK_CON3_VAL
       .word   CFG_BANK_REFRESH_VAL_133_128
       .word   CFG_BANK_TIMEOUT_VAL_133_128

sdram_bank_set_val_64:
	.word	CFG_BANK_CFG_VAL_64
	.word	CFG_BANK_CON1_VAL
	.word	CFG_BANK_CON2_VAL_133
	.word	CFG_BANK_CON3_VAL
	.word	CFG_BANK_REFRESH_VAL_133_64
	.word   CFG_BANK_TIMEOUT_VAL_133_64

sdram_bank_set_val_32:
	.word   CFG_BANK_CFG_VAL_32
	.word   CFG_BANK_CON1_VAL
	.word   CFG_BANK_CON2_VAL_133
	.word   CFG_BANK_CON3_VAL
	.word   CFG_BANK_REFRESH_VAL_133_32
	.word   CFG_BANK_TIMEOUT_VAL_133_32

#ifdef CONFIG_SDRAM_AUTO_DETECT
sdram_bank_sel_val_alt:
	.word	CFG_BANK_CFG_VAL_ALT
	.word	CFG_BANK_CON1_VAL_ALT
#endif

