#include "stdafx.h"
#include "CppUnitTest.h"


#include "../videoInput/videoInput.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestvideoInput
{		
	TEST_CLASS(UnitTestvideoInput)
	{
	public:
		
		TEST_METHOD(getListOfDevices)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");
		}
		
		TEST_METHOD(setupDeviceAndStopDevice)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");

			Assert::IsFalse(listOfDevices.empty(), L"listOfDevices is empty!!!");

			DeviceSettings deviceSettings;

			deviceSettings.symbolicLink = listOfDevices[0].symbolicName;

			deviceSettings.indexStream = 0;

			deviceSettings.indexMediaType = 1;


			

			CaptureSettings captureSettings;

			captureSettings.readMode = ReadMode::SYNC;

			captureSettings.pIStopCallback = 0;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;



			result = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: setupDevice() cannot be executed!!!");

			Sleep(500);

			result = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);

			Assert::IsTrue(result == ResultCode::VIDEOCAPTUREDEVICEMANAGER_DEVICEISSETUPED, L"VIDEOINPUT: second execution setupDevice() cannot be executed!!!");

			result = videoInput::getInstance().closeDevice(deviceSettings);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: stopDevice() cannot be executed!!!");
		}
				
		TEST_METHOD(setupDeviceAndreadPixelsAndStopDevice)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");

			Assert::IsFalse(listOfDevices.empty(), L"listOfDevices is empty!!!");

			DeviceSettings deviceSettings;

			deviceSettings.symbolicLink = listOfDevices[0].symbolicName;

			deviceSettings.indexStream = 0;

			deviceSettings.indexMediaType = 0;


			

			CaptureSettings captureSettings;

			captureSettings.pIStopCallback = 0;

			captureSettings.readMode = ReadMode::ASYNC;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;


			MediaType MT = listOfDevices[0].listStream[0].listMediaType[0];

			std::auto_ptr<unsigned char> pixels;

			pixels.reset(new unsigned char[MT.height * MT.width * 3]);

			ReadSetting readSetting;

			readSetting.symbolicLink = deviceSettings.symbolicLink;

			readSetting.pPixels = pixels.get();



			result = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: setupDevice() cannot be executed!!!");
			
			ResultCode::Result readState = videoInput::getInstance().readPixels(readSetting);

			Assert::IsTrue(readState == ResultCode::READINGPIXELS_DONE, L"VIDEOINPUT: readPixels() cannot be executed!!!");

			result = videoInput::getInstance().closeDevice(deviceSettings);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: stopDevice() cannot be executed!!!");
		}
		
		
		TEST_METHOD(setupDeviceAndStopAllDevices)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");

			Assert::IsFalse(listOfDevices.empty(), L"listOfDevices is empty!!!");

			DeviceSettings deviceSettings;

			deviceSettings.symbolicLink = listOfDevices[0].symbolicName;

			deviceSettings.indexStream = 0;

			deviceSettings.indexMediaType = 0;


			

			CaptureSettings captureSettings;

			captureSettings.readMode = ReadMode::ASYNC;

			captureSettings.pIStopCallback = 0;
			
			captureSettings.videoFormat = CaptureVideoFormat::RGB24;



			result = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: setupDevice() cannot be executed!!!");

			result = videoInput::getInstance().setupDevice(deviceSettings, captureSettings);

			Assert::IsTrue(result == ResultCode::VIDEOCAPTUREDEVICEMANAGER_DEVICEISSETUPED, L"VIDEOINPUT: second execution setupDevice() cannot be executed!!!");

			result = videoInput::getInstance().closeAllDevices();

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: stopAllDevices() cannot be executed!!!");
		}

	};
}