/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2011 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
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

#ifndef _HAVE_FB_H
#define _HAVE_FB_H

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "util.h"
#include "res/fonts/font.h"
#include "rgb.h"

typedef struct FB *FBPTR;

typedef void (*plot_pixel_func)(FBPTR fb, int x, int y,
		kx_ccomp red, kx_ccomp green, kx_ccomp blue);

typedef void (*draw_hline_func)(FBPTR fb, int x, int y, int length,
		kx_ccomp red, kx_ccomp green, kx_ccomp blue);

typedef struct FB {
	int fd;
	int type;
	int visual;
	int width, height;
	int bpp;
	int depth;		/* Color depth to enable 18bpp mode */
	int byte_pp;	/* Byte per pixel, 0 for bpp < 8 */
	int stride;
	char *data;
	char *backbuffer;
	char *base;

	int screensize;
	int angle;
	int real_width, real_height;

	enum RGBMode rgbmode;
	int red_offset;
	int red_length;
	int green_offset;
	int green_length;
	int blue_offset;
	int blue_length;

	plot_pixel_func plot_pixel;
	draw_hline_func draw_hline;
} FB;

/* Picture structure */
/* FIXME: store pixels as colors triplets per uint32_t value */
typedef struct {
	unsigned int width;		/* picture width */
	unsigned int height;	/* picture height */
	kx_rgba *pixels;		/* RGBA array */
} kx_picture;


void fb_destroy(FB * fb);

FB *fb_new(int angle);

#ifdef DEBUG
void print_fb(FB *fb);
#endif

void
fb_draw_rect(FB * fb, int x, int y,
		int width, int height, kx_rgba rgba);

void
fb_draw_rounded_rect(FB * fb, int x, int y,
		int width, int height, kx_rgba rgba);


/* Return text width and height in pixels. Will return 0,0 for empty text */
void
fb_text_size(FB * fb, int *width, int *height,
		const Font * font, const char *text);

int
fb_draw_constrained_text(FB * fb, int x, int y,
		int max_x, int max_y, kx_rgba rgba,
		const Font * font, const char *text);

void
fb_draw_text(FB * fb, int x, int y, kx_rgba rgba,
		const Font * font, const char *text);

/* Move backbuffer contents to videomemory */
void fb_render(FB * fb);

/* Save backbuffer contents to further usage */
char *fb_dump(FB * fb);

/* Restore saved backbuffer */
void fb_restore(FB * fb, char *dump);

/* Draw picture on framebuffer */
void fb_draw_picture(FB * fb, int x, int y, kx_picture *pic);

/* Free picture's data structure */
void fb_destroy_picture(kx_picture *pic);

#endif	/* _HAVE_FB_H */
