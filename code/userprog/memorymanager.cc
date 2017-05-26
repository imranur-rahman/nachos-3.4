#include "memorymanager.h"

MemoryManager::MemoryManager(int numPages)
{
	bitMap = new BitMap(numPages);
	lock = new Lock("lock of memory manager");
	processMap = new int[numPages];
	entries = new TranslationEntry*[numPages];
}

MemoryManager::~MemoryManager()
{
	delete bitMap;
	delete lock;
	delete processMap;
	delete entries;
}

int 
MemoryManager::Alloc(int processNo, TranslationEntry *entry)
{
	lock->Acquire();
	int ret = bitMap->Find();
	if(ret >= 0)
	{
		processMap[ret] = processNo;
		entries[ret] = entry;
	}
	else
	{
		printf("kono jhamela hoiche\n");
	}
	lock->Release();
	return ret;
}

int
MemoryManager::AllocByForce()
{
	lock->Acquire();
	int ret = Random() % NumPhysPages;
	if(entries[ret] != NULL)
	{
		entries[ret]->valid = false;
		//physicalPage chaile -1 kora jeto
	}
	else
	{
		printf("entry is not valid\n");
	}
	lock->Release();
	return ret;
}

int
MemoryManager::AllocPage()
{
	lock->Acquire();
	int ret = bitMap->Find();
	lock->Release();
	return ret;
}

void
MemoryManager::FreePage(int physPageNum)
{
	lock->Acquire();
	bitMap->Clear(physPageNum);
	lock->Release();
}

bool
MemoryManager::PageIsAllocated(int physPageNum)
{
	lock->Acquire();
	bool ret = bitMap->Test(physPageNum);
	lock->Release();
	return ret;
}

bool
MemoryManager::IsAnyPageFree()
{
	lock->Acquire();
	bool ret;
	if(bitMap->NumClear() == 0)
		ret = false;
	else
		ret = true;
	lock->Release();
	return ret;
}

int
MemoryManager::NumFreePages()
{
	lock->Acquire();
	int ret = bitMap->NumClear();
	lock->Release();
	return ret;
}