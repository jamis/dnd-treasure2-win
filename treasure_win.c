#include <windows.h>
#include <commctrl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include <richedit.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/timeb.h>

#include "resource.h"

#include "bskenv.h"
#include "bskdb.h"
#include "bskparse.h"
#include "bskvalue.h"
#include "bskexec.h"
#include "bskstream.h"
#include "bskrule.h"
#include "bskthing.h"
#include "bskctgry.h"
#include "bskatdef.h"
#include "bskarray.h"
#include "bskutil.h"

#define PAGE_COUNT  ( 3 )
#define APP_NAME    "TG2"

#define WM_CONSOLE_CLOSING    ( WM_USER + 1 )

#define GENERATE_BY_LEVEL "rGenerateByELAndDisplay"
#define GENERATE_BY_TYPE  "rGenerateByTypeAndDisplay"
#define HOARD_DISPLAY     "rDisplayTreasureHoard"
#define INIT_OPTIONS      "rInitializeOptions"
#define ALL_SOURCES       "groupAllSources"

#define fSOURCES_CATEGORY    ( 0x00000001 )

typedef struct {
  HWND pages[ PAGE_COUNT ];
  int  current;

  BSKDatabase* db;
  char indexFile[512];
  char searchPaths[4096];

  BSKBOOL looping;
  BSKFLOAT runningTotal;

  int startBold;
  int endBold;

  int startItalic;
  int endItalic;
  
  int dblSpace;

  OPENFILENAME openData;

  HMENU editPopup;
  HWND  console;
  HWND  main;
} WINDATA;

typedef struct {
  BSKExecOpts opts;
  HWND        parent;
  HWND        main;
  BOOL        running;
  UINT        timerID;
} EXECDATA;

int           MessageBoxf( HWND hWnd, LPSTR title, UINT flags, LPSTR format, ... );
int WINAPI    WinMain( HINSTANCE, HINSTANCE, LPSTR, INT );
int           InitApplication( HINSTANCE );
int           InitInstance( LPSTR, UINT );
BOOL CALLBACK MainTreasureProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK RandomByELProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK RandomByItemTypeProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK OptionsPageProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK LoadWaitProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK ExecWaitProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK AboutDlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK ConsoleProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK CompileParmsProc( HWND, UINT, WPARAM, LPARAM );
int           InitMainWindow( HWND );
void          CloseMainWindow( HWND );
int           InitRandomELWindow( HWND );
int           InitRandomByTypeWindow( HWND );
void          GenerateTreasure( HWND );
void          GenerateRandomTreasure( HWND hWnd, WINDATA* data );
void          GenerateRandomByItemType( HWND hWnd, WINDATA* data );
void          LoadDatabase( HWND, char*, BOOL );
void          DoDatabaseLoad( HWND );
void          ExecRule( HWND, EXECDATA* );
void          InitExecWaitDlg( HWND, LPARAM );
void          CloseExecWaitDlg( HWND );
void          DoExecRule( EXECDATA* );
void          ClearConsole( HWND );
void          DoPopup( HWND, MSGFILTER* );
void          RefreshEditMenu( HWND, HMENU );
void          ProcessEdit( HWND, UINT );
void          UseCurrentSeed( HWND );
LONG          GetSeed( HWND, HWND );
void          InitGeneratorOptions( WINDATA*, BSKThing*, BSKUI32* );
void          InitConsole( HWND, LPARAM );
void          CloseConsole( HWND );
void          OpenConsoleWindow( HWND );
void          appendToConsole( HWND hWnd, BSKCHAR* text );
void          addToConsole( HWND hWnd, BSKCHAR* text );
void          setCheck( HWND hWnd, int ident, BSKBOOL check );
int           InitCompileWindow( HWND, LPARAM );
int           CloseCompileWindow( HWND, BSKBOOL );
void          CompileDatabase( HWND );
void          BrowseForSourceFile( HWND hWnd );
void          PostDBLoad( HWND hWnd );
void          PopulateSources( HWND hWnd, BSKDatabase* db );
void          CleanupOptions( BSKDatabase* db, BSKThing* options, BSKUI32 flags );
void          AddToTally( HWND hWnd, BSKValue* value );
void          Commify( BSKFLOAT number, BSKCHAR* buffer );
void          SaveConsole( HWND hWnd );
DWORD CALLBACK SaveConsoleCallback( DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb );
void          LoadConsole( HWND hWnd );
DWORD CALLBACK LoadConsoleCallback( DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb );
void          PrintConsole( HWND hWnd );
void          printStackTrace( BSKExecutionEnvironment* env, HWND hWnd );
void          SaveAllOptions( HWND hWnd );
void          RestoreAllOptions( HWND hWnd );
void          WriteProfileInt( char* appname, char* section, int value );
  
HINSTANCE ghInst;


void strrepl( char* buf, char* srch, char* repl ) {
  char* p;
  int   slen;
  int   rlen;

  slen = strlen( srch );
  rlen = strlen( repl );

  p = strstr( buf, srch );
  while( p != NULL ) {
    memmove( p+rlen, p+slen, strlen( p + slen ) + 1 );
    strncpy( p, repl, rlen );
    p = strstr( p+rlen, srch );
  }
}


int MessageBoxf( HWND hWnd, LPSTR title, UINT flags, LPSTR format, ... ) {
  char buffer[ 4096 ];
  va_list args;

  va_start( args, format );
  vsprintf( buffer, format, args );
  va_end( args );

  return MessageBox( hWnd, buffer, title, flags );
}


int
WINAPI
WinMain( HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         INT nCmdShow )
{
  if( InitApplication( hInstance ) == 0 )
  {
    MessageBox( NULL, "Could not initialize application.", "Error!", MB_OK | MB_ICONSTOP );
    return FALSE;
  }

  if( InitInstance( lpCmdLine, nCmdShow ) == 0 )
  {
	  MessageBox( NULL, "Could not initialize instance.", "Error!", MB_OK | MB_ICONSTOP );
	  return FALSE;
	}

  return FALSE;
}


int
InitApplication( HINSTANCE hInstance )
{
  ghInst = hInstance;

  srand( time( NULL ) );

  return TRUE;
}


int
InitInstance( LPSTR lpCmdLine, UINT nCmdShow )
{
  HMODULE mod;

  InitCommonControls();

  mod = LoadLibrary( "riched32.dll" );

  DialogBoxParam( ghInst, MAKEINTRESOURCE( IDD_TREASURE ), NULL, MainTreasureProc, 0 );

  FreeLibrary( mod );

  return TRUE;
}


void CloseMainWindow( HWND hWnd ) {
  WINDATA *data;

  SaveAllOptions( hWnd );

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  if( data->db != 0 ) {
    BSKDestroyDatabase( data->db );
  }
  if( data->console != 0 ) {
    DestroyWindow( data->console );
  }
  free( data );

  EndDialog( hWnd, 0 );
}


BOOL 
CALLBACK 
MainTreasureProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
  LPNMHDR  hdr;
  WINDATA *data;
  
  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  switch( msg )
  {
    case WM_INITDIALOG:
      return InitMainWindow( hWnd );

    case WM_CLOSE:
      CloseMainWindow( hWnd );
      return TRUE;

    case WM_COMMAND:
      switch( LOWORD( wParam ) ) {
        case IDCLOSE:
          CloseMainWindow( hWnd );
          return TRUE;
        case IDC_CLEAR:
          ClearConsole( hWnd );
          return TRUE;
        case IDC_FILE_LOAD_DATABASE:
          LoadDatabase( hWnd, 0, FALSE );
          return TRUE;
        case IDC_FILE_LOAD_CONSOLE:
          LoadConsole( hWnd );
          break;
        case IDC_SAVE_CONSOLE:
          SaveConsole( hWnd );
          break;
        case ID_HELP_ABOUT:
          DialogBox( ghInst, MAKEINTRESOURCE( IDD_ABOUT ), hWnd, AboutDlgProc );
          return TRUE;
        case ID_TOOLS_COMPILEDATABASE:
          CompileDatabase( hWnd );
          return TRUE;
        case IDC_GENERATE:
          GenerateTreasure( hWnd );
          return TRUE;
        case ID_TOOLS_DISPLAYCONSOLE:
          OpenConsoleWindow( hWnd );
          return TRUE;
        case IDC_EDIT_CUT:
        case IDC_EDIT_COPY:
        case IDC_EDIT_PASTE:
        case IDC_EDIT_CLEAR:
        case IDC_EDIT_SELECT_ALL:
        case IDC_COPY_ALL:
          ProcessEdit( hWnd, LOWORD( wParam ) );
          return TRUE;
        case IDC_FILE_PRINT:
          PrintConsole( hWnd );
          break;
        default:
          break;
      }
      break;

    case WM_INITMENUPOPUP:
      RefreshEditMenu( hWnd, (HMENU)wParam );
      break;

    case WM_NOTIFY:
      switch( LOWORD( wParam ) ) {
        case IDC_TAB:
          hdr = (LPNMHDR)lParam;
          if( hdr->code == TCN_SELCHANGE ) {
            ShowWindow( data->pages[ data->current ], SW_HIDE );
            data->current = TabCtrl_GetCurSel( GetDlgItem( hWnd, IDC_TAB ) );
            ShowWindow( data->pages[ data->current ], SW_SHOW );
            EnableWindow( GetDlgItem( hWnd, IDC_GENERATE ), ( data->current < 2 ) );
            return TRUE;
          }
          break;
      }
      break;
	}

	return 0;
}


