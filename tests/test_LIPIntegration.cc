#include "doctest.h"
#include "testdata.h"

#include <lip.h>

#include <new>
#include <string.h>
//#include <stdex/hashlib.h>
#include <stdio.h>
#include <vvpkg/c_file_funcs.h>

#include <FileSystem/File.h>
#include <unistd.h>

#include <stdint.h>
#include <vvpkg/vvpkg.h>
#include <vvpkg/vfile.h>
#include <iostream>

//using namespace stdex::literals;
//using stdex::hashlib::hexlify;
using namespace std;
using namespace lip;

// TODO::move helper functions below here and above the first test case into a
// place to be shared
bool fileExists(const char* filePath)
{
	bool retval = false;
	FILE* tmp = fopen(filePath, "r");
	if (tmp != 0)
	{
		retval = true;
		fclose(tmp);
	}
	return retval;
};

char cCurrentPath[FILENAME_MAX];

void PrintCurrentPath()
{
	getcwd(cCurrentPath, sizeof(cCurrentPath));

	printf("The current working directory is %s\n", cCurrentPath);
}

#define testDataLIP1Root "./tests/testData/Lip1"
#define testDataLIP2Root "./tests/testData/Lip2"
#define testDataLIP1Output "./tests/testData/Lip1.lip"
#define testDataLIP2Output "./tests/testData/Lip2.lip"
#define testDataLIP1Checkout "./tests/testData/cLip1.lip"
#define testDataUnpackDirectory "./tests/testData/Output"

// TODO:: write into the CMakeLists.txt the commands to copy the necessary test
// directories from the src to the binary automatically.

// This tests the ability to create files with textOnly objects with no
// directories and no symlinks included to verify this it will also include an
// unpack and a compare to origonal sourceMaterial

//TODO:: make subcase of test to diff this with the Lip in VVPKG
TEST_CASE("lipCreate,TextFilesOnly,NoSubDIR,NoSymlink")
{
	//just so you can see where the files are on the machine if you wanted to verify them manually
	PrintCurrentPath();
	//verifying test files

	REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentEqual.txt"));

	REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentMatchedButDiffContent.txt"));

	REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentNotMatched.txt"));

	//creating LIP

	FILE* fileHandle = fopen(testDataLIP1Output, "w");

	REQUIRE(fileHandle != nullptr);

	//this is a somewhat arcane line that uses the existing lip archiver from Z, I don't know why he uses mutable labdas instead of just calling the file function and passing the handle. 
	//I would redo this if i had the time but there's too many things depending on it currently
	lip::archive(vvpkg::to_c_file(fileHandle), testDataLIP1Root);

	fclose(fileHandle);
	REQUIRE(fileExists(testDataLIP1Output));

	//reading lip index
	LIP t(testDataLIP1Output);

	LIP::Index* index = t.getIndex();

	REQUIRE(index->getIndexSize() == 4);

	index->dumpIndex();

	fcard* temp = index->getNext();
	fcard* check = temp;

	temp = index->getNext();
	temp = index->getNext();
	temp = index->getNext();
	temp = index->getNext();
	//		REQUIRE(temp == check);

			//unpacking LIP
	t.UnpackAt(testDataUnpackDirectory);

	REQUIRE(fileExists("./tests/testData/Output/Lip1/TextDocumentEqual.txt"));

	REQUIRE(fileExists("./tests/testData/Output/Lip1/TextDocumentMatchedButDiffContent.txt"));

	REQUIRE(fileExists("./tests/testData/Output/Lip1/TextDocumentNotMatched.txt"));

	printf("\nPack/Unpack TestPassed!!!\n");

	//t.UnpackAt()
}

