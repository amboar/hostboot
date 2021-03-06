/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/io/p9_io_xbus_dccal.C $    */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2017                        */
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
/// @file p9_io_xbus_dccal.C
/// @brief Train the Link.
///-----------------------------------------------------------------------------
/// *HWP HWP Owner        : Chris Steffen <cwsteffen@us.ibm.com>
/// *HWP HWP Backup Owner : Gary Peterson <garyp@us.ibm.com>
/// *HWP FW Owner         : Jamie Knight <rjknight@us.ibm.com>
/// *HWP Team             : IO
/// *HWP Level            : 3
/// *HWP Consumed by      : FSP:HB
///-----------------------------------------------------------------------------
///
/// @verbatim
/// High-level procedure flow:
///
/// Run Dccal
///
/// Dccal is completed on all thin/thick PHYs:
///     - XBUS(EDIP)
///
/// Procedure Prereq:
///     - System clocks are running.
///     - Scominit Procedure is completed.
///
/// @endverbatim
///----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------
#include <p9_io_xbus_dccal.H>
#include <p9_io_scom.H>
#include <p9_io_regs.H>
#include <p9_io_common.H>

//-----------------------------------------------------------------------------
// Function Declarations
//-----------------------------------------------------------------------------


/**
 * @brief Tx Z Impedance Calibration State Machine
 * @param[in] i_tgt FAPI2 Target
 * @retval ReturnCode
 */
fapi2::ReturnCode tx_zcal_run_bus( const XBUS_TGT i_tgt );

/**
 * @brief Tx Z Impedance Calibration
 * @param[in] i_tgt  FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode tx_zcal_set_grp( const XBUS_TGT i_tgt, const uint8_t i_grp );

/**
 * @brief Rx Dc Calibration Poll
 * @param[in] i_tgt  FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode rx_dccal_poll_grp( const XBUS_TGT i_tgt, const uint8_t i_grp );

/**
 * @brief Rx Dc Calibration Start
 * @param[in] i_tgt  FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode rx_dccal_start_grp( const XBUS_TGT i_tgt, const uint8_t i_grp );

//-----------------------------------------------------------------------------
//  Function Definitions
//-----------------------------------------------------------------------------

/**
 * @brief A I/O EDI+ Procedure that runs Rx Dccal and Tx Z Impedance calibration
 * on every instance of the XBUS on a per group level.
 * @param[in] i_mode Selects which operation to perform
 * @param[in] i_tgt  FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode p9_io_xbus_dccal(
    const XbusDccalMode i_mode,
    const XBUS_TGT      i_tgt,
    const uint8_t       i_grp )
{
    FAPI_IMP( "p9_io_xbus_dccal: I/O EDI+ Xbus Entering" );

    char l_tgtStr[fapi2::MAX_ECMD_STRING_LEN];
    fapi2::toString( i_tgt, l_tgtStr, fapi2::MAX_ECMD_STRING_LEN );
    FAPI_DBG( "I/O EDI+ Xbus Dccal %s:g%d", l_tgtStr, i_grp );

    switch( i_mode )
    {
        case XbusDccalMode::Noop: // Runs nothing, can be used for dry runs
            FAPI_IMP( "I/O EDI+ Xbus Dccal Noop" );
            break;

        case XbusDccalMode::TxZcalRunBus: // Runs Tx Zcal on a per bus basis
            FAPI_TRY( tx_zcal_run_bus( i_tgt ), "I/O Edi+ Xbus Tx Z-Cal Run Bus Failed" );
            break;

        case XbusDccalMode::TxZcalSetGrp: // Sets Tx Zcal Group Settings based on the bus results
            FAPI_TRY( tx_zcal_set_grp( i_tgt, i_grp ), "I/O Edi+ Xbus Tx Z-Cal Set Grp Failed" );
            break;

        case XbusDccalMode::RxDccalStartGrp: // Starts Rx Dccal on a per group basis
            FAPI_TRY( rx_dccal_start_grp( i_tgt, i_grp ), "I/O Edi+ Xbus Rx DC Cal Start Failed" );
            break;

        case XbusDccalMode::RxDccalCheckGrp: // Checks/polls Rx Dccal on a per group basis
            FAPI_TRY( rx_dccal_poll_grp( i_tgt, i_grp ), "I/O Edi+ Xbus Rx DC Cal Poll Failed" );
            break;

        default:
            FAPI_ERR( "I/O EDI+ Xbus Dccal No Valid Mode" );
            break;
    }

fapi_try_exit:
    FAPI_IMP( "p9_io_xbus_dccal: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Convert a 4R decimal value to a 1R thermometer code
 * @param[in] i_4r_val 4R Value
 * @retval Converted 1R Value
 */
