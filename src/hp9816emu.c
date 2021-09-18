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
#include "common.h"
#include "hp9816emu.h"
#include "kml.h"

/* Globals */

BOOL        bAutoSave = FALSE;
BOOL        bAutoSaveOnExit = FALSE;
BOOL        bAlwaysDisplayLog = TRUE;
int         bFPU = TRUE;
int         bRamInd = 1; // 512K
#define     MEM_SZ_CNT 6
// Memory sizes in KB last entry 8MB-512KB
int         memSizes[MEM_SZ_CNT] = {256, 512, 1024, 2048, 4096, 7680}; 
static char memDisp[64];
int         bKeeptime = TRUE;
pthread_t   cpuThread = 0;
Window      hWnd=0;

static int OnKey(int,unsigned int, int);
static int OnMouseWheel(UINT nFlags, WORD x, WORD y);
static int OnLButtonDown(UINT nFlags, WORD x, WORD y);
static int OnLButtonUp(UINT nFlags, WORD x, WORD y);
static int OnFileSaveAs(VOID);
static void emuSetEvents(EZ_Widget *);

static EZ_Widget *toplevel = NULL;     /* toplevel frame           */
static EZ_Widget *emulator = NULL;     /* vt emulator              */
static EZ_Widget *flistBox = NULL;     /* font lister              */
static EZ_Widget *fileSelector = NULL; /* image selector           */
/* Settings menu */
static EZ_Widget *settingsMenu = NULL; /* general settings menu    */
static EZ_Widget *seButton = NULL;     /* settings button          */
static EZ_Widget *memSel;              /* memory size selector     */
static EZ_Widget *fpuBtn;
static EZ_Widget *timBtn;
/* Speed menu */
static EZ_Widget *spButton = NULL;     /* speed selector button    */
static EZ_Widget *speedMenu = NULL;    /* speed setting menu       */
static int        speedMenuPosted = 0; /* speed menu is posted flag*/
static EZ_Widget *speedButton[6];      /* speed selection buttons  */
/* System image menu */
static EZ_Widget *siButton = NULL;     /* system immage button     */
static EZ_Widget *siMenu   = NULL;     /* system immage menu       */
static EZ_Widget *siName;              /* system image name widget */
static int        siMenuPosted = 0;    /* sys img menu posted flag */
static char       imageFile[256];      /* image file name          */
/* Run button */
static EZ_Widget *runBtn;

/* Display variables */
static EZ_Widget *workArea;            /* the 9816 display area    */
static int        emuInit = 0, panelInit = 0;
static Display   *dpy;                 /* the display */
static GC         drawGC;
static XGCValues  gcvalues;

#define LARGE_FONT "-adobe-helvetica-bold-r-normal-*-*-100-*-*-*-*-*-*"

void emuFlush() {
  XSync(dpy,0);
}

static int oldop = -1; 
void emuBitBlt(Pixmap dst, int dx0, int dy0, int dw, int dh, Pixmap src, int sx0, int sy0, int op) {
  if (op != oldop) {
    oldop = op;
    XSetFunction(dpy, drawGC, op );
  }
  XCopyArea(dpy, src, dst, drawGC, sx0, sy0, dw, dh, dx0, dy0);
}


void emuPatBlt(Pixmap dst, int dx0, int dy0, int dw, int dh, int op) {
  if (op != oldop) {
    oldop = op;
    XSetFunction(dpy, drawGC, op );
  }
  XFillRectangle(dpy, dst, drawGC,  dx0, dy0, dw, dh);
}


void emuPutImage(Pixmap dst,int x0, int y0, int w, int h, XImage *src, int op) {	    
  if (op != oldop) {
    oldop = op;
    XSetFunction(dpy, drawGC, op );
  }
  XPutImage(dpy, dst, drawGC, src, x0, y0, x0, y0, w, h);
}


Pixmap emuCreateBitMap(int w, int h) {
  Pixmap imagePixmap=0;
  //fprintf(stderr,"emuCreateBitMap %d %d\n",w,h);
  imagePixmap = XCreatePixmap(dpy, hWnd, w, h, EZ_GetDepth());
  return imagePixmap;
}

Pixmap emuLoadBitMap(LPCTSTR fn) {
  unsigned int w,h;
  Pixmap imagePixmap=0;
  //fprintf(stderr,"emuLoadImage %s\n",fn);
  EZ_CreateXPixmapFromImageFile((char *)fn, &w, &h, &imagePixmap);
  return imagePixmap;
}

void emuFreeBitMap(Pixmap bm) {
  XFreePixmap(dpy,bm);
}

