/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
 *
 * (C) Copyright 2003
 * Texas Instruments, <www.ti.com>
 * Kshitij Gupta <Kshitij@ti.com>
 *
 * Copyright (C) 2005 by FS Forth-Systeme GmbH.
 * All rights reserved.
 * Jonas Dietsche <jdietsche@fsforth.de>
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
/*
 *  !Revision:   $Revision: 1.1 $
 *  !Author:     Markus Pietrek
 *  !Descr:
 *  !References: [1] NS9360 HW Reference Manual Rev. 03/2006
 *               [2] Email "new cc9c -05 deltas" from Bill Kumpf on 01/07/07
*/

#include <common.h>

#include <ns9750_bbus.h>
#include <ns9750_sys.h>
#include <ns9750_mem.h>
#include <nvram.h>
#include <net.h>

#ifdef CONFIG_STATUS_LED
# include <status_led.h>
static int status_led_fail = 0;
#endif /* CONFIG_STATUS_LED */

#include <env.h>
#include <partition.h>
#include "spi_ver.h"

#define BOOTSTRAP_CS		0x3
#define BOOTSTRAP_BASE_ADDRESS	0x30000000
#define WCE_BOOTARGS_ADDR	0x8000

static uint8_t ccx9c_get_bootstrapping( void );
#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
extern void run_auto_script(void);
#endif

/***********************************************************************
 * @Function: cc9c_flash_init
 * @Return: Nothing
 * @Descr: Configures CS1 for NAND flash access
 ***********************************************************************/
void cc9c_flash_init (void)
{
#ifndef CONFIG_CC9C_NAND
	/* setup CS1 */
	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_BASE( 1 ) ) = 0x50000000;
	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_MASK( 1 ) ) = 0xff000001;
	*get_mem_reg_addr( NS9750_MEM_STAT_CFG( 1 ) ) = NS9750_MEM_STAT_CFG_MW_16 | NS9750_MEM_STAT_CFG_PB;
	*get_mem_reg_addr( NS9750_MEM_STAT_WAIT_WEN( 1 ) ) = 0x2;
	*get_mem_reg_addr( NS9750_MEM_STAT_WAIT_OEN( 1 ) ) = 0x2;
	*get_mem_reg_addr( NS9750_MEM_STAT_RD( 1 ) ) = 0x6;
	*get_mem_reg_addr( NS9750_MEM_STAT_WR( 1 ) ) = 0x6;
#endif
}

/***********************************************************************
 * @Function: get_sdram_bank_size
 * @Return: The SDRAM size for the especified bank
 * @Descr: Returns the sdram size for the especified bank. If the SPI
 *         loader is used, that information is read from the CS
 *         configuration.
 ***********************************************************************/
static ulong get_sdram_bank_size (int bank)
{
	ulong base;
	ulong size = 0;

	switch (bank) {
		case 0:
#ifdef CONFIG_CC9C_NAND
			base = (*get_sys_reg_addr(NS9750_SYS_CS_DYN_BASE(0)) & 0xFFFFF000);
			size = ~(*get_sys_reg_addr(NS9750_SYS_CS_DYN_MASK(0)) & 0xFFFFF000) + 1;
			base += size;
			if ((*get_sys_reg_addr(NS9750_SYS_CS_DYN_BASE(1)) & 0xFFFFF000) == base)
				size += size;	/* second bank equipped */
#else
			size = PHYS_SDRAM_1_SIZE;
#endif
			break;
		default:	break;
	}
	return size;
}


/***********************************************************************
 * @Function: dram_init
 * @Return: 0 always
 * @Descr: Initializes the board SDRAM information
 ***********************************************************************/
int dram_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = get_sdram_bank_size(0);

#if CONFIG_NR_DRAM_BANKS > 1
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
#endif

        /* now we now the memory size, enable dcache */
	dcache_enable();

	return 0;
}

/***********************************************************************
 * @Function: cc9c_gpio_init
 * @Return: Nothing
 * @Descr: Processor GPIO initialization.
 ***********************************************************************/
