/*
 * rtcclock.c
 *
 *  Created on: Nov 19, 2017
 *      Author: spiri
 */

#include "rtcclock.h"
#include <msp430.h>
#include <stdlib.h>


// The use of unsigned 32-bit timestamps supports 136-year operation, from 1970 to 2106.
// Hopefully we're not still using tiny 16-bit MCUs by then...

// Statically-initialized variable
#ifdef __TI_COMPILER_VERSION__
#pragma PERSISTENT(rtcclock_current)
uint32_t rtcclock_current = 0;
#elif __IAR_SYSTEMS_ICC__
__persistent uint32_t rtcclock_current = 0;
#elif defined(__GNUC__)
uint32_t rtcclock_current __attribute__((section(".text"))) = 0;
#endif

/* Database of current alarms */
rtclock_alarm_t * rtcAlarms[RTC_MAX_ALARM_COUNT];

// Uses XT1 and expects it to already be initialized
void RTClock_init_using_XT1CLK(uint32_t curclk)
{
    if (curclk != 0) {
        // Write curclk to rtcclock_current
        SYSCFG0 = FRWPPW | DFWP;
        rtcclock_current = curclk;
        SYSCFG0 = FRWPPW | PFWP | DFWP;
    }
    // Using XT1
    RTCMOD = 32 - 1;
    // Clear RTCIFG if set
    uint16_t iv = RTCIV;
    // Source from XT1 dividing by 1024, so RTC count of 32 yields 1 second per tick
    RTCCTL = RTCSS__XT1CLK | RTCSR | RTCPS__1024 | RTCIE;
}

// Use SMCLK for those without a soldered XT1 who go no lower than LPM0
void RTClock_init_using_SMCLK(uint32_t curclk, uint32_t smclk_freq)
{
    if (curclk != 0) {
        // Write curclk to rtcclock_current
        SYSCFG0 = FRWPPW | DFWP;
        rtcclock_current = curclk;
        SYSCFG0 = FRWPPW | PFWP | DFWP;
    }
    // Using SMCLK at caller-specified frequency
    RTCMOD = (uint16_t)(smclk_freq / 1024) - 1;
    // Clear RTCIFG if set
    uint16_t iv = RTCIV;
    // Source from SMCLK dividing by 1024, RTCMOD computed to tick once per second
    RTCCTL = RTCSS__SMCLK | RTCSR | RTCPS__1024 | RTCIE;
}

// Use VLOCLK for those without a soldered XT1 who don't care about accuracy
void RTClock_init_using_VLOCLK(uint32_t curclk)
{
    if (curclk != 0) {
        // Write curclk to rtcclock_current
        SYSCFG0 = FRWPPW | DFWP;
        rtcclock_current = curclk;
        SYSCFG0 = FRWPPW | PFWP | DFWP;
    }
    // Using VLOCLK assumed to be 10KHz
    RTCMOD = 100 - 1;
    // Clear RTCIFG if set
    uint16_t iv = RTCIV;
    // Source from VLOCLK dividing by 1000, so RTC count of 100 yields 1 second per tick
    RTCCTL = RTCSS__SMCLK | RTCSR | RTCPS__100 | RTCIE;

}

// RTC interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=RTC_VECTOR
__interrupt void RTC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(RTC_VECTOR))) RTC_ISR (void)
#else
#error Compiler not supported!
#endif
{
    uint16_t iv = RTCIV; // Clears RTCIFG

    //P1OUT ^= BIT0;
    uint16_t frwp = SYSCFG0 & (DFWP | PFWP); // Save SYSCFG0 so we restore it before exiting

    SYSCFG0 = FRWPPW;
    rtcclock_current++;
    unsigned int i;
    for (i=RTC_MAX_ALARM_COUNT; i > 0; i--) {
        if (rtcAlarms[i-1] != (void *)0) {
            if (rtcAlarms[i-1]->timestamp == rtcclock_current) {
                // We can support FRAM-based rtc_alarm_t struct pointers b/c FRAM WP is disabled earlier in this ISR.
                rtcAlarms[i-1]->triggered = true;
                __bic_SR_register_on_exit(LPM4_bits);
            }
        }
    }
    SYSCFG0 = FRWPPW | frwp; // Restore SYSCFG0 to pre-interrupt state
}

/* The bulk of this codebase is the interpretation functionality. */
int RTClock_compare(uint32_t tstmp)
{
    uint32_t tmpstamp = rtcclock_current; // Save current timestamp locally in case it changes mid-function

    if (tstmp > tmpstamp) {
        return 1;
    }
    if (tstmp < tmpstamp) {
        return -1;
    }
    return 0; // Exact same time!
}

void RTClock_get(uint32_t * buf)
{
    if (buf == (void *)0) {
        return;
    }

    *buf = rtcclock_current;
}

bool RTClock_setAlarm(rtclock_alarm_t * a)
{
    unsigned int i;

    for (i=RTC_MAX_ALARM_COUNT; i > 0; i--) {
        if (rtcAlarms[i-1] == (void *)0) {
            a->triggered = false;
            rtcAlarms[i-1] = a;
            return true; // Found an open slot for this alarm
        }
    }
    return false; // Ran out of slots to put this in
}

bool RTClock_clearAlarm(rtclock_alarm_t * a)
{
    unsigned int i;

    for (i=RTC_MAX_ALARM_COUNT; i > 0; i--) {
        if (rtcAlarms[i-1] == a) {
            rtcAlarms[i-1] = (void *)0;
            return true; // Found & cleared the alarm
        }
    }
    return false; // Didn't find it
}
