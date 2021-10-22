/*----------------------------------------------------------------*
 *                                                                *
 *                     hp9816emu             V0.01 18/09/21       *
 *                     =========                                  *
 *                                                                *
 *        An hp9816 emulator for linux (64-bit)                   *
 *        Ported to linux by Dave Penkler based on                *
 *        Olivier de Smet's 98x6 emulator for windows             *
 *                                                                *
 *  License: GNU GENERAL PUBLIC LICENSE Version 3                 *
 *                                                                *
 *  Disclaimer: This software is provided as is. It comes with    *
 *  the big girl/boy warranty - you use it entirely on your       *
 *  own responsibility and at your own risk.                      *
 *  It should only be used for educational or recreational        *
 *  purposes and not for any commercial purposes whatsoever.      *
 *                                                                *
 *----------------------------------------------------------------*/
#include "EZ.h"
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include "common.h"
#include "hp9816emu.h"

/* Globals */

BOOL        bAutoSave = FALSE;
BOOL        bAutoSaveOnExit = FALSE;
BOOL        bDisplayLog = TRUE;
int         bPhosphor   = 1;   /* white=0, green=1, amber=2 */
int         bFPU = TRUE;
int         bRamInd = 1; // 512K
#define     MEM_SZ_CNT 6
// Memory sizes in KB last entry 8MB-512KB
int         memSizes[MEM_SZ_CNT] = {256, 512, 1024, 2048, 4096, 7680}; 
volatile unsigned int cpuCycles;
static char memDisp[64];
int         bKeeptime = TRUE;
pthread_t   cpuThread = 0;
Window      hWnd=0;

static int ret;
static int onKey(int,unsigned int, int);
static int onMouseWheel(UINT nFlags, WORD x, WORD y);
static int onFileSaveAs(VOID);
static int onFileOpen(VOID);
static void emuSetEvents(EZ_Widget *);

static EZ_Widget *toplevel = NULL;     /* toplevel frame           */
static EZ_Widget *fileSelector = NULL; /* image selector           */
/* Settings menu */
static EZ_Widget *settingsMenu = NULL; /* general settings menu    */
static EZ_Widget *seButton = NULL;     /* settings button          */
static EZ_Widget *memSel;              /* memory size selector     */
static EZ_Widget *fpuBtn;
static EZ_Widget *timBtn;
static EZ_Widget *pBtnW, *pBtnG, *pBtnA;
/* Menu bar */
static EZ_Widget *mhzLed;
static EZ_Widget *fpuLed;
/* Speed menu */
static EZ_Widget *spButton = NULL;     /* speed selector button    */
static EZ_Widget *speedMenu = NULL;    /* speed setting menu       */
/* System image menu */
static EZ_Widget *siButton = NULL;     /* system immage button     */
static EZ_Widget *siMenu   = NULL;     /* system immage menu       */
static int        siMenuPosted = 0;    /* sys img menu posted flag */
static char       imageFile[256];      /* image file name          */
/* Run button */
static EZ_Widget *runBtn;
/* Disk type labels */
static EZ_Widget *diskLabels[4]; /* 9121,9122,7908,7908 */
/* Unit/LIF volume labels */
static EZ_Widget *volumeLabels[6]; /* 700,0 700,1 702,0 702,1 703,0 704,0 */
/* Leds */
static EZ_Widget *leds[11]; /* Run 700 700,0 700,1 702 702,0 702,1 703 703,0 704 704,0 */
/* CPU Status leds */
static EZ_Widget *cpuStatus;

/* Display variables */
static EZ_Widget *workArea;            /* the 9816 display area    */
static int        emuInit = 0;
Display          *dpy;                 /* the display */
static GC         dispGC;
static XGCValues  gcvalues;

#define LARGE_FONT "-adobe-helvetica-bold-r-normal--14-100-100-100-*-*-*-*"

void emuFlush() {
  XSync(dpy,0);
}

static int oldop = -1; 
void emuBitBlt(Pixmap dst, int dx0, int dy0, int dw, int dh, Pixmap src, int sx0, int sy0, int op) {
  //fprintf(stderr,"emuBitBlt %ld  %ld\n",(long) dst, (long) src);

  if (op != oldop) {
    oldop = op;
    XSetFunction(dpy, dispGC, op );
  }
  XCopyArea(dpy, src, dst, dispGC, sx0, sy0, dw, dh, dx0, dy0);
}


void emuPatBlt(Pixmap dst, int dx0, int dy0, int dw, int dh, int op) {
  // fprintf(stderr,"emuPatBlt %ld  ",(long) dst);
 if (op != oldop) {
    oldop = op;
    XSetFunction(dpy, dispGC, op );
  }
  XFillRectangle(dpy, dst, dispGC,  dx0, dy0, dw, dh);
}

void emuImgBlt(Pixmap dst, GC gc, int dx0, int dy0, int w, int h, XImage *src, int sx0, int sy0, int op) {
  //  fprintf(stderr,"emuPutImage %ld  %ld %4d %4d %4d %4d %5d %5d\n",(long) dst, (long) src, sx0, sy0, dx0, dy0, w, h);	    
  XPutImage(dpy, dst, gc, src, sx0, sy0, dx0, dy0, w, h);
}

void emuPutImage(Pixmap dst, int dx0, int dy0, int w, int h, XImage *src, int op) {

  if (op != oldop) {
    oldop = op;
    XSetFunction(dpy, dispGC, op );
  }
  XPutImage(dpy, dst, dispGC, src, dx0, dy0, dx0, dy0, w, h);
}

Pixmap emuCreateBitMap(int w, int h) {
  Pixmap imagePixmap=0;
  // fprintf(stderr,"emuCreateBitMap %d %d\n",w,h);
  imagePixmap = XCreatePixmap(dpy, hWnd, w, h, EZ_GetDepth());
  return imagePixmap;
}

Pixmap emuCreateBitMapFromImage(int w, int h, XImage *xim) {
  Pixmap p;
  p = XCreatePixmap(dpy, hWnd, w, h, EZ_GetDepth());
  // fprintf(stderr,"CBMFD:w %d h %d xim 0x%08lx pix 0x%08lx \n", w, h, (unsigned long)xim, p);
  return p;
}

Pixmap emuLoadBitMap(LPCTSTR fn) {
  unsigned int w,h;
  Pixmap imagePixmap=0;
  // fprintf(stderr,"emuLoadImage %s\n",fn);
  EZ_CreateXPixmapFromImageFile((char *)fn, &w, &h, &imagePixmap);
  return imagePixmap;
}

void emuFreeBitMap(Pixmap bm) {
  ret =  XFreePixmap(dpy,bm);
  // fprintf(stderr,"emuFreeBitMap: %d\n",ret);
}

XImage *emuCreateImage(int w, int h) {
  char *img;
  img = malloc(w*h*4);
  // fprintf(stderr,"emuCreateImage %d %d\n", w, h);
  return XCreateImage(dpy, XDefaultVisual(dpy, EZ_GetScreen()) ,EZ_GetDepth(), ZPixmap,
		      0, img, w, h, 32, 0);
}

XImage *emuCreateImageFromBM(int w, int h, char *img) {
  // fprintf(stderr,"emuCreateImageFromBM %d %d\n", w, h);
  return XCreateImage(dpy, XDefaultVisual(dpy, EZ_GetScreen()) ,EZ_GetDepth(), ZPixmap,
		      0, img, w, h, 32, 0);
}

