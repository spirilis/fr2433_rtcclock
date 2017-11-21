/* --COPYRIGHT--,BSD_EX
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************
 *
 *                       MSP430 CODE EXAMPLE DISCLAIMER
 *
 * MSP430 code examples are self-contained low-level programs that typically
 * demonstrate a single peripheral function or device feature in a highly
 * concise manner. For this the code may rely on the device's power-on default
 * register values and settings such as the clock configuration and care must
 * be taken when combining code from several examples to avoid potential side
 * effects. Also see www.ti.com/grace for a GUI- and www.ti.com/msp430ware
 * for an API functional library-approach to peripheral configuration.
 *
 * --/COPYRIGHT--*/

/* FR2433_RTCCLOCK demonstration
 *
 * Author: spirilis
 *
 * Description: This sets the rtcclock library's counter to a prespecified timestamp, then enables an Alarm entry
 *              while actively monitoring its trigger, keeping the chip in LPM0 between, and advancing the alarm
 *              time to blink the green LED (P1.1 on the FR2433 LaunchPad).
 *
 * Gotchas:     The rtcclock library modifies each rtcclock_alarm_t entry's "triggered" field, however, on the FR2433
 *              which has FRAM Write Protection, if the rtcclock_alarm_t pointer happens to point to FRAM, this will
 *              cause a write violation since the library does not disable FRAM write protection.  I may add a SYSCFG0
 *              write-protect-disable around that soon just to support persistent alarms.
 *
 */

#include <msp430.h>
#include "driverlib.h"
#include "rtcclock.h"

#pragma PERSISTENT(has_init_rtc)
bool has_init_rtc = false;
const uint32_t rtc_initial_load_timestamp = 1511296859;

void main (void)
{
    WDTCTL = WDTPW | WDTHOLD;

    P1DIR = 0;
    P1REN = 0xFF;
    P1OUT = 0;
    P2DIR = 0;
    P2REN = 0xFF;
    P2OUT = 0;
    P3DIR = 0;
    P3REN = 0xFF;
    P3OUT = 0;

    P1DIR |= BIT0|BIT1;
    P1OUT &= ~(BIT0|BIT1);
    // XT1 support
    P2SEL1 &= ~(BIT0 | BIT1);
    P2SEL0 |= BIT0|BIT1;

    PMM_unlockLPM5();

    FRAMCtl_configureWaitStateControl(FRAMCTL_ACCESS_TIME_CYCLES_1);

    CS_setExternalClockSource(32768);
    bool xt1Ret;
    xt1Ret = CS_turnOnXT1LFWithTimeout(CS_XT1_DRIVE_3, 65535);
    if (xt1Ret) {
        CS_initClockSignal(CS_FLLREF, CS_XT1CLK_SELECT, CS_CLOCK_DIVIDER_1); // DCO FLL ref = XT1 (32.768KHz physical crystal)
    } else {
        CS_initClockSignal(CS_FLLREF, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_1); // DCO FLL ref = REFO (internal RC oscillator 32.768KHz reference)
    }
    bool fllRet;
    fllRet = CS_initFLLSettle(16000, 488); // 488 is 16000000 / 32768
    if (!fllRet) {
        P1OUT |= BIT0;
        LPM4;
    }
    CS_initClockSignal(CS_MCLK, CS_DCOCLKDIV_SELECT, CS_CLOCK_DIVIDER_1); // Activate MCLK = DCO w/ FLL
    __enable_interrupt();

    // RTClock library init
    if (!has_init_rtc) {
        RTClock_init_using_XT1CLK(rtc_initial_load_timestamp);
        SYSCFG0 = FRWPPW | DFWP; // FR2433 FRAM write protection disable
        has_init_rtc = true;
        SYSCFG0 = FRWPPW | PFWP | DFWP; // FR2433 FRAM write protection re-enable
    } else {
        RTClock_init_using_XT1CLK(0); // 0 means, leave the FRAM-based internal RTC counter alone during init, counting from where the MCU left off last time
    }

    // Setting an alarm for 60 seconds after the initial timestamp
    // Note that if you reset the chip more than 60 seconds after loading the code, none of this should work again!  The testAlarm does not
    // live in FRAM.
    rtclock_alarm_t testAlarm;
    testAlarm.timestamp = rtc_initial_load_timestamp + 60;
    RTClock_setAlarm(&testAlarm); // RTClock library stores a pointer to your alarm and checks it during the RTC interrupt.

    while (1) {
        LPM0;
        if (testAlarm.triggered == true) {
            testAlarm.triggered = false;
            testAlarm.timestamp += 10;
            P1OUT ^= BIT1;
        }
    }
}
