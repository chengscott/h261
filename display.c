/* display.c, X11 interface                                                 */

/*
 * tmndecode 
 * Copyright (C) 1995 Telenor R&D
 *                    Karl Olav Lillevold <kol@nta.no>                    
 *
 * based on mpeg2decode, (C) 1994, MPEG Software Simulation Group
 * and mpeg2play, (C) 1994 Stefan Eckart
 *                         <stefan@lis.e-technik.tu-muenchen.de>
 *
 */

 /* the Xlib interface is closely modeled after
  * mpeg_play 2.0 by the Berkeley Plateau Research Group
  */

#include "globals.h"

#ifdef X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>

extern boolean init_display(char *name);
extern void init_dither(void);
extern void dither(FSTORE *fs);

/* private prototypes */
static void display_image(XImage *ximage, unsigned char *dithered_image);
static void ditherframe(FSTORE *fs);
static void interpolate_image(FSTORE *fs_in, FSTORE *fs_out);

/**************************************/
/* common variables */
extern IMAGE *Image;
extern boolean expand;	/* set it in h261.c */

static int convmat[8][4] =
{
  {117504, 138453, 13954, 34903}, /* no sequence_display_extension */
  {117504, 138453, 13954, 34903}, /* ITU-R Rec. 709 (1990) */
  {104597, 132201, 25675, 53279}, /* unspecified */
  {104597, 132201, 25675, 53279}, /* reserved */
  {104448, 132798, 24759, 53109}, /* FCC */
  {104597, 132201, 25675, 53279}, /* ITU-R Rec. 624-4 System B, G */
  {104597, 132201, 25675, 53279}, /* SMPTE 170M */
  {117579, 136230, 16907, 35559}  /* SMPTE 240M (1987) */
};
static int matrix_coefficients = 5;

/* local data */
static unsigned char *dithered_image;

static unsigned char ytab[16*(256+16)];
static unsigned char uvtab[256*269+270];

