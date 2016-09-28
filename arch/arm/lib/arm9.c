/*
 *  lib_arm/arm9.c
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision: 1.1 $
 *  !Author:     Markus Pietrek
 *  !References: [1] ARM Document DDI0198D_926_TRM.pdf
 *               [2] Cache/TCM initialization (926Dhry)
 *                   http://www.arm.com/support/downloads/info/2317.html
 *               [3] DDI0151C_920T_TRM.pdf
 *  !Descr: dcache on enables also the MMU for a flat memory model, otherwise
 *          it can't work. The SDRAM
 *          banks (reported by bdinfo) are configured for caching-writeback.
 *          dcache off disables the MMU as well.
 *          No specific commands have been added, so that the least set of
 *          changes needs to be added to the booting mechanism.
*/

#include <common.h>

#if defined(CONFIG_ARM926EJS) || defined(CONFIG_ARM920T)

#include <command.h>

/* return byte count of x in megabytes */
#define MiB( x )	( (x) * 1024 * 1024 )

/* See also ARM926EJ-S Technical Reference Manual */
#define C1_MMU		(1<<0)		/* mmu off/on */
#define C1_ALIGN	(1<<1)		/* alignment faults off/on */
#define C1_DC		(1<<2)		/* dcache off/on */

#define C1_BIG_ENDIAN	(1<<7)		/* big endian off/on */
#define C1_SYS_PROT	(1<<8)		/* system protection */
#define C1_ROM_PROT	(1<<9)		/* ROM protection */
#define C1_IC		(1<<12)		/* icache off/on */
#define C1_HIGH_VECTORS	(1<<13)		/* location of vectors: low/high addresses */

#define DOMAIN_USED	15      /* we use only one, no special meaning */

/* domain control (p15,c3) */
#define DOMAIN_CLIENT		1

/* First Level Descriptor for Section, see [1] 3.2.
 * Keep this low-level stuff in #define*/
#define FL_SEC_MAX_ENTRIES	4096
#define FL_SEC_ALIGNMENT	16384

/* bit fields */
#define FL_SEC_PAGE		0x00000002  /* section descriptor */
#define FL_SEC_CACHE_B		0x00000004  /* Bufferable,[1] 4.3, Table 4-4 */
#define FL_SEC_CACHE_C		0x00000008  /* Cacheable, [1] 4.3, Table 4-4 */
#define FL_SEC_BACKWARD		0x00000010  /* for backward compatibility */
#define FL_SEC_DOMAIN(x)	(( ( x ) & 0xf) << 5)
#define FL_SEC_AP_CLIENT	0x00000400  /* Check against permission */
#define FL_SEC_BASE(x)		(( ( x ) & (FL_SEC_MAX_ENTRIES - 1 )) << 20)  /* Base register starts at 20 */

/* general settings for our flat memory model */
#define FL_CFG_PAGE_COMMON	( FL_SEC_PAGE      | \
               		          FL_SEC_BACKWARD  | \
                                  FL_SEC_DOMAIN( DOMAIN_USED ) | \
                                  FL_SEC_AP_CLIENT )

/* No caching by default. Simulating no MMU behaviour */
#define FL_CFG_PAGE_EXTERN	FL_CFG_PAGE_COMMON
/* SDRAM is cached-writeback for best performance. Failures in DMA
   handling are detected most easily */
#define FL_CFG_PAGE_SDRAM	( FL_CFG_PAGE_COMMON | \
                                  FL_SEC_CACHE_B     | \
                                  FL_SEC_CACHE_C )


DECLARE_GLOBAL_DATA_PTR;

/* ********** local variables ********** */

#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
/* contains the Translation Table/First Level Descriptors
 * Must be aligned on 16KiB boundary, [1] 3.2.1 */
static uint32_t auiTranslationTable[ FL_SEC_MAX_ENTRIES ] __attribute__ ((aligned( FL_SEC_ALIGNMENT )));
#endif
/* ********** functions ********** */

/* read co-processor 15, register #1 (control register) */
static unsigned long read_p15_c1 (void)
{
	unsigned long value;

	__asm__ __volatile__(
		"mrc	p15, 0, %0, c1, c0, 0   @ read control reg\n"
		: "=r" (value)
		:
		: "memory");

#ifdef MMU_DEBUG
	printf ("p15/c1 is = %08lx\n", value);
#endif
	return value;
}