XImage *emuCreateImage(int x, int y) {
  char *img;
  img = malloc(x*y*4);
  return XCreateImage(dpy, EZ_GetVisual() ,EZ_GetDepth(), ZPixmap,
		      0, img, x, y, 32, 0);
}

void emuFreeImage(XImage *img) {
  XDestroyImage(img);
}
void emuSetWidgetPosition(EZ_Widget *parent, EZ_Widget *move, int x, int y) {
  // really move relative to parent
  int px,py,pw,ph;
  EZ_GetWidgetAbsoluteGeometry(parent, &px, &py, &pw, &ph);
  fprintf(stderr,"emuSetWidgetPosition px %d, py %d, pw %d, ph %d\n",px,py,pw,ph);
  EZ_MoveWidgetWindow(move,x+px+pw,y+py+ph);
  emuFlush();
}

static void hp9816emuComputeSize(EZ_Widget *widget, int *w, int *h) {
  /* this procedure is invoked by the geometry manager to
   * figure out the minimal size.
   */
  *w = bgW; *h = bgH;  
  printf("hp9816emuComputeSize %d %d\n",bgW,bgH);
}

static void hp9816emuDraw(EZ_Widget *widget) {
  RECT rc;

  rc.left = rc.top = 0;
  rc.bottom = bgH;
  rc.right  = bgW;
  if (!emuInit) {
    hWnd = EZ_GetWidgetWindow(widget); /* widget window */
    XMapWindow(dpy, hWnd);
    emuSetEvents(widget);
    emuInit = 1;
    return;
  }
  if (hMainBM) { /* background image loaded */
    // redraw background image
    emuBitBlt(hWnd, 0, 0, bgW, bgH, hMainBM, 0, 0, GXcopy );
    // redraw main display area
    Refresh_Display(TRUE);
    hpib_names();
    UpdateAnnunciators(TRUE); 
    RefreshButtons(&rc);
  }
}

static void hp9816emuFreeData(EZ_Widget *widget) {
}
				 
static void hp9816emuEventHandler(EZ_Widget *widget, XEvent *event) {
  int x,y;
  if (!event) return;
  switch (event->type) {
  case Expose: hp9816emuDraw(widget); break;
  case ButtonPress:
    x = event->xbutton.x;
    y = event->xbutton.y;
    //fprintf(stderr,"Button press %d\n",event->xbutton.button);
    switch (event->xbutton.button) {
    case Button1: OnLButtonDown(1,x,y);  EZ_SetFocusTo(workArea); break;
    case Button4: OnMouseWheel(-1,x,y); break;
    case Button5: OnMouseWheel( 1,x,y); break;
    default:;
    }
    break;
  case ButtonRelease:
    x = event->xbutton.x;
    y = event->xbutton.y;
  //fprintf(stderr,"Button Release %d\n",event->xbutton.button);
    switch (event->xbutton.button) {
    case Button1: OnLButtonUp(1,x,y); break;
    case Button4: OnMouseWheel(-1,x,y); break;
    case Button5: OnMouseWheel( 1,x,y); break;
    default:;
    }
    break;
  case KeyPress:   OnKey(1, event->xkey.keycode, event->xkey.state); break;
  case KeyRelease: OnKey(0, event->xkey.keycode, event->xkey.state); break;
  default:
    // fprintf(stderr,"Unexpected event type %d\n",event->type);
  }
  return;
  //hp9816emuDraw(widget);
  //EZ_CallWidgetMotionCallbacks(widget);
}

