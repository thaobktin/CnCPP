#include "stdafx.h"
#include "CppUnitTest.h"


#include "../videoInput/ReadWriteBufferFactory.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestvideoInput
{
	TEST_CLASS(UnitTestIReadWriteBuffer)
	{
	public:
		
		TEST_METHOD(createRegularReadWriteBuffer)
		{
			CComPtr<IReadWriteBuffer> pIReadWriteBuffer;

			ReadMode::Read readMode = ReadMode::SYNC;

			unsigned long sizeImage = 1920 * 1080 * 4;
							
			pIReadWriteBuffer = ReadWriteBufferFactory::getInstance().createIReadWriteBuffer(readMode, sizeImage);

			Assert::IsNotNull(pIReadWriteBuffer.p, L"pIReadWriteBuffer is NULL");
			
		}
		
		TEST_METHOD(ASYNCreadData)
		{
			CComPtr<IReadWriteBuffer> pIReadWriteBuffer;

			IRead *pIRead;

			ReadMode::Read readMode = ReadMode::ASYNC;

			unsigned long sizeImage = 1920 * 1080 * 4;

			std::auto_ptr<unsigned char> pixels;
			
			pixels.reset(new unsigned char[sizeImage]);
							
			pIReadWriteBuffer = ReadWriteBufferFactory::getInstance().createIReadWriteBuffer(readMode, sizeImage);

			pIRead = pIReadWriteBuffer;

			Assert::IsNotNull(pIReadWriteBuffer.p, L"pIReadWriteBuffer is NULL");

			ResultCode::Result state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_DONE, L"Reading Rejected");

			state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_REJECTED, L"Reading Rejected");
			
		}
		
		TEST_METHOD(SYNCreadData)
		{
			CComPtr<IReadWriteBuffer> pIReadWriteBuffer;

			IRead *pIRead;

			ReadMode::Read readMode = ReadMode::SYNC;

			unsigned long sizeImage = 1920 * 1080 * 4;

			std::auto_ptr<unsigned char> pixels;
			
			pixels.reset(new unsigned char[sizeImage]);
							
			pIReadWriteBuffer = ReadWriteBufferFactory::getInstance().createIReadWriteBuffer(readMode, sizeImage);

			pIRead = pIReadWriteBuffer;

			Assert::IsNotNull(pIReadWriteBuffer.p, L"pIReadWriteBuffer is NULL");

			ResultCode::Result state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_REJECTED_TIMEOUT, L"Reading Rejected");
			
		}
		
		TEST_METHOD(ASYNCwriteData_readData)
		{

			IRead *pIRead;

			CComPtr<IReadWriteBuffer> pIReadWriteBuffer;

			IWrite *pIWrite;

			ReadMode::Read readMode = ReadMode::ASYNC;

			unsigned long sizeImage = 1920 * 1080 * 4;

			std::auto_ptr<unsigned char> pixels;
			
			pixels.reset(new unsigned char[sizeImage]);
							
			pIReadWriteBuffer = ReadWriteBufferFactory::getInstance().createIReadWriteBuffer(readMode, sizeImage);

			pIRead = pIReadWriteBuffer; 

			pIWrite = pIReadWriteBuffer;

			Assert::IsNotNull(pIReadWriteBuffer.p, L"pIReadWriteBuffer is NULL");

			ResultCode::Result state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_DONE, L"Reading is rejected");

			pIWrite->writeData(pixels.get());

			state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_DONE, L"Reading is not done");

			state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_REJECTED, L"Reading is not rejected");
			
		}
		
		TEST_METHOD(SYNCwriteData_readData)
		{

			IRead *pIRead;

			CComPtr<IReadWriteBuffer> pIReadWriteBuffer;

			IWrite *pIWrite;

			ReadMode::Read readMode = ReadMode::SYNC;

			unsigned long sizeImage = 1920 * 1080 * 4;

			std::auto_ptr<unsigned char> pixels;
			
			pixels.reset(new unsigned char[sizeImage]);
							
			pIReadWriteBuffer = ReadWriteBufferFactory::getInstance().createIReadWriteBuffer(readMode, sizeImage);

			pIRead = pIReadWriteBuffer; 

			pIWrite = pIReadWriteBuffer;

			Assert::IsNotNull(pIReadWriteBuffer.p, L"pIReadWriteBuffer is NULL");

			ResultCode::Result state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_REJECTED_TIMEOUT, L"Reading is not rejected");

			pIWrite->writeData(pixels.get());

			state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_DONE, L"Reading is not done");

			state = pIRead->readData(pixels.get());

			Assert::IsTrue(state == ResultCode::READINGPIXELS_REJECTED_TIMEOUT, L"Reading is not rejected");
			
		}

	};
}