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
 * Copyright (C) 2004-2005 by FS Forth-Systeme GmbH.
 * All rights reserved.
 * Markus Pietrek <mpietrek@fsforth.de>
 * derived from omap1610innovator.c
 * @References: [1] NS9750 Hardware Reference/December 2003
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

#include <common.h>

#include <ns9750_bbus.h>
#include <ns9750_mem.h>
#include <ns9750_sys.h>
#include <nvram.h>
#include <net.h>
#include "spi_ver.h"

#ifdef CONFIG_STATUS_LED
# include <status_led.h>
static int status_led_fail = 0;
#endif /* CONFIG_STATUS_LED */

#ifdef CONFIG_IEEE1588
#include "fpga_checkbitstream.h"
#endif

void flash__init( void );
void ether__init( void );
static void cc9p9360_gpio_init (void);
#ifdef CONFIG_A9M9750DEV
static void a9m9750dev_fpga_init(void);
#endif
#ifdef CONFIG_IEEE1588
static void ieee1588_load_firmware(void);
#endif
#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
extern void run_auto_script(void);
#endif

#define WCE_BOOTARGS_ADDR	0x8000

/***********************************************************************
 * @Function: board_init
 * @Return: 0
 * @Descr: Enables BBUS modules and other devices
 ***********************************************************************/
int board_init( void )
{
	DECLARE_GLOBAL_DATA_PTR;

	icache_enable();
        /* dcache is enabled after dram has been initialized */

	/* Active BBUS modules */
	*get_bbus_reg_addr( NS9750_BBUS_MASTER_RESET ) = 0;

	gd->bd->bi_arch_number = machine_arch_type;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x00000100;

	cc9p9360_gpio_init();

#ifdef CONFIG_A9M9750DEV
	a9m9750dev_fpga_init();
#endif

	return 0;
}

/***********************************************************************
 * @Function: cc9p9360_gpio_init
 * @Return: Nothing
 * @Descr: Processor GPIO initialization on cc9p9360 module
 ***********************************************************************/
static void cc9p9360_gpio_init (void)
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

#ifdef CONFIG_A9M9750DEV
static void a9m9750dev_fpga_init(void)
{
	/* setup CS0 */
	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_BASE( 0 ) ) = 0x40000000;
	*get_sys_reg_addr( NS9750_SYS_CS_STATIC_MASK( 0 ) ) = 0xfffff001;
	*get_mem_reg_addr( NS9750_MEM_STAT_CFG( 0 ) ) = NS9750_MEM_STAT_CFG_MW_8 | NS9750_MEM_STAT_CFG_PB;
	*get_mem_reg_addr( NS9750_MEM_STAT_WAIT_WEN( 0 ) ) = 0x2;
	*get_mem_reg_addr( NS9750_MEM_STAT_WAIT_OEN( 0 ) ) = 0x2;
	*get_mem_reg_addr( NS9750_MEM_STAT_RD( 0 ) ) = 0x6;
	*get_mem_reg_addr( NS9750_MEM_STAT_WR( 0 ) ) = 0x6;

	/* setup CS2 */
        *get_sys_reg_addr( NS9750_SYS_CS_STATIC_BASE( 2 ) ) = 0x60000000;
        *get_sys_reg_addr( NS9750_SYS_CS_STATIC_MASK( 2 ) ) = 0xff000001;
        *get_mem_reg_addr( NS9750_MEM_STAT_CFG( 2 ) ) = NS9750_MEM_STAT_CFG_MW_16 | NS9750_MEM_STAT_CFG_PB;
        *get_mem_reg_addr( NS9750_MEM_STAT_WAIT_WEN( 2 ) ) = 0x2;
        *get_mem_reg_addr( NS9750_MEM_STAT_WAIT_OEN( 2 ) ) = 0x2;
        *get_mem_reg_addr( NS9750_MEM_STAT_RD( 2 ) ) = 0x6;
        *get_mem_reg_addr( NS9750_MEM_STAT_WR( 2 ) ) = 0x6;
}
#endif