static void passThruEventHandler(EZ_Widget *w, void *d, int etype, XEvent *event) {
  if ((event->type == KeyPress) || (event->type == KeyRelease)) {
    hp9816emuEventHandler(workArea, event);
    //    fprintf(stderr,"ptevh dropping event\n");
    EZ_RemoveEvent(event);
  } else {
    // fprintf(stderr,"ptevh calling next\n");
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
    SwitchToState(SM_RETURN);
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
    if (pbyRom) {
      SwitchToState(SM_RUN); 
      EZ_SetFocusTo(workArea);// Send keyboard events to emulator
    }	
    else EZ_SetCheckButtonState(w,EZ_CHECK_BUTTON_ON_OFF);
  } else {
    if (nState != SM_RUN) return;
    SwitchToState(SM_INVALID);
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
 EZ_Widget *jnk, *ok, *filter, *cancel, *scaleBtn;
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
  EZ_Widget *parent = data;
  EZ_HideWidget(parent);
}

void settingsCB(EZ_Widget *w, void *data) {
  EZ_SetCheckButtonState(fpuBtn,bFPU);
  EZ_SetCheckButtonState(timBtn,bKeeptime);
  EZ_DisplayWidget(settingsMenu);
  emuSetWidgetPosition(seButton, settingsMenu,-32,0);
  EZ_RaiseWidgetWindow(settingsMenu);
}

void speedMenuCB(EZ_Widget *w, void *data) {
  if (speedMenuPosted) {
     EZ_HideWidget(speedMenu);
     speedMenuPosted = 0;
     return;
  }
  EZ_DisplayWidget(speedMenu);
  emuSetWidgetPosition(spButton, speedMenu,-32,0);
  EZ_RaiseWidgetWindow(speedMenu);
  speedMenuPosted = 1;
}

void speedCB(EZ_Widget *w, void *data) {
  int speed = EZ_GetRadioButtonGroupVariableValue(w);
  fprintf(stderr,"speedCB %d\n",speed);
  SetSpeed(speed);
  wRealSpeed = speed;
  EZ_HideWidget(speedMenu);
  speedMenuPosted = 0;
}

void siMenuCB(EZ_Widget *w, void *data) {
  if (siMenuPosted) {
     EZ_HideWidget(siMenu);
     siMenuPosted = 0;
  } else {
    EZ_DisplayWidget(siMenu);
    emuSetWidgetPosition(siButton, siMenu,-32,0);
    EZ_RaiseWidgetWindow(siMenu);
    siMenuPosted = 1;
  }
}

static void genCB(EZ_Widget *w, void *d) {
  int s;
  long b = (long) d;
  EZ_GetCheckButtonState(w,&s);
  switch (b) {
  case 0: bAutoSave = s; break;
  case 1: bAutoSaveOnExit; break;
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
}

static void cancelCB(EZ_Widget *widget, void *data) {
  EZ_Widget *w =(EZ_Widget *)data; 
  EZ_HideWidget(w);
  EZ_DestroyWidget(w);
}

void emuInfoMessage(char *str) {

  EZ_Widget  *toplevel, *frame, *label, *restart, *quit;	      

  toplevel = EZ_CreateWidget(EZ_WIDGET_FRAME,   NULL,
			     EZ_LABEL_STRING,   " ",
			     EZ_TEXT_LINE_LENGTH, 30,
			     EZ_JUSTIFICATION,    EZ_CENTER,
			     EZ_TRANSIENT,       EZ_TRUE,
			     EZ_IPADY,           10,
			     EZ_ORIENTATION,     EZ_VERTICAL,
			     EZ_FILL_MODE,       EZ_FILL_BOTH,
			     NULL);

  label  =   EZ_CreateWidget(EZ_WIDGET_LABEL,    toplevel,
			     EZ_LABEL_STRING,    str,
			     EZ_TEXT_LINE_LENGTH, 60,
			     EZ_JUSTIFICATION,    EZ_CENTER,
			     EZ_FOREGROUND,         "red",
			     EZ_LABEL_SHADOW,       1,1,
			     EZ_FONT_NAME,         LARGE_FONT,
			     NULL);
  
  frame =    EZ_CreateWidget(EZ_WIDGET_FRAME,   toplevel,
			     EZ_HEIGHT,         0,
			     NULL);
  

  quit =     EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON, frame,
			     EZ_LABEL_STRING,   "OK",
			     EZ_UNDERLINE,       0,
			     EZ_CALLBACK,        cancelCB, toplevel,
			     NULL );

  EZ_DisplayWidgetUnderPointer(toplevel, -10, -10);
  EZ_SetGrab(toplevel);
  EZ_SetFocusTo(quit);
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
  EZ_Widget *tmp, *memFrame, *genFrame, *fpuFrame;

  settingsMenu = EZ_CreateWidget(EZ_WIDGET_FRAME,         NULL,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_TRANSIENT,            EZ_TRUE,
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  memFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         settingsMenu,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_GROOVE,
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  tmp          = EZ_CreateWidget(EZ_WIDGET_LABEL,         memFrame,
				 EZ_LABEL_STRING,         "Memory Size",
				 EZ_JUSTIFICATION,        EZ_LEFT,
				 NULL);

  memSel       = EZ_CreateWidget(EZ_WIDGET_SPIN_BUTTON,   memFrame,
				 EZ_SPIN_VALUE,           bRamInd, "512 KB",
				 EZ_SPIN_FUNCTION,        selectMem, NULL,
				 NULL);
  
  genFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         settingsMenu,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_GROOVE,
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  tmp          = EZ_CreateWidget(EZ_WIDGET_LABEL,         genFrame,
				 EZ_LABEL_STRING,         "General Settings",
				 EZ_JUSTIFICATION,        EZ_LEFT,
				 NULL);
  
  timBtn       = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  genFrame,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Set system time",
				 EZ_CALLBACK,             timCB, 0,
				 0);
  
  tmp          = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  genFrame,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Auto save system image",
				 EZ_CALLBACK,             genCB, 0,
				 0);

  tmp          = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  genFrame,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Auto save on exit",
				 EZ_CALLBACK,             genCB, 1,
				 0);

  fpuFrame     = EZ_CreateWidget(EZ_WIDGET_FRAME,         settingsMenu,
				 EZ_ORIENTATION,          EZ_VERTICAL_TOP,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_GROOVE,
				 EZ_SIDE,                 EZ_LEFT,
				 NULL);

  tmp          = EZ_CreateWidget(EZ_WIDGET_LABEL,         fpuFrame,
				 EZ_LABEL_STRING,         "HP98635 Floating point card",
				 EZ_JUSTIFICATION,        EZ_LEFT,
				 NULL);

  fpuBtn       = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  fpuFrame,
				 EZ_BORDER_WIDTH,         4,
				 EZ_BORDER_TYPE,          EZ_BORDER_RAISED,
				 EZ_LABEL_STRING,         "Enabled",
				 EZ_CALLBACK,             fpuCB, 0,
				 0);
		      
  tmp          = EZ_CreateWidget(EZ_WIDGET_NORMAL_BUTTON, settingsMenu,
				 EZ_LABEL_STRING,         "OK",
				 EZ_UNDERLINE,            0,
				 EZ_CALLBACK,             okCB, settingsMenu,
				 NULL );

}

