/*
 *   Kml.h
 *
 */

#define LEX_BLOCK   1
#define LEX_COMMAND 2
#define LEX_PARAM   3

typedef enum eTokenId
{
	TOK_NONE, //0
	TOK_ANNUNCIATOR, //1
	TOK_BACKGROUND, //2
	TOK_IFPRESSED, //3
	TOK_RESETFLAG, //4
	TOK_SCANCODE, //5
	TOK_HARDWARE, //6
	TOK_MENUITEM, //7
	TOK_INTEGER, //8
	TOK_SETFLAG, //9
	TOK_RELEASE, //10
	TOK_VIRTUAL, //11
	TOK_INCLUDE, //12
	TOK_STRING, //13
	TOK_GLOBAL, //14
	TOK_AUTHOR, //15
	TOK_BITMAP, //16
	TOK_OFFSET, //17
	TOK_BUTTON, //18
	TOK_IFFLAG, //19
	TOK_ONDOWN, //20
	TOK_NOHOLD, //21
	TOK_TITLE, //22
	TOK_OUTIN, //23
	TOK_PATCH, //24
	TOK_PRINT, //25
	TOK_DEBUG, //26
	TOK_COLOR, //27
	TOK_MODEL, //28
	TOK_CLASS, //29
	TOK_PRESS, //30
	TOK_TYPE, //31
	TOK_SIZE, //32
	TOK_DOWN, //33
	TOK_ZOOM, //34
	TOK_ELSE, //35
	TOK_ONUP, //36
	TOK_EOL, //37
	TOK_MAP, //38
	TOK_ROM, //39
	TOK_SCR, //40
	TOK_NOTFLAG, //41
	TOK_END //42
} TokenId;

#define TYPE_NONE    00
#define TYPE_INTEGER 01
#define TYPE_STRING  02

typedef struct KmlToken
{
	TokenId eId;
	DWORD  nParams;
	DWORD  nLen;
	TCHAR  szName[20];
} KmlToken;

typedef struct KmlLine
{
	struct KmlLine* pNext;
	TokenId eCommand;
	long nParam[6];
} KmlLine;

typedef struct KmlBlock
{
	TokenId eType;
	DWORD nId;
	struct KmlLine*  pFirstLine;
	struct KmlBlock* pNext;
} KmlBlock;

#define BUTTON_NOHOLD  0x0001
#define BUTTON_VIRTUAL 0x0002
typedef struct KmlButton
{
	UINT nId;
	BOOL bDown;
	UINT nType;
	DWORD dwFlags;
	UINT nOx, nOy;
	UINT nDx, nDy;
	UINT nCx, nCy;
	UINT nOut, nIn;
	KmlLine* pOnDown;
	KmlLine* pOnUp;
} KmlButton;

typedef struct KmlAnnunciator
{
	UINT nOx, nOy;
	UINT nDx, nDy;
	UINT nCx, nCy;
} KmlAnnunciator;

extern KmlBlock* pKml;
extern BOOL DisplayChooseKml(CHAR cType);
extern VOID FreeBlocks(KmlBlock* pBlock);
extern VOID DrawAnnunciator(UINT nId, BOOL bOn);
extern VOID RefreshButtons(RECT *rc);
extern VOID MouseButtonDownAt(UINT nFlags, DWORD x, DWORD y);
extern VOID MouseButtonUpAt(UINT nFlags, DWORD x, DWORD y);
extern VOID MouseMovesTo(UINT nFlags, DWORD x, DWORD y);
extern VOID RunKey(BYTE nId, BOOL bPressed);
extern BOOL InitKML(LPCTSTR szFilename, BOOL bNoLog);
extern VOID KillKML(VOID);

//
// specific function for 98x6
//
extern VOID kmlButtonText6(UINT nId, BYTE *str, INT dx, INT dy);
extern VOID kmlAnnunciatorText5(UINT nId, CHAR *str, INT dx, INT dy);