uint32_t convert_4r( const uint32_t i_4r_val )
{
    // 1. Add 2 for averaging since we will truncate the last 2 bits
    // 2. Divide by 4 to bring back to a 1r value
    // 2. Convert the decimal number to number of bits set by shifting
    //    a 0x1 over by the amount and subtracting 1
    return ( ( 0x1 << ( ( i_4r_val + 2 ) / 4 ) ) - 1  );
}

uint32_t convert_4r_with_2r( const uint32_t i_4r_val, const uint8_t i_width )
{
    // Add 1 for rounding, then shift the 4r bit off.  We now have a 2r equivalent
    uint32_t l_2r_equivalent = ( i_4r_val + 1 ) >> 0x1;

    // If the LSB of the 2r equivalent is on, then we need to set the 2r bit (MSB)
    uint32_t l_2r_on = ( l_2r_equivalent & 0x1 );

    // Shift the 2r equivalent to a 1r value and convert to a thermometer code.
    uint32_t l_1r_equivalent ( ( 0x1 << ( l_2r_equivalent >> 0x1 ) ) - 1 );

    // combine 1r equivalent thermometer code + the 2r MSB value.
    return ( l_2r_on << ( i_width - 1 ) ) | l_1r_equivalent ;
}

/**
 * @brief Tx Z Impedance Calibration Get Results
 * @param[in] io_pval  Tx Zcal P-value
 * @param[in] io_nval  Tx Zcal N-value
 * @retval ReturnCode
 */
