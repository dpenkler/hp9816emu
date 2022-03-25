/*
 *   hp9816emu.h
 * 
 *	 General definitions of functions
 *
 *   Copyright 2010-2011 Olivier De Smet
 *             2021      Dave Penkler
 */

#include "cpu9816.h"
#include "opcodes.h"

#define SM_RUN		0	// states of cpu emulation thread
#define SM_INVALID	1
#define SM_RETURN	2

#define HARDWARE "Chipmunk" // emulator hardware

// hp9816emu.c
extern int      bKeeptime; // set system time
extern int      bFPU;      // hp98635 floating point card enabled
extern int      bPhosphor;
extern int      bRamInd;   // Ram size index
extern int      memSizes[];
extern int 	bSpeed;    // speed index
extern int      bErrno;    // file access error code
extern volatile unsigned int cpuCycles;
extern Window	hWnd;
extern Display *dpy;

extern pthread_t  cpuThread;		// system thread
extern pthread_t  sndThread;            // sound thread
extern void *sndMonitor(void *);        // start function for sndThread

extern void	setWindowTitle(LPTSTR szString);
extern void	updateWindowStatus(VOID);
extern void	buttonEvent(BOOL state, UINT nId, int x, int y);
extern Pixmap   emuCreateBitMap(int w, int h);
extern void     emuFreeBitMap(Pixmap);
extern XImage  *emuCreateImage(int w, int h);
extern XImage  *emuCreateImageFromBM(int w, int h, char *img);
extern void     emuFreeImage(XImage *);
extern Pixmap   emuLoadBitMap(LPCTSTR Name);
extern void     emuBitBlt(Pixmap Dst, int, int, int, int,
			  Pixmap Src, int, int,
			  int op);
extern void     emuPatBlt(Pixmap Dst, int, int, int, int, int op);
extern Pixmap   emuCreateBitMapFromImage(int w, int h, XImage *xim);
extern XImage  *emuGetImage(Pixmap bm, int w, int h);
extern void     emuImgBlt(Pixmap dst, GC gc, int dx0, int dy0, int w, int h, XImage *src, int sx0, int sy0, int op);
extern void     emuPutImage(Pixmap dst, int dx0, int dy0, int w, int h, XImage *src, int op);
extern void     emuSetColours(unsigned int fg, unsigned int bg);
extern void     emuInfoMessage(char *);
extern void     emuUpdateButton(int hpibAddr, int unit, char * lifVolume);
extern void     emuUpdateDisk(int, char *);
extern void     emuUpdateLed(int,int);
extern void     emuFlush();

// Display.c
extern UINT	bgX;
extern UINT	bgY;
extern UINT	bgW;
extern UINT	bgH;
extern UINT	scrX;
extern UINT	scrY;

extern unsigned short fontBM[];  // actual bitmap
extern Pixmap	hAlpha1BM;	// display alpha 1
extern Pixmap	hAlpha2BM;	// display alpha 2 (for blink)
extern XImage  *hGraphImg;	// display graph
extern Pixmap	hScreenBM;      // main window bitmap

extern VOID     mkFontBM(int);
extern VOID     testFont();
extern VOID	setGraphColor(BYTE col);
extern VOID	createScreenBitmap(VOID);
extern VOID	destroyScreenBitmap(VOID);
extern BOOL	createMainBitmap(LPCTSTR szFilename);
extern VOID	destroyMainBitmap(VOID);
extern VOID	updateLeds(BOOL bForce);
extern VOID	refreshDisplay(BOOL bforce);	// refresh altered part of display at vsync
extern VOID	updateAlpha(BOOL bforce);	// refresh altered part of display at vsync
extern VOID	reloadGraph(VOID);		// reload graph bitmap from graph mem
extern BYTE	readDisplay(BYTE *a, WORD d, BYTE s);
extern BYTE	writeDisplay(BYTE *a, WORD d, BYTE s);  // write into display memory
extern BYTE	readAlpha(BYTE *a, WORD d, BYTE s);
extern BYTE	readGraph(BYTE *a, WORD d, BYTE s);
extern BYTE	writeGraph(BYTE *a, WORD d, BYTE s);	// for graph part
extern VOID	doDisplayTimers(VOID);

// cpu9816.c
extern BOOL	bInterrupt;
extern UINT	nState;
extern UINT	nNextState;
//extern BOOL	bSound;			// no sound yet
extern long  	dwMaxTimers;
extern WORD	wCyclesTimer;
extern SYSTEM	Chipset;
extern WORD	wInstrSize;
extern WORD	wInstrWp;
extern WORD	wInstrRp;
extern long	dwVsyncRef;
extern VOID	setSpeed(WORD wAdjust);
extern UINT	switchToState(UINT nNewState);
extern void    *cpuEmulator(void *);
extern void	setHPTime(VOID);
extern int       checkChipset();
// fetch.c
extern VOID	EvalOpcode(BYTE newI210);
extern VOID	initOP(VOID);

// files.c
extern TCHAR	szCurrentDirectory[MAX_PATH];
extern TCHAR	szCurrentFilename[MAX_PATH];
extern TCHAR	szBufferFilename[MAX_PATH];
extern VOID	SetWindowLocation(Window hWnd,INT nPosX,INT nPosY);
extern VOID	resetSystemImage(VOID);
extern BOOL	newSystemImage(VOID);
extern BOOL	openSystemImage(LPCTSTR szFilename);
extern BOOL	saveSystemImage(VOID);
extern BOOL	saveSystemImageAs(LPCTSTR szFilename);

