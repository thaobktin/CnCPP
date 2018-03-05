#include <Windows.h>
#include <stdio.h>
#include "videoInput.h"

#define EXPORT extern "C" __declspec(dllexport)


typedef struct _Resolution
{
	int width;
	int height;
} Resolution, *PtrResolution;




EXPORT int __cdecl getDeviceCount();

EXPORT int __cdecl getDeviceInfo(int number, wchar_t *aPtrFriendlyName, wchar_t *aPtrSymbolicName);

EXPORT int __cdecl getDeviceResolution(int number, wchar_t *aPtrSymbolicName, PtrResolution aPtrResolution);

EXPORT int __cdecl setupDevice(wchar_t *aPtrSymbolicName, int aResolutionIndex, int aCaptureVideoFormat, int ReadMode);

EXPORT int __cdecl closeAllDevices();

EXPORT int __cdecl closeDevice(wchar_t *aPtrSymbolicName);

EXPORT int __cdecl readPixels(wchar_t *aPtrSymbolicName, unsigned char *aPtrPixels);

EXPORT int __cdecl setCamParametrs(wchar_t *aPtrSymbolicName, CamParametrs *aPtrCamParametrs);

EXPORT int __cdecl getCamParametrs(wchar_t *aPtrSymbolicName, CamParametrs *aPtrCamParametrs);