/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/isteps/istep08/call_proc_xbus_enable_ridi.C $         */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2018                        */
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

/**
   @file call_proc_xbus_enable_ridi.C
 *
 *  Support file for IStep: nest_chiplets
 *   Nest Chiplets
 *
 *  HWP_IGNORE_VERSION_CHECK
 *
 */

/******************************************************************************/
// Includes
/******************************************************************************/

//  Component ID support
#include <hbotcompid.H>                // HWPF_COMP_ID

//  TARGETING support
#include <attributeenums.H>            // TYPE_PROC, TYPE_PROC

//  Error handling support
#include <isteps/hwpisteperror.H>      // ISTEP_ERROR::IStepError

//  Tracing support
#include <trace/interface.H>           // TRACFCOMP
#include <initservice/isteps_trace.H>  // g_trac_isteps_trace
#include <initservice/initserviceif.H>

//  HWP call support
#include <nest/nestHwpHelperFuncs.H>   // fapiHWPCallWrapper

namespace ISTEP_08
{
using   namespace   ISTEP;
using   namespace   ISTEP_ERROR;
using   namespace   ISTEPS_TRACE;
using   namespace   TARGETING;


//******************************************************************************
// Wrapper function to call proc_xbus_enable_ridi
//******************************************************************************
void* call_proc_xbus_enable_ridi( void *io_pArgs )
{
    IStepError l_stepError;

    TRACFCOMP(g_trac_isteps_trace, ENTER_MRK"call_proc_xbus_enable_ridi entry");

    do {
        // Make the FAPI call to p9_xbus_enable_ridi
        if (!fapiHWPCallWrapperHandler(P9_XBUS_ENABLE_RIDI, l_stepError,
                                       HWPF_COMP_ID, TYPE_PROC))
        {
            break;
        }

        if (INITSERVICE::isSMPWrapConfig())
        {
            // Make the FAPI call to p9_chiplet_scominit
            // Make the FAPI call to p9_io_obus_firmask_save_restore, if previous call succeeded
            // Make the FAPI call to p9_psi_scominit, if previous call succeeded
            // Make the FAPI call to p9_io_obus_scominit, if previous call succeeded
            // Make the FAPI call to p9_npu_scominit, if previous call succeeded
            // Make the FAPI call to p9_chiplet_enable_ridi, if previous call succeeded
            fapiHWPCallWrapperHandler(P9_CHIPLET_SCOMINIT, l_stepError,
                                      HWPF_COMP_ID, TYPE_PROC)             &&
            fapiHWPCallWrapperHandler(P9_OBUS_FIRMASK_SAVE_RESTORE, l_stepError,
                                      HWPF_COMP_ID, TYPE_PROC)             &&
            fapiHWPCallWrapperHandler(P9_PSI_SCOMINIT, l_stepError,
                                      HWPF_COMP_ID, TYPE_PROC)             &&
            fapiHWPCallWrapperHandler(P9_IO_OBUS_SCOMINIT, l_stepError,
                                      HWPF_COMP_ID, TYPE_OBUS)             &&
            fapiHWPCallWrapperHandler(P9_NPU_SCOMINIT, l_stepError,
                                      HWPF_COMP_ID, TYPE_PROC)             &&
            fapiHWPCallWrapperHandler(P9_CHIPLET_ENABLE_RIDI, l_stepError,
                                      HWPF_COMP_ID, TYPE_PROC);
        }
    } while (0);
    TRACFCOMP(g_trac_isteps_trace, EXIT_MRK"call_proc_xbus_enable_ridi exit");

    return l_stepError.getErrorHandle();
}

};   // end namespace ISTEP_08
