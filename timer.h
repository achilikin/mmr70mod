/* Simple timer routine for ATmega32 on MMR-70
   Counts milliseconds and separately tenth of a second

   Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _TIMER_MILLIS_H_
#define _TIMER_MILLIS_H_

#include <avr/interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint8_t  ms_clock;     // up to 100 ms
extern volatile uint8_t  tenth_clock;  // increments every 1/10 of a second
extern volatile uint32_t millis_clock; // global millis counter

void init_time_clock(void);

static inline uint32_t millis(void)
{
	cli();
	uint32_t mil = millis_clock;
	sei();
	return mil;
}

static inline uint16_t mill16(void)
{
	cli();
	uint16_t mil = (uint16_t)millis_clock;
	sei();
	return mil;
}

static inline uint8_t mill8(void)
{
	uint8_t mil = ms_clock;
	return mil;
}

#ifdef __cplusplus
}
#endif

#endif

