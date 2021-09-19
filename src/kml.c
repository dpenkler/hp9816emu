/*
 *   Kml.c
 *
 *	 taken from emu42	
 */

//
// Handling of KML scripts, taken as is from Emu42
// some changes for keyboard mapping 
// added 2 functions for name of instruments and lif labels
//

#include "common.h"
#include "hp9816emu.h"
#include "kml.h"

//#define DEBUG_KB
KmlBlock* pKml;

static VOID      FatalError(VOID);
static VOID      InitLex(LPTSTR szScript);
static VOID      CleanLex(VOID);
static VOID      SkipWhite(UINT nMode);
static TokenId   ParseToken(UINT nMode);
static DWORD     ParseInteger(VOID);
static LPTSTR    ParseString(VOID);
static TokenId   Lex(UINT nMode);
static KmlLine*  ParseLine(TokenId eCommand);
static KmlLine*  IncludeLines(LPCTSTR szFilename);
static KmlLine*  ParseLines(VOID);
static KmlBlock* ParseBlock(TokenId eBlock);
static KmlBlock* IncludeBlocks(LPCTSTR szFilename);
static KmlBlock* ParseBlocks(VOID);
static VOID      FreeLines(KmlLine* pLine);
static VOID      PressButton(UINT nId);
static VOID      ReleaseButton(UINT nId);
static VOID      PressButtonById(UINT nId);
static VOID      ReleaseButtonById(UINT nId);
static LPTSTR    GetStringParam(KmlBlock* pBlock, TokenId eBlock, TokenId eCommand, UINT nParam);
static DWORD     GetIntegerParam(KmlBlock* pBlock, TokenId eBlock, TokenId eCommand, UINT nParam);
static KmlLine*  SkipLines(KmlLine* pLine, TokenId eCommand);
static KmlLine*  If(KmlLine* pLine, BOOL bCondition);
static KmlLine*  RunLine(KmlLine* pLine);
static KmlBlock* LoadKMLGlobal(LPCTSTR szFilename);

static KmlBlock* pVKey[256];
static BYTE      byVKeyMap[256];
static KmlButton pButton[256];
static KmlAnnunciator pAnnunciator[32];
static UINT      nButtons = 0;
static UINT      nScancodes = 0;
static UINT      nAnnunciators = 0;
static BOOL      bDebug = TRUE;
static UINT      nLexLine;
static UINT      nLexInteger;
static UINT      nBlocksIncludeLevel;
static UINT      nLinesIncludeLevel;
static DWORD     nKMLFlags = 0;
static LPTSTR    szLexString;
static LPTSTR    szText;
static LPTSTR    szLexDelim[] =
  {
    _T(""),
    _T(" \t\n\r"),
    _T(" \t\n\r"),
    _T(" \t\r")
  };

static KmlToken pLexToken[] =
  {
    {TOK_ANNUNCIATOR,000001,11,_T("Annunciator")},
    {TOK_BACKGROUND, 000000,10,_T("Background")},
    {TOK_IFPRESSED,  000001, 9,_T("IfPressed")},
    {TOK_RESETFLAG,  000001, 9,_T("ResetFlag")},
    {TOK_SCANCODE,   000001, 8,_T("Scancode")},
    {TOK_HARDWARE,   000002, 8,_T("Hardware")},
    {TOK_MENUITEM,   000001, 8,_T("MenuItem")},
    {TOK_SETFLAG,    000001, 7,_T("SetFlag")},
    {TOK_RELEASE,    000001, 7,_T("Release")},
    {TOK_VIRTUAL,    000000, 7,_T("Virtual")},
    {TOK_INCLUDE,    000002, 7,_T("Include")},
    {TOK_NOTFLAG,    000001, 7,_T("NotFlag")},
    {TOK_GLOBAL,     000000, 6,_T("Global")},
    {TOK_AUTHOR,     000002, 6,_T("Author")},
    {TOK_BITMAP,     000002, 6,_T("Bitmap")},
    {TOK_OFFSET,     000011, 6,_T("Offset")},
    {TOK_BUTTON,     000001, 6,_T("Button")},
    {TOK_IFFLAG,     000001, 6,_T("IfFlag")},
    {TOK_ONDOWN,     000000, 6,_T("OnDown")},
    {TOK_NOHOLD,     000000, 6,_T("NoHold")},
    {TOK_TITLE,      000002, 5,_T("Title")},
    {TOK_OUTIN,      000011, 5,_T("OutIn")},
    {TOK_PATCH,      000002, 5,_T("Patch")},
    {TOK_PRINT,      000002, 5,_T("Print")},
    {TOK_DEBUG,      000001, 5,_T("Debug")},
    {TOK_COLOR,      001111, 5,_T("Color")},
    {TOK_MODEL,      000001, 5,_T("Model")},
    {TOK_CLASS,      000001, 5,_T("Class")},
    {TOK_PRESS,      000001, 5,_T("Press")},
    {TOK_TYPE,       000001, 4,_T("Type")},
    {TOK_SIZE,       000011, 4,_T("Size")},
    {TOK_ZOOM,       000001, 4,_T("Zoom")},
    {TOK_DOWN,       000011, 4,_T("Down")},
    {TOK_ELSE,       000000, 4,_T("Else")},
    {TOK_ONUP,       000000, 4,_T("OnUp")},
    {TOK_MAP,        000011, 3,_T("Map")},
    {TOK_ROM,        000002, 3,_T("Rom")},
    {TOK_SCR,        000000, 3,_T("Scr")},
    {TOK_END,        000000, 3,_T("End")},
    {0,              000000, 0,_T("")},
  };

static TokenId eIsBlock[] =
  {
    TOK_IFFLAG,
    TOK_IFPRESSED,
    TOK_ONDOWN,
    TOK_ONUP,
    TOK_NONE
  };

static BOOL bClicking = FALSE;
static UINT uButtonClicked = 0;

static BOOL bPressed = FALSE;				// no key pressed
static UINT uLastPressedKey = 0;			// var for last pressed key

//################
//#
//#    Compilation Result
//#
//################

static UINT nLogLength = 0;
static LPTSTR szLog = NULL;

static VOID AddToLog(LPTSTR szString) {
  fprintf(stderr,"%s\n",szString);
}


//################
//#
//#    Choose Scripting
//#
//################

typedef struct _KmlScript
{
  LPTSTR szFilename;
  LPTSTR szTitle;
  DWORD   nId;
  struct _KmlScript* pNext;
} KmlScript;

