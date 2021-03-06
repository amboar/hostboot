/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/memory/p9_mss_draminit_mc.C $ */
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

///
/// @file p9_mss_draminit_mc.C
/// @brief Initialize the memory controller to take over the DRAM
///
// *HWP HWP Owner: Jacob Harvey <jlharvey@us.ibm.com>
// *HWP HWP Backup: Andre Marin <aamarin@us.ibm.com>
// *HWP Team: Memory
// *HWP Level: 3
// *HWP Consumed by: FSP:HB

#include <fapi2.H>
#include <mss.H>

#include <p9_mc_scom_addresses_fld.H>
#include <p9_mss_draminit_mc.H>
#include <lib/fir/unmask.H>
#include <generic/memory/lib/utils/find.H>
#include <generic/memory/lib/utils/count_dimm.H>
#include <lib/workarounds/mca_workarounds.H>

using fapi2::TARGET_TYPE_MCBIST;
using fapi2::TARGET_TYPE_MCA;
using fapi2::TARGET_TYPE_MCS;

extern "C"
{
///
/// @brief Initialize the MC now that DRAM is up
/// @param[in] i_target, the McBIST of the ports
/// @return FAPI2_RC_SUCCESS iff ok
///
    fapi2::ReturnCode p9_mss_draminit_mc( const fapi2::Target<TARGET_TYPE_MCBIST>& i_target )
    {
        FAPI_INF("%s Start draminit MC", mss::c_str(i_target));

        // No need to check to see if we have ports - this loop will just be skipped
        for (const auto& p : mss::find_targets<fapi2::TARGET_TYPE_MCA>(i_target))
        {
            //skip this MCA if we have no DIMM's configured
            if(mss::count_dimm(p) == 0)
            {
                FAPI_INF("No DIMM's configured on %s. Skipping this MCA.", mss::c_str(p));
                continue;
            }

            // Setup the MC port/dimm address translation registers
            FAPI_TRY( mss::mc::setup_xlate_map(p), "%s Failed setup_xlate_map", mss::c_str(i_target) );

            // Setup the read_pointer_delay
            // TK: Do we need to do this in general or is this a place holder until the
            // init file gets here?
            FAPI_TRY( mss::setup_read_pointer_delay(p), "Failed to setup read pointer delay for %s", mss::c_str(p));

            //Enable Power management based off of mrw_power_control_requested
            //Needs to be set near end of IPL
            FAPI_INF("%s Enable Power min max domains", mss::c_str(i_target));
            {
                fapi2::buffer<uint64_t> l_data;
                uint8_t  l_pwr_cntrl = 0;

                // Before enabling power controls, run the parity disable workaround
                for(const auto& l_mca : mss::find_targets<fapi2::TARGET_TYPE_MCA>(i_target))
                {
                    FAPI_TRY(mss::workarounds::str_non_tsv_parity(l_mca));
                }

                // TODO:RTC179508 - Cleanup draminit_mc
                // Mostly add in and fix register API elements below and create helper functions as need be
                FAPI_TRY(mss::getScom(p, MCA_MBARPC0Q, l_data));
                FAPI_TRY(mss::mrw_power_control_requested(l_pwr_cntrl));

                const bool is_pwr_cntrl = ((l_pwr_cntrl == fapi2::ENUM_ATTR_MSS_MRW_POWER_CONTROL_REQUESTED_POWER_DOWN)
                                           || (l_pwr_cntrl == fapi2::ENUM_ATTR_MSS_MRW_IDLE_POWER_CONTROL_REQUESTED_PD_AND_STR)
                                           || (l_pwr_cntrl == fapi2::ENUM_ATTR_MSS_MRW_IDLE_POWER_CONTROL_REQUESTED_PD_AND_STR_CLK_STOP));

                l_data.writeBit<MCA_MBARPC0Q_CFG_MIN_MAX_DOMAINS_ENABLE>(is_pwr_cntrl);

                FAPI_TRY( mss::putScom(p, MCA_MBARPC0Q, l_data) );

            }

            // Set the IML Complete bit MBSSQ(3) (SCOM Addr: 0x02011417) to indicate that IML has completed
            // Can't find MBSSQ or the iml_complete bit - asked Steve. Gary VH created this bit as a scratch
            // 'you are hre bit' and it was removed for Nimbus. Gary VH asked for it to be put back in. Not
            // sure if that happened yet. BRS (2/16).

            // Reset addr_mux_sel to “0” to allow the MCA to take control of the DDR interface over from CCS.
            // (Note: this step must remain in this procedure to ensure that data path is placed into mainline
            // mode prior to running memory diagnostics. This step may be superfluous but not harmful.)
            // Note: addr_mux_sel is set low in p9_mss_draminit(), however that might be a work-around so we
            // set it low here kind of like belt-and-suspenders. BRS
            FAPI_TRY( mss::change_addr_mux_sel(p, mss::LOW),
                      "%s Failed to change_addr_mux_sel", mss::c_str(i_target) );

            // Re-enable port fails. Turned off in draminit_training
            FAPI_TRY( mss::change_port_fail_disable(p, mss::OFF ),
                      "%s Failed to change_port_fail_disable", mss::c_str(i_target) );

            // MC work around for OE bug (seen in periodics + PHY)
#ifndef REMOVE_FOR_DD2
            // Turn on output-enable always on. Shelton tells me they'll fix for DD2
            FAPI_TRY( mss::change_oe_always_on(p, mss::ON ), "%s Failed to change_oe_always_on", mss::c_str(i_target) );
#endif
            // Step Two.1: Check RCD protect time on RDIMM and LRDIMM
            // Step Two.2: Enable address inversion on each MBA for ALL CARDS

            // Start the refresh engines by setting MBAREF0Q(0) = “1”. Note that the remaining bits in
            // MBAREF0Q should retain their initialization values.
            FAPI_TRY( mss::change_refresh_enable(p, mss::HIGH), "%s Failed change_refresh_enable", mss::c_str(i_target) );

            // Power management is handled in the init file. (or should be BRS)

            // Enabling periodic calibration
            FAPI_TRY( mss::enable_periodic_cal(p), "%s Failed enable_periodic_cal", mss::c_str(i_target) );

            // Step Six: Setup Control Bit ECC
            FAPI_TRY( mss::enable_read_ecc(p), "%s Failed enable_read_ecc", mss::c_str(i_target) );

            // apply marks from MVPD
            FAPI_TRY( mss::apply_mark_store(p), "%s Failed enable_read_ecc", mss::c_str(i_target) );
        }

        // At this point the DDR interface must be monitored for memory errors. Memory related FIRs should be unmasked.
        FAPI_TRY( mss::unmask::after_draminit_mc(i_target), "%s Failed after_draminit_mc", mss::c_str(i_target) );

    fapi_try_exit:
        FAPI_INF("End draminit MC");
        return fapi2::current_err;
    }
}