//
// ID_9122_LOAD
//
static int On9122Load(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to load a Disk.");
    goto cancel;
  }
  if (hp9122_widle(ctrl)) {						// wait for idle hp9122 state
    emuInfoMessage(_T("The HP9122 is busy."));
    goto cancel;
  } 
  hp9122_load(ctrl, byUnit, szBufferFilename);
  UpdateWindowStatus();
 cancel:
  return 0;
}

//
// ID_9122_SAVE
//
static int On9122Save(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage(_T("The emulator must be running to save a Disk."));
    goto cancel;
  }
  if (hp9122_widle(ctrl))	{			// wait for idle hp9122 state
    emuInfoMessage(_T("The HP9122 is busy."));
    goto cancel;
  }
  _ASSERT(ctrl->name[byUnit][0] != 0x00);
  if (!GetSaveDiskFilename(ctrl->name[byUnit]))
    goto cancel;
  hp9122_save(ctrl, byUnit, szBufferFilename);
 cancel:
  return 0;
}

//
// ID_9122_EJECT
//
static int On9122Eject(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage(_T("The emulator must be running to eject a Disk."));
    goto cancel;
  }
  if (hp9122_widle(ctrl))	{			// wait for idle hp9122 state
    emuInfoMessage(_T("The HP9122 is busy."));
    goto cancel;
  }
  hp9122_eject(ctrl, byUnit);
  UpdateWindowStatus();
 cancel:
  return (0);
}

//
// ID_9121_LOAD
//
static int On9121Load(HP9121 *ctrl, BYTE byUnit) {	// load an image to a disc unit

  if (nState != SM_RUN) {
    emuInfoMessage(_T("The emulator must be running to load a Disk."));
    goto cancel;
  }
  if (hp9121_widle(ctrl))	{			// wait for idle hp9121 state
    emuInfoMessage(_T("The HP9121 is busy."));
    goto cancel;
  } 
  hp9121_load(ctrl, byUnit, szBufferFilename);
  UpdateWindowStatus();
 cancel:
  return 0;
}

//
// ID_9121_SAVE
//
static int On9121Save(HP9121 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage(_T("The emulator must be running to save a Disk."));
    goto cancel;
  }
  if (hp9121_widle(ctrl))	{			// wait for idle hp9122 state
    emuInfoMessage(_T("The HP9121 is busy."));
    goto cancel;
  }
  _ASSERT(ctrl->name[byUnit][0] != 0x00);
  if (!GetSaveDiskFilename(ctrl->name[byUnit]))
    goto cancel;
  hp9121_save(ctrl, byUnit, szBufferFilename);
 cancel:
  return 0;
}

//
// ID_9121_EJECT
//
static int On9121Eject(HP9121 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage(_T("The emulator must be running to eject a Disk."));
    goto cancel;
  }
  if (hp9121_widle(ctrl)) {    // wait for idle hp9121 state
    emuInfoMessage(_T("The HP9121 is busy."));
    goto cancel;
  }
  hp9121_eject(ctrl, byUnit);
  UpdateWindowStatus();
 cancel:
  return (0);
}