TEST_CASE("lipDIFF,TextFilesOnly,NoSubDIR,NoSymlink")
{
	PrintCurrentPath();
	// verifying test files

	REQUIRE(fileExists("./tests/testData/Lip2/TextDocumentEqual.txt"));
	REQUIRE(fileExists("./tests/testData/Lip2/TextDocumentMatchedButDiffContent.txt"));
	REQUIRE(fileExists("./tests/testData/Lip2/TextDocumentOppositeNoMatch.txt"));

	// creating LIP

	FILE* fileHandle = fopen(testDataLIP2Output, "w");

	REQUIRE(fileHandle != nullptr);

	lip::archive(vvpkg::to_c_file(fileHandle), testDataLIP2Root);

	fclose(fileHandle);
	REQUIRE(fileExists(testDataLIP2Output));

	// reading lip index
	LIP lip2(testDataLIP2Output);

	LIP::Index* index = lip2.getIndex();

	REQUIRE(index->getIndexSize() == 4);

	index->dumpIndex();

	fcard* temp = index->getNext();
	fcard* check = temp;

	temp = index->getNext();
	temp = index->getNext();
	temp = index->getNext();
	temp = index->getNext();
	//		REQUIRE(temp == check);

	// unpacking LIP
	lip2.UnpackAt(testDataUnpackDirectory);

	REQUIRE(fileExists("./tests/testData/Output/Lip2/TextDocumentEqual.txt"));

	REQUIRE(fileExists("./tests/testData/Output/Lip2/"
		"TextDocumentMatchedButDiffContent.txt"));

	REQUIRE(fileExists(
		"./tests/testData/Output/Lip2/TextDocumentOppositeNoMatch.txt"));

	//TODO:: finish the asserts for the diff test, I need to implement the return structure for the diff operation first though. 
	//DIFF
	LIP lip1(testDataLIP1Output);

	lip1.diff(lip2);

	printf("End Diff\n");

}


TEST_CASE("insertLIP")
{

	auto repo = vvpkg::repository(".", "r+");
	vvpkg::managed_bundle<vvpkg::rabin_boundary> bs(10 * 1024 * 1024);

	char const* fn = testDataLIP1Output;

	commit("lip1", fn);

	REQUIRE(fileExists(testDataLIP1Output));

	//std::remove(testDataLIP1Output);

	//REQUIRE(!fileExists(testDataLIP1Output));
	
	checkout("lip1", testDataLIP1Checkout);

	REQUIRE(fileExists(testDataLIP1Output));
	REQUIRE(fileExists(testDataLIP1Checkout));

	{
		LIP lip1(testDataLIP1Output);

		LIP clip(testDataLIP1Checkout);

		lip1.diff(clip);
	}

	printf("Succees on checkincheckout\n\n");

	std::remove(testDataLIP1Checkout);

}

class filechunk
{
public:

	uint64_t offset;
	uint64_t size;

	filechunk()
	{
		offset = 0;
		size = 0;
	}

	~filechunk(){}

	filechunk& operator =(const filechunk& rhs)
	{
		this->offset = rhs.offset;
		this->size = rhs.size;
		return *this;
	}

	filechunk(const filechunk& rhs) : offset(rhs.offset), size(rhs.size){}

	filechunk(std::pair<uint64_t, uint64_t>& pair)
	{
		offset = pair.first;
		size = pair.second;
	}

};

TEST_CASE("extract from vfile")
{

	vvpkg::vfile t(".");

	auto pairs = t.list("lip1");

	std::pair<uint64_t, uint64_t> list = pairs();

	std::list<filechunk> chunkList;
	printf("listing pairs\n");

	int i = 0;

	while (list.second != 0)
	{
		chunkList.emplace_back(list);
		printf("pair %i starts at %i and is of size %i\n", i, list.first, list.second);

		i++;
		list = pairs();
	}

	printf("end of list\n");

	printf("vfile id = %s\n", t.id().to_string().c_str());

	File::Handle fh;
	File::Handle outFh;

	REQUIRE(File::Open(fh, "./vvpkg.bin", File::Mode::READ)==File::SUCCESS);
	REQUIRE(File::Open(outFh, "./LipvFileOut.lip", File::Mode::WRITE)==File::SUCCESS);

	for (filechunk c : chunkList)
	{
		File::Seek(fh,File::Location::BEGIN, c.offset);

		char* buff = new char[c.size];

		File::Read(fh, buff, c.size);
		File::Write(outFh, buff, c.size);

		delete buff;
	}

	File::Close(fh);
	File::Close(outFh);
	
	{
		REQUIRE(fileExists(testDataLIP1Output));
		LIP lip1Control(testDataLIP1Output);
		LIP lip1FromvFile("./LipvFileOut.lip");

		lip1Control.diff(lip1FromvFile);
	}

	std::remove("./LipvFileOut.lip");

}

