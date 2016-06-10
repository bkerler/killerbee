// This file has been prepared for Doxygen automatic documentation generation.
/*! \file *********************************************************************
 *
 * \brief  This file implements the reactive jammer
 *
 * \par Application note:
 *      AVR2017: RZRAVEN FW
 *
 * \author
 *      Spencer Michaels
 *      (based on Atmel's AirCapture code)
 *
 *****************************************************************************/

/*================================= INCLUDES         =========================*/
#include <stdint.h>
#include <stdbool.h>

#include "vrt_mem.h"
#include "vrt_timer.h"
#include "led.h"
#include "at86rf230_registermap.h"
#include "rf230.h"
#include "usb_descriptors.h"
#include "usb_task.h"
#include "rzusbstickcommon.hh"
#include "reactive_jammer.h"

//! \addtogroup applAirCapture
//! @{
/*================================= MACROS           =========================*/
/* State definitions for the reactive jammer. These are used in this file only.
 */
#define RJ_NOT_INITIALIZED   (0x01) //!< The reactive_jammer_init function has not been called yet.
#define RJ_IDLE              (0x02) //!< The reactive jammer initialized and ready.
#define RJ_BUSY_LISTENING    (0x04) //!< The reactive jammer is busy listening.
#define RJ_BUSY_JAMMING      (0x08) //!< The reactive jammer is busy jamming.
#define RJ_BUSY_TRANSMITTING (0x10) //!< The reactive jammer is busy transmitting.

#define RJ_TICK_PER_US       (2)   //!< Number of ticks per microsecond.

// TODO: RX_START only?
#define RJ_SUPPORTED_INTERRUPT_MASK  (0x0C) //!< Only interrested in RX_START and TRX_END interrupts.
/*================================= TYEPDEFS         =========================*/
/*================================= GLOBAL VARIABLES =========================*/
/*================================= LOCAL VARIABLES  =========================*/
/*! \brief Variable holding the reactive jammer's internal state. */
static uint8_t rj_state = RJ_NOT_INITIALIZED;

static uint8_t rj_rssi;
static uint8_t rj_unknown_isr; //!< Incremented each time an unknown interrupt event is received.

/*! \brief Frame with randomized data. Used by the jammer. */
static uint8_t jammer_frame_length = 127;
const PROGMEM_DECLARE(static uint8_t jammer_frame[127]) = {                 \
                        186,38,120,91,206,116,184,22,42,239,243,204,139,78, \
                        83,10,226,215,183,60,86,76,181,102,219,30,87,238,   \
                        230,244,67,26,6,223,205,159,134,62,138,121,58,4,9,  \
                        124,31,187,18,160,119,155,64,252,0,173,49,111,154,  \
                        166,158,21,13,108,68,112,53,240,100,214,126,72,61,  \
                        80,98,47,198,48,231,96,248,220,92,95,8,195,185,19,  \
                        168,190,233,122,129,101,188,210,46,85,229,144,247,  \
                        167,123,194,193,234,74,174,147,242,255,179,197,103, \
                        57,152,73,5,44,63,56,141,211,202,45,224,178,0,0};
/*================================= PROTOTYPES       =========================*/
static bool init_rf(void);
static void jamming_listen_callback(uint8_t isr_event);
static void transmission_callback(uint8_t isr_event);
static bool send_jamming_frame(void);
static bool jamming_listen_enable();
static bool jamming_listen_disable();
static void wait_for_rx_start(void);
static void wait_for_state_idle(void);
static void read_frame_to_buf(uint8_t* dst_buf, const uint8_t len);
static void reactive_jammer(void);

/*! \brief This function is used to initialize the RF230 radio transceiver to be
 *         used for capturing/jamming.
 *
 *  \retval true The radio transceiver was started successfully.
 *  \retval false The radio transceiver could not be started.
 */
static bool init_rf(void) {
    (bool)rf230_init();

    delay_us(TIME_TO_ENTER_P_ON);

    rf230_set_tst_low();
    rf230_set_rst_low();
    rf230_set_slptr_low();
    delay_us(TIME_RESET);
    rf230_set_rst_high();

    /* Could be that we were sleeping before we got here. */
    delay_us(TIME_SLEEP_TO_TRX_OFF);

    /* Could be that we were sleeping before we got here. */
    delay_us(TIME_SLEEP_TO_TRX_OFF);

    /* Force transition to TRX_OFF and verify. */
    rf230_subregister_write(SR_TRX_CMD, CMD_FORCE_TRX_OFF);
    delay_us(TIME_P_ON_TO_TRX_OFF);

    bool rf230_init_status = false;
    if (TRX_OFF != rf230_subregister_read(SR_TRX_STATUS)) {
    } else {

        /* Enable automatic CRC generation and set the ISR mask. */
        rf230_subregister_write(SR_CLKM_SHA_SEL, 0);
        rf230_subregister_write(SR_CLKM_CTRL, 0);
        rf230_subregister_write(SR_TX_AUTO_CRC_ON, 1);
        rf230_register_write(RG_IRQ_MASK, RJ_SUPPORTED_INTERRUPT_MASK);

        RF230_ENABLE_TRX_ISR();

        rf230_init_status = true;
    }

    return rf230_init_status;
}