#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
/* write to co-processor 15, register #1 (control register) */
static void write_p15_c1 (unsigned long value)
{
#ifdef MMU_DEBUG
	printf ("write %08lx to p15/c1\n", value);
#endif
	__asm__ __volatile__(
		"mcr	p15, 0, %0, c1, c0, 0   @ write it back\n"
		:
		: "r" (value)
		: "memory");

	read_p15_c1 ();
}
#endif
/* write to co-processor 15, register #2, (translation table base) */
static inline void set_translation_table_base( void* pvBase )
{
	asm volatile (
		"mcr	p15, 0, %0, c2, c0, 0   @ write it\n"
		:
		: "r" ( pvBase )
		: "memory");
}

/* write to co-processor 15, register #3, (domain access control) */
static inline void set_domain_ctrl( uint32_t uiDomain, uint32_t uiMode )
{
	asm volatile (
		"mcr	p15, 0, %0, c3, c0, 0   @ write it\n"
		:
		: "r" ( (uiMode) << ((uiDomain) * 2 ) )
		: "memory");
}

void dcache_flush( void )
{
        int iDummyForSync;

        if( !dcache_status() )
                /* no cache */
                return;

        /* [1], 9.3 */
#if defined(CONFIG_ARM926EJS)
	asm volatile(
                /* clean all cache lines until there are no unclean ones */
                "1:\n"
                "mrc p15, 0, r15, c7, c10, 3\n"
                "bne 1b\n" );
      /* until no more are unclean */
#elif defined(CONFIG_ARM920T)
        /* [1], 2-23 A=6, S=3 from
         * [3], 4.3 16KiB, 64 ways, 32bytes per line*/
        /* from linux/arch/arm/mm/proc-arm920.S */
        asm volatile(
                "mov	r1, #(8 - 1) << 5	@ 8 segments\n"
                "1:\n"
                "orr	r3, r1, #(64 - 1) << 26 @ 64 entries\n"
                "2:\n"
                "mcr	p15, 0, r3, c7, c10, 2	@ clean D index\n"
                "subs	r3, r3, #1 << 26\n"
                "bcs	2b				@ entries 63 to 0\n"
                "subs	r1, r1, #1 << 5\n"
                "bcs	1b				@ segments 7 to 0\n"
                :
                :
                : "r1", "r3"
                );
#else
# error need flush
#endif
	asm volatile(
                /* invalidate I cache */
                "mcr	p15, 0, ip, c7, c5, 0\n"
                /* drain writebuffer */
                "mcr p15, 0, r0,  c7, c10, 4\n"
                /* nonbuffered store to signal L2 world to synchronize */
                "mov r0, #0\n"
                "str r0, %0\n"
                :
                : "m" ( iDummyForSync )
                : "r0"
                );
}

void dcache_invalidate( void )
{
        if( !dcache_status() )
                /* no cache */
                return;

        /* write back the current contents */
        dcache_flush();

#if defined(CONFIG_ARM926EJS)
        {
                int i = 0;
                /* invalidate data cache only */
                asm ("mcr p15, 0, %0, c7, c6, 0": :"r" (i));
        }
#elif defined(CONFIG_ARM920T)
        asm volatile(
                "mov	r1, #(8 - 1) << 5	@ 8 segments\n"
                "1:\n"
                "orr	r3, r1, #(64 - 1) << 26 @ 64 entries\n"
                "2:\n"
                "mcr	p15, 0, r3, c7, c14, 2	@ clean+invalidateD index\n"
                "subs	r3, r3, #1 << 26\n"
                "bcs	2b				@ entries 63 to 0\n"
                "subs	r1, r1, #1 << 5\n"
                "bcs	1b				@ segments 7 to 0\n"
                :
                :
                : "r1", "r3"
                );
#else
# error need some implementation
#endif
}


