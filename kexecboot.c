/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
 *
 *  small parts:
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

/*
 * TODO Show kexecboot version somewhere
 * TODO Create WARN/ERR/INFO functions in addition to DPRINTF
 * TODO Show debug info in special dialog
 * TODO Cleanup debug info
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <termios.h>
#include <unistd.h>
#include <sys/mount.h>
#include <ctype.h>
#include <errno.h>
#include <sys/reboot.h>
#include <asm/setup.h> // for COMMAND_LINE_SIZE
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "util.h"
#include "devicescan.h"
#include "menu.h"
#include "kexecboot.h"

/**
 * Only ARM still defines COMMAND_LINE_SIZE. In cases where COMMAND_LINE_SIZE
 * is undefined, lets set a safe value. 255 should be safe; it was what i386
 * used until 2.6.21(?.)
 *
 * FIXME: Find a better way to do this
 **/

#ifndef COMMAND_LINE_SIZE
#define COMMAND_LINE_SIZE 255
#endif

#ifdef USE_FBMENU
#include "gui.h"
#endif

/* Machine-dependent kernel patch */
char *machine_kernel = NULL;

/* NULL-terminated array of kernel search paths
 * First item should be filled with machine-dependent path */
char *default_kernels[] = {
	"/mnt/boot/zImage",
	"/mnt/zImage",
	NULL
};

/* Tags we want from /proc/cmdline */
char *wanted_tags[] = {
	"mtdparts",
	"fbcon",
	NULL
};

/* Menu/keyboard/TS actions */
enum actions_t {
	A_NONE,
	A_UP,
	A_DOWN,
	A_REFRESH,
	A_MAINMENU,
	A_SYSMENU,
	A_REBOOT,
	A_RESCAN,
	A_DEBUG,
	A_DEVICES
};


/* Return lowercased and stripped machine-specific kernel path */
/* Return value should be free()'d */
char *get_machine_kernelpath() {
	FILE *f;
	char *tmp, *hw = NULL, c;
	char line[80];

	f = fopen("/proc/cpuinfo", "r");
	if (!f) {
		perror("/proc/cpuinfo");
		exit(-1);
	}

	/* Search string 'Hardware' */
	while (fgets(line, sizeof(line), f)) {
		line[strlen(line) - 1] = '\0';
		hw = strstr(line, "Hardware");
		if (NULL != hw) break;
	}
	fclose(f);

	if ( NULL != hw) {
		/* Search colon then skip it and space after */
		hw = strchr(hw, ':');
		if (NULL == hw) {	/* Should not happens but anyway */
			DPRINTF("Can't find ':' in 'Hardware' line\n");
			return NULL;
		}
		hw += 2;	/* May be ltrim()? */

		/* Lowercase name and replace spaces with '_' */
		tmp = hw;
		while('\0' != *tmp) {
			c = *tmp;
			if (isspace(c)) {
				*tmp = '_';
			} else {
				*tmp = tolower(c);
			}
			++tmp;
		}

		/* Prepend "/mnt/boot/zImage-" to hw */
		tmp = malloc(strlen(hw) + 17 + 1);	/* strlen("/mnt/boot/zImage-") */
		if (NULL == tmp) {
			DPRINTF("Can't allocate memory for machine-specific kernel path\n");
			return NULL;
		}

		strcpy(tmp, "/mnt/boot/zImage-");
		strcat(tmp, hw);

		return tmp;
	}

	DPRINTF("Can't find 'Hardware' line in cpuinfo\n");
	return NULL;
}


/*
 * Function: get_extra_cmdline()
 * It gets wanted tags from original cmdline.
 * Takes 2 args:
 * - extra_cmdline - buffer to store cmdline parameters;
 * - extra_cmdline_size - size of buffer
 *   (inc. terminating '\0').
 * Return values:
 * - length of extra_cmdline on success (w/o term. zero);
 * - -1 on error;
 * - (- length of extra_cmdline - 1)  on insufficient buffer space.
 * REFACTOR: do something with wanted_tags and this function (may be move to util)
 */