//
// ID_7908_LOAD
//
static int On7908Load(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to load a Disk.");
    goto cancel;
  }
  if (hp7908_widle(ctrl))	{	       	// wait for idle hp7908 state
    emuInfoMessage(_T("The HP7908 is busy."));
    goto cancel;
  } 
  hp7908_load(ctrl, byUnit, szBufferFilename);
  UpdateWindowStatus();
 cancel:
  return 0;
}

//
// ID_7908_SAVE
//
static int On7908Save(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage(_T("The emulator must be running to save a Disk."));
    goto cancel;
  }
  if (hp7908_widle(ctrl))	{			// wait for idle hp7908 state
    emuInfoMessage(_T("The HP7908 is busy."));
    goto cancel;
  }
  _ASSERT(ctrl->name[byUnit][0] != 0x00);
  if (!GetSaveDiskFilename(ctrl->name[byUnit]))
    goto cancel;
  hp7908_save(ctrl, byUnit, szBufferFilename);
 cancel:
  return 0;
}

//
// ID_7908_EJECT
//
static int On7908Eject(HPSS80 *ctrl, BYTE byUnit) {

  if (nState != SM_RUN) {
    emuInfoMessage("The emulator must be running to eject a Disk.");
    goto cancel;
  }
  if (hp9122_widle(ctrl))	{			// wait for idle hp7908 state
    emuInfoMessage("The HP7908 is busy.");
    goto cancel;
  }
  hp7908_eject(ctrl, byUnit);
  UpdateWindowStatus();
 cancel:
  return (0);
}

