/*
 *  board/cc9p921x/platform.S
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
 *  !References: [1] Sebastien's bdi2000 debugger config file
 *               [2] /net/r/TEILE/TM02981/64MSDRAM.pdf
 *  !Descr:      No autodetection of SDRAM is performed. It is hardcoded into assembler. If different memory is being used
*/

#include <config.h>

#include <status_led.h>

#include <asm-arm/arch-ns9xxx/ns921x_sys.h>
#include <asm-arm/arch-ns9xxx/ns921x_gpio.h>

#define C1_IC		(1<<12)		/* icache off/on */
#define DUMMY1		0xdeadbeef  @ For "kicking" the SDRAM controller
#define DUMMY2		0xbadc0ffe  @ and associated internal cache

/* calibrated using _blink_led: BLINK_ERROR 0x55555555 and BLINK_DELAY 1 */
#define COUNT_FOR_1MS 	0x9248


/**
 * mdelay - MACRO. Delays for about 1ms or longer (calibrated on
 *          149 MHz CC9P9215)
 * @r7: temp
 * @r8: temp
 */
.macro	mdelay ms
        ldr     r7, =\ms
1:
        ldr     r8, =COUNT_FOR_1MS
2:
        /* wait approx 1ms */
        subs    r8, r8, #1
        bne     2b

        /* count down milliseconds */
        subs    r7, r7, #1
        bne     1b
.endm /* mdelay */

/**
 * write_register_block - MACRO
 * @r0: register base address
 * @r1: start of register set (in RAM)
 * @r2: end of register set (in RAM)
 * @r3: linked RAM base address of U-Boot
 * @r4: ROM base address
 * @r5: temp
 * @r6: temp
 * @return: nothing
 *
 * Copies the register block of register_offset/register value to
 * the registers at base r0
 */

.macro	write_register_block
	/* convert the RAM addresses (+TEXT_BASER) to ROM addresses (0x0) */
	sub	r1, r1, r3
	add	r1, r1, r4
	sub	r2, r2, r3
	add	r2, r2, r4

	/* copy all */
1:
	/* Write register/value pair starting at [r1] to register base r0 */
	ldr	r5, [r1], #4
	ldr	r6, [r1], #4
	str	r6, [r0,r5]
	cmp	r1, r2
	blt	1b
.endm /* write_register_block */

/**
 * gpio_ctrl_set_0 - sets the GPIO to 0
 * @r0-r4: tmp
 */
.macro gpio_ctrl_set_0	gpio
        ldr	r1, =( GPIO_BASE_PA + GPIO_CTRL_BASE )
        ldr     r2, =( ( \gpio / 32 ) * 4 )
        mov     r3, #( \gpio & 0x1f )
        mov     r4, #1
        ldr     r0, [r1, r2 ]
        bic     r0, r0, r4, lsl r3
        str     r0, [r1, r2 ]
.endm

/**
 * gpio_ctrl_set_1 - sets the GPIO to 1
 * @r0-r4: tmp
 */
.macro gpio_ctrl_set_1	gpio
        ldr	r1, =( GPIO_BASE_PA + GPIO_CTRL_BASE )
        ldr     r2, =( ( \gpio / 32 ) * 4 )
        mov     r3, #( \gpio & 0x1f )
        mov     r4, #1
        ldr     r0, [r1, r2 ]
        orr     r0, r0, r4, lsl r3
        str     r0, [r1, r2 ]
.endm

/**
 * gpio_cfg_as_output - MACRO
 * @r0-r5: tmp
 *
 * GPIO is set as output
 * Copies the register block of register_offset/register value to
 * the registers at base r0
 */
.macro  gpio_cfg_as_output	gpio
        /* see gpio_cfg_set() */
        ldr	r1, =( GPIO_BASE_PA + GPIO_CFG_BASE )
        ldr     r2, =( \gpio / 4 ) * 4
        ldr     r3, =( ( \gpio & 0x3 ) * 8 )
        ldr     r4, =(GPIO_CFG_OUTPUT | GPIO_CFG_FUNC_GPIO)
        ldr     r5, =GPIO_CFG_MA
        ldr     r0, [r1, r2 ]
        bic     r0, r0, r5, lsl r3
        orr     r0, r0, r4, lsl r3
        str     r0, [r1, r2 ]
