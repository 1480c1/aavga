#include <aalib.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/times.h>

static aa_context *context = NULL;
static unsigned char *buffer;
static aa_palette palette;
static aa_renderparams *params;
static void (*kbd_handler) (int scancode, int press) = NULL;
static int scantokey[128];
static unsigned char scanpressed[128] =
{0,};
static int mousesupport;
static int cmode;
static const int debug = 0;
int page;
int mouse_button, mouse_x, mouse_y;
#define IS_LINEAR 32

struct vga_modeinfo
  {
    int width;
    int height;
    int bytesperpixel;
    int colors;
    int linewidth;		/* scanline width in bytes */
    int maxlogicalwidth;	/* maximum logical scanline width */
    int startaddressrange;	/* changeable bits set */
    int maxpixels;		/* video memory / bytesperpixel */
    int haveblit;		/* mask of blit functions available */
    int flags;			/* other flags */

    /* Extended fields: */

    int chiptype;		/* Chiptype detected */
    int memory;			/* videomemory in KB */
    int linewidth_unit;		/* Use only a multiple of this as parameter for set_logicalwidth and
				 * set_displaystart */
    char *linear_aperture;	/* points to mmap secondary mem aperture of card (NULL if
				 * unavailable) */
    int aperture_size;		/* size of aperture in KB if size>=videomemory. 0 if unavail */
    void (*set_aperture_page) (int page);
    /* if aperture_size<videomemory select a memory page */
    void *extensions;		/* points to copy of eeprom for mach32 */
    /* depends from actual driver/chiptype.. etc. */
  }
