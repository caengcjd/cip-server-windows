// server.cpp : ����Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <string>
#include <fstream>
#include <stdlib.h>
#include "server.h"
#include "WebsocketServer.h"
#include <boost/thread/thread.hpp>
#include "uwm.h"
#include "cip.h"
#include "cip_protocol.h"

using namespace std;

cip_context_t cip_context;
WebsocketServer wsServer;

typedef BOOL(CALLBACK *INSTALLHOOK)(HWND, DWORD);
typedef BOOL(CALLBACK *UNINSTALLHOOK)();

void WsServerThread()
{
	wsServer.run(9002);
}

#define MAX_LOADSTRING 100

// ȫ�ֱ���: 
HINSTANCE hInst;                                // ��ǰʵ��
WCHAR szTitle[MAX_LOADSTRING];                  // �������ı�
WCHAR szWindowClass[MAX_LOADSTRING];            // ����������
HWND hWnd;

// �˴���ģ���а����ĺ�����ǰ������: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


void CaptureScreen(HWND window, const char* filename)
{
	// get screen rectangle
	RECT windowRect;
	int ret = 0;
	ret = GetWindowRect(window, &windowRect);
	DWORD d = GetLastError();
	// bitmap dimensions
	int bitmap_dx = windowRect.right - windowRect.left;
	int bitmap_dy = windowRect.bottom - windowRect.top;

	// create file
	ofstream file(filename, ios::binary);
	if (!file) return;

	// save bitmap file headers
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;

	fileHeader.bfType = 0x4d42;
	fileHeader.bfSize = 0;
	fileHeader.bfReserved1 = 0;
	fileHeader.bfReserved2 = 0;
	fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	infoHeader.biSize = sizeof(infoHeader);
	infoHeader.biWidth = bitmap_dx;
	infoHeader.biHeight = bitmap_dy;
	infoHeader.biPlanes = 1;
	infoHeader.biBitCount = 24;
	infoHeader.biCompression = BI_RGB;
	infoHeader.biSizeImage = 0;
	infoHeader.biXPelsPerMeter = 0;
	infoHeader.biYPelsPerMeter = 0;
	infoHeader.biClrUsed = 0;
	infoHeader.biClrImportant = 0;

	file.write((char*)&fileHeader, sizeof(fileHeader));
	file.write((char*)&infoHeader, sizeof(infoHeader));

	// dibsection information
	BITMAPINFO info;
	info.bmiHeader = infoHeader;

	// ------------------
	// THE IMPORTANT CODE
	// ------------------
	// create a dibsection and blit the window contents to the bitmap
	HDC winDC = GetWindowDC(window);
	HDC memDC = CreateCompatibleDC(winDC);
	BYTE* memory = 0;
	HBITMAP bitmap = CreateDIBSection(winDC, &info, DIB_RGB_COLORS, (void**)&memory, 0, 0);
	SelectObject(memDC, bitmap);
	BitBlt(memDC, 0, 0, bitmap_dx, bitmap_dy, winDC, 0, 0, SRCCOPY);
	DeleteDC(memDC);
	ReleaseDC(window, winDC);

	// save dibsection data
	int bytes = (((24 * bitmap_dx + 31) & (~31)) / 8)*bitmap_dy;
	file.write((char*)memory, bytes);

	// HA HA, forgot paste in the DeleteObject lol, happy now ;)?
	DeleteObject(bitmap);
}