.endm

/**
 * gpio_block_cfg_as_func0 - configures a block of gpio, start at gpio_start
 * and ending at gpio_end, as func0
 * @r0-r8: tmp
 */
.macro  gpio_block_cfg_as_func0	gpio_start, gpio_end
        ldr     r6, =\gpio_start
        ldr     r7, =\gpio_end
        ldr	r1, =( GPIO_BASE_PA + GPIO_CFG_BASE )
        ldr     r4, =GPIO_CFG_FUNC_0
        ldr     r5, =GPIO_CFG_MA
        ldr     r8, =~0x3
1:
        /* see gpio_cfg_set() */
        mov     r2, r6
        and     r2, r2, r8
        mov     r3, r6
        and     r3, r3, #0x3
        mov     r3, r3, lsl #0x3
        ldr     r0, [r1, r2 ]
        bic     r0, r0, r5, lsl r3
        orr     r0, r0, r4, lsl r3
        str     r0, [r1, r2 ]

        add     r6, r6, #1
        cmp     r6, r7
        ble     1b
.endm

#define BLINK_ERROR     0xA8CCC2A0 /* SOS, 3 short, 3 long, 3 short */
#define BLINK_DELAY	0x00000100

#ifndef CONFIG_DOWNLOAD_BY_DEBUGGER
#include <asm-arm/arch-ns9xxx/ns9xxx_mem.h>

/* ********** defines ********** */

#define CS0     	0
#define CS1		1

#define CS0_MODE	0x00019800 /* [2], Burst length = 3, CAS latency = 3 */

/* Memory Setup Definitions:
 * struct { u32 uOffs; u32 uVal; } axCfg[];
 * for each axCfg[i], uVal needs to be written to r0+uOffs

/* Boot flash, [1] */
_CS1_CFG_START:
        .word	MEM_STAT_CFG( CS1 )
        .word	MEM_STAT_CFG_PB | \
                MEM_STAT_CFG_MW_16

        .word	MEM_STAT_WAIT_WEN( CS1 )
        .word	MEM_STAT_WAIT_WEN_VAL( 0 )

        .word	MEM_STAT_WAIT_OEN( CS1 )
        .word	MEM_STAT_WAIT_OEN_VAL( 1 )

        .word	MEM_STAT_RD( CS1 )
        .word	MEM_STAT_RD_VAL( 0x8 )

        .word	MEM_STAT_WR( CS1 )
        .word	MEM_STAT_WR_VAL( 0x8 )

        .word	MEM_STAT_PAGE( CS1 )
        .word	MEM_STAT_PAGE_VAL( 0 )

        .word	MEM_STAT_TURN( CS1 )
        .word	MEM_STAT_TURN_VAL( 2 )
_CS1_CFG_END:

/* Main SDRAM, [1] */
_CS0_CFG_BASE_START:
        .word   MEM_DYN_REFRESH
        .word   MEM_DYN_REFRESH_VAL( 0x18 )

        .word   MEM_DYN_READ_CFG
        .word   MEM_DYN_READ_CFG_DELAY0 /*Command delayed strategy, using CLKDELAY (command delayed, clock out not delayed) */

        .word   MEM_DYN_TRP
        .word   MEM_DYN_TRP_VAL( 1 )  /* tRP = 27ns (min) => 2xAHB_CLK */

        .word   MEM_DYN_TRAS
        .word   MEM_DYN_TRAS_VAL( 4 )  /* tRAS = 67ns (min) => 5xAHB_CLK */

        .word   MEM_DYN_TSREX
        .word   MEM_DYN_TSREX_VAL( 5 )  /*tSREX = 80ns (min) => 6xAHB_CLK */

        .word   MEM_DYN_TAPR
        .word   MEM_DYN_TAPR_VAL( 1 )  /* value programmed in tRP - 1 */

        .word   MEM_DYN_TDAL
        .word   MEM_DYN_TDAL_VAL( 5 )  /* tDAL/APW = 6tCK (min) => 6xAHB_CLK */

        .word   MEM_DYN_TWR
        .word   MEM_DYN_TWR_VAL( 1 )  /* tWR/DPL/RWL/RDL = 2tCK (min) => 2xAHB_CLK */

        .word   MEM_DYN_TRC
        .word   MEM_DYN_TRC_VAL( 6 )  /* tRC = 80ns */

        .word   MEM_DYN_TRFC
        .word   MEM_DYN_TRFC_VAL( 6 )  /* tRFC = 80ns */

        .word   MEM_DYN_TXSR
        .word   MEM_DYN_TXSR_VAL( 5 )  /* tXSR = 80ns (min) */

        .word   MEM_DYN_TRRD
        .word   MEM_DYN_TRRD_VAL( 1 )  /* tRRD = 2tCK (min) => 2xAHB_CLK */

        .word   MEM_DYN_TMRD
        .word   MEM_DYN_TMRD_VAL( 1 )  /* tMRC = 2tCK (min) => 2xAHB_CLK */

