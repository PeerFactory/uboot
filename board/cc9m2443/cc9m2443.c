/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
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

#include <common.h>
#include <regs.h>
#include <nvram.h>
#include <net.h>

#include <asm/io.h>
#include <asm-arm/arch-s3c24xx/gpio.h>
DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_AUTOLOAD_BOOTSCRIPT
extern void run_auto_script(void);
#endif

/* ------------------------------------------------------------------------- */
static inline void delay(unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n" "bne 1b":"=r" (loops):"0"(loops));
}

/*
 * Miscellaneous platform dependent initialisations
 */

static void lan9215_pre_init(void)
{

	s3c_gpio_cfgpin(S3C_GPA13, S3C_GPA_OUTPUT);
	s3c_gpio_setpin(S3C_GPA13, 1);

	SMBIDCYR5_REG = 0xa;			//Bank1 Idle cycle ctrl.
	SMBWSTWRR5_REG = 14;			//Bank1 Write Wait State ctrl.
	SMBWSTOENR5_REG = 0x7;			//Bank1 Output Enable Assertion Delay ctrl.
	SMBWSTWENR5_REG = 0x7;			//Bank1 Write Enable Assertion Delay ctrl.
	SMBWSTRDR5_REG = 14;			//Bank1 Read Wait State cont. = 14 clk
	SMBCR5_REG |=  (1<<0);			//SMWAIT active High, Read Byte Lane Enable
	SMBCR5_REG |= ((3<<20)|(3<<12));	//SMADDRVALID = always High when Read/Write
	SMBCR5_REG &= ~(3<<4);			//Clear Memory Width
	SMBCR5_REG |=  (1<<4);			//Memory Width = 16bit
}

static void usb_pre_init (void)
{
	/* Initialy disable USB_POWEREN line to
	 * reset any connected USB device.
	 * This line will be enabled when booting
	 * the OS or when calling a USB command */
	s3c_gpio_cfgpin(S3C_GPA14, S3C_GPA_OUTPUT);
	s3c_gpio_setpin(S3C_GPA14, 1);

	CLKDIV1CON_REG |= 1<<4;

	USB_RSTCON_REG = 0x1;
	delay(500);
	USB_RSTCON_REG = 0x2;
	delay(500);
	USB_RSTCON_REG = 0x0;
	delay(500);

	USB_CLKCON_REG |= 0x2;

	/*
	 * Enable the USB-PHY and reset it, otherwise a connected USB-host will
	 * try to enumerate the non-available USB-device
	 */
	PWRCFG_REG |= 0x10;
	USB_RSTCON_REG |= 0x01;
}

/*
 * When NAND is not used as Boot Device
 */
static void nand_pre_init(void)
{
	SMBIDCYR1_REG = 0xf;
	SMBWSTRDR1_REG = 0x1f;
	SMBWSTWRR1_REG = 0x1f;
	SMBWSTOENR1_REG = 0;
	SMBWSTWENR1_REG = 1;
	SMBCR1_REG = 0x00303000;
	SMBSR1_REG = 0;
	SMBWSTBDR1_REG = 0x1f;
}