static UINT nKmlFiles;
static KmlScript* pKmlList;
static CHAR cKmlType;

//################
//#
//#    KML File Mapping
//#
//################

static LPTSTR MapKMLFile(INT hFile) {
  DWORD  lBytesRead;
  DWORD  dwFileSizeLow;
  DWORD  dwFileSizeHigh;
  LPTSTR lpBuf = NULL;
  struct stat rs;
  fstat(hFile,&rs);
  dwFileSizeLow = rs.st_size;

  lpBuf = malloc((dwFileSizeLow+1)*sizeof(lpBuf[0]));
  if (lpBuf == NULL)    {
    fprintf(stderr,"Cannot allocate %li bytes.", (dwFileSizeLow+1)*sizeof(lpBuf[0]));
    goto fail;
  }

  read(hFile, lpBuf, dwFileSizeLow);

  lpBuf[dwFileSizeLow] = 0;

 fail:
  close(hFile);
  return lpBuf;
}

//################
//#
//#    Script Parsing
//#
//################

static VOID FatalError(VOID) {
  fprintf(stderr,_T("Fatal Error at line %i"), nLexLine);
  szText[0] = 0;
}

static VOID InitLex(LPTSTR szScript) {
  nLexLine = 1;
  szText = szScript;
}

static VOID CleanLex(VOID) {
  nLexLine = 0;
  nLexInteger = 0;
  szLexString = NULL;
  szText = NULL;
}

// TODO: Change this poor (and slow!) code
static BOOL IsBlock(TokenId eId) {
  UINT uBlock = 0;
  while (eIsBlock[uBlock] != TOK_NONE) {
    if (eId == eIsBlock[uBlock]) return TRUE;
    uBlock++;
  }
  return FALSE;
}

static LPCTSTR GetStringOf(TokenId eId) {
  UINT i = 0;
  while (pLexToken[i].nLen) {
    if (pLexToken[i].eId == eId) return pLexToken[i].szName;
    i++;
  }
  return _T("<Undefined>");
}

static VOID SkipWhite(UINT nMode) {
  UINT i;
 loop:
  i = 0;
  while (szLexDelim[nMode][i]) {
    if (*szText == szLexDelim[nMode][i]) break;
    i++;
  }
  if (szLexDelim[nMode][i] != 0) {
    if (szLexDelim[nMode][i]==_T('\n'))	nLexLine++;
    szText++;
    goto loop;
  }
  if (*szText==_T('#')) {
    do szText++; while (*szText != _T('\n'));
    if (nMode != LEX_PARAM) goto loop;
  }
}

static TokenId ParseToken(UINT nMode) {
  UINT i, j, k;
  i = 0;
  while (szText[i]) {
    j = 0;
    while (szLexDelim[nMode][j]) {
      if (szLexDelim[nMode][j] == szText[i]) break;
      j++;
    }
    if (szLexDelim[nMode][j] == _T('\n')) nLexLine++;
    if (szLexDelim[nMode][j] != 0) break;
    i++;
  }
  if (i==0) {
    return TOK_NONE;
  }
  j = 0;
  while (pLexToken[j].nLen) {
    if (pLexToken[j].nLen>i) {
      j++;
      continue;
    }
    if (pLexToken[j].nLen<i) break;
    k = 0;
    if (memcmp(pLexToken[j].szName, szText, i)==0) {
	  szText += i;
	  return pLexToken[j].eId;
    }
    j++;
  }
  szText[i] = 0;
  if (bDebug)  {
    fprintf(stderr,"%i: Undefined token %s", nLexLine, szText);
    return TOK_NONE;
  }
  return TOK_NONE;
}

static DWORD ParseInteger(VOID) {
  DWORD nNum = 0;
  while (isdigit(*szText)) {
      nNum = nNum * 10 + ((*szText) - _T('0'));
      szText++;
    } 
  return nNum;
}

static LPTSTR ParseString(VOID) {
  LPTSTR szString;
  LPTSTR szBuffer;
  UINT   nLength;
  UINT   nBlock;

  szText++;
  nLength = 0;
  nBlock = 255;
  szBuffer = malloc(nBlock+1);
  while (*szText != _T('"')) {
      if (*szText == _T('\\')) szText++;
      if (*szText == 0)	{
	FatalError();
	return NULL;
      }
      szBuffer[nLength] = *szText;
      nLength++;
      if (nLength == nBlock) {
	nBlock += 256;
	szBuffer = realloc(szBuffer, nBlock+1);
      }
      szText++;
  }
  szText++;
  szBuffer[nLength] = 0;
  szString = DuplicateString(szBuffer);
  free(szBuffer);
  return szString;
}		

static TokenId Lex(UINT nMode) {
  SkipWhite(nMode);
  if (isdigit(*szText))    {
      nLexInteger = ParseInteger();
      return TOK_INTEGER;
  }
  if (*szText == '"') {
      szLexString = ParseString();
      return TOK_STRING;
  }
  if ((nMode == LEX_PARAM) && (*szText == '\n')) {
      nLexLine++;
      szText++;
      return TOK_EOL;
  }
  return ParseToken(nMode);
}

static KmlLine* ParseLine(TokenId eCommand) {
  UINT     i, j;
  DWORD    nParams;
  TokenId  eToken;
  KmlLine* pLine;

  i = 0;
  while (pLexToken[i].nLen) {
    if (pLexToken[i].eId == eCommand) break;
    i++;
  }
  if (pLexToken[i].nLen == 0) return NULL;
  
  j = 0;
  pLine = malloc(sizeof(KmlLine));
  pLine->eCommand = eCommand;
  nParams = pLexToken[i].nParams;
 loop:
  eToken = Lex(LEX_PARAM);
  if ((nParams&7)==TYPE_NONE) {
    if (eToken != TOK_EOL) {
      fprintf(stderr,"%i: Too many parameters (%i expected).", nLexLine, j);
      goto errline;					// free memory of arguments
    }
    return pLine;
  }
  if ((nParams&7)==TYPE_INTEGER) {
    if (eToken != TOK_INTEGER) {
      fprintf(stderr,"%i: Parameter %i of %s must be an integer.", nLexLine, j+1, pLexToken[i].szName);
      goto errline;					// free memory of arguments
    }
    pLine->nParam[j++] = nLexInteger;
    nParams >>= 3;
    goto loop;
  }
  if ((nParams&7)==TYPE_STRING) {
      if (eToken != TOK_STRING)	{
	fprintf(stderr,"%i: Parameter %i of %s must be a string.", nLexLine, j+1, pLexToken[i].szName);
	goto errline;					// free memory of arguments
      }
      pLine->nParam[j++] = (long) szLexString;
      nParams >>= 3;
      goto loop;
  }
  AddToLog(_T("Oops..."));
 errline:
  // if last argument was string, free it
  if (eToken == TOK_STRING) free(szLexString);
  
  nParams = pLexToken[i].nParams;			// get argument types of command
  for (i=0; i<j; i++) {						// handle all scanned arguments
    if ((nParams&7) == TYPE_STRING)	{	// string type
      free((LPVOID)pLine->nParam[i]);
    }
    nParams >>= 3;						// next argument type
  }
  free(pLine);
  return NULL;
}