int get_extra_cmdline(char *const extra_cmdline, const size_t extra_cmdline_size)
{
	char *p, *t, *tag = NULL;
	char line[COMMAND_LINE_SIZE];
	int i, len, sp_size;
	int sum_len = 0;
	int wanted_tag_found = 0;
	int overflow = 0;

	const char proc_cmdline_path[] = "/proc/cmdline";

	/* Open /proc/cmdline and read cmdline */
	FILE *f = fopen(proc_cmdline_path, "r");
	if (NULL == f) {
		perror("Can't open /proc/cmdline");
		return -1;
	}

	if ( NULL == fgets(line, sizeof(line), f) ) {
		perror("Can't read /proc/cmdline");
		fclose(f);
		return -1;
	}

	fclose(f);

	/* clean up buffer before parsing */
	t = extra_cmdline;
	*t = '\0';

	p = line-1;		/* because of ++p below */

	sp_size = 0;	/* size of (first) space */

	do {
		++p;

		if ( (NULL == tag) && (isalnum(*p)) ) {
			/* new tag found */
			tag = p;
		} else if (tag) {
			/* we are working on some tag */

			if (isspace(*p) || ('\0' == *p) || ('=' == *p) ) {
				/* end of tag or '=' found */

				if (!wanted_tag_found) {
					/* search in wanted_tags */
					for (i = 0; wanted_tags[i] != NULL; i++) {
						if ( 0 == strncmp(wanted_tags[i], tag, p-tag) ) {
							/* match found */
							wanted_tag_found = 1;
							break;
						}
					}
				}

				if ( ('=' != *p) && wanted_tag_found ) {
					/* end of wanted tag found -> copy */

					len = p - tag;
					if ( (sum_len + len + sp_size) < extra_cmdline_size ) {
						if (sp_size) {
							/* prepend space when have tags already */
							*t = ' ';
							++t;
							*t = '\0';
							++sum_len;
						} else {
							sp_size = sizeof(char);
						}

						/* NOTE: tag is not null-terminated so copy only
						 * len chars and terminate it directly
						 */
						strncpy(t, tag, len);
						/* move pointer to position after copied tag */
						t += len ;
						/* null-terminate */
						*t = '\0';
						/* update summary length */
						sum_len += len;
					} else {
						/* we have no space - skip this tag */
						overflow = 1;
					}

					/* reset wanted_tag_found */
					wanted_tag_found = 0;
				}

				/* reset tag */
				if (!wanted_tag_found) tag = NULL;

			}
		}

	} while ('\0' != *p);

	if (overflow) return -sum_len-1;
	return sum_len;
}

