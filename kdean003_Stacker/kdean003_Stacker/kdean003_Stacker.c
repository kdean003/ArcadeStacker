#include <avr/io.h>
#include <avr/eeprom.h>

#include "nokia5110.c"
#include "timer.h"
#include "scheduler.h"
#include "ledmatrix88.c"
#include "usart.h"

enum SM1_States{SM1_init, SM1_move, SM1_stop_pressed, SM1_gameover};
enum SM2_States{SM2_init, SM2_matrix_display};

unsigned timer = 200;
unsigned char t = 0;
unsigned char i = 0;
unsigned char count = 0;
unsigned char receive = 0x00;
unsigned char score = 0;

unsigned char done = 0;
unsigned char direction = 0; // move current row left or right

unsigned char column_val = 0b10000000; // sets the pattern displayed on columns
unsigned char column_sel = 0b11110000; // grounds column to display pattern

unsigned char row[8][2] = {
	{0b10000000, 0b11110000},
	{0b01000000, 0b11111111},
	{0b00100000, 0b11111111},
	{0b00010000, 0b11111111},
	{0b00001000, 0b11111111},
	{0b00000100, 0b11111111},
	{0b00000010, 0b11111111},
	{0b00000001, 0b11111111}
};

/////////////////////////// STACKER FUNCTIONS FOR MATRIX ///////////////////////////

void run_matrix(){
	if(count > 0) {
		i++;
		if(i == 9)
		i = 0;
		column_val = row[i-1][0];
		column_sel = row[i-1][1];
	}
}

void set_matrix(){
	if(count == 1){
		row[count][1] = row[count-1][1];
	}
	else{
		row[count-1][1] = (row[count-1][1] | row[count-2][1]);
		row[count][1] = row[count-1][1];
		if (row[count][1] == 0xFF){
			done = 1;
		}
	}
}

void move_row(){
	if(count>0)
	{
		if (direction == 0) { // move row right
			if ((row[count-1][1] & 0x01) == 0x00) {
				row[count-1][1] = (row[count-1][1] << 1) | 0x01;
				direction = 1; // set move row left
			}
			else {
				row[count-1][1] = (row[count-1][1] >> 1) | 0x80;
			}
		}
		else if (direction == 1) { // move row left
			if ((row[count-1][1] & 0x80) == 0x00) {
				row[count-1][1] = (row[count-1][1] >> 1) | 0x80; // resets next moving column to far left
				direction = 0; //set move row right
			}
			else {
				row[count-1][1] = (row[count-1][1] << 1) | 0x01;
			}
		}
		column_sel = row[count-1][1];
	}
}

void write_score(){
	nokia_lcd_clear();
	nokia_lcd_set_cursor(0, 25);
	nokia_lcd_write_string("Your Score:", 1);
	nokia_lcd_set_cursor(70, 25);
	nokia_lcd_write_char(score + '0', 1);
	nokia_lcd_set_cursor(0, 35);
	nokia_lcd_write_string("High Score:", 1);
	nokia_lcd_set_cursor(70, 35);
	nokia_lcd_write_char(eeprom_read_byte((uint8_t*)46) + '0', 1);
	nokia_lcd_render();
}