XImage *emuGetImage(Pixmap bm, int w, int h) {
  // fprintf(stderr,"emuGetImage %d %d\n", w, h);
  return XGetImage(dpy, bm, 0, 0, w, h, AllPlanes, ZPixmap);
}

void emuSetColours(unsigned int fg, unsigned int bg) {
  fprintf(stderr,"emuSetColours: 0x%08x 0x%08x\n",fg, bg);
  XSetBackground(dpy,dispGC,bg);
  XSetForeground(dpy,dispGC,fg);
}

void emuFreeImage(XImage *img) {
  ret = XDestroyImage(img);
  // fprintf(stderr,"emuFreeImage: %d\n",ret);
}

void emuSetWidgetPosition(EZ_Widget *parent, EZ_Widget *move, int x, int y) {
  // really move relative to parent
  int px,py,pw,ph;
  EZ_GetWidgetAbsoluteGeometry(parent, &px, &py, &pw, &ph);
  //  fprintf(stderr,"emuSetWidgetPosition px %d, py %d, pw %d, ph %d\n",px,py,pw,ph);
  EZ_MoveWidgetWindow(move,x+px+pw,y+py+ph);
  emuFlush();
}

static void hp9816emuComputeSize(EZ_Widget *widget, int *w, int *h) {
  /* this procedure is invoked by the geometry manager to
   * figure out the minimal size.
   */
  *w = bgW; *h = bgH;  
  //  fprintf(stderr,"hp9816emuComputeSize %d %d\n",bgW,bgH);
}

static void hp9816emuDraw(EZ_Widget *widget) {
  //  fprintf(stderr,"hp9816emuDraw\n");
  if (!emuInit) {
    hWnd = EZ_GetWidgetWindow(widget); /* widget window */
    /* GC for graphics and alpha display operations */
    gcvalues.function = GXclear;
    gcvalues.graphics_exposures = 0;
    gcvalues.plane_mask = AllPlanes;
    dispGC = XCreateGC(dpy, hWnd, GCPlaneMask | GCGraphicsExposures | GCFunction, &gcvalues);
    XFillRectangle(dpy, hWnd, dispGC, 0, 0, bgW, bgH);
    // XMapWindow(dpy, hWnd);
    emuSetEvents(widget);
    emuInit = 1;
    return;
  } else {
    // redraw main display area
    refreshDisplay(TRUE);
  }
}

static void hp9816emuFreeData(EZ_Widget *widget) {
}
				 
static void hp9816emuEventHandler(EZ_Widget *widget, XEvent *event) {
  int x,y;
  if (!event) return;
  //fprintf(stderr,"hp9816emuEventHandler\n");
  switch (event->type) {
  case Expose: hp9816emuDraw(widget); break;
  case ButtonPress:
    x = event->xbutton.x;
    y = event->xbutton.y;
    //fprintf(stderr,"Button press %d\n",event->xbutton.button);
    switch (event->xbutton.button) {
    case Button4: onMouseWheel(-1,x,y); break;
    case Button5: onMouseWheel( 1,x,y); break;
    default:;
    }
    break;
  case ButtonRelease:
    x = event->xbutton.x;
    y = event->xbutton.y;
    //fprintf(stderr,"Button Release %d\n",event->xbutton.button);
    switch (event->xbutton.button) {
    case Button4: onMouseWheel(-1,x,y); break;
    case Button5: onMouseWheel( 1,x,y); break;
    default:;
    }
    break;
  case KeyPress:   onKey(1, event->xkey.keycode, event->xkey.state); break;
  case KeyRelease: onKey(0, event->xkey.keycode, event->xkey.state); break;
  default:
    // fprintf(stderr,"Unexpected event type %d\n",event->type);
  }
  return;
  //hp9816emuDraw(widget);
  //EZ_CallWidgetMotionCallbacks(widget);
}

static double tstart = 0.0;
static int tfpu = 0;
static void timerCB(EZ_Timer *t, void *p) {
  struct timeval tv;
  double mtime,itime,ticks,mhz;
  char buf[64];

  gettimeofday(&tv,NULL);
  ticks = cpuCycles;
  cpuCycles = 0;
  mtime = tv.tv_sec*1e6L + tv.tv_usec;
  if (tstart==0.0) tstart = mtime;
  else {
    if (!checkChipset()) fprintf(stderr,"Chipset memory corruption\n");
    itime  = mtime - tstart;
    tstart = mtime;
    mhz = ticks/itime;
    sprintf(buf, " Freq:%5.1fMHz Mem:%5dK\n",mhz,Chipset.RamSize/1024);
    EZ_SetLedString(mhzLed, buf, "green");
    if (Chipset.Hp98635 != tfpu) {
      tfpu = Chipset.Hp98635;
      EZ_ConfigureWidget(fpuLed, EZ_LED_PIXEL_COLOR, (tfpu ? "green" : "gray20"), 0);
    }
  }
}

static void passThruEventHandler(EZ_Widget *w, void *d, int etype, XEvent *event) {
  if ((event->type == KeyPress) || (event->type == KeyRelease)) {
    hp9816emuEventHandler(workArea, event);
    //fprintf(stderr,"ptevh dropping event\n");
    EZ_RemoveEvent(event);
  } else {
    //fprintf(stderr,"ptevh calling next\n");
  }
}

static void emuSetEvents(EZ_Widget *widget) {
  Window   win;        /* widget window */
  XWindowAttributes watt;
  XSetWindowAttributes swatt;

  win = EZ_GetWidgetWindow(widget);
  XGetWindowAttributes(dpy,win,&watt);
  // Need to add key and button up to default EZWGL events
  swatt.event_mask = watt.your_event_mask | KeyReleaseMask | ButtonReleaseMask;
  XChangeWindowAttributes(dpy,win,CWEventMask,&swatt);
}

static void quitCB(EZ_Widget *w, void *d) {
  int retval;
  if (cpuThread) {
    switchToState(SM_RETURN);
    pthread_join(cpuThread, (void **)&retval);
    fprintf(stderr,"CPU thread stopped\n");
  }
  if (bAutoSaveOnExit) {
    /* TODO save image */
  } 
  EZ_Shutdown();
  // CloseWaves();				// free sound memory
  exit(0);
}

static void runCB(EZ_Widget *w, void *d) {
  int s;
  EZ_GetCheckButtonState(w,&s);
  
  if (s) {
    if (nState == SM_RUN) {
      emuInfoMessage("runCB: cpu already in run state\n");
      return;
    }
    switchToState(SM_RUN); 
    EZ_SetFocusTo(workArea);// Send keyboard events to emulator
  } else {
    if (nState != SM_RUN) return;
    switchToState(SM_INVALID);
  }
}

static void setBufferFilename(EZ_Widget *widget, void *d) {
  char *file = NULL;
  long munit;
  file = EZ_GetFileSelectorSelection(widget);
  munit = EZ_GetWidgetIntData(widget);

  if (file && strlen(file)) {
    strcpy(szBufferFilename, file);
    ((EZ_CallBack)d)(widget,(void *)munit);
  } else {
    szBufferFilename[0] = 0;
  }
  EZ_HideWidget(fileSelector);
}