static void doLoadCB(EZ_Widget *w, void *d) {
  long munit = (long) d;
  fprintf(stderr,"loadCB %ld\n",munit);
  switch(munit) {
  case 1: On9121Load(&Chipset.Hp9121, 0); break;
  case 2: On9121Load(&Chipset.Hp9121, 1); break;
  case 3: On9122Load(&Chipset.Hp9122, 0); break;
  case 4: On9122Load(&Chipset.Hp9122, 1); break;
  case 5: On7908Load(&Chipset.Hp7908_0, 0); break;
  case 6: On7908Load(&Chipset.Hp7908_1, 0); break;
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
  fprintf(stderr,"saveCB %ld\n", munit);
  switch(munit) {
  case 1: On9121Save(&Chipset.Hp9121, 0); break;
  case 2: On9121Save(&Chipset.Hp9121, 1); break;
  case 3: On9122Save(&Chipset.Hp9122, 0); break;
  case 4: On9122Save(&Chipset.Hp9122, 1); break;
    //  case 5: On7908Save(&Chipset.Hp7908_0, 0); break;
    //  case 6: On7908Save(&Chipset.Hp7908_1, 0); break;
  default: fprintf(stderr,"Invalid unit\n");
  }
}

static void ejectCB(EZ_Widget *w, void *d) {
  long munit = (long) d;
  fprintf(stderr,"ejectCB %ld\n", munit);
   switch(munit) {
  case 1: On9121Eject(&Chipset.Hp9121, 0); break;
  case 2: On9121Eject(&Chipset.Hp9121, 1); break;
  case 3: On9122Eject(&Chipset.Hp9122, 0); break;
  case 4: On9122Eject(&Chipset.Hp9122, 1); break;
  case 5: On7908Eject(&Chipset.Hp7908_0, 0); break;
  case 6: On7908Eject(&Chipset.Hp7908_1, 0); break;
  default: fprintf(stderr,"Invalid unit\n");
  }
}

static UINT SaveChanges(BOOL bAuto) {
  char buf[1024];
  if (pbyRom == NULL) return 0;
  if (!bAuto) return 1;
  if (!imageFile[0]) {
    strcpy(imageFile,"system.img");
    snprintf(buf, 512, "Saving image in default file: %s\n",imageFile);
    emuInfoMessage(buf);
  }
  fprintf(stderr, "Saving system image in: %s\n",imageFile);
  if (SaveSystemImageAs(imageFile)) {
    SetWindowTitle(imageFile);
    return 1;
  } else {
    emuInfoMessage("Failed to save image");
    imageFile[0] = 0; // kill bad file name
    return 0;
  }
}

static void loadSICB(EZ_Widget *w, void *d) {
  EZ_ConfigureWidget(fileSelector,
		     EZ_CALLBACK, setBufferFilename, (void *)OnFileOpen,
		     EZ_CLIENT_INT_DATA, (long)d,
		     NULL);
  EZ_SetFileSelectorInitialPattern(fileSelector, "*.img");
  showFileSelector(w,d);
}

static void saveSICB(EZ_Widget *w, void *data) {
  int mstate=nState;
  if (pbyRom == NULL) return;
  SwitchToState(SM_INVALID);
  SaveChanges(TRUE);
  SwitchToState(mstate);
  return;
}

static void saveAsSICB(EZ_Widget *w, void *data) {
  if (pbyRom == NULL) return;
  EZ_ConfigureWidget(fileSelector,
		     EZ_CALLBACK, setBufferFilename, (void *)OnFileSaveAs,
		     EZ_CLIENT_INT_DATA, (long)data,
		     NULL);
  EZ_SetFileSelectorInitialPattern(fileSelector, "*.img");
  showFileSelector(w, data);
  return;
}

static int OnFileNew(VOID) {
  int mstate=nState;
  if (pbyRom) {
    SwitchToState(SM_INVALID);
    if (0 == SaveChanges(bAutoSave))
      goto cancel;
  }
  if (NewSystemImage()) SetWindowTitle("New System Image");
  UpdateWindowStatus();
 cancel:
  imageFile[0] = 0;
  if (pbyRom) SwitchToState(mstate);
  EZ_HideWidget(siMenu);
  siMenuPosted =0;
  return 0;
}

static int OnFileOpen(VOID) { 
  int mstate=nState;
  strcpy(imageFile,szBufferFilename);
  fprintf(stderr,"OnFileOpen: image %s; state %d\n",imageFile,mstate);
  if (pbyRom) {
    SwitchToState(SM_INVALID);
     if (0 == SaveChanges(bAutoSave))
       goto cancel; 
  } 
  if (szBufferFilename[0] != 0) {
    if (!OpenSystemImage(szBufferFilename)) emuInfoMessage("Load of system image failed");
    else {
      strcpy(imageFile,szBufferFilename);
      SetWindowTitle(imageFile);
    }
  }
 cancel:
  if (pbyRom) SwitchToState(mstate);
  EZ_HideWidget(siMenu);
  siMenuPosted =0;
  return 0;
}

static int OnFileSaveAs(VOID) {
  int mstate=nState;
  if (pbyRom == NULL) goto cancel;
  
  SwitchToState(SM_INVALID);

  if (szBufferFilename[0] != 0) {
    strcpy(imageFile,szBufferFilename);
    fprintf(stderr,"OnFileSaveAs: image %s; state %d\n",imageFile,mstate);
    SaveSystemImageAs(szBufferFilename);
  }
  SwitchToState(mstate);
 cancel:
  EZ_HideWidget(siMenu);
  siMenuPosted =0;
  return 0;
}

static int OnFileClose(VOID) {
  int mstate=nState;
  if (pbyRom == NULL) goto cancel;
  SwitchToState(SM_INVALID);
  if (SaveChanges(bAutoSave)!= 0) {
    SetWindowTitle("Untitled");
    imageFile[0] = 0;         // No file associated with image anymore
  }
  
  SwitchToState(mstate);

 cancel:
  EZ_HideWidget(siMenu);
  siMenuPosted =0;
  return 0;
}

static EZ_Widget *menus[7][6]; // indexed by nId => File, *H700, *H701, *H720, *H721, *H730, *H740

static void setupMenus() {
  EZ_Widget *menu, *tmp;
  int i;

  menus[0][0] = EZ_CreateWidget(EZ_WIDGET_MENU,      NULL,
			 EZ_MENU_TEAR_OFF, 0,
			 NULL);

  siMenu = menus[0][0];
  
  menus[0][1] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[0][0],
				EZ_LABEL_STRING,               "New",
				EZ_UNDERLINE,                  0,
				EZ_CALLBACK,                   OnFileNew, 0,
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
				EZ_CALLBACK,                   OnFileClose,
				NULL);

  // Annunciator button menus
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
					     NULL) :
      NULL;

    
    menus[i][3] = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[i][0],
				  EZ_LABEL_STRING,               "Eject",
				  EZ_UNDERLINE,                  0,
				  EZ_CALLBACK,                   ejectCB, i,
				  NULL);

    tmp  = EZ_CreateWidget(EZ_WIDGET_MENU_NORMAL_BUTTON,  menus[i][0],
			   EZ_LABEL_STRING,               "Cancel",
			   EZ_UNDERLINE,                  0,
			   EZ_CALLBACK,                   cancelCB, i,
			   NULL);
  }
}

static void setupMenuBar(EZ_Widget *mbar) {
  EZ_Widget *menu, *tmp, *mitem, *fgmenu, *bgmenu;
  int i = 0;

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

  tmp    = EZ_CreateWidget(EZ_WIDGET_CHECK_BUTTON,  mbar,
			   EZ_LABEL_STRING,         "Quit",
			   EZ_WIDTH,                64,
			   EZ_BORDER_WIDTH,         4,
			   EZ_BORDER_TYPE,          EZ_BORDER_SHADOW3,
			   EZ_INDICATOR_TYPE,       EZ_EMPTY_INDICATOR,
			   EZ_GRID_CELL_GEOMETRY,   9, 0, 1, 1,
			   EZ_GRID_CELL_PLACEMENT,  EZ_FILL_BOTH, EZ_CENTER,
			   EZ_CALLBACK,             quitCB, NULL,
			   NULL);
    
}

