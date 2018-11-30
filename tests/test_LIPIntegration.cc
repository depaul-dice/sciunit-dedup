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
TEST_CASE("lipCreate,TextFilesOnly,NoSubDIR,NoSymlink")
{
	PrintCurrentPath();
	//verifying test files

	REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentEqual.txt"));

	REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentMatchedButDiffContent.txt"));

	REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentNotMatched.txt"));

	//creating LIP

	FILE* fileHandle = fopen(testDataLIP1Output, "w");

	REQUIRE(fileHandle != nullptr);

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
		LIP lip1Control(testDataLIP1Output);
		LIP lip1FromvFile("./LipvFileOut.lip");

		lip1Control.diff(lip1FromvFile);
	}

	std::remove("./LipvFileOut.lip");

}

//TODO: FILE* isn't the right spot for me to extend to be at I think I need to do either an indirectbuf or stream buff
//this is working because the chunks are signular I need to override the underflow operator I think, still more research to do on how to make 
//an ideal stream wrapper for vvpkg, I wanna try overloading the stream operators and see what that gets me.
//https://www.slac.stanford.edu/comp/unix/gnu-info/iostream_5.html#SEC26 resource to follow up on
struct vvHandle : public FILE //public indrectbuf //public streambuf
{

	std::list<filechunk> chunkList;
	uint64_t readCursor;
	File::Handle vvbin;

	vvHandle(std::list<filechunk> cL)
	{
		chunkList = cL;
		File::Open(vvbin, "./vvpkg.bin", File::READ);
	}

	vvHandle(const vvHandle&) = delete;
	vvHandle operator = (const vvHandle&) = delete;

	vvHandle(const vvHandle&& other)
	{
		this->chunkList = other.chunkList;
		this->vvbin = other.vvbin;
	}

	vvHandle& operator = (const vvHandle&& other)
	{
		this->chunkList = other.chunkList;
		this->vvbin = other.vvbin;

		return *this;
	}

	~vvHandle()
	{
		File::Close(vvbin);
	}

};

vvHandle getHandleFromVVPKG()
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

	return vvHandle(chunkList);

}

TEST_CASE("lipStreamWrapper")
{

	//auto repo = vvpkg::repository(".", "r+");
	//vvpkg::managed_bundle<vvpkg::rabin_boundary> bs(10 * 1024 * 1024);

	vvHandle vHandle = getHandleFromVVPKG();
	//vHandle.read();
	
	File::Handle outFh;

	//REQUIRE(File::Open(vHandle, "./vvpkg.bin", File::Mode::READ) == File::SUCCESS);
	REQUIRE(File::Open(outFh, "./LipvFileOut2.lip", File::Mode::WRITE) == File::SUCCESS);

	
	for (filechunk c : vHandle.chunkList)
	{
		File::Seek(vHandle.vvbin, File::Location::BEGIN, c.offset);

		char* buff = new char[c.size];

		File::Read(vHandle.vvbin, buff, c.size);
		File::Write(outFh, buff, c.size);

		delete buff;

	}

	//File::Close(fh);
	File::Close(outFh);

	{
		LIP lip1Control(testDataLIP1Output);
		LIP lip1FromvFile("./LipvFileOut2.lip");

		lip1Control.diff(lip1FromvFile);
	}

	std::remove("./LipvFileOut2.lip");

	

	
}