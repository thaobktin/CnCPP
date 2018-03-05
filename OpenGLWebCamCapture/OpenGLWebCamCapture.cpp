// OpenGLWebCamCapture.cpp: определяет точку входа для приложения.
//



#define WIN32_LEAN_AND_MEAN    

#include <windows.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <gl/gl.h>
#include <memory>
#include <vector>
#include "../videoInput/videoInput.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "../videoInput/Debug/videoInput.lib")


#define GL_BGR                            0x80E0

/**************************
* Function Declarations
*
**************************/

LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam);
void EnableOpenGL(HWND hWnd, HDC *hDC, HGLRC *hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);


	/* initialisation videoinput */

	using namespace std;

	vector<Device> listOfDevices;

	ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

	if (listOfDevices.size() == 0)
		return -1;

	DeviceSettings deviceSettings;

	deviceSettings.symbolicLink = listOfDevices[0].symbolicName;

	deviceSettings.indexStream = 0;

	deviceSettings.indexMediaType = 0;

	CaptureSettings captureSettings;

	captureSettings.pIStopCallback = 0;

	captureSettings.readMode = ReadMode::SYNC;

	captureSettings.videoFormat = CaptureVideoFormat::RGB24;


	MediaType MT = listOfDevices[0].listStream[0].listMediaType[0];

	unique_ptr<unsigned char> frame(new unsigned char[3 * MT.width * MT.height]);



	ReadSetting readSetting;

	readSetting.symbolicLink = deviceSettings.symbolicLink;

	readSetting.pPixels = frame.get();

	result = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);

	ResultCode::Result readState = videoInput::getInstance().readPixels(readSetting);

	/* check accesse to the video device */
	if (readState != ResultCode::READINGPIXELS_DONE)
		return -1;


	float halfQuadWidth = 0.75;
	float halfQuadHeight = 0.75;


	WNDCLASS wc;
	HWND hWnd;
	HDC hDC;
	HGLRC hRC;
	MSG msg;
	BOOL bQuit = FALSE;
	float theta = 0.0f;

	/* register window class */
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"OpenGLWebCamCapture";
	RegisterClass(&wc);

	/* create main window */
	hWnd = CreateWindow(
		L"OpenGLWebCamCapture", L"OpenGLWebCamCapture Sample",
		WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
		0, 0, MT.width, MT.height,
		NULL, NULL, hInstance, NULL);

	/* enable OpenGL for the window */
	EnableOpenGL(hWnd, &hDC, &hRC);

	GLuint textureID;
	glGenTextures(1, &textureID);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Give the image to OpenGL
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MT.width, MT.height, 0, GL_BGR, GL_UNSIGNED_BYTE, frame.get());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	/* program main loop */
	while (!bQuit)
	{
		/* check for messages */
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			/* handle or dispatch messages */
			if (msg.message == WM_QUIT)
			{
				bQuit = TRUE;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			/* OpenGL animation code goes here */

			ResultCode::Result readState = videoInput::getInstance().readPixels(readSetting);

			if (readState != ResultCode::READINGPIXELS_DONE)
				break;

			glClear(GL_COLOR_BUFFER_BIT);
			glLoadIdentity();

			glBindTexture(GL_TEXTURE_2D, textureID);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MT.width, MT.height, GL_BGR, GL_UNSIGNED_BYTE, frame.get());

			glBegin(GL_QUADS);      // Begining of drawing of Square.

			glTexCoord2f(0, 1);  glVertex2f(-halfQuadWidth, -halfQuadHeight);	// Buttom Left
			glTexCoord2f(1, 1);  glVertex2f(halfQuadWidth, -halfQuadHeight);	// Buttom right
			glTexCoord2f(1, 0);  glVertex2f(halfQuadWidth, halfQuadHeight);		// Top right
			glTexCoord2f(0, 0);  glVertex2f(-halfQuadWidth, halfQuadHeight);	// Top Left
			glEnd();


			SwapBuffers(hDC);

			Sleep(1);
		}
	}

	/* shutdown OpenGL */
	DisableOpenGL(hWnd, hDC, hRC);

	/* destroy the window explicitly */
	DestroyWindow(hWnd);

	/* release captured webcam */
	videoInput::getInstance().closeDevice(deviceSettings);

	return msg.wParam;
}


/********************
* Window Procedure
*
********************/

LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_CREATE:
		return 0;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	case WM_DESTROY:
		return 0;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
		}
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}


/*******************
* Enable OpenGL
*
*******************/

void EnableOpenGL(HWND hWnd, HDC *hDC, HGLRC *hRC)
{
	PIXELFORMATDESCRIPTOR pfd;
	int iFormat;

	/* get the device context (DC) */
	*hDC = GetDC(hWnd);

	/* set the pixel format for the DC */
	ZeroMemory(&pfd, sizeof (pfd));
	pfd.nSize = sizeof (pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	iFormat = ChoosePixelFormat(*hDC, &pfd);
	SetPixelFormat(*hDC, iFormat, &pfd);

	/* create and enable the render context (RC) */
	*hRC = wglCreateContext(*hDC);
	wglMakeCurrent(*hDC, *hRC);

	glEnable(GL_TEXTURE_2D);			// Enable of texturing for OpenGL

}

/******************
* Disable OpenGL
*
******************/

void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(hWnd, hDC);
}