int InitMainWindow( HWND hWnd ) {
  HWND     ctrl;
  HWND     tab;
  TC_ITEM  item;
  RECT     rect;
  WINDATA *data;
  HICON    icon;
  int      i;
  char*    p;

  icon = LoadIcon( ghInst, MAKEINTRESOURCE( IDI_TREASURE ) );
  SetClassLong( hWnd, GCL_HICON, (LONG)icon );

  data = (WINDATA*)malloc( sizeof( WINDATA ) );
  memset( data, 0, sizeof( WINDATA ) );
  SetWindowLong( hWnd, GWL_USERDATA, (LONG)data );

  data->main = hWnd;
  data->editPopup = GetSubMenu( GetMenu( hWnd ), 1 );

  data->startBold = data->endBold = -1;
  data->startItalic = data->endItalic = -1;
  data->dblSpace = TRUE;

  tab = GetDlgItem( hWnd, IDC_TAB );
  memset( &item, 0, sizeof( item ) );

  ClearConsole( hWnd );

  setCheck( hWnd, IDC_SHOWPARMS, BSKTRUE );
  setCheck( hWnd, IDC_RUNNINGTOTAL, BSKTRUE );

  item.mask = TCIF_TEXT;
  item.pszText = "Random by Level";
  TabCtrl_InsertItem( tab, 0, &item );
  item.pszText = "Random by Item Type";
  TabCtrl_InsertItem( tab, 1, &item );
  item.pszText = "Options";
  TabCtrl_InsertItem( tab, 2, &item );

  GetWindowRect( tab, &rect );
  TabCtrl_AdjustRect( tab, FALSE, &rect );

  ctrl = CreateDialog( ghInst,
                       MAKEINTRESOURCE( IDD_PAGE1 ),
                       hWnd,
                       RandomByELProc );
  SetWindowPos( ctrl, HWND_TOP, 
                rect.left-5, rect.top - 40, 
                ( rect.right - rect.left ),
                ( rect.bottom - rect.top ),
                SWP_SHOWWINDOW );
  data->pages[ 0 ] = ctrl;

  ctrl = CreateDialog( ghInst,
                       MAKEINTRESOURCE( IDD_PAGE2 ),
                       hWnd,
                       RandomByItemTypeProc );
  SetWindowPos( ctrl, HWND_TOP, 
                rect.left-5, rect.top - 40, 
                ( rect.right - rect.left ),
                ( rect.bottom - rect.top ),
                SWP_SHOWWINDOW );
  data->pages[ 1 ] = ctrl;

  ctrl = CreateDialog( ghInst,
                       MAKEINTRESOURCE( IDD_PAGE3 ),
                       hWnd,
                       OptionsPageProc );
  SetWindowPos( ctrl, HWND_TOP, 
                rect.left-5, rect.top - 40, 
                ( rect.right - rect.left ),
                ( rect.bottom - rect.top ),
                SWP_SHOWWINDOW );
  data->pages[ 2 ] = ctrl;

  ShowWindow( data->pages[1], SW_HIDE );
  ShowWindow( data->pages[2], SW_HIDE );

  data->current = 0;

  OpenConsoleWindow( hWnd );

  GetCurrentDirectory( sizeof( data->indexFile ), data->indexFile );
  i = strlen( data->indexFile );
  if( ( i < 1 ) || ( data->indexFile[ i - 1 ] != '\\' ) ) {
    strcat( data->indexFile, "\\" );
  }

  /* build the search paths */
  p = data->searchPaths;
  strcpy( p, data->indexFile ); 
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\standard" );
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\snfist" );
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\scitadel" );
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\dragon" );
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\nbomt" );
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\defenders" );
    p += strlen( p ) + 1; *p = 0;
  strcpy( p, data->indexFile );
  strcat( p, "dat\\tnblood" );
    p += strlen( p ) + 1; *p = 0;

  /* build the index file */
  strcat( data->indexFile, "dat\\standard\\index.bdb" );

  memset( &(data->openData), 0, sizeof( data->openData ) );
  data->openData.lStructSize = sizeof( data->openData );
  data->openData.hwndOwner = hWnd;
  data->openData.lpstrFilter = "Basilisk Files\0*.BSK;*.BDB\0Basilisk Script Files (*.bsk)\0*.BSK\0Compiled Basilisk Files (*.bdb)\0*.BDB\0\0";
  data->openData.lpstrFile = data->indexFile;
  data->openData.nMaxFile = sizeof( data->indexFile );
  data->openData.Flags = OFN_FILEMUSTEXIST;

  LoadDatabase( hWnd, data->indexFile, FALSE );
  RestoreAllOptions( hWnd );

  return TRUE;
}


BOOL CALLBACK RandomByELProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  switch( msg )
  {
    case WM_INITDIALOG:
      return InitRandomELWindow( hWnd );
	}

	return FALSE;
}


BOOL CALLBACK RandomByItemTypeProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  switch( msg )
  {
    case WM_INITDIALOG:
      return InitRandomByTypeWindow( hWnd );
  }

	return FALSE;
}


BOOL CALLBACK OptionsPageProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  switch( msg )
  {
    case WM_INITDIALOG:
      return FALSE;
  }

	return FALSE;
}


int InitRandomELWindow( HWND hWnd ) {
  int  i;
  char buffer[ 20 ];
  HWND ctrl;

  ctrl = GetDlgItem( hWnd, IDC_LEVEL );
  for( i = 0; i < 30; i++ ) {
    sprintf( buffer, "%d", i+1 );
    SendMessage( ctrl, CB_ADDSTRING, 0, (LPARAM)buffer );
  }
  SendMessage( ctrl, CB_SETCURSEL, 0, 0 );
  SetFocus( ctrl );

  SetWindowText( GetDlgItem( hWnd, IDC_COINS ), "100" );
  SetWindowText( GetDlgItem( hWnd, IDC_GOODS ), "100" );
  SetWindowText( GetDlgItem( hWnd, IDC_ITEMS ), "100" );

  return TRUE;
}


int InitRandomByTypeWindow( HWND hWnd ) {
  HWND ctrl;

  ctrl = GetDlgItem( hWnd, IDC_COUNT );
  SetWindowText( ctrl, "1" );

  ctrl = GetDlgItem( hWnd, IDC_MINOR );
  SendMessage( ctrl, BM_SETCHECK, BST_CHECKED, 0 );
  SetFocus( ctrl );

  ctrl = GetDlgItem( hWnd, IDC_ARMOR );
  SendMessage( ctrl, BM_SETCHECK, BST_CHECKED, 0 );

  return TRUE;
}


void setMenuItem( HMENU menu, int ident, int grayed ) {
  MENUITEMINFO mii;

  mii.cbSize = sizeof( mii );
  mii.fMask = MIIM_STATE;
  mii.fState = ( grayed ? MFS_GRAYED : MFS_ENABLED );
  SetMenuItemInfo( menu, ident, FALSE, &mii );
}


void setCheck( HWND hWnd, int ident, BSKBOOL check ) {
  HWND ctrl;

  ctrl = GetDlgItem( hWnd, ident );
  SendMessage( ctrl, BM_SETCHECK, ( check ? BST_CHECKED : BST_UNCHECKED ), 0 );
}


