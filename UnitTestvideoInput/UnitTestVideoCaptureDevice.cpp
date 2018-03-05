#include "stdafx.h"
#include "CppUnitTest.h"


#include <mfobjects.h>
#include <mfidl.h>


#include "../videoInput/videoInput.h"
#include "../videoInput/MediaFoundation.h"
#include "../videoInput/VideoCaptureDevice.h"
#include "../videoInput/FormatReader.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestvideoInput
{
	TEST_CLASS(UnitTestVideoCaptureDevice)
	{
	public:
		
		TEST_METHOD(init)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");
			
			Assert::IsTrue(!listOfDevices.empty(), L"listOfDevices is empty!!!");
			
			CComPtr<IMFMediaSource> pSource;
			
			CComPtr<IMFPresentationDescriptor> pPD;
	
			CComPtr<IMFStreamDescriptor> pSD;

			CComPtr<IMFMediaType> pMediaType;

			BOOL isSelected;

			result = MediaFoundation::getInstance().getSorceBySymbolicLink(listOfDevices[0].symbolicName, &pSource);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");
			
			
			result = MediaFoundation::getInstance().createPresentationDescriptor(pSource, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex() cannot be executed!!!");
	
			result = MediaFoundation::getInstance().getCurrentMediaType(pSD, &pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getCurrentMediaType() cannot be executed!!!");

			MediaType MT = FormatReader::getInstance().Read(pMediaType);


			DeviceSettings deviceSettings;

			deviceSettings.indexStream = 0;


			CaptureSettings captureSettings;
			
			captureSettings.pIStopCallback = 0;

			captureSettings.readMode = ReadMode::SYNC;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;


			VideoCaptureDevice *pDevice = new VideoCaptureDevice;

			result = pDevice->init(pSource, deviceSettings, captureSettings, MT);

			delete pDevice;

			Assert::IsTrue(result == ResultCode::OK, L"VIDEO CAPTURE DEVICE: init() cannot be executed!!!");

			pSource->Shutdown();
		}
				
		TEST_METHOD(createTopology)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");
			
			Assert::IsTrue(!listOfDevices.empty(), L"listOfDevices is empty!!!");
			
			CComPtr<IMFMediaSource> pSource;
			
			CComPtr<IMFPresentationDescriptor> pPD;
	
			CComPtr<IMFStreamDescriptor> pSD;

			CComPtr<IMFMediaType> pMediaType;

			BOOL isSelected;

			result = MediaFoundation::getInstance().getSorceBySymbolicLink(listOfDevices[0].symbolicName, &pSource);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");
			
			
			result = MediaFoundation::getInstance().createPresentationDescriptor(pSource, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex() cannot be executed!!!");
	
			result = MediaFoundation::getInstance().getCurrentMediaType(pSD, &pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getCurrentMediaType() cannot be executed!!!");

			MediaType MT = FormatReader::getInstance().Read(pMediaType);



			CaptureSettings captureSettings;

			captureSettings.pIStopCallback = 0;

			captureSettings.readMode = ReadMode::SYNC;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;


			CComPtr<IMFMediaType> pSinkMediaType;

			CComPtr<IMFActivate> pSinkActivate;

			CComPtr<VideoCaptureSink> pSink = new VideoCaptureSink(0);
			
			result = MediaFoundation::getInstance().createMediaType(&pSinkMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().createSampleGrabberSinkActivate(pSinkMediaType, pSink, &pSinkActivate);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSampleGrabberSinkActivate() cannot be executed!!!");

			
			CComPtr<IMFTopology> pTopology;
			
			result = MediaFoundation::getInstance().createTopology(&pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopology() cannot be executed!!!");



			CComPtr<VideoCaptureDevice> pDevice = new VideoCaptureDevice;

			result = pDevice->createTopology(pSource, 0, pSinkActivate, pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEO CAPTURE DEVICE: createTopology() cannot be executed!!!");

			pSource->Shutdown();


		}	

		TEST_METHOD(addSourceNode)
		{
			vector<Device> listOfDevices;

			ResultCode::Result result = videoInput::getInstance().getListOfDevices(listOfDevices);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEOINPUT: getListOfDevices() cannot be executed!!!");
			
			Assert::IsTrue(!listOfDevices.empty(), L"listOfDevices is empty!!!");
			
			CComPtr<IMFMediaSource> pSource;
			
			CComPtr<IMFPresentationDescriptor> pPD;
	
			CComPtr<IMFStreamDescriptor> pSD;

			CComPtr<IMFMediaType> pMediaType;

			BOOL isSelected;

			result = MediaFoundation::getInstance().getSorceBySymbolicLink(listOfDevices[0].symbolicName, &pSource);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");
			
			
			result = MediaFoundation::getInstance().createPresentationDescriptor(pSource, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex() cannot be executed!!!");
	
			result = MediaFoundation::getInstance().getCurrentMediaType(pSD, &pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getCurrentMediaType() cannot be executed!!!");

			MediaType MT = FormatReader::getInstance().Read(pMediaType);



			CaptureSettings captureSettings;
			
			captureSettings.pIStopCallback = 0;

			captureSettings.readMode = ReadMode::SYNC;

			captureSettings.videoFormat = CaptureVideoFormat::RGB24;


			CComPtr<IMFMediaType> pSinkMediaType;

			CComPtr<IMFActivate> pSinkActivate;

			CComPtr<VideoCaptureSink> pSink = new VideoCaptureSink(0);
			
			result = MediaFoundation::getInstance().createMediaType(&pSinkMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().createSampleGrabberSinkActivate(pSinkMediaType, pSink, &pSinkActivate);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSampleGrabberSinkActivate() cannot be executed!!!");

			
			CComPtr<IMFTopology> pTopology;
			
			result = MediaFoundation::getInstance().createTopology(&pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopology() cannot be executed!!!");



			CComPtr<VideoCaptureDevice> pDevice = new VideoCaptureDevice;
			
			CComPtr<IMFTopologyNode> pNode1;

			CComPtr<IMFTopologyNode> pNode2;

	
			result = pDevice->addSourceNode(pTopology, pSource, pPD, pSD, &pNode1);

			Assert::IsTrue(result == ResultCode::OK, L"VIDEO CAPTURE DEVICE: addSourceNode() cannot be executed!!!");

			pSource->Shutdown();


		}
	};
}