static void setupFileSelector() {
  EZ_Widget *jnk, *ok, *filter, *cancel;
  fileSelector = EZ_CreateWidget(EZ_WIDGET_FILE_SELECTOR, NULL,
				 EZ_CALLBACK, setBufferFilename, NULL,
				 EZ_SIZE,     400, 400,
				 EZ_TRANSIENT, EZ_TRUE,
				 0);
  EZ_GetFileSelectorWidgetComponents(fileSelector, &jnk, &jnk, &jnk, &jnk,
				     &ok, &filter, &cancel);
  EZ_ConfigureWidget(ok, EZ_LABEL_STRING, "OK", NULL);
  EZ_ConfigureWidget(cancel, EZ_LABEL_STRING, "Cancel", NULL);
}

static void showFileSelector(EZ_Widget *notused, void *d) {
  EZ_DisplayWidget(fileSelector); 
  EZ_RaiseWidgetWindow(fileSelector);
}

void okCB(EZ_Widget *widget, void *data) {
  int phos;
  EZ_Widget *parent = data;
  EZ_HideWidget(parent);
  
  phos = EZ_GetRadioButtonGroupVariableValue(pBtnW);
  // fprintf(stderr,"Phosphor: %d  bP %d\n",phos ,bPhosphor);
  if (phos != bPhosphor) { 
    bPhosphor = phos;
  } else testFont(); 
}

void settingsCB(EZ_Widget *w, void *data) {
  EZ_SetCheckButtonState(fpuBtn,bFPU);
  EZ_SetCheckButtonState(timBtn,bKeeptime);
  EZ_SetRadioButtonGroupVariableValue(pBtnW, bPhosphor);
  EZ_DisplayWidget(settingsMenu);
  emuSetWidgetPosition(seButton, settingsMenu,-32,0);
  EZ_RaiseWidgetWindow(settingsMenu);
}

void speedMenuCB(EZ_Widget *w, void *data) {
  EZ_DoPopup(speedMenu, EZ_BOTTOM_RIGHT);
}

void speedCB(EZ_Widget *w, void *data) {
  int speed = EZ_GetRadioButtonGroupVariableValue(w);
  // fprintf(stderr,"speedCB %d\n",speed);
  setSpeed(speed);
  wRealSpeed = speed;
}

void siMenuCB(EZ_Widget *w, void *data) {
  EZ_DoPopup(siMenu, EZ_BOTTOM_RIGHT);
}

static void genCB(EZ_Widget *w, void *d) {
  int s;
  long b = (long) d;
  EZ_GetCheckButtonState(w,&s);
  switch (b) {
  case 0: bAutoSave = s; break;
  case 1: bAutoSaveOnExit = s; break;
  default: fprintf(stderr,"Unknown id %ld in genCB\n",b);
  }
}

static void timCB(EZ_Widget *w, void *d) {
  int s;
  EZ_GetCheckButtonState(w,&s);
  bKeeptime = s;
}

static void fpuCB(EZ_Widget *w, void *d) {
  int s;
  EZ_GetCheckButtonState(w,&s);
  bFPU = s;
  // fprintf(stderr, "bFPU now %d\n",bFPU);
}

static void cancelCB(EZ_Widget *widget, void *data) {
  EZ_Widget *w =(EZ_Widget *)data; 
  EZ_HideWidget(w);
  EZ_DestroyWidget(w);
}

void emuInfoMessage(char *str) {

  EZ_Widget  *toplevel,  *quit;	      

  toplevel = EZ_CreateWidget(EZ_WIDGET_FRAME,   NULL,
			     EZ_LABEL_STRING,   " ",
			     EZ_TRANSIENT,       EZ_TRUE,
			     EZ_BORDER_WIDTH,    0,
			     EZ_ORIENTATION,     EZ_VERTICAL,
			     EZ_FILL_MODE,       EZ_FILL_BOTH,
			     NULL);

  EZ_CreateWidget(EZ_WIDGET_LABEL,    toplevel,
		  EZ_LABEL_STRING,    str,
		  EZ_TEXT_LINE_LENGTH, 120,
		  EZ_JUSTIFICATION,    EZ_LEFT,
		  EZ_FOREGROUND,       "red",
		  EZ_FONT_NAME,         LARGE_FONT,
		  NULL);
  
  
  quit =     EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON, toplevel,
			     EZ_WIDTH,          36,
			     EZ_LABEL_STRING,   "OK",
			     EZ_UNDERLINE,       0,
			     EZ_CALLBACK,        cancelCB, toplevel,
			     NULL );

  emuSetWidgetPosition(workArea,toplevel,-700,-400);
  EZ_DisplayWidget(toplevel);
  EZ_SetGrab(toplevel);
  EZ_SetFocusTo(quit);
}

void emuUpdateButton(int hpibAddr, int unit, char * lifVolume) {
  char buf[64];
  char * col = "red";
  int volId;
  switch (hpibAddr) {
  case 0: volId = unit;   break;
  case 2: volId = 2+unit; break;
  case 3: volId = 4;      break;
  case 4: volId = 5;      break;
  default:
    fprintf(stderr,"emuUpdateButton: Invalid Add: %d unit %d\n", hpibAddr, unit);
  }
  //  fprintf(stderr,"Addr: %d unit %d, volId %d vol <%s>\n", hpibAddr, unit, volId, lifVolume);
  if (!lifVolume) {
    sprintf(buf,"Unit %d",unit);
    lifVolume = buf;
    col = "black";
  } else if (lifVolume[0] == 0x00) {
    sprintf(buf,"Unit %d",unit);
    lifVolume = buf;
  }
  EZ_ConfigureWidget(volumeLabels[volId],EZ_FOREGROUND, col, EZ_LABEL_STRING, lifVolume, 0);
}

void emuUpdateDisk(int hpibadd, char * name) {
  int diskNo = 0;
  if (hpibadd>0) diskNo = hpibadd-1;
  fprintf(stderr,"Disk: %d: <%s>\n",diskNo, name);
  EZ_ConfigureWidget(diskLabels[diskNo], EZ_LABEL_STRING, name, 0);
}


void emuUpdateLed(int id, int status) {
  if (id < 11) {
     EZ_ConfigureWidget(leds[id], EZ_LED_PIXEL_COLOR, (status ? "green" : "gray20"), 0);
  } else {
    id -= 11;
    EZ_OnOffLedPixel(cpuStatus,2,id,0,status ? "red" : "gray20");
  }
}

static void setupSpeedMenu() {

  speedMenu = EZ_CreateSimpleMenu("8Mhz %r[2,1,1] %f|16MHz %r[2,2,1] %f|24Mhz%r[2,3,1]%f|"
				  "32Mhz %r[2,4,1] %f|40MHz %r[2,5,1] %f|Max %r[2,0,1]%f",
				  speedCB, NULL,  speedCB, NULL, speedCB, NULL, speedCB, NULL,
				  speedCB, NULL, speedCB, NULL);

  EZ_ConfigureWidget(speedMenu, EZ_MENU_TEAR_OFF, 0, NULL);
}

static char *selectMem(int last, int current, void *data) {
  current %= MEM_SZ_CNT;
  if (current < 0) current += MEM_SZ_CNT;
  sprintf(memDisp,"%4d KB",memSizes[current]);
  bRamInd = current;
  return memDisp;
}