static KmlLine* IncludeLines(LPCTSTR szFilename)  {
  INT      hFile;
  LPTSTR   lpbyBuf;
  UINT     uOldLine;
  LPTSTR   szOldText;
  KmlLine *pLine;

  hFile = open(szFilename, O_RDONLY);
  if (hFile < 0)  {
      fprintf(stderr,"Error while opening include file %s.", szFilename);
      FatalError();
      return NULL;
  }
  if ((lpbyBuf = MapKMLFile(hFile)) == NULL) {
    FatalError();
    return NULL;
  }

  uOldLine  = nLexLine;
  szOldText = szText;

  nLinesIncludeLevel++;
  fprintf(stderr,"l%i:Including %s", nLinesIncludeLevel, szFilename);
  InitLex(lpbyBuf);
  pLine = ParseLines();
  CleanLex();
  nLinesIncludeLevel--;

  nLexLine  = uOldLine;
  szText    = szOldText;
  free(lpbyBuf);

  return pLine;
}

static KmlLine* ParseLines(VOID) {
  KmlLine* pLine = NULL;
  KmlLine* pFirst = NULL;
  TokenId  eToken;
  UINT     nLevel = 0;

  while ((eToken = Lex(LEX_COMMAND)))  {
    if (IsBlock(eToken)) nLevel++;
    if (eToken == TOK_INCLUDE) {
      LPTSTR szFilename;
      eToken = Lex(LEX_PARAM);	// get include parameter in 'szLexString'
      if (eToken != TOK_STRING) {// not a string (token doesn't begin with ")
	AddToLog(_T("Include: string expected as parameter."));
	FatalError();
	goto abort;
      }
      szFilename = szLexString;		// save pointer to allocated memory
      if (pFirst)   {
	pLine->pNext = IncludeLines(szLexString);
      }  else {
	pLine = pFirst = IncludeLines(szLexString);
      }
      free(szFilename);			// free memory
      if (pLine == NULL)		// parsing error
	goto abort;
      while (pLine->pNext) pLine=pLine->pNext;
      continue;
    }
    if (eToken == TOK_END)  {
      if (nLevel) {
	nLevel--;
      }	else {
	if (pLine) pLine->pNext = NULL;
	return pFirst;
      }
    }
    if (pFirst) {
      pLine = pLine->pNext = ParseLine(eToken);
    } else {
      pLine = pFirst = ParseLine(eToken);
    }
    if (pLine == NULL) goto abort;	// parsing error
  }
  if (nLinesIncludeLevel) {
  if (pLine) pLine->pNext = NULL;
  return pFirst;
  }	
  AddToLog(_T("Open block."));
 abort:
  if (pFirst) FreeLines(pFirst);
  return NULL;
}

static KmlBlock* ParseBlock(TokenId eType) {
  UINT      u1;
  KmlBlock* pBlock;
  TokenId   eToken;

  nLinesIncludeLevel = 0;

  pBlock = malloc(sizeof(KmlBlock));
  pBlock->eType = eType;

  u1 = 0;
  while (pLexToken[u1].nLen) {
    if (pLexToken[u1].eId == eType) break;
    u1++;
  }
  if (pLexToken[u1].nParams) {
    eToken = Lex(LEX_COMMAND);
    switch (eToken) {
    case TOK_NONE:
      AddToLog(_T("Open Block at End Of File."));
      free(pBlock);
      FatalError();
      return NULL;
    case TOK_INTEGER:
      if ((pLexToken[u1].nParams&7)!=TYPE_INTEGER) {
	AddToLog(_T("Wrong block argument."));
	free(pBlock);
	FatalError();
	return NULL;
      }
      pBlock->nId = nLexInteger;
      break;
    default:
      AddToLog(_T("Wrong block argument."));
      free(pBlock);
      FatalError();
      return NULL;
    }
  }
  
  pBlock->pFirstLine = ParseLines();
  
  if (pBlock->pFirstLine == NULL) {		// break on ParseLines error
    free(pBlock);
    pBlock = NULL;
  }
  
  return pBlock;
}

static KmlBlock* IncludeBlocks(LPCTSTR szFilename) {
  INT       hFile;
  LPTSTR    lpbyBuf;
  UINT      uOldLine;
  LPTSTR    szOldText;
  KmlBlock *pFirst;

  hFile = open(szFilename,  0, O_RDONLY);
  if (hFile < 0)   {
    fprintf(stderr,"Error while opening include file %s.", szFilename);
  FatalError();
  return NULL;
  }
  if ((lpbyBuf = MapKMLFile(hFile)) == NULL)  {
    FatalError();
    return NULL;
  }
  
  uOldLine  = nLexLine;
  szOldText = szText;

  nBlocksIncludeLevel++;
  fprintf(stderr,"b%i:Including %s", nBlocksIncludeLevel, szFilename);
  InitLex(lpbyBuf);
  pFirst = ParseBlocks();
  CleanLex();
  nBlocksIncludeLevel--;

  nLexLine  = uOldLine;
  szText    = szOldText;
  free( lpbyBuf);

  return pFirst;
}