////this still may be a viable option but it would completely shift the paradiam that has been used so far for dealing with files and it would'nt match what vvpkg does so i'm leaving this here as a starter point in case someone else decides to pick up the torch
//class handleOverrideTest : public streambuf
//{
//	FILE* handle;
//public:
//	handleOverrideTest(const char* filePath)
//	{
//		File::Open(handle, filePath, File::Mode::READ);
//	}
//	handleOverrideTest();
//	~handleOverrideTest(){File::Close(handle);}
//	handleOverrideTest& operator = (const handleOverrideTest& rhs) = default;
//	handleOverrideTest(const handleOverrideTest& rhs) = default;
//	int underflow() override{}
//};

//class dedupedFileInfo
//{
//public:
//	std::list<filechunk> chunkList;
//	uint64_t chunkCount;
//	uint64_t fileSize;
//
//	dedupedFileInfo() = delete;
//
//	dedupedFileInfo(std::list<filechunk> _chunkList)
//	{
//		chunkList = _chunkList;
//	}
//	~dedupedFileInfo()	{}
//};

//ReadOnly FileHandle into deduplicated chunks, chunks cannot be edited once in VVPKG
//Note this is here in the test cases beacuse this still needs testing with larger LIP's to make sure all the operations work properly across chunks. 
//I didn't get a chance to generate data to cover edge cases
class dedupedFileHandle : public readOnlyFileHandle
{
	//file info
	std::list<filechunk> chunkList;
	uint64_t chunkCount;
	uint64_t fileSize;

	//stream info
	uint64_t logicalReadCursor;
	//uint64_t currentChunkNumber;
	uint64_t currentChunkOffset;

private:

	//this is an ineffecient way to handle this but I am just going for functionality. I need to setup a system to remember which chunk I'm currently in and page back and forth
	filechunk findChunk(uint64_t offset)
	{
		uint64_t cumlativeOffset = 0;

		for (filechunk t : chunkList)
		{
			if (cumlativeOffset + t.size > offset)
			{
				currentChunkOffset = offset - cumlativeOffset;
				return t;
			}
			else
			{
				cumlativeOffset += t.size;
			}
		}
		//if chunk doesn't exist offset is outside file range
		return filechunk();
	}

public:

	dedupedFileHandle(std::list<filechunk> cL) : chunkList(cL), logicalReadCursor(0), currentChunkOffset(0), chunkCount(0), fileSize(0)
	{
		//if the file doesn't open nothing else will work;
		assert(File::SUCCESS == File::Open(handle, "./vvpkg.bin", File::READ));
		
		for (filechunk c : cL)
		{
			fileSize += c.size;
			chunkCount++;
		}
	}

	void Close() override
	{
		File::Close(handle);
	}

	int64_t Read(void* const _buffer, const int64_t _size) override
	{
		char* fillCursor = (char*)_buffer;//where the next byte will be entered;
		int64_t sizeRemaining = _size;//how many bytes still need to be read into the buffer
		int64_t readbytes = 0; //number of bytes read successfully

		filechunk chunk;

		while (sizeRemaining != 0)
		{
			
			chunk = findChunk(logicalReadCursor);

			//check for EOF
			if (chunk.size == 0)
			{
				return readbytes;
			}

			if (chunk.size >= currentChunkOffset + sizeRemaining)
			{//one whole read from chunk all at once 
				File::Seek(handle, File::BEGIN, chunk.offset + currentChunkOffset);
				File::Read(handle, fillCursor, sizeRemaining);
				logicalReadCursor += sizeRemaining;
				currentChunkOffset += sizeRemaining;
				sizeRemaining = 0;
			}
			else //chunk size is less than the buffer to be filled fill what we can and increment
			{// read from the chunk and increase the logical cursor and the fillCursor and refind chunk;

				int64_t sizeToRead = chunk.size - currentChunkOffset;

				File::Seek(handle, File::BEGIN, chunk.offset + currentChunkOffset);
				File::Read(handle, fillCursor, sizeToRead);
				
				logicalReadCursor += sizeToRead;
				currentChunkOffset += sizeToRead;
				sizeRemaining -= sizeToRead;
				fillCursor += sizeToRead;

			}
		}
	}