bool reactive_jammer_init(void) {
    /* Initialize local variables. */
    rj_unknown_isr = 0;
    rj_rssi = 0;

    if (true == init_rf()) {
        rj_state = RJ_IDLE;
        return true;
    } else {
        /* Disable the radio transceiver. */
        rf230_deinit();
        return false;
    }
}


void reactive_jammer_deinit(void) {
    /* Ensure that the reactive jammer has been running and memory allocated
     * before it can be teared down.
     */
    if (RJ_NOT_INITIALIZED != rj_state) {
        /* Deinit the radio transceiver and set the internal status variable to
         * reflect the new state.
         */
        rf230_deinit();
        rj_state = RJ_NOT_INITIALIZED;
        LED_GREEN_ON();
    }
}

/* This function will set new channel for the radio transceiver to work on. */
bool reactive_jammer_set_channel(uint8_t channel) {
    /* Perform sanity checks to see if it is  possible to run the function. */
    if (RJ_IDLE != rj_state) { return false; }
    if ((channel < RJ_MIN_CHANNEL) || (channel > RJ_MAX_CHANNEL)) { return false; }

    /* Fix for timing issue with setting channel immediately after setting CMD_MODE_RJ - JLW */
    /* Could be that we were sleeping before we got here. */
    delay_us(TIME_SLEEP_TO_TRX_OFF);

    /* Force TRX_OFF mode and wait for transition to complete. */
    rf230_subregister_write(SR_TRX_CMD, CMD_FORCE_TRX_OFF);
    delay_us(TIME_P_ON_TO_TRX_OFF);

    /* Set channel and verify. */
    bool ac_set_channel_status = false;
    if (TRX_OFF == rf230_subregister_read(SR_TRX_STATUS)) {
        /* Set the new channel and verify. */
        rf230_subregister_write(SR_CHANNEL, channel);
        if (channel == rf230_subregister_read(SR_CHANNEL)) {
            ac_set_channel_status = true;
        }
    }

    return ac_set_channel_status;
}

static void reactive_jammer(void) {
    while (true) {
        // Listen for incoming packets
        jamming_listen_enable();

        // Wait until a packet is received
        // Will flash orange while waiting
        wait_for_rx_start();

        // Read the first few bytes of the frame
        static uint8_t FRAME_READ_LEN = 4;
        uint8_t* buf = (uint8_t*)malloc(sizeof(uint8_t) * FRAME_READ_LEN);
        read_frame_to_buf(buf, FRAME_READ_LEN);

        // Stop listening (to prepare for transmission)
        jamming_listen_disable();

        // TODO: Decide whether or not to jam
        if (true) {
            // Send a jamming frame
            LED_ORANGE_ON();
            send_jamming_frame();
            LED_ORANGE_OFF();
        }
        LED_GREEN_ON();
    }
}

/* This function will enable jamming. */
bool reactive_jammer_on(void) {
    if (RJ_IDLE != rj_state) return false;

    LED_RED_ON();
    jamming_listen_enable();

    return true;
}

/* This function will disable jamming.
 *
 *  \ingroup reactive_jammer
 */
void reactive_jammer_off(void) {
    jamming_listen_disable();
    LED_RED_OFF();
}

static void read_frame_to_buf(uint8_t* dst_buf, const uint8_t len) {
    // "Each access starts by setting ~SEL = L."
    RF230_SS_LOW();

    // "The first byte transferred on MOSI is the command byte and must indicate
    // a Frame Buffer access mode."
    SPDR = RF230_TRX_CMD_FR;
    RF230_WAIT_FOR_SPI_TX_COMPLETE();

    // Get the length of the incoming frame
    uint8_t frame_length = SPDR;

    // TODO: Ensure no buffer overrun
    //assert(frame_length <= len);

    // Request `len` bytes
    // TODO: Can length alone tell us if this is the right kind of packet?
    SPDR = len;
    RF230_WAIT_FOR_SPI_TX_COMPLETE();

    for (int i = 0; i < len; ++i) {
        const uint8_t byte = SPDR;
        SPDR = byte; // Stall for time

        // TODO: Do something with the byte
        dst_buf[i] = byte;
        RF230_WAIT_FOR_SPI_TX_COMPLETE();
    }

    RF230_SS_HIGH();
}