int isChecked( HWND hWnd, int ident ) {
  HWND ctrl;

  ctrl = GetDlgItem( hWnd, ident );
  return ( SendMessage( ctrl, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
}


void GenerateTreasure( HWND hWnd ) {
  WINDATA*      data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  if( data->db == 0 ) {
    return;
  }

  SendMessage( GetDlgItem( data->console, IDC_TEXT ), WM_SETREDRAW, FALSE, 0 );

  if( isChecked( hWnd, IDC_AUTOCLEAR ) ) {
    ClearConsole( hWnd );
  }

  switch( data->current ) {
    case 0: /* random treasure generation */
      GenerateRandomTreasure( data->pages[0], data );
      break;
    case 1: /* random by specific item type */
      GenerateRandomByItemType( data->pages[1], data );
      break;
    default:
      return;
  }

  addToConsole( hWnd, "----------------------~n~n" );
  SendMessage( GetDlgItem( data->console, IDC_TEXT ), WM_SETREDRAW, TRUE, 0 );
  InvalidateRect( GetDlgItem( data->console, IDC_TEXT ), NULL, TRUE );
}


void appendToConsole( HWND hWnd, BSKCHAR* text ) {
  HWND ctrl;
  BSKUI32 len;
  WINDATA *data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  ctrl = GetDlgItem( data->console, IDC_TEXT );

  len = GetWindowTextLength( ctrl );
  SendMessage( ctrl, EM_SETSEL, len, len );
  SendMessage( ctrl, EM_REPLACESEL, FALSE, (LPARAM)text );
  SendMessage( ctrl, EM_SCROLLCARET, 0, 0 );
}


void addToConsole( HWND hWnd, BSKCHAR* text ) {
  BSKCHAR temp[256];
  int     i;
  int     p;
  CHARFORMAT cfmt;
  HWND ctrl;
  WINDATA*      data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  ctrl = GetDlgItem( data->console, IDC_TEXT );

  cfmt.cbSize = sizeof( cfmt );
  cfmt.dwMask = 0;

  if( data->startBold < 0 ) {
    cfmt.dwMask = CFM_BOLD;
    cfmt.dwEffects = 0;
    SendMessage( ctrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfmt );
  }

  if( data->startItalic < 0 ) {
    cfmt.dwMask = CFM_ITALIC;
    cfmt.dwEffects = 0;
    SendMessage( ctrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfmt );
  }

  i = 0;
  for( p = 0; text[p] != 0; p++ ) {
    if( text[p] == '~' ) {
      p++;
      switch( text[p] ) {
        case '~':
          temp[i++] = '~';
          break;
        case 'n':
          temp[i++] = '\r';
          temp[i++] = '\n';
          data->dblSpace = TRUE;
          break;
        case 'B':
          if( data->startBold < 0 ) {
            data->startBold = GetWindowTextLength( ctrl ) + i;
          }
          break;
        case 'b':
          data->endBold = GetWindowTextLength( ctrl ) + i;
          temp[i] = 0;
          appendToConsole( hWnd, temp );
          SendMessage( ctrl, EM_SETSEL, data->startBold, data->endBold ); 
          cfmt.dwMask = CFM_BOLD;
          cfmt.dwEffects = CFE_BOLD;
          SendMessage( ctrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfmt );
          data->startBold = -1;
          SendMessage( ctrl, EM_SETSEL, data->endBold, data->endBold );
          cfmt.dwMask = CFM_BOLD;
          cfmt.dwEffects = 0;
          SendMessage( ctrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfmt );
          i = 0;
          break;
        case 'I':
          if( data->startItalic < 0 ) {
            data->startItalic = GetWindowTextLength( ctrl ) + i;
          }
          break;
        case 'i':
          data->endItalic = GetWindowTextLength( ctrl ) + i;
          temp[i] = 0;
          appendToConsole( hWnd, temp );
          SendMessage( ctrl, EM_SETSEL, data->startItalic, data->endItalic ); 
          cfmt.dwMask = CFM_ITALIC;
          cfmt.dwEffects = CFE_ITALIC;
          SendMessage( ctrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfmt );
          data->startItalic = -1;
          SendMessage( ctrl, EM_SETSEL, data->endItalic, data->endItalic );
          cfmt.dwMask = CFM_ITALIC;
          cfmt.dwEffects = 0;
          SendMessage( ctrl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfmt );
          i = 0;
          break;
        case 'C':
        case 'c':
        case 'G':
        case 'g':
          break;
      }
    } else {
      if( text[p] == ' ' && data->dblSpace ) {
        temp[i++] = ' ';
      } else {
        data->dblSpace = FALSE;
      }
      temp[i++] = text[p];
    }
    if( ( i > 250 ) || ( text[p+1] == 0 ) ) {
      temp[ i ] = 0;
      appendToConsole( hWnd, temp );
      i = 0;
    }
  }
}


BSKI32 rteHandler( BSKI32 code,
                   BSKCHAR* msg,
                   BSKExecutionEnvironment* env,
                   BSKNOTYPE userData )
{
  BSKRule* rule;
  BSKCHAR name[100];
  HWND hWnd = (HWND)userData;
  BSKCHAR buffer[100];

  addToConsole( hWnd, "\r\n" );
  switch( code ) {
    case RTE_SUCCESS:
      addToConsole( hWnd, "*** no error" );
      break;
    case RTE_INVALID_RULE:
      addToConsole( hWnd, "*** invalid rule" );
      break;
    case RTE_BUG:
      addToConsole( hWnd, "*** bug detected" );
      break;
    case RTE_STACK_UNDERFLOW:
      addToConsole( hWnd, "*** stack underflow" );
      break;
    case RTE_INVALID_OPERANDS:
      addToConsole( hWnd, "*** invalid operands" );
      break;
    case RTE_DIVIDE_BY_ZERO:
      addToConsole( hWnd, "*** divide by zero" );
      break;
    case RTE_UNKNOWN_INSTRUCTION:
      addToConsole( hWnd, "*** unknown instruction" );
      break;
    case RTE_WRONG_PARAM_COUNT:
      addToConsole( hWnd, "*** wrong parameter count" );
      break;
    case RTE_DOMAIN_ERROR:
      addToConsole( hWnd, "*** domain error" );
      break;
    case RTE_WRONG_UNITS:
      addToConsole( hWnd, "*** wrong units" );
      break;
    case RTE_CALL_OF_NONFUNCTION:
      addToConsole( hWnd, "*** call of non-function" );
      break;
  }

  addToConsole( hWnd, " (" );
  if( env->stackFrame == 0 ) {
    addToConsole( hWnd, "main: " );
  } else {
    rule = BSKFindRule( env->db->rules, env->stackFrame->ruleId );
    BSKGetIdentifier( env->db->idTable, rule->id, name, sizeof( name ) );
    if( rule->file != 0 ) {
      sprintf( buffer, "%s:", rule->file );
      addToConsole( hWnd, buffer );
    }
    sprintf( buffer, "%s:", name );
    addToConsole( hWnd, buffer );
  }
  sprintf( buffer, "%d)", env->line );
  addToConsole( hWnd, buffer );

  if( msg != 0 ) {
    sprintf( buffer, " (%s)", msg );
    addToConsole( hWnd, buffer );
  }
  addToConsole( hWnd, "\r\n" );
  
  printStackTrace( env, hWnd );
  addToConsole( hWnd, "\r\n\r\n" );

  return 0;
}


BSKI32 treasureConsole( BSKCHAR* msg,
                        BSKExecutionEnvironment* env,
                        BSKNOTYPE userData )
{
  HWND hWnd = (HWND)userData;
  addToConsole( hWnd, msg );
  return 0;
}


void GenerateRandomTreasure( HWND hWnd, WINDATA* data ) {
  HWND ctrl;
  int  level;
  EXECDATA execData;
  BSKValue* parms[6];
  BSKValue retVal;
  BSKValue parm1;
  BSKValue parm2;
  BSKValue parm3;
  BSKValue parm4;
  BSKValue parm5;
  BSKValue parm6;
  BSKUI32 dpyId;
  BSKBOOL halt;
  BSKThing* options;
  BSKCHAR buffer[100];
  BSKCHAR parmDesc[512];
  BSKBOOL showParms;
  BSKUI32 flags;

  ctrl = GetDlgItem( hWnd, IDC_LEVEL );
  level = SendMessage( ctrl, CB_GETCURSEL, 0, 0 );
  if( level == CB_ERR ) {
    return;
  }
  level++;

  showParms = isChecked( data->main, IDC_SHOWPARMS );

  if( showParms ) {
    parmDesc[0] = 0;
    sprintf( parmDesc, "~IEL %d, ", level );
  }

  memset( &execData.opts, 0, sizeof( execData.opts ) );
  execData.opts.ruleId = BSKFindIdentifier( data->db->idTable, GENERATE_BY_LEVEL );
  if( execData.opts.ruleId < 1 ) {
    MessageBoxf( hWnd, "Error", MB_OK,
                 "The data file must contain a rule named %s",
                 GENERATE_BY_LEVEL );
    return;
  }

  BSKSetValueNumber( &parm1, level );
  parms[0] = &parm1;

  dpyId = BSKFindIdentifier( data->db->idTable, HOARD_DISPLAY );
  if( dpyId < 1 ) {
    MessageBoxf( hWnd, "Error", MB_OK,
                 "The data file must contain a rule named %s "
                 "that takes one parameter (a treasure hoard) and returns the "
                 "total value of the hoard.\n", HOARD_DISPLAY );
    return;
  }

  GetWindowText( GetDlgItem( hWnd, IDC_COINS ), buffer, sizeof( buffer ) );
  BSKSetValueNumber( &parm2, atof(buffer)/100.0 );
  parms[1] = &parm2;
  if( showParms ) {
    strcat( parmDesc, " coins: " );
    strcat( parmDesc, buffer );
    strcat( parmDesc, "%, " );
  }

  GetWindowText( GetDlgItem( hWnd, IDC_GOODS ), buffer, sizeof( buffer ) );
  BSKSetValueNumber( &parm3, atof(buffer)/100.0 );
  parms[2] = &parm3;
  if( showParms ) {
    strcat( parmDesc, " goods: " );
    strcat( parmDesc, buffer );
    strcat( parmDesc, "%, " );
  }

  GetWindowText( GetDlgItem( hWnd, IDC_ITEMS ), buffer, sizeof( buffer ) );
  BSKSetValueNumber( &parm4, atof(buffer)/100.0 );
  parms[3] = &parm4;
  if( showParms ) {
    strcat( parmDesc, " items: " );
    strcat( parmDesc, buffer );
    strcat( parmDesc, "%~i~n" );
  }

  if( showParms ) {
    addToConsole( GetParent(hWnd), parmDesc );
  }

  parm5.type = VT_RULE;
  parm5.datum = BSKFindRule( data->db->rules, dpyId );
  parms[4] = &parm5;

  options = BSKNewThing( 0 );
  InitGeneratorOptions( data, options, &flags );
  parm6.type = VT_THING;
  parm6.datum = options;
  parms[5] = &parm6;

  halt = BSKFALSE;

  execData.opts.console = treasureConsole;
  execData.opts.db = data->db;
  execData.opts.errorHandler = rteHandler;
  execData.opts.parameterCount = 6;
  execData.opts.parameters = parms;
  execData.opts.rval = &retVal;
  execData.opts.userData = GetParent( hWnd );
  execData.opts.halt = &halt;

  ExecRule( GetParent( hWnd ), &execData ), 
  AddToTally( GetParent( hWnd ), &retVal );

  BSKInvalidateValue( &parm1 );
  BSKInvalidateValue( &parm2 );
  BSKInvalidateValue( &parm3 );

  CleanupOptions( data->db, options, flags );
  BSKCleanupReturnValue( &retVal );
}


void GenerateRandomByItemType( HWND hWnd, WINDATA* data ) {
  int  count;
  char buffer[100];
  BSKUI16  i;
  BSKArray* magnitudes;
  BSKArray* itemTypes;
  BSKValue  parm1;
  BSKValue  parm2;
  BSKValue  parm3;
  BSKValue  parm4;
  BSKValue  parm5;
  BSKValue  retVal;
  BSKValue  temp;
  BSKValue* parms[5];
  EXECDATA  execData;
  BSKBOOL   halt;
  BSKUI32 dpyId;
  BSKThing* options;
  BSKBOOL showParms;
  BSKCHAR parmBuffer[256];
  BSKUI32 flags;

  showParms = isChecked( data->main, IDC_SHOWPARMS );

  memset( &execData.opts, 0, sizeof( execData.opts ) );
  execData.opts.ruleId = BSKFindIdentifier( data->db->idTable, GENERATE_BY_TYPE );
  if( execData.opts.ruleId < 1 ) {
    MessageBoxf( hWnd, "Error", MB_OK,
                 "The data file must contain a rule named %s",
                 GENERATE_BY_TYPE );
    return;
  }

  GetWindowText( GetDlgItem( hWnd, IDC_COUNT ), buffer, sizeof( buffer ) );
  count = atoi( buffer );
  if( count < 1 ) {
    count = 1;
  }
  sprintf( buffer, "%d", count );
  SetWindowText( GetDlgItem( hWnd, IDC_COUNT ), buffer );

  if( showParms ) {
    addToConsole( GetParent( hWnd ), "~ICount: " );
    addToConsole( GetParent( hWnd ), buffer );
    addToConsole( GetParent( hWnd ), "~i~n" );
  }

  BSKSetValueNumber( &parm1, count );
  parms[0] = &parm1;

  halt = BSKFALSE;

  magnitudes = BSKNewArray( 0 );
  i = 0;

  parmBuffer[0] = 0;
  if( isChecked( hWnd, IDC_MINOR ) ) {
    BSKSetValueString( &temp, "minor" );
    strcat( parmBuffer, "minor" );
    BSKPutElement( magnitudes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_MEDIUM ) ) {
    BSKSetValueString( &temp, "medium" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "medium" );
    BSKPutElement( magnitudes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_MAJOR ) ) {
    BSKSetValueString( &temp, "major" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "major" );
    BSKPutElement( magnitudes, i++, &temp );
    BSKInvalidateValue( &temp );
  }

  if( i == 0 ) {
    MessageBoxf( hWnd, "Warning", MB_OK, "You did not choose a magnitude, so \"minor\" "
                 "will be chosen by default.\n"
                 "To prevent this message from appearing, please choose a magnitude "
                 "before generating a treasure." );
    BSKSetValueString( &temp, "minor" );
    strcpy( parmBuffer, "minor" );
    BSKPutElement( magnitudes, i++, &temp );
    BSKInvalidateValue( &temp );
  }

  if( showParms ) {
    addToConsole( GetParent( hWnd ), "~IMagnitudes:  " );
    addToConsole( GetParent( hWnd ), parmBuffer );
    addToConsole( GetParent( hWnd ), "~i~n" );
  }

  parm2.type = VT_ARRAY;
  parm2.datum = magnitudes;
  parms[1] = &parm2;

  itemTypes = BSKNewArray( 0 );
  i = 0;

  parmBuffer[0] = 0;
  if( isChecked( hWnd, IDC_ARMOR ) ) {
    BSKSetValueString( &temp, "armor" );
    strcat( parmBuffer, "armor" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_POTION ) ) {
    BSKSetValueString( &temp, "potion" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "potion" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_RING ) ) {
    BSKSetValueString( &temp, "ring" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "ring" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_ROD ) ) {
    BSKSetValueString( &temp, "rod" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "rod" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_SCROLL ) ) {
    BSKSetValueString( &temp, "scroll" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "scroll" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_STAFF ) ) {
    BSKSetValueString( &temp, "staff" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "staff" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_WAND ) ) {
    BSKSetValueString( &temp, "wand" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "wand" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_WEAPON ) ) {
    BSKSetValueString( &temp, "weapon" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "weapon" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }
  if( isChecked( hWnd, IDC_WONDROUS ) ) {
    BSKSetValueString( &temp, "wondrous item" );
    if( i > 0 ) strcat( parmBuffer, ", " );
    strcat( parmBuffer, "wondrous item" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }

  if( i == 0 ) {
    MessageBoxf( hWnd, "Warning", MB_OK, "You did not choose an item type, so \"armor\" "
                 "will be chosen by default.\n"
                 "To prevent this message from appearing, please choose an item type "
                 "before generating a treasure." );
    BSKSetValueString( &temp, "armor" );
    strcpy( parmBuffer, "armor" );
    BSKPutElement( itemTypes, i++, &temp );
    BSKInvalidateValue( &temp );
  }

  if( showParms ) {
    addToConsole( GetParent( hWnd ), "~IItem Types:  " );
    addToConsole( GetParent( hWnd ), parmBuffer );
    addToConsole( GetParent( hWnd ), "~i~n" );
  }

  parm3.type = VT_ARRAY;
  parm3.datum = itemTypes;
  parms[2] = &parm3;

  dpyId = BSKFindIdentifier( data->db->idTable, HOARD_DISPLAY );
  if( dpyId < 1 ) {
    MessageBoxf( hWnd, "Error", MB_OK,
                 "The data file must contain a rule named %s "
                 "that takes one parameter (a treasure hoard) and returns the "
                 "total value of the hoard.\n", HOARD_DISPLAY );
    return;
  }

  parm4.type = VT_RULE;
  parm4.datum = BSKFindRule( data->db->rules, dpyId );
  parms[3] = &parm4;

  options = BSKNewThing( 0 );
  InitGeneratorOptions( data, options, &flags );
  parm5.type = VT_THING;
  parm5.datum = options;
  parms[4] = &parm5;

  execData.opts.console = treasureConsole;
  execData.opts.db = data->db;
  execData.opts.errorHandler = rteHandler;
  execData.opts.parameterCount = 5;
  execData.opts.parameters = parms;
  execData.opts.rval = &retVal;
  execData.opts.userData = GetParent( hWnd );
  execData.opts.halt = &halt;

  ExecRule( GetParent( hWnd ), &execData );
  AddToTally( GetParent( hWnd ), &retVal );

  BSKInvalidateValue( &parm1 );
  BSKInvalidateValue( &parm2 );
  BSKInvalidateValue( &parm3 );
  BSKInvalidateValue( &parm4 );

  BSKDestroyArray( magnitudes );
  BSKDestroyArray( itemTypes );

  CleanupOptions( data->db, options, flags );
  BSKCleanupReturnValue( &retVal );
}


BSKI32 parseErrorHook( BSKI32 code,
                       BSKDatabase* db,
                       BSKCHAR* file,
                       BSKToken* token,
                       BSKUI32 data,
                       BSKNOTYPE userData )
{
  BSKCHAR buffer1[ 128 ];
  BSKCHAR buffer2[ 128 ];
  BSKCHAR text[ 1024 ];

  switch( code ) {
    case PE_NOERROR:
      strcpy( text, "*** no error\r\n" );
      break;
    case PE_UNEXPECTED_TOKEN:
      BSKGetTokenDescription( (BSKTokenId)data, buffer1, sizeof( buffer1 ) );
      BSKGetTokenDescription( token->type, buffer2, sizeof( buffer2 ) );
      sprintf( text, "*** [%s:%d,%d] expected %s, found %s\r\n", file, token->row, token->col, buffer1, buffer2 );
      break;
    case PE_REDEFINED_IDENTIFIER:
      BSKGetIdentifier( db->idTable, data, buffer1, sizeof( buffer1 ) );
      sprintf( text, "*** [%s:%d,%d] identifier '%s' redefined\r\n", file, token->row, token->col, buffer1 );
      break;
    case PE_UNDECLARED_IDENTIFIER:
      BSKGetIdentifier( db->idTable, data, buffer1, sizeof( buffer1 ) );
      sprintf( text, "*** [%s:%d,%d] identifier '%s' undeclared\r\n", file, token->row, token->col, buffer1 );
      break;
    case PE_WRONG_TYPE:
      BSKGetIdentifier( db->idTable, data, buffer1, sizeof( buffer1 ) );
      sprintf( text, "*** [%s:%d,%d] identifier '%s' of wrong type\r\n", file, token->row, token->col, buffer1 );
      break;
    case PE_BUG_DETECTED:
      sprintf( text, "*** [%s:%d,%d] bug detected (%s)\r\n", file, token->row, token->col, (BSKCHAR*)data );
      break;
    case PE_TOO_MANY_ATTRIBUTES:
      sprintf( text, "*** [%s:%d,%d] too many attributes\r\n", file, token->row, token->col );
      break;
    case PE_EXIT_LOOP_NOT_IN_LOOP:
      sprintf( text, "*** [%s:%d,%d] \"exit loop\" may only be used in a loop context\r\n", file, token->row, token->col );
      break;
    case PE_CANNOT_OPEN_FILE:
      sprintf( text, "*** [%s:%d,%d] cannot open file '%s'\r\n", file, token->row, token->col, (BSKCHAR*)data );
      break;
    case PE_FORWARD_NOT_DEFINED:
      BSKGetIdentifier( db->idTable, data, buffer1, sizeof( buffer1 ) );
      sprintf( text, "*** forwarded identifier '%s' never defined\r\n", buffer1 );
      break;
    default:
      sprintf( text, "*** [%s:%d,%d] unknown error %d\r\n", file, token->row, token->col, code );
  }

  addToConsole( (HWND)userData, text );

  return 0;
}


void LoadDatabase( HWND hWnd, char* useFile, BOOL compile ) {
  WINDATA *data;
  HWND dlg;
  MSG msg;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  if( useFile == NULL ) {
    if( !GetOpenFileName( &data->openData ) ) {
      DWORD d;

      d = CommDlgExtendedError();
      if( d != 0 ) {
        MessageBoxf( hWnd, "Error", MB_OK, "GetOpenFileName caused an error: %d", d );
      }
      return;
    }
  }

  if( compile ) {
    char* p;
    p = data->indexFile + strlen( data->indexFile );
    while( ( p > data->indexFile ) && ( *p != '.' ) ) {
      p--;
    }
    if( BSKStrCaseCmp( p, ".bdb" ) == 0 ) {
      MessageBoxf( hWnd, "Notice", MB_OK, "The file you selected is already compiled." );
      return;
    }
  }

  dlg = CreateDialog( ghInst, MAKEINTRESOURCE( IDD_LOADWAIT ), hWnd, LoadWaitProc );
  if( dlg == NULL ) {
    MessageBoxf( hWnd, "ERROR", MB_OK, "Could not load database" );
    return;
  }

  ShowWindow( dlg, SW_SHOW );

  data->looping = BSKTRUE;
  _beginthread( DoDatabaseLoad, 0, hWnd );

  while( data->looping ) {
    GetMessage( &msg, 0, 0, 0 );
    TranslateMessage( &msg );
    DispatchMessage( &msg );
  }

  DestroyWindow( dlg );

  if( data->db != 0 ) {
    PostDBLoad( hWnd );
  }

  if( ( data->db != 0 ) && compile ) {
    char* p;
    BSKStream* stream;

    p = data->indexFile + strlen( data->indexFile );
    while( p > data->indexFile && *p != '.' ) {
      p--;
    }
    if( *p == '.' ) {
      p++;
    } else {
      p = data->indexFile + strlen( data->indexFile );
      *p = '.';
      p++;
    }

    strcpy( p, "bdb" );
    stream = BSKStreamOpenFile( data->indexFile, "wb" );
    if( stream == 0 ) {
      MessageBoxf( hWnd, "Notice", MB_OK, "The file '%s' could not be opened for writing", data->indexFile );
      return;
    }

    BSKSerializeDatabaseOut( data->db, stream );

    stream->close( stream );

    MessageBoxf( hWnd, "Done!", MB_OK, "The requested database has been compiled and loaded, "
                 "and is now the current database." );
  }
}


BOOL CALLBACK LoadWaitProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  HWND ctrl;
  int  i;
  char buf[20];

  switch( msg )
  {
    case WM_INITDIALOG:
      ctrl = GetDlgItem( hWnd, IDC_COUNTER );
      SetWindowText( ctrl, "00" );
      SetTimer( hWnd, 0, 1000, NULL );
      InvalidateRect( ctrl, NULL, TRUE );
      return FALSE;

    case WM_TIMER:
      ctrl = GetDlgItem( hWnd, IDC_COUNTER );
      GetWindowText( ctrl, buf, sizeof( buf ) );
      i = atoi( buf ) + 1;
      sprintf( buf, "%02d", i );
      SetWindowText( ctrl, buf );
      SetTimer( hWnd, 0, 1000, NULL );
      InvalidateRect( ctrl, NULL, TRUE );
      return TRUE;
	}

	return FALSE;
}


BOOL CALLBACK ExecWaitProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  HWND ctrl;
  int  i;
  char buf[20];
  EXECDATA* dta;

  dta = (EXECDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  switch( msg )
  {
    case WM_INITDIALOG:
      InitExecWaitDlg( hWnd, lParam );
      return FALSE;

    case WM_COMMAND:
      switch( LOWORD( wParam ) ) {
        case IDCANCEL:
          CloseExecWaitDlg( hWnd );
          return FALSE;
      }
      break;

    case WM_TIMER:
      ctrl = GetDlgItem( hWnd, IDC_COUNTER );
      GetWindowText( ctrl, buf, sizeof( buf ) );
      i = atoi( buf ) + 1;
      sprintf( buf, "%02d", i );
      SetWindowText( ctrl, buf );
      dta->timerID = SetTimer( hWnd, 0, 1000, NULL );
      InvalidateRect( ctrl, NULL, TRUE );
      return TRUE;
	}

	return FALSE;
}


BOOL CALLBACK AboutDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  char buffer[1024];

  switch( msg )
  {
    case WM_INITDIALOG:
      LoadString( ghInst, IDS_THANKS, buffer, sizeof( buffer ) );
      SetWindowText( GetDlgItem( hWnd, IDC_THANKS ), buffer );
      return FALSE;

    case WM_CLOSE:
      EndDialog( hWnd, 0 );
      return TRUE;

    case WM_COMMAND:
      switch( LOWORD( wParam ) ) {
        case IDOK:
          EndDialog( hWnd, 0 );
          return TRUE;
      }
      break;
	}

	return FALSE;
}


void DoDatabaseLoad( HWND hWnd ) {
  WINDATA *data;
  BSKI32   rc;
  BSKStream* stream;
  BSKCHAR  path[512];

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  if( data->db != 0 ) {
    BSKDestroyDatabase( data->db );
  }

  strcpy( path, data->indexFile );
  rc = strlen( path );
  while( rc >= 0 ) {
    if( path[ rc ] == '\\' ) {
      break;
    }
    rc--;
  }

  if( rc >= 0 ) {
    path[rc] = 0;
    chdir( path );
  }

  stream = BSKStreamOpenFile( data->indexFile, "rb" );
  if( stream == 0 ) {
    MessageBoxf( hWnd, "Error", MB_OK, "Could not open '%s'!", data->indexFile );
  } else {
    if( strstr( data->indexFile, ".bdb" ) ) {
      data->db = BSKSerializeDatabaseIn( stream );
    } else {
      data->db = BSKNewDatabase();

      rc = BSKParse( stream, data->db, data->searchPaths, parseErrorHook, (BSKNOTYPE)hWnd );

      if( rc != 0 ) {
        addToConsole( hWnd, "\r\nError were detected while attempting to load the database.\r\n"
                              "The database was not loaded.\r\n" );
        BSKDestroyDatabase( data->db );
        data->db = 0;
      }
    }

    stream->close( stream );
  }

  data->looping = BSKFALSE;
}


void InitExecWaitDlg( HWND hWnd, LPARAM lParam ) {
  HWND ctrl;
  EXECDATA* dta = (EXECDATA*)lParam;

  SetWindowLong( hWnd, GWL_USERDATA, lParam );
  dta->parent = hWnd;

  ctrl = GetDlgItem( hWnd, IDC_COUNTER );
  SetWindowText( ctrl, "00" );
  dta->timerID = SetTimer( hWnd, 0, 1000, NULL );
  InvalidateRect( ctrl, NULL, TRUE );

  dta->running = TRUE;

  _beginthread( DoExecRule, 0, dta );
}


void CloseExecWaitDlg( HWND hWnd ) {
  EXECDATA* dta;
  MSG msg;
  HWND ctrl;

  ctrl = GetDlgItem( hWnd, IDC_COUNTER );
  SetWindowText( ctrl, "Halting..." );

  dta = (EXECDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  *(dta->opts.halt) = BSKTRUE;

  while( dta->running ) {
    GetMessage( &msg, 0, 0, 0 );
    TranslateMessage( &msg );
    DispatchMessage( &msg );
  }
   
  KillTimer( hWnd, dta->timerID );

  EndDialog( hWnd, 0 );
}


void DoExecRule( EXECDATA* data ) {
  BSKI32 rc;
  HWND   primary;
  HWND   secondary;
  WINDATA* wdata;
  BSKUI32 seed;
  char   buffer[128];

  wdata = (WINDATA*)GetWindowLong( data->main, GWL_USERDATA );
  primary = wdata->pages[ wdata->current ];
  secondary = wdata->pages[ ( wdata->current + 1 ) % 2 ];

  seed = GetSeed( primary, secondary );
  BSKSRand( seed );

  sprintf( buffer, "~IRandom seed:  %lu~i~n", seed );
  addToConsole( data->main, buffer );

  rc = BSKExec( &(data->opts) );

  if( rc == RTE_HALTED ) {
    addToConsole( data->main, "~nScript execution was halted.~n" );
  } else {
    EndDialog( data->parent, 0 );
  }

  data->running = FALSE;
}


void ExecRule( HWND hWnd, EXECDATA* data ) {
  data->main = hWnd;
  
  DialogBoxParam( ghInst, MAKEINTRESOURCE( IDD_EXECWAIT ), hWnd, ExecWaitProc, (LPARAM)data );
}


void ClearConsole( HWND hWnd ) {
  HWND ctrl;
  WINDATA* data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  ctrl = GetDlgItem( data->console, IDC_TEXT );
  SetWindowText( ctrl, "" );
  data->runningTotal = 0;
}


void RefreshEditMenu( HWND hWnd, HMENU menu ) {
  WINDATA*      data;
  HWND          ctrl;
  DWORD         startSel;
  DWORD         endSel;
  DWORD         len;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  if( menu != data->editPopup ) {
    return;
  }

  ctrl = GetDlgItem( data->console, IDC_TEXT );
  len = GetWindowTextLength( ctrl );

  SendMessage( ctrl, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel );
  setMenuItem( menu, IDC_EDIT_CUT, ( endSel <= startSel ) );
  setMenuItem( menu, IDC_EDIT_COPY, ( endSel <= startSel ) );
  setMenuItem( menu, IDC_EDIT_CLEAR, ( endSel <= startSel ) );
  setMenuItem( menu, IDC_EDIT_SELECT_ALL, ( ( ( endSel >= len ) || ( endSel < 0 ) ) && ( startSel < 1 ) ) );
  setMenuItem( menu, IDC_EDIT_PASTE, !IsClipboardFormatAvailable( CF_TEXT ) );
}


void DoPopup( HWND hWnd, MSGFILTER* filter ) {
  WINDATA*      data;
  int           xPos;
  int           yPos;
  HWND          ctrl;
  RECT          rect;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  xPos = LOWORD( filter->lParam );
  yPos = HIWORD( filter->lParam );

  ctrl = GetDlgItem( data->console, IDC_TEXT );
  GetWindowRect( ctrl, &rect );

  xPos = xPos + rect.left;
  yPos = yPos + rect.top;

  TrackPopupMenu( data->editPopup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
                  xPos, yPos, 0, data->console, 0 );
}


void ProcessEdit( HWND hWnd, UINT cmd ) {
  HWND ctrl;
  HANDLE hMem;
  char* buffer;
  DWORD start;
  DWORD end;
  WINDATA* data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  ctrl = GetDlgItem( data->console, IDC_TEXT );

  if( !OpenClipboard( ctrl ) ) {
    return;
  }

  switch( cmd ) {
    case IDC_COPY_ALL:
      SendMessage( ctrl, EM_SETSEL, 0, -1 );
    case IDC_EDIT_CUT:
    case IDC_EDIT_COPY:
      SendMessage( ctrl, EM_GETSEL, (WPARAM)&start, (LPARAM)&end );
      hMem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, ( end - start + 2 ) );
      buffer = GlobalLock( hMem );
      SendMessage( ctrl, EM_GETSELTEXT, 0, (LPARAM)buffer );
      GlobalUnlock( hMem );
      SetClipboardData( CF_TEXT, hMem );
      if( cmd == IDC_EDIT_CUT ) {
        SendMessage( ctrl, EM_REPLACESEL, BSKTRUE, (LPARAM)"" );
      } else if( cmd == IDC_COPY_ALL ) {
        SendMessage( ctrl, EM_SETSEL, end, end );
      }
      break;
    case IDC_EDIT_CLEAR:
      SendMessage( ctrl, EM_REPLACESEL, BSKTRUE, (LPARAM)"" );
      break;
    case IDC_EDIT_PASTE:
      hMem = GetClipboardData( CF_TEXT );
      buffer = GlobalLock( hMem );
      SendMessage( ctrl, EM_REPLACESEL, BSKTRUE, (LPARAM)buffer );
      GlobalUnlock( hMem );
      break;
    case IDC_EDIT_SELECT_ALL:
      SendMessage( ctrl, EM_SETSEL, 0, -1 );
      break;
  }

  CloseClipboard();
}


void UseCurrentSeed( HWND hWnd ) {
  char buffer[ 15 ];

  GetWindowText( GetDlgItem( hWnd, IDC_CURRENT ), buffer, sizeof( buffer ) );
  SetWindowText( GetDlgItem( hWnd, IDC_SEED ), buffer );
}


LONG GetSeed( HWND primary, HWND secondary ) {
  char buffer[ 15 ];
  BSKUI32 seed;

  GetWindowText( GetDlgItem( primary, IDC_SEED ), buffer, sizeof( buffer ) );
  seed = atol( buffer );
  if( seed == 0 ) {
    struct _timeb tm;
    _ftime( &tm );
    seed = tm.time * 100 + tm.millitm;
  }
  sprintf( buffer, "%lu", seed );
  SetWindowText( GetDlgItem( primary, IDC_CURRENT ), buffer );

  SetWindowText( GetDlgItem( secondary, IDC_CURRENT ), "" );

  return seed;
}


void SetOrReplaceNumberU( BSKThing* thing, BSKUI32 id, BSKFLOAT number, BSKUI32 unit ) {
  BSKAttribute* attr;
  BSKValue value;

  BSKSetValueNumberU( &value, number, unit );
  attr = BSKGetAttributeOf( thing, id );
  if( attr != 0 ) {
    BSKInvalidateValue( &(attr->value) );
    BSKCopyValue( &(attr->value), &value );
  } else {
    BSKAddAttributeTo( thing, id, &value );
  }
  BSKInvalidateValue( &value );
}


void InitGeneratorOptions( WINDATA *data, BSKThing *options, BSKUI32* flags ) {
  BSKValue* parms[1];
  BSKValue  parm1;
  BSKValue  retVal;
  BSKExecOpts execData;
  BSKBOOL   halt;
  char      buffer[128];
  BSKUI32   id;
  BSKUI32   unit;
  BSKFLOAT  num;
  BSKFLOAT  min;
  BSKFLOAT  max;
  BSKBOOL   showFillStatus;
  BSKBOOL   showParms;
  BSKUI32   sourceCount;
  BSKUI32   i;
  HWND      ctrl;

  *flags = 0;

  showParms = isChecked( data->main, IDC_SHOWPARMS );

  execData.ruleId = BSKFindIdentifier( data->db->idTable, INIT_OPTIONS );
  if( execData.ruleId > 0 ) {
    parm1.type = VT_THING;
    parm1.datum = options;
    parms[0] = &parm1;

    halt = BSKFALSE;
    execData.console = treasureConsole;
    execData.db = data->db;
    execData.errorHandler = rteHandler;
    execData.parameterCount = 4;
    execData.parameters = parms;
    execData.rval = &retVal;
    execData.userData = GetParent( data->pages[0] );
    execData.halt = &halt;

    BSKExec( &execData );

    BSKInvalidateValue( &parm1 );
    BSKInvalidateValue( &retVal );
  }

  unit = BSKFindIdentifier( data->db->idTable, "gp" );

  showFillStatus = BSKFALSE;

  GetWindowText( GetDlgItem( data->pages[2], IDC_MINIMUM_VALUE ),
                 buffer, sizeof( buffer ) );
  min = atof( buffer );

  GetWindowText( GetDlgItem( data->pages[2], IDC_MAXIMUM_VALUE ),
                 buffer, sizeof( buffer ) );
  max = atof( buffer );

  if( min > 0 && max > 0 ) {
    if( min > max ) {
      MessageBoxf( data->main, "Notice", MB_OK, "The minimum treasure value is greater than the maximum treasure value.  The two have been swapped." );
      num = min;
      min = max;
      max = num;

      sprintf( buffer, "%.9g", min );
      SetWindowText( GetDlgItem( data->pages[2], IDC_MINIMUM_VALUE ), buffer );
      sprintf( buffer, "%.9g", max );
      SetWindowText( GetDlgItem( data->pages[2], IDC_MAXIMUM_VALUE ), buffer );
    }
  }

  id = BSKFindIdentifier( data->db->idTable, "optMinTreasureValue" );
  if( min > 0 ) {
    SetOrReplaceNumberU( options, id, min, unit );
    if( showParms ) {
      sprintf( buffer, "%.9g", min );
      addToConsole( data->main, "~IMinimum Treasure Value:  " );
      addToConsole( data->main, buffer );
      addToConsole( data->main, " gp~i~n" );
      showFillStatus = BSKTRUE;
    }
  }

  id = BSKFindIdentifier( data->db->idTable, "optMaxTreasureValue" );
  if( max > 0 ) {
    if( showParms ) {
      sprintf( buffer, "%.9g", max );
      SetOrReplaceNumberU( options, id, max, unit );
      addToConsole( data->main, "~IMaximum Treasure Value:  " );
      addToConsole( data->main, buffer );
      addToConsole( data->main, " gp~i~n" );
      showFillStatus = BSKTRUE;
    }
  }

  id = BSKFindIdentifier( data->db->idTable, "optFillDifferenceWithCoins" );
  if( isChecked( data->pages[2], IDC_PADCOINS ) ) {
    SetOrReplaceNumberU( options, id, 1, 0 );
    if( showFillStatus ) {
      addToConsole( data->main, "~IFill Difference With Coins:  yes~i~n" );
    }
  } else {
    SetOrReplaceNumberU( options, id, 0, 0 );
    if( showFillStatus ) {
      addToConsole( data->main, "~IFill Difference With Coins:  no~i~n" );
    }
  }

  id = BSKFindIdentifier( data->db->idTable, "optAlwaysIntelligent" );
  if( isChecked( data->pages[2], IDC_ALWAYSINTELLIGENT ) ) {
    SetOrReplaceNumberU( options, id, 1, 0 );
    if( showParms ) {
      addToConsole( data->main, "~IItems are Always Intelligent:  yes~i~n" );
    }
  } else {
    SetOrReplaceNumberU( options, id, 0, 0 );
  }

  id = BSKFindIdentifier( data->db->idTable, "optShowSourceName" );
  if( isChecked( data->pages[2], IDC_SHOWSOURCE ) ) {
    SetOrReplaceNumberU( options, id, 1, 0 );
    if( showParms ) {
      addToConsole( data->main, "~ISource Names Shown:  yes~i~n" );
    }
  } else {
    SetOrReplaceNumberU( options, id, 0, 0 );
  }

  ctrl = GetDlgItem( data->pages[2], IDC_SOURCES );
  sourceCount = SendMessage( ctrl, LB_GETSELCOUNT, 0, 0 );

  if( showParms ) {
    addToConsole( data->main, "~ISources:  " );
  }

  if( sourceCount < 1 ) {
    MessageBoxf( data->main, "Notice", MB_OK, "You did not select any sources.\n\n"
                 "By default, all sources will be valid.  To avoid receiving this "
                 "message in the future, please select one or more sources from the "
                 "options tab." );
    if( showParms ) {
      addToConsole( data->main, "  all~i~n" );
    }
  } else {
    BSKCategory* cat;
    BSKThing* thing;
    BSKAttribute* attr;
    BSKBOOL flag;

    id = BSKFindIdentifier( data->db->idTable, "optUseSources" );
    if( id > 0 ) {
      attr = BSKGetAttributeOf( options, id );
      if( attr == 0 ) {
        BSKValue  value;
    
        *flags |= fSOURCES_CATEGORY;

        cat = BSKNewCategory( 0 );

        value.type = VT_CATEGORY;
        value.datum = cat;

        BSKAddAttributeTo( options, id, &value );
        BSKInvalidateValue( &value );
      } else {
        cat = BSKValueGetCategory( BSKThingAttributeGetValue( attr ) );
      }

      id = BSKFindIdentifier( data->db->idTable, "name" );

      flag = BSKFALSE;
      sourceCount = SendMessage( ctrl, LB_GETCOUNT, 0, 0 );
      for( i = 0; i < sourceCount; i++ ) {
        if( SendMessage( ctrl, LB_GETSEL, i, 0 ) ) {
          thing = (BSKThing*)SendMessage( ctrl, LB_GETITEMDATA, i, 0 );
          BSKAddToCategory( cat, 1, thing );
          if( ( id > 0 ) && showParms ) {
            if( flag ) {
              addToConsole( data->main, ", " );
            }
            attr = BSKGetAttributeOf( thing, id );
            if( attr != 0 ) {
              addToConsole( data->main, BSKValueGetString( BSKThingAttributeGetValue( attr ) ) );
            } else {
              addToConsole( data->main, "(unnamed)" );
            }
            flag = BSKTRUE;
          }
        }
      }
      addToConsole( data->main, "~i~n" );
    }
  }

  if( data->current == 0 ) {
    if( isChecked( data->pages[0], IDC_NOGEMS ) ) {
      id = BSKFindIdentifier( data->db->idTable, "optExcludeGems" );
      SetOrReplaceNumberU( options, id, 1, 0 );
      if( showParms ) {
        addToConsole( data->main, "~IExcluding gems~i~n" );
      }
    } else if( isChecked( data->pages[0], IDC_NOART ) ) {
      id = BSKFindIdentifier( data->db->idTable, "optExcludeArt" );
      SetOrReplaceNumberU( options, id, 1, 0 );
      if( showParms ) {
        addToConsole( data->main, "~IExcluding art~i~n" );
      }
    }
  }
}


BOOL CALLBACK ConsoleProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  int w;
  int h;
  LPNMHDR hdr;
  MSGFILTER* filter;

  switch( msg )
  {
    case WM_INITDIALOG:
      InitConsole( hWnd, lParam );
      return FALSE;

    case WM_CLOSE:
    case WM_DESTROY:
      CloseConsole( hWnd );
      return TRUE;

    case WM_SIZE:
      w = LOWORD( lParam );
      h = HIWORD( lParam );
      switch( wParam ) {
        case SIZE_MAXIMIZED:
        case SIZE_RESTORED:
          MoveWindow( GetDlgItem( hWnd, IDC_TEXT ), 0, 0, w, h, TRUE );
          return TRUE;
      }
      break;

    case WM_COMMAND:
      switch( LOWORD(wParam) ) {
        case IDC_EDIT_CUT:
        case IDC_EDIT_COPY:
        case IDC_EDIT_PASTE:
        case IDC_EDIT_CLEAR:
        case IDC_EDIT_SELECT_ALL:
        case IDC_COPY_ALL:
          ProcessEdit( hWnd, LOWORD( wParam ) );
          return TRUE;
        case IDC_CLEAR:
          ClearConsole( hWnd );
          return TRUE;
      }
      break;

    case WM_INITMENUPOPUP:
      RefreshEditMenu( hWnd, (HMENU)wParam );
      return TRUE;

    case WM_NOTIFY:
      switch( LOWORD( wParam ) ) {
        case IDC_TEXT:
          hdr = (LPNMHDR)lParam;
          if( hdr->code == EN_MSGFILTER ) {
            filter = (MSGFILTER*)lParam;
            if( filter->msg == WM_RBUTTONUP ) {
              DoPopup( hWnd, filter );
              return TRUE;
            }
          }
          break;
      }
      break;
  }

	return FALSE;
}


void InitConsole( HWND hWnd, LPARAM lParam ) {
  WINDATA *data;
  HWND ctrl;

  SetWindowLong( hWnd, GWL_USERDATA, lParam );
  data = (WINDATA*)lParam;

  ctrl = GetDlgItem( hWnd, IDC_TEXT );
  SendMessage( ctrl, EM_SETEVENTMASK, 0, (LPARAM)( ENM_MOUSEEVENTS ) );

  data->console = hWnd;
}


void CloseConsole( HWND hWnd ) {
  WINDATA* data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  SendMessage( GetParent( hWnd ), WM_CONSOLE_CLOSING, 0, (LPARAM)hWnd );
  ShowWindow( data->console, SW_HIDE );
}


void OpenConsoleWindow( HWND hWnd ) {
  WINDATA* data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  
  if( data->console == 0 ) {
    RECT rect;
    RECT prect;

    data->console = CreateDialogParam( ghInst, MAKEINTRESOURCE( IDD_CONSOLE ),
                                       NULL, ConsoleProc, (LPARAM)data );
    GetWindowRect( data->console, &rect );
    GetWindowRect( hWnd, &prect );

    MoveWindow( data->console, prect.right, prect.top, 
                rect.right - rect.left, rect.bottom - rect.top, FALSE );
  }

  ShowWindow( data->console, SW_SHOW );
  UpdateWindow( data->console );
}


BOOL CALLBACK CompileParmsProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  switch( msg ) {
    case WM_INITDIALOG:
      InitCompileWindow( hWnd, lParam );
      return FALSE;

    case WM_DESTROY:
      CloseCompileWindow( hWnd, BSKTRUE );
      return TRUE;

    case WM_COMMAND:
      switch( LOWORD( wParam ) ) {
        case IDOK:
          CloseCompileWindow( hWnd, BSKFALSE );
          return TRUE;
        case IDCANCEL:
          CloseCompileWindow( hWnd, BSKTRUE );
          return TRUE;
        case IDC_BROWSE:
          BrowseForSourceFile( hWnd );
          return TRUE;
      }
      break;
  }

  return FALSE;
}