void start_kernel(struct boot_item_t *item)
{
	/* we use var[] instead of *var because sizeof(var) using */
	const char mount_point[] = "/mnt";
	const char kexec_path[] = "/usr/sbin/kexec";

	const char str_cmdline_start[] = "--command-line=";
	const char str_root[] = " root=";
	const char str_rootfstype[] = " rootfstype=";
	const char str_rootwait[] = " rootwait";

	/* empty environment */
	char *const envp[] = { NULL };

	const char *kexec_load_argv[] = { NULL, "-l", NULL, NULL, NULL };
	const char *kexec_exec_argv[] = { NULL, "-e", NULL, NULL};

	char extra_cmdline_buffer[COMMAND_LINE_SIZE];
	char *extra_cmdline, *cmdline_arg = NULL;
	int n, idx;
	struct stat *sinfo = malloc(sizeof(struct stat));

	kexec_exec_argv[0] = kexec_path;

	/* Check /proc/sys/net presence */
	if ( -1 == stat("/proc/sys/net", sinfo) ) {
		if (ENOENT == errno) {
			/* We have no network, don't issue ifdown() while kexec'ing */
			kexec_exec_argv[2] = "-x";
			DPRINTF("No network is detected, disabling ifdown()\n");
		} else {
			perror("Can't stat /proc/sys/net");
		}
	}
	free(sinfo);

	kexec_load_argv[0] = kexec_path;

	/* --command-line arg generation */
	idx = 2;	/* kexec_load_argv current option index */

	/* get some parts of cmdline from boot loader (e.g. mtdparts) */
	n = get_extra_cmdline( extra_cmdline_buffer,
			sizeof(extra_cmdline_buffer) );
	if ( -1 == n ) {
		/* clean up extra_cmdline on error */
		extra_cmdline = NULL;
/*	} else if ( n < -1 ) { */
		/* Do something when we have no space to get all wanted tags */
		/* Now do nothing ;) */
	} else {
		extra_cmdline = extra_cmdline_buffer;
	}

	/* fill '--command-line' option */
	if (item->cmdline || extra_cmdline || item->device) {
		/* allocate space */
		n = strlenn(str_cmdline_start) + strlenn(item->cmdline) +
				sizeof(char) + strlenn(extra_cmdline) +
				strlenn(str_root) + strlenn(item->device) +
				strlenn(str_rootfstype) + strlenn(item->fstype) +
				strlenn(str_rootwait) + sizeof(char);

		cmdline_arg = (char *)malloc(n);
		if (NULL == cmdline_arg) {
			perror("Can't allocate memory for cmdline_arg");
			/*  Should we exit?
			exit(-1);
			*/
		} else {
			strcpy(cmdline_arg, str_cmdline_start);
			if (extra_cmdline)
				strcat(cmdline_arg, extra_cmdline);
			if (item->cmdline && extra_cmdline)
				strcat(cmdline_arg, " ");
			if (item->cmdline)
				strcat(cmdline_arg, item->cmdline);
			if (item->device) {
				strcat(cmdline_arg, str_root);
				strcat(cmdline_arg, item->device);
				if (item->fstype) {
					strcat(cmdline_arg, str_rootfstype);
					strcat(cmdline_arg, item->fstype);
				}
			}
			strcat(cmdline_arg, str_rootwait);

			kexec_load_argv[idx] = cmdline_arg;
			++idx;
		}
	}

	/* Append kernelpath as last arg of kexec */
	kexec_load_argv[idx] = item->kernelpath;

	DPRINTF("kexec_load_argv: %s, %s, %s, %s\n", kexec_load_argv[0],
			kexec_load_argv[1], kexec_load_argv[2],
			kexec_load_argv[3]);

	DPRINTF("kexec_exec_argv: %s, %s, %s\n", kexec_exec_argv[0],
			kexec_exec_argv[1], kexec_exec_argv[2]);

	/* Mount boot device */
	if ( -1 == mount(item->device, mount_point, item->fstype,
			MS_RDONLY, NULL) ) {
		perror("Can't mount boot device");
		exit(-1);
	}

	/* Load kernel */
	n = fexecw(kexec_path, (char *const *)kexec_load_argv, envp);
	if (-1 == n) {
		perror("Kexec can't load kernel");
		exit(-1);
	}

	if (cmdline_arg)
		free(cmdline_arg);

	umount(mount_point);

	/* Boot new kernel */
	execve(kexec_path, (char *const *)kexec_exec_argv, envp);
}


int scan_devices(struct bootconf_t **bootcfg, unsigned int bpp)
{
	struct charlist *fl;
	struct bootconf_t *bootconf;
	struct device_t dev;
	struct cfgfile_t cfgdata;
	int rc;
	FILE *f;
#ifdef USE_FBMENU
	int rows;
	char **xpm_data;
	struct xpm_parsed_t *icon;
#endif

	bootconf = create_bootcfg(4);
	if (NULL == bootconf) {
		DPRINTF("Can't allocate bootconf structure\n");
		return -1;
	}

	f = devscan_open(&fl);
	if (NULL == f) {
		DPRINTF("Can't initiate device scan\n");
		return -1;
	}

	for (;;) {
		rc = devscan_next(f, fl, &dev);
		if (rc < 0) continue;	/* Error */
		if (0 == rc) break;		/* EOF */

		/* Mount device */
		if (-1 == mount(dev.device, MOUNTPOINT, dev.fstype, MS_RDONLY, NULL)) {
			perror("mount");
			goto free_device;
		}

		/* NOTE: Don't go out before umount'ing */

		/* Search boot method and return boot info */
		rc = get_bootinfo(&cfgdata);

		if (-1 == rc) {	/* Error */
			DPRINTF("Can't get device boot information\n");
			goto umount;
		}

#ifdef USE_FBMENU
		/* Load custom icon */
		icon = NULL;
		if (NULL != cfgdata.iconpath) {
			rows = xpm_load_image(&xpm_data, cfgdata.iconpath);
			if (-1 == rows) {
				DPRINTF("Can't load xpm icon %s\n", cfgdata.iconpath);
				goto umount;
			}

			icon = xpm_parse_image(xpm_data, rows, bpp);
			if (NULL == icon) {
				DPRINTF("Can't parse xpm icon %s\n", cfgdata.iconpath);
				goto umount;
			}
		}
#endif

umount:
		/* Umount device */
		if (-1 == umount(MOUNTPOINT)) {
			perror("umount");
			goto free_device;
		}

		if (-1 == rc) {	/* Error */
			goto free_device;
		}

		/* Now we have something in cfgdata */
		rc = addto_bootcfg(bootconf, &dev, &cfgdata);
		if (-1 == rc) {
			DPRINTF("Can't import config file data\n");
			goto free_device;
		}

#ifdef USE_FBMENU
		if (NULL != icon) {
			/* FIXME: associate custom icon with bootconf item */
			// associate_icon(icons, rc, icon);
		}
#endif

		continue;

free_device:
		free(dev.device);
	}

	*bootcfg = bootconf;
	return 0;
}


