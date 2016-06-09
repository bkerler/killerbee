// This file has been prepared for Doxygen automatic documentation generation.
/*! \file *********************************************************************
 *
 * \brief  This file implements the API for the reactive jammer.
 *
 * \defgroup applReactiveJammer Reactive Jammer
 * \ingroup applRzUsbStick
 *
 *      TODO: DESCRIPTION
 *
 * \par Application note:
 *      AVR2017: RZRAVEN FW
 *
 * \author
 *      Spencer Michaels
 *      (based on Atmel's AirCapture code)
 *
 *****************************************************************************/
#ifndef REACTIVE_JAMMER_H
#define REACTIVE_JAMMER_H

/*================================= INCLUDES         =========================*/
#include <stdint.h>
#include <stdbool.h>
//! \addtogroup applReactiveJammer
//! @{
/*================================= MACROS           =========================*/
#define RJ_MIN_CHANNEL (11) //!< Lowest supported IEEE 802.15.4 channel.
#define RJ_MAX_CHANNEL (26) //!< Highest supported IEEE 802.15.4 channel.

#define RJ_MAX_FRAME_SIZE (1 + 127 + 1) //!< Configured for IEEE 802.15.4 frames: length + PSDU + LQI.
/*================================= GLOBAL VARIABLES =========================*/
/*================================= LOCAL VARIABLES  =========================*/
/*================================= PROTOTYPES       =========================*/

/*! \brief This function must be called to initialize the reactive jammer.
 *
 *  \retval true The reactive jammer has been initialized and is ready for
 *               use.
 *  \retval false The reactive jammer could not be started; no other
 *                function in this API should be called.
 *
 *  \ingroup applReactiveJammer
 */
bool reactive_jammer_init(void);

/*! \brief This function is called to disable the reactive jammer.
 *
 *         No other functions in the API should be called after this function
 *         has been executed.
 *
 *  \ingroup reactive_jammer
 */
void reactive_jammer_deinit(void);

/*! \brief This function will set new channel for the radio transceiver to work on.
 *
 *  \param channel New channel for the radio transceiver to operate on.
 *
 *  \retval true Channel was changed successfully.
 *  \retval false Channel could not be set.
 *
 *  \ingroup reactive_jammer
 */
bool reactive_jammer_set_channel(uint8_t channel);

/*! \brief This function will enable jamming.
 *
 *  \retval true Jammer started successfully.
 *  \retval false Jammer could not be started.
 *
 *  \ingroup reactive_jammer
 */
bool reactive_jammer_on(void);

/*! \brief This function will disable jamming.
 *
 *  \ingroup reactive_jammer
 */
void reactive_jammer_off(void);

//! @}
#endif
/*EOF*/