int InitCompileWindow( HWND hWnd, LPARAM parm ) {
  WINDATA* data;
  char buffer[512];
  char* p;

  data = (WINDATA*)parm;
  SetWindowLong( hWnd, GWL_USERDATA, parm );
  strcpy( buffer, data->indexFile );
  p = buffer + strlen(buffer);
  while( p > buffer && *p != '.' ) {
    p--;
  }

  if( *p == '.' ) {
    p++;
    strcpy( p, "bsk" );
  }

  SetWindowText( GetDlgItem( hWnd, IDC_SOURCE ), buffer );
  
  buffer[0] = 0;
  p = data->searchPaths;
  while( *p != '\0' ) {
    strcat( buffer, p );
    p += strlen( p ) + 1;
    if( *p != '\0' ) {
      strcat( buffer, "\r\n" );
    }
  }

  SetWindowText( GetDlgItem( hWnd, IDC_SEARCHPATHS ), buffer );

  return 0;
}


int CloseCompileWindow( HWND hWnd, BSKBOOL cancel ) {
  WINDATA *data;
  char* p;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  if( !cancel ) {
    GetWindowText( GetDlgItem( hWnd, IDC_SOURCE ), data->indexFile, sizeof( data->indexFile ) );
    GetWindowText( GetDlgItem( hWnd, IDC_SEARCHPATHS ), data->searchPaths, sizeof( data->searchPaths ) );
    while( BSKStringReplace( data->searchPaths, "\r\n", "|", 0 ) != 0 ) ;
    p = data->searchPaths;
    while( *p != 0 ) {
      if( *p == '|' ) {
        *p = 0;
      }
      p++;
    }
    p++;
    *p = 0;
  }

  EndDialog( hWnd, ( cancel ? IDCANCEL : IDOK ) );

  return 0;
}


