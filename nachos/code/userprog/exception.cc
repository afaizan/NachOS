// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;        // Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SysCall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SysCall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
	  writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
	     writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
	     writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintChar)) {
	writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_GetReg))
   {
        printval = machine->ReadRegister(4);
        tempval = machine->ReadRegister(printval);
        machine->WriteRegister(2,tempval);

        // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   }
   else if ((which == SyscallException) && (type == SysCall_GetPA))
   {
        int virtAddr = machine->ReadRegister(4);
        int vpn = (unsigned) virtAddr/PageSize;
        int offset = (unsigned) virtAddr % PageSize;
        if (vpn >= machine->pageTableSize)
        {
                machine->WriteRegister(2,-1);
        }
        TranslationEntry *entry = &machine->KernelPageTable[vpn];
        if (entry == NULL)
                machine->WriteRegister(2,-1);
        if (entry->physicalPage >= NumPhysPages)
                machine->WriteRegister(2,-1);
        else
        {
                int physAddr = entry->physicalPage*PageSize + offset;
                machine->WriteRegister(2,physAddr);
        }

        // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   }
   else if ((which == SyscallException) && (type == SysCall_GetPID))
   {
        machine->WriteRegister(2,currentThread->GetPID());

        // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

   }
   else if ((which == SyscallException) && (type == SysCall_GetPPID))
   {
        machine->WriteRegister(2,currentThread->GetPPID());

        // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

   }
   else if ((which == SyscallException) && (type == SysCall_Time))
   {
       machine->WriteRegister(2, stats->totalTicks);
   
       // Advance program counters.	   
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   
   }
   else if ((which == SyscallException) && (type == SysCall_NumInstr))
   {   
       machine->WriteRegister(2, currentThread->GetInstructionCount());
       
       //Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

   }
	else if((which == SyscallException) && (type == SysCall_Yield)){
		currentThread->YieldCPU();
		//Advance PC
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
   else if ((which == SyscallException) && (type == SysCall_Exec)){
	vaddr = machine->ReadRegister(4);
	char file[100];
	int i=0;
	machine->ReadMem(vaddr, 1, &memval);

	while((*(char *)&memval) != '\0'){
	    file[i] = *(char *)&memval;
	    i = i+1;
	    vaddr = vaddr+1;
	    machine->ReadMem(vaddr, 1, &memval);
	}
	file[i] = *(char *)&memval;

	OpenFile *executable = fileSystem->Open(file);
	if (executable == NULL){
	    printf("Unable to open file %s\n", file);
	    return;
	}

	ProcessAddressSpace *space = new ProcessAddressSpace(executable);
	
	currentThread->space = space;

	space->InitUserModeCPURegisters();
	space->RestoreContextOnSwitch();

	machine->Run();
//	ASSERT(FALSE);
   }
   else if ((which == SyscallException) && (type == SysCall_Fork))
   {     // Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	   
	 // create a new kernel thread
	NachOSThread *child = new NachOSThread("forked thread");  
	 
	 // Set the parent of the child process
	child->parentthread = currentThread;
	   
	currentThread->initializeChildStatus(child->GetPID());
	   
	// Here we copy(or duplicate) the address-space of the currentThread to the childThread   
	child->space = new ProcessAddressSpace(currentThread->space);    
	  
        machine->WriteRegister(2, 0);   // Change the return address register to 0
	child->SaveUserState();         // save the child's state
	   
	// Setting the return value of the parent thread
	machine->WriteRegister(2, child->GetPID());   
	   
	// Allocating the stack to the child thread
	child->CreateThreadStack(&forkStart, 0);  
	   
	//The child thread is now ready to run   
	IntStatus oldLevel = interrupt->SetLevel(IntOff);    //disables interrupt
    	scheduler->MoveThreadToReadyQueue(this);	// MoveThreadToReadyQueue assumes that interrupts 
							// are disabled!
	(void) interrupt->SetLevel(oldLevel);                //re-enable interrupt
	      
   }	   
    else {
    	printf("Unexpected user mode exception %d %d\n", which, type);
    	ASSERT(FALSE);
    } 
}