static void setupSettingsMenu() {
  EZ_Widget *sysFrame, *memFrame,*fpuFrame, *genFrame, *btnFrame;

  settingsMenu = EZ_CreateWidget(EZ_WIDGET_FRAME,         NULL,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_TRANSIENT,            EZ_TRUE,
				 EZ_SIDE,                 EZ_LEFT,
				 EZ_FILL_MODE,            EZ_FILL_BOTH,
				 NULL);
 
  sysFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         settingsMenu,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_LABEL_STRING,         "System Image Settings",
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  memFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         sysFrame,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_LABEL_STRING,         "Memory Size",
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  memSel       = EZ_CreateWidget(EZ_WIDGET_SPIN_BUTTON,   memFrame,
				 EZ_SPIN_VALUE,           bRamInd, "512 KB",
				 EZ_SPIN_FUNCTION,        selectMem, NULL,
				 NULL);

  fpuFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         sysFrame,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_LABEL_STRING,        "HP98635 Floating Point",
				 EZ_JUSTIFICATION,        EZ_LEFT,
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  fpuBtn       = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  fpuFrame,
				 //				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Enabled",
				 EZ_CALLBACK,             fpuCB, 0,
				 0);
		      
 
  genFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         settingsMenu,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_LABEL_STRING,         "General Settings",
				 EZ_FILL_MODE,            EZ_FILL_BOTH,
				 EZ_SIDE,                 EZ_CENTER,
				 NULL);

  timBtn       = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  genFrame,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Set system time",
				 EZ_CALLBACK,             timCB, 0,
				 0);

  EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  genFrame,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Auto save system image",
				 EZ_CALLBACK,             genCB, 0,
				 0);
  
  EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  genFrame,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Auto save on exit",
				 EZ_CALLBACK,             genCB, 1,
				 0);

  btnFrame  = EZ_CreateWidget(EZ_WIDGET_FRAME,      genFrame,
			      EZ_WIDTH,             100,
			      EZ_LABEL_STRING,     "Phosphor",
			      EZ_ORIENTATION,       EZ_VERTICAL,
			      EZ_FILL_MODE,         EZ_FILL_NONE,
			      EZ_SIDE,              EZ_CENTER,
			      NULL);
   
  pBtnW  = EZ_CreateWidget(EZ_WIDGET_RADIO_BUTTON,   btnFrame,
			   EZ_LABEL_STRING,          "White",
			   EZ_BORDER_WIDTH,          1,
			   EZ_BORDER_TYPE,           EZ_BORDER_UP,
			   EZ_RADIO_BUTTON_GROUP,    3,
			   EZ_RADIO_BUTTON_VALUE,    0,
			   EZ_INDICATOR_COLOR,       "white",
			   EZ_INDICATOR_TYPE,        EZ_SQUARE_INDICATOR,
			   EZ_INDICATOR_SIZE_ADJUST, 5,
			   NULL);

  
  pBtnG  = EZ_CreateWidget(EZ_WIDGET_RADIO_BUTTON,    btnFrame,
			   EZ_LABEL_STRING,           "Green",
			   EZ_BORDER_WIDTH,           0,
			   EZ_BORDER_TYPE,            EZ_BORDER_UP,
			   EZ_RADIO_BUTTON_GROUP,     3,
			   EZ_RADIO_BUTTON_VALUE,     1,
			   EZ_INDICATOR_COLOR,        "green",
			   EZ_INDICATOR_TYPE,         EZ_SQUARE_INDICATOR,
			   EZ_INDICATOR_SIZE_ADJUST,  5,
			   NULL);

  pBtnA  = EZ_CreateWidget(EZ_WIDGET_RADIO_BUTTON,    btnFrame,
			   EZ_LABEL_STRING,           "Amber",
			   EZ_BORDER_WIDTH,           1,
			   EZ_BORDER_TYPE,            EZ_BORDER_UP,
			   EZ_RADIO_BUTTON_GROUP,     3,
			   EZ_RADIO_BUTTON_VALUE,     2,
			   EZ_INDICATOR_COLOR,        "#ffaa00",
			   EZ_INDICATOR_TYPE,         EZ_SQUARE_INDICATOR,
			   EZ_INDICATOR_SIZE_ADJUST,  5,
			   NULL);

  
  EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON, settingsMenu,
		  EZ_LABEL_STRING,         "OK",
		  EZ_UNDERLINE,            0,
		  EZ_CALLBACK,             okCB, settingsMenu,
		  NULL);
}


//
// ID_9122_LOAD
//
static int on9122Load(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to load a Disk.");
    goto cancel;
  }
  if (hp9122_widle(ctrl)) {						// wait for idle hp9122 state
    emuInfoMessage("The HP9122 is busy.");
    goto cancel;
  } 
  hp9122_load(ctrl, byUnit, szBufferFilename);
  updateWindowStatus();
 cancel:
  return 0;
}

//
// ID_9122_SAVE
//
static int on9122Save(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to save a Disk.");
    goto cancel;
  }
  if (hp9122_widle(ctrl))	{			// wait for idle hp9122 state
    emuInfoMessage("The HP9122 is busy.");
    goto cancel;
  }
  _ASSERT(ctrl->name[byUnit][0] != 0x00);
  hp9122_save(ctrl, byUnit, szBufferFilename);
 cancel:
  return 0;
}

//
// ID_9122_EJECT
//
static int on9122Eject(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to eject a Disk.");
    goto cancel;
  }
  if (hp9122_widle(ctrl))	{			// wait for idle hp9122 state
    emuInfoMessage("The HP9122 is busy.");
    goto cancel;
  }
  hp9122_eject(ctrl, byUnit);
  updateWindowStatus();
 cancel:
  return (0);
}

//
// ID_9121_LOAD
//
static int on9121Load(HP9121 *ctrl, BYTE byUnit) {	// load an image to a disc unit

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to load a Disk.");
    goto cancel;
  }
  if (hp9121_widle(ctrl))	{			// wait for idle hp9121 state
    emuInfoMessage("The HP9121 is busy.");
    goto cancel;
  } 
  hp9121_load(ctrl, byUnit, szBufferFilename);
  updateWindowStatus();
 cancel:
  return 0;
}

//
// ID_9121_SAVE
//
static int on9121Save(HP9121 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to save a Disk.");
    goto cancel;
  }
  if (hp9121_widle(ctrl))	{			// wait for idle hp9122 state
    emuInfoMessage("The HP9121 is busy.");
    goto cancel;
  }
  _ASSERT(ctrl->name[byUnit][0] != 0x00);
  hp9121_save(ctrl, byUnit, szBufferFilename);
 cancel:
  return 0;
}

//
// ID_9121_EJECT
//
static int on9121Eject(HP9121 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to eject a Disk.");
    goto cancel;
  }
  if (hp9121_widle(ctrl)) {    // wait for idle hp9121 state
    emuInfoMessage("The HP9121 is busy.");
    goto cancel;
  }
  hp9121_eject(ctrl, byUnit);
  updateWindowStatus();
 cancel:
  return (0);
}


//
// ID_7908_LOAD
//
static int on7908Load(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to load a Disk.");
    goto cancel;
  }
  if (hp7908_widle(ctrl))	{	       	// wait for idle hp7908 state
    emuInfoMessage("The HP7908 is busy.");
    goto cancel;
  } 
  if (hp7908_load(ctrl, byUnit, szBufferFilename)) {
    updateWindowStatus();
  }
 cancel:
  return 0;
}

//
// ID_7908_EJECT
//
static int on7908Eject(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to eject a Disk.");
    goto cancel;
  }
  if (hp9122_widle(ctrl))	{			// wait for idle hp7908 state
    emuInfoMessage("The HP7908 is busy.");
    goto cancel;
  }
  hp7908_eject(ctrl, byUnit);
  updateWindowStatus();
 cancel:
  return (0);
}