void CompileDatabase( HWND hWnd ) {
  WINDATA* data;
  BSKI32   rc;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  rc = DialogBoxParam( ghInst, MAKEINTRESOURCE( IDD_COMPILE ), hWnd, CompileParmsProc, (LONG)data );

  if( rc == IDOK ) {
    LoadDatabase( hWnd, data->indexFile, TRUE );
  }
}


void BrowseForSourceFile( HWND hWnd ) {
  WINDATA *data;
  char     buffer[512];
  char*    p;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  GetWindowText( GetDlgItem( hWnd, IDC_SOURCE ), buffer, sizeof( buffer ) );

  data->openData.lpstrFile = buffer;
  p = (char*)data->openData.lpstrFilter;

  data->openData.lpstrFilter = "Basilisk Script Files (*.bsk)\0*.BSK\0All Files (*.*)\0*.*\0\0";

  if( !GetOpenFileName( &data->openData ) ) {
    DWORD d;

    d = CommDlgExtendedError();
    if( d != 0 ) {
      MessageBoxf( hWnd, "Error", MB_OK, "GetOpenFileName caused an error: %d", d );
    }
  } else {
    SetWindowText( GetDlgItem( hWnd, IDC_SOURCE ), buffer );
  }

  data->openData.lpstrFile = data->indexFile;
  data->openData.lpstrFilter = p;
}


