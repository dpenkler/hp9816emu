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
#include "kml.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

TCHAR  szHP8xDirectory[MAX_PATH];
TCHAR  szCurrentDirectory[MAX_PATH];
TCHAR  szCurrentKml[] = "9816-L.KML";
TCHAR  szCurrentFilename[MAX_PATH];
TCHAR  szBufferFilename[MAX_PATH];
TCHAR  szRomFileName[MAX_PATH];
static TCHAR  szBackupKml[MAX_PATH];
static TCHAR  szBackupFilename[MAX_PATH];

// pointers for roms data
LPBYTE pbyRom = NULL;

UINT   nCurrentRomType = 16;					// Model
UINT   nCurrentClass = 0;					// Class -> derivate

// document signatures
#define SignatureLength 32
static BYTE pbySignature [SignatureLength] = "hp9816emu Configuration V1.0";
static int hCurrentFile = -1;



static TCHAR *szDiscType0[] = { _T("None"), _T("9121D"), _T("9895D"), _T("9122D"), _T("9134A")};
static WORD wDiscType0Numbers = 5;
static TCHAR *szDiscType2[] = { _T("None"), _T("9121D"), _T("9895D"), _T("9122D"), _T("9134A")};
static WORD wDiscType2Numbers = 5;
static TCHAR *szDiscType3[] = { _T("None"), _T("7908"), _T("7911"), _T("7912")};
static WORD wDiscType3Numbers = 4;

//################
//#
//#    Low level subroutines
//#
//################


static LPBYTE  LoadRom(LPCTSTR szRomDirectory, LPCTSTR szFilename) {
  INT  hRomFile = -1;
  DWORD dwFileSize, dwRead;
  LPBYTE pRom = NULL;
  struct stat rs;

  fprintf(stderr,"Loading ROM file %s ... ",szFilename);
  hRomFile = open(szFilename,0, O_RDONLY);
  if (hRomFile < 0) {
    hRomFile = -1;
    perror("Failed ");
    return NULL;
  }
  fstat(hRomFile,&rs);
  dwFileSize = rs.st_size;
  if (dwFileSize < 16*1024) { // file is too small.
    close(hRomFile);
    fprintf(stderr," failed - too small\n");
    hRomFile = -1;
    return NULL;
  }

  pRom = malloc(dwFileSize);
  if (pRom == NULL) {
    close(hRomFile);
    fprintf(stderr," failed - no memory\n");
    hRomFile = -1;
    return NULL;
  }

  // load file content
  read(hRomFile,pRom,dwFileSize);
  close(hRomFile);
  fprintf(stderr," success\n");
  return pRom;
}

