/*
 * rtcclock.h
 *
 *  Created on: Nov 19, 2017
 *      Author: spiri
 */

#ifndef RTCCLOCK_H_
#define RTCCLOCK_H_

#include <stdint.h>
#include <stdbool.h>

/* Day of week */
typedef enum {
    SUNDAY=0,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY
} rtclock_dayofweek_t;

/* Month/Day/Year Hour:Minute:Second data */
typedef struct {
    rtclock_dayofweek_t dayofweek;
    uint8_t month;
    uint8_t day;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtclock_date_t;

/* Alarms */
typedef struct {
    // All declared volatile because the usermode can modify these on the fly, and the ISR reads them every time.
    volatile uint32_t timestamp;
    volatile bool triggered;
} rtclock_alarm_t;

#define RTC_MAX_ALARM_COUNT 3

/* Function prototypes */
void RTClock_init_using_XT1CLK(uint32_t);  // Supply optional "current rtc clock" setting (if 0, it pulls from internal FRAM)
void RTClock_init_using_SMCLK(uint32_t, uint32_t smclk_freq); // Configure RTC using SMCLK, must specify frequency
void RTClock_init_using_VLOCLK(uint32_t); // Configure RTC using 10KHz VLOCLK, for when you can deal with poor accuracy and no XT1
int RTClock_compare(uint32_t); // Compare specified date yielding -1 if it came before, 0 if same time, 1 if came after present time.
void RTClock_get(uint32_t *);  // Caller supplies a pointer to a 32-bit unsigned integer.
bool RTClock_setAlarm(rtclock_alarm_t *);
bool RTClock_clearAlarm(rtclock_alarm_t *);

#endif /* RTCCLOCK_H_ */