mode[] =
{
  { 0, 0 } ,				/*0 */
  { 0, 0 } ,				/*1 */
  { 0, 0 } ,				/*2 */
  { 0, 0 } ,				/*3 */
  { 0, 0 } ,				/*4 */
  { 320, 200, 1, 256, 320, 320, 0, 320 * 200, 0, IS_LINEAR, 0, 320 * 200, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*5 */
  { 320, 240, 1, 256, 320, 320, 0, 320 * 240, 0, IS_LINEAR, 0, 320 * 240, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*6 */
  { 320, 400, 1, 256, 320, 320, 0, 320 * 400, 0, IS_LINEAR, 0, 320 * 400, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*7 */
  { 360, 480, 1, 256, 360, 360, 0, 260 * 480, 0, IS_LINEAR, 0, 360 * 480, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*8 */
  { 0, 0 } ,				/*9 */
  { 640, 480, 1, 256, 640, 480, 0, 640 * 480, 0, IS_LINEAR, 0, 640 * 480, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*11 */
  { 800, 600, 1, 256, 800, 600, 0, 800 * 600, 0, IS_LINEAR, 0, 800 * 600, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*12 */
  { 1024, 768, 1, 256, 1024, 768, 0, 1024 * 768, 0, IS_LINEAR, 0, 1024*768, 0, NULL, 1024 * 1024, NULL, NULL } ,				/*13 */

};
#define MAXMODE 13
int 
vga_setmousesupport (int stat)
{
  if (debug)
    fprintf (stderr, " AA-lib mouse support:%i\n", stat);
  mousesupport = stat;
}


int 
vga_init ()
{
  if (debug)
    fprintf (stderr, " AA-lib SVGA emulation initialized\n");
  return 1;
}
int 
vga_getcurrentmode (void)
{
  if (debug)
    fprintf (stderr, " AA-lib get current mode\n");
  return cmode;
}

int 
vga_setmode (int x)
{
  int initialized = 0;
  fprintf (stderr, " AA-lib SVGA emulation mode:%i\n", x);
  aa_parseoptions (NULL, NULL, NULL, NULL);
  cmode = x;
  if (x == 0 && context)
    return aa_close (context), free (buffer), 0;
  if (x <= MAXMODE && mode[x].width && !context)
    if (context = aa_autoinit (&aa_defparams))
      {
	if (buffer = malloc (mode[x].width * mode[x].height))
	  {
	    params = aa_getrenderparams ();
#if 0
	    params->bright = 100;
	    params->contrast = 100;
#endif
	    memset (aa_image (context), 0, aa_imgwidth (context) * aa_imgheight (context));
	    mode[cmode].linear_aperture = buffer;
	    if(mousesupport) {
            if (!aa_autoinitkbd (context, AA_SENDRELEASE))
              return fprintf (stderr, "Error in aa_autoinitkbd!\n"), 1;
	    if (!aa_autoinitmouse (context, AA_SENDRELEASE))
	      return fprintf (stderr, "Error in aa_autoinitmouse!\n"), 1;
	    }
	    fprintf (stderr, " AA-lib initialized\n");
	    return 0;
	  }
	else
	  exit ((perror ("malloc"), -1));
      }
  return fprintf (stderr, "aavga: vga_setmode(%d) failed!\n", x), 1;
}

void *
vga_getgraphmem ()
{
  if (debug)
    fprintf (stderr, " AA-lib getgraphmem\n");
  return buffer + page * 65536;
}

void 
fastscale (register char *b1, register char *b2, int x1, int x2, int y1, int y2)
{
  register int ex, spx = 0, ddx, ddx1;
  int ddy1, ddy, spy = 0, ey;
  int x;
  char *bb1 = b1;
  if (!x1 || !x2 || !y1 || !y2)
    return;
  ddx = x1 + x1;
  ddx1 = x2 + x2;
  if (ddx1 < ddx)
    spx = ddx / ddx1, ddx %= ddx1;
  ddy = y1 + y1;
  ddy1 = y2 + y2;
  if (ddy1 < ddy)
    spy = (ddy / ddy1) * x1, ddy %= ddy1;
  ey = -ddy1;
  for (; y2; y2--)
    {
      ex = -ddx1;
      for (x = x2; x; x--)
	{
	  *b2 = *b1;
	  b2++;
	  b1 += spx;
	  ex += ddx;
	  if (ex > 0)
	    {
	      b1++;
	      ex -= ddx1;
	    }
	}
      bb1 += spy;
      ey += ddy;
      if (ey > 0)
	{
	  bb1 += x1;
	  ey -= ddy1;
	}
      b1 = bb1;
    }
}
#define MAXFRAMES 40
#define TIME (2)
static void 
vga_flush ()
{
  struct tms t;
  clock_t time1;
  static clock_t ttime;
  unsigned char *img = aa_image (context);
  time1 = times (&t);
  if (time1 > (ttime + (clock_t) TIME) || time1 < ttime)
    {
      fastscale (buffer, img, mode[cmode].width, aa_imgwidth (context), mode[cmode].height, aa_imgheight (context));
      aa_renderpalette (context, palette, params, 0, 0, aa_scrwidth (context), aa_scrheight (context));
      aa_flush (context);
      ttime = times (&t);
    }
}
int 
vga_getxdim (void)
{
  if (debug)
    fprintf (stderr, " AA-lib getxdim\n");
  return mode[cmode].width;
}
int 
vga_getydim (void)
{
  if (debug)
    fprintf (stderr, " AA-lib getydim\n");
  return mode[cmode].height;
}
int 
vga_getcolors (void)
{
  if (debug)
    fprintf (stderr, " AA-lib getcolors\n");
  return 256;
}
void 
vga_setpage (int i)
{
  if (debug)
    fprintf (stderr, " AA-lib setpage\n");
  page = i;
  vga_flush ();
}


int 
vga_lastmodenumber ()
{
  if (debug)
    fprintf (stderr, "AA-VGA lastmodenumber\n");
  return MAXMODE;
}

int 
vga_hasmode (int x)
{
  if (debug)
    fprintf (stderr, "AA-VGA hasmode:%i\n", x);
  return (x <= MAXMODE && mode[x].width != 0);
}

struct vga_modeinfo *
vga_getmodeinfo (int x)
{
  if (debug)
    fprintf (stderr, "AA-VGA getmodeinfo:%i %i\n", x,mode[x].colors);
  return &mode[x];
}

int 
vga_getmodenumber (char *s)
{
  if (debug)
    fprintf (stderr, "AA-VGA getmodenumber,%s\n", s);
  return 5;
}


int 
vga_setpalvec (int a, int b, int *n)
{
  int i;

  if (debug)
    fprintf (stderr, "AA-VGA setplavec\n");
  for (i = a; i < b; i++)
    aa_setpalette (palette, i, n[i * 3] * 4, n[i * 3 + 1] * 4, n[i * 3 + 2] * 4);
  vga_flush (0);
}
int 
vga_setpalette (int i, int r, int g, int b)
{
  if (debug)
    fprintf (stderr, "AA-VGA setpalette\n");
  aa_setpalette (palette, i, r * 4, g * 4, b * 4);
  vga_flush (0);
}
int 
vga_setcolor (int i)
{
  if (debug)
    fprintf (stderr, "AA-VGA setcolor %i\n",i);
  vga_flush (0);
}

void 
vga_waitretrace ()
{
  if (debug)
    fprintf (stderr, "AA-VGA waitretrace\n");
  vga_flush ();
}

int 
vga_oktowrite ()
{
  if (debug)
    fprintf (stderr, "AA-VGA oktowrite\n");
  return 1;
}

int 
vga_getmousetype ()
{
  if (debug)
    fprintf (stderr, "AA-VGA getmousetype\n");
  return 0;
}

void 
vga_runinbackground (int x)
{
  if (debug)
    fprintf (stderr, "AA-VGA runinbackground\n");
}

int 
mouse_init ()
{
  mousesupport = 1;
  if (debug)
    fprintf (stderr, "AA-VGA mouse_init\n");
  return 0;
}
/* We can't emulate mouse returning fd.  */
int 
mouse_init_return_fd ()
{
  mousesupport = 1;
  if (debug)
    fprintf (stderr, "AA-VGA mouse_init_return_fd\n");
  return -1;
}

void 
mouse_seteventhandler (void *h)
{
  if (debug)
    fprintf (stderr, "AA-VGA seteventhandler\n");
}

void 
mouse_close ()
{
  if (debug)
    fprintf (stderr, "AA-VGA mouse_close\n");
}

int 
mouse_update ()
{
  keyboard_update ();
  aa_getmouse (context, &mouse_x, &mouse_y, &mouse_button);
  mouse_x = mouse_x * mode[cmode].width / aa_scrwidth (context);
  mouse_y = mouse_y * mode[cmode].height / aa_scrheight (context);
  if (debug)
    fprintf (stderr, "AA-VGA mouse_update %i %i %i\n", mouse_x, mouse_y, mouse_button);
  return 0;
}

int 
keyboard_init ()
{
  int i;

  if (debug)
    fprintf (stderr, "AA-VGA keyboard_init\n");
  if (!context)
    return fprintf (stderr, "Hrm, called before vga_setmode()?\n"), 1;

  for (i = 0; i < 128; i++)
    scantokey[i] = 0;

  scantokey[59] = AA_UNKNOWN;	/* F1 */// row 0

  scantokey[60] = AA_UNKNOWN;
  scantokey[61] = AA_UNKNOWN;
  scantokey[62] = AA_UNKNOWN;
  scantokey[63] = AA_UNKNOWN;
  scantokey[64] = AA_UNKNOWN;
  scantokey[65] = AA_UNKNOWN;
  scantokey[66] = AA_UNKNOWN;
  scantokey[67] = AA_UNKNOWN;
  scantokey[68] = AA_UNKNOWN;
  scantokey[87] = AA_UNKNOWN;
  scantokey[88] = AA_UNKNOWN;	/* F12 */

  scantokey[1] = AA_ESC;	/* escape */// row 1

  scantokey[2] = '1';
  scantokey[3] = '2';
  scantokey[4] = '3';
  scantokey[5] = '4';
  scantokey[6] = '5';
  scantokey[7] = '6';
  scantokey[8] = '7';
  scantokey[9] = '8';
  scantokey[10] = '9';
  scantokey[11] = '0';
  scantokey[12] = '-';
  scantokey[13] = '=';
  scantokey[14] = AA_BACKSPACE;

  scantokey[15] = '\t';		// row 2

  scantokey[16] = 'q';
  scantokey[17] = 'w';
  scantokey[18] = 'e';
  scantokey[19] = 'r';
  scantokey[20] = 't';
  scantokey[21] = 'y';
  scantokey[22] = 'u';
  scantokey[23] = 'i';
  scantokey[24] = 'o';
  scantokey[25] = 'p';
  scantokey[26] = '[';
  scantokey[27] = ']';
  scantokey[28] = '\r';		/* ENTER */

  scantokey[30] = 'a';		// row 3

  scantokey[31] = 's';
  scantokey[32] = 'd';
  scantokey[33] = 'f';
  scantokey[34] = 'g';
  scantokey[35] = 'h';
  scantokey[36] = 'j';
  scantokey[37] = 'k';
  scantokey[38] = 'l';
  scantokey[39] = ';';
  scantokey[40] = '\'';
  scantokey[41] = '`';

  scantokey[42] = AA_UNKNOWN;	/* shift */// row 4

  scantokey[43] = '\\';
  scantokey[44] = 'z';
  scantokey[45] = 'x';
  scantokey[46] = 'c';
  scantokey[47] = 'v';
  scantokey[48] = 'b';
  scantokey[49] = 'n';
  scantokey[50] = 'm';
  scantokey[51] = ',';
  scantokey[52] = '.';
  scantokey[53] = '/';
  scantokey[54] = AA_UNKNOWN;	/* shift */

  scantokey[29] = AA_UNKNOWN;	// row 5 /* ctrl */

  scantokey[56] = AA_UNKNOWN;	/* alt */
  scantokey[57] = ' ';
  scantokey[100] = AA_UNKNOWN;	/* alt */
  scantokey[97] = AA_UNKNOWN;	/* ctrl */


  scantokey[98] = '/';
  scantokey[55] = '8';
  scantokey[74] = '-';
  scantokey[71] = AA_UNKNOWN;	/* home */
  scantokey[72] = AA_UP;
  scantokey[73] = AA_UNKNOWN;	/* pgup */
  scantokey[75] = AA_LEFT;
  scantokey[76] = '5';
  scantokey[77] = AA_RIGHT;
  scantokey[78] = '+';
  scantokey[79] = AA_UNKNOWN;	/* end */
  scantokey[80] = AA_DOWN;
  scantokey[81] = AA_UNKNOWN;	/* pgdn */
  scantokey[82] = AA_UNKNOWN;	/* ins */
  scantokey[83] = AA_UNKNOWN;	/* del */
  scantokey[96] = '\n';

  scantokey[103] = AA_UP;	// arrow pad

  scantokey[108] = AA_DOWN;
  scantokey[105] = AA_LEFT;
  scantokey[106] = AA_RIGHT;

  scantokey[119] = AA_UNKNOWN;	/* pause */// misc

  if (!mousesupport)
  if (!aa_autoinitkbd (context, AA_SENDRELEASE))
    return fprintf (stderr, "Error in aa_autoinitkbd!\n"), 1;

  return 0;
}

int
keyboard_update ()
{
  int release = 0;
  int i, stat = 1, scan;
  int key = 0;
  if (debug)
    fprintf (stderr, "AA-VGA keyboard_update\n");
  vga_flush ();
  while (1)
    {
      if ((i = aa_getevent (context, 0)) == AA_NONE)
	return key;
      aa_getmouse (context, &mouse_x, &mouse_y, &mouse_button);
      mouse_x = mouse_x * mode[cmode].width / aa_scrwidth (context);
      mouse_y = mouse_y * mode[cmode].height / aa_scrheight (context);
      if (i >= AA_UNKNOWN && i < AA_RELEASE)
	{
	  /*fprintf (stderr, "key: %x pressed\n",i);*/
		return;
	  continue;
	}
      if (i >= AA_RELEASE)
	{
	  stat = 0;
	  release = 1;
	  i &= ~AA_RELEASE;
	}
      for (scan = 0; scan < 128; scan++)
	{
	  if (scantokey[scan] != i)
	    continue;
	  if (kbd_handler != NULL)
	    kbd_handler (scan, stat);

	  if (!(context->kbddriver->flags & AA_SENDRELEASE))
	    {
	      int y;
	      for (y = 0; y < 128; y++)
		if (scanpressed[y])
		  {
		    if (kbd_handler != NULL)
		      kbd_handler (y, 0);
		    scanpressed[y] = 0;
		  }
	      scanpressed[scan] = 1;
	    }
	  else
	    {
	      scanpressed[scan] = stat;
	    }
	}
    }
  return 1;
}
char *
keyboard_getstate ()
{
  if (debug)
    fprintf (stderr, "AA-VGA keyboard_getstate\n");
  return scanpressed;
}
int 
keyboard_keypressed (int scancode)
{
  if (debug)
    fprintf (stderr, "AA-VGA keyboard_keypressed\n");
  return scanpressed[scancode];
}
int 
vga_ext_set (int what,...)
{
  fprintf (stderr, "AA-VGA: Unknown extension %d\n", mode);
  return 0;
}

void 
keyboard_translatekeys (int m)
{
  if (debug)
    fprintf (stderr, "AA-VGA: translatekeys %i\n", m);
}

void 
keyboard_seteventhandler (void *h)
{
  if (debug)
    fprintf (stderr, "AA-VGA: seteventhandler\n");
  kbd_handler = h;
}

void 
keyboard_close ()
{
  if (debug)
    fprintf (stderr, "AA-VGA: keboard_close\n");
  aa_uninitkbd (context);
}
int 
mouse_getbutton ()
{
  if (debug)
    fprintf (stderr, "AA-VGA: mouse_button\n");
  return mouse_button;
}
int 
mouse_getx ()
{
  if (debug)
    fprintf (stderr, "AA-VGA: mouse_getx\n");
  return mouse_x;
}
int 
mouse_gety ()
{
  if (debug)
    fprintf (stderr, "AA-VGA: mouse_gety\n");
  return mouse_y;
}
void 
vga_setdisplaystart (int p)
{
  if (debug)
    fprintf (stderr, "AA-VGA: setdisplaystart\n");
}
void 
mouse_setwrap (int w)
{
  if (debug)
    fprintf (stderr, "AA-VGA: setwrap\n");
}
void 
mouse_setxrange (int x, int x2)
{
  if (debug)
    fprintf (stderr, "AA-VGA: setxrange\n");
}
void 
mouse_setyrange (int x, int x2)
{
  if (debug)
    fprintf (stderr, "AA-VGA: setyrange\n");
}
void 
mouse_setposition (int x, int y)
{
  if (debug)
    fprintf (stderr, "AA-VGA: setposition\n");
}
void 
vga_copytoplanar256 (char *v, int pitch, int voffset, int vpitch, int w, int h)
{
  int i, pos1 = 0, pos2 = voffset;
  if (debug)
    fprintf (stderr, "AA-VGA: copytoplanar256\n");
  for (i = 0; i < h; i++)
    {
      memcpy (buffer + pos2, v + pos1, w);
      pos1 += pitch;
      pos2 += vpitch;
    }
}