void PostDBLoad( HWND hWnd ) {
  WINDATA *data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  PopulateSources( data->pages[2], data->db );
}


void PopulateSources( HWND hWnd, BSKDatabase* db ) {
  HWND ctrl;
  BSKCategory* category;
  BSKUI32 id;
  BSKUI32 nameId;
  BSKCategoryEntry* entry;

  ctrl = GetDlgItem( hWnd, IDC_SOURCES );
  SendMessage( ctrl, LB_RESETCONTENT, 0, 0 );

  id = BSKFindIdentifier( db->idTable, ALL_SOURCES );
  if( id < 1 ) {
    MessageBoxf( hWnd, "Error", MB_OK, "The category '%s' does not exist in the database.", ALL_SOURCES );
    return;
  }

  nameId = BSKFindIdentifier( db->idTable, "name" );
  if( nameId < 1 ) {
    MessageBoxf( hWnd, "Error", MB_OK, "The attribute 'name' does not exist in the database." );
    return;
  }

  category = BSKFindCategory( db, id );
  if( category == 0 ) {
    MessageBoxf( hWnd, "Error", MB_OK, "The category '%s' could not be found.", ALL_SOURCES );
    return;
  }

  for( entry = BSKCategoryGetFirstMember( category ); entry != 0; entry = entry->next ) {
    BSKCategoryMember* member;
    BSKThing* thing;
    BSKUI32   idx;
    BSKCHAR   name[128];
    BSKAttribute* attr;

    member = BSKCategoryEntryGetMember( entry );
    if( BSKCategoryMemberGetType( member ) != OT_THING ) {
      MessageBoxf( hWnd, "Error", MB_OK, "Sources must all be Things." );
      return;
    }

    thing = (BSKThing*)member;
    attr = BSKGetAttributeOf( thing, nameId );
    if( attr == 0 ) {
      MessageBoxf( hWnd, "Error", MB_OK, "Sources should have names, to be referencable." );
      continue;
    }

    strcpy( name, BSKValueGetString( BSKThingAttributeGetValue( attr ) ) );
    idx = SendMessage( ctrl, LB_ADDSTRING, 0, (LPARAM)name );
    SendMessage( ctrl, LB_SETITEMDATA, idx, (LPARAM)thing );
  }

  SendMessage( ctrl, LB_SETSEL, TRUE, -1 );
}


