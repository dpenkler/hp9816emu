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


#define	ARRAYSIZEOF(a)	(sizeof(a) / sizeof(a[0]))
#define	_KB(a)			((a)*1024)\
  
#define HARDWARE "Chipmunk" // emulator hardware

// hp9816emu.c
extern int      bKeeptime; // set system time
extern int      bFPU;      // hp98635 floating point card enabled
extern int      bRamInd;   // Ram size index
extern Window	hWnd;
		
extern pthread_t  cpuThread;			// system thread
extern void	SetWindowTitle(LPTSTR szString);
extern void	UpdateWindowStatus(VOID);
extern void	ButtonEvent(BOOL state, UINT nId, int x, int y);
extern Pixmap   emuCreateBitMap(int w, int h);
extern void     emuFreeBitMap(Pixmap);
extern XImage  *emuCreateImage(int w, int h);
extern void     emuFreeImage(XImage *);
extern Pixmap   emuLoadBitMap(LPCTSTR Name);
extern void     emuBitBlt(Pixmap Dst, int, int, int, int,
			  Pixmap Src, int, int,
			  int op);
extern void     emuPatBlt(Pixmap Dst, int, int, int, int, int op);
extern void     emuPutImage(Pixmap dst,int x0, int y0, int w, int h, XImage *src, int op);
extern void     emuInfoMessage(char *);
extern void     emuFlush();
static int      OnFileOpen(VOID);

// Display.c
extern UINT	bgX;
extern UINT	bgY;
extern UINT	bgW;
extern UINT	bgH;
extern UINT	scrX;
extern UINT	scrY;

extern Pixmap	hFontBM;	// alpha font
extern Pixmap	hMainBM;	// main bitmap for border display
extern Pixmap	hAlpha1BM;	// display alpha 1
extern Pixmap	hAlpha2BM;	// display alpha 2 (for blink)
extern XImage  *hGraphImg;	// display graph
extern Pixmap	hScreenBM;      // main window bitmap

extern VOID	UpdateClut(VOID); // colour lookup table
extern VOID	SetScreenColor(UINT nId, UINT nRed, UINT nGreen, UINT nBlue);
extern VOID	SetGraphColor(BYTE col);
extern VOID	CreateScreenBitmap(VOID);
extern VOID	DestroyScreenBitmap(VOID);
extern BOOL	CreateMainBitmap(LPCTSTR szFilename);
extern VOID	DestroyMainBitmap(VOID);
extern VOID	UpdateAnnunciators(BOOL bForce);
extern VOID	Refresh_Display(BOOL bforce);	// refresh altered part of display at vsync
extern VOID	UpdateMainDisplay(BOOL bforce);	// refresh altered part of display at vsync
extern VOID	Reload_Graph(VOID);		// reload graph bitmap from graph mem
extern VOID	UpdateClut(VOID);

// Display16.c
extern BYTE	Write_Display16(BYTE *a, WORD d, BYTE s);	// for alpha part
extern BYTE	Read_Display16(BYTE *a, WORD d, BYTE s);
extern BYTE	Write_Graph16(BYTE *a, WORD d, BYTE s);		// for graph part
extern BYTE	Read_Graph16(BYTE *a, WORD d, BYTE s);
extern VOID	Refresh_Display16(BOOL bforce);			// refresh altered part of display at vsync
extern VOID	UpdateMainDisplay16(BOOL force);
extern VOID	do_display_timers16(VOID);
extern VOID	Reload_Graph16(VOID);								

// cpu9816.c
extern BOOL	bInterrupt;
extern UINT	nState;
extern UINT	nNextState;
extern WORD	wRealSpeed;
//extern BOOL	bSound;			// no sound yet
extern long  	dwMaxTimers;
extern WORD	wCyclesTimer;
extern SYSTEM	Chipset;
extern WORD	wInstrSize;
extern WORD	wInstrWp;
extern WORD	wInstrRp;
extern long	dwVsyncRef;
extern VOID	SetSpeed(WORD wAdjust);
extern UINT	SwitchToState(UINT nNewState);
extern void    *CpuEmulator(void *);
extern void	SetHPTime(VOID);

// Fetch.c
extern VOID		EvalOpcode(BYTE newI210);
extern VOID		initOP(VOID);