//
// New document settings
//
static BOOL NewSettingsProc() {
  
  Chipset.RamSize = _KB(memSizes[bRamInd]);	        // RAM size
  fprintf(stderr,"Ram size is %dKB\n",Chipset.RamSize/1024);
  Chipset.RamStart = 0x01000000 - Chipset.RamSize;	// from ... to 0x00FFFFFF
  
  Chipset.Hpib71x = 1;	// printer ?
  
  Chipset.Hpib70x = 1;	// type of disk 9121
  Chipset.Hpib72x = 3;	// type of disk 9122
  Chipset.Hpib73x = 1;	// type of disk 7908
  Chipset.Hpib74x = 1;	// type of disk 7908
    
  hpib_init();		// initialize it
  
  Chipset.RomVer = 30;

  SetSpeed(wRealSpeed);	// set speed
  
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
//   Roms
//

//
// get the right BootRom
//
BOOL MapRom(LPCTSTR szRomDirectory)  {
  if (pbyRom != NULL) return FALSE;
  Chipset.Rom = pbyRom = LoadRom(szRomDirectory,szRomDirectory);
  Chipset.RomSize = 64*1024;
  return TRUE;
}

//
// remove BootRom from bus
//
VOID UnmapRom(VOID) {
  if (pbyRom==NULL)  return;
  free(pbyRom);
  pbyRom = NULL;
}

//
//   System images
//

VOID ResetSystemImage(VOID) {
  hpib_stop_bus();

  if (szCurrentKml[0]) {
    KillKML();
  }
  if (hCurrentFile>=0) {
    close(hCurrentFile);
    hCurrentFile = -1;
  }
  // szCurrentKml[0] = 0;			// preserve the current KML
  szCurrentFilename[0]=0;
  if (Chipset.Ram)  {
    free(Chipset.Ram);
    Chipset.Ram = NULL;
  }
  bzero(&Chipset,sizeof(Chipset));

  UpdateWindowStatus();
}

BOOL NewSystemImage(VOID) {
  hpib_stop_bus();

  ResetSystemImage();

  if (!InitKML(szCurrentKml,FALSE)) goto restore;

  Chipset.type = nCurrentRomType;

  NewSettingsProc(); // initial settings

  // allocate memory
  if (Chipset.Ram == NULL) {
    Chipset.Ram = (LPBYTE)malloc(Chipset.RamSize);
  }

  if (Chipset.Rom == NULL) {
    MapRom(szCurrentDirectory);
  }

  SystemReset();
  return TRUE;
 restore:

  return FALSE;
}

BOOL OpenSystemImage(LPCTSTR szFilename) {
  INT   hFile = -1;
  DWORD lBytesRead,lSizeofChipset;
  BYTE  pbyFileSignature[SignatureLength];
  UINT  ctBytesCompared;
  UINT  nLength;

  ResetSystemImage();

  hFile = open(szFilename, O_RDWR);
  if (hFile < 0) {
    perror("This file is missing or already loaded in another instance of Hp98x6.");
    goto restore;
  }

  // Read and Compare signature
  lBytesRead = read(hFile, pbyFileSignature, sizeof(pbyFileSignature));
  for (ctBytesCompared=0; ctBytesCompared<sizeof(pbyFileSignature); ctBytesCompared++) {
    if (pbyFileSignature[ctBytesCompared]!=pbySignature[ctBytesCompared]) {
      fprintf(stderr,"This file is not a valid hp9816 configuration.");
      goto restore;
    }
  }

  read(hFile,&nLength,sizeof(nLength));
  lBytesRead = read(hFile, szCurrentKml, nLength);
  if (nLength != lBytesRead) goto read_err;
  szCurrentKml[nLength] = 0;

  // read chipset size inside file
  lBytesRead = read(hFile, &lSizeofChipset, sizeof(lSizeofChipset));
  if (lBytesRead != sizeof(lSizeofChipset)) goto read_err;
  if (lSizeofChipset <= sizeof(Chipset)) {	// actual or older chipset version
    // read chipset content
    bzero(&Chipset,sizeof(Chipset)); // init chipset
    read(hFile, &Chipset, lSizeofChipset);
  } else goto read_err;

  // clean pointers

  hAlpha1BM = 0;
  hAlpha2BM = 0;
  hGraphImg = NULL;
  hFontBM   = 0;
    
  while (TRUE) {
    BOOL bOK;

    bOK = InitKML(szCurrentKml,FALSE);

    bOK = bOK && (nCurrentRomType == Chipset.type);
    if (bOK) break;

    KillKML();
    fprintf(stderr,"Could not find KML file %s\n, ciao!",szCurrentKml);
    exit(1);
  }

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

  strcpy(szCurrentFilename, szFilename);
  hCurrentFile = hFile;

  Reload_Graph();
  UpdateMainDisplay(TRUE);	// refresh screen alpha, graph & cmap
  hpib_names();
  UpdateAnnunciators(TRUE);
  return TRUE;

 read_err:
  fprintf(stderr,"This file must be truncated, and cannot be loaded.");
 restore:
  if (hFile != -1) close(hFile);
  return FALSE;
}

BOOL SaveSystemImage(VOID) {
  DWORD           lBytesWritten;
  DWORD           lSizeofChipset;
  UINT            nLength,nFSize=0;

  if (hCurrentFile == -1) return FALSE;

  //  wndpl.length = sizeof(wndpl);			// update saved window position
  //  GetWindowPlacement(hWnd, &wndpl);
  //  Chipset.nPosX = (SWORD) wndpl.rcNormalPosition.left;
  //  Chipset.nPosY = (SWORD) wndpl.rcNormalPosition.top;


  lseek(hCurrentFile,0L,SEEK_SET);
  
  nFSize = sizeof(pbySignature);
  if (nFSize != write(hCurrentFile, pbySignature, sizeof(pbySignature))) {
    fprintf(stderr,"Could not write into file !");
    return FALSE;
  }

  nLength = strlen(szCurrentKml);
  lBytesWritten = write(hCurrentFile, &nLength, sizeof(nLength));
  nFSize += lBytesWritten;
  lBytesWritten = write(hCurrentFile, szCurrentKml, nLength);
  nFSize += lBytesWritten;

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

BOOL SaveSystemImageAs(LPCTSTR szFilename) {
  INT hFile;

  if (hCurrentFile >= 0) {	// already file in use
    close(hCurrentFile);	// close it, even it's same, so data always will be saved
    hCurrentFile = -1;
  }
  hFile = creat(szFilename, O_EXCL | 0640);
  if (hFile < 0) {	// error, couldn't create a new file
    fprintf(stderr,"This file must be currently used by another instance of hp9816emu.");
    return FALSE;
  }
  strcpy(szCurrentFilename, szFilename);	// save new file name
  hCurrentFile = hFile;				// and the corresponding handle
  SetWindowTitle(szCurrentFilename);		// update window title line
  UpdateWindowStatus();				// and draw it 
  return SaveSystemImage();			// save current content
}

//
//   Open File Common Dialog Boxes
//

BOOL GetOpenFilename(VOID) {
    return TRUE;
}

BOOL GetSaveAsFilename(VOID) {
  return TRUE;
}

BOOL GetLoadLifFilename(VOID) {
  return TRUE;
}

BOOL GetSaveLifFilename(VOID) {
  return TRUE;
}

BOOL GetSaveDiskFilename(LPCTSTR lpstrName) {
  return TRUE;
}