_CS0_CFG_BASE_END:

_CS0_CFG_BASE2_START:
        .word   MEM_DYN_RAS_CAS( CS0 )
        .word   MEM_DYN_RAS_CAS_CAS( 0x3 ) | \
                MEM_DYN_RAS_CAS_RAS( 0x3 )

        .word   MEM_DYN_CTRL
        .word   MEM_DYN_CTRL_I_PALL | \
                MEM_DYN_CTRL_BIT1   | \
                MEM_DYN_CTRL_CE

        .word   MEM_DYN_REFRESH
        .word   MEM_DYN_REFRESH_VAL( 0x1 ) /* Set to minimum for having eight
                                               SDRAM access during the
                                              1ms wait time */
_CS0_CFG_BASE2_END:

/* a delay needs to be done before setting CFG_MODE values after CFG_BASE */
_CS0_CFG_MODE_START:
        .word   MEM_DYN_REFRESH
        .word   MEM_DYN_REFRESH_VAL( 0x18 ) /*((64e-3 / 4096) * 75e+6) / 16 = 73 = 0x49 */

        .word   MEM_DYN_CTRL
        .word   MEM_DYN_CTRL_I_MODE | \
                MEM_DYN_CTRL_BIT1   | \
                MEM_DYN_CTRL_CE
_CS0_CFG_MODE_END:

/* the memory needs to be configured by reading before setting CFG_NORMAL */
_CS0_CFG_NORMAL_START:
        .word   MEM_DYN_CTRL
        .word   MEM_DYN_CTRL_I_NORMAL | \
                MEM_DYN_CTRL_BIT1     | \
                MEM_DYN_CTRL_CE
_CS0_CFG_NORMAL_END:
#endif /* CONFIG_DOWNLOAD_BY_DEBUGGER */

_TEXT_BASE:
	.word	TEXT_BASE	/* sdram load addr from config.mk */
_PHYS_FLASH_1:
	.word	PHYS_FLASH_1    /* real flash address on CS1 */

/** _blink_led - blinks the bit pattern in r0, starting from 0.
 * @r0:  contains the 32bit pattern, e.g. BLINK_ERROR
 * @r1: temp (register address of GPIO)
 * @r2: temp (GPIO register)
 * @r3: temp (bit position in GPIO register)
 * @r4: temp (always #1)
 * @r5: temp (bit position in r0)
 * @r6: GPIO control register
 * @return: never *
 *
 * bit set to 1 means light, 0 means off
 */

	.type _blink_led,function
_blink_led:
        /* determine GPIO address */
        ldr	r1, =( GPIO_BASE_PA + GPIO_CTRL_BASE )
        ldr     r2, =( ( STATUS_LED_BIT / 32 ) * 4 )
        mov     r3, #( STATUS_LED_BIT & 0x1f )
        ldr     r6, [r1, r2 ]   /* get current GPIO value */
        mov     r4, #1

_blink_led_pattern:
        mov     r5, #0         /* start at bit 0 in r0 */

_blink_led_pattern_bit:
        tst     r0, r4, lsl r5  @ what bit is set in r0
        biceq   r6, r6, r4, lsl r3
        orrne   r6, r6, r4, lsl r3
        str     r6, [r1, r2 ]   /* light or darken LED based on bit in r0 */

        mdelay  BLINK_DELAY     /* human eyes are so sloooowwwwww */

        /* determine next bit to be displayed*/
        add     r5, r5, #1
        cmp     r5, #32
        bne	_blink_led_pattern_bit

        /* continue from beginning */
        b       _blink_led_pattern