/////////////////////////// STATE MACHINE 1 ///////////////////////////
int SMTick1(int state){
	switch(state){
		case SM1_init:
			state = SM1_move;
			break;
		case SM1_move:
			if(done)
				state = SM1_gameover;
			if(USART_HasReceived(0)){
				receive = USART_Receive(0);
				if(receive == 1){
					state = SM1_init;
				}
				if(receive == 2){
					state = SM1_stop_pressed;
				}
				if(receive == 3){
					eeprom_write_byte((uint8_t*)46, 0);
					write_score();
				}
			}	
			break;
		case SM1_stop_pressed:
			if(done)
				state = SM1_gameover;
			state = SM1_move;
			break;
		case SM1_gameover:
			if(USART_HasReceived(0)){
				receive = USART_Receive(0);
				if(receive == 1 || receive == 2){
					state = SM1_init;
				}
			}
			break;
	}
	switch(state){
			
		case SM1_init:
			t=0;
			count = 0;
			score = 0;
			timer = 200;
			done = 0;
			direction = 0;
			column_val = 0b10000000; 
			column_sel = 0b11110000;
			row[0][0] = 0b10000000; row[0][1] = 0b11110000;
			row[1][0] = 0b01000000; row[1][1] = 0b11111111;
			row[2][0] = 0b00100000; row[2][1] = 0b11111111;
			row[3][0] = 0b00010000; row[3][1] = 0b11111111;
			row[4][0] = 0b00001000; row[4][1] = 0b11111111;
			row[5][0] = 0b00000100; row[5][1] = 0b11111111;
			row[6][0] = 0b00000010; row[6][1] = 0b11111111;
			row[7][0] = 0b00000001; row[7][1] = 0b11111111;
			
			write_score();
			break;
		case SM1_move:			
			if(t >= timer && !done){
				move_row();
				t = 0;
			}
			break;
			
		case SM1_stop_pressed:
			set_matrix();
			
			if(count!=0 && !done)
				++score;
				
			count++;
			
			if(count == 9)
				state = SM1_gameover;
				
			timer -= 20;
			
			write_score();
			break;
			
		case SM1_gameover:
			if (score > eeprom_read_byte((uint8_t*)46)) {
				eeprom_write_byte((uint8_t*)46, score);
			}
			break;
	}
	t++;
	ledmatrix88_setrow(column_val);
	ledmatrix88_setcol(column_sel);
	ledmatrix88_print();
	return state;
}

/////////////////////////// STATE MACHINE 2 ///////////////////////////
int SMTick2(int state){
	switch(state){
		case SM2_init:
			state = SM2_matrix_display;
			break;
		case SM2_matrix_display:
			break;
	}
	switch(state){
		case SM2_init:
			break;
		case SM2_matrix_display:
			if(count>0)
				run_matrix();
			break;
	}
	return state;
}

/////////////////////////// MAIN ///////////////////////////
int main()
{
	DDRB = 0xFF; PORTB = 0x00; // Initialize DDRB to outputs
	DDRC = 0xFF; PORTC = 0x00; // Initialize DDRC to outputs
	DDRD = 0x03; PORTD = 0xFC;
	
	if(eeprom_read_byte((uint8_t*)46) < 0 || eeprom_read_byte((uint8_t*)46) > 9)
		eeprom_write_byte((uint8_t*)46, 0);
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 1;
	unsigned long int SMTick2_calc = 1;
	
	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SMTick1_calc, SMTick2_calc);
	//tmpGCD = 1;

	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;
	
	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;
	unsigned long int SMTick2_period = SMTick2_calc/GCD;

	//Declare an array of tasks
	static task task1, task2;
	task *tasks[] = { &task1, &task2};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	// Task 1
	task1.state = 0;//Task initial state.
	task1.period = SMTick1_period;//Task Period.
	task1.elapsedTime = SMTick1_period;//Task current elapsed time.
	task1.TickFct = &SMTick1;//Function pointer for the tick.
	
	// Task 2
	task2.state = 0;//Task initial state.
	task2.period = SMTick2_period;//Task Period.
	task2.elapsedTime = SMTick2_period;//Task current elapsed time.
	task2.TickFct = &SMTick2;//Function pointer for the tick.
	
	
	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();
	
	initUSART(0);
	
	nokia_lcd_init();
	nokia_lcd_clear();
	nokia_lcd_write_string("Welcome to",1);
	nokia_lcd_set_cursor(0, 10);
	nokia_lcd_write_string("Arcade Stacker",1);
	for(unsigned char k = 4; k < 80; k++){
		nokia_lcd_set_pixel(k, 20, 1);
	}
	nokia_lcd_set_cursor(0, 35);
	nokia_lcd_write_string("High Score: ", 1);
	nokia_lcd_set_cursor(70, 35);
	nokia_lcd_write_char(eeprom_read_byte((uint8_t*)46) + '0', 1);
	nokia_lcd_render();
	
	ledmatrix88_init();

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