/* X11 related variables */
static Display *display;
static Window window;
static GC gc;
static XImage *ximage;
static unsigned char pixel[256];

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
boolean init_display(char *name)
{
  unsigned char *clp, clp_tab[1024];
  int crv, cbu, cgu, cgv;
  int y, u, v, r, g, b;
  int i;
  char dummy;
  int screen;
  Colormap cmap;
  int private;
  XColor xcolor;
  unsigned int fg, bg;
  char *hello = "H.261 Codec";
  XSizeHints hint;
  XVisualInfo vinfo;
  XEvent xev;
  unsigned long tmp_pixel;
  XWindowAttributes xwa;

  display = XOpenDisplay(name);

  if (display == NULL) {
    printf("Can not open display\n");
    return FALSE;
  }

  screen = DefaultScreen(display);

  hint.x = 200;
  hint.y = 200;
  if (expand) {
    hint.width = 2*Image->Width;  /* width of the display window */
    hint.height = 2*Image->Height;  /* height of the display window */
  }
  else {
    hint.width = Image->Width;  /* width of the display window */
    hint.height = Image->Height;  /* height of the display window */
  }

  hint.flags = PPosition | PSize;

  /* Get some colors */

  bg = WhitePixel (display, screen);
  fg = BlackPixel (display, screen);

  /* Make the window */

  if (!XMatchVisualInfo(display, screen, 8, PseudoColor, &vinfo))
  {
    if (!XMatchVisualInfo(display, screen, 8, GrayScale, &vinfo)) {
      printf("requires 8 bit display\n");
      return FALSE;
    }
  }

  window = XCreateSimpleWindow (display, DefaultRootWindow (display),
             hint.x, hint.y, hint.width, hint.height, 4, fg, bg);

  XSelectInput(display, window, StructureNotifyMask);

  /* Tell other applications about this window */

  XSetStandardProperties (display, window, hello, hello, None, NULL, 0, &hint);

  /* Map window. */

  XMapWindow(display, window);

  /* Wait for map. */
  do
  {
    XNextEvent(display, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != window);

  XSelectInput(display, window, NoEventMask);

  /* matrix coefficients */
  crv = convmat[matrix_coefficients][0];
  cbu = convmat[matrix_coefficients][1];
  cgu = convmat[matrix_coefficients][2];
  cgv = convmat[matrix_coefficients][3];

  /* allocate colors */

  gc = DefaultGC(display, screen);
  cmap = DefaultColormap(display, screen);
  private = 0;

  /* color allocation:
   * i is the (internal) 8 bit color number, it consists of separate
   * bit fields for Y, U and V: i = (yyyyuuvv), we don't use yyyy=0000
   * yyyy=0001 and yyyy=1111, this leaves 48 colors for other applications
   *
   * the allocated colors correspond to the following Y, U and V values:
   * Y:   40, 56, 72, 88, 104, 120, 136, 152, 168, 184, 200, 216, 232
   * U,V: -48, -16, 16, 48
   *
   * U and V values span only about half the color space; this gives
   * usually much better quality, although highly saturated colors can
   * not be displayed properly
   *
   * translation to R,G,B is implicitly done by the color look-up table
   */
 
 /* clip table */
  clp = clp_tab + 384;
  for (i=-384; i<640; i++)
    clp[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

  for (i=32; i<240; i++)
  {
    /* color space conversion */
    y = 16*((i>>4)&15) + 8;
    u = 32*((i>>2)&3)  - 48;
    v = 32*(i&3)       - 48;

    y = 76309 * (y - 16); /* (255/219)*65536 */

    r = clp[(y + crv*v + 32768)>>16];
    g = clp[(y - cgu*u -cgv*v + 32768)>>16];
    b = clp[(y + cbu*u + 32786)>>16];

    /* X11 colors are 16 bit */
    xcolor.red   = r << 8;
    xcolor.green = g << 8;
    xcolor.blue  = b << 8;

    if (XAllocColor(display, cmap, &xcolor) != 0)
      pixel[i] = xcolor.pixel;
    else
    {
      /* allocation failed, have to use a private colormap */

      if (private) {
        printf("Couldn't allocate private colormap\n");
	return FALSE;
      }

      private = 1;

      /* Free colors. */
      while (--i >= 32)
      {
        tmp_pixel = pixel[i]; /* because XFreeColors expects unsigned long */
        XFreeColors(display, cmap, &tmp_pixel, 1, 0);
      }

      /* i is now 31, this restarts the outer loop */

      /* create private colormap */

      XGetWindowAttributes(display, window, &xwa);
      cmap = XCreateColormap(display, window, xwa.visual, AllocNone);
      XSetWindowColormap(display, window, cmap);
    }
  }

    if (expand) {
      ximage = XCreateImage(display,None,8,ZPixmap,0,&dummy,
			    2*Image->Width,2*Image->Height,8,0);
      if (!(dithered_image = (unsigned char *)malloc(Image->Width*
						     Image->Height*4))) {
	printf("malloc failed while initializing display\n");
        return FALSE;
      }
    }
    else {
      ximage = XCreateImage(display,None,8,ZPixmap,0,&dummy,
			    Image->Width,Image->Height,8,0);
      if (!(dithered_image = (unsigned char *)malloc(Image->Width*
						     Image->Height))) {
	printf("malloc failed while initializing display\n");
        return FALSE;
     }
    }
}

static void display_image(ximage,dithered_image)
XImage *ximage;
unsigned char *dithered_image;
{
  /* display dithered image */
    ximage->data = (char *) dithered_image; 
    XPutImage(display, window, gc, ximage, 0, 0, 0, 0, ximage->width, ximage->height);
}


/* 4x4 ordered dither
 *
 * threshold pattern:
 *   0  8  2 10
 *  12  4 14  6
 *   3 11  1  9
 *  15  7 13  5
 */

void init_dither(void)
{
  int i, j, v;
  unsigned char ctab[256+32];

  for (i=0; i<256+16; i++)
  {
    v = (i-8)>>4;
    if (v<2)
      v = 2;
    else if (v>14)
      v = 14;
    for (j=0; j<16; j++)
      ytab[16*i+j] = pixel[(v<<4)+j];
  }

  for (i=0; i<256+32; i++)
  {
    v = (i+48-128)>>5;
    if (v<0)
      v = 0;
    else if (v>3)
      v = 3;
    ctab[i] = v;
  }

  for (i=0; i<255+15; i++)
    for (j=0; j<255+15; j++)
      uvtab[256*i+j]=(ctab[i+16]<<6)|(ctab[j+16]<<4)|(ctab[i]<<2)|ctab[j];
}

void dither(FSTORE *fs)
{
  FSTORE *fs_temp;

  if (expand) {
	fs_temp = make_FS(Image->width[_Y]<<1, Image->height[_Y]<<1);
	interpolate_image(fs, fs_temp);
	fs = fs_temp;
  }
  ditherframe(fs);
  display_image(ximage,dithered_image);

  if (expand) free_FS(fs);
}

/* only for 4:2:0 and 4:2:2! */

static void ditherframe(FSTORE *fs)
{
  int i,j;
  unsigned int uv;
  unsigned char *py,*pu,*pv,*dst;

  int width, height, cwidth;

  if (expand) {
    width = 2*Image->Width;
    height = 2*Image->Height;
    cwidth = 2*Image->width[_Cb];
  }
  else {
    width = Image->Width;
    height = Image->Height;
    cwidth = Image->width[_Cb];
  }

  py = fs->fs[_Y]->data;
  pu = fs->fs[_Cb]->data;
  pv = fs->fs[_Cr]->data;
  dst = dithered_image;

  for (j=0; j<height; j+=4)
  {
    /* line j + 0 */
    for (i=0; i<width; i+=8)
    {
      uv = uvtab[(*pu++<<8)|*pv++];
      *dst++ = ytab[((*py++)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +8)<<4)|(uv>>4)];
      uv = uvtab[((*pu++<<8)|*pv++)+1028];
      *dst++ = ytab[((*py++ +2)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +10)<<4)|(uv>>4)];
      uv = uvtab[(*pu++<<8)|*pv++];
      *dst++ = ytab[((*py++)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +8)<<4)|(uv>>4)];
      uv = uvtab[((*pu++<<8)|*pv++)+1028];
      *dst++ = ytab[((*py++ +2)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +10)<<4)|(uv>>4)];
    }

    pu -= cwidth;
    pv -= cwidth;

    /* line j + 1 */
    for (i=0; i<width; i+=8)
    {
      uv = uvtab[((*pu++<<8)|*pv++)+2056];
      *dst++ = ytab[((*py++ +12)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +4)<<4)|(uv&15)];
      uv = uvtab[((*pu++<<8)|*pv++)+3084];
      *dst++ = ytab[((*py++ +14)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +6)<<4)|(uv&15)];
      uv = uvtab[((*pu++<<8)|*pv++)+2056];
      *dst++ = ytab[((*py++ +12)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +4)<<4)|(uv&15)];
      uv = uvtab[((*pu++<<8)|*pv++)+3084];
      *dst++ = ytab[((*py++ +14)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +6)<<4)|(uv&15)];
    }

    /* line j + 2 */
    for (i=0; i<width; i+=8)
    {
      uv = uvtab[((*pu++<<8)|*pv++)+1542];
      *dst++ = ytab[((*py++ +3)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +11)<<4)|(uv>>4)];
      uv = uvtab[((*pu++<<8)|*pv++)+514];
      *dst++ = ytab[((*py++ +1)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +9)<<4)|(uv>>4)];
      uv = uvtab[((*pu++<<8)|*pv++)+1542];
      *dst++ = ytab[((*py++ +3)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +11)<<4)|(uv>>4)];
      uv = uvtab[((*pu++<<8)|*pv++)+514];
      *dst++ = ytab[((*py++ +1)<<4)|(uv&15)];
      *dst++ = ytab[((*py++ +9)<<4)|(uv>>4)];
    }

    pu -= cwidth;
    pv -= cwidth;

    /* line j + 3 */
    for (i=0; i<width; i+=8)
    {
      uv = uvtab[((*pu++<<8)|*pv++)+3598];
      *dst++ = ytab[((*py++ +15)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +7)<<4)|(uv&15)];
      uv = uvtab[((*pu++<<8)|*pv++)+2570];
      *dst++ = ytab[((*py++ +13)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +5)<<4)|(uv&15)];
      uv = uvtab[((*pu++<<8)|*pv++)+3598];
      *dst++ = ytab[((*py++ +15)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +7)<<4)|(uv&15)];
      uv = uvtab[((*pu++<<8)|*pv++)+2570];
      *dst++ = ytab[((*py++ +13)<<4)|(uv>>4)];
      *dst++ = ytab[((*py++ +5)<<4)|(uv&15)];
    }
  }
}

