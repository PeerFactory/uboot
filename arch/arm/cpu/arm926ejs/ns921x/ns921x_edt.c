/*
 * Copyright (C) 2009 by Digi International Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <config.h>
#include <common.h>
#include <lcd.h>
#include <asm-arm/arch-ns9xxx/ns921x_gpio.h>
#include <asm-arm/arch-ns9xxx/io.h>
#include <asm-arm/arch-ns9xxx/ns921x_sys.h>
#include <asm-arm/arch-ns9xxx/ns9xxx_mem.h>
#include <partition.h>
#include <nvram.h>
#include <linux/mtd/compat.h>
#if  (CONFIG_COMMANDS & CFG_CMD_BSP && defined(CONFIG_DIGI_CMD))
# include "../../../common/digi/cmd_bsp.h"
#endif

#define EDTQVGA_DISPLAY              \
{                                    \
       .name           = "EDTQVGA",  \
       .vl_col         = 320,        \
       .vl_row         = 240,        \
       .vl_bpix        = LCD_BPP     \
}

#ifdef CONFIG_LCD

#define LCD_CS_OFFSET	0x40000000
#define	LCD_POINTER	LCD_CS_OFFSET
#define LCD_DATA	(LCD_CS_OFFSET + 2)
#define GPIO_RESET_LCD	86
#define GPIO_ENABLE_LCD	87

struct vidinfo panel_info = EDTQVGA_DISPLAY;

/* Externally used variables */
void *lcd_base;				/* Start of framebuffer memory	*/
void *lcd_console_address;		/* Start of console buffer	*/
short console_col;
short console_row;
int lcd_line_length;
int lcd_color_fg;
int lcd_color_bg;
int display_initialized = 0;

int lcd_display_init(void);
void lcd_ctrl_init( void *lcdbase );
void lcd_enable( void );
static int edtdisplay_init_mem( void *lcdbase );
static int edtdisplay_init( void *lcdbase );

DECLARE_GLOBAL_DATA_PTR;

static void write_lcd_reg(uint addr, uint data)
{
	writew(addr, LCD_POINTER);
	writew(data, LCD_DATA);
}

static void mdelay(int count)
{
	for(;count != 0;count--)
		udelay(1000);
}

int lcd_display_init(void)
{
	display_initialized = 1;
	return 0;
}

void config_cs(void)
{
	int cs = 0;
	unsigned int reg;

	/* CS configuration */
	writel(0x3,   MEM_BASE_PA + MEM_STAT_EXT_WAIT);
	writel(0x181, MEM_BASE_PA + MEM_STAT_CFG(cs));
	writel(0x4,   MEM_BASE_PA + MEM_STAT_WAIT_WEN(cs));
	writel(0x0,   MEM_BASE_PA + MEM_STAT_WAIT_OEN(cs));
	writel(0x0,   MEM_BASE_PA + MEM_STAT_RD(cs));
	writel(0x0,   MEM_BASE_PA + MEM_STAT_PAGE(cs));
	writel(0x0,   MEM_BASE_PA + MEM_STAT_WR(cs));
	writel(0x0,   MEM_BASE_PA + MEM_STAT_TURN(cs));
	/* Enable CS */
	reg = readl(SYS_BASE_PA + SYS_CS_STATIC_BASE(cs));
	writel(reg | 0x1, SYS_BASE_PA + SYS_CS_STATIC_BASE(cs));
}

void lcd_reset(void)
{
	gpio_cfg_set(GPIO_RESET_LCD,
		     GPIO_CFG_FUNC_GPIO | GPIO_CFG_OUTPUT);
	gpio_ctrl_set(GPIO_RESET_LCD, 0);
	mdelay(100);
	gpio_ctrl_set(GPIO_RESET_LCD, 1);
}

/* Configuration for the EDT QVGA display, most of the settings have
 * been taken from a Himax application note */