void SetWindowTitle(char *Title) {
  EZ_ConfigureWidget(toplevel, EZ_WM_WINDOW_NAME, Title, 0);
}

static void EnableMenuItem(int menuId, int state) {
  // fprintf(stderr,"EnableMenuItem: %d:%d state: %d\n",menuId/10, menuId%10,state);
  if (state) EZ_EnableWidget(menus[menuId/10][menuId%10]);
  else       EZ_DisableWidget(menus[menuId/10][menuId%10]);
}

VOID UpdateWindowStatus(VOID)  {
  if (hWnd) {					// window open
    BOOL bRun         = nState == SM_RUN;
    UINT uRun         = bRun ? 1 : 0;

    EnableMenuItem(ID_FILE_NEW, 1);
    EnableMenuItem(ID_FILE_OPEN, 1);
    EnableMenuItem(ID_FILE_SAVE, (bRun && imageFile[0]));
    EnableMenuItem(ID_FILE_SAVEAS, uRun);
    EnableMenuItem(ID_FILE_CLOSE, (bRun && imageFile[0]));
    EZ_SetCheckButtonState(runBtn,uRun);


    // HPIB 700 drives
    if (Chipset.Hpib70x != 1) {
      EnableMenuItem(ID_D700_LOAD,  0);
      EnableMenuItem(ID_D700_SAVE,  0);
      EnableMenuItem(ID_D700_EJECT, 0);
      EnableMenuItem(ID_D701_LOAD,  0);
      EnableMenuItem(ID_D701_SAVE,  0);
      EnableMenuItem(ID_D701_EJECT, 0);
    } else {
      EnableMenuItem(ID_D700_LOAD,  (Chipset.Hp9121.disk[0] == NULL));
      EnableMenuItem(ID_D700_SAVE,  (Chipset.Hp9121.disk[0] != NULL));
      EnableMenuItem(ID_D700_EJECT, (Chipset.Hp9121.disk[0] != NULL));
      EnableMenuItem(ID_D701_LOAD,  (Chipset.Hp9121.disk[1] == NULL));
      EnableMenuItem(ID_D701_SAVE,  (Chipset.Hp9121.disk[1] != NULL));
      EnableMenuItem(ID_D701_EJECT, (Chipset.Hp9121.disk[1] != NULL));
    }
    
    // HPIB 701 is printer
    
    // HPIB 702 drives
    if (Chipset.Hpib72x != 3) {
      EnableMenuItem(ID_D720_LOAD,  0);
      EnableMenuItem(ID_D720_SAVE,  0);
      EnableMenuItem(ID_D720_EJECT, 0);
      EnableMenuItem(ID_D721_LOAD,  0);
      EnableMenuItem(ID_D721_SAVE,  0);
      EnableMenuItem(ID_D721_EJECT, 0);
    } else {
      EnableMenuItem(ID_D720_LOAD,  (Chipset.Hp9122.disk[0] == NULL));
      EnableMenuItem(ID_D720_SAVE,  (Chipset.Hp9122.disk[0] != NULL));
      EnableMenuItem(ID_D720_EJECT, (Chipset.Hp9122.disk[0] != NULL));
      EnableMenuItem(ID_D721_LOAD,  (Chipset.Hp9122.disk[1] == NULL));
      EnableMenuItem(ID_D721_SAVE,  (Chipset.Hp9122.disk[1] != NULL));
      EnableMenuItem(ID_D721_EJECT, (Chipset.Hp9122.disk[1] != NULL));
    }

    // HPIB 703 drive
    if (Chipset.Hpib73x != 1) {
      EnableMenuItem(ID_H730_LOAD,  0);
      EnableMenuItem(ID_H730_EJECT, 0);
    } else {
      EnableMenuItem(ID_H730_LOAD,  (Chipset.Hp7908_0.disk[0] == NULL));
      EnableMenuItem(ID_H730_EJECT, (Chipset.Hp7908_0.disk[0] != NULL));
    }
    // HPIB 704 drive
    if (Chipset.Hpib74x != 1) {
      EnableMenuItem(ID_H740_LOAD,  0);
      EnableMenuItem(ID_H740_EJECT, 0);
    } else {
      EnableMenuItem(ID_H740_LOAD,  (Chipset.Hp7908_1.disk[0] == NULL));
      EnableMenuItem(ID_H740_EJECT, (Chipset.Hp7908_1.disk[0] != NULL));
    }
  }
}