static KmlBlock* ParseBlocks(VOID) {
  TokenId  eToken;
  KmlBlock *pFirst = NULL;
  KmlBlock *pBlock;

  while ((eToken=Lex(LEX_BLOCK))!=TOK_NONE) {
    if (eToken == TOK_INCLUDE) {
      LPTSTR szFilename;
      eToken = Lex(LEX_PARAM);	// get include parameter in 'szLexString'
      if (eToken != TOK_STRING) {// not a string (token doesn't begin with ")
	AddToLog(_T("Include: string expected as parameter."));
	FatalError();
	goto abort;
      }
      szFilename = szLexString;		// save pointer to allocated memory
      if (pFirst) pBlock = pBlock->pNext = IncludeBlocks(szLexString);
      else        pBlock = pFirst = IncludeBlocks(szLexString);
      free( szFilename);		// free memory
      if (pBlock == NULL) goto abort;		// parsing error
      while (pBlock->pNext) pBlock=pBlock->pNext;
      continue;
    }
    if (pFirst) pBlock = pBlock->pNext = ParseBlock(eToken);
    else        pBlock = pFirst = ParseBlock(eToken);
    if (pBlock == NULL) {
      AddToLog(_T("Invalid block."));
      FatalError();
      goto abort;
    }
  }
  if (pFirst) pBlock->pNext = NULL;
  return pFirst;
 abort:
  if (pFirst) FreeBlocks(pFirst);
  return NULL;
}

//################
//#
//#    Initialization Phase
//#
//################

static VOID InitGlobal(KmlBlock* pBlock) {
  KmlLine* pLine = pBlock->pFirstLine;
  while (pLine) {
    switch (pLine->eCommand) {
    case TOK_TITLE:
      fprintf(stderr,"Title: %s\n", (LPTSTR)pLine->nParam[0]);
      break;
    case TOK_AUTHOR:
      fprintf(stderr,"Author: %s\n", (LPTSTR)pLine->nParam[0]);
      break;
    case TOK_PRINT:
      AddToLog((LPTSTR)pLine->nParam[0]);
      break;
    case TOK_HARDWARE:
      fprintf(stderr,"Hardware Platform: %s\n", (LPTSTR)pLine->nParam[0]);
      break;
    case TOK_MODEL:
      nCurrentRomType = pLine->nParam[0];
      fprintf(stderr,"Calculator Model : %u\n", nCurrentRomType);
      break;
    case TOK_CLASS:
      nCurrentClass = pLine->nParam[0];
      fprintf(stderr,"Calculator Class : %u\n", nCurrentClass);
      break;
    case TOK_DEBUG:
      bDebug = pLine->nParam[0]&1;
      fprintf(stderr,"Debug %s\n", bDebug?_T("On"):_T("Off"));
      break;
    case TOK_ROM:
      if (pbyRom != NULL) {
	fprintf(stderr,"Rom %s Ignored.\n", (LPTSTR)pLine->nParam[0]);
	AddToLog(_T("Please put only one Rom command in the Global block."));
	break;
      }
      if (!MapRom((LPTSTR)pLine->nParam[0])) {
	fprintf(stderr,"Cannot open Rom %s\n", (LPTSTR)pLine->nParam[0]);
	break;
      }
      strcpy(szRomFileName,(LPTSTR)pLine->nParam[0]);
      break;
    case TOK_PATCH:
      if (pbyRom == NULL) {
	fprintf(stderr,"Patch %s ignored.\n", (LPTSTR)pLine->nParam[0]);
	AddToLog(_T("Please put the Rom command before any Patch."));
	break;
      }
      fprintf(stderr,"Patch %s is Wrong or Missing\n", (LPTSTR)pLine->nParam[0]);
      break;
    case TOK_BITMAP:
      if (hMainBM != 0) {
	fprintf(stderr,"Bitmap %s Ignored.\n", (LPTSTR)pLine->nParam[0]);
	AddToLog("Please put only one Bitmap command in the Global block.");
	break;
      }
      if (!CreateMainBitmap((LPTSTR)pLine->nParam[0])) {
	fprintf(stderr,"Cannot Load Bitmap %s.\n", (LPTSTR)pLine->nParam[0]);
	break;
      }
      fprintf(stderr,"Bitmap %s Loaded.\n", (LPTSTR)pLine->nParam[0]);
      break;
    default:
      fprintf(stderr,"Command %s Ignored in Block %s\n",
	      GetStringOf(pLine->eCommand), GetStringOf(pBlock->eType));
    }
    pLine = pLine->pNext;
  }
}

static KmlLine* InitBackground(KmlBlock* pBlock) {
  KmlLine* pLine = pBlock->pFirstLine;
  while (pLine) {
    switch (pLine->eCommand){
    case TOK_OFFSET:
      bgX = pLine->nParam[0];
      bgY = pLine->nParam[1];
      break;
    case TOK_SIZE:
      bgW = pLine->nParam[0];
      bgH = pLine->nParam[1];
      break;
    case TOK_END:
      return pLine;
    default:
      fprintf(stderr,"Command %s Ignored in Block %s\n",
	      GetStringOf(pLine->eCommand), GetStringOf(pBlock->eType));
    }
    pLine = pLine->pNext;
  }
  return NULL;
}

static KmlLine* InitScreen(KmlBlock* pBlock) {
  KmlLine* pLine = pBlock->pFirstLine;
  while (pLine) {
    switch (pLine->eCommand) {
    case TOK_OFFSET:
      scrX = pLine->nParam[0];
      scrY = pLine->nParam[1];
      break;
    case TOK_ZOOM: 
      break;
    case TOK_COLOR:
      // FIXME SetScreenColor(pLine->nParam[0],pLine->nParam[1],pLine->nParam[2],pLine->nParam[3]);
      break;
    case TOK_END:
      return pLine;
    default:
      fprintf(stderr,"Command %s Ignored in Block %s",
	      GetStringOf(pLine->eCommand), GetStringOf(pBlock->eType));
    }
    pLine = pLine->pNext;
  }
  return NULL;
}

static KmlLine* InitAnnunciator(KmlBlock* pBlock) {
  KmlLine* pLine = pBlock->pFirstLine;
  UINT nId = pBlock->nId-1;
  if (nId >= ARRAYSIZEOF(pAnnunciator)) {
    fprintf(stderr,"Wrong Annunciator Id %i", nId);
    return NULL;
  }
  nAnnunciators++;
  while (pLine) {
    switch (pLine->eCommand) {
    case TOK_OFFSET:
      pAnnunciator[nId].nOx = pLine->nParam[0];
      pAnnunciator[nId].nOy = pLine->nParam[1];
      break;
    case TOK_DOWN:
      pAnnunciator[nId].nDx = pLine->nParam[0];
      pAnnunciator[nId].nDy = pLine->nParam[1];
      break;
    case TOK_SIZE:
      pAnnunciator[nId].nCx = pLine->nParam[0];
      pAnnunciator[nId].nCy = pLine->nParam[1];
      break;
    case TOK_END:
      return pLine;
    default:
      fprintf(stderr,"Command %s Ignored in Block %s", GetStringOf(pLine->eCommand), GetStringOf(pBlock->eType));
    }
    pLine = pLine->pNext;
  }
  return NULL;
}