void pnt(HWND hwnd)
{
	CaptureScreen(hwnd, "C:\\d.bmp");
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: �ڴ˷��ô��롣

	boost::thread thrd(&WsServerThread);

    // ��ʼ��ȫ���ַ���
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SERVER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // ִ��Ӧ�ó����ʼ��: 
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SERVER));

	// ��ʼDLLע��
	HINSTANCE hDLL = LoadLibrary(TEXT("C:\\Users\\Administrator\\Documents\\Visual Studio 2015\\Projects\\cip-server-windows\\x64\\Debug\\hook.dll"));

	INSTALLHOOK InstallHook = (INSTALLHOOK)GetProcAddress(hDLL, "InstallHook");
	UNINSTALLHOOK UninstallHook = (UNINSTALLHOOK)GetProcAddress(hDLL, "UninstallHook");
	HWND notepadhandle = FindWindow(L"Notepad", NULL);
	if (notepadhandle == NULL) {
		printf("Notepad Not Found.\n");
		return TRUE;
	}
	DWORD pid = GetWindowThreadProcessId(notepadhandle, NULL);
	int ret = InstallHook(hWnd, pid);

	if (!ret)
	{
		DWORD dw = GetLastError();
		printf("err %u\n", dw);
	}

    MSG msg;

    // ����Ϣѭ��: 
    while (GetMessage(&msg, nullptr, 0, 0))
    {

		switch (msg.message)
		{
		case UWM_DEBUG: {
			std::stringstream ss;
			ss << msg.wParam;
			wsServer.broadcast(ss.str(), 1);
			break;
		}
		case UWM_CREATE_WINDOW: {
			RECT rc;
			GetClientRect((HWND)msg.wParam, &rc);
			cip_event_window_create_t *cewc = (cip_event_window_create_t*)malloc(sizeof(cip_event_window_create_t));
			cewc->type = CIP_EVENT_WINDOW_CREATE;
			cewc->wid = (u32)msg.hwnd;
			cewc->x = (u32)rc.left;
			cewc->y = (u32)rc.top;
			cewc->width = (u32)rc.right - (u32)rc.left;
			cewc->height = (u32)rc.bottom - (u32)rc.top;
			cewc->bare = 0;
			wsServer.broadcast(cewc, sizeof(cip_event_window_create_t), 2);
		}
		case UWM_SHOW_WINDOW: {

			cip_event_window_show_t *cews = (cip_event_window_show_t*)malloc(sizeof(cip_event_window_show_t));
			cews->type = CIP_EVENT_WINDOW_SHOW;

			cews->wid = (u32)msg.hwnd;
			cews->bare = 0;
			wsServer.broadcast(cews, sizeof(cip_event_window_show_t), 2);
			break;
		}
		case UWM_DESTROY_WINDOW: {
			cip_event_window_destroy_t *cewd = (cip_event_window_destroy_t*)malloc(sizeof(cip_event_window_destroy_t));
			cewd->type = CIP_EVENT_WINDOW_DESTROY;
			cewd->wid = (u32)msg.hwnd;
			wsServer.broadcast(cewd, sizeof(cip_event_window_destroy_t), 2);
			break;
		}
		case UWM_PAINT_WINDOW: {

			std::stringstream ss;
			ss << msg.wParam;
			wsServer.broadcast(ss.str(), 1);
			//prtMenu((HWND)msg.wParam, (HMENU)msg.wParam, "C:\\d.png");
			Sleep(50);
			HWND w = FindWindow(L"#32768", NULL);
			pnt(w);
			break;
		}
		default:
			break;
		}

        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  ����: MyRegisterClass()
//
//  Ŀ��: ע�ᴰ���ࡣ
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SERVER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SERVER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ����: InitInstance(HINSTANCE, int)
//
//   Ŀ��: ����ʵ�����������������
//
//   ע��: 
//
//        �ڴ˺����У�������ȫ�ֱ����б���ʵ�������
//        ��������ʾ�����򴰿ڡ�
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 500, 100, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  ����: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  Ŀ��:    ���������ڵ���Ϣ��
//
//  WM_COMMAND  - ����Ӧ�ó���˵�
//  WM_PAINT    - ����������
//  WM_DESTROY  - �����˳���Ϣ������
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // �����˵�ѡ��: 
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: �ڴ˴����ʹ�� hdc ���κλ�ͼ����...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// �����ڡ������Ϣ�������
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