static void doLoadCB(EZ_Widget *w, void *d) {
  long munit = (long) d;
  //fprintf(stderr,"loadCB %ld\n",munit);
  switch(munit) {
  case 1: on9121Load(&Chipset.Hp9121, 0); break;
  case 2: on9121Load(&Chipset.Hp9121, 1); break;
  case 3: on9122Load(&Chipset.Hp9122, 0); break;
  case 4: on9122Load(&Chipset.Hp9122, 1); break;
  case 5: on7908Load(&Chipset.Hp7908_0, 0); break;
  case 6: on7908Load(&Chipset.Hp7908_1, 0); break;
  default: fprintf(stderr,"Invalid unit\n");
  }
}

static void loadCB(EZ_Widget *w, void *d) {
  EZ_ConfigureWidget(fileSelector,
		     EZ_CALLBACK, setBufferFilename, (void *)doLoadCB,
		     EZ_CLIENT_INT_DATA, (long)d,
		     NULL);
  EZ_SetFileSelectorInitialPattern(fileSelector, "*.hpi");
  showFileSelector(w,d);
}

static void saveCB(EZ_Widget *w, void *d) {
  long munit = (long) d;
  //fprintf(stderr,"saveCB %ld\n", munit);
  switch(munit) {
  case 1: on9121Save(&Chipset.Hp9121, 0); break;
  case 2: on9121Save(&Chipset.Hp9121, 1); break;
  case 3: on9122Save(&Chipset.Hp9122, 0); break;
  case 4: on9122Save(&Chipset.Hp9122, 1); break;
    //  case 5: on7908Save(&Chipset.Hp7908_0, 0); break;
    //  case 6: on7908Save(&Chipset.Hp7908_1, 0); break;
  default: fprintf(stderr,"Invalid unit\n");
  }
}

static void ejectCB(EZ_Widget *w, void *d) {
  long munit = (long) d;
  //fprintf(stderr,"ejectCB %ld\n", munit);
   switch(munit) {
  case 1: on9121Eject(&Chipset.Hp9121, 0); break;
  case 2: on9121Eject(&Chipset.Hp9121, 1); break;
  case 3: on9122Eject(&Chipset.Hp9122, 0); break;
  case 4: on9122Eject(&Chipset.Hp9122, 1); break;
  case 5: on7908Eject(&Chipset.Hp7908_0, 0); break;
  case 6: on7908Eject(&Chipset.Hp7908_1, 0); break;
  default: fprintf(stderr,"Invalid unit\n");
  }
}

static UINT saveChanges(BOOL bAuto) {
  char buf[1024];
  if (!bAuto) return 1;
  if (!imageFile[0]) {
    strcpy(imageFile,"system.img");
    snprintf(buf, 512, "Saving image in default file: %s\n",imageFile);
    emuInfoMessage(buf);
  }
  fprintf(stderr, "Saving system image in: %s\n",imageFile);
  if (saveSystemImageAs(imageFile)) {
    setWindowTitle(imageFile);
    return 1;
  } else {
    emuInfoMessage("Failed to save image");
    imageFile[0] = 0; // kill bad file name
    return 0;
  }
}

static void loadSICB(EZ_Widget *w, void *d) {
  EZ_HideWidget(siMenu);
  EZ_ConfigureWidget(fileSelector,
		     EZ_CALLBACK, setBufferFilename, (void *)onFileOpen,
		     EZ_CLIENT_INT_DATA, (long)d,
		     NULL);
  EZ_SetFileSelectorInitialPattern(fileSelector, "*.img");
  showFileSelector(w,d);
}

static void saveSICB(EZ_Widget *w, void *data) {
  int mstate=nState;
  switchToState(SM_INVALID);
  saveChanges(TRUE);
  switchToState(mstate);
  return;
}

static void saveAsSICB(EZ_Widget *w, void *data) {
  //  EZ_HideWidget(siMenu);
  EZ_ConfigureWidget(fileSelector,
		     EZ_CALLBACK, setBufferFilename, (void *)onFileSaveAs,
		     EZ_CLIENT_INT_DATA, (long)data,
		     NULL);
  EZ_SetFileSelectorInitialPattern(fileSelector, "*.img");
  showFileSelector(w, data);
  return;
}

static int onFileNew(VOID) {
  int mstate=nState;

  switchToState(SM_INVALID);
  if (!saveChanges(bAutoSave))  goto cancel;
  if (newSystemImage()) setWindowTitle("New System Image");
  updateWindowStatus();
 cancel:
  imageFile[0] = 0;
  switchToState(mstate);
  EZ_HideWidget(siMenu);
  siMenuPosted = 0;
  return 0;
}

static int onFileOpen(VOID) { 
  int mstate=nState;
  strcpy(imageFile,szBufferFilename);
  fprintf(stderr,"onFileOpen: image %s; state %d\n",imageFile,mstate);
  switchToState(SM_INVALID);
  if (!saveChanges(bAutoSave)) goto cancel; 
  if (szBufferFilename[0] != 0) {
    if (!openSystemImage(szBufferFilename)) emuInfoMessage("Load of system image failed");
    else {
      strcpy(imageFile,szBufferFilename);
      setWindowTitle(imageFile);
    }
  }
 cancel:
  switchToState(mstate);
  return 0;
}

static int onFileSaveAs(VOID) {
  int mstate=nState;

  switchToState(SM_INVALID);

  if (szBufferFilename[0] != 0) {
    strcpy(imageFile,szBufferFilename);
    fprintf(stderr,"onFileSaveAs: image %s; state %d\n",imageFile,mstate);
    saveSystemImageAs(szBufferFilename);
  }
  switchToState(mstate);

  EZ_HideWidget(siMenu);
  siMenuPosted =0;
  return 0;
}

static int onFileClose(VOID) {
  int mstate=nState;

  switchToState(SM_INVALID);
  if (saveChanges(bAutoSave)!= 0) {
    setWindowTitle("Untitled");
    imageFile[0] = 0;         // No file associated with image anymore
  }
  
  switchToState(mstate);

  EZ_HideWidget(siMenu);
  siMenuPosted =0;
  return 0;
}

static void resetCB(EZ_Widget *widget, void *data) {
  if (nState == SM_RUN) {
    switchToState(SM_INVALID);
    systemReset();	
    switchToState(SM_RUN);
  }
}

static EZ_Widget *menus[7][6]; // indexed by nId => File, *H700, *H701, *H720, *H721, *H730, *H740