/* Create system menu */
struct menu_t *create_system_menu()
{
	struct menu_t *sys_menu;
	/* Initialize menu */
	sys_menu = menu_init(5);
	if (NULL == sys_menu) {
		return NULL;
	}

	menu_add_item(sys_menu, "Back", A_MAINMENU,  NULL);
	menu_add_item(sys_menu, "Reboot", A_REBOOT, NULL);
	menu_add_item(sys_menu, "Rescan", A_RESCAN, NULL);
	menu_add_item(sys_menu, "Show debug info", A_DEBUG, NULL);

	return sys_menu;
}


int main(int argc, char **argv)
{
	int choice = 0;
	int initmode = 0;
	int angle = KXB_FBANGLE;
	char *eventif = KXB_EVENTIF;
	struct bootconf_t *bl;
	int b_items;
	int i;
	struct termios old, new;
	struct charlist *evlist;
	char *label;

	struct menu_t *menu, *main_menu, *sys_menu;
#ifdef USE_FBMENU
	struct gui_t *gui;
// 	int nicons = 0;
	struct xpm_parsed_t **icons = NULL;
#endif

	DPRINTF("%s starting\n", PACKAGE_STRING);

	/* When our pid is 1 we are init-process */
	if ( 1 == getpid() ) {
		initmode = 1;

		DPRINTF("I'm the init-process!\n");

		/* extra delay for initializing slow SD/CF */
		sleep(1);

		/* Mount procfs */
		if ( -1 == mount("proc", "/proc", "proc",
				0, NULL) ) {
			perror("Can't mount procfs");
			exit(-1);
		}

		DPRINTF("Procfs mounted\n");

		FILE *f;
		/* Set up console loglevel */
		f = fopen("/proc/sys/kernel/printk", "w");
		if (NULL == f) {
			perror("/proc/sys/kernel/printk");
			exit(-1);
		}
		fputs("0 4 1 7\n", f);
		fclose(f);

		DPRINTF("Console loglevel is set\n");

	}

/* FIXME: Read fbcon tag from cmdline and fill angle
4. fbcon=rotate:<n>

This option changes the orientation angle of the console display. The
value 'n' accepts the following:

0 - normal orientation (0 degree)
1 - clockwise orientation (90 degrees)
2 - upside down orientation (180 degrees)
3 - counterclockwise orientation (270 degrees)
*/

	/* Check command-line args when not an init-process */
	if (!initmode) {
		int i = 0;
		while (++i < argc) {
			if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--angle")) {
				if (++i > argc)
					goto fail;
				angle = atoi(argv[i]);
				continue;
			}

			if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
				if (++i > argc)
					goto fail;
				eventif = argv[i];
				continue;
			}

			fail:
			fprintf(stderr,	"Usage: %s [-a|--angle <0|90|180|270>] \
				[-i|--input </dev/input/eventX>\n",
				argv[0]);
			exit(-1);
		}
	}

	DPRINTF("FB angle is %d, input device is %s\n", angle, eventif);
	DPRINTF("Going to fb mode\n");

	machine_kernel = get_machine_kernelpath();