void CleanupOptions( BSKDatabase* db, BSKThing* options, BSKUI32 flags ) {
  BSKUI32 id;
  BSKCategory* category;
  BSKAttribute* attr;

  if( ( flags & fSOURCES_CATEGORY ) != 0 ) {
    id = BSKFindIdentifier( db->idTable, "optUseSources" );
    if( id > 0 ) {
      attr = BSKGetAttributeOf( options, id );
      if( attr != 0 ) {
        category = BSKValueGetCategory( BSKThingAttributeGetValue( attr ) );
        BSKInvalidateValue( BSKThingAttributeGetValue( attr ) );
        BSKDestroyCategory( category );
      }
    }
  }

  BSKDestroyThing( options );
}


void AddToTally( HWND hWnd, BSKValue* value ) {
  WINDATA* data;
  BSKUI32  goalUnit;
  BSKUI32  fromUnit;
  BSKFLOAT number;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  if( !BSKValueIsType( value, VT_NUMBER ) ) {
    return;
  }

  goalUnit = BSKFindIdentifier( data->db->idTable, "gp" );
  fromUnit = BSKValueUnits( value );

  number = BSKEvaluateNumber( value );
  BSKConvertUnits( data->db->unitDef, number, fromUnit, goalUnit, &number );

  data->runningTotal += number;

  if( isChecked( data->main, IDC_RUNNINGTOTAL ) ) {
    BSKCHAR buffer[128];
    Commify( data->runningTotal, buffer );
    addToConsole( data->main, "~BConsole total:~b  " );
    addToConsole( data->main, buffer );
    addToConsole( data->main, " gp~n" );
  }
}


void Commify( BSKFLOAT number, BSKCHAR* buffer ) {
  BSKCHAR  temp[128];
  BSKCHAR  temp2[128];
  BSKCHAR* p;

  *buffer = 0;

  sprintf( temp, "%.9g", number );
  p = strchr( temp, '.' );
  if( p != 0 ) {
    strcpy( buffer, p );
    *p = 0;
  } else {
    p = temp + strlen( temp );
  }

  p -= 3;
  while( p >= temp ) {
    sprintf( temp2, "%s%s%s", ( p > temp ? "," : "" ), p, buffer );
    strcpy( buffer, temp2 );
    *p = 0;
    p -= 3;
  }
  sprintf( temp2, "%s%s", temp, buffer );
  strcpy( buffer, temp2 );
}


void SaveConsole( HWND hWnd ) {
  WINDATA* data;
  EDITSTREAM stream;
  OPENFILENAME ofn;
  BSKCHAR fname[256];

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  memset( &ofn, 0, sizeof( ofn ) );

  fname[0] = 0;

  ofn.lStructSize = sizeof( ofn );
  ofn.hwndOwner = hWnd;
  ofn.lpstrFilter = "Rich Text (*.rtf)\0*.RTF\0Plain Text (*.txt)\0*.TXT\0\0";
  ofn.lpstrFile = fname;
  ofn.nMaxFile = sizeof( fname );
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  
  if( GetSaveFileName( &ofn ) ) {
    FILE* f;
    WPARAM mode;

    if( ofn.nFileExtension == 0 || fname[ ofn.nFileExtension ] == '\0' ) {
      if( fname[ strlen(fname)-1 ] != '.' ) {
        strcat( fname, "." );
      }
      if( ofn.nFilterIndex == 1 ) {
        strcat( fname, "rtf" );
      } else if( ofn.nFilterIndex == 2 ) {
        strcat( fname, "txt" );
      } else {
        strcat( fname, "rtf" );
      }
    }

    f = fopen( fname, "wt" );
    if( f == 0 ) {
      MessageBoxf( hWnd, "Error", MB_OK, "The file '%s' could not be created.", fname );
      return;
    }

    if( ofn.nFilterIndex == 1 ) {
      if( stricmp( &(fname[ strlen(fname)-4 ]), ".txt" ) == 0 ) {
        mode = SF_TEXT;
      } else {
        mode = SF_RTF;
      }
    } else {
      if( stricmp( &(fname[ strlen(fname)-4 ]), ".rtf" ) == 0 ) {
        mode = SF_RTF;
      } else {
        mode = SF_TEXT;
      }
    }

    stream.dwCookie = (DWORD)f;
    stream.pfnCallback = SaveConsoleCallback;
    SendMessage( GetDlgItem( data->console, IDC_TEXT ),
                 EM_STREAMOUT,
                 mode,
                 (LPARAM)&stream );

    fclose( f );
  }
}


DWORD CALLBACK SaveConsoleCallback( DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb ) {
  FILE* f;

  f = (FILE*)dwCookie;
  *pcb = fwrite( pbBuff, 1, cb, f );

  if( *pcb < cb ) {
    return -1;
  }

  return 0;
}


void LoadConsole( HWND hWnd ) {
  WINDATA* data;
  EDITSTREAM stream;
  OPENFILENAME ofn;
  BSKCHAR fname[256];

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );
  memset( &ofn, 0, sizeof( ofn ) );

  fname[0] = 0;

  ofn.lStructSize = sizeof( ofn );
  ofn.hwndOwner = hWnd;
  ofn.lpstrFilter = "Rich Text (*.rtf)\0*.RTF\0Plain Text (*.txt)\0*.TXT\0\0";
  ofn.lpstrFile = fname;
  ofn.nMaxFile = sizeof( fname );
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  
  if( GetOpenFileName( &ofn ) ) {
    FILE* f;
    WPARAM mode;

    if( ofn.nFileExtension == 0 || fname[ ofn.nFileExtension ] == '\0' ) {
      if( fname[ strlen(fname)-1 ] != '.' ) {
        strcat( fname, "." );
      }
      if( ofn.nFilterIndex == 1 ) {
        strcat( fname, "rtf" );
      } else if( ofn.nFilterIndex == 2 ) {
        strcat( fname, "txt" );
      } else {
        strcat( fname, "rtf" );
      }
    }

    f = fopen( fname, "rt" );
    if( f == 0 ) {
      MessageBoxf( hWnd, "Error", MB_OK, "The file '%s' could not be opened.", fname );
      return;
    }

    if( ofn.nFilterIndex == 1 ) {
      if( stricmp( &(fname[ strlen(fname)-4 ]), ".txt" ) == 0 ) {
        mode = SF_TEXT;
      } else {
        mode = SF_RTF;
      }
    } else {
      if( stricmp( &(fname[ strlen(fname)-4 ]), ".rtf" ) == 0 ) {
        mode = SF_RTF;
      } else {
        mode = SF_TEXT;
      }
    }

    stream.dwCookie = (DWORD)f;
    stream.pfnCallback = LoadConsoleCallback;
    SendMessage( GetDlgItem( data->console, IDC_TEXT ),
                 EM_STREAMIN,
                 mode,
                 (LPARAM)&stream );

    fclose( f );
  }
}


DWORD CALLBACK LoadConsoleCallback( DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb ) {
  FILE* f;

  f = (FILE*)dwCookie;
  *pcb = fread( pbBuff, 1, cb, f );

  if( *pcb < 0 ) {
    return -1;
  }

  return 0;
}