/** _handle_data_mismatch - blinks SOS on data mismatch
 * @r0: address1 _after_ error
 * @r1: address2 _after_ bogus word
 * @r3: data word from address1
 * @r4: data word from address2
 *
 * r0,r1 will be moved to r7,r8 and r3,r4 to r12,r13, so a debugger might
 * return it.
 */

	.type _handle_data_mismatch,function
        .globl _handle_data_mismatch
_handle_data_mismatch:
        mov     r7,  r0
        mov     r8,  r1
        mov     r12, r3
        mov     r13, r4
        ldr     r0, =BLINK_ERROR
        b       _blink_led

/** lowlevel_init - configure memory settings and error LED
 *
 * r12 and r14 both contain link registers from cpu_init_critical and reset.
 * They mustn't be changed.
 */
	.type lowlevel_init,function
        .globl lowlevel_init
lowlevel_init:
__full_cpu_clock_and_low_power:
        /* CPU will be run with 2*x AHB clock, not necessary modules are
           disabled for EMI */
        ldr     r1, =( SYS_BASE_PA + SYS_CLOCK )
        ldr     r2, =( SYS_CLOCK_CSC_1     | \
                       SYS_CLOCK_MAX_CSC_1 | \
                       SYS_CLOCK_CCSEL     | \
                       SYS_CLOCK_MCCOUT( 0 ) | \
                       SYS_CLOCK_IO )
        str     r2, [r1]

_error_led_set_on:
        gpio_ctrl_set_0    STATUS_LED_BIT

_error_led_cfg_as_output:
        gpio_cfg_as_output STATUS_LED_BIT

#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
_enable_icache:
        /* boost instruction processing */
        mrc     15, 0, r0, cr1, cr0
        orr     r0, r0, #C1_IC
        mcr     15, 0, r0, cr1, cr0
#endif

#ifdef CFG_STRAP_PUT_ETH_OUT_OF_RESET
        /* is strapping pin set? */
        ldr     r1, = (SYS_BASE_PA + SYS_GEN_ID)
        ldr     r2, [r1]
        ldr     r3, =CFG_STRAP_PUT_ETH_OUT_OF_RESET
        ands     r2, r2, r3
        bne     _phy_cfg_done  /* not wanted, e.g. to save power */

_put_phy_out_of_reset:
        /* strapping is set, reduce network setup time by doing it here.
           This means more power and EMI */
        ldr     r1, = (SYS_BASE_PA + SYS_CLOCK)
        ldr     r2, [r1]
        ldr     r3, = SYS_CLOCK_ETH
        orr     r2, r2, r3
        str     r2, [r1]

_configure_eth_gpios:
        /* the phy needs the correct signals */
        gpio_block_cfg_as_func0 32, 49

_take_phy_out_of_reset:
        gpio_ctrl_set_1    GPIO_ETH_PHY_RESET
        gpio_cfg_as_output GPIO_ETH_PHY_RESET
_phy_cfg_done:
#endif /* CFG_STRAP_PUT_ETH_OUT_OF_RESET */

#ifndef CONFIG_DOWNLOAD_BY_DEBUGGER
_configure_mem_ctrl:
        /* where we relocate data from/to. */
       	ldr	r3, _TEXT_BASE
	ldr	r4, _PHYS_FLASH_1
        /* don't touch r3 and r4 any longer. They will be needed a lot */
_finetune_cs1:
        ldr	r0, =MEM_BASE_PA
	ldr	r1, =_CS1_CFG_START
	ldr	r2, =_CS1_CFG_END

	write_register_block

_check_running_from_PHYS_FLASH_1:
        /* check to see if we are running from 0x0 (normal reset) or
         * already from PHYS_FLASH_1 (software jump to 0x50000000).
         * Then there is no relocate + PHYS_FLASH_1 */
        mov     r0, pc
        ldr     r1, =_PHYS_FLASH_1
        cmp     r0, r1
        bgt     _setup_cs0_from_cs1