#ifdef CONFIG_IEEE1588
static void ieee1588_load_firmware(void)
{
	int ret;
	const nv_param_part_t* pPartEntry;

	if(!NvParamPartFind(&pPartEntry, NVPT_FPGA, NVFS_NONE, 0, 0))
		ret = LOAD_FPGA_FAIL;
	else {
		PartRead(pPartEntry, (void *)CFG_LOAD_ADDR, CFG_FPGA_SIZE, 1);
		ret = fpga_checkbitstream((void *)CFG_LOAD_ADDR, CFG_FPGA_SIZE);
	}
	if(ret == LOAD_FPGA_OK) {
		printf("FPGA: bitstream ok, loading to device...\n");
		ret = fpga_load((void *)CFG_LOAD_ADDR, CFG_FPGA_SIZE);
	}
	if(ret != LOAD_FPGA_OK) {
		run_command("setenv bootcmd", 0);
		printf("autoboot disabled due to FPGA problems!\n");
	}
}
#endif


int misc_init_r (void)
{
	/* currently empty */
	return (0);
}

void dev_board_info( void )
{
	/* Nothing to do */
}

/***********************************************************************
 * @Function: Get_SDRAM_Size
 * @Return: The SDRAM size for the first bank
 * @Descr: Returns the sdram size for the first bank. The information
 *         is read from the CS configuration.
 ***********************************************************************/
static ulong Get_SDRAM_Size(void)
{
	/* !!!This function is only working, if the correct SPI bootloader is used!!! */
        ulong size;
	ulong base;

	base = (*get_sys_reg_addr(NS9750_SYS_CS_DYN_BASE(0)) & 0xFFFFF000);
	size = ~(*get_sys_reg_addr(NS9750_SYS_CS_DYN_MASK(0)) & 0xFFFFF000) + 1;
	base += size;

	if ((*get_sys_reg_addr(NS9750_SYS_CS_DYN_BASE(1)) & 0xFFFFF000) == base)
		size += size;	/* second bank equipped */

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
	gd->bd->bi_dram[0].size = Get_SDRAM_Size();

#if CONFIG_NR_DRAM_BANKS > 1
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
#endif

        /* now we now the memory size, enable dcache */
        dcache_enable();
        
	return 0;
}

#ifdef BOARD_LATE_INIT
int board_late_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;
	nv_critical_t *pNvram;
	volatile uint32_t *wce_bootargs = (volatile uint32_t *)WCE_BOOTARGS_ADDR;
	char cmd[60];

	printf("CPU:   %s @ %i.%iMHz\n", CPU, CPU_CLK_FREQ/1000000 , CPU_CLK_FREQ %1000000);

#ifdef CONFIG_HAVE_SPI_LOADER
        ns9xxx_print_spi_version();
#endif  /* CONFIG_HAVE_SPI_LOADER */

#if (CONFIG_COMMANDS & CFG_CMD_BSP)
	if (NvCriticalGet(&pNvram)) {
		/* Pass location of environment in memory to OS through bi */
		gd->bd->nvram_addr = (unsigned long)pNvram;
	}
#endif
	 /* copy bdinfo to start of boot-parameter block */
 	memcpy((int*)gd->bd->bi_boot_params, gd->bd, sizeof(bd_t));

	/* Clean Windows CE bootargs signature at address WCE_BOOTARGS_ADDR
	 * This is necessary to ensure WinCE knows when is in debug mode
	 * Eboot writes the magic signature on that address */
	*wce_bootargs = 0;

#ifdef CONFIG_IEEE1588
	ieee1588_load_firmware();
#endif

#if (CONFIG_COMMANDS & CFG_CMD_NET)
        eth_use_mac_from_env( gd->bd );
#endif

#if defined(CONFIG_USER_KEY)
        if (get_gpio_stat(USER_KEY1_GPIO) == 0)
        {
                printf("\nUser Key 1 pressed\n");
                if (getenv("key1") != NULL)
                {
                        sprintf(cmd, "run key1");
                        run_command(cmd, 0);
                }

        }
        if (get_gpio_stat(USER_KEY2_GPIO) == 0)
        {
                printf("\nUser Key 2 pressed\n");
                if (getenv("key2") != NULL)
                {
                        sprintf(cmd, "run key2");
                        run_command(cmd, 0);
                }
        }
#endif /* CONFIG_USER_KEY */

#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
	run_auto_script();
#endif

	return 0;
}
#endif /*BOARD_LATE_INIT*/

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
