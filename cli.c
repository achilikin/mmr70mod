/* Simple command line parser for ATmega32 on MMR-70

   Copyright (c) 2014 Andrey Chilikin (https://github.com/achilikin)

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
#include <stdio.h>
#include <string.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "cli.h"
#include "rht.h"
#include "sht10.h"
#include "ns741.h"
#include "timer.h"
#include "serial.h"
#include "ossd_i2c.h"
#include "mmr70pin.h"

static uint16_t free_mem(void)
{
	extern int __heap_start, *__brkval; 
	unsigned val;
	val = (unsigned)((int)&val - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
	return val;
}

inline int8_t str_is(const char *cmd, const char *str)
{
	return strcmp_P(cmd, str) == 0;
}

inline const char *is_on(uint8_t val)
{
	if (val) return "ON";
	return "OFF";
}

// list of supported commands 
const char cmd_list[] PROGMEM = 
	"  reset\n"
	"  status\n"
	"  mem\n"
	"  poll\n"
	"  debug rht|adc|rds|off\n"
	"  adc chan\n"
	"  rdsid id\n"
	"  rdstext text\n"
	"  freq nnnn\n"
	"  txpwr 0-3\n"
	"  volume 0-6\n"
	"  mute on|off\n"
	"  stereo on|off\n"
	"  radio on|off\n"
	"  gain low|off\n"
	"  calibrate\n";

static int8_t process(char *buf, void *rht)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	memcpy(cmd, buf, sizeof(cmd));

	for(arg = cmd; *arg && *arg != ' '; arg++);
	if (*arg == ' ') {
		*arg = '\0';
		arg++;
	}

	if (str_is(cmd, PSTR("help"))) {
		uart_puts_p(cmd_list);
		return 0;
	}

	if (str_is(cmd, PSTR("reset"))) {
		puts_P(PSTR("\nresetting..."));
		wdt_enable(WDTO_15MS);
		while(1);
	}
	if (str_is(cmd, PSTR("calibrate"))) {
		puts_P(PSTR("\ncalibrating..."));
		if (str_is(arg, PSTR("default")))
			serial_calibrate(osccal_def);
		else
			serial_calibrate(OSCCAL);
		return 0;
	}

	if (str_is(cmd, PSTR("poll"))) {
		puts_P(PSTR("polling..."));
		rht_read(rht, RHT03_ECHO);
		ns741_rds_set_radiotext(rds_data);
		return 0;
	}

	if (str_is(cmd, PSTR("debug"))) {
		if (str_is(arg, PSTR("rht"))) {
			debug_flags ^= RHT03_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(debug_flags & RHT03_ECHO));
			return 0;
		}
#if ADC_MASK
		if (str_is(arg, PSTR("adc"))) {
			debug_flags ^= ADC_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(rt_flags & ADC_ECHO));
			return 0;
		}
#endif
		if (str_is(arg, PSTR("off"))) {
			debug_flags = 0;
			printf_P(PSTR("echo OFF\n"));
			return 0;
		}
		if (str_is(arg, PSTR("rds"))) {
			ns741_rds_debug(1);
			return 0;
		}
		return -1;
	}
	if (str_is(cmd, PSTR("mem"))) {
		printf_P(PSTR("memory %d\n"), free_mem());
		return 0;
	}
#if ADC_MASK
	if (str_is(cmd, PSTR("adc"))) {
		// put names for your analogue inputs here
		const char *name[4] = { "Vcc", "GND", "Light", "R"};
		uint8_t ai = atoi((const char *)arg);
		if (ai > 7 || (ADC_MASK & (1 << ai)) == 0) {
			printf_P(PSTR("Invalid Analog input, use"));
			for(uint8_t i = 0; i < 8; i++) {
				if (ADC_MASK & (1 << i))
					printf(" %d", i);
			}
			printf("\n");
			return -1;
		}
		uint16_t val = analogRead(ai);
		uint32_t v = val * 323LL + 500LL;
		uint32_t dec = (v % 100000LL) / 1000LL;
		v = v / 100000LL;
		printf_P(PSTR("ADC %d %4d %d.%dV (%s)\n"), ai, val, (int)v, (int)dec, name[ai-4]);
		// if you want to use floats then do not forget to uncomment in Makefile 
		// #PRINTF_LIB = $(PRINTF_LIB_FLOAT)
		// to enable float point printing
		//double v = val*3.3/1024.0;
		//printf("ADC %d %4d %.2fV (%s)\n", ai, val, v, name[ai-4]);
		return 0;
	}
#endif

	if (str_is(cmd, PSTR("rdsid"))) {
		if (*arg != '\0') {
			memset(ns741_name, 0, 8);
			for(int8_t i = 0; (i < 8) && arg[i]; i++)
				ns741_name[i] = arg[i];
			ns741_rds_set_progname(ns741_name);
			eeprom_update_block((const void *)ns741_name, (void *)em_ns741_name, 8);
		}
		printf_P(PSTR("%s %s\n"), cmd, ns741_name);
		ossd_putlx(0, -1, ns741_name, 0);
		return 0;
	}

	if (str_is(cmd, PSTR("rdstext"))) {
		if (*arg == '\0') {
			puts(rds_data);
			return 0;
		}
		rt_flags |= RDS_RESET;
		if (str_is(arg, PSTR("reset"))) 
			rt_flags &= ~RDS_RT_SET;
		else {
			rt_flags |= RDS_RT_SET;
			ns741_rds_set_radiotext(arg);
		}
		return 0;
	}

	if (str_is(cmd, PSTR("status"))) {
		printf_P(PSTR("RDSID %s, %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n"),
			ns741_name,	fm_freq,
			is_on(rt_flags & NS741_RADIO), is_on(rt_flags & NS741_STEREO),
			rt_flags & NS741_TXPWR, (rt_flags & NS741_VOLUME) >> 8, (rt_flags & NS741_GAIN) ? -9 : 0);
		puts(rds_data);
		return 0;
	}

	if (str_is(cmd, PSTR("freq"))) {
		uint16_t freq = atoi((const char *)arg);
		if (freq < NS741_MIN_FREQ || freq > NS741_MAX_FREQ) {
			puts_P(PSTR("Frequency is out of band\n"));
			return -1;
		}
		freq = NS741_FREQ_STEP*(freq / NS741_FREQ_STEP);
		if (freq != ns741_freq) {
			ns741_freq = freq;
			ns741_set_frequency(ns741_freq);
			eeprom_update_word(&em_ns741_freq, ns741_freq);
		}
		printf_P(PSTR("%s set to %u\n"), cmd, ns741_freq);
		sprintf_P(fm_freq, PSTR("FM %u.%02uMHz"), ns741_freq/100, ns741_freq%100);
		ossd_putlx(2, -1, fm_freq, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);
		return 0;
	}

	if (str_is(cmd, PSTR("txpwr"))) {
		uint8_t pwr = atoi((const char *)arg);
		if (pwr > 3) {
			puts_P(PSTR("Invalid TX power level\n"));
			return -1;
		}
		rt_flags &= ~NS741_TXPWR;
		rt_flags |= pwr;
		ns741_txpwr(pwr);
		printf_P(PSTR("%s set to %d\n"), cmd, pwr);
		eeprom_update_word(&em_ns741_flags, rt_flags);

		get_tx_pwr(status);
		uint8_t font = ossd_select_font(OSSD_FONT_8x8);
		ossd_putlx(7, -1, status, 0);
		ossd_select_font(font);
		return 0;
	}

	if (str_is(cmd, PSTR("volume"))) {
		uint8_t gain = (rt_flags & NS741_VOLUME) >> 8;

		if (*arg != '\0')
			gain = atoi((const char *)arg);
		if (gain > 6) {
			puts_P(PSTR("Invalid Audio Gain value 0-6\n"));
			return -1;
		}
		ns741_volume(gain);
		printf_P(PSTR("%s set to %d\n"), cmd, gain);
		rt_flags &= ~NS741_VOLUME;
		rt_flags |= gain << 8;
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, PSTR("gain"))) {
		int8_t gain = (rt_flags & NS741_GAIN) ? -9 : 0;

		if (str_is(arg, PSTR("low")))
			gain = -9;
		else if (str_is(arg, PSTR("off")))
			gain = 0;

		ns741_input_low(gain);
		printf_P(PSTR("%s is %ddB\n"), cmd, gain);
		rt_flags &= ~NS741_GAIN;
		if (gain)
			rt_flags |= NS741_GAIN;
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, PSTR("mute"))) {
		if (str_is(arg, PSTR("on")))
			rt_flags |= NS741_MUTE;
		else if (str_is(arg, PSTR("off")))
			rt_flags &= ~NS741_MUTE;
		ns741_mute(rt_flags & NS741_MUTE);
		printf_P(PSTR("mute %s\n"), is_on(rt_flags & NS741_MUTE));
		return 0;
	}

	if (str_is(cmd, PSTR("stereo"))) {
		if (str_is(arg, PSTR("on")))
			rt_flags |= NS741_STEREO;
		else if (str_is(arg, PSTR("off")))
			rt_flags &= ~NS741_STEREO;
		ns741_stereo(rt_flags & NS741_STEREO);
		printf_P(PSTR("stereo %s\n"), is_on(rt_flags & NS741_STEREO));
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, PSTR("radio"))) {
		if (str_is(arg, PSTR("on")))
			rt_flags |= NS741_RADIO;
		else if (str_is(arg, PSTR("off")))
			rt_flags &= ~NS741_RADIO;
		ns741_radio(rt_flags & NS741_RADIO);
		printf_P(PSTR("radio %s\n"), is_on(rt_flags & NS741_RADIO));
		get_tx_pwr(status);
		uint8_t font = ossd_select_font(OSSD_FONT_8x8);
		ossd_putlx(7, -1, status, 0);
		ossd_select_font(font);
		return 0;
	}

	printf_P(PSTR("Unknown command '%s'\n"), cmd);
	return -1;
}

static uint16_t led;
static uint8_t  cursor;
static char cmd[CMD_LEN + 1];
static char hist[CMD_LEN + 1];

void cli_init(void)
{
	led = 0;
	cursor = 0;

	for(uint8_t i = 0; i <= CMD_LEN; i++) {
		cmd[i] = '\0';
		hist[i] = '\0';
	}

	mmr_led_off();
}

int8_t cli_interact(void *rht)
{
	uint16_t ch;

	// check if LED1 is ON long enough (20 ms in this case)
	if (led) {
		uint16_t span = mill16();
		if ((span - led) > 20) {
			mmr_led_off();
			led = 0;
		}
	}

	if ((ch = serial_getc()) == 0)
		return 0;
	// light up on board LED1 indicating serial data 
	mmr_led_on();
	led = mill16();
	if (!led)
		led = 1;

	if (ch & EXTRA_KEY) {
		if (ch == ARROW_UP && (cursor == 0)) {
			// execute last successful command
			for(cursor = 0; ; cursor++) {
				cmd[cursor] = hist[cursor];
				if (cmd[cursor] == '\0')
					break;
			}
			uart_puts(cmd);
		}
		return 1;
	}

	if (ch == '\n') {
		serial_putc(ch);
		if (*cmd && process(cmd, &rht) == 0)
			memcpy(hist, cmd, sizeof(cmd));
		for(uint8_t i = 0; i < cursor; i++)
			cmd[i] = '\0';
		cursor = 0;
		serial_putc('>');
		serial_putc(' ');
		return 1;
	}

	// backspace processing
	if (ch == '\b') {
		if (cursor) {
			cursor--;
			cmd[cursor] = '\0';
			serial_putc('\b');
			serial_putc(' ');
			serial_putc('\b');
		}
	}

	// skip control or damaged bytes
	if (ch < ' ')
		return 0;

	// echo
	serial_putc((uint8_t)ch);

	cmd[cursor++] = (uint8_t)ch;
	cursor &= CMD_LEN;
	// clean up in case of overflow (command too long)
	if (!cursor) {
		for(uint8_t i = 0; i <= CMD_LEN; i++)
			cmd[i] = '\0';
	}

	return 1;
}