_prepare_jump_to_cs1:
       	/* at this point, we are running from 0x0, which will be CS0
           Adjust pc and lr to be on CS1 */
        add     lr, lr, r4      /* return to CS1:cpu_init_crit, not 0:cpu_init_crit. This has been called from 0x0, not TEXT_BASE */
        add     ip, ip, r4      /* return to CS1:reset, not 0:reset */

      	ldr	r0, =_setup_cs0_from_cs1
	sub	r0, r0, r3      /* PC is also on CS1 */
	add	r0, r0, r4
	mov	pc, r0
	nop			/* for pipelining */

_setup_cs0_from_cs1:
	/* now we are running from CS1 (PHYS_FLASH_1), safe to
         *  hazzle with CS0 */

	/* Enable SDRAM */
	ldr	r0, =MEM_BASE_PA
	ldr	r1, =MEM_CTRL_E
	str	r1, [r0, #MEM_CTRL]

	/* NOP SDRAM command */
#define NOP_SDRAM	0x183
	ldr	r1, =NOP_SDRAM
	str	r1, [r0, #MEM_DYN_CTRL]

	/* Wait at least for 200ms before PALL command */
	mdelay	200

	ldr	r0, =MEM_BASE_PA
	ldr	r1, =_CS0_CFG_BASE_START
	ldr	r2, =_CS0_CFG_BASE_END

	write_register_block

	/* Set Dynamic Memory Configuration 0 register
	   depending on content of GEN_ID */

	ldr     r1, =(SYS_BASE_PA + SYS_GEN_ID)
	ldr     r9, [r1]
	mov	r9, r9, lsr #8
	and	r9, r9, #7
	rsb	r9, r9, #8

	ldr	r5, =0x80
	orr	r9, r5, r9, lsl #9
	ldr	r1, =(MEM_BASE_PA + MEM_DYN_CFG(CS0))
	str	r9, [r1]

	/* RAS/CAS, PALL command, REFRESH 1 */
	ldr	r0, =MEM_BASE_PA
	ldr	r1, =_CS0_CFG_BASE2_START
	ldr	r2, =_CS0_CFG_BASE2_END

	write_register_block

	/* Give the SDRAM some time to stabilize by executing
	 * some dummy operations */
#define DUMMY_REG	0xa0900000
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]
	ldr	r1, =DUMMY_REG
	ldr	r2, [r0, #0]

       	ldr	r1, =_CS0_CFG_MODE_START
	ldr	r2, =_CS0_CFG_MODE_END

	write_register_block

        ldr     r5, =CS0_MODE
	/* if bit 10 is set -> need to shift one more */
	tst	r9, #0x400
	movne	r5, r5, lsl #1
        ldr     r6, [r5] /* A read transaction from the SDRAM memory programs the mode register */

       	ldr	r1, =_CS0_CFG_NORMAL_START
	ldr	r2, =_CS0_CFG_NORMAL_END

	write_register_block

	ldr	r5, =MEM_DYN_CFG_BDMC
	orr	r9, r9, r5
	ldr	r1, =(MEM_BASE_PA + MEM_DYN_CFG(CS0))
	str	r9, [r1]

	/* Do some dummy reads, writes, and then more reads (from
	 * locations we didn't write to) in the hopes that there
	 * SDRAM controller will become stable and the internal
	 * cache will be sane */
	mov	r0, #0
	ldr	r1, [r0, #0x0]
	ldr	r1, [r0, #0x4]
	ldr	r1, [r0, #0x8]
	ldr	r1, [r0, #0xc]

	ldr	r1, =DUMMY1
	ldr	r2, =DUMMY2
	str	r1, [r0, #0x0]
	str	r2, [r0, #0x4]
	str	r1, [r0, #0x8]
	str	r2, [r0, #0xc]

	ldr	r1, [r0, #0x10]
	ldr	r1, [r0, #0x14]
	ldr	r1, [r0, #0x18]
	ldr	r1, [r0, #0x1c]
        /* all registers, even r3,r4  can now be modified */

        /* Detect SDRAM */
#endif /* CONFIG_DOWNLOAD_BY_DEBUGGER */

       	mov	pc, lr		/* cpu_init_crit will continue */