void PrintConsole( HWND hWnd ) {
  MessageBox( hWnd,
              "This version of the Treasure Generator does not directly support printing.  "
              "However, you can still print your treasures if you do the following:\n\n"
              "1) Generate the treasure (or treasures) you want, in the console.\n"
              "2) Save the console (File menu, 'Save Console') to a file.\n"
              "3) Open the file in a word processor, and print it.\n\n"
              "The author apologizes for the inconvenience.",
              "To Print...",
              MB_OK | MB_ICONINFORMATION );
/*
  PRINTDLG pdlg;

  memset( &pdlg, 0, sizeof( pdlg ) );
  pdlg.lStructSize = sizeof( pdlg );
  pdlg.hwndOwner = hWnd;
  pdlg.Flags = PD_ALLPAGES | PD_DISABLEPRINTTOFILE | PD_HIDEPRINTTOFILE |
               PD_NOPAGENUMS | PD_NOSELECTION | PD_RETURNDC;


  if( PrintDlg( &pdlg ) ) {
    FORMATRANGE fr;

    // sendmessage - EM_FORMATRANGE
    // sendmessage - EM_DISPLAYRANGE
  }
*/
}


void printStackTrace( BSKExecutionEnvironment* env, HWND hWnd ) {
  BSKCHAR name[100];
  BSKStackFrame* frame;
  BSKCHAR buffer[512];
  BSKRule* rule;

  for( frame = BSKExecEnvironmentGetCurrentFrame(env); frame != 0; frame = frame->next ) {
    BSKGetIdentifier( BSKExecEnvironmentGetDB( env )->idTable, 
                      BSKStackFrameGetRuleID( frame ), 
                      name, sizeof( name ) );
    rule = BSKFindRule( BSKExecEnvironmentGetDB( env )->rules, BSKStackFrameGetRuleID( frame ) );
    sprintf( buffer, " *** %s (%s:%ld)\r\n", 
      name,
      ( BSKRuleGetSourceFile( rule ) ? BSKRuleGetSourceFile( rule ) : "" ),
      BSKStackFrameGetLineNumber( frame ) );
    addToConsole( hWnd, buffer );
  }
}


void SaveAllOptions( HWND hWnd ) {
  BSKCHAR buffer[512];
  BSKCHAR* p;
  BSKUI32 i;
  WINDATA* data;
  HWND ctrl;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  WriteProfileInt( APP_NAME, "current_tab", data->current );
  WriteProfileString( APP_NAME, "index_file", data->indexFile );

  p = data->searchPaths;
  i = 0;
  while( *p != 0 ) {
    i++;
    sprintf( buffer, "path%02d", i );
    WriteProfileString( APP_NAME, buffer, p );
    p += strlen( p ) + 1;
  }
  WriteProfileInt( APP_NAME, "path_count", i );

  WriteProfileInt( APP_NAME, "auto_clear", isChecked( data->main, IDC_AUTOCLEAR ) );
  WriteProfileInt( APP_NAME, "show_parms", isChecked( data->main, IDC_SHOWPARMS ) );
  WriteProfileInt( APP_NAME, "running_total", isChecked( data->main, IDC_RUNNINGTOTAL ) );

  ctrl = GetDlgItem( data->pages[0], IDC_LEVEL );
  WriteProfileInt( APP_NAME, "level", SendMessage( ctrl, CB_GETCURSEL, 0, 0 ) );

  ctrl = GetDlgItem( data->pages[0], IDC_COINS );
  GetWindowText( ctrl, buffer, sizeof( buffer ) );
  WriteProfileString( APP_NAME, "coins", buffer );

  ctrl = GetDlgItem( data->pages[0], IDC_GOODS );
  GetWindowText( ctrl, buffer, sizeof( buffer ) );
  WriteProfileString( APP_NAME, "goods", buffer );

  ctrl = GetDlgItem( data->pages[0], IDC_ITEMS );
  GetWindowText( ctrl, buffer, sizeof( buffer ) );
  WriteProfileString( APP_NAME, "items", buffer );

  if( isChecked( data->pages[0], IDC_NORMAL ) ) {
    WriteProfileString( APP_NAME, "goods_restriction", "normal" );
  } else if( isChecked( data->pages[0], IDC_NOART ) ) {
    WriteProfileString( APP_NAME, "goods_restriction", "art" );
  } else if( isChecked( data->pages[0], IDC_NOGEMS ) ) {
    WriteProfileString( APP_NAME, "goods_restriction", "gems" );
  }

  WriteProfileInt( APP_NAME, "minor", isChecked( data->pages[1], IDC_MINOR ) );
  WriteProfileInt( APP_NAME, "medium", isChecked( data->pages[1], IDC_MEDIUM ) );
  WriteProfileInt( APP_NAME, "major", isChecked( data->pages[1], IDC_MAJOR ) );
  WriteProfileInt( APP_NAME, "armor", isChecked( data->pages[1], IDC_ARMOR ) );
  WriteProfileInt( APP_NAME, "potion", isChecked( data->pages[1], IDC_POTION ) );
  WriteProfileInt( APP_NAME, "ring", isChecked( data->pages[1], IDC_RING ) );
  WriteProfileInt( APP_NAME, "rod", isChecked( data->pages[1], IDC_ROD ) );
  WriteProfileInt( APP_NAME, "scroll", isChecked( data->pages[1], IDC_SCROLL ) );
  WriteProfileInt( APP_NAME, "staff", isChecked( data->pages[1], IDC_STAFF ) );
  WriteProfileInt( APP_NAME, "wand", isChecked( data->pages[1], IDC_WAND ) );
  WriteProfileInt( APP_NAME, "weapon", isChecked( data->pages[1], IDC_WEAPON ) );
  WriteProfileInt( APP_NAME, "wondrous", isChecked( data->pages[1], IDC_WONDROUS ) );

  ctrl = GetDlgItem( data->pages[1], IDC_COUNT );
  GetWindowText( ctrl, buffer, sizeof( buffer ) );
  WriteProfileString( APP_NAME, "count", buffer );
}


void RestoreAllOptions( HWND hWnd ) {
  BSKCHAR buffer[512];
  BSKCHAR* p;
  BSKUI32 count;
  BSKUI32 i;
  WINDATA* data;

  data = (WINDATA*)GetWindowLong( hWnd, GWL_USERDATA );

  data->current = GetProfileInt( APP_NAME, "current_tab", 0 );

  ShowWindow( data->pages[ data->current ], SW_SHOW );
  ShowWindow( data->pages[ ( data->current + 1 ) % 3 ], SW_HIDE );
  ShowWindow( data->pages[ ( data->current + 2 ) % 3 ], SW_HIDE );
  TabCtrl_SetCurSel( GetDlgItem( data->main, IDC_TAB ), data->current );

  GetProfileString( APP_NAME, "index_file", data->indexFile, buffer, sizeof( buffer ) );
  strcpy( data->indexFile, buffer );

  count = GetProfileInt( APP_NAME, "path_count", 0 );
  if( count > 0 ) {
    data->searchPaths[0] = 0;
    p = data->searchPaths;
    for( i = 1; i <= count; i++ ) {
      sprintf( buffer, "path%02d", i );
      GetProfileString( APP_NAME, buffer, "", buffer, sizeof( buffer ) );
      strcpy( p, buffer );
      p = p + strlen( p ) + 1;
      *p = 0;
    }
  }

  setCheck( data->main, IDC_AUTOCLEAR, (BSKBOOL)GetProfileInt( APP_NAME, "auto_clear", FALSE ) );
  setCheck( data->main, IDC_SHOWPARMS, (BSKBOOL)GetProfileInt( APP_NAME, "show_parms", FALSE ) );
  setCheck( data->main, IDC_RUNNINGTOTAL, (BSKBOOL)GetProfileInt( APP_NAME, "running_total", FALSE ) );

  SendMessage( GetDlgItem( data->pages[0], IDC_LEVEL ),
               CB_SETCURSEL,
               GetProfileInt( APP_NAME, "level", 0 ), 0 );

  GetProfileString( APP_NAME, "coins", "100", buffer, sizeof( buffer ) );
  SetWindowText( GetDlgItem( data->pages[0], IDC_COINS ), buffer );
  GetProfileString( APP_NAME, "goods", "100", buffer, sizeof( buffer ) );
  SetWindowText( GetDlgItem( data->pages[0], IDC_GOODS ), buffer );
  GetProfileString( APP_NAME, "items", "100", buffer, sizeof( buffer ) );
  SetWindowText( GetDlgItem( data->pages[0], IDC_ITEMS ), buffer );

  GetProfileString( APP_NAME, "goods_restriction", "normal", buffer, sizeof( buffer ) );
  if( strcmp( buffer, "normal" ) == 0 ) {
    setCheck( data->pages[0], IDC_NORMAL, BSKTRUE );
  } else if( strcmp( buffer, "art" ) == 0 ) {
    setCheck( data->pages[0], IDC_NOART, BSKTRUE );
  } else if( strcmp( buffer, "gems" ) == 0 ) {
    setCheck( data->pages[0], IDC_NOGEMS, BSKTRUE );
  }

  setCheck( data->pages[1], IDC_MINOR, (BSKBOOL)GetProfileInt( APP_NAME, "minor", TRUE ) );
  setCheck( data->pages[1], IDC_MEDIUM, (BSKBOOL)GetProfileInt( APP_NAME, "medium", FALSE ) );
  setCheck( data->pages[1], IDC_MAJOR, (BSKBOOL)GetProfileInt( APP_NAME, "major", FALSE ) );

  setCheck( data->pages[1], IDC_ARMOR, (BSKBOOL)GetProfileInt( APP_NAME, "armor", TRUE ) );
  setCheck( data->pages[1], IDC_POTION, (BSKBOOL)GetProfileInt( APP_NAME, "potion", FALSE ) );
  setCheck( data->pages[1], IDC_RING, (BSKBOOL)GetProfileInt( APP_NAME, "ring", FALSE ) );
  setCheck( data->pages[1], IDC_ROD, (BSKBOOL)GetProfileInt( APP_NAME, "rod", FALSE ) );
  setCheck( data->pages[1], IDC_SCROLL, (BSKBOOL)GetProfileInt( APP_NAME, "scroll", FALSE ) );
  setCheck( data->pages[1], IDC_STAFF, (BSKBOOL)GetProfileInt( APP_NAME, "staff", FALSE ) );
  setCheck( data->pages[1], IDC_WAND, (BSKBOOL)GetProfileInt( APP_NAME, "wand", FALSE ) );
  setCheck( data->pages[1], IDC_WEAPON, (BSKBOOL)GetProfileInt( APP_NAME, "weapon", FALSE ) );
  setCheck( data->pages[1], IDC_WONDROUS, (BSKBOOL)GetProfileInt( APP_NAME, "wondrous", FALSE ) );

  GetProfileString( APP_NAME, "count", "1", buffer, sizeof( buffer ) );
  SetWindowText( GetDlgItem( data->pages[1], IDC_COUNT ), buffer );
}


void WriteProfileInt( char* appname, char* section, int value ) {
  char buffer[32];

  sprintf( buffer, "%d", value );
  WriteProfileString( appname, section, buffer );
}
