#include "windows.h"

HMODULE g_hIME = NULL;

typedef struct{
	int pre;
	int suf;
}BACK;

typedef struct{
	HWND	parent_hwnd;
	HWND	sub_hwnd;

	WNDPROC p_origin_editproc;

	HHOOK	hk_kboard;

	DWORD	thread_id;

	BOOL	forbid_fsr;

	BACK	back;			/* save back and delete key clicked nums. */
	//int		spe_key;		/* save three special key value, return¡¢tab and esc. */

	unsigned int ref_count;	/* save the reference counts of the rmtp struct, 
							   the rmtp is null only the ref_count is 0. */
}RMTypeParameter;

/* a global variables. */
RMTypeParameter *rmtp = NULL;

/**************************************************************************************
 *	BOOL WINAPI DllMain(HINSTANCE,DWORD fdwReason,LPVOID)
 *
 *	The dll main function, it's the entry of the dll.
 *
 *	param:
 *		fdwReason - state of the dll
 *
 *	return:
 *		TRUE
 *************************************************************************************/
BOOL WINAPI DllMain(HINSTANCE hInst,DWORD fdwReason,LPVOID)
{
	g_hIME = (HMODULE)hInst;

	switch(fdwReason)
	{
		case DLL_PROCESS_DETACH:
			if(rmtp)
			{
				if (rmtp->hk_kboard) UnhookWindowsHookEx(rmtp->hk_kboard);
				free(rmtp);
				rmtp = NULL;
			}
			break;
	}
	return TRUE;
}


LRESULT CALLBACK _kboard_proc(int code,WPARAM wParam,LPARAM lParam)
{
	if (code < 0)
		return CallNextHookEx(rmtp->hk_kboard,code,wParam,lParam);

	if(rmtp->forbid_fsr && VK_RETURN==wParam && lParam>>29)
		return 1;

	return CallNextHookEx(rmtp->hk_kboard,code,wParam,lParam);
}

LRESULT CALLBACK StaticImeEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CHAR:
		if (wParam == VK_RETURN || wParam == VK_TAB || wParam == VK_ESCAPE)
			return FALSE;
		break;
	case WM_KEYDOWN:
		{
			if(VK_BACK==wParam) 
			{
				rmtp->back.pre++;
				return FALSE;
			}
			if(VK_DELETE==wParam)
			{
				 rmtp->back.suf++;
				 return FALSE;
			}
		}
		break;
	default:
		break;
	}
	return CallWindowProc(rmtp->p_origin_editproc, hwnd, uMsg, wParam, lParam);
}

/* window's bounds is 480,432,608,32 */

/**************************************************************************************
 *	void _init(HWND hwnd,unsigned int x,unsigned int y)
 *
 *	Init the IME object, it create a new RMTypeParameter object if rmtp is null else it
 *	simple to add the reference count of the rmtp var.
 *
 *	param:
 *		hwnd - the handle of the parent window
 *		x	 - the ime window's x coordinate
 *		y    - the ime window's y coordinate
 *************************************************************************************/
extern "C" _declspec(dllexport) void _init(HWND hwnd,unsigned int x,unsigned int y)
{
	if(rmtp)
	{
		rmtp->ref_count++;
		MoveWindow(rmtp->sub_hwnd,x,y,608,32,0);
	}
	else
	{
		rmtp = (RMTypeParameter*)malloc(sizeof(RMTypeParameter));

		rmtp->thread_id			= GetCurrentThreadId();

		rmtp->hk_kboard			= 0;//SetWindowsHookEx(WH_KEYBOARD,_kboard_proc,g_hIME,rmtp->thread_id);

		rmtp->parent_hwnd		= hwnd;
		rmtp->sub_hwnd			= CreateWindow("edit",NULL,WS_CHILD,x,y,608,32,hwnd,NULL,NULL,NULL);

		rmtp->p_origin_editproc	= (WNDPROC)SetWindowLong(rmtp->sub_hwnd, GWL_WNDPROC, (LONG)StaticImeEditProc);

		rmtp->ref_count			= 1;
		rmtp->back.pre			= rmtp->back.suf = 0;

		rmtp->forbid_fsr		= TRUE;

		SetFocus(rmtp->sub_hwnd);
	}
}

