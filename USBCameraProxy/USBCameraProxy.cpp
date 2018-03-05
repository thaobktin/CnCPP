#include "USBCameraProxy.h"

#include <algorithm>


EXPORT int __cdecl getDeviceCount()
{
	vector<Device> listOfDevices;

	videoInput::getInstance().getListOfDevices(listOfDevices);

	return listOfDevices.size();
}

EXPORT int __cdecl getDeviceInfo(int number, wchar_t *aPtrFriendlyName, wchar_t *aPtrSymbolicName)
{
	int lresult = -1;

	do
	{
		if (number < 0)
			break;

		vector<Device> listOfDevices;

		videoInput::getInstance().getListOfDevices(listOfDevices);

		if (listOfDevices.size() <= 0)
			break;

		wcscpy(aPtrFriendlyName, listOfDevices[number].friendlyName.c_str());

		wcscpy(aPtrSymbolicName, listOfDevices[number].symbolicName.c_str());
		
		lresult = listOfDevices[number].listStream[0].listMediaType.size();

	} while(false);

	return lresult;
}

EXPORT int __cdecl getDeviceResolution(int number, wchar_t *aPtrSymbolicName, PtrResolution aPtrResolution)
{
	int lresult = -1;

	do
	{
		if (number < 0)
			break;

		std::wstring lSymbolicName(aPtrSymbolicName);

		vector<Device> listOfDevices;

		videoInput::getInstance().getListOfDevices(listOfDevices);

		if (listOfDevices.size() <= 0)
			break;

		auto lfindIter = std::find_if(
			listOfDevices.begin(),
			listOfDevices.end(),
			[lSymbolicName](Device lDevice)
		{
			return lDevice.symbolicName.compare(lSymbolicName) == 0;
		});
		
		if (lfindIter != listOfDevices.end())
		{
			aPtrResolution->height = (*lfindIter).listStream[0].listMediaType[number].height;

			aPtrResolution->width = (*lfindIter).listStream[0].listMediaType[number].width;

			lresult = 0;
		}

	} while (false);

	return lresult;
}

EXPORT int __cdecl setupDevice(wchar_t *aPtrSymbolicName, int aResolutionIndex, int aCaptureVideoFormat, int ReadMode)
{
	int lresult = -1;
	
	do
	{
		if (aResolutionIndex < 0)
			break;

		DeviceSettings deviceSettings;

		deviceSettings.symbolicLink = std::wstring(aPtrSymbolicName);

		deviceSettings.indexStream = 0;

		deviceSettings.indexMediaType = aResolutionIndex;


		CaptureSettings captureSettings;

		captureSettings.pIStopCallback = nullptr;

		captureSettings.videoFormat = CaptureVideoFormat::VideoFormat(aCaptureVideoFormat);// ::RGB24;

		captureSettings.readMode = ReadMode::Read(ReadMode);

		lresult = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);


	} while (false);

	return lresult;
}

EXPORT int __cdecl closeAllDevices()
{
	return videoInput::getInstance().closeAllDevices();
}

EXPORT int __cdecl closeDevice(wchar_t *aPtrSymbolicName)
{

	DeviceSettings deviceSettings;

	deviceSettings.symbolicLink = std::wstring(aPtrSymbolicName);

	deviceSettings.indexStream = 0;

	deviceSettings.indexMediaType = 0;

	return videoInput::getInstance().closeDevice(deviceSettings);

}

EXPORT int __cdecl readPixels(wchar_t *aPtrSymbolicName, unsigned char *aPtrPixels)
{
	ReadSetting readSetting;

	readSetting.symbolicLink = std::wstring(aPtrSymbolicName);

	readSetting.pPixels = aPtrPixels;

	int lresult = videoInput::getInstance().readPixels(readSetting);

	return lresult;
}

EXPORT int __cdecl setCamParametrs(wchar_t *aPtrSymbolicName, CamParametrs *aPtrCamParametrs)
{
	CamParametrsSetting lCamParametrsSetting;

	lCamParametrsSetting.symbolicLink = std::wstring(aPtrSymbolicName);

	lCamParametrsSetting.settings = *aPtrCamParametrs;

	videoInput::getInstance().setParametrs(lCamParametrsSetting);

	return 0;
}

EXPORT int __cdecl getCamParametrs(wchar_t *aPtrSymbolicName, CamParametrs *aPtrCamParametrs)
{
	CamParametrsSetting lCamParametrsSetting;

	lCamParametrsSetting.symbolicLink = std::wstring(aPtrSymbolicName);

	videoInput::getInstance().getParametrs(lCamParametrsSetting);

	*aPtrCamParametrs = lCamParametrsSetting.settings;

	return 0;
}