static void inline user_key_init(void)
{
	s3c_gpio_cfgpin(USER_KEY1_GPIO, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(USER_KEY2_GPIO, S3C_GPIO_INPUT);
}

static void inline user_led_init(void)
{
	s3c_gpio_cfgpin(USER_LED1_GPIO, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgpin(USER_LED2_GPIO, S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(USER_LED1_GPIO, 0);
	s3c_gpio_setpin(USER_LED2_GPIO, 0);
}

static void inline confiure_S4(void)
{
	s3c_gpio_cfgpin(S3C_GPC8, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(S3C_GPC9, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(S3C_GPD0, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(S3C_GPD1, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(S3C_GPD8, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(S3C_GPD9, S3C_GPIO_INPUT);
}

static void inline disable_wlan(void)
{
	GPBCON_REG &= ~(3<<4);
        GPBCON_REG |=  (1<<4);
}

int board_init(void)
{
	icache_enable();
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
	nand_pre_init();
#endif
#ifdef CONFIG_DRIVER_SMSC9118
	 lan9215_pre_init();
#endif
#ifdef CONFIG_USB_OHCI
	usb_pre_init();
#endif
#if defined(CONFIG_USER_KEY)
 	user_key_init();
#endif
	user_led_init();

#if defined(CONFIG_SILENT_CONSOLE) && defined(ENABLE_CONSOLE_GPIO)
	s3c_gpio_cfgpin( ENABLE_CONSOLE_GPIO, S3C_GPIO_INPUT);
#endif

	disable_wlan();
	gd->bd->bi_arch_number = MACH_TYPE_CC9M2443JS;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1+0x100);

	return 0;
}

ulong get_dram_size (void)
{
	ulong dram_size = 0;
	ulong gpdat = 0;

	s3c_gpio_cfgpin(S3C_GPF4, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(S3C_GPF3, S3C_GPIO_INPUT);
	gpdat  = s3c_gpio_getpin(S3C_GPF3);
	gpdat |= s3c_gpio_getpin(S3C_GPF4);
	gpdat  = gpdat >> 3;

	switch (gpdat) {
		case 0x00000000:
			dram_size = 0x1000000;
			break;
		case 0x000000001:
			dram_size = 0x4000000;
			break;
		case 0x00000002:
			dram_size = 0x2000000;
			break;
		case 0x00000003:
			dram_size = 0x8000000;
			break;
	}

	return dram_size;
}

/* Writes a magic number to offset 0 of each DRAM bank address
 * and verifies that the value read is the value written. If so,
 * the bank exists. Different magic numbers are used for each bank
 * as some SDRAM controllers mirror the memory from the first bank
 * to the second if the second doesn't exist. */
int get_dram_banks(void)
{
	ulong magic1 = 0xA9871111;
	ulong magic2 = 0xFEDC2222;
	ulong val, offset = 0;
	int banks = 0;

	writel(magic1, PHYS_SDRAM_1 + offset);
	writel(magic2, PHYS_SDRAM_2 + offset);
	val = readl(PHYS_SDRAM_1 + offset);
	if (val == magic1)
		banks++;
	val = readl(PHYS_SDRAM_2 + offset);
	if (val == magic2)
		banks++;

	return banks;
}

int dram_init(void)
{
	ulong size;
	int nrbanks;

	size = get_dram_size();
	nrbanks = get_dram_banks();
	if (nrbanks != 1 && nrbanks != 2)
		nrbanks = 1;

	if (nrbanks > 0) {
		gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
		gd->bd->bi_dram[0].size = size;
#if CONFIG_NR_DRAM_BANKS == 2
		/* Fill the information of the second bank
		 * with correct address but size 0, for compatibility */
		gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
		gd->bd->bi_dram[1].size =  0;
#endif
	}
	if (nrbanks > 1) {
		gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
		gd->bd->bi_dram[1].size = size;
	}

	dcache_enable();

	return 0;
}

#if !defined(CONFIG_SPLASH_SCREEN) && !defined(CONFIG_USE_VIDEO_CONSOLE)
static void SetLcdPort(void)
{
	ulong gpdat;
	/* Enable clock to LCD */
	HCLKCON_REG |= (1<<10);

	/* To select TFT LCD type */
	gpdat = readl(MISCCR);
	gpdat  |= (1<<28);
	writel(gpdat, MISCCR);

	GPCCON_REG = 0x55505554;
	GPDCON_REG = 0x55505550;

#ifndef	CONFIG_UBOOT_CRT_VGA
	/* disalbe LCD_POWER_ENABLE */
	GPGCON_REG &= ~(3<<8);
	GPGCON_REG |=  (1<<8);
	GPGDAT_REG |=  (1<<4);
#endif
}
#endif

#ifdef BOARD_LATE_INIT
int board_late_init (void)
{
	char cmd[60];
	nv_critical_t *pNvram;
	printf("CPU:   S3C2443@%dMHz\n", get_ARMCLK()/1000000);
	printf("       Fclk = %dMHz, Hclk = %dMHz, Pclk = %dMHz\n",
			get_FCLK()/1000000, get_HCLK()/1000000, get_PCLK()/1000000);

#if !defined(CONFIG_SPLASH_SCREEN) && !defined(CONFIG_USE_VIDEO_CONSOLE)
	SetLcdPort();
#endif

	if (NvCriticalGet(&pNvram)) {
		/* Pass location of environment in memory to OS
		 *                  * through bi */
		gd->bd->nvram_addr = (unsigned long)pNvram;
	}
	memcpy((int*)gd->bd->bi_boot_params, gd->bd, sizeof(bd_t));

	/*
	 * Store the MAC-address in the Ethernet-controller, otherwise the
	 * kernel-driver will not have the correct address.
	 */
#if defined(CONFIG_COMMANDS) && defined(CFG_CMD_NET)
	eth_use_mac_from_env( gd->bd );
#endif

#if defined(CONFIG_USER_KEY)
        if (s3c_gpio_get_stat(USER_KEY1_GPIO) == 0)
        {
                printf("\nUser Key 1 pressed\n");
                if (getenv("key1") != NULL)
                {
                        sprintf(cmd, "run key1");
                        run_command(cmd, 0);
                }

        }
        if (s3c_gpio_get_stat(USER_KEY2_GPIO) == 0)
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
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	vu_long *mem_reg = (vu_long*) 0x48000000;

	printf("Board: CC9M2443 ");
	switch ((*mem_reg>>2) & 0x3) {
	case 0:
		puts("SDRAM\n");
		break;

	case 1:
		puts("Mobile SDRAM\n");
		break;

	case 2:
	case 3:
		puts("Mobile DDR\n");
		break;

	default:
		puts("unknown Memory Type\n");
	}
	return (0);
}
#endif

#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_smdk2443(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xc4000000))
		return (addr - 0xc0000000 + 0x30000000);
	else
		printf("do not support this address : %08lx\n", addr);

	return addr;
}
#endif

#if (CONFIG_COMMANDS & CFG_CMD_NAND) && defined(CFG_NAND_LEGACY)
#include <linux/mtd/nand.h>
extern struct nand_chip nand_dev_desc[CFG_MAX_NAND_DEVICE];

void nand_init(void)
{
	nand_probe(CFG_NAND_BASE);
	if (nand_dev_desc[0].ChipID != NAND_ChipID_UNKNOWN) {
		print_size(nand_dev_desc[0].totlen, "\n");
	}
}
#endif

#if defined(CONFIG_SILENT_CONSOLE)

int test_console_gpio (void)
{
#if defined(ENABLE_CONSOLE_GPIO) && defined(CONSOLE_ENABLE_GPIO_STATE)
	if( s3c_gpio_get_stat(ENABLE_CONSOLE_GPIO) == CONSOLE_ENABLE_GPIO_STATE){
		return 1;
	}else{
		return 0;
	}
#else

	return 0;
#endif
}
#endif
