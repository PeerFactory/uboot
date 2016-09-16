#include <config.h>

#ifdef CONFIG_BOOT_NAND

.globl _nandboot

/***********************************************************************
 * @Function: _nandboot (unsigned int unStartBlock,
 *			  unsigned int* punDest, unsigned int unLength)
 * @Return: always, check for bad blocks and verify ECC bytes
 * @Descr: copies blocks from NAND to RAM (punDest) until unLength data has been
 *	   read. unStartBlock
 ***********************************************************************/

#define rNF_BASE	(0x4e000000)

#define	oNF_CONF	(0x00)
#define	oNF_CONT	(0x04)
#define	oNF_CMD		(0x08)
#define oNF_ADDR	(0x0c)
#define oNF_DATA	(0x10)
#define oNF_STAT	(0x28)
#define oNF_MECCD0	(0x14)
#define oNF_MECCD1	(0x18)
#define oNF_MECC0	(0x34)
#define oNF_ESTAT0      (0x2c)
#define oNF_REG_CS	oNF_CONT
#define oNF_REG_CMD	oNF_CMD

#define vNF_CONF_RESET  (0xf830) /* NAND Controller enable, enable chip,
	                            TWRPH0 = 3 */
#define vNF_CS		(0x02)

#define vNF_BS_LARGE	(0x20000)	@ large block size
#define vNF_BS_SMALL	(0x4000)	@ small block size
#define vNF_PS_LARGE	(0x800)		@ large page size
#define vNF_PS_SMALL	(0x200)		@ small page size
#define vNF_OOB_LARGE	(0x40)		@ large oob size
#define vNF_OOB_SMALL	(0x10)		@ small oob size
#define vNF_BB_POS_L	(0x00)		@ bad block position for large NAND
#define vNF_BB_POS_S	(0x05)		@ bad block position for small NAND
#define vNF_CMD_READ0	(0x00)		@ command READ0
#define vNF_CMD_READS	(0x30)		@ command READ start for large NAND
#define vNF_CMD_READO	(0x50)		@ command READ OOB for small NAND
#define vNF_CMD_RESET	(0xff)		@ command RESET
@#define vNF_WAIT	(0x30)		@ counter for wait loop
#define vNF_WAIT	(0x100)		@ counter for wait loop
#define vNF_CHECK_BAD	(0xff)		@ value to check if block is bad
#define vNF_MECCLOCK	(0x80)		@ MainECCLock
#define vNF_INITMECC	(0x20)		@ Init MainECC

#define oNF_SP_ADDR	(0x1000)	@ address for SP in stepping stone
#define vNF_ECC_POS	(0x28)		@ start position of ECC data (24 Bytes)
#define oNF_ECC_DAT_R	(0xe20)		@ read ECC data in steppingstone