static VOID InitButton(KmlBlock* pBlock) {
  KmlLine* pLine = pBlock->pFirstLine;
  UINT nLevel = 0;
  if (nButtons>=256) {
    AddToLog(_T("Only the first 256 buttons will be defined."));
    return;
  }
  pButton[nButtons].nId = pBlock->nId;
  pButton[nButtons].bDown = FALSE;
  pButton[nButtons].nType = 0; // default : user defined button
  while (pLine) {
    if (nLevel) {
      if (pLine->eCommand == TOK_END) nLevel--;
      pLine = pLine->pNext;
      continue;
    }
    if (IsBlock(pLine->eCommand)) nLevel++;
    switch (pLine->eCommand) {
    case TOK_TYPE:
      pButton[nButtons].nType = pLine->nParam[0];
      break;
    case TOK_OFFSET:
      pButton[nButtons].nOx = pLine->nParam[0];
      pButton[nButtons].nOy = pLine->nParam[1];
      break;
    case TOK_DOWN:
      pButton[nButtons].nDx = pLine->nParam[0];
      pButton[nButtons].nDy = pLine->nParam[1];
      break;
    case TOK_SIZE:
      pButton[nButtons].nCx = pLine->nParam[0];
      pButton[nButtons].nCy = pLine->nParam[1];
      break;
    case TOK_OUTIN:
      pButton[nButtons].nOut = pLine->nParam[0];
      pButton[nButtons].nIn  = pLine->nParam[1];
      break;
    case TOK_ONDOWN:
      pButton[nButtons].pOnDown = pLine;
      break;
    case TOK_ONUP:
      pButton[nButtons].pOnUp = pLine;
      break;
    case TOK_NOHOLD:
      pButton[nButtons].dwFlags &= ~(BUTTON_VIRTUAL);
      pButton[nButtons].dwFlags |= BUTTON_NOHOLD;
      break;
    case TOK_VIRTUAL:
      pButton[nButtons].dwFlags &= ~(BUTTON_NOHOLD);
      pButton[nButtons].dwFlags |= BUTTON_VIRTUAL;
      break;
    default:
      fprintf(stderr,"Command %s Ignored in Block %s %i", GetStringOf(pLine->eCommand), GetStringOf(pBlock->eType), pBlock->nId);
    }
    pLine = pLine->pNext;
  }
  if (nLevel)
    fprintf(stderr,"%i Open Block(s) in Block %s %i", nLevel, GetStringOf(pBlock->eType), pBlock->nId);
  nButtons++;
}

//################
//#
//#    Execution
//#
//################

static KmlLine* SkipLines(KmlLine* pLine, TokenId eCommand) {
  UINT nLevel = 0;
  while (pLine) {
    if (IsBlock(pLine->eCommand)) nLevel++;
    if (pLine->eCommand==eCommand) {
      if (nLevel == 0) return pLine->pNext;
    }
    if (pLine->eCommand == TOK_END) {
      if (nLevel)
	nLevel--;
      else
	return NULL;
    }
    pLine = pLine->pNext;
  }
  return pLine;
}

static KmlLine* If(KmlLine* pLine, BOOL bCondition) {
  pLine = pLine->pNext;
  if (bCondition) {
    while (pLine) {
      if (pLine->eCommand == TOK_END) {
	pLine = pLine->pNext;
	break;
      }
      if (pLine->eCommand == TOK_ELSE) {
	pLine = SkipLines(pLine, TOK_END);
	break;
      }
      pLine = RunLine(pLine);
    }
  } else {
    pLine = SkipLines(pLine, TOK_ELSE);
    while (pLine) {
      if (pLine->eCommand == TOK_END) {
	pLine = pLine->pNext;
	break;
      }
      pLine = RunLine(pLine);
    }
  }
  return pLine;
}

static KmlLine* RunLine(KmlLine* pLine) {
  fprintf(stderr,"RunLine token %d\n",pLine->eCommand);
  switch (pLine->eCommand) {
    case TOK_MAP:
      if (byVKeyMap[pLine->nParam[0]&0xFF]&1)
	PressButtonById(pLine->nParam[1]);
      else
	ReleaseButtonById(pLine->nParam[1]);
      break;
    case TOK_PRESS:
      PressButtonById(pLine->nParam[0]);
      break;
    case TOK_RELEASE:
      ReleaseButtonById(pLine->nParam[0]);
      break;
    case TOK_MENUITEM:
      // FIXME  PostMessage(hWnd, WM_COMMAND, 0x19C40+(pLine->nParam[0]&0xFF), 0);
      break;
    case TOK_SETFLAG:
      nKMLFlags |= 1<<(pLine->nParam[0]&0x1F);
      break;
    case TOK_RESETFLAG:
      nKMLFlags &= ~(1<<(pLine->nParam[0]&0x1F));
      break;
    case TOK_NOTFLAG:
      nKMLFlags ^= 1<<(pLine->nParam[0]&0x1F);
      break;
    case TOK_IFPRESSED:
      return If(pLine,byVKeyMap[pLine->nParam[0]&0xFF]);
      break;
    case TOK_IFFLAG:
      return If(pLine,(nKMLFlags>>(pLine->nParam[0]&0x1F))&1);
    default:
      break;
    }
  return pLine->pNext;
}

//################
//#
//#    Clean Up
//#
//################

static VOID FreeLines(KmlLine* pLine) {
  while (pLine) {
    KmlLine* pThisLine = pLine;
    UINT i = 0;
    DWORD nParams;
    while (pLexToken[i].nLen) {		// search in all token definitions
      // break when token definition found
      if (pLexToken[i].eId == pLine->eCommand) break;
      i++;				// next token definition
    }
    nParams = pLexToken[i].nParams;	// get argument types of command
    i = 0;				// first parameter
    while ((nParams&7)) {		// argument left
      if ((nParams&7) == TYPE_STRING) {	// string type
	free( (LPVOID)pLine->nParam[i]);
      }
      i++;				// incr. parameter buffer index
      nParams >>= 3;			// next argument type
    }
    pLine = pLine->pNext;		// get next line
    free( pThisLine);
  }
}

VOID FreeBlocks(KmlBlock* pBlock) {
  while (pBlock)  {
    KmlBlock* pThisBlock = pBlock;
    pBlock = pBlock->pNext;
    FreeLines(pThisBlock->pFirstLine);
    free( pThisBlock);
  }
}