// Files.c
extern TCHAR	szCurrentDirectory[MAX_PATH];
extern TCHAR	szCurrentKml[MAX_PATH];
extern TCHAR	szCurrentFilename[MAX_PATH];
extern TCHAR	szBufferFilename[MAX_PATH];
extern TCHAR	szRomFileName[MAX_PATH];
extern LPBYTE	pbyRom;
extern LPBYTE	pbyRomPage[256];
extern UINT	nCurrentRomType;
extern UINT	nCurrentClass;
extern BOOL	bBackup;
extern VOID	SetWindowLocation(Window hWnd,INT nPosX,INT nPosY);
extern BOOL	PatchRom(LPCTSTR szFilename);
extern BOOL	MapRom(LPCTSTR szRomDirectory);
extern VOID	UnmapRom(VOID);
extern VOID	ResetSystemImage(VOID);
extern BOOL	NewSystemImage(VOID);
extern BOOL	OpenSystemImage(LPCTSTR szFilename);
extern BOOL	SaveSystemImage(VOID);
extern BOOL	SaveSystemImageAs(LPCTSTR szFilename);
extern BOOL	SaveBackup(VOID);
extern BOOL	RestoreBackup(VOID);
extern BOOL	ResetBackup(VOID);
extern BOOL	GetOpenFilename(VOID);
extern BOOL	GetSaveAsFilename(VOID);
extern BOOL	GetLoadLifFilename(VOID);
extern BOOL	GetSaveLifFilename(VOID);
extern BOOL	GetSaveDiskFilename(LPCTSTR lpstrName);
extern BOOL	LoadLif(LPCTSTR szFilename);
extern BOOL	SaveLif(LPCTSTR szFilename);
extern BOOL	LoadDisk(LPCTSTR szFilename);
extern BOOL	SaveDisk(LPCTSTR szFilename);
extern Pixmap	LoadBitmapFile(LPCTSTR szFilename);

// Hp-ib.c
extern BYTE Write_HPIB(BYTE *a, WORD d, BYTE s);// write in HPIB I/O space
extern BYTE Read_HPIB(BYTE *a, WORD d, BYTE s);	// read in HPIB I/O space
extern VOID DoHPIB(VOID);			// fsm for HPIB controller
extern VOID hpib_names(VOID);			// update names of HIB instruments
extern BOOL hpib_init_bus(VOID);		// to initialize peripherals on bus 
extern BOOL hpib_stop_bus(VOID);		// to stop peripherals on bus
extern VOID hpib_init(VOID);			// to initialize peripherals functions
extern BOOL h_push(BYTE b, BYTE st);		// called by peripheral to reply on bus

//Hp-9121.c	
extern VOID DoHp9121(HP9121 *ctrl);		// fsm for HP9121 disk unit
extern BOOL hp9121_eject(HP9121 *ctrl, BYTE unit);
extern BOOL hp9121_save(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9121_load(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9121_widle(HP9121 *ctrl);
extern VOID hp9121_reset(VOID *controler);
extern VOID hp9121_stop(VOID *controler);
extern BOOL hp9121_push_c(VOID *controler, BYTE c);
extern BOOL hp9121_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-9122.c
extern VOID DoHp9122(HPSS80 *ctrl);		// fsm for HP9122 disk unit
extern BOOL hp9122_eject(HPSS80 *ctrl, BYTE unit);
extern BOOL hp9122_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9122_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp9122_widle(HPSS80 *ctrl);
extern VOID hp9122_reset(VOID *controler);
extern VOID hp9122_stop(VOID *controler);
extern BOOL hp9122_push_c(VOID *controler, BYTE c);
extern BOOL hp9122_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-7908.c
extern VOID DoHp7908(HPSS80 *ctrl);		// fsm for HP7908 hard disk unit
extern BOOL hp7908_eject(HPSS80 *ctrl, BYTE unit);
extern BOOL hp7908_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp7908_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename);
extern BOOL hp7908_widle(HPSS80 *ctrl);
extern VOID hp7908_reset(VOID *controler);
extern VOID hp7908_stop(VOID *controler);
extern BOOL hp7908_push_c(VOID *controler, BYTE c);
extern BOOL hp7908_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-2225.c
extern VOID DoHp2225(HP2225 *ctrl);		// fsm for HP2225 like printer
extern BOOL hp2225_eject(HP2225 *ctrl);
extern BOOL hp2225_save(HP2225 *ctrl);
extern BOOL hp2225_widle(HP2225 *ctrl);
extern VOID hp2225_reset(VOID *controler);
extern VOID hp2225_stop(HP2225 *ctrl);
extern BOOL hp2225_push_c(VOID *controler, BYTE c);
extern BOOL hp2225_push_d(VOID *controler, BYTE d, BYTE eoi);

//Hp-98635.c
extern BYTE Write_98635(BYTE *a, WORD d, BYTE s);	// write in HP98635 I/O space
extern BYTE Read_98635(BYTE *a, WORD d, BYTE s);	// read in HP98635 I/O space

//Hp-98626.c
extern BYTE Write_98626(BYTE *a, WORD d, BYTE s);	// write in HP98626 I/O space
extern BYTE Read_98626(BYTE *a, WORD d, BYTE s);	// read in HP98626 I/O space

// Mops.c
extern VOID SystemReset(VOID);				// reset system as power on
extern WORD GetWORD(DWORD d);				// read a word in RAM/ROM for external purpose
extern BYTE ReadMEM(BYTE *a, DWORD d, BYTE s);		// read on system bus 
extern BYTE WriteMEM(BYTE *a, DWORD d, BYTE s);		// write on system bus

// opcodes.c
extern VOID decode_op(WORD op, OP *ope);                 // decode 'op' mc68000 opcode

// Keyboard.c
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


// Missing Win32 API calls
static __inline LPTSTR DuplicateString(LPCTSTR szString) {
	UINT   uLength = strlen(szString) + 1;
	LPTSTR szDup   = malloc(uLength*sizeof(szDup[0]));
	strcpy(szDup,szString);
	return szDup;
}