void cc9c_gpio_init (void)
{
#if defined(CONFIG_USER_KEY)
	/* Configure Key1 and Key2 as inputs */
	set_gpio_cfg_reg_val( USER_KEY1_GPIO, 
			NS9750_GPIO_CFG_FUNC_GPIO | NS9750_GPIO_CFG_INPUT);
	set_gpio_cfg_reg_val( USER_KEY2_GPIO,
		       	NS9750_GPIO_CFG_FUNC_GPIO | NS9750_GPIO_CFG_INPUT);
#endif

#if (CONFIG_COMMANDS & CFG_CMD_BSP) &&         \
    defined(CONFIG_UBOOT_CMD_BSP_TESTHW) && \
    defined(CONFIG_STATUS_LED)
	/* Configure Led1 and Led2 as outputs */
	set_gpio_cfg_reg_val( USER_LED1_GPIO,
			NS9750_GPIO_CFG_FUNC_GPIO | NS9750_GPIO_CFG_OUTPUT);
	set_gpio_cfg_reg_val( USER_LED2_GPIO,
			NS9750_GPIO_CFG_FUNC_GPIO | NS9750_GPIO_CFG_OUTPUT);
	/* Set outputs to off */
	__led_set( USER_LED1_GPIO, STATUS_LED_OFF );
	__led_set( USER_LED2_GPIO, STATUS_LED_OFF );
#endif

#if defined(CONFIG_SILENT_CONSOLE) && defined(ENABLE_CONSOLE_GPIO)
	set_gpio_cfg_reg_val( ENABLE_CONSOLE_GPIO, NS9750_GPIO_CFG_FUNC_GPIO |
			NS9750_GPIO_CFG_INPUT);
#endif
}

/***********************************************************************
 * @Function: board_init
 * @Return: 0
 * @Descr: Enables BBUS modules and other devices
 ***********************************************************************/
int board_init( void ) {
	DECLARE_GLOBAL_DATA_PTR;

	icache_enable();
        /* dcache is enabled after dram has been initialized */

	/* Active BBUS modules */
	*get_bbus_reg_addr( NS9750_BBUS_MASTER_RESET ) = 0;

	gd->bd->bi_arch_number = machine_arch_type;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x00000100;

	cc9c_gpio_init();

#if defined(CONFIG_STATUS_LED)
	status_led_set(STATUS_LED_BOOT,STATUS_LED_ON);
#endif /* CONFIG_STATUS_LED */

/* this speeds up your boot a quite a bit.  However to make it
 *  work, you need make sure your kernel startup flush bug is fixed.
 *  ... rkw ...
 */
	cc9c_flash_init();

	return 0;
}



#ifdef BOARD_LATE_INIT

/***********************************************************************
 * @Function: cc9c_run_user_key_commands
 * @Return: Nothing
 * @Descr: Runs key1 | key2 macro commands, if the corresponding user
 *         keys are pressed.
 ***********************************************************************/
void cc9c_run_user_key_commands (void)
{
#if defined(CONFIG_USER_KEY)
	char cmd[10];

	if (get_gpio_stat(USER_KEY1_GPIO) == 0) {
		printf("\nUser Key 1 pressed\n");
		if (getenv("key1") != NULL) {
			sprintf(cmd, "run key1");
			run_command(cmd, 0);
		}
	}
	if (get_gpio_stat(USER_KEY2_GPIO) == 0) {
		printf("\nUser Key 2 pressed\n");
		if (getenv("key2") != NULL) {
			sprintf(cmd, "run key2");
			run_command(cmd, 0);
		}
	}
#endif /* CONFIG_USER_KEY */
}


/***********************************************************************
 * @Function: board_late_init
 * @Return: 0
 * @Descr: Late initialitation routines.
 ***********************************************************************/