#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
/* prepare the translation table so the MMU knows where to find the SDRAM */
static void translation_table_init( void )
{
        uint32_t* puiSection = &auiTranslationTable[ 0 ];
        int i;

        /* where can the MMU find the first level descriptors/translat */
        set_translation_table_base( puiSection );

        /* create flat mapping, each page is 1 MiB */
        for( i = 0; i < ARRAY_SIZE( auiTranslationTable ); i++, puiSection++) {
                uint32_t uiCfgPage = FL_CFG_PAGE_EXTERN;
                int j;

                /* check whether we are in SDRAM and return setting */
                for( j = 0; j < ARRAY_SIZE( gd->bd->bi_dram ); j++ ) {
                        if( gd->bd->bi_dram[ j ].size &&
                            ( MiB( i ) >= gd->bd->bi_dram[ j ].start ) &&
                            ( MiB( i ) < ( gd->bd->bi_dram[ j ].start + gd->bd->bi_dram[ j ].size ) ) ) {
                                uiCfgPage = FL_CFG_PAGE_SDRAM;
                                break;
                        }
                } /* for( j = 0 ) */

                *puiSection = uiCfgPage | FL_SEC_BASE( i );
        }

        /* configure how the domain can be used */
        set_domain_ctrl( DOMAIN_USED, DOMAIN_CLIENT );
}

static void mmu_dcache_enable( void )
{
	int i = 0;
        
	/* set up the pages */
        translation_table_init();

	/* invalidate data cache only */
	asm ("mcr p15, 0, %0, c7, c6, 0": :"r" (i));

        /* enable mmu */
        write_p15_c1( read_p15_c1() | ( C1_MMU | C1_DC ) );

        /* two nops for the instruction pipeline. They are read unmapped, but
         * executed with mapping enabled. There shouldn't be any problems if
         * these ones are missed, but be safe */
	asm volatile (
                "mov r0,r0\n"
                "mov r0, r0\n"
                );
}

static void mmu_dcache_disable( void )
{
        dcache_invalidate();

        write_p15_c1( read_p15_c1() & ~( C1_MMU | C1_DC ) );

        /* two nops for the instruction pipeline. They are read mapped, but
         * executed with mapping disabled. There shouldn't be any problems if
         * these ones are missed, but be safe */
	asm volatile (
                "mov r0,r0\n"
                "mov r0, r0\n"
                );
}

static void cp_delay (void)
{
	volatile int i;

	/* copro seems to need some delay between reading and writing */
	for (i = 0; i < 100; i++);
}
#endif

int cpu_init (void)
{
	/*
	 * setup up stacks if necessary
	 */
#ifdef CONFIG_USE_IRQ
	IRQ_STACK_START = _armboot_start - CFG_MALLOC_LEN - CFG_GBL_DATA_SIZE - 4;
	FIQ_STACK_START = IRQ_STACK_START - CONFIG_STACKSIZE_IRQ;
#endif
	return 0;
}

int cleanup_before_linux (void)
{
	/*
	 * this function is called just before we call linux
	 * it prepares the processor for linux
	 *
	 * we turn off caches etc ...
	 */

	disable_interrupts ();

        icache_disable();
        dcache_disable();

	return (0);
}
#ifndef CONFIG_NS9215
int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	disable_interrupts ();

        serial_tx_flush();

	reset_cpu (0);
	/*NOTREACHED*/
	return (0);
}
#endif

void icache_enable (void)
{
#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
	ulong reg;

	reg = read_p15_c1 ();		/* get control reg. */
	cp_delay ();
	write_p15_c1 (reg | C1_IC);
#endif  /* (CONFIG_COMMANDS & CFG_CMD_CACHE) */
}

void icache_disable (void)
{
#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
	ulong reg;

	reg = read_p15_c1 ();
	cp_delay ();
	write_p15_c1 (reg & ~C1_IC);
#endif  /* (CONFIG_COMMANDS & CFG_CMD_CACHE) */
}

int icache_status (void)
{
	return (read_p15_c1 () & C1_IC) != 0;
}

void dcache_enable (void)
{
#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
	if(! dcache_status())
		/* we need MMU for caching */
		mmu_dcache_enable();
#endif  /* (CONFIG_COMMANDS & CFG_CMD_CACHE) */
}

void dcache_disable (void)
{
#if (CONFIG_COMMANDS & CFG_CMD_CACHE)
	if(dcache_status())
		mmu_dcache_disable();
#endif  /* (CONFIG_COMMANDS & CFG_CMD_CACHE) */
}

int dcache_status (void)
{
	return (read_p15_c1 () & C1_DC) != 0;
}
#endif /* (CONFIG_ARM926EJS) || defined(CONFIG_ARM920T) */