VOID KillKML(VOID) {
  if (nState==SM_RUN) {
    fprintf(stderr,"FATAL: KillKML while emulator is running !!!");
    SwitchToState(SM_RETURN);
  }
  UnmapRom();
  DestroyScreenBitmap();
  DestroyMainBitmap();
  bClicking = FALSE;
  uButtonClicked = 0;
  FreeBlocks(pKml);
  pKml = NULL;
  nButtons = 0;
  nScancodes = 0;
  nAnnunciators = 0;
  bzero(pButton, sizeof(pButton));
  bzero(pAnnunciator, sizeof(pAnnunciator));
  bzero(pVKey, sizeof(pVKey));
  bgX = 0;
  bgY = 0;
  bgW = 256;
  bgH = 0;
  UpdateWindowStatus();
}

//################
//#
//#    Extract Keyword's Parameters
//#
//################

static LPTSTR GetStringParam(KmlBlock* pBlock, TokenId eBlock, TokenId eCommand, UINT nParam) {
  while (pBlock) {
    if (pBlock->eType == eBlock) {
      KmlLine* pLine = pBlock->pFirstLine;
      while (pLine) {
	if (pLine->eCommand == eCommand) {
	  return (LPTSTR)pLine->nParam[nParam];
	}
	pLine = pLine->pNext;
      }
    }
    pBlock = pBlock->pNext;
  }
  return NULL;
}

static DWORD GetIntegerParam(KmlBlock* pBlock, TokenId eBlock, TokenId eCommand, UINT nParam) {
  while (pBlock)  {
    if (pBlock->eType == eBlock) {
      KmlLine* pLine = pBlock->pFirstLine;
      while (pLine) {
	if (pLine->eCommand == eCommand){
	  return pLine->nParam[nParam];
	}
	pLine = pLine->pNext;
      }
    }
    pBlock = pBlock->pNext;
  }
  return 0;
}

//################
//#
//#    Buttons
//#
//################

static INT iSqrt(INT nNumber) {		// integer y=sqrt(x) function
  INT m, b = 0, t = nNumber;
  
  do {
    m = (b + t + 1) / 2;		// median number
    if (m * m - nNumber > 0)		// calculate x^2-y
      t = m;				// adjust upper border
    else 
      b = m;				// adjust lower border
  } while(t - b > 1);
  return b;
}

static VOID AdjustPixel(UINT x, UINT y, BYTE byOffset) {
  //FIXME
#if 0
  COLORREF rgb;
  WORD     wB, wG, wR;
					
  rgb = GetPixel(hWindowBM, x, y);

  // adjust color red
  wR = (((WORD) rgb) & 0x00FF) + byOffset;
  if (wR > 0xFF) wR = 0xFF;
  rgb >>= 8;
  // adjust color green
  wG = (((WORD) rgb) & 0x00FF) + byOffset;
  if (wG > 0xFF) wG = 0xFF;
  rgb >>= 8;
  // adjust color blue
  wB = (((WORD) rgb) & 0x00FF) + byOffset;
  if (wB > 0xFF) wB = 0xFF;

  SetPixel(hWindowBM, x, y, RGB(wR,wG,wB));
#endif
}

// draw transparent circle with center coordinates and radius in pixel
static __inline VOID TransparentCircle(UINT cx, UINT cy, UINT r)  {
#define HIGHADJ 0x80			// color incr. at center
#define LOWADJ  0x08			// color incr. at border

  INT x, y;
  INT rr = r * r;			// calculate r^2

  // y-rows of circle
  for (y = 0; y < (INT) r; ++y) {
    INT yy = y * y;			// calculate y^2
    
    // x-columns of circle 
    INT nXWidth = iSqrt(rr-yy);
    
    for (x = 0; x < nXWidth; ++x) {
      // color offset, sqrt(x*x+y*y) <= r !!!
      BYTE byOff = HIGHADJ - (BYTE) (iSqrt((x*x+yy) * (HIGHADJ-LOWADJ)*(HIGHADJ-LOWADJ) / rr));
      
      AdjustPixel(cx+x, cy+y, byOff);
      if (x != 0) 	      AdjustPixel(cx-x, cy+y, byOff);
      if (y != 0)	      AdjustPixel(cx+x, cy-y, byOff);
      if (x != 0 && y != 0) AdjustPixel(cx-x, cy-y, byOff);
    }
  }
#undef HIGHADJ
#undef LOWADJ
}

static VOID DrawButton(UINT nId) {
  UINT x0 = pButton[nId].nOx;
  UINT y0 = pButton[nId].nOy;

  switch (pButton[nId].nType) {
    case 0: // bitmap key
      if (pButton[nId].bDown) {
	emuBitBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy,
		  hMainBM, pButton[nId].nDx, pButton[nId].nDy, GXcopy);
      } else {
	// update background only
	emuBitBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy,
		  hMainBM, x0, y0, GXcopy);
      }
      break;
  case 1: // shift key to right down
    if (pButton[nId].bDown) {
      UINT x1 = x0+pButton[nId].nCx-1;
      UINT y1 = y0+pButton[nId].nCy-1;
      emuBitBlt(hWnd, x0+3,y0+3,pButton[nId].nCx-5,pButton[nId].nCy-5,
		hMainBM,x0+2,y0+2,GXcopy);
      // FIXME
      //  SelectObject(hWindowDC, GetStockObject(BLACK_PEN));
      //  MoveToEx(hWindowDC, x0, y0, NULL); LineTo(hWindowDC, x1, y0);
      //  MoveToEx(hWindowDC, x0, y0, NULL); LineTo(hWindowDC, x0, y1);
      //  SelectObject(hWindowDC, GetStockObject(WHITE_PEN));
      //  MoveToEx(hWindowDC, x1, y0, NULL); LineTo(hWindowDC, x1,   y1);
      //  MoveToEx(hWindowDC, x0, y1, NULL); LineTo(hWindowDC, x1+1, y1);
    } else {
      emuBitBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy,
		hMainBM, x0, y0, GXcopy);
    }
    break;
  case 2: // do nothing
    break;
  case 3: // invert key color, even in display
    if (pButton[nId].bDown) {
      emuPatBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy, GXxor);
    } else {
      RECT Rect;
      Rect.left = x0 - bgX;
      Rect.top  = y0 - bgY;
      Rect.right  = Rect.left + pButton[nId].nCx;
      Rect.bottom = Rect.top + pButton[nId].nCy;
      // FIXME InvalidateRect(hWnd, &Rect, FALSE);	// call WM_PAINT for background and display redraw
    }
    break;
  case 4: // bitmap key, even in display
    if (pButton[nId].bDown) {
      // update background only
      emuBitBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy,
		hMainBM, x0, y0, GXcopy);
    } else {
      RECT Rect;
      Rect.left = x0 - bgX;
      Rect.top  = y0 - bgY;
      Rect.right  = Rect.left + pButton[nId].nCx;
      Rect.bottom = Rect.top  + pButton[nId].nCy;
      // FIXME InvalidateRect(hWnd, &Rect, FALSE);	// call WM_PAINT for background and display redraw
    }
    break;
  case 5: // transparent circle
    if (pButton[nId].bDown) {
      TransparentCircle(x0 + pButton[nId].nCx / 2, // x-center coordinate
			y0 + pButton[nId].nCy / 2, // y-center coordinate
			min(pButton[nId].nCx,pButton[nId].nCy) / 2); // radius
    } else {
      // update background only
      emuBitBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy,
		hMainBM, x0, y0, GXcopy);
    }
    break;
  default: // black key, default drawing on illegal types
    if (pButton[nId].bDown) {
      emuPatBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy, GXset);
    } else {
      // update background only
      emuBitBlt(hWnd, x0, y0, pButton[nId].nCx, pButton[nId].nCy,
		hMainBM, x0, y0, GXcopy);
    }
  }
  emuFlush();
}