static void setupMenus() {
  int i;

  menus[0][0] = EZ_CreateWidget(EZ_WIDGET_MENU,      NULL,
			 EZ_MENU_TEAR_OFF, 0,
			 NULL);

  siMenu = menus[0][0];
  
  menus[0][1] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[0][0],
				EZ_LABEL_STRING,               "New",
				EZ_UNDERLINE,                  0,
				EZ_CALLBACK,                   onFileNew, 0,
				NULL);
    
  menus[0][2] =  EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[0][0],
				 EZ_LABEL_STRING,               "Load",
				 EZ_UNDERLINE,                  0,
				 EZ_CALLBACK,                   loadSICB, 0,
				 NULL);

  menus[0][3] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[0][0],
				EZ_LABEL_STRING,               "Save",
				EZ_UNDERLINE,                  0,
				EZ_CALLBACK,                   saveSICB, 0,
				NULL);

  menus[0][4] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[0][0],
				EZ_LABEL_STRING,               "SaveAs",
				EZ_UNDERLINE,                  4,
				EZ_CALLBACK,                   saveAsSICB, 0,
				NULL);

  menus[0][5] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[0][0],
				EZ_LABEL_STRING,               "Close",
				EZ_UNDERLINE,                  0,
				EZ_CALLBACK,                   onFileClose,
				NULL);

  // Led button menus
  for (i=1; i<7 ; i++) {
    menus[i][0] = EZ_CreateWidget(EZ_WIDGET_MENU,      NULL,
				  EZ_MENU_TEAR_OFF, 0,
				  NULL);

    
    menus[i][1] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[i][0],
				  EZ_LABEL_STRING,               "Load",
				  EZ_UNDERLINE,                  0,
				  EZ_CALLBACK,                   loadCB, i,
				  NULL);
    
    menus[i][2] = (i <= 4) ? EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[i][0],
					     EZ_LABEL_STRING,               "Save",
					     EZ_UNDERLINE,                  0,
					     EZ_CALLBACK,                   saveCB, i,
					     NULL) : NULL;

    
    menus[i][3] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[i][0],
				  EZ_LABEL_STRING,               "Eject",
				  EZ_UNDERLINE,                  0,
				  EZ_CALLBACK,                   ejectCB, i,
				  NULL);

    EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[i][0],
		    EZ_LABEL_STRING,               "Cancel",
		    EZ_UNDERLINE,                  0,
		    EZ_CALLBACK,                   cancelCB, i,
		    NULL);
  }
}

static void setupMenuBar(EZ_Widget *mbar) {
  EZ_Widget *frame;

  /* font menu */
 
   seButton  = EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON,  mbar,
			       EZ_WIDTH,        80,
			       EZ_BORDER_WIDTH, 4,
			       EZ_PADX,         6,
			       EZ_BORDER_STYLE, EZ_BORDER_RAISED,
			       EZ_LABEL_STRING, "Settings",
			       EZ_GRID_CELL_GEOMETRY,    0, 0, 1, 1, 
			       EZ_GRID_CELL_PLACEMENT,  EZ_FILL_BOTH, EZ_CENTER,
			       EZ_CALLBACK,             settingsCB, NULL,
			       NULL);
  

  spButton  = EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON,  mbar,
			      EZ_WIDTH, 64,
			      EZ_BORDER_WIDTH, 4,
			      EZ_BORDER_TYPE, EZ_BORDER_RAISED,
			      EZ_GRID_CELL_GEOMETRY,    1, 0, 1, 1, 
			      EZ_GRID_CELL_PLACEMENT,  EZ_FILL_BOTH, EZ_CENTER,
			      EZ_LABEL_STRING,         "Speed",
			      EZ_CALLBACK,             speedMenuCB, NULL,
			      NULL);


  siButton  = EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON,  mbar,
			      EZ_WIDTH, 92,
			      EZ_BORDER_WIDTH, 4,
			      EZ_BORDER_TYPE, EZ_BORDER_RAISED,
			      EZ_GRID_CELL_GEOMETRY,    2, 0, 1, 1, 
			      EZ_GRID_CELL_PLACEMENT,  EZ_FILL_BOTH, EZ_CENTER,
			      EZ_LABEL_STRING,         "SystemImage",
			      EZ_CALLBACK,             siMenuCB, NULL,
			      NULL);

  // EZ_SetSubMenuMenu(siButton,menus[0][0]);

  
  runBtn = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON, mbar,
			   EZ_WIDTH, 64,
			   EZ_BORDER_WIDTH, 4,
			   EZ_BORDER_TYPE, EZ_BORDER_SHADOW3,
			   EZ_INDICATOR_TYPE, EZ_EMPTY_INDICATOR,
			   EZ_LABEL_STRING, "Run",
			   EZ_GRID_CELL_GEOMETRY,    3, 0, 1, 1, 
			   EZ_GRID_CELL_PLACEMENT,  EZ_FILL_BOTH, EZ_CENTER,
			   EZ_CALLBACK,  runCB, NULL,
			   NULL);



  mhzLed = EZ_CreateWidget(EZ_WIDGET_LED, mbar,
			   EZ_LED_WIDTH,          232,
			   EZ_LED_HEIGHT,         8,
			   EZ_BORDER_WIDTH,       4,
			   EZ_BORDER_TYPE,        EZ_BORDER_GROOVE,
			   EZ_LED_PIXEL_SIZE,     1,1,
			   EZ_LABEL_POSITION,     EZ_LEFT,
			   EZ_LABEL_STRING,       " Starting...",
			   EZ_LED_PIXEL_COLOR,    "black",
			   EZ_GRID_CELL_GEOMETRY,  4, 0, 1, 1, 
			   EZ_GRID_CELL_PLACEMENT, EZ_FILL_VERTICALLY, EZ_CENTER,
			   0);
  
  frame  = EZ_CreateWidget(EZ_WIDGET_FRAME,        mbar,
			   EZ_LABEL_STRING,        "FPU",
			   EZ_GRID_CELL_GEOMETRY,  5, 0, 1, 1, 
			   0);

  fpuLed = EZ_CreateWidget(EZ_WIDGET_LED,         frame,
			   EZ_HEIGHT,             16,
			   EZ_WIDTH,              16,
			   EZ_LED_WIDTH,          1,
			   EZ_LED_HEIGHT,         1,
			   EZ_BORDER_WIDTH,       0,
			   EZ_LED_PIXEL_SIZE,     15,15 ,       /* 64 is the largest */
			   EZ_INDICATOR_TYPE,     1,        /* shaded balls    */
			   EZ_BACKGROUND,         "gray74",
			   EZ_LED_PIXEL_COLOR,    "gray20", /* off pixel color */
			   EZ_LED_BACKGROUND, 	  "gray74",
			   NULL);
  

  EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  mbar,
		  EZ_LABEL_STRING,         "Quit",
		  EZ_WIDTH,                64,
		  EZ_BORDER_WIDTH,         4,
		  EZ_BORDER_TYPE,          EZ_BORDER_SHADOW3,
		  EZ_INDICATOR_TYPE,       EZ_EMPTY_INDICATOR,
		  EZ_GRID_CELL_GEOMETRY,   6, 0, 1, 1,
		  EZ_GRID_CELL_PLACEMENT,  EZ_FILL_BOTH, EZ_CENTER,
		  EZ_CALLBACK,             quitCB, NULL,
		  NULL);
}

static int ledId = 1;
EZ_Widget *emuCreateLed(EZ_Widget *frame) {
  EZ_Widget *led;
  led =  EZ_CreateWidget(EZ_WIDGET_LED,         frame,
			 EZ_HEIGHT,             16,
			 EZ_WIDTH,              16,
			 EZ_LED_WIDTH,          1,
			 EZ_LED_HEIGHT,         1,
			 EZ_BORDER_WIDTH,       0,
			 EZ_LED_PIXEL_SIZE,     15,15 ,       /* 64 is the largest */
			 EZ_INDICATOR_TYPE,     0,
			 EZ_BACKGROUND,         "gray74",
			 EZ_LED_PIXEL_COLOR,    "gray20", /* off pixel color */
			 EZ_LED_BACKGROUND,     "gray74",
			 0);

  leds[ledId++] = led;
  return led;
}

static int volId = 0;

