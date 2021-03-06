/*
 * U-Boot Configuration
 */
#define CONFIG_SHOW_HELP 1
#define CONFIG_UBOOT_SETTINGS 1
#define CONFIG_DIGIEL_USERCONFIG 1
#define CONFIG_HAVE_MII 1
#undef CONFIG_HAVE_MMC
#undef CONFIG_HAVE_FPGA
#define CONFIG_HAVE_INTERN_CLK 1
#undef CONFIG_HAVE_EXTERN_CLK
#undef CONFIG_HAVE_NAND
#define CONFIG_HAVE_NOR 1
#undef CONFIG_HAVE_MAC_IN_EEPROM
#define CONFIG_HAVE_LCD 1
#define CONFIG_HAVE_ETHERNET 1
#undef CONFIG_HAVE_EEPROM
#undef CONFIG_HAVE_USB
#define CONFIG_HAVE_I2C 1
#undef CONFIG_HAVE_SPI
#define CONFIG_UBOOT_NVRAM_UNCHANGEABLE 1

/*
 * Commands
 */

/*
 * Network
 */
#define CONFIG_CMD_MII 1
#define CONFIG_CMD_NET 1
#define CONFIG_CMD_DHCP 1
#define CONFIG_CMD_PING 1
#define CONFIG_CMD_SNTP 1
#define CONFIG_CMD_NFS 1

/*
 * Serial, console and environment
 */
#define CONFIG_CMD_LOADS 1
#define CONFIG_CMD_LOADB 1
#define CONFIG_CMD_CONSOLE 1
#define CONFIG_CMD_RUN 1
#define CONFIG_CMD_ECHO 1
#define CONFIG_CMD_ENV 1

/*
 * Memory
 */
#define CONFIG_CMD_CACHE 1
#define CONFIG_CMD_MEMORY 1

/*
 * Storage
 */
#define CONFIG_CMD_FLASH 1

/*
 * Removable storage devices
 */

/*
 * Data bus
 */
#define CONFIG_CMD_I2C 1

/*
 * Filesystem
 */
#undef CONFIG_CMD_FAT
#undef CONFIG_CMD_EXT2

/*
 * Debug and information
 */
#define CONFIG_CMD_BDI 1

/*
 * Image tools
 */
#define CONFIG_CMD_IMI 1
#define CONFIG_CMD_IMLS 1

/*
 * Board specific commands
 */
#define CONFIG_CMD_DATE 1
#define CONFIG_CMD_BSP 1

/*
 * Boot commands
 */
#define CONFIG_CMD_BOOTD 1
#define CONFIG_CMD_AUTOSCRIPT 1
#define CONFIG_AUTOLOAD_BOOTSCRIPT 1
#undef CONFIG_CMD_ELF
#undef CONFIG_UBOOT_FIMS_SUPPORT
#undef CONFIG_DISPLAY_SELECTED

/*
 * Misc commands
 */
#undef CONFIG_UBOOT_CMD_BSP_TESTHW
#define CONFIG_CMD_MISC 1
#define CONFIG_CMD_ITEST 1
#undef CONFIG_PARTITION
#undef CONFIG_UBOOT_PROMPT
#undef CONFIG_UBOOT_BOARDNAME
#undef CONFIG_NS9360_CONSOLE_PORT_A
#undef CONFIG_NS9360_CONSOLE_PORT_B
#undef CONFIG_NS9360_CONSOLE_PORT_C
#undef CONFIG_NS9360_CONSOLE_PORT_D
#undef CONFIG_NS9215_CONSOLE_PORT_A
#define CONFIG_NS9215_CONSOLE_PORT_B 1
#undef CONFIG_NS9215_CONSOLE_PORT_C
#undef CONFIG_NS9215_CONSOLE_PORT_D
#undef CONFIG_NS9210_CONSOLE_PORT_A
#undef CONFIG_NS9210_CONSOLE_PORT_C
#undef CONFIG_S3C2443_CONSOLE_PORT_A
#undef CONFIG_S3C2443_CONSOLE_PORT_B
#undef CONFIG_S3C2443_CONSOLE_PORT_C
#undef CONFIG_S3C2443_CONSOLE_PORT_D
#undef CONFIG_UBOOT_JTAG_CONSOLE
#undef CONFIG_SILENT_CONSOLE
#define CONFIG_BOOTDELAY 4
#undef CONFIG_UBOOT_BOOTFILE
#define CONFIG_UBOOT_IMAGE_NAME "u-boot-cc9p9215js.bin"
#undef CONFIG_UBOOT_DISABLE_USER_KEYS
#define CONFIG_UBOOT_CHECK_CRC32_ON_BOOT 1
#define CONFIG_UBOOT_VERIFY_IN_SDRAM 1
#define CONFIG_CONS_INDEX 1
#define CONFIG_IMAGE_JFFS2_BASENAME "rootfs-cc9p9215js"
#define CONFIG_LINUX_IMAGE_NAME "uImage-cc9p9215js"
