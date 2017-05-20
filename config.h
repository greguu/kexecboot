/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Where to look for kexec binary */
#define KEXEC_PATH "/usr/sbin/kexec"

/* Name of package */
#define PACKAGE "kexecboot"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "kexecboot-devel@linuxtogo.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "kexecboot"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "kexecboot 0.6"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "kexecboot"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.6"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Where to look for ubiattach binary */
#define UBIATTACH_PATH "/usr/sbin/ubiattach"

/* UBI VID header offset */
/* #undef UBI_VID_HDR_OFFSET */

/* Define if you want to support this bpp mode */
#define USE_16BPP 1

/* Define if you want to support this bpp mode */
#define USE_18BPP 1

/* Define if you want to support this bpp mode */
#define USE_24BPP 1

/* Define if you want to support this bpp mode */
#define USE_32BPP 1

/* Define if you want to use special buffer to hold pre-drawed background */
/* #undef USE_BG_BUFFER */

/* Define if you want to parse cfgfiles */
#define USE_CFGFILES 1

/* Define delay to sleep before scanning devices */
#define USE_DELAY 3

/* Define to enable devices re-creating */
#define USE_DEVICES_RECREATING 1

/* Define if you wish to mount devtmpfs at startup in init-mode */
#define USE_DEVTMPFS 1

/* Define evdev (keyboard/mouse) repeat rate to use in milliseconds
   (first_delay, repeat_delay) */
/* #undef USE_EVDEV_RATE */

/* Define if you use the framebuffer user interface */
#define USE_FBMENU 1

/* Define if you want to limit FB UI height to specified value */
#define USE_FBUI_HEIGHT 480

/* Define if you want to limit FB UI width to specified value */
#define USE_FBUI_WIDTH 480

/* How to change data length for RAM-to-FB transfers */
#define USE_FB_TRANS_LENGTH(x) (x >> 2)

/* Data type for RAM-to-FB transfers */
#define USE_FB_TRANS_TYPE uint32_t

/* Define if you wish to debug kexecboot right on host system */
/* #undef USE_HOST_DEBUG */

/* Define if you want to parse and use custom icons */
#define USE_ICONS 1

/* Define if you wish to enable looking for machine-specific zImage kernel */
/* #undef USE_MACHINE_KERNEL */

/* Define if you wish to allow to choose menu items by 0-9 keys */
#define USE_NUMKEYS 1

/* Define if you wish to enable text-mode UI support */
/* #undef USE_TEXTUI */

/* Define timeout in seconds to automatic boot 1st kernel after this time */
#define USE_TIMEOUT 5

/* Define if you wish to compile uImage kernel support */
/* #undef USE_UIMAGE */

/* Define if you wish to compile Sharp Zaurus specific code */
/* #undef USE_ZAURUS */

/* Define if you wish to compile zImage kernel support */
#define USE_ZIMAGE 1

/* Version number of package */
#define VERSION "0.6"