/**************************************************************************************
 *	void _update(unsigned int x,unsigned int y)
 *
 *	Update the ime window's focus and position.
 *
 *	param:
 *		x - new x coordinate
 *		y - new y coordinate
 *************************************************************************************/
extern "C" _declspec(dllexport) void _update(unsigned int x,unsigned int y)
{
	if(rmtp)
	{
		SetFocus(rmtp->sub_hwnd);
		MoveWindow(rmtp->sub_hwnd,x,y,608,32,0);
	}
}

extern "C" _declspec(dllexport) void _clear_key()
{
	if(rmtp)
	{
		//rmtp->spe_key = 0;
		rmtp->back.pre = rmtp->back.suf = 0;
	}
}

extern "C" _declspec(dllexport) void _set_active(int v)
{
	if (rmtp)
	{
		SetFocus(v != 0 ? rmtp->sub_hwnd : rmtp->parent_hwnd);
		EnableWindow(rmtp->sub_hwnd, v != 0);
		SetWindowText(rmtp->sub_hwnd,"");
	}
}

/**************************************************************************************
 *	void _dispose()
 *
 *	Dispose a ime object, if the count of the rmtp's reference is 0 then it Unhook and
 *	it simple to subtract the value.
 *************************************************************************************/
extern "C" _declspec(dllexport) void _dispose()
{
	if(rmtp)
	{
		if(--rmtp->ref_count == 0)
		{
			if (rmtp->p_origin_editproc)
			{
				SetWindowLong(rmtp->sub_hwnd, GWL_WNDPROC, (LONG)rmtp->p_origin_editproc);
				rmtp->p_origin_editproc = 0;
			}
			if (rmtp->sub_hwnd)
			{
				DestroyWindow(rmtp->sub_hwnd);
				rmtp->sub_hwnd = 0;
			}
			if (rmtp->hk_kboard) UnhookWindowsHookEx(rmtp->hk_kboard);
			free(rmtp);
			rmtp = NULL;
		}
	}
}

/**************************************************************************************
 *	void _get_text(char* buf)
 *
 *	Get the text of the ime window's and fill it to the buf, the buf's size is fixed.
 *
 *	param:
 *		buf - recv the text's buf
 *************************************************************************************/
extern "C" _declspec(dllexport) void _get_text(char* buf) /* buf size is 1024 bytes */
{
	if(rmtp)
	{
		GetWindowText(rmtp->sub_hwnd,buf,1024);
		SetWindowText(rmtp->sub_hwnd,"");
		if(strlen(buf) != 0)
		{
			int len = MultiByteToWideChar(0,0,buf,-1,0,0);
			LPWSTR buf2 = (LPWSTR)malloc(len*2);
			MultiByteToWideChar(0,0,buf,-1,buf2,len);
			len = WideCharToMultiByte(65001,0,buf2,-1,0,0,0,0);
			WideCharToMultiByte(65001,0,buf2,-1,buf,1024,0,0);
			free(buf2);
		}
	}
}

/**************************************************************************************
 *	int _spe_key_info()
 *
 *	Get some special key's info, such as return¡¢tab and esc key.
 *
 *	return:
 *		return the special key's vk code.
 *************************************************************************************/
//extern "C" _declspec(dllexport) int _spe_key_info()
//{
//	if(rmtp)
//	{
//		int tmp = rmtp->spe_key;
//		rmtp->spe_key = 0;
//		return tmp;
//	}
//	else
//		return -1;
//}

/**************************************************************************************
 *	void _back(BACK* bk)
 *
 *	Get the BACK and DELETE key's clicked counts.
 *
 *	param:
 *		bk - recv the nums buf
 *************************************************************************************/
extern "C" _declspec(dllexport) void _back(BACK* bk)
{
	if(rmtp)
	{
		bk->pre = rmtp->back.pre;
		bk->suf = rmtp->back.suf;
		rmtp->back.pre = rmtp->back.suf = 0;
	}
}

