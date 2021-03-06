/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/kernel/kernel.C $                                         */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2010,2018                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
#include <stdint.h>
#include <kernel/console.H>
#include <kernel/pagemgr.H>
#include <kernel/heapmgr.H>
#include <kernel/cpumgr.H>
#include <util/singleton.H>
#include <kernel/cpu.H>
#include <kernel/task.H>
#include <kernel/scheduler.H>
#include <kernel/taskmgr.H>
#include <kernel/vmmmgr.H>
#include <kernel/timemgr.H>
#include <sys/vfs.h>
#include <kernel/deferred.H>
#include <kernel/misc.H>
#include <util/align.H>
#include <securerom/sha512.H>
#include <kernel/bltohbdatamgr.H>
#include <kernel/cpuid.H>

#include <stdlib.h>

extern "C" void kernel_dispatch_task();
extern void* init_main(void* unused);
extern uint64_t kernel_other_thread_spinlock;


class Kernel
{
    public:
	void cppBootstrap();
	void memBootstrap();
	void cpuBootstrap();
	void inittaskBootstrap();

    protected:
	Kernel() {};
};


/**
 * @brief Searches multiple locations for the BlToHbData structure.
 *
 * @description Due to the possibility of a mismatch between the bootloader and
 *              hostboot, we must check every location of this structure in
 *              history. Searches latest to oldest.
 *
 * @return if found, pointer to structure
 *         otherwise nullptr
 */
const Bootloader::BlToHbData* getBlToHbData()
{
    const Bootloader::BlToHbData* l_resultBltoHbData = nullptr;

    // Order bootloader addr versions from most recent to oldest
    // Note: Defined here to limit code space in bootloader code
    static const std::array<uint64_t,2> BlToHbDataAddrs =
    {
        BLTOHB_COMM_DATA_ADDR_LATEST,
        BLTOHB_COMM_DATA_ADDR_V1
    };

    for (auto addr : BlToHbDataAddrs)
    {
        const auto l_bltoHbData =
                          reinterpret_cast<const Bootloader::BlToHbData*>(addr);
        if(Bootloader::BlToHbDataValid(l_bltoHbData))
        {
            l_resultBltoHbData = l_bltoHbData;
            break;
        }
    }

    return l_resultBltoHbData;
}

extern "C"
int main()
{
    printk("Booting %s kernel...\n\n", "Hostboot");
    printk("CPU=%s  PIR=%ld\n",
           ProcessorCoreTypeStrings[CpuID::getCpuType()],
           static_cast<uint64_t>(getPIR()));
    MAGIC_INST_PRINT_ISTEP(6,2);

    // Erase task-pointer so that TaskManager::getCurrentTask() returns NULL.
    setSPRG3(NULL);

    Kernel& kernel = Singleton<Kernel>::instance();
    kernel.cppBootstrap();


    // Get pointer to BL and HB comm data
    const auto l_pBltoHbData = getBlToHbData();
    if ( l_pBltoHbData != nullptr )
    {
        printk("Valid BlToHbData found at 0x%lX\n", reinterpret_cast<uint64_t>(l_pBltoHbData));
        // Initialize Secureboot Data class
        g_BlToHbDataManager.initValid(*l_pBltoHbData);
    }
    else
    {
        printk("BL to HB commun invalid\n");
        // Force invalidation of securebootdata
        g_BlToHbDataManager.initInvalid();
    }

    kernel.memBootstrap();
    kernel.cpuBootstrap();

    // Let FSP/BMC know that Hostboot is now running
    KernelMisc::setHbScratchStatus(KernelMisc::HB_RUNNING);

    kernel.inittaskBootstrap();

    // Ready to let the other CPUs go.
    lwsync();
    kernel_other_thread_spinlock = 1;

    kernel_dispatch_task(); // no return.
    while(1);
    return 0;
}

extern "C"
int smp_slave_main(cpu_t* cpu)
{
    // Erase task-pointer so that TaskManager::getCurrentTask() returns NULL.
    setSPRG3(NULL);

    CpuManager::init_slave_smp(cpu);
    VmmManager::init_slb();
    cpu->scheduler->setNextRunnable();
    DeferredQueue::execute();
    kernel_dispatch_task();
    return 0;
}

void Kernel::cppBootstrap()
{
    // Call default constructors for any static objects.
    extern void (*ctor_start_address)();
    extern void (*ctor_end_address)();
    void(**ctors)() = &ctor_start_address;
    while(ctors != &ctor_end_address)
    {
        (*ctors)();
        ctors++;
    }
}

void Kernel::memBootstrap()
{
    PageManager::init();
    HeapManager::init();
    VmmManager::init();
}

void Kernel::cpuBootstrap()
{
    TimeManager::init();
    CpuManager::init();
}

void Kernel::inittaskBootstrap()
{
    task_t * t = TaskManager::createTask(&init_main, NULL, true);
    t->cpu = CpuManager::getCurrentCPU();
    TaskManager::setCurrentTask(t);
}