// hp-ib.c
extern BYTE Write_HPIB(BYTE *a, WORD d, BYTE s);// write in HPIB I/O space
extern BYTE Read_HPIB(BYTE *a, WORD d, BYTE s);	// read in HPIB I/O space
extern VOID DoHPIB(VOID);			// fsm for HPIB controller
extern VOID hpib_names(VOID);			// update names of HIB instruments
extern BOOL hpib_init_bus(VOID);		// to initialize peripherals on bus 
extern BOOL hpib_stop_bus(VOID);		// to stop peripherals on bus
extern VOID hpib_init(VOID);			// to initialize peripherals functions
extern BOOL h_push(BYTE b, BYTE st);		// called by peripheral to reply on bus

// hp-9121.c	
extern VOID DoHp9121(HP9121 *ctrl);		// fsm for HP9121 disk unit
extern BOOL hp9121_eject(HP9121 *ctrl, BYTE unit);
extern BOOL hp9121_save(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9121_load(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9121_widle(HP9121 *ctrl);
extern VOID hp9121_reset(VOID *controler);
extern VOID hp9121_stop(VOID *controler);
extern BOOL hp9121_push_c(VOID *controler, BYTE c);
extern BOOL hp9121_push_d(VOID *controler, BYTE d, BYTE eoi);

// hp-9122.c
extern VOID DoHp9122(HPSS80 *ctrl);		// fsm for HP9122 disk unit
extern BOOL hp9122_eject(HPSS80 *ctrl, BYTE unit);
extern BOOL hp9122_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9122_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9122_widle(HPSS80 *ctrl);
extern VOID hp9122_reset(VOID *controler);
extern VOID hp9122_stop(VOID *controler);
extern BOOL hp9122_push_c(VOID *controler, BYTE c);
extern BOOL hp9122_push_d(VOID *controler, BYTE d, BYTE eoi);

// hp-7908.c
extern VOID DoHp7908(HPSS80 *ctrl);		// fsm for HP7908 hard disk unit
extern BOOL hp7908_eject(HPSS80 *ctrl, BYTE unit);
extern BOOL hp7908_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp7908_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp7908_widle(HPSS80 *ctrl);
extern VOID hp7908_reset(VOID *controler);
extern VOID hp7908_stop(VOID *controler);
extern BOOL hp7908_push_c(VOID *controler, BYTE c);
extern BOOL hp7908_push_d(VOID *controler, BYTE d, BYTE eoi);

// hp-2225.c
extern VOID DoHp2225(HP2225 *ctrl);		// fsm for HP2225 like printer
extern BOOL hp2225_eject(HP2225 *ctrl);
extern BOOL hp2225_save(HP2225 *ctrl);
extern BOOL hp2225_widle(HP2225 *ctrl);
extern VOID hp2225_reset(VOID *controler);
extern VOID hp2225_stop(HP2225 *ctrl);
extern BOOL hp2225_push_c(VOID *controler, BYTE c);
extern BOOL hp2225_push_d(VOID *controler, BYTE d, BYTE eoi);

// hp-98635.c
extern BYTE Write_98635(BYTE *a, WORD d, BYTE s);	// write in HP98635 I/O space
extern BYTE Read_98635(BYTE *a, WORD d, BYTE s);	// read in HP98635 I/O space

// hp-98626.c
extern BYTE Write_98626(BYTE *a, WORD d, BYTE s);	// write in HP98626 I/O space
extern BYTE Read_98626(BYTE *a, WORD d, BYTE s);	// read in HP98626 I/O space

// mops.c
extern VOID systemReset(VOID);				// reset system as power on
extern WORD GetWORD(DWORD d);				// read a word in RAM/ROM for external purpose
extern BYTE ReadMEM(BYTE *a, DWORD d, BYTE s);		// read on system bus 
extern BYTE WriteMEM(BYTE *a, DWORD d, BYTE s);		// write on system bus

// opcodes.c
extern VOID decode_op(WORD op, OP *ope);                 // decode 'op' mc68000 opcode

// keyboard.c
extern VOID  KnobRotate(SWORD knob);			// mouse wheel is knob
extern VOID KeyboardEventDown(BYTE nId);
extern VOID KeyboardEventUp(BYTE nId);
//extern BOOL		WriteToKeyboard(LPBYTE lpData, DWORD dwSize);
extern BYTE Write_Keyboard(BYTE *a, WORD d, BYTE s);
extern BYTE Read_Keyboard(BYTE *a, WORD d, BYTE s);
extern VOID Do_Keyboard_Timers(DWORD cycles);		// do keyboard timers
extern VOID Do_Keyboard(VOID);				// do keyboard stuff
extern VOID Reset_Keyboard(VOID);			// at cold boot
extern VOID Init_Keyboard(VOID);			// at re-load sys image

// sound.c
extern void sound_init();
extern void sound_close();
extern void emuBeep(int f, int d);

#define ID_FILE_NEW    01  
#define ID_FILE_OPEN   02
#define ID_FILE_SAVE   03
#define ID_FILE_SAVEAS 04
#define ID_FILE_CLOSE  05

#define ID_D700_LOAD   11
#define ID_D700_SAVE   12
#define ID_D700_EJECT  13
#define ID_D701_LOAD   21
#define ID_D701_SAVE   22
#define ID_D701_EJECT  23
              
#define ID_D720_LOAD   31 
#define ID_D720_SAVE   32
#define ID_D720_EJECT  33
#define ID_D721_LOAD   41
#define ID_D721_SAVE   42 
#define ID_D721_EJECT  43
              
#define ID_H730_LOAD   51 
#define ID_H730_EJECT  53
              
#define ID_H740_LOAD   61 
#define ID_H740_EJECT  63