static VOID PressButton(UINT nId) {
  if (pButton[nId].bDown) return;	// key already pressed -> exit

  pButton[nId].bDown = TRUE;
  DrawButton(nId);
  if (pButton[nId].nIn) {
    ButtonEvent(TRUE, nId + 1, pButton[nId].nOx, pButton[nId].nOy);
  } else {
    KmlLine* pLine = pButton[nId].pOnDown;
    while ((pLine)&&(pLine->eCommand!=TOK_END)) {
      pLine = RunLine(pLine);
    }
  }
}

static VOID ReleaseButton(UINT nId) {
  pButton[nId].bDown = FALSE;
  DrawButton(nId);
  if (pButton[nId].nIn) {
    ButtonEvent(FALSE, nId + 1, pButton[nId].nOx, pButton[nId].nOy);
  } else {
    KmlLine* pLine = pButton[nId].pOnUp;
    while ((pLine)&&(pLine->eCommand!=TOK_END))	{
      pLine = RunLine(pLine);
    }
  }
}

static VOID PressButtonById(UINT nId) {
  UINT i;
  for (i=0; i<nButtons; i++) {
    if (nId == pButton[i].nId) {
      PressButton(i);
      return;
    }
  }
}

static VOID ReleaseButtonById(UINT nId) {
  UINT i;
  for (i=0; i<nButtons; i++) {
    if (nId == pButton[i].nId) {
      ReleaseButton(i);
      return;
    }
  }
}

static VOID ReleaseAllButtons(VOID) { // release all buttons
  UINT i;
  for (i=0; i<nButtons; i++) {		// scan all buttons
    if (pButton[i].bDown)		// button pressed
      ReleaseButton(i);		        // release button
  }
  
  bPressed = FALSE;			// key not pressed
  bClicking = FALSE;			// var uButtonClicked not valid (no virtual or nohold key)
  uButtonClicked = 0;			// set var to default
}

VOID RefreshButtons(RECT *rc)  {

  UINT i;
  for (i=0; i<nButtons; i++)     {
    if (pButton[i].bDown
	&& rc->right  >  (LONG) (pButton[i].nOx)
	&& rc->bottom >  (LONG) (pButton[i].nOy)
	&& rc->left   <= (LONG) (pButton[i].nOx + pButton[i].nCx)
	&& rc->top    <= (LONG) (pButton[i].nOy + pButton[i].nCy))	{
      // on button type 3 and 5 clear complete key area before drawing
      if (pButton[i].nType == 3 || pButton[i].nType == 5) {
	UINT x0 = pButton[i].nOx;
	UINT y0 = pButton[i].nOy;
	emuBitBlt(hWnd, x0, y0, pButton[i].nCx, pButton[i].nCy,
		  hMainBM, x0, y0, GXcopy);
      }
      DrawButton(i);			// redraw pressed button
    }
  }
  emuFlush();
}

//################
//#
//#    Text buttons
//#
//################

VOID kmlButtonText6(UINT nId, BYTE *str, INT dx, INT dy) {
  BYTE i, c;
  BOOL end = FALSE;
  UINT x, y;

  //fprintf(stderr,"kmlButton: %d, %d, %s\n",nId,nButtons,str);
  
  if (nId >= nButtons) return;				// no button

  x = (UINT) ((INT)(pButton[nId].nOx) + dx) ;
  y = (UINT) ((INT)(pButton[nId].nOy) + dy) ;

  
  for (i = 0; i< 6; i++) {
    if (end) c = 0;
    else c = str[i];
    if (c == 0) end = TRUE;
    if ((c < 32) || (c > 127)) c = 0;
    else c -= 32;
    emuBitBlt(hWnd, x, y, 8, 12, hMainBM, 8 * c, bgY + bgH, GXcopy);
    x += 8;
  }
  emuFlush();
}

//################
//#
//#    Text annunciators
//#
//################

VOID kmlAnnunciatorText5(UINT nId, CHAR *str, INT dx, INT dy) {
  BYTE i, c;
  BOOL end = FALSE;
  UINT x, y;
	
  //fprintf(stderr,"kmlAnnun: %d, %ld, %s\n",nId,ARRAYSIZEOF(pAnnunciator),str);

  if (nId >= ARRAYSIZEOF(pAnnunciator)) return;		// no annunciator

  x = (UINT) ((INT)(pAnnunciator[nId].nOx) + dx) ;
  y = (UINT) ((INT)(pAnnunciator[nId].nOy) + dy) ;

  for (i = 0; i< 5; i++) {
    if (end) c = 0;
    else c = str[i];
    if (c == 0) end = TRUE;
    if ((c < 32) || (c > 127)) c = 0;
    else c -= 32;
    emuBitBlt(hWnd, x, y, 8, 11, hMainBM, 8 * c, bgY + bgH, GXcopy);
    x += 8;
  }
  emuFlush();
}

//################
//#
//#    Annunciators
//#
//################