void interpolate_image(FSTORE *fs_in, FSTORE *fs_out)
{
  int16 width, height, w2, h2;
  int i, x,xx,y;
  unsigned char *out,*in;

  for (i=0; i<NUMBER_OF_COMPONENTS; i++) {
	width = Image->width[i];
	height = Image->height[i];
	in = fs_in->fs[i]->data;
	out = fs_out->fs[i]->data;
	w2 = 2 * width;

	/* Horizontally */
	for (y = 0; y < height-1; y++) {
		for (x = 0,xx=0; x < width-1; x++,xx+=2) {
			*(out + xx) = *(in + x);
			*(out + xx+1) = (*(in + x)  + *(in + x + 1) )>>1;
			*(out + w2 + xx) = (*(in + x) + *(in + x + width))>>1;
			*(out + w2 + xx+1) = (*(in + x) + *(in + x + 1) +
			   *(in + x + width) + *(in + x + width + 1))>>2;

		}
		*(out + w2 - 2) = *(in + width - 1);
		*(out + w2 - 1) = *(in + width - 1);
		*(out + w2 + w2 - 2) = *(in + width + width - 1);
		*(out + w2 + w2 - 1) = *(in + width + width - 1);
		out += w2<<1;
		in += width;
	}
	/* last lines */
	for (x = 0,xx=0; x < width-1; x++,xx+=2) {
		*(out+ xx) = *(in + x);
		*(out+ xx+1) = (*(in + x) + *(in + x + 1) + 1)>>1;
		*(out+ w2+ xx) = *(in + x);
		*(out+ w2+ xx+1) = (*(in + x) + *(in + x + 1) + 1)>>1;
	}

	/* bottom right corner pels */
	*(out + (width<<1) - 2) = *(in + width -1);
	*(out + (width<<1) - 1) = *(in + width -1);
	*(out + (width<<2) - 2) = *(in + width -1);
	*(out + (width<<2) - 1) = *(in + width -1);
  }
}

#endif
