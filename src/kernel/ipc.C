/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/kernel/ipc.C $                                            */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2013,2018                        */
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
#include <arch/ppc.H>
#include <kernel/ipc.H>
#include <kernel/cpu.H>
#include <kernel/intmsghandler.H>
#include <kernel/console.H>
#include <errno.h>
#include <kernel/doorbell.H>

using namespace KernelIpc;

/**
 * IPC communication area. Interrupt service provider initializes.
 * @see intrrp.C
 */
KernelIpc::ipc_data_area_t KernelIpc::ipc_data_area;

// Put the IPC message in the other nodes memory space
//    Two potential issues:
//    1. The destination node is not there - memory location is nonexistant.
//    2. The destination node never responds, potentially hanging this thread.
int KernelIpc::send(uint64_t i_q, msg_t * i_msg)
{
    // the destination node is a 3 bit field encoded in the 64 bit queue id
    // big endian bits 29:31, ie, xxxx_xxxN__xxxx_xxxx
    // extract it from the appropriate field
    uint64_t dest_node = ((i_q >> 32) &
                         (internode_info_vals_t::MAX_NODES_PER_SYS - 1));

    ipc_data_area_t * p_dest = ipc_data_area.remote_ipc_data_addr[dest_node];
    printkd("IPC Dest addr %px Q_id:%lx dest_node:%.lx\n",
            p_dest,i_q, dest_node);

    // validate destination address
    if ( (p_dest == nullptr ) ||
         ((reinterpret_cast<uint64_t>(p_dest) &
                 IPC_INVALID_REMOTE_ADDR_MASK) ==
           IPC_INVALID_REMOTE_ADDR))
    {
        return( EINVAL);
    }

    // get lock on IPC data area in other node
    if(false == __sync_bool_compare_and_swap(&(p_dest->msg_queue_id),
                                                IPC_DATA_AREA_CLEAR,
                                                IPC_DATA_AREA_LOCKED))
    {
        return -EAGAIN;
    }

    p_dest->msg_payload = *i_msg;  // copy in message
    lwsync();

    p_dest->msg_queue_id = i_q;    // set destination queue id
    lwsync();

    printk("IPC to PIR %x\n",p_dest->pir);

    // send doorbell to interrupt the other drawer
    send_doorbell_ipc(p_dest->pir);

    // The message allocation is freed here to make msg_send for IPC
    // messages behave the same as non-IPC msg_send; that is, the message
    // is freed by the consumer; however, i_msg was allocated in user space
    // code and freed here in kernel space. The assumption is that this is OK.
    msg_free(i_msg);

    return 0;
}


// update the address this IPC instance will use to send messages
//  to a remote node
int KernelIpc::updateRemoteIpcAddr(uint64_t i_Node, uint64_t i_RemoteAddr)
{
    int rc;
    if // input node is valid
      (i_Node < internode_info_vals_t::MAX_NODES_PER_SYS)
    {
        // update local array entry
        rc = 0;
        ipc_data_area.remote_ipc_data_addr[i_Node] =
                reinterpret_cast<ipc_data_area_t*>(i_RemoteAddr);
    }
    else
    {
        rc = EINVAL;
        printk("updateRemoteAddr() Invalid input node: %lx\n",i_Node);
    }

    return(rc);
}


// query the node and remote address other nodes will use to send
//  messages to this IPC instance
int KernelIpc::qryLocalIpcInfo(uint64_t * i_pONode, uint64_t * i_pOAddr)
{
    // determine node and remote address
    uint64_t l_localNode = getPIR()/KERNEL_MAX_SUPPORTED_CPUS_PER_NODE;

    uint64_t l_localAddr = reinterpret_cast<uint64_t>(&ipc_data_area);
    uint64_t l_hrmorOffset = (getHRMOR() -
                              (l_localNode * (ipc_data_area.hrmor_base)));
    uint64_t l_remoteHrmor =
            ((ipc_data_area.hrmor_base * l_localNode) + l_hrmorOffset);

    uint64_t l_oAddr = (( l_localAddr +
                          l_remoteHrmor ) |
                          0x8000000000000000ul);

    *i_pONode = l_localNode;
    *i_pOAddr = l_oAddr;

    return(0);
}