VOID DrawAnnunciator(UINT nId, BOOL bOn) {
  UINT nSx,nSy;

  --nId;					// zero based ID
  if (nId >= ARRAYSIZEOF(pAnnunciator)) return;
  if (bOn) {
      nSx = pAnnunciator[nId].nDx;		// position of annunciator
      nSy = pAnnunciator[nId].nDy;
  } else {
    nSx = pAnnunciator[nId].nOx;		// position of background
    nSy = pAnnunciator[nId].nOy;
  }
  emuBitBlt(hWnd,
	    pAnnunciator[nId].nOx, pAnnunciator[nId].nOy,
	    pAnnunciator[nId].nCx, pAnnunciator[nId].nCy,
	    hMainBM,
	    nSx, nSy,
	    GXcopy);
}

//################
//#
//#    Mouse
//#
//################

static BOOL ClipButton(UINT x, UINT y, UINT nId) {
  x += bgX;						// source display offset
  y += bgY;

  return (pButton[nId].nOx<=x)
    && (pButton[nId].nOy<=y)
    &&(x<(pButton[nId].nOx+pButton[nId].nCx))
    &&(y<(pButton[nId].nOy+pButton[nId].nCy));
}

#define MK_LBUTTON 1 //FIXME

VOID MouseButtonDownAt(UINT nFlags, DWORD x, DWORD y) {
  UINT i;
  for (i=0; i<nButtons; i++) {
    if (ClipButton(x,y,i)) {
      if (pButton[i].dwFlags&BUTTON_NOHOLD) {
	if (nFlags & MK_LBUTTON) {	// use only with left mouse button
	  bClicking = TRUE;
	  uButtonClicked = i;
	  pButton[i].bDown = TRUE;
	  DrawButton(i);
	}
	return;
      }
      if (pButton[i].dwFlags&BUTTON_VIRTUAL) {
	if (!(nFlags&MK_LBUTTON))	// use only with left mouse button
	  return;
	bClicking = TRUE;
	uButtonClicked = i;
      }
      bPressed = TRUE;			// key pressed
      uLastPressedKey = i;		// save pressed key
      PressButton(i);
      return;
    }
  }
}

VOID MouseButtonUpAt(UINT nFlags, DWORD x, DWORD y) {
  UINT i;
  if (bPressed)	{			// emulator key pressed
    ReleaseAllButtons();		// release all buttons
    return;
  }
  for (i=0; i<nButtons; i++) {
      if (ClipButton(x,y,i)) {
	if ((bClicking)&&(uButtonClicked != i)) break;
	ReleaseButton(i);
	break;
      }
  }
  bClicking = FALSE;
  uButtonClicked = 0;
}

VOID MouseMovesTo(UINT nFlags, DWORD x, DWORD y) {
  if (!(nFlags & MK_LBUTTON)) return;	// left mouse key not pressed -> quit
  if ((bPressed) && !(ClipButton(x,y,uLastPressedKey)))
    // not on last pressed key
    ReleaseAllButtons();			// release all buttons
  if (!bClicking) return;			// normal emulation key -> quit

  if (pButton[uButtonClicked].dwFlags&BUTTON_NOHOLD) {
    if (ClipButton(x,y, uButtonClicked) != pButton[uButtonClicked].bDown) {
      pButton[uButtonClicked].bDown = !pButton[uButtonClicked].bDown;
      DrawButton(uButtonClicked);
    }
    return;
  }
  if (pButton[uButtonClicked].dwFlags&BUTTON_VIRTUAL) {
    if (!ClipButton(x,y, uButtonClicked)) {
      ReleaseButton(uButtonClicked);
      bClicking = FALSE;
      uButtonClicked = 0;
    }
    return;
  }
}


//################
//#
//#    Load and Initialize Script
//#
//################

static KmlBlock* LoadKMLGlobal(LPCTSTR szFilename) {
  INT       hFile;
  LPTSTR    lpBuf;
  KmlBlock *pBlock;
  DWORD     eToken;

  hFile = open(szFilename, 0, O_RDONLY);
  if (hFile < 0) return NULL;
  if ((lpBuf = MapKMLFile(hFile)) == NULL)  return NULL;

  InitLex(lpBuf);
  pBlock = NULL;
  eToken = Lex(LEX_BLOCK);
  if (eToken == TOK_GLOBAL) {
    pBlock = ParseBlock(eToken);
    if (pBlock) pBlock->pNext = NULL;
  }
  CleanLex();
  free(lpBuf);
  return pBlock;
}


BOOL InitKML(LPCTSTR szFilename, BOOL bNoLog) {
  INT       hFile;
  LPTSTR    lpBuf;
  KmlBlock *pBlock;
  BOOL      bOk = FALSE;

  KillKML();

  nBlocksIncludeLevel = 0;
  fprintf(stderr,"Reading %s\n", szFilename);
  hFile = open(szFilename, 0, O_RDONLY);
  if (hFile < 0)  {
      AddToLog(_T("Error while opening the file."));
      goto quit;
  }
  if ((lpBuf = MapKMLFile(hFile)) == NULL) goto quit;

  InitLex(lpBuf);
  pKml = ParseBlocks();
  CleanLex();

  free(lpBuf);
  if (pKml == NULL) goto quit;

  pBlock = pKml;
  while (pBlock)  {
    switch (pBlock->eType) {
    case TOK_BUTTON:
      InitButton(pBlock);
      break;
    case TOK_SCANCODE:
      nScancodes++;
      pVKey[pBlock->nId] = pBlock;
      break;
    case TOK_ANNUNCIATOR:
      InitAnnunciator(pBlock);
      break;
    case TOK_GLOBAL:
      InitGlobal(pBlock);
      break;
    case TOK_SCR:
      InitScreen(pBlock);
      break;
    case TOK_BACKGROUND:
      InitBackground(pBlock);
      break;
    default:
      fprintf(stderr,"Block %s Ignored.", GetStringOf(pBlock->eType));
      pBlock = pBlock->pNext;
    }
    pBlock = pBlock->pNext;
  }
		
  if (nCurrentRomType == 0) {
    AddToLog(_T("This KML Script doesn't specify the ROM Type."));
    goto quit;
  }
  if (pbyRom == NULL) {
    AddToLog("This KML Script doesn't specify the ROM to use, or the ROM could not be loaded.");
    goto quit;
  }
  CreateScreenBitmap();
  
  fprintf(stderr,"%i Buttons Defined\n", nButtons);
  fprintf(stderr,"%i Scancodes Defined\n", nScancodes);
  fprintf(stderr,"%i Annunciators Defined\n", nAnnunciators);
  
  bOk = TRUE;
  
 quit:

  return bOk;
}
