/*
 *   Files.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   All stuff for files and system images, still some stuff from emu42
//

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"
#include "rom.h"


//#define DEBUG_FILES
#if defined(DEBUG_FILES)
static TCHAR buffer[256];
static int k;
#endif

TCHAR  szCurrentDirectory[MAX_PATH];
TCHAR  szCurrentFilename[MAX_PATH];
TCHAR  szBufferFilename[MAX_PATH];

// System image signatures
#define SignatureLength 32
static BYTE pbySignature [SignatureLength] = "hp9816emu Configuration V1.0";
static int hCurrentFile = -1;

//################
//#
//#    Low level subroutines
//#
//################


//
// New document settings
//
static BOOL newSettingsProc() {

#if defined(DEBUG_FILES)
  k = wsprintf(buffer,"newSettingsProc\n");
  OutputDebugString(buffer);
#endif

  Chipset.RamSize = memSizes[bRamInd]*1024;	        // RAM size
  Chipset.RamStart = 0x01000000 - Chipset.RamSize;	// from ... to 0x00FFFFFF
  
  
  Chipset.Hpib700 = 1;	// type of disk 9121
  Chipset.Hpib701 = 1;	// printer ?
  Chipset.Hpib702 = 3;	// type of disk 9122
  Chipset.Hpib703 = 1;	// type of disk 7908
  Chipset.Hpib704 = 1;	// type of disk 7908
    
  hpib_init();		// initialize it
  
  setSpeed(wRealSpeed);	// set speed
  
  Chipset.keeptime = bKeeptime;
  
  Chipset.Keyboard.keymap = 1;
  
  Chipset.Hp98635 = bFPU;
  
  return TRUE;
}



//################
//#
//#    Public functions
//#
//################


//
//   System images
//
VOID resetSystemImage(VOID) {

#if defined(DEBUG_FILES)
  k = wsprintf(buffer,"resetSystemImage\n");
  OutputDebugString(buffer);
#endif

  hpib_stop_bus();

  if (hCurrentFile>=0) {
    close(hCurrentFile);
    hCurrentFile = -1;
  }
  szCurrentFilename[0]=0;
  if (Chipset.Ram)  {
    free(Chipset.Ram);
    Chipset.Ram = NULL;
  }
  bzero(&Chipset,sizeof(Chipset));

  Chipset.Rom = (LPBYTE)rom30;
  Chipset.RomSize = sizeof(rom30);

#if defined(DEBUG_FILES)
  k = wsprintf(buffer,"CS %ld, %ld\n",sizeof(Chipset),sizeof(rom30));
  OutputDebugString(buffer);
#endif
  
  destroyScreenBitmap();

  createScreenBitmap();

  updateWindowStatus();
}

BOOL newSystemImage(VOID) {

#if defined(DEBUG_FILES)
  k = wsprintf(buffer,"newSystemImage\n");
  OutputDebugString(buffer);
#endif


  resetSystemImage();

  newSettingsProc(); // initial settings

  // allocate memory for RAM
  if (Chipset.Ram == NULL) {
    Chipset.Ram = (LPBYTE)malloc(Chipset.RamSize);
  }
#if defined(DEBUG_FILES)
  k = wsprintf(buffer,"RamSize %d\n",Chipset.RamSize);
  OutputDebugString(buffer);
#endif

  systemReset();

  return TRUE;
}

BOOL openSystemImage(LPCTSTR szFilename) {
  INT   hFile = -1;
  DWORD lBytesRead,lSizeofChipset;
  BYTE  pbyFileSignature[SignatureLength];
  UINT  ctBytesCompared;

  resetSystemImage();

  hFile = open(szFilename, O_RDWR);
  if (hFile < 0) {
    perror("This file is missing or already loaded in another instance of Hp98x6.");
    goto restore;
  }

  // Read and Compare signature
  lBytesRead = read(hFile, pbyFileSignature, sizeof(pbyFileSignature));
  for (ctBytesCompared=0; ctBytesCompared<sizeof(pbyFileSignature); ctBytesCompared++) {
    if (pbyFileSignature[ctBytesCompared]!=pbySignature[ctBytesCompared]) {
      emuInfoMessage("This file is not a valid hp9816 configuration.");
      goto restore;
    }
  }

  // read chipset size inside file
  lBytesRead = read(hFile, &lSizeofChipset, sizeof(lSizeofChipset));
  if (lBytesRead != sizeof(lSizeofChipset)) goto read_err;
  if (lSizeofChipset <= sizeof(Chipset)) {	// actual or older chipset version
    // read chipset content
    bzero(&Chipset,sizeof(Chipset)); // init chipset
    read(hFile, &Chipset, lSizeofChipset);
  } else goto read_err;

  Chipset.Ram = (LPBYTE)malloc(Chipset.RamSize);
  if (Chipset.Ram == NULL) {
    fprintf(stderr,"RAM Memory Allocation Failure.");
    goto restore;
  }
  
  lBytesRead = read(hFile, Chipset.Ram, Chipset.RamSize);
  if (lBytesRead != Chipset.RamSize) goto read_err;

  // clean pointers

  Chipset.Hp9121.disk[0] = NULL;
  Chipset.Hp9121.disk[1] = NULL;
  Chipset.Hp9122.disk[0] = NULL;
  Chipset.Hp9122.disk[1] = NULL;

  Chipset.Hp7908_0.hdisk[0] = -1;
  Chipset.Hp7908_0.hdisk[1] = -1;
  Chipset.Hp7908_1.hdisk[0] = -1;
  Chipset.Hp7908_1.hdisk[1] = -1;

  Chipset.Hp2225.hfile = -1;

  hp9122_reset(&Chipset.Hp9122);	// reload Disk if needed
  hp9121_reset(&Chipset.Hp9121);	// reload Disk if needed
  hp7908_reset(&Chipset.Hp7908_0);	// reload Disk if needed
  hp7908_reset(&Chipset.Hp7908_1);	// reload Disk if needed

  hp2225_reset(&Chipset.Hp2225);	// reset printer

  hpib_init();

  Init_Keyboard();

  setSpeed(wRealSpeed);	// set speed
  
  strcpy(szCurrentFilename, szFilename);
  hCurrentFile = hFile;

  reloadGraph();
  updateAlpha(TRUE);	// refresh screen alpha, graph & cmap
  hpib_names();
  updateLeds(TRUE);
  return TRUE;

 read_err:
  emuInfoMessage("This file must be truncated, and cannot be loaded.");
 restore:
  if (hFile != -1) close(hFile);
  return FALSE;
}

BOOL saveSystemImage(VOID) {
  DWORD           lBytesWritten;
  DWORD           lSizeofChipset;
  UINT            nFSize=0;

  if (hCurrentFile == -1) return FALSE;

  //  wndpl.length = sizeof(wndpl);			// update saved window position
  //  GetWindowPlacement(hWnd, &wndpl);
  //  Chipset.nPosX = (SWORD) wndpl.rcNormalPosition.left;
  //  Chipset.nPosY = (SWORD) wndpl.rcNormalPosition.top;


  lseek(hCurrentFile,0L,SEEK_SET);
  
  nFSize = sizeof(pbySignature);
  if (nFSize != write(hCurrentFile, pbySignature, sizeof(pbySignature))) {
    emuInfoMessage("Could not write into file !");
    return FALSE;
  }

  lSizeofChipset = sizeof(Chipset);
  lBytesWritten = write(hCurrentFile, &lSizeofChipset, sizeof(lSizeofChipset));
  nFSize += lBytesWritten;
  lBytesWritten = write(hCurrentFile, &Chipset, lSizeofChipset);
  nFSize += lBytesWritten;
  if (Chipset.Ram) {
     lBytesWritten = write(hCurrentFile, Chipset.Ram,  Chipset.RamSize);
     nFSize += lBytesWritten;
  }
  ftruncate(hCurrentFile,nFSize);				// cut the rest 
  return TRUE;
}

BOOL saveSystemImageAs(LPCTSTR szFilename) {
  INT hFile;

  if (hCurrentFile >= 0) {	// already file in use
    close(hCurrentFile);	// close it, even it's same, so data always will be saved
    hCurrentFile = -1;
  }
  hFile = creat(szFilename, O_EXCL | 0640);
  if (hFile < 0) {	// error, couldn't create a new file
    emuInfoMessage("This file must be currently used by another instance of hp9816emu.");
    return FALSE;
  }
  strcpy(szCurrentFilename, szFilename);	// save new file name
  hCurrentFile = hFile;				// and the corresponding handle
  setWindowTitle(szCurrentFilename);		// update window title line
  updateWindowStatus();				// and draw it 
  return saveSystemImage();			// save current content
}