#ifdef USE_FBMENU
	gui = gui_init(angle);
	if (NULL == gui) {
		DPRINTF("Can't initialize GUI\n");
		exit(-1);	/* FIXME dont exit while other UI exists */
	}
	scan_devices(&bl, gui->fb->bpp);
#else
	scan_devices(&bl, 0);
#endif

	if ((NULL != bl) && (bl->fill > 0)) b_items = bl->fill;
	else b_items = 0;

	/* Initialize menu */
	main_menu = menu_init(b_items + 2);
	if (NULL == main_menu) {
		exit(-1);
	}

	/* Insert system menu */
	sys_menu = create_system_menu();
	menu_add_item(main_menu, "System menu", A_SYSMENU, sys_menu);


	/* FIXME: sort bootconf items by priority field before inserting */
	label = malloc(160);
	for (i = 0; i < b_items; i++) {
		/* FIXME insert full text of label with device name, fstype and size */
		snprintf(label, 160, "%s\n%s %s %luMb",
				( NULL != bl->list[i]->label ? bl->list[i]->label : bl->list[i]->kernelpath ),
				bl->list[i]->device,
				bl->list[i]->fstype,
				bl->list[i]->blocks/1024);
		DPRINTF("+ [%s]\n", label);
		menu_add_item(main_menu, label, A_DEVICES + i, NULL);
	}
	free(label);

	menu = main_menu;

#if 1
	/* Switch cursor off. FIXME: works only when master-console is tty */
	printf("\033[?25l\n");

	// deactivate terminal input
	tcgetattr(fileno(stdin), &old);
	new = old;
	new.c_lflag &= ~ECHO;
//	new.c_cflag &=~CREAD;
	tcsetattr(fileno(stdin), TCSANOW, &new);