//
// ID_VIEW_RESET
//
static int OnViewReset(VOID) {
  if (nState == SM_RUN) {
    SwitchToState(SM_INVALID);
    SystemReset();			// Chipset setting after power cycle
    SwitchToState(SM_RUN);
  }
  return 0;
}

//
// Mouse wheel handler
//
static int OnMouseWheel(UINT clicks, WORD x, WORD y) {
#ifdef DEBUG_KEYBOARDW
  k = wsprintf(buffer,_T("      : Wheel : wP %08X lP %04X%04X\n"), nFlags, x, y);
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif
  KnobRotate(clicks);
  return 0;
}

// 
// Keyboard handler
//
static int OnLButtonDown(UINT nFlags, WORD x, WORD y) {
  if (nState == SM_RUN) MouseButtonDownAt(nFlags, x,y); 
  return 0;
}

static int OnLButtonUp(UINT nFlags, WORD x, WORD y) {
  if (nState == SM_RUN) MouseButtonUpAt(nFlags, x,y); 
  return 0;
}

// for key up/down 
static int OnKey(int down, unsigned int keycode, int state) {
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
 VOID ButtonEvent(BOOL state, UINT nId, int x, int y) {
   //  fprintf(stderr,"Button event: state %d, Id %d\n",state,nId);
  if (state == TRUE) return;
  if (nId == 1) OnViewReset();
  else if (nId >= 2 && nId <=7)  EZ_DoPopup(menus[nId-1][0], EZ_BOTTOM_RIGHT);
}

int main(int ac, char **av) {
  Pixmap     pixmap,shape;
  EZ_Widget *frame, *mbar;
  int        w,h;
  char      *value;

  XInitThreads();

  EZ_Initialize(ac, av, 0);

  toplevel  = EZ_CreateWidget(EZ_WIDGET_FRAME,   NULL,
			   // EZ_SIZE,           864, 640,
			   EZ_PADX,           0,
			   EZ_PADY,           0,
			   EZ_FILL_MODE,      EZ_FILL_BOTH,
			   EZ_ORIENTATION, EZ_VERTICAL_TOP,
			   EZ_WM_WINDOW_NAME, "hp9816emu",
			   0);

  mbar =  EZ_CreateWidget(EZ_WIDGET_FRAME, toplevel,
			  EZ_PADX,                0,
			  EZ_PADY,                0,
			  EZ_GRID_CONSTRAINS,    EZ_ROW,      1,     300,  10,   0, 
			  EZ_GRID_CONSTRAINS,    EZ_COLUMN,   8,    200,  10,   0, 
			  NULL);

  setupMenus();

  setupSettingsMenu();
  
  setupSpeedMenu();
  
  setupMenuBar(mbar);

  setupFileSelector();

  frame = EZ_CreateWidget(EZ_WIDGET_FRAME,   toplevel,
			  EZ_SIZE,           864+16,600+16,
			  EZ_PADX,           0,
			  EZ_PADY,           0,
			  EZ_BORDER_TYPE,    EZ_BORDER_SUNKEN,
			  EZ_BORDER_WIDTH,   8,
			  EZ_FILL_MODE,      EZ_FILL_BOTH,
			  EZ_ORIENTATION,    EZ_HORIZONTAL_LEFT,
			  0);

  workArea = EZ_CreateWidget(EZ_WIDGET_RAW_XWINDOW, frame,
			     EZ_WIDTH,  864,
			     EZ_HEIGHT, 600,
			     //	     EZ_WINDOW_DEPTH, 8,
			     EZ_SIDE, EZ_LEFT,
			     0);

  EZ_SetupRawXWinWidget(workArea,
			hp9816emuComputeSize,
			hp9816emuDraw,
			hp9816emuFreeData,
			hp9816emuEventHandler);


  dpy    = EZ_GetDisplay();            /* display   */
  drawGC = EZ_GetGC(0L,&gcvalues);

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
  pthread_create(&cpuThread,NULL,&CpuEmulator,NULL);

  while (nState!=nNextState) usleep(10000);  // wait for thread initialized

  if (ac == 2)	{			// use decoded parameter line
    if (OpenSystemImage(av[1])) {
      SetWindowTitle(av[1]);
      strcpy(imageFile,av[1]);
      hp9816emuDraw(workArea);
      SwitchToState(SM_RUN); 
      goto start;
    }
  }

  if (NewSystemImage()) {
    SetWindowTitle("New system image");
    goto start;
  }

  exit(1);

 start:
  EZ_SetFocusTo(workArea);// Send keyboard events to emulator
  EZ_EventMainLoop();
}