/* Flash enable by setting low nFCE */
.macro	flash_enable
	ldr	r4, [r3, #oNF_REG_CS]
	bic	r4, r4, #vNF_CS
	str	r4, [r3, #oNF_REG_CS]
.endm

/* Flash disable by setting high nFCE */
.macro	flash_disable
	ldr	r4, [r3, #oNF_REG_CS]
	orr	r4, r4, #vNF_CS
	str	r4, [r3, #oNF_REG_CS]
.endm

.macro	nand_wait_not_busy
1:	ldr	r4, [r3, #oNF_STAT]	@ Status register
	tst	r4, #0x01		@ RnB
	beq	1b
.endm

.macro delay reg, numloops
	ldr	\reg, =\numloops
2:	subs    \reg, \reg, #1
	bne	2b
.endm

.macro ndelay reg, numloops
	mov	\reg, #0
3:	add	\reg, \reg, #0x1
	cmp	\reg, #\numloops
	blt	3b
.endm

.macro short_delay
	nop
	nop
.endm


_nandboot:
	mov	r4, lr
	ldr	sp, =oNF_SP_ADDR	@ set up SP

	stmdb   sp!, {r4}

	mov	r3, #rNF_BASE

	ldr	r4, [r3]
	tst 	r4, #(0x1 << 3);
	ldreq	r6, =vNF_PS_SMALL	@ small page size
	ldrne	r6, =vNF_PS_LARGE	@ large page size
	flash_enable			@ chip enable
	ndelay	r4, vNF_WAIT		@ wait
	mov	r4, #vNF_CMD_RESET	@ send reset to NAND
	strb	r4, [r3, #oNF_REG_CMD]

	ndelay	r4, vNF_WAIT		@ wait
	nand_wait_not_busy		@ wait until ready
	flash_disable			@ disable the chip

_nandboot_read_page:
	@ read one page
	mov	ip, lr
	bl	_nandboot_bb_check	@ first check, if block is bad
	mov	lr, ip

	flash_enable			@ chip enable
	ldr     r4, [r3, #oNF_CONT]
        bic     r4, r4, #vNF_MECCLOCK	@ unlock main area ecc
        orr	r4, r4, #vNF_INITMECC   @ init/clear main ecc
	str     r4, [r3, #oNF_CONT]	@ ecc is genarted by hardware,
					@ while reading page
	ndelay	r4, vNF_WAIT		@ wait
	mov	r4, #vNF_CMD_READ0	@ READ0
	strb	r4, [r3, #oNF_REG_CMD]
	cmp	r6, #vNF_PS_SMALL
	beq	512f
	mov	r4, r0		        @ ADDR
        bic	r4, r4, #0x000000FF 	@ clear A0..7, start of sector
	strb	r4, [r3, #oNF_ADDR]	@ write address byte
	short_delay			@ short delay to acomplish tALS
  	mov	r4, r4, lsr #8		@ A8..11
        bic	r4, r4, #0x000000FF 	@ clear upper nibble and start of sector
	strb	r4, [r3, #oNF_ADDR]	@ write A8..11
	short_delay			@ short delay to acomplish tALS
	mov	r4, r0		        @ again whole ADDR
  	mov	r4, r4, lsr #11
	strb	r4, [r3, #oNF_ADDR]	@ write A11..18
	short_delay			@ short delay to acomplish tALS
  	mov	r4, r4, lsr #8
	strb	r4, [r3, #oNF_ADDR]	@ write A19..26
	short_delay			@ short delay to acomplish tALS
  	mov	r4, r4, lsr #8
	strb	r4, [r3, #oNF_ADDR]	@ write A27..31
	mov	r4, #vNF_CMD_READS	@ READ start
	strb	r4, [r3, #oNF_REG_CMD]
	b	1f
512:
	mov     r4, r0                  @ ADDR
        bic     r4, r4, #0x000000FF     @ clear A0..7, start of sector
        strb    r4, [r3, #oNF_ADDR]     @ write address byte
	short_delay			@ short delay to acomplish tALS
	mov     r4, r4, lsr #9          @ A8 not relevant here
        strb    r4, [r3, #oNF_ADDR]     @ write A9..16
	short_delay			@ short delay to acomplish tALS
	mov     r4, r4, lsr #8
        strb    r4, [r3, #oNF_ADDR]     @ write A17..24
	short_delay			@ short delay to acomplish tALS
	mov     r4, r4, lsr #8
        bic     r4, r4, #0x000000FC     @ clear A2..7
        strb    r4, [r3, #oNF_ADDR]     @ write A25..26

1:	ndelay	r4, vNF_WAIT		@ wait
	nand_wait_not_busy		@ wait until ready

	@ copy always one page size
	@ r2 is number of bytes overall left after this copy
	@ r5 is number in page left
	cmp	r2, r6
	subge	r2, r2, r6
	movlt	r2, #0
	mov	r5, r6

_nandboot_page_copy_to_mem:
	@ copy page to memory
	cmp	r5, #0
	beq	8f
	sub	r5, r5, #1

	ldrb	r4, [r3, #oNF_DATA]
	strb	r4, [r1], #1
	b	_nandboot_page_copy_to_mem

8:
	ldr     r4, [r3, #oNF_CONT]
        orr     r4, r4, #vNF_MECCLOCK	@ lock main area ecc
        str     r4, [r3, #oNF_CONT]	@ keep ecc

	flash_disable

	@ now we check ECC
	stmdb   sp!, {r0 - r6}
	sub	r1, r1, r6		@ set pointer to start of page
	ldr	r2, =oNF_ECC_DAT_R	@ pointer to ECC data read
	cmp	r6, #vNF_PS_SMALL
	addne	r2, r2, #vNF_ECC_POS	@ correct pointer for large NAND Flash

	mov	ip, lr
	bl	_nandboot_correct_data
	mov	lr, ip
	tst 	r0, #2
	beq	_ecc_correct
_ecc_error:
	ldr	r4, =0x56000014
	ldr	r5, [r4]
	eor	r5, r5, #0x04
	str	r5, [r4]

	ndelay	r3, 0x3000000	@ led delay
	b	_ecc_error

_ecc_correct:
	ldmia   sp!, {r0 - r6}

	cmp	r2, #0
	addne	r0, r0, r6          @ select next page

	bne	_nandboot_read_page

	@ finished, all bytes read
	ldmia   sp!, {lr}
	mov	pc, lr


/***********************************************************************
 * @Function: _nandboot_bb_check
 *
 * @Descr: checks 1st page of block, if block is bad
 *	   stores ECC bytes to RAM
 ***********************************************************************/
_nandboot_bb_check:
	stmdb   sp!, {r1, r2, r7 - r9}
_bb_check_loop:
	mov	r8, #0x00

	cmp	r6, #vNF_PS_SMALL
	beq 	512f
	mov	r5, #vNF_BS_LARGE	@ block size
	mov	r7, #vNF_OOB_LARGE	@ oob size
	mov	r9, #vNF_BB_POS_L	@ bad block position
	b	1f
512:
	mov     r5, #vNF_BS_SMALL       @ block size
        mov     r7, #vNF_OOB_SMALL      @ oob size
        mov     r9, #vNF_BB_POS_S       @ bad block position
1:
	mov	r4, r0		        @ ADDR
	cmp	r4, #0x00
	beq	20f
	mov	r8, r4, lsl #15		@ we only check 1st page
20:
	flash_enable			@ enable flash chip
	ndelay	r4, vNF_WAIT		@ wait
	mov	r4, #vNF_CMD_READO	@ READ OOB for small NAND
	cmp	r6, #vNF_PS_SMALL
	beq	23f
	mov	r4, #vNF_CMD_READ0	@ READ0
23:	strb	r4, [r3, #oNF_REG_CMD]

	cmp	r6, #vNF_PS_SMALL
	beq	512f
	mov	r4, r0		        @ ADDR
	orr	r4, r4, r6
        bic	r4, r4, #0x000000FF 	@ clear A0..7, start of sector
	strb	r4, [r3, #oNF_ADDR]	@ write address byte
  	mov	r4, r4, lsr #8		@ A8..11
        bic	r4, r4, #0x000000F7 	@ clear upper nibble and start of sector
	strb	r4, [r3, #oNF_ADDR]	@ write A8..11
	mov	r4, r0		        @ again whole ADDR
  	mov	r4, r4, lsr #11
	strb	r4, [r3, #oNF_ADDR]	@ write A11..18
  	mov	r4, r4, lsr #8
	strb	r4, [r3, #oNF_ADDR]	@ write A19..26
  	mov	r4, r4, lsr #8
	strb	r4, [r3, #oNF_ADDR]	@ write A27..31

	mov	r4, #vNF_CMD_READS	@ READ start
	strb	r4, [r3, #oNF_REG_CMD]
	b	1f
512:
	mov     r4, r0                  @ ADDR
        bic     r4, r4, #0x000000FF     @ clear A0..7, start of sector
        strb    r4, [r3, #oNF_ADDR]     @ write address byte
        mov     r4, r4, lsr #9          @ A8 not relevant here
        strb    r4, [r3, #oNF_ADDR]     @ write A9..16
        mov     r4, r4, lsr #8
        strb    r4, [r3, #oNF_ADDR]     @ write A17..24
        mov     r4, r4, lsr #8
        bic     r4, r4, #0x000000FC     @ clear A2..7
        strb    r4, [r3, #oNF_ADDR]     @ write A25..26
1:
	ndelay	r4, vNF_WAIT		@ wait
	nand_wait_not_busy		@ wait until ready

	ldr	r1, =oNF_ECC_DAT_R
	mov	r2, #0x00
21:
	ldrb	r4, [r3, #oNF_DATA]
	strb	r4, [r1, r2]		@ store ECC bytes
	add	r2, r2, #1
	cmp	r2, r7			@ read all oob data
	bne	21b

	cmp	r8, #0x00		@ 1st page of block?
	bne	22f
	ldrb	r4, [r1, r9]		@ read bad block info
	cmp	r4, #vNF_CHECK_BAD	@ bad block?
	beq	22f
	add	r0, r0, r5		@ next block
	flash_disable			@ disable the flash chip

	b	_bb_check_loop		@ check new block
22:
	cmp     r6, #vNF_PS_SMALL
	beq 	512f
	ldr	r4, =vNF_BB_POS_L
	b	1f
512:
	ldr	r4, =vNF_BB_POS_S
1:
	cmp	r9, r4
	beq	_bb_check_out
	ldrb	r4, [r1, #6]		@ move ECC4 to the correct place
	strb	r4, [r1, #4]
	ldrb	r4, [r1, #7]		@ move ECC5 to the correct place
	strb	r4, [r1, #5]

_bb_check_out:
	flash_disable			@ disable the flash chip
	ldmia   sp!, {r1, r2, r7 - r9}
	mov	pc, lr

/***********************************************************************
 * @Function: _nandboot_correct_data
 *
 * @Descr: compares calculated and read ECC values
 *	   correct error if possible
 *	   R0 = return value  (out)
 *	   R1 =	start of page (in)
 *	   R2 = ECC read from NAND (in)
 ***********************************************************************/
_nandboot_correct_data:
	stmdb   sp!, {r1 - r5, sl, lr}
	ldr	r0, [r2]
	and 	r4, r0, #0xff00
	and 	r5, r0,	#0xff
	orr 	r5, r5,	r4, lsl #8
	str     r5, [r3, #oNF_MECCD0]

	mov 	r0, r0, lsr #16
	and 	r4, r0, #0xff00
	and 	r5, r0, #0xff
	orr 	r5, r5,	r4, lsl #8
	str     r5, [r3, #oNF_MECCD1]

	ldr	r0, [r3, #oNF_ESTAT0]
	and	r0, r0, #0x3
	cmp 	r0, #0x1
	bne	out
	@ correctable error
	@ (nfestat0 >> 7) & 0x7ff :error byte number
        @ (nfestat0 >> 4) & 0x7   :error bit number
	ldr	r5, [r3, #oNF_ESTAT0]
	mov 	r2, r5, lsl #14
	mov	r2, r2, lsr #21
	mov 	r3, r5,	lsr #4
	and	r3, r3, #0x7

	mov	r4, #0x1
	mov 	r4, r4, lsl r3	@ pattern to clear error
	ldrb	r5, [r1, r2]	@ error byte in sdram
	eor	r5, r5, r4	@ toggle error bit
	strb	r5, [r1, r2]
out:
	ldmia   sp!, {r1 - r5, sl, pc}

#endif /* CONFIG_BOOT_NAND */