#endif

	/* Search for keyboard/touchscreen/mouse/joystick/etc.. */
	/* FIXME there can be no event devices (web/serial) */
	evlist = scan_event_devices();
	if (NULL == evlist) {
		DPRINTF("Can't get event devices list\n");
		exit(-1);
	}

	int *ev_fds;
	fd_set fds0, fds;
	int nready, maxfd, nev, efd;

	/* Open event devices */
	ev_fds = open_event_devices(evlist);
	if (NULL == ev_fds) {
		DPRINTF("Can't open event devices\n");
		exit(-1);
	}

	nev = evlist->fill;
	maxfd = -1;

	/* Fill FS set with descriptors and search maximum fd number */
	FD_ZERO(&fds0);
	for (i = 0; i < nev; i++) {
		efd = ev_fds[i];
		if (efd > 0) {
			FD_SET(efd, &fds0);
			if (efd > maxfd) maxfd = efd;	/* Find maximum fd no */
		}
	}
	++maxfd;	/* Increase maxfd according to select() manual */

	struct input_event evt;
	int is_selected = 0;
	int action = A_REFRESH;

	/* Event loop */
	/* FIXME: events are repeating too fast (e.g. up/down) */
	do {
#ifdef USE_FBMENU
		if (action != A_NONE) gui_show_menu(gui, menu, choice, icons);
#endif
		action = A_NONE;
		fds = fds0;

		/* Wait for some input */
		nready = select(maxfd, &fds, NULL, NULL, NULL);	/* Wait indefinitely */

		if (-1 == nready) {
			if (errno == EINTR) continue;
			else {
				perror("select");
				exit(-1);
			}
		}

		/* Check fds */
		for(i = 0; i < nev; i++) {
			efd = ev_fds[i];
			if (FD_ISSET(efd, &fds)) {
				read(efd, &evt, sizeof(evt));	/* Read event */
				DPRINTF("+ event on %d, type: %d, code: %d, value: %d\n",
						efd, evt.type, evt.code, evt.value);
				if ((EV_KEY == evt.type) && (0 != evt.value)) {
					/* EV_KEY event actions */

					switch (evt.code) {
					case KEY_UP:
						action = A_UP;
						break;
					case KEY_DOWN:
					case BTN_TOUCH:	/* GTA02: touchscreen touch (330) */
						action = A_DOWN;
						break;
					case KEY_R:
						action = A_REBOOT;
						break;
					case KEY_S:	/* reScan */
						action = A_RESCAN;
						break;
					case KEY_ENTER:
					case KEY_SPACE:
					case KEY_HIRAGANA:	/* Zaurus SL-6000 */
					case KEY_HENKAN:	/* Zaurus SL-6000 */
					case 87:			/* Zaurus: OK (remove?) */
					case 63:			/* Zaurus: Enter (remove?) */
					case KEY_POWER:		/* GTA02: Power (116) */
					case KEY_PHONE:		/* GTA02: AUX (169) */
						action = menu->list[choice]->tag;
						break;
					default:
						break;
					}
#if 0
				} else if ((EV_ABS == evt.type) && (0 != evt.value)) {
					/* EV_KEY event actions */
					suitable_event = 1;
					switch (evt.code) {
					case ABS_PRESSURE:	/* Touchscreen touch */
						if (choice < (bl->fill - 1)) choice++;
						else choice = 0;
						break;
					default:
						suitable_event = 0;
					}
#endif
				}
			}
		}

		switch (action) {
		case A_NONE:
			break;
		case A_UP:
			if (choice > 0) --choice;
			else choice = menu->fill - 1;
			break;
		case A_DOWN:
			if (choice < (menu->fill - 1)) ++choice;
			else choice = 0;
			break;
		case A_SYSMENU:
			menu = sys_menu;
			break;
		case A_MAINMENU:
			menu = main_menu;
			break;
		case A_REBOOT:
			gui_show_text(gui, "Rebooting...");
			sync();
			/* if ( -1 == reboot(LINUX_REBOOT_CMD_RESTART) ) { */
			if ( -1 == reboot(RB_AUTOBOOT) ) {
				perror("Can't initiate reboot");
			}
			break;
		case A_RESCAN:
			gui_show_text(gui, "Rescanning devices.\nPlease wait...");
			free_bootcfg(bl);
			menu_destroy(main_menu);

			/* FIXME Should be separate function */
#ifdef USE_FBMENU
			scan_devices(&bl, gui->fb->bpp);
#else
			scan_devices(&bl, 0);
#endif

			if ((NULL != bl) && (bl->fill > 0)) b_items = bl->fill;
			else b_items = 0;

			/* Initialize menu */
			main_menu = menu_init(b_items + 2);
			if (NULL == main_menu) {
				exit(-1);
			}

			/* Insert system menu */
			menu_add_item(main_menu, "System menu", A_SYSMENU, sys_menu);

			/* FIXME: sort bootconf items by priority field before inserting */
			label = malloc(160);
			for (i = 0; i < b_items; i++) {
				/* FIXME insert full text of label with device name, fstype and size */
				snprintf(label, 160, "%s\n%s %s %luMb",
						( NULL != bl->list[i]->label ? bl->list[i]->label : bl->list[i]->kernelpath ),
						bl->list[i]->device,
						bl->list[i]->fstype,
						bl->list[i]->blocks/1024);
				DPRINTF("+ [%s]\n", label);
				menu_add_item(main_menu, label, A_DEVICES + i, NULL);
			}
			free(label);

			menu = main_menu;
			choice = 0;

			break;
		case A_DEBUG:
			gui_show_text(gui, "Debug info dialog is not implemented yet...");
			sleep(1);
			break;
		default:
			if ( (action >= A_DEVICES) && (b_items > 0) ) is_selected = 1;
			break;
		}

	} while (!is_selected);

	// reset terminal
	tcsetattr(fileno(stdin), TCSANOW, &old);

	gui_destroy(gui);
// 	free_associated_icons(icons, nicons);
	close_event_devices(ev_fds, evlist->fill);
	free_charlist(evlist);

// 	start_kernel(bl->list[choice]);
	start_kernel(bl->list[action - A_DEVICES]);
	/* When we reach this point then some error has occured */
	DPRINTF("We should not reach this point!");
	return -1;
}