static bool send_jamming_frame(void) {

    /* Check that the reactive jammer is initialized and not busy. */
    if (RJ_IDLE != rj_state) { return false; }

    /* Check that the radio transceiver is in TRX_OFF. */
    if (TRX_OFF != rf230_subregister_read(SR_TRX_STATUS)) { return false; }

    /* Go to PLL_ON and send the frame. */
    rf230_subregister_write(SR_TRX_CMD, CMD_PLL_ON);
    delay_us(TIME_TRX_OFF_TO_PLL_ACTIVE);

    bool send_status = false;

    /* Verify that the PLL_ON state was entered. */
    if (PLL_ON != rf230_subregister_read(SR_TRX_STATUS)) {
    } else {
        rf230_set_callback_handler(jamming_transmission_callback);

        /* Send frame with pin start. */
        rf230_set_slptr_high();
        rf230_set_slptr_low();
        rf230_frame_write(jammer_frame_length, jammer_frame);

        /* Update state information. */
        rj_state = RJ_BUSY_TRANSMITTING;
        send_status = true;
    }

    return send_status;
}

static bool jamming_listen_enable() {
    if (RJ_IDLE != rj_state) { return false; }

    /* Initialize the frame pool in the RF230 device driver and set the radio
     * transceiver in receive mode.
     */
    rf230_subregister_write(SR_TRX_CMD, CMD_FORCE_TRX_OFF);
    delay_us(TIME_P_ON_TO_TRX_OFF);

    bool open_stream_status = false;
    if (TRX_OFF == rf230_subregister_read(SR_TRX_STATUS)) {
        /* Do transition from TRX_OFF to RF_ON. */
        rf230_subregister_write(SR_TRX_CMD, RX_ON);
        delay_us(TIME_TRX_OFF_TO_PLL_ACTIVE);

        /* Verify that the state transition to RX_ON was successful. */
        if (RX_ON == rf230_subregister_read(SR_TRX_STATUS)) {
            /* Set callback for captured frames. */
            rf230_set_callback_handler(jamming_listen_callback);
            rj_state = RJ_BUSY_LISTENING;
            open_stream_status = true;
        }
    }

    return open_stream_status;
}

static bool jamming_listen_disable() {
    /* Perform sanity checks to see if it is  possible to run the function. */
    if (RJ_BUSY_LISTENING != rj_state) { return false; }

    /* Close stream. */
    rf230_clear_callback_handler();
    rf230_subregister_write(SR_TRX_CMD, CMD_FORCE_TRX_OFF);
    delay_us(TIME_P_ON_TO_TRX_OFF);

    /* Verify that the TRX_OFF state was entered. */
    bool close_stream_status = false;
    if (TRX_OFF == rf230_subregister_read(SR_TRX_STATUS)) {
        rj_state = RJ_IDLE;
        close_stream_status = true;
    }

    return close_stream_status;
}

/*! \brief This is an internal callback function that is used to handle
 *         listening for packet receipt.
 *
 *  \param[in] isr_event Event signaled by the radio transceiver.
 */
static void jamming_listen_callback(uint8_t isr_event) {
    if (RF230_RX_START_MASK == (isr_event & RF230_RX_START_MASK)) {
        // Record the RSSI and timestamp when we pick up a packet
        //uint32_t time_stamp = vrt_timer_get_tick_cnt() / RJ_TICK_PER_US;
        RF230_QUICK_SUBREGISTER_READ(0x06, 0x1F, 0, rj_rssi);

        // TODO: Do something with these bytes
        // Read the first few bytes of the frame
        static uint8_t FRAME_READ_LEN = 4;
        uint8_t* buf = (uint8_t*)malloc(sizeof(uint8_t) * FRAME_READ_LEN);
        read_frame_to_buf(buf, FRAME_READ_LEN);

        // Stop listening (to prepare for transmission)
        jamming_listen_disable();

        // TODO: Decide whether or not to jam
        if (true) {
            // Send a jamming frame
            LED_ORANGE_ON();
            send_jamming_frame();
            LED_ORANGE_OFF();
        }
    } else {
        rj_unknown_isr++;
    }
}

static void jamming_transmission_callback(uint8_t isr_event) {
    if (RF230_TRX_END_MASK == (isr_event & RF230_TRX_END_MASK)) {
        RF230_QUICK_CLEAR_ISR_CALLBACK();

        /* Force radio transceiver to TRX_OFF mode and set state to RJ_IDLE. */
        RF230_QUICK_SUBREGISTER_WRITE(0x02, 0x1f, 0, CMD_FORCE_TRX_OFF);
        delay_us(TIME_CMD_FORCE_TRX_OFF);
        rj_state = RJ_IDLE;

        if (rj_should_continue_jamming) {
            jamming_listen_enable();
        }
    }
}

static inline void wait_for_state_idle(void) {
    while (rj_state != RJ_IDLE) {
        delay_us(10); // TODO
    };
}

//! @}
/*EOF*/
