/*
 * main.cpp
 *
 *  Created on: 24.09.2015
 *      Author: Shcheblykin
 */

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

static const uint8_t FSM_NEXT_MAX = 3;
static const uint8_t FSM_LOOP_TIME_MS = 5;
static const uint32_t TIME_LOCK_OPEN_MS = 60000UL;
static const uint8_t phoneNumber[] = "30954";

void low_level_init() __attribute__((__naked__)) __attribute__((section(".init3")));

static void setCheck(bool set);
static void openLock(bool open);
static void ledOn(bool set);

static uint8_t reset();
static uint8_t waitRise();
static uint8_t calcPulse();
static uint8_t waitFall();
static uint8_t checkDig();
static uint8_t lockOpen();

typedef uint8_t (*pFunc) ();

typedef enum {
	FSM_RESET 			= 0,
	FSM_WAIT_RISE		= 1,
	FSM_CALC_PULSE		= 2,
	FSM_WAIT_FALL		= 3,
	FSM_CHECK_DIG		= 4,
	FSM_LOCK_OPEN		= 5,
	FSM_MAX
} eFSM;

typedef struct {
	pFunc func;
	eFSM next[FSM_NEXT_MAX];
} sFSM;

static const sFSM fsm[FSM_MAX] = {
		{&reset, 		{FSM_RESET,		FSM_RESET,		FSM_RESET}		},
		{&waitRise, 	{FSM_WAIT_RISE, FSM_CALC_PULSE,	FSM_CHECK_DIG}	},
		{&calcPulse, 	{FSM_WAIT_FALL, FSM_WAIT_FALL, 	FSM_WAIT_FALL}	},
		{&waitFall,		{FSM_WAIT_FALL, FSM_WAIT_RISE, 	FSM_CHECK_DIG}	},
		{&checkDig,		{FSM_WAIT_RISE,	FSM_LOCK_OPEN,	FSM_RESET}		},
		{&lockOpen,		{FSM_LOCK_OPEN, FSM_RESET,		FSM_RESET}		}
};


uint8_t digit = 0;
uint8_t pos = 0;

void setCheck(bool set) {

	if (set) {
		PORTC |= (1 << PC1);
	} else {
		PORTC &= ~(1 << PC1);
	}
}

void openLock(bool open) {

	ledOn(open);

	if ((PINB & (1 << PB2)) == 0) {
		open = !open;
	}

	if (open) {
		PORTC |= (1 << PC3);
	} else {
		PORTC &= ~(1 << PC3);
	}
}

void ledOn(bool set) {

	if (set) {
		PORTD |= (1 << PD5);
	} else {
		PORTD &= ~(1 << PD5);
	}
}

uint8_t reset() {

	openLock(false);

	return 0;
}

uint8_t waitRise() {

	if ((PINC & (1 << PC2)) == 0) {
		return 2;
	}

	return PINC & (1 << PC0) ? 1 : 0;
}

uint8_t calcPulse() {

	digit++;
	return 0;
}

uint8_t waitFall() {

	if ((PINC & (1 << PC2)) == 0) {
		return 2;
	}

	return PINC & (1 << PC0) ? 0 : 1;
}

uint8_t checkDig() {

	if (digit <= 1) {
		return 0;
	}

	digit = (digit - 1) % 10 + '0';

	if (digit != phoneNumber[pos]) {
		return 2;
	}

	digit = 0;

	pos++;
	if (pos == (sizeof(phoneNumber) / sizeof(phoneNumber[0]) - 1)) {
		return 1;
	}

	return 0;
}

uint8_t lockOpen() {
	static uint16_t time = (uint16_t) TIME_LOCK_OPEN_MS / FSM_LOOP_TIME_MS;

	if (time > 0) {
		openLock(true);
		time--;
		return 0;
	}

	return 2;
}


int __attribute__ ((OS_main)) main() {
	eFSM current = FSM_WAIT_RISE;
	uint8_t next = 0;

	setCheck(true);
	openLock(false);

	while(1) {
		if (current >= FSM_MAX) current = FSM_RESET;
		next = (*fsm[current].func) ();
		current = (next >= FSM_NEXT_MAX) ? FSM_RESET : fsm[current].next[next];
		_delay_ms(FSM_LOOP_TIME_MS);
	}
}

void low_level_init() {

	// по-умолчанию все порты настроены на вход без подтяжки

	// PB2 - CTRL, вход с подтяжкой к 5B
	DDRB = 0x00;
	PORTB = (1 << PB2);

	// PC0 - IN2/NUMBER/зеленый, вход с подтяжкой к 5В
	// PC1 - OUT1/CHECK/желтый, выход по-умолчанию GND
	// PC2 - IN1/DIAL/красный, вход с подтяжкой к 5В
	// PC3 - RELAY, выход по-умолчанию GND
	DDRC = (0 << PC0) | (1 << PC1) | (0 << PC2) | (1 << PC3);
	PORTC = (1 << PC0) | (1 << PC2);

	// PD5 - IND, выход по-умолчанию GND
	DDRD = (1 << PD5);
	PORTD = 0x00;
}