int board_late_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;
	nv_critical_t *pNvram;
	volatile uint32_t *wce_bootargs = (volatile uint32_t *)WCE_BOOTARGS_ADDR;

	printf( "CPU:   %s @%i.%iMHz\n", CPU, CPU_CLK_FREQ/1000000 , CPU_CLK_FREQ %1000000);

	printf( "Strap: 0x%02x\n", ccx9c_get_bootstrapping() );

#ifdef CONFIG_HAVE_SPI_LOADER
        ns9xxx_print_spi_version();
#endif  /* CONFIG_HAVE_SPI_LOADER */

	if (NvCriticalGet(&pNvram)) {
		/* Pass location of environment in memory to OS through bi */
		gd->bd->nvram_addr = (unsigned long)pNvram;
	}

	/* copy bdinfo to start of boot-parameter block */
 	memcpy((int*)gd->bd->bi_boot_params, gd->bd, sizeof(bd_t));

	/* Clean Windows CE bootargs signature at address WCE_BOOTARGS_ADDR
	 * This is necessary to ensure WinCE knows when is in debug mode
	 * Eboot writes the magic signature on that address */
	*wce_bootargs = 0;

        eth_use_mac_from_env( gd->bd );

	/* Run keyX commands */
	cc9c_run_user_key_commands();


#if defined(CONFIG_STATUS_LED)
	if (!status_led_fail) {
		status_led_set(STATUS_LED_BOOT,STATUS_LED_OFF);
		status_led_fail = 0;
  	}
#endif /* CONFIG_STATUS_LED */

#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
	run_auto_script();
#endif

	return 0;
}
#endif /*BOARD_LATE_INIT*/

/***********************************************************************
 * @Function: ccx9c_get_bootstrapping
 * @Return:   bootstraping of CCX9C Module
 * @Descr:    BOOTSTRAP_CS is not routed extern. So we use it to determine the
 *            pull-up/downed bootstrap info.
 ***********************************************************************/

static uint8_t ccx9c_get_bootstrapping( void )
{
        uint8_t ucBootStrapping = 0;

        /* access it as mentioned in [2] */
	*get_mem_reg_addr( NS9750_MEM_STAT_CFG( BOOTSTRAP_CS ) ) =
                ( NS9750_MEM_STAT_CFG_PSMC |
                  NS9750_MEM_STAT_CFG_BSMC |
                  NS9750_MEM_STAT_CFG_MW_32 );
        /* we need a lot of waitstates to have bus
         * settled because some lines are floating */
        *get_mem_reg_addr( NS9750_MEM_STAT_RD( BOOTSTRAP_CS ) ) = 0xf;

        /* set CS3 to defaults, [1] p.164 */

	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_BASE( BOOTSTRAP_CS ) ) =
                BOOTSTRAP_BASE_ADDRESS;
	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_MASK( BOOTSTRAP_CS ) ) =
                0xf0000001;

        ucBootStrapping = ((*(volatile uint32_t*) BOOTSTRAP_BASE_ADDRESS) >> 24) & 0xff;

        /* we don't need CS3 any longer, disable it */
	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_MASK( BOOTSTRAP_CS ) ) = 0xf0000000;

        return ucBootStrapping;
}

#ifdef CONFIG_SHOW_BOOT_PROGRESS
void show_boot_progress (int status) {
#if defined(CONFIG_STATUS_LED)
	if (status < 0) {
        	/* ready to transfer to kernel, make sure LED is proper state */
        	status_led_set(STATUS_LED_BOOT,STATUS_LED_ON);
        	status_led_fail = 1;
	}
#endif /* CONFIG_STATUS_LED */
}
#endif /* CONFIG_SHOW_BOOT_PROGRESS */

#if defined(CONFIG_SILENT_CONSOLE)
int test_console_gpio (void)
{
#if defined(ENABLE_CONSOLE_GPIO) && defined(CONSOLE_ENABLE_GPIO_STATE)
	if (get_gpio_stat(ENABLE_CONSOLE_GPIO) == CONSOLE_ENABLE_GPIO_STATE)
		return 1;
	else
		return 0;
#else
	return 0;
#endif
}
#endif
