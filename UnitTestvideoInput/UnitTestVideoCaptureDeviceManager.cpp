#include "stdafx.h"
#include "CppUnitTest.h"


#include "../videoInput/VideoCaptureDeviceManager.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestvideoInput
{
	TEST_CLASS(UnitTestVideoCaptureDeviceManager)
	{
	public:
		
		TEST_METHOD(getListOfDevices)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = VideoCaptureDeviceManager::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: getListOfDevices() cannot be executed!!!");
		}
		
		TEST_METHOD(initAndAddDevice)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = VideoCaptureDeviceManager::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: getListOfDevices() cannot be executed!!!");

			DeviceSettings deviceSettings;

			deviceSettings.symbolicLink = listOfDevices[0].symbolicName;

			deviceSettings.indexStream = 0;

			deviceSettings.indexMediaType = 0;


			

			CaptureSettings captureSettings;
			
			captureSettings.pIStopCallback = 0;

			captureSettings.readMode = ReadMode::SYNC;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;



			result = VideoCaptureDeviceManager::getInstance().initAndAddDevice(deviceSettings, captureSettings);

			result = VideoCaptureDeviceManager::getInstance().closeAllDevices();

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: initAndAddDevice() cannot be executed!!!");
		}
		
		TEST_METHOD(startDevice_stopDevice)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = VideoCaptureDeviceManager::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: getListOfDevices() cannot be executed!!!");

			DeviceSettings deviceSettings;

			deviceSettings.symbolicLink = listOfDevices[0].symbolicName;

			deviceSettings.indexStream = 0;

			deviceSettings.indexMediaType = 0;


			

			CaptureSettings captureSettings;
			
			captureSettings.pIStopCallback = 0;

			captureSettings.readMode = ReadMode::SYNC;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;



			result = VideoCaptureDeviceManager::getInstance().initAndAddDevice(deviceSettings, captureSettings);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: initAndAddDevice() cannot be executed!!!");
		
			result = VideoCaptureDeviceManager::getInstance().startDevice(deviceSettings.symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: startDevice() cannot be executed!!!");
		
		
			result = VideoCaptureDeviceManager::getInstance().closeDevice(deviceSettings.symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOCAPTUREDEVICEMANAGER: stopDevice() cannot be executed!!!");
		
		
		}

	};
}