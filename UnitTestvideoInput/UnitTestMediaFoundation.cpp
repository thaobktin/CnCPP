#include "stdafx.h"
#include "CppUnitTest.h"


#include <mfobjects.h>
#include <mfidl.h>
#include <mfapi.h>


#include "../videoInput/MediaFoundation.h"
#include "../videoInput/VideoCaptureSink.h"
#include "../videoInput/VideoCaptureSession.h"
#include "../videoInput/ComMassivPtr.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestvideoInput
{
	TEST_CLASS(UnitTestMediaFoundation)
	{
	public:
		
		TEST_METHOD(initializeCOM)
		{
			ResultCode::Result result = MediaFoundation::getInstance().initializeCOM();

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: initializeCOM() cannot be executed!!!");
		}
		
		TEST_METHOD(initializeMF)
		{
			ResultCode::Result result = MediaFoundation::getInstance().initializeMF();

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: initializeMF() cannot be executed!!!");
		}
		
		TEST_METHOD(shutdown)
		{
			ResultCode::Result result = MediaFoundation::getInstance().shutdown();

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: shutdown() cannot be executed!!!");
		}
		
		TEST_METHOD(getState)
		{
			ResultCode::Result result = MediaFoundation::getInstance().getInitializationState();

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getState() cannot be executed!!!");
		}
		
		TEST_METHOD(enumDevices)
		{		
			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");

		}	
		
		TEST_METHOD(readFriendlyName)
		{
			using namespace std;
					

			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");

		
			
			wstring friendlyName;
			
			result = MediaFoundation::getInstance().readFriendlyName(ppDevices[0], friendlyName);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readFriendlyName cannot be read!!!");

			Assert::IsTrue(friendlyName == L"1.3M WebCam", wstring(wstring(L"MEDIA FOUNDATION: friendlyName is ") + friendlyName).c_str());
			
		}
		
		TEST_METHOD(readSymbolicLink)
		{
			using namespace std;
			

			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");



			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readSymbolicLink cannot be read!!!");
			
		}		
		
		TEST_METHOD(getDevice)
		{
			using namespace std;
			
			
			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");



			CComPtr<IMFMediaSource> pDevice;
						
			result = MediaFoundation::getInstance().getSource(ppDevices[0], &pDevice);
			
			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getDevice cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");
			
			pDevice->Shutdown();

		}

		TEST_METHOD(createPresentationDescriptor)
		{
			using namespace std;
			
			CComPtr<IMFPresentationDescriptor> pPD;

			CComPtr<IMFMediaSource> pDevice;

			
			
			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");

			
			
			
			result = MediaFoundation::getInstance().getSource(ppDevices[0], &pDevice);
					

			
			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getDevice cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");
						
			pDevice->Shutdown();

		}
		
		TEST_METHOD(getAmountStreams)
		{
			using namespace std;
			
			CComPtr<IMFPresentationDescriptor> pPD;
			
			CComPtr<IMFMediaSource> pDevice;



			
			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");



			result = MediaFoundation::getInstance().getSource(ppDevices[0], &pDevice);
					

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getDevice cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");

			pDevice->Shutdown();
						
		}
		
		TEST_METHOD(getStreamDescriptorByIndex)
		{
			using namespace std;
			
			CComPtr<IMFPresentationDescriptor> pPD;
			
			CComPtr<IMFMediaSource> pDevice;


			
			
			CComMassivPtr<IMFActivate> ppDevices;
			
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");



			result = MediaFoundation::getInstance().getSource(ppDevices[0], &pDevice);
					

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getDevice cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");

			CComPtr<IMFStreamDescriptor> pSD;
			
			BOOL isSelected;

			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex cannot be executed!!!");

			Assert::IsNotNull(pSD.p, L"pSD: it is NULL!!!");
			
			pDevice->Shutdown();
			
		}
		
		TEST_METHOD(enumMediaType)
		{
			using namespace std;
			
			CComPtr<IMFPresentationDescriptor> pPD;
			
			CComPtr<IMFMediaSource> pDevice;
			
			CComPtr<IMFStreamDescriptor> pSD;

			BOOL isSelected;			
			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");



			result = MediaFoundation::getInstance().getSource(ppDevices[0], &pDevice);
					
			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getDevice cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");


			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex cannot be executed!!!");

			Assert::IsNotNull(pSD.p, L"pSD: it is NULL!!!");

			std::vector<MediaType> listMediaType;

			result = MediaFoundation::getInstance().enumMediaType(pSD, listMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: enumMediaType cannot be executed!!!");

			Assert::IsTrue(listMediaType.size() > 0, L"MEDIA FOUNDATION: listMediaType is empty!!!");
						
			pDevice->Shutdown();
						
		}
		
		TEST_METHOD(getSorceBySymbolicLink)
		{
			using namespace std;

			CComPtr<IMFMediaSource> pDevice;

					
			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");





			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readSymbolicLink cannot be read!!!");
						


			result = MediaFoundation::getInstance().getSorceBySymbolicLink(symbolicLink, &pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			pDevice->Shutdown();

		}

		TEST_METHOD(selectStream)
		{
			using namespace std;

			CComPtr<IMFMediaSource> pDevice;

			CComPtr<IMFPresentationDescriptor> pPD;
			
			CComPtr<IMFStreamDescriptor> pSD;

			BOOL isSelected;


			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");



			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readSymbolicLink cannot be read!!!");
						


			result = MediaFoundation::getInstance().getSorceBySymbolicLink(symbolicLink, &pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");


			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex cannot be executed!!!");

			Assert::IsNotNull(pSD.p, L"pSD: it is NULL!!!");

			result = MediaFoundation::getInstance().selectStream(0, pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: selectStream() cannot be executed!!!");

			pDevice->Shutdown();

			
		}

		TEST_METHOD(deselectStream)
		{
			using namespace std;

			CComPtr<IMFMediaSource> pDevice;

			CComPtr<IMFPresentationDescriptor> pPD;

			
			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");




			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readSymbolicLink cannot be read!!!");
						

			result = MediaFoundation::getInstance().getSorceBySymbolicLink(symbolicLink, &pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");

			result = MediaFoundation::getInstance().deselectStream(0, pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: selectStream() cannot be executed!!!");
			
			pDevice->Shutdown();
		}
		
		TEST_METHOD(setCurrentMediaType)
		{
			using namespace std;

			CComPtr<IMFMediaSource> pDevice;

			CComPtr<IMFPresentationDescriptor> pPD;


			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");




			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readSymbolicLink cannot be read!!!");
						

			result = MediaFoundation::getInstance().getSorceBySymbolicLink(symbolicLink, &pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");

			CComPtr<IMFStreamDescriptor> pSD;

			BOOL isSelected;

			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex cannot be executed!!!");

			Assert::IsNotNull(pSD.p, L"pSD: it is NULL!!!");

			result = MediaFoundation::getInstance().setCurrentMediaType(pSD, 0);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: setCurrentMediaType() cannot be executed!!!");
			
			pDevice->Shutdown();
		}
		
		TEST_METHOD(getCurrentMediaType)
		{
			using namespace std;

			CComPtr<IMFMediaSource> pDevice;

			CComPtr<IMFPresentationDescriptor> pPD;

			CComPtr<IMFMediaType> pCurrentType;

			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");


			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: readSymbolicLink cannot be read!!!");
						


			result = MediaFoundation::getInstance().getSorceBySymbolicLink(symbolicLink, &pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");

			Assert::IsNotNull(pDevice.p, L"pDevice: it is NULL!!!");

			result = MediaFoundation::getInstance().createPresentationDescriptor(pDevice, &pPD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createPresentationDescriptor cannot be executed!!!");

			Assert::IsNotNull(pPD.p, L"pPD: it is NULL!!!");

			DWORD streamCount = 0;

			result = MediaFoundation::getInstance().getAmountStreams(pPD, streamCount);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getAmountStreams cannot be executed!!!");

			Assert::IsTrue(streamCount != 0, L"streamCount: it is 0!!!");

			CComPtr<IMFStreamDescriptor> pSD;

			BOOL isSelected;

			result = MediaFoundation::getInstance().getStreamDescriptorByIndex(0, pPD, isSelected, &pSD);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getStreamDescriptorByIndex cannot be executed!!!");

			Assert::IsNotNull(pSD.p, L"pSD: it is NULL!!!");

			result = MediaFoundation::getInstance().getCurrentMediaType(pSD, &pCurrentType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getCurrentMediaType() cannot be executed!!!");
			
			pDevice->Shutdown();
		}

		TEST_METHOD(createSession)
		{
			using namespace std;

			CComPtr<IMFMediaSession> pSession;
			
			ResultCode::Result result = MediaFoundation::getInstance().createSession(&pSession);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSession() cannot be executed!!!");
						
		}

		TEST_METHOD(createMediaType)
		{
			using namespace std;

			CComPtr<IMFMediaType> pMediaType;
			
			ResultCode::Result result = MediaFoundation::getInstance().createMediaType(&pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
						
		}

		TEST_METHOD(setGUID)
		{
			using namespace std;

			CComPtr<IMFMediaType> pMediaType;
			
			ResultCode::Result result = MediaFoundation::getInstance().createMediaType(&pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().setGUID(pMediaType, MF_MT_MAJOR_TYPE, MFMediaType_Video);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: setGUID() cannot be executed!!!");
			
		}

		TEST_METHOD(setUINT32)
		{
			using namespace std;

			CComPtr<IMFMediaType> pMediaType;
			
			ResultCode::Result result = MediaFoundation::getInstance().createMediaType(&pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().setUINT32(pMediaType, MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, TRUE);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: setUINT32() cannot be executed!!!");
			
		}

		TEST_METHOD(createSampleGrabberSinkActivate)
		{
			using namespace std;

			CComPtr<IMFMediaType> pMediaType;

			CComPtr<IMFActivate> pSinkActivate;

			CComPtr<VideoCaptureSink> pSink = new VideoCaptureSink(0);
			
			ResultCode::Result result = MediaFoundation::getInstance().createMediaType(&pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().createSampleGrabberSinkActivate(pMediaType, pSink, &pSinkActivate);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSampleGrabberSinkActivate() cannot be executed!!!");
			
		}
		
		TEST_METHOD(createTopology)
		{
			CComPtr<IMFTopology> pTopology;
			
			ResultCode::Result result = MediaFoundation::getInstance().createTopology(&pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopology() cannot be executed!!!");
		}	

		TEST_METHOD(createTopologyNode)
		{
			CComPtr<IMFTopologyNode> pTopologyNode;
			
			ResultCode::Result result = MediaFoundation::getInstance().createTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pTopologyNode);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");
		}
		
		TEST_METHOD(setUnknown)
		{
			
			using namespace std;

			CComPtr<IMFMediaSource> pDevice;

			CComPtr<IMFPresentationDescriptor> pPD;

			
			CComMassivPtr<IMFActivate> ppDevices;
						
			ResultCode::Result result = MediaFoundation::getInstance().enumDevices(ppDevices.getPtrMassivPtr(), *ppDevices.setSizeMassivPtr());

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: Devices cannot be enumurated!!!");

			Assert::IsNotNull(ppDevices.getMassivPtr(), L"ppDevices: it is NULL!!!");

			Assert::IsTrue(ppDevices.setSizeMassivPtr() != 0, L"count: it is 0!!!");


			wstring symbolicLink;

			result = MediaFoundation::getInstance().readSymbolicLink(ppDevices[0], symbolicLink);
						

			result = MediaFoundation::getInstance().getSorceBySymbolicLink(symbolicLink, &pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: getSorceBySymbolicLink() cannot be executed!!!");
			
			CComPtr<IMFTopologyNode> pTopologyNode;
			
			result = MediaFoundation::getInstance().createTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pTopologyNode);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");

			result = MediaFoundation::getInstance().setUnknown(pTopologyNode, MF_TOPONODE_SOURCE,  pDevice);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: setUnknown() cannot be executed!!!");
			
			pDevice->Shutdown();
		}

		TEST_METHOD(setObject)
		{
			using namespace std;

			CComPtr<IMFMediaType> pMediaType;

			CComPtr<IMFActivate> pSinkActivate;

			CComPtr<VideoCaptureSink> pSink = new VideoCaptureSink(0);
			
			ResultCode::Result result = MediaFoundation::getInstance().createMediaType(&pMediaType);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createMediaType() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().createSampleGrabberSinkActivate(pMediaType, pSink, &pSinkActivate);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSampleGrabberSinkActivate() cannot be executed!!!");
			
			CComPtr<IMFTopologyNode> pTopologyNode;
			
			result = MediaFoundation::getInstance().createTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pTopologyNode);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().setObject(pTopologyNode, pSinkActivate);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");


		}

		TEST_METHOD(addNode)
		{
			CComPtr<IMFTopology> pTopology;
			
			ResultCode::Result result = MediaFoundation::getInstance().createTopology(&pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopology() cannot be executed!!!");
			
			CComPtr<IMFTopologyNode> pTopologyNode;
			
			result = MediaFoundation::getInstance().createTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pTopologyNode);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().addNode(pTopology, pTopologyNode);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: addNode() cannot be executed!!!");
		}

		TEST_METHOD(connectOutputNode)
		{
			
			CComPtr<IMFTopologyNode> pTopologyNode1;
			
			ResultCode::Result result = result = MediaFoundation::getInstance().createTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pTopologyNode1);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");
			
			CComPtr<IMFTopologyNode> pTopologyNode2;
			
			result = MediaFoundation::getInstance().createTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pTopologyNode2);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().connectOutputNode(pTopologyNode1, pTopologyNode2);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopologyNode() cannot be executed!!!");


		}
		
		TEST_METHOD(setTopology)
		{
			CComPtr<IMFTopology> pTopology;
			
			ResultCode::Result result = MediaFoundation::getInstance().createTopology(&pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createTopology() cannot be executed!!!");

			CComPtr<IMFMediaSession> pSession;
			
			result = MediaFoundation::getInstance().createSession(&pSession);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSession() cannot be executed!!!");
			
			result = MediaFoundation::getInstance().setTopology(pSession, pTopology);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSession() cannot be executed!!!");
			
		}

		TEST_METHOD(beginGetEvent)
		{
			using namespace std;

			CComPtr<IMFMediaSession> pSession;
			
			ResultCode::Result result = MediaFoundation::getInstance().createSession(&pSession);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: createSession() cannot be executed!!!");

			CComPtr<VideoCaptureSession> pVideoCaptureSession = new VideoCaptureSession();
			
			
			result = MediaFoundation::getInstance().beginGetEvent(pSession, pVideoCaptureSession, 0);

			Assert::IsTrue(result == ResultCode::OK, L"MEDIA FOUNDATION: beginGetEvent() cannot be executed!!!");
									
		}	
	};
}