unsigned char edt_qvga_lcd_init[][3] = {
	/* Index, value, delay to write next register in ms*/
	{0x46, 0x94, 0},
	{0x47, 0x41, 0},
	{0x48, 0x00, 0},
	{0x49, 0x33, 0},
	{0x4a, 0x23, 0},
	{0x4b, 0x45, 0},
	{0x4c, 0x44, 0},
	{0x4d, 0x77, 0},
	{0x4e, 0x12, 0},
	{0x4f, 0xcc, 0},
	{0x50, 0x46, 0},
	{0x51, 0x82, 0},
	{0x02, 0x00, 0},	/* Column address start 2 */
	{0x03, 0x00, 0},	/* Column address start 1 */
	{0x04, 0x01, 0},	/* Column address end 2 */
	{0x05, 0x3f, 0},	/* Column address end 1 */
	{0x06, 0x00, 0},	/* Row address start 2 */
	{0x07, 0x00, 0},	/* Row address start 1 */
	{0x08, 0x00, 0},	/* Row address end 2 */
	{0x09, 0xef, 0},	/* Row address end 1 */
	{0x01, 0x06, 0},
	{0x16, 0x68, 0},
	{0x23, 0x95, 0},
	{0x24, 0x95, 0},
	{0x25, 0xff, 0},
	{0x27, 0x02, 0},
	{0x28, 0x02, 0},
	{0x29, 0x02, 0},
	{0x2a, 0x02, 0},
	{0x2c, 0x02, 0},
	{0x2d, 0x02, 0},
	{0x3a, 0x01, 0},
	{0x3b, 0x01, 0},
	{0x3c, 0xf0, 0},
	{0x3d, 0x00, 20},
	{0x35, 0x38, 0},
	{0x36, 0x78, 0},
	{0x3e, 0x38, 0},
	{0x40, 0x0f, 0},
	{0x41, 0xf0, 0},
	{0x19, 0x49, 0},
	{0x93, 0x0f, 10},
	{0x20, 0x40, 0},
	{0x1d, 0x07, 0},
	{0x1e, 0x00, 0},
	{0x1f, 0x04, 0},
	{0x44, 0x40, 0},
	{0x45, 0x12, 10},
	{0x1c, 0x04, 20},
	{0x43, 0x80, 5},
	{0x1b, 0x08, 40},
	{0x1b, 0x10, 40},
	{0x90, 0x7f, 0},
	{0x26, 0x04, 40},
	{0x26, 0x24, 0},
	{0x26, 0x2c, 40},
	{0x26, 0x3c, 0},
	{0x57, 0x02, 0},
	{0x55, 0x00, 0},
	{0x57, 0x00, 0}
};

void edt_init(void)
{
	int i;

	for (i=0; i < (sizeof(edt_qvga_lcd_init)/3); i++) {

		write_lcd_reg(edt_qvga_lcd_init[i][0],
			      edt_qvga_lcd_init[i][1]);
		mdelay(edt_qvga_lcd_init[i][2]);
	}
	/* Prepare to print data */
	writew(0x22, LCD_POINTER);
}

void lcd_disable(void)
{
	gpio_ctrl_set(GPIO_ENABLE_LCD, 0);
}

void lcd_enable(void)
{
	gpio_cfg_set(GPIO_ENABLE_LCD,
		     GPIO_CFG_FUNC_GPIO | GPIO_CFG_OUTPUT);
	gpio_ctrl_set(GPIO_ENABLE_LCD, 1);
}

void lcd_ctrl_init (void *lcdbase)
{
        const nv_param_part_t *part_entry;

	if (NvParamPartFind(&part_entry, NVPT_SPLASH_SCREEN, NVFS_NONE, 0, 0)) {
		edtdisplay_init_mem(lcdbase);
		edtdisplay_init(lcdbase);
		lcd_disable();
		return;
	}
	gd->bd->fb_base = 0xffffffff;
}

ulong calc_fbsize (void)
{
	ulong size;
	int line_length = (panel_info.vl_col * NBITS (panel_info.vl_bpix)) / 8;

	size = line_length * panel_info.vl_row;
	size += PAGE_SIZE;

	return size;
}

static int edtdisplay_init_mem (void *lcdbase)
{
	u_long palette_mem_size;
	int fb_size = panel_info.vl_row * (panel_info.vl_col * NBITS (panel_info.vl_bpix)) / 8;

	panel_info.screen = (u_long)lcdbase;

	panel_info.palette_size = NBITS(panel_info.vl_bpix) == 8 ? 256 : 16;
	palette_mem_size = panel_info.palette_size * sizeof(u16);

	debug("palette_mem_size = 0x%08lx\n", (u_long) palette_mem_size);
	/* locate palette and descs at end of page following fb */
	panel_info.palette = (u_long)lcdbase + fb_size + PAGE_SIZE - palette_mem_size;

	return 0;
}

static int edtdisplay_init(void *lcdbase)
{
	lcd_enable();
	lcd_reset();
	config_cs();

	mdelay(50);
	writeb(0x67, LCD_POINTER);
	if (readb(LCD_DATA) != 0x47) {
		printf("error: HX8347 controller not detected\n");
	}

	edt_init();
	/* Disable to avoid displaying anything on the
	 * screen until we have the splash image */
	lcd_disable();

	return 0;
}

void lcd_write_pixel(unsigned short val)
{
	writew(val, LCD_DATA);
}

#endif /* CONFIG_LCD */