	//NOTE:: the fgets function appends a null terminator and breaks on newline this doesn't exactly work that way this only really works for our use case because I just need the file names from vvpkg there's not multiple lines to read.
	void ReadLine(char* const buffer, const int maxSize) override
	{
		bool EOLFound = false;
		int sizeRemaining = maxSize;
		char* buffCursor = buffer;

		filechunk chunk = findChunk(logicalReadCursor);

		while (sizeRemaining != 0 && !EOLFound)
		{
			//TODO:: optimze this above I only find the chunk and then read as much of it as possibble/necessary,
			//with this I am only reading one byte at a time so it's not ideal i should only refind chunk after I know I need too i.e i am out of bytes on this chunk. but for now this works
			chunk = findChunk(logicalReadCursor);

			File::Seek(handle, File::BEGIN, chunk.offset + currentChunkOffset);
			File::Read(handle, buffCursor, 1);
			
			if (*buffCursor == '\0')
			{
				EOLFound = true;
			}

			logicalReadCursor++;
			//currentChunkOffset++;
			buffCursor++;
			sizeRemaining--;
		}
	}

	//I'm gonna start with seek from beginning only add from current and from end later
	//void Seek(int64_t offset)
	//{
	//	//convert the logical file offset to the physical deduplicated chunk + offset into that chunk
	//	logicalReadCursor = offset;
	//	findChunk(logicalReadCursor);
	//}

	void Seek(int64_t offset, File::Location fileLocation = File::Location::BEGIN) override
	{
		if (fileLocation == File::Location::BEGIN)
		{
			logicalReadCursor = offset;
		}
		else if (fileLocation == File::Location::CURRENT)
		{
			logicalReadCursor += offset;
		}
		else
		{//I'm assuming since how it's used in the standard c functions is you pass a negatve offset to rewind;
			logicalReadCursor = fileSize + offset;
		}
		findChunk(logicalReadCursor);
	}

	int64_t Tell() override
	{
		//return the current logical file offset
		return logicalReadCursor;
	}

	int64_t FileSize()
	{
		return fileSize;
	}

};

dedupedFileHandle* getHandleFromVVPKG(const char* fileName)
{
	vvpkg::vfile t(".");

	auto pairs = t.list(fileName);

	std::pair<uint64_t, uint64_t> list = pairs();

	std::list<filechunk> chunkList;
	printf("listing pairs\n");

	int i = 0;

	while (list.second != 0)
	{
		chunkList.emplace_back(list);
		printf("pair %i starts at %i and is of size %i\n", i, list.first, list.second);

		i++;
		list = pairs();
	}

	printf("end of list\n");

	printf("vfile id = %s\n", t.id().to_string().c_str());

	return new dedupedFileHandle(chunkList);
}

//this is to test the deduped handle approach
TEST_CASE("dedupedFileHandleTest")
{

	dedupedFileHandle* t(getHandleFromVVPKG("lip1"));

	REQUIRE(t->Tell() == 0);

	char magic[4];

	t->Read(magic, 4);

	assert(magic[0] == 'L');
	assert(magic[1] == 'I');
	assert(magic[2] == 'P');
	assert(magic[3] == 0);

	REQUIRE(t->Tell() == 4);

	t->Seek(0);

	REQUIRE(t->Tell() == 0);

	REQUIRE(fileExists(testDataLIP1Output));

	LIP ControlLIP1(testDataLIP1Output);

	LIP vvLIPTest(t);

	REQUIRE(ControlLIP1.diff(vvLIPTest));

	printf("control diff version in vvpkg is perfect match!");
}

//this is to test the stream wrapper approach
TEST_CASE("deduped multiblock handle test")
{

	//this is just to test to make sure the stream underflow works on a normal file
	//handleOverrideTest t(testDataLIP1Output);



}


TEST_CASE("create lip from VVPKGHandle")
{



}

TEST_CASE("lipStreamWrapper")
{
	//auto repo = vvpkg::repository(".", "r+");
	//vvpkg::managed_bundle<vvpkg::rabin_boundary> bs(10 * 1024 * 1024);

	//dedupedFileHandle vHandle = getHandleFromVVPKG("lip1");
	//vHandle.read();
	
	//File::Handle outFh;

	//REQUIRE(File::Open(vHandle, "./vvpkg.bin", File::Mode::READ) == File::SUCCESS);
	//REQUIRE(File::Open(outFh, "./LipvFileOut.lip", File::Mode::WRITE) == File::SUCCESS);

	//File::Close(fh);
	//File::Close(outFh);

	//{
	//	LIP lip1Control(testDataLIP1Output);
	//	LIP lip1FromvFile("./LipvFileOut.lip");

	//	lip1Control.diff(lip1FromvFile);
	//}

	//std::remove("./LipvFileOut.lip");
	
}