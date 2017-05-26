// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "synch.h"
#include "memorymanager.h"

extern MemoryManager *memoryManager;
extern Lock *memoryLock;

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, j, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
    
    this->noffH = noffH;
    this->executable = executable;

    // how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    //ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
    	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
    	pageTable[i].physicalPage = -1;
        /*if(memoryManager->IsAnyPageFree() == true)
            pageTable[i].physicalPage = memoryManager->AllocPage();
        else
        {
            for(j = 0; j < i; ++j)
                memoryManager->FreePage(pageTable[j].physicalPage);
            ASSERT(false);
        }*/

    	pageTable[i].valid = false;
    	pageTable[i].use = false;
    	pageTable[i].dirty = false;
    	pageTable[i].readOnly = false;  // if the code segment was entirely on 
                    					// a separate page, we could set its 
                    					// pages to be read-only
    }
    
    // zero out the entire address space, to zero the unitialized data segment 
    // and the stack segment
    //bzero(machine->mainMemory, size);
    //memoryLock->Acquire();
    /*for(i = 0; i < numPages; ++i)
    {
        bzero(&machine->mainMemory[pageTable[i].physicalPage * PageSize], PageSize);
    }*/

    
    // then, copy in the code and data segments into memory
    /*unsigned int numPagesForCode = divRoundUp(noffH.code.size, PageSize);
    DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
		noffH.code.virtualAddr, noffH.code.size);
    for(i = 0; i < numPagesForCode; ++i)
    {
        executable->ReadAt(&(machine->mainMemory[ pageTable[i].physicalPage * PageSize ]),
                            PageSize, 
                            noffH.code.inFileAddr + i * PageSize);
    }

    unsigned int numPagesForData = divRoundUp(noffH.initData.size, PageSize);

    DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
        noffH.initData.virtualAddr, noffH.initData.size);
    for(j = numPagesForCode; j < numPagesForCode + numPagesForData; ++j)
    {
        executable->ReadAt(&(machine->mainMemory[ pageTable[i].physicalPage * PageSize ]),
                            PageSize, 
                            noffH.initData.inFileAddr + (j - numPagesForCode) * PageSize);
    }*/
    //memoryLock->Release();

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
   delete executable;           // close file
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

int min(int a, int b)
{
    if(a < b)
        return a;
    return b;
}


int 
AddrSpace::loadIntoFreePage(int addr, int physicalPageNo){
    int vpn = addr / PageSize;
    pageTable[vpn].physicalPage = physicalPageNo;
    pageTable[vpn].valid = true;

    int codeOffset, codeSize, initDataOffset, initDataSize, uninitDataSize;
    //jei page e poreche tar start e niye jacchi
    addr = ( addr / PageSize ) * PageSize;

    if(addr >= noffH.code.virtualAddr  &&  addr < noffH.code.virtualAddr + noffH.code.size)
    {
        //addr koto tomo page e poreche, tar start byte offset
        codeOffset = ( ( addr - noffH.code.virtualAddr ) / PageSize ) * PageSize;
        //code segment theke koto byte porte hobe
        codeSize = min( noffH.code.size - codeOffset, PageSize );

        executable->ReadAt(&(machine->mainMemory[ pageTable[vpn].physicalPage * PageSize ]),
                            codeSize, 
                            noffH.code.inFileAddr + codeOffset);

        if( codeSize < PageSize )
        {
            initDataSize = min( PageSize - codeSize, noffH.initData.size );

            executable->ReadAt(&(machine->mainMemory[ pageTable[vpn].physicalPage * PageSize + codeSize ]),
                            initDataSize, 
                            noffH.initData.inFileAddr);

            if( codeSize + initDataSize < PageSize )
            {
                uninitDataSize = PageSize - codeSize - initDataSize;

                bzero(&machine->mainMemory[ pageTable[vpn].physicalPage * PageSize + codeSize + initDataSize ],
                    uninitDataSize);
            }
        }
    }
    else if(addr >= noffH.initData.virtualAddr  &&  addr < noffH.initData.virtualAddr + noffH.initData.size)
    {
        initDataOffset = ( (addr - noffH.initData.virtualAddr) / PageSize ) * PageSize;
        initDataSize = min( noffH.initData.size - initDataOffset, PageSize );

        executable->ReadAt(&(machine->mainMemory[ pageTable[vpn].physicalPage * PageSize ]),
                            initDataSize, 
                            noffH.initData.inFileAddr + initDataOffset);

        if( initDataSize < PageSize )
        {
            uninitDataSize = PageSize - initDataSize;

            bzero(&machine->mainMemory[ pageTable[vpn].physicalPage * PageSize + initDataSize ],
                uninitDataSize);
        }
    }
    else
    {
        bzero(&machine->mainMemory[pageTable[vpn].physicalPage * PageSize], PageSize);
    }

    return 0;
}
