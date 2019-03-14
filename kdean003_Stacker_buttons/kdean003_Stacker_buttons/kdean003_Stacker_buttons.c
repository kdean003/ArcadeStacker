#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "timer.h"
#include "scheduler.h"
#include "usart.h"

#define restart_button (~PINA & 0x01)
#define stop_button (~PINC & 0x20)
#define reset_button (~PINB & 0x01)

enum SM1_States{SM1_wait, SM1_restart_button_press, SM1_restart_button_release, SM1_stop_button_press, SM1_stop_button_release, SM1_reset_button_press, SM1_reset_button_release};

int SMTick1(int state){
	switch(state){
		case SM1_wait:
			if(restart_button && USART_IsSendReady(0))
				state = SM1_restart_button_press;
			else if(stop_button && USART_IsSendReady(0))
				state = SM1_stop_button_press;
			else if(reset_button && USART_IsSendReady(0))
				state = SM1_reset_button_press;
			break;
		case SM1_restart_button_press:
			if(USART_HasTransmitted(0))
				state = SM1_restart_button_release;
			break;
		case SM1_restart_button_release:
			if(!restart_button)
				state = SM1_wait;
			break;
		case SM1_stop_button_press:
			if(USART_HasTransmitted(0))
				state = SM1_stop_button_release;
			break;
		case SM1_stop_button_release:
			if(!stop_button)
				state = SM1_wait;
			break;
		case SM1_reset_button_press:
			if(USART_HasTransmitted(0))
				state = SM1_reset_button_release;
			break;
		case SM1_reset_button_release:
			if(!reset_button)
				state = SM1_wait;
			break;
	}
	switch(state){
		case SM1_wait:
			break;
		case SM1_restart_button_press:
			USART_Send(1, 0);
			break;
		case SM1_restart_button_release:
			USART_Flush(0);
			break;
		case SM1_stop_button_press:
			USART_Send(2, 0);
			break;
		case SM1_stop_button_release:
			USART_Flush(0);
			break;
		case SM1_reset_button_press:
			USART_Send(3, 0);
			break;
		case SM1_reset_button_release:
			USART_Flush(0);
			break;
	}
	return state;
}

int main()
{
	DDRA = 0x00; PORTA = 0xFF; // Initialize DDRA to inputs
	DDRB = 0x00; PORTB = 0xFF; // Initialize DDRB to inputs
	DDRC = 0x00; PORTC = 0xFF; // Initialize DDRC to inputs
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 10;

	//Calculating GCD
	unsigned long int tmpGCD = 1;
	//tmpGCD = findGCD(SMTick1_calc);
	tmpGCD = 10;

	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;
	
	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;

	//Declare an array of tasks
	static task task1;
	task *tasks[] = { &task1};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	// Task 1
	task1.state = 0;//Task initial state.
	task1.period = SMTick1_period;//Task Period.
	task1.elapsedTime = SMTick1_period;//Task current elapsed time.
	task1.TickFct = &SMTick1;//Function pointer for the tick.
	

	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();
	
	initUSART(0);
	USART_Flush(0);

	unsigned short i; // Scheduler for-loop iterator
	while(1) {
		// Scheduler code
		for ( i = 0; i < numTasks; i++ ) {
			// Task is ready to tick
			if ( tasks[i]->elapsedTime >= tasks[i]->period ) {
				// Setting next state for task
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				// Reset the elapsed time for next tick.
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}

	// Error: Program should not exit!
	return 0;
}