fapi2::ReturnCode tx_zcal_verify_results(
    uint32_t& io_pvalx4,
    uint32_t& io_nvalx4 )
{
    // l_zcal_p and l_zcal_n are 9 bit registers
    // These are also 4x of a 1R segment
    const uint32_t X4_MIN = 16 * 4; // 16 segments * 4 = 64 (0x40)
    const uint32_t X4_MAX = 33 * 4; // 33 segments * 4 = 132(0x84)
    FAPI_IMP( "tx_zcal_verify_results: I/O EDI+ Xbus Entering" );

    FAPI_INF( "tx_zcal_verify_results: Min/Max Allowed(0x%X,0x%X) Read Pval/Nval(0x%X,0x%X)",
              X4_MIN, X4_MAX,
              io_pvalx4, io_nvalx4 );

    if( io_pvalx4 > X4_MAX )
    {
        FAPI_ERR("I/O EDI+ Xbus Tx Zcal Pval(0x%X) > Max Allowed(0x%X); Code will override with 0x%X and continue.",
                 io_pvalx4, X4_MAX, X4_MAX);
        io_pvalx4 = X4_MAX;
    }

    if( io_nvalx4 > X4_MAX )
    {
        FAPI_ERR("I/O EDI+ Xbus Tx Zcal Nval(0x%X) > Max Allowed(0x%X); Code will override with 0x%X and continue.",
                 io_nvalx4, X4_MAX, X4_MAX);
        io_nvalx4 = X4_MAX;
    }

    if( io_pvalx4 < X4_MIN )
    {
        FAPI_ERR("I/O EDI+ Xbus Tx Zcal Pval(0x%X) < Min Allowed(0x%X); Code will override with 0x%X and continue.",
                 io_pvalx4, X4_MIN, X4_MIN);
        io_pvalx4 = X4_MIN;
    }

    if( io_nvalx4 < X4_MIN )
    {
        FAPI_ERR("I/O EDI+ Xbus Tx Zcal Nval(0x%X) < Min Allowed(0x%X); Code will override with 0x%X and continue.",
                 io_nvalx4, X4_MIN, X4_MIN);
        io_nvalx4 = X4_MIN;
    }

    FAPI_IMP( "tx_zcal_verify_results: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Tx Z Impedance Calibration State Machine
 * @param[in] i_tgt FAPI2 Target
 * @retval ReturnCode
 */
fapi2::ReturnCode tx_zcal_run_bus( const XBUS_TGT i_tgt )
{
    const uint64_t DLY_20MS         = 20000000;
    const uint64_t DLY_10US         = 10000;
    const uint64_t DLY_10MIL_CYCLES = 10000000;
    const uint64_t DLY_1MIL_CYCLES  = 1000000;
    const uint32_t TIMEOUT          = 200;
    const uint8_t GRP0              = 0;
    const uint8_t LN0               = 0;
    uint32_t l_count                = 0;
    uint64_t l_data                 = 0;
    uint8_t       l_is_sim = 0;

    FAPI_IMP( "tx_zcal_run_sm: I/O EDI+ Xbus Entering" );

    ///////////////////////////////////////////////////////////////////////////
    /// Simulation Speed Up
    ///////////////////////////////////////////////////////////////////////////
    FAPI_TRY( FAPI_ATTR_GET(fapi2::ATTR_IS_SIMULATION, fapi2::Target<fapi2::TARGET_TYPE_SYSTEM>(), l_is_sim) );

    if( l_is_sim )
    {
        // To speed up simulation, xbus_unit model + pie driver
        //   without these settings: 50 million cycles
        //   with these settings: 13 million cycles
        io::set( EDIP_TX_ZCAL_SM_MIN_VAL, 50, l_data );
        io::set( EDIP_TX_ZCAL_SM_MAX_VAL, 52, l_data );
        FAPI_TRY( io::write( EDIP_TX_IMPCAL_SWO2_PB, i_tgt, GRP0, LN0, l_data ) );
    }

    // Request to start Tx Impedance Calibration
    // The Done bit is read only pulse, must use pie driver or system model in sim
    FAPI_TRY( io::rmw( EDIP_TX_ZCAL_REQ, i_tgt, GRP0, LN0, 1 ),
              "tx_zcal_run_sm: RMW Tx Zcal Req Failed");

    // Delay before we start polling.  20ms was use from past p8 learning
    FAPI_TRY( fapi2::delay( DLY_20MS, DLY_10MIL_CYCLES ),
              "tx_zcal_run_sm: Fapi Delay Failed." );

    // Poll Until Tx Impedance Calibration is done or errors out
    FAPI_TRY( io::read( EDIP_TX_IMPCAL_PB, i_tgt, GRP0, LN0, l_data ),
              "tx_zcal_run_sm: Reading Tx Impcal Pb Failed" );

    while( ( ++l_count < TIMEOUT ) &&
           !( io::get( EDIP_TX_ZCAL_DONE, l_data ) || io::get( EDIP_TX_ZCAL_ERROR, l_data ) ) )
    {
        FAPI_DBG( "tx_zcal_run_sm: I/O EDI+ Xbus Tx Zcal Poll, Count(%d/%d).", l_count, TIMEOUT );

        // Delay for 10us between polls for a total of 22ms.
        FAPI_TRY( fapi2::delay( DLY_10US, DLY_1MIL_CYCLES ),
                  "tx_zcal_run_sm: Fapi Delay Failed." );

        FAPI_TRY( io::read( EDIP_TX_IMPCAL_PB, i_tgt, GRP0, LN0, l_data ),
                  "tx_zcal_run_sm: Reading Tx Impcal Pb Failed" );
    }

    // Check for Zcal Done
    if( io::get( EDIP_TX_ZCAL_DONE, l_data ) == 1 )
    {
        FAPI_DBG( "I/O EDI+ Xbus Tx Zcal Poll Completed(%d/%d).", l_count, TIMEOUT );
    }

    // Check for Zcal Error
    if( io::get( EDIP_TX_ZCAL_ERROR, l_data ) == 1 )
    {
        FAPI_ERR( "I/O EDI+ Xbus Tx Z Calibration Error" );
    }

    // Check for Zcal Timeout
    if( l_count >= TIMEOUT)
    {
        FAPI_ERR( "I/O EDI+ Xbus Tx Z Calibration Timeout: Loops(%d)", l_count );
    }

fapi_try_exit:
    FAPI_IMP( "tx_zcal_run_sm: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Tx Z Impedance Calibration Apply Segments.  The results of the Tx Impedance
 *   calibrationMargining and FFE Precursor
 * @param[in] i_tgt FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @param[in] i_pval   Tx Zcal P-value
 * @param[in] i_nval   Tx Zcal N-value
 * @retval ReturnCode
 */
fapi2::ReturnCode tx_zcal_apply(
    const XBUS_TGT i_tgt,
    const uint8_t  i_grp,
    const uint32_t i_pval,
    const uint32_t i_nval )
{
    FAPI_IMP( "tx_zcal_apply: I/O EDI+ Xbus Entering" );
    const uint8_t LN0              = 0;
    const uint8_t PRE_WIDTH        = 5;
    const uint8_t MAIN_WIDTH       = 13;
    //             4R Total        = ( 1R * 4 ) + (2R * 2 );
    const uint32_t PRE_4R_TOTAL    = (  4 * 4 ) + ( 1 * 2 );
    const uint32_t MARGIN_4R_TOTAL = (  8 * 4 ) + ( 0 * 2 );
    const uint32_t MAIN_4R_TOTAL   = ( 12 * 4 ) + ( 1 * 2 );
    uint8_t        l_margin_ratio  = 0;
    uint8_t        l_ffe_pre_coef  = 0;
    uint32_t       p_en_pre        = 0;
    uint32_t       p_en_margin_pu  = 0;
    uint32_t       p_en_margin_pd  = 0;
    uint32_t       p_en_main       = 0;
    uint32_t       p_sel_pre       = 0;
    uint32_t       n_en_pre        = 0;
    uint32_t       n_en_margin_pu  = 0;
    uint32_t       n_en_margin_pd  = 0;
    uint32_t       n_en_main       = 0;
    uint32_t       n_sel_pre       = 0;
    uint32_t       sel_margin_pu   = 0;
    uint32_t       sel_margin_pd   = 0;
    uint32_t       l_4r_pval       = i_pval;
    uint32_t       l_4r_nval       = i_nval;

    // Encoding of Margin Ratio and Tx FFE Precursor
    // 100.00% = 128(0x80) / 128
    //  75.00% =  96(0x60) / 128
    //  50.00% =  64(0x40) / 128
    //  25.00% =  32(0x20) / 128
    //   4.67% =   6(0x06) / 128
    //   0.00% =   0(0x00) / 128

    // During normal operation we will not use margining :: min(0, 0%) max(0.5, 50%)
    FAPI_TRY( FAPI_ATTR_GET( fapi2::ATTR_IO_XBUS_TX_MARGIN_RATIO, i_tgt, l_margin_ratio ) );

    //   -4.8% FFE Precursor Tap Weight :: min(0.0, 0%) max(0.115, 11.5%)
    FAPI_TRY( FAPI_ATTR_GET( fapi2::ATTR_IO_XBUS_TX_FFE_PRECURSOR, i_tgt, l_ffe_pre_coef ) );

    ///////////////////////////////////////////////////////////////////////////
    // Set P val enables
    // All Precursor Segments will always be enabled as the mininum number
    //   of segments is 16 bits enabled, while there are only 4 1R & 1 2R
    //   Precursor Segments.
    p_en_pre       = PRE_4R_TOTAL;
    p_en_margin_pu = MARGIN_4R_TOTAL;
    p_en_margin_pd = MARGIN_4R_TOTAL;

    l_4r_pval -= p_en_pre;

    // If # of segments is less than margin max, calculate correct margin bits,
    //   otherwise we will stick with setting the margins to the max.
    if( l_4r_pval < ( 2 * MARGIN_4R_TOTAL ) )
    {
        // If the l_4r_pval( running count of remaining 4r segments to apply),
        //   has a 2r segment that it needs to apply, we need to apply this to the
        //   main 2r bit.
        if( ( l_4r_pval % 4 ) != 0 )
        {
            p_en_main = 2;
            l_4r_pval -= p_en_main;
        }

        // Apply half of what we need to pd, then apply remaining to the pu.
        //   The pu will always be equal or +1 segment.
        // We will divide the value by 4 to make a 1r segment count. Then we
        //   will again divide by 2, so we can split up between the pd & pu
        //   margins. This will also truncate the fraction, so that the extra
        //   can be applied to the pu.  We then multiply by 4 to get it back to
        //   a 4r equivelant. *Note, the divide must be before the 4R multiplier
        //   in order to truncate.
        p_en_margin_pd = (l_4r_pval / (4 * 2) ) * 4;
        p_en_margin_pu = l_4r_pval - p_en_margin_pd;
    }

    // Apply the remaining segments to the main bank
    p_en_main += l_4r_pval - p_en_margin_pu - p_en_margin_pd;

    // We should never have more segments than allowed since we check the results
    //   before we eneter this function.  Although, if we do exceed the max main
    //   then we will set the main segments to the max.
    if( p_en_main > MAIN_4R_TOTAL )
    {
        p_en_main = MAIN_4R_TOTAL;
    }

    // Precursor FFE Select bits will be set as a percentage out of 128
    p_sel_pre = ( i_pval * l_ffe_pre_coef ) / 128;

    // We can only select as many precursor segments as we have enabled.
    if( p_sel_pre > p_en_pre )
    {
        p_sel_pre = p_en_pre;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Set N val enables
    // All Precursor Segments will always be enabled as the mininum number
    //   of segments is 16 bits enabled, while there are only 4 1R & 1 2R
    //   Precursor Segments.
    n_en_pre       = PRE_4R_TOTAL;
    n_en_margin_pu = MARGIN_4R_TOTAL;
    n_en_margin_pd = MARGIN_4R_TOTAL;

    l_4r_nval -= n_en_pre;

    // If # of segments is less than margin max, calculate correct margin bits,
    //   otherwise we will stick with setting the margins to the max.
    if( l_4r_nval < ( 2 * MARGIN_4R_TOTAL ) )
    {
        // If the l_4r_nval( running count of remaining 4r segments to apply),
        //   has a 2r segment that it needs to apply, we need to apply this to the
        //   main 2r bit.
        if( ( l_4r_nval % 4 ) != 0 )
        {
            n_en_main = 2;
            l_4r_nval -= n_en_main;
        }

        // Apply half of what we need to pd, then apply remaining to the pu.
        //   The pu will always be equal or +1 segment.
        // We will divide the value by 4 to make a 1r segment count. Then we
        //   will again divide by 2, so we can split up between the pd & pu
        //   margins. This will also truncate the fraction, so that the extra
        //   can be applied to the pu.  We then multiply by 4 to get it back to
        //   a 4r equivelant. *Note, the divide must be before the 4R multiplier
        //   in order to truncate.
        n_en_margin_pd = ( l_4r_nval / ( 4 * 2 ) ) * 4;
        n_en_margin_pu = l_4r_nval - n_en_margin_pd;
    }

    // Apply the remaining segments to the main bank
    n_en_main += l_4r_nval - n_en_margin_pu - n_en_margin_pd;

    // We should never have more segments than allowed since we check the results
    //   before we eneter this function.  Although, if we do exceed the max main
    //   then we will set the main segments to the max.
    if( n_en_main > MAIN_4R_TOTAL )
    {
        n_en_main = MAIN_4R_TOTAL;
    }

    // Precursor FFE Select bits will be set as a percentage out of 128
    n_sel_pre = ( i_nval * l_ffe_pre_coef ) / 128;

    // We can only select as many precursor segments as we have enabled.
    if( n_sel_pre > n_en_pre )
    {
        n_sel_pre = n_en_pre;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Set Margin Selects
    // l_margin_ratio(0-64) / 128 = (0 - 0.5)
    // To calculate the margin selects,
    // select = ( P/N 4R Value * Margin Ratio(margin_val / 128) ) / 2
    // The final divide by 2 is because we have a pu & a pd.
    sel_margin_pu = ( i_pval * l_margin_ratio ) / ( 128 * 2 );

    // We can only select as many margin segments as we have enabled.
    if( sel_margin_pu > n_en_margin_pu )
    {
        sel_margin_pu = n_en_margin_pu;
    }

    // We can only select as many margin segments as we have enabled.
    if( sel_margin_pu > p_en_margin_pu )
    {
        sel_margin_pu = p_en_margin_pu;
    }

    // l_margin_ratio(0-64) / 128 = (0 - 0.5)
    // To calculate the margin selects,
    // select = ( P/N 4R Value * Margin Ratio(margin_val / 128) ) / 2
    // The final divide by 2 is because we have a pu & a pd.
    sel_margin_pd = ( i_nval * l_margin_ratio ) / ( 128 * 2 );

    // We can only select as many margin segments as we have enabled.
    if( sel_margin_pd > n_en_margin_pd )
    {
        sel_margin_pd = n_en_margin_pd;
    }

    // We can only select as many margin segments as we have enabled.
    if( sel_margin_pd > p_en_margin_pd )
    {
        sel_margin_pd = p_en_margin_pd;
    }

    // Finally Make sure the margin pu / pd are the same.  If not, take the
    //   lower select.
    if( sel_margin_pu > sel_margin_pd )
    {
        sel_margin_pu = sel_margin_pd;
    }
    else if( sel_margin_pu < sel_margin_pd )
    {
        sel_margin_pd = sel_margin_pu;
    }



    // We can write these registers without reading, since we are writing
    //   the entire register.  To convert the 4R values to needed register values,
    //   we will  add the appropriate amount and shift to convert to 1R or
    //   1R + a 2R.
    FAPI_TRY( io::rmw( EDIP_TX_PSEG_PRE_EN, i_tgt, i_grp, LN0,  convert_4r_with_2r( p_en_pre, PRE_WIDTH ) ),
              "tx_zcal_apply: Pseg Pre Enable RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_PSEG_PRE_SEL, i_tgt, i_grp, LN0, convert_4r_with_2r( p_sel_pre, PRE_WIDTH ) ),
              "tx_zcal_apply: Pseg Pre Select RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_NSEG_PRE_EN, i_tgt, i_grp, LN0, convert_4r_with_2r( n_en_pre, PRE_WIDTH ) ),
              "tx_zcal_apply: Nseg Pre Enable RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_NSEG_PRE_SEL, i_tgt, i_grp, LN0, convert_4r_with_2r( n_sel_pre, PRE_WIDTH ) ),
              "tx_zcal_apply: Nseg Pre Select RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_PSEG_MARGINPD_EN, i_tgt, i_grp, LN0, convert_4r( p_en_margin_pd ) ),
              "tx_zcal_apply: Pseg Margin Enable RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_PSEG_MARGINPU_EN, i_tgt, i_grp, LN0, convert_4r( p_en_margin_pu ) ),
              "tx_zcal_apply: Pseg Margin Enable RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_NSEG_MARGINPD_EN, i_tgt, i_grp, LN0, convert_4r( n_en_margin_pd ) ),
              "tx_zcal_apply: Nseg Margin Enable RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_NSEG_MARGINPU_EN, i_tgt, i_grp, LN0, convert_4r( n_en_margin_pu ) ),
              "tx_zcal_apply: Nseg Margin Enable RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_MARGINPD_SEL, i_tgt, i_grp, LN0, convert_4r( sel_margin_pd ) ),
              "tx_zcal_apply: Margin PD Select RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_MARGINPU_SEL, i_tgt, i_grp, LN0, convert_4r( sel_margin_pu ) ),
              "tx_zcal_apply: Margin PU Select RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_PSEG_MAIN_EN, i_tgt, i_grp, LN0, convert_4r_with_2r( p_en_main, MAIN_WIDTH ) ),
              "tx_zcal_apply: Pseg Main RMW Fail" );

    FAPI_TRY( io::rmw( EDIP_TX_NSEG_MAIN_EN, i_tgt, i_grp, LN0, convert_4r_with_2r( n_en_main, MAIN_WIDTH ) ),
              "tx_zcal_apply: Nseg Main RMW Fail" );

fapi_try_exit:
    FAPI_IMP( "tx_zcal_apply: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Tx Z Impedance Calibration
 * @param[in] i_tgt  FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode tx_zcal_set_grp( const XBUS_TGT i_tgt, const uint8_t i_grp )
{
    FAPI_IMP( "tx_zcal_set_grp: I/O EDI+ Xbus Entering" );

    const uint8_t  GRP0             = 0;
    const uint8_t  LN0              = 0;
    const uint32_t DEFAULT_SEGMENTS = 25 * 4; // Nominal 25 Segments 1R * 4 = 4R
    uint32_t       l_pval           = DEFAULT_SEGMENTS;
    uint32_t       l_nval           = DEFAULT_SEGMENTS;
    uint64_t       l_data           = 0;


    FAPI_TRY( io::read( EDIP_TX_IMPCAL_PB, i_tgt, GRP0, LN0, l_data ),
              "tx_zcal_run_sm: Reading Tx Impcal Pb Failed" );

    if( io::get( EDIP_TX_ZCAL_DONE, l_data ) == 1 )
    {
        FAPI_DBG( "Using zCal Results." );

        FAPI_TRY( io::read( EDIP_TX_ZCAL_P, i_tgt, GRP0, LN0, l_data ),
                  "tx_impcal_pval_pb read fail" );

        // We need to convert the 8R value to a 4R equivalent
        l_pval = io::get( EDIP_TX_ZCAL_P, l_data ) / 2;

        FAPI_TRY( io::read( EDIP_TX_ZCAL_N, i_tgt, GRP0, LN0, l_data ),
                  "tx_impcal_nval_pb read fail" );

        // We need to convert the 8R value to a 4R equivalent
        l_nval = io::get( EDIP_TX_ZCAL_N, l_data ) / 2;


        FAPI_TRY( tx_zcal_verify_results( l_pval, l_nval ), "Tx Z Cal Verify Results Failed" );
    }
    else
    {
        FAPI_INF("Warning: P9 IO Xbus Using Default Tx Zcal Segments." );
    }

    // Convert the results of the zCal to actual segments.
    FAPI_TRY( tx_zcal_apply( i_tgt, i_grp, l_pval, l_nval ), "Tx Zcal Apply Segments Failed" );

fapi_try_exit:
    FAPI_IMP( "tx_zcal_set_grp: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Rx Dc Calibration Set Lanes Invalid
 * @param[in] i_tgt    FAPI2 Target
 * @param[in] i_grp    Clock Group
 * @param[in] i_data   Data to Set Lanes Invalid
 * @retval ReturnCode
 */
fapi2::ReturnCode set_lanes_invalid(
    const XBUS_TGT i_tgt,
    const uint8_t  i_grp,
    const uint8_t  i_data )
{
    const uint8_t XBUS_LANES = 17;

    FAPI_IMP( "set_lanes_invalid: I/O EDI+ Xbus Entering" );

    for( uint8_t lane = 0; lane < XBUS_LANES; ++lane )
    {
        FAPI_TRY( io::rmw( EDIP_RX_LANE_INVALID, i_tgt, i_grp, lane, i_data ),
                  "set_lanes_invalid: RMW Invalid Lane Failed" );
    }

fapi_try_exit:
    FAPI_IMP( "set_lanes_invalid: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Start Cleanup Pll
 * @param[in] i_tgt FAPI2 Target
 * @param[in] i_grp Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode start_cleanup_pll( const XBUS_TGT i_tgt, const uint8_t i_grp )
{
    FAPI_IMP( "start_cleanup_pll: I/O EDI+ Xbus Entering" );
    const uint8_t LN0 = 0;
    uint64_t reg_data = 0;

    FAPI_TRY( io::read( EDIP_RX_CTL_CNTL4_E_PG, i_tgt, i_grp, LN0, reg_data ),
              "read edip_rx_ctl_cntl4_e_pg failed" );

    io::set( EDIP_RX_WT_PLL_REFCLKSEL, 0x1, reg_data );
    io::set( EDIP_RX_PLL_REFCLKSEL_SCOM_EN, 0x1, reg_data );

    FAPI_TRY( io::write( EDIP_RX_CTL_CNTL4_E_PG, i_tgt, i_grp, LN0, reg_data ),
              "write edip_rx_ctl_cntl4_e_pg failed" );

    FAPI_TRY( fapi2::delay( 150000, 0 ), " Fapi Delay Failed." );

    FAPI_TRY( io::rmw( EDIP_RX_WT_CU_PLL_PGOOD, i_tgt, i_grp, LN0, 0x1 ),
              "rmw edip_rx_wt_cu_pll_pgood failed" );

    FAPI_TRY( fapi2::delay( 5000, 0 ), " Fapi Delay Failed." );


// The PLL Lock bit is not getting updated.  According to AET it is locked.
#if 0
    FAPI_TRY( io::read( EDIP_RX_WT_CU_BYP_PLL_LOCK, i_tgt, i_group, LANE_00, reg_data ),
              "read edip_rx_wt_cu_byp_pll_lock failed" );

    FAPI_ASSERT( ( io::get( EDIP_RX_WT_CU_BYP_PLL_LOCK, reg_data ) == 1 ),
                 fapi2::IO_XBUS_RX_CLEANUP_PLL_NOT_LOCKED().set_TARGET( i_tgt ).set_GROUP( i_group ),
                 "start_cleanup_pll: Cleanup Pll Not Locking" );
#endif


fapi_try_exit:
    FAPI_IMP( "start_cleanup_pll: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Stop Cleanup Pll
 * @param[in] i_tgt FAPI2 Target
 * @param[in] i_grp Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode stop_cleanup_pll( const XBUS_TGT i_tgt, const uint8_t i_grp )
{
    FAPI_IMP( "stop_cleanup_pll: I/O EDI+ Xbus Entering" );
    const uint8_t LN0 = 0;
    uint64_t reg_data = 0;

    FAPI_TRY( io::read( EDIP_RX_CTL_CNTL4_E_PG, i_tgt, i_grp, LN0, reg_data ),
              "read edip_rx_ctl_cntl4_e_pg failed" );

    io::set( EDIP_RX_WT_PLL_REFCLKSEL,      0, reg_data );
    io::set( EDIP_RX_PLL_REFCLKSEL_SCOM_EN, 0, reg_data );
    io::set( EDIP_RX_WT_CU_PLL_PGOOD,       0, reg_data );

    FAPI_TRY( io::write( EDIP_RX_CTL_CNTL4_E_PG, i_tgt, i_grp, LN0, reg_data ),
              "write edip_rx_ctl_cntl4_e_pg failed" );

    FAPI_TRY( fapi2::delay( 110500, 0 ), " Fapi Delay Failed." );

fapi_try_exit:
    FAPI_IMP( "stop_cleanup_pll: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}

/**
 * @brief Rx Dc Calibration
 * @param[in] i_tgt FAPI2 Target
 * @param[in] i_grp  Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode rx_dccal_start_grp( const XBUS_TGT i_tgt, const uint8_t i_grp )
{
    FAPI_IMP( "rx_dccal_start_grp: I/O EDI+ Xbus Entering" );
    const uint8_t LN0 = 0;

    ///////////////////////////////////////////////////////////////////////////
    /// Simulation Speed Up
    ///////////////////////////////////////////////////////////////////////////
    FAPI_TRY( p9_io_xbus_shorten_timers( i_tgt, i_grp ), "Shorten Timers Failed." );

    ////////////////////////////////////////////////////////////////////////////
    /// Start Rx Dccal
    ////////////////////////////////////////////////////////////////////////////

    // Must set lane invalid bit to 0 to run rx dccal, this enables us to run
    //   dccal on the specified lane.  These bits are normally set by wiretest
    //   although we are not running that now.
    FAPI_TRY( set_lanes_invalid( i_tgt, i_grp, 0 ),
              "Error Setting Lane Invalid to 0" );

    // We must start the cleanup pll to have a clock that clocks the
    //  dccal logic.  This function locks the rx cleanup pll to the internal
    //  grid clock reference.
    FAPI_TRY( start_cleanup_pll( i_tgt, i_grp ), "rx_dc_cal: Starting Cleanup Pll" );

    // Clear the rx dccal done bit in case rx dccal was previously run.
    FAPI_TRY( io::rmw( EDIP_RX_DC_CALIBRATE_DONE, i_tgt, i_grp, LN0, 0),
              "RMW Dccal Done Failed" );

    // Start DC Calibrate, this iniates the rx dccal state machine
    FAPI_TRY( io::rmw( EDIP_RX_START_DC_CALIBRATE, i_tgt, i_grp, LN0, 1 ),
              "RMW Start Dccal Failed" );

    FAPI_DBG( "I/O EDI+ Xbus Rx Dccal Complete on Group(%d)", i_grp );


fapi_try_exit:
    FAPI_IMP( "rx_dccal_start_grp: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}





/**
 * @brief Rx Dc Calibration
 * @param[in] i_tgt FAPI2 Target
 * @param[in] i_grp Clock Group
 * @retval ReturnCode
 */
fapi2::ReturnCode rx_dccal_poll_grp( const XBUS_TGT i_tgt, const uint8_t  i_grp )
{
    FAPI_IMP( "rx_dccal_poll_grp: I/O EDI+ Xbus Entering" );
    const uint8_t  TIMEOUT           = 200;
    const uint64_t DLY_100MS         = 100000000;
    const uint64_t DLY_10MS          = 10000000;
    const uint64_t DLY_1MIL_CYCLES   = 1000000;
    const uint8_t  LN0               = 0;
    uint8_t        l_poll_count      = 0;
    uint64_t       l_data            = 0;

    ////////////////////////////////////////////////////////////////////////////
    /// Poll Rx Dccal
    ////////////////////////////////////////////////////////////////////////////
    // In the pervasive unit model, this takes 750,000,000 sim cycles to finish
    //   on a group.  This equates to 30 loops with 25,000,000 delay each.

    // Delay before we start polling.  100ms was use from past p8 learning
    FAPI_TRY( fapi2::delay( DLY_100MS, DLY_1MIL_CYCLES ),
              "rx_dc_cal_poll: Fapi Delay Failed." );

    do
    {
        FAPI_DBG( "I/O EDI+ Xbus Rx Dccal Polling Count(%d/%d).", l_poll_count, TIMEOUT );

        FAPI_TRY( fapi2::delay( DLY_10MS, DLY_1MIL_CYCLES ), "Fapi Delay Failed." );

        FAPI_TRY( io::read( EDIP_RX_DC_CALIBRATE_DONE, i_tgt, i_grp, LN0, l_data ),
                  "Read Dccal Done Failed" );
    }
    while( ( ++l_poll_count < TIMEOUT ) && !io::get( EDIP_RX_DC_CALIBRATE_DONE, l_data ) );

    FAPI_ASSERT( ( io::get( EDIP_RX_DC_CALIBRATE_DONE, l_data ) == 1 ),
                 fapi2::IO_XBUS_RX_DCCAL_TIMEOUT().set_TARGET( i_tgt ).set_GROUP( i_grp ),
                 "Rx Dccal Timeout: Loops(%d) delay(%d ns, %d cycles)",
                 l_poll_count, DLY_10MS, DLY_1MIL_CYCLES );

    FAPI_DBG( "I/O EDI+ Xbus Rx Dccal Successful." );

    ////////////////////////////////////////////////////////////////////////////
    /// Cleanup Rx Dccal
    ////////////////////////////////////////////////////////////////////////////

    // Stop DC Calibrate
    FAPI_TRY( io::rmw( EDIP_RX_START_DC_CALIBRATE, i_tgt, i_grp, LN0, 0 ),
              "Stopping Dccal Failed" );

    // We must stop the cleanup pll to cleanup after ourselves.  Wiretest will
    //  turn this back on.  This function will turn off the cleanup pll and
    //   switch it back to bus clock reference.
    FAPI_TRY( stop_cleanup_pll( i_tgt, i_grp ), "rx_dc_cal: Stopping Cleanup Pll" );

    // Restore the invalid bits, Wiretest will modify these as training is run.
    FAPI_TRY( set_lanes_invalid( i_tgt, i_grp, 1 ), "Error Setting Lane Invalid to 1" );

    FAPI_DBG( "I/O EDI+ Xbus Rx Dccal Complete on Group(%d)", i_grp );


fapi_try_exit:
    FAPI_IMP( "rx_dccal_poll_grp: I/O EDI+ Xbus Exiting" );
    return fapi2::current_err;
}
