#include <common.h>
#if (CONFIG_COMMANDS & CFG_CMD_BSP && \
		defined(CONFIG_UBOOT_CMD_BSP_TESTHW) &&    \
		(defined(CONFIG_CC9M2443) || defined(CONFIG_CCW9M2443))) && \
		defined(CONFIG_LCD)
DECLARE_GLOBAL_DATA_PTR;
#include <cmd_testhw/testhw.h>
#include <regs.h>

#define LOADADDR  0x30200000 /*same like loadaddr */
extern void lcd_enable( void );

static int do_testhw_lcd( int argc, char* argv[] )
{
	const char szCmd [60];
	int ret, x, y, x2, y2;
	#ifndef CONFIG_SPLASH_SCREEN
	extern void lcd_ctrl_init (void *lcdbase);
	extern void lcd_enable (void);
	#endif

	if( argc != 1 ) {
		eprintf( "Usage: lcd image_ name \n" );
		goto error;
	}
	sprintf((char *)szCmd, "tftp 0x%x %s", LOADADDR, argv[ 0 ] );
	ret = (run_command( szCmd, 0 ) >=  0);

	/* The test image is 320x240 and must be horizontally
	 * and vertically flipped */
	u16 data;
	u16 *pdata = (u16 *)LOADADDR;
	u16 *plcd;
	for (y = 239, y2 = 0; y >= 0; y--, y2++) {
		for (x = 319, x2 = 0; x >= 0; x--, x2++) {
			data = *(pdata + (y * 320) + x);
			plcd = (u16 *)(gd->fb_base) + (y2 * 320) + x2;
			*plcd = data;
		}
	}

	if(!ret)
		goto error;

#ifndef CONFIG_SPLASH_SCREEN
	lcd_ctrl_init((void *) gd->fb_base);
#endif
	lcd_enable();
	return 1;
error:
	return 0;
}
/* ********** Test command implemented ********** */

TESTHW_CMD( lcd, "loads given image via tftp into framebuffer.");
#endif