static void emuCreateDisk(EZ_Widget *frame, char * diskType, char * hpibAddress, int nunits, int diskNo) {
  EZ_Widget *tmp, *mframe, *lframe;

  char buf[64];

  mframe  = EZ_CreateWidget(EZ_WIDGET_FRAME,      frame,
			    EZ_BORDER_WIDTH,       0,
			    EZ_ORIENTATION, EZ_VERTICAL,
			    EZ_PADY,               0,
			    EZ_LABEL_STRING,    diskType,
			    0);

  diskLabels[diskNo] = mframe;
 
  lframe  = EZ_CreateWidget(EZ_WIDGET_FRAME,      mframe,
			    EZ_BORDER_WIDTH,       0,
			    EZ_ORIENTATION,        EZ_HORIZONTAL,
			    EZ_FILL_MODE,          EZ_FILL_HORIZONTALLY,
			    0);
 
  tmp =   emuCreateLed(lframe);

  tmp   =   EZ_CreateWidget(EZ_WIDGET_LABEL,    lframe,
			    EZ_PADX,            0,
			    EZ_PADY,            0,
			    EZ_BORDERWIDTH,     0,
			    EZ_LABEL_STRING,    hpibAddress,
			    EZ_JUSTIFICATION,    EZ_LEFT,
			    NULL);
  
  for (int i=0;i<nunits;i++) {

    lframe  = EZ_CreateWidget(EZ_WIDGET_FRAME,      mframe,
			      EZ_BORDER_WIDTH,       1,
			      EZ_BORDER_TYPE,        EZ_BORDER_EMBOSSED,
			      EZ_ORIENTATION, EZ_HORIZONTAL,
			      0);
   
    tmp =   emuCreateLed(lframe); 

    sprintf(buf,"Unit %d",i);
   
    tmp   =   EZ_CreateWidget(EZ_WIDGET_MENU_BUTTON,    lframe,
			      EZ_PADX,            0,
			      EZ_PADY,            0,
			      EZ_BORDERWIDTH,     3,
			      EZ_BORDER_TYPE,     EZ_BORDER_RAISED,
			      EZ_LABEL_STRING,    buf,
			      //			     EZ_JUSTIFICATION,    EZ_LEFT,
			      NULL);
    volumeLabels[volId++] = tmp;
    EZ_SetMenuButtonMenu(tmp, menus[volId][0]);
  }

}

static void setupRightHandPanel(EZ_Widget *frame) {
  EZ_Widget*topframe,*hpibframe,*lframe;

  topframe = EZ_CreateWidget(EZ_WIDGET_FRAME,       frame,
			     EZ_BORDER_WIDTH,       0,
			     EZ_PADX,               0,
			     EZ_ORIENTATION,        EZ_VERTICAL,
			     0);


  hpibframe = EZ_CreateWidget(EZ_WIDGET_FRAME,       topframe,
			     EZ_BORDER_WIDTH,       1,
			     EZ_BORDER_TYPE,        EZ_BORDER_FLAT,
			     EZ_FILL_MODE,          EZ_FILL_NONE,
			     EZ_PADX,               0,
			     EZ_ORIENTATION,        EZ_VERTICAL,
			     EZ_LABEL_STRING,       "HPIB",
			     0);

  emuCreateDisk(hpibframe,"hp9121D","Addr:700", 2, 0);
  emuCreateDisk(hpibframe,"hp9122D","Addr:702", 2, 1);
  emuCreateDisk(hpibframe,"hp7908" ,"Addr:703", 1, 2);
  emuCreateDisk(hpibframe,"hp7908" ,"Addr:704", 1, 3);

  lframe  = EZ_CreateWidget(EZ_WIDGET_FRAME,      topframe,
			    EZ_BORDER_WIDTH,       0,
			    EZ_LABEL_STRING,       "CpuStatus",
			    0);

  cpuStatus = EZ_CreateWidget(EZ_WIDGET_LED,         lframe,
			      EZ_LED_WIDTH,          8,
			      EZ_LED_HEIGHT,         1,
			      EZ_BORDER_WIDTH,       2,
			      EZ_BORDER_TYPE,        EZ_BORDER_FLAT,
			      EZ_LED_PIXEL_SIZE,     12,12,
			      EZ_FOREGROUND,         "green",
			      NULL);
  
  EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON, topframe,
		  EZ_PADX,           0,
		  EZ_PADY,           0,
		  EZ_LABEL_STRING,   "Reset",
		  EZ_FOREGROUND,     "Red",
		  EZ_CALLBACK,        resetCB, NULL,
		  NULL );
  
}

void setWindowTitle(char *Title) {
  EZ_ConfigureWidget(toplevel, EZ_WM_WINDOW_NAME, Title, 0);
}

static void enableMenuItem(int menuId, int state) {
  // fprintf(stderr,"enableMenuItem: %d:%d state: %d\n",menuId/10, menuId%10,state);
  if (state) EZ_EnableWidget(menus[menuId/10][menuId%10]);
  else       EZ_DisableWidget(menus[menuId/10][menuId%10]);
}

VOID updateWindowStatus(VOID)  {
  if (hWnd) {					// window open
    BOOL bRun         = nState == SM_RUN;
    UINT uRun         = bRun ? 1 : 0;

    enableMenuItem(ID_FILE_NEW, 1);
    enableMenuItem(ID_FILE_OPEN, 1);
    enableMenuItem(ID_FILE_SAVE, (bRun && imageFile[0]));
    enableMenuItem(ID_FILE_SAVEAS, uRun);
    enableMenuItem(ID_FILE_CLOSE, (bRun && imageFile[0]));
    EZ_SetCheckButtonState(runBtn,uRun);

    for (int i=0; i<volId; i++)
      if (bRun) EZ_EnableWidget(volumeLabels[i]);
      else      EZ_DisableWidget(volumeLabels[i]);

    // HPIB 700 drives
    if (Chipset.Hpib700 != 1) {
      enableMenuItem(ID_D700_LOAD,  0);
      enableMenuItem(ID_D700_SAVE,  0);
      enableMenuItem(ID_D700_EJECT, 0);
      enableMenuItem(ID_D701_LOAD,  0);
      enableMenuItem(ID_D701_SAVE,  0);
      enableMenuItem(ID_D701_EJECT, 0);
    } else {
      enableMenuItem(ID_D700_LOAD,  (Chipset.Hp9121.disk[0] == NULL));
      enableMenuItem(ID_D700_SAVE,  (Chipset.Hp9121.disk[0] != NULL));
      enableMenuItem(ID_D700_EJECT, (Chipset.Hp9121.disk[0] != NULL));
      enableMenuItem(ID_D701_LOAD,  (Chipset.Hp9121.disk[1] == NULL));
      enableMenuItem(ID_D701_SAVE,  (Chipset.Hp9121.disk[1] != NULL));
      enableMenuItem(ID_D701_EJECT, (Chipset.Hp9121.disk[1] != NULL));
    }
    
    // HPIB 701 is printer
    
    // HPIB 702 drives
    if (Chipset.Hpib702 != 3) {
      enableMenuItem(ID_D720_LOAD,  0);
      enableMenuItem(ID_D720_SAVE,  0);
      enableMenuItem(ID_D720_EJECT, 0);
      enableMenuItem(ID_D721_LOAD,  0);
      enableMenuItem(ID_D721_SAVE,  0);
      enableMenuItem(ID_D721_EJECT, 0);
    } else {
      enableMenuItem(ID_D720_LOAD,  (Chipset.Hp9122.disk[0] == NULL));
      enableMenuItem(ID_D720_SAVE,  (Chipset.Hp9122.disk[0] != NULL));
      enableMenuItem(ID_D720_EJECT, (Chipset.Hp9122.disk[0] != NULL));
      enableMenuItem(ID_D721_LOAD,  (Chipset.Hp9122.disk[1] == NULL));
      enableMenuItem(ID_D721_SAVE,  (Chipset.Hp9122.disk[1] != NULL));
      enableMenuItem(ID_D721_EJECT, (Chipset.Hp9122.disk[1] != NULL));
    }

    // HPIB 703 drive
    if (Chipset.Hpib703 == 0) {
      enableMenuItem(ID_H730_LOAD,  0);
      enableMenuItem(ID_H730_EJECT, 0);
    } else {
      enableMenuItem(ID_H730_LOAD,  (Chipset.Hp7908_0.disk[0] == NULL));
      enableMenuItem(ID_H730_EJECT, (Chipset.Hp7908_0.disk[0] != NULL));
    }
    // HPIB 704 drive
    if (Chipset.Hpib704 == 0) {
      enableMenuItem(ID_H740_LOAD,  0);
      enableMenuItem(ID_H740_EJECT, 0);
    } else {
      enableMenuItem(ID_H740_LOAD,  (Chipset.Hp7908_1.disk[0] == NULL));
      enableMenuItem(ID_H740_EJECT, (Chipset.Hp7908_1.disk[0] != NULL));
    }
  }
}

//
// Mouse wheel handler
//
static int onMouseWheel(UINT clicks, WORD x, WORD y) {
#ifdef DEBUG_KEYBOARDW
  k = wsprintf(buffer,_T("      : Wheel : wP %08X lP %04X%04X\n"), nFlags, x, y);
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif
  KnobRotate(clicks);
  return 0;
}


// for key up/down 
static int onKey(int down, unsigned int keycode, int state) {
  int kc = keycode & 0x7F;

  // fprintf(stderr,"in    : kc %02x %d %02x %d\n", kc, down, state&0xf, Chipset.Keyboard.shift);

  if (nState == SM_RUN) {
    // first time down (for autorepeat) ??
   
    switch ((BYTE) kc) {
    case 0x32: // left shift
    case 0x3E: // right shift
      if (down & !(state & LockMask)) Chipset.Keyboard.shift = 1;
      else Chipset.Keyboard.shift = 0;
      break;
    case 0x25: // left ctrl
    case 0x69: // right ctrl
      Chipset.Keyboard.ctrl = down;  break;
    case 0x40: // alt
      Chipset.Keyboard.alt = down;   break;
    default:
      Chipset.Keyboard.shift = ((state & ShiftMask) != 0);
      if (Chipset.Keyboard.shift && (state & LockMask)) Chipset.Keyboard.shift = 0;
      if (state &ControlMask) Chipset.Keyboard.ctrl = down;
      if (state &Mod1Mask)    Chipset.Keyboard.alt  = down;
      if (down) KeyboardEventDown((BYTE) kc);
      else      KeyboardEventUp((BYTE) kc);  
    } 
  }
  //  fprintf(stderr,"out   : kc %02x %d %02x %d\n", kc, down, state&0xf, Chipset.Keyboard.shift);

  return 0;
}

//
// annunciator button handler
//
 VOID buttonEvent(BOOL state, UINT nId, int x, int y) {
   //  fprintf(stderr,"Button event: state %d, Id %d\n",state,nId);
  if (state == TRUE) return;
  if (nId >= 2 && nId <=7)  EZ_DoPopup(menus[nId-1][0], EZ_BOTTOM_RIGHT);
}

int main(int ac, char **av) {
  EZ_Widget *frame, *mbar;

  XInitThreads();

  EZ_Initialize(ac, av, 0);

  toplevel  = EZ_CreateWidget(EZ_WIDGET_FRAME,   NULL,
			      EZ_PADX,           0,
			      EZ_PADY,           0,
			      EZ_FILL_MODE,      EZ_FILL_HORIZONTALLY,
			      EZ_ORIENTATION, EZ_VERTICAL,
			      EZ_WM_WINDOW_NAME, "hp9816emu",
			      0);

  mbar =  EZ_CreateWidget(EZ_WIDGET_FRAME, toplevel,
			  EZ_PADX,                0,
			  EZ_PADY,                0,
			  EZ_GRID_CONSTRAINS,    EZ_ROW,      1,     300,  10,   0, 
			  EZ_GRID_CONSTRAINS,    EZ_COLUMN,   4,    200,  10,   0, 
			  NULL);

  setupMenus();

  setupSettingsMenu();
  
  setupSpeedMenu();
  
  setupMenuBar(mbar);

  setupFileSelector();

  frame = EZ_CreateWidget(EZ_WIDGET_FRAME,   toplevel,
			  EZ_PADX,           0,
			  EZ_PADY,           0,
			  EZ_BORDER_TYPE,    EZ_BORDER_SUNKEN,
			  EZ_BORDER_WIDTH,   8,
			  EZ_FILL_MODE,      EZ_FILL_NONE,
			  EZ_ORIENTATION,    EZ_HORIZONTAL,
			  0);

  
  workArea = EZ_CreateWidget(EZ_WIDGET_RAW_XWINDOW, frame,
			     EZ_WIDTH,  800,
			     EZ_HEIGHT, 600,
			     //	     EZ_WINDOW_DEPTH, 8,
			     EZ_SIDE, EZ_LEFT,
			     0);

  EZ_SetupRawXWinWidget(workArea,
			hp9816emuComputeSize,
			hp9816emuDraw,
			hp9816emuFreeData,
			hp9816emuEventHandler);


  setupRightHandPanel(frame);

  dpy    = EZ_GetDisplay();            /* display   */

  
  /* EZ_Timer *mTimer = Timer for Mhz and mem display update */
  EZ_CreateTimer(1,0,-1,timerCB,toplevel,0);

  EZ_DisplayWidget(frame);

  emuSetEvents(toplevel);
  EZ_AddEventHandler(toplevel, passThruEventHandler, NULL, 0);
  emuSetEvents(mbar);
  EZ_AddEventHandler(mbar, passThruEventHandler, NULL, 0);
  emuSetEvents(siButton);
  EZ_AddEventHandler(siButton, passThruEventHandler, NULL, 0);
  emuSetEvents(seButton);
  EZ_AddEventHandler(seButton, passThruEventHandler, NULL, 0);
  emuSetEvents(spButton);
  EZ_AddEventHandler(spButton, passThruEventHandler, NULL, 0);
  emuSetEvents(runBtn);
  EZ_AddEventHandler(runBtn, passThruEventHandler, NULL, 0);
  EZ_AddEventHandler(workArea, passThruEventHandler, NULL, 0);
  nState     = SM_RUN;			// init state must be <> nNextState
  nNextState = SM_INVALID;		// go into invalid state
  pthread_create(&cpuThread,NULL,&cpuEmulator,NULL);

  
  while (nState!=nNextState) usleep(10000);  // wait for thread initialized

  if (ac == 2)	{			// use decoded parameter line
    if (openSystemImage(av[1])) {
      setWindowTitle(av[1]);
      strcpy(imageFile,av[1]);
      hp9816emuDraw(workArea);
      switchToState(SM_RUN); 
      goto start;
    }
  }

  //  goto start;

  if (!newSystemImage()) { 
    fprintf(stderr,"Failed to create new system image\n");
    exit(1);
  }

  setWindowTitle("New system image"); 

 start:
  EZ_SetFocusTo(workArea);// Send keyboard events to emulator
  EZ_EventMainLoop();
}

