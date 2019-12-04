// Main.c
// Runs on LM4F120/TM4C123
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file

// Jonathan W. Valvano 2/20/17, valvano@mail.utexas.edu
// Modified by Sile Shu 10/4/17, ss5de@virginia.edu
// Modified by Mustafa Hotaki 7/29/18, mkh3cf@virginia.edu

#include <stdint.h>
#include "OS.h"
#include "tm4c123gh6pm.h"
#include "LCD.h"
#include <string.h> 
#include "UART.h"
#include "FIFO.h"
#include "joystick.h"
#include "PORTE.h"
#include <math.h>

// Constants
#define BGCOLOR     					LCD_BLACK
#define CROSSSIZE            	5
#define PERIOD               	4000000   // DAS 20Hz sampling period in system time units
#define PSEUDOPERIOD         	8000000
#define LIFETIME             	1000
#define RUNLENGTH            	600 // 30 seconds run length
#define PI 3.14159265

#define PADDLEHEIGHT					25
#define PADDLEX								10
#define PADDLEY								117

extern Sema4Type LCDFree;
uint16_t origin[2]; 	// The original ADC value of x,y if the joystick is not touched, used as reference
int16_t x = 63;  			// horizontal position of the crosshair, initially 63
int16_t y = 63;  			// vertical position of the crosshair, initially 63
int16_t prevx, prevy =63;	// Previous x and y values of the crosshair
uint8_t select;  			// joystick push
uint8_t area[2];
uint32_t PseudoCount;

uint8_t cond = 0; 

char* s1 = "Paused";
char* sw = "Winner";
char* sl = "Loser";
char* s4 = "Btn 2 to play again";

int count = 99;

static unsigned long NumCreated;   		// Number of foreground threads created
unsigned long NumSamples;   		// Incremented every ADC sample, in Producer
unsigned long UpdateWork;   		// Incremented every update on position values
unsigned long Calculation;  		// Incremented every cube number calculation
unsigned long DisplayCount; 		// Incremented every time the Display thread prints on LCD 
unsigned long ConsumerCount;		// Incremented every time the Consumer thread prints on LCD
unsigned long Button1RespTime; 	// Latency for Task 2 = Time between button1 push and response on LCD 
unsigned long Button2RespTime; 	// Latency for Task 7 = Time between button2 push and response on LCD
unsigned long Button1PushTime; 	// Time stamp for when button 1 was pushed
unsigned long Button2PushTime; 	// Time stamp for when button 2 was pushed

void World_Init(void);


// Ball stuff ;)
int16_t ball_x = 63;
int16_t ball_y = 63;

int16_t ball_xold = 63;
int16_t ball_yold = 63;

// Paddle Stuff ;)
int l_paddle_y = 63;
int r_paddle_y = 63;


void Device_Init(void){
	UART_Init();
	BSP_LCD_OutputInit();
	BSP_Joystick_Init();
}


//******** Producer *************** 
int UpdatePosition(uint16_t rawx, uint16_t rawy, jsDataType* data){

	if (rawy < origin[1]){
		y = y + ((origin[1] - rawy) >> 4);
	}
	else{
		y = y - ((rawy - origin[1]) >> 4);
	}

	if (y > 127 - PADDLEHEIGHT/2){
		y = 127 - PADDLEHEIGHT/2;}
	if (y < 0 + PADDLEHEIGHT/2){
		y = 0 + PADDLEHEIGHT/2;}
	data->y = y;
	return 1;
}

void Producer(void){
		uint16_t rawX,rawY; // raw adc value
		uint8_t select;
		jsDataType data;
		data.x = l_paddle_y;
		data.y = r_paddle_y;
		while (1) { // Recv from master
			int data1 = UART_Recv();
			if (data1 == 0xFE) {
				break;
			}
			else if (data1 == 0xFA){
				World_Init();
			}
			else if (data1 == 0xFF){
				count = 0;
			}
			else if (data1 == 0xF0) { // pause
				cond = 0;
			}
			else if (data1 == 0xF1) { // pause
				cond = 1;
			}
			else if (data1 == 0xFB) { // master win (LOSS)
				cond = 12;
			}
			else if (data1 == 0xFC) { // slave win (WIN)
				cond = 13;
			}
			else if (count == 0) {
				data.x = data1;
				count++;
			}
			else if (count == 1) {
				ball_x = data1;
				count++;
			}
			else if (count == 2) {
				ball_y = data1;
				count++;
			}
		}
		if (cond == 0){
			BSP_Joystick_Input(&rawX,&rawY,&select);
			UpdateWork += UpdatePosition(rawX,rawY,&data); // calculation work 
			UART_Send(data.y); // Send To master
		}
		JsFifo_Put(data); // send to consumer
}


//******** Consumer *************** 
void Consumer(void){
	while(1){
		if (cond == 12) { //LOSS
				BSP_LCD_FillScreen(LCD_BLACK);
				BSP_LCD_DrawString(8, 5, sl, LCD_RED);
				BSP_LCD_DrawString(1, 6, s4, LCD_WHITE);
				cond = 2;
		}
		else if (cond == 13) { // WIN
				BSP_LCD_FillScreen(LCD_BLACK);
				BSP_LCD_DrawString(8, 5, sw, LCD_GREEN);
				BSP_LCD_DrawString(1, 6, s4, LCD_WHITE);
				cond = 3;
		}
		else if (cond == 1) {
			BSP_LCD_DrawString(0, 0, s1, LCD_WHITE);
		}
		else if (cond == 0) {
			jsDataType data;
			JsFifo_Get(&data);
			OS_bWait(&LCDFree);
			BSP_LCD_DrawBall(ball_xold, ball_yold, LCD_BLACK);
			BSP_LCD_DrawBall(ball_x, ball_y, LCD_WHITE);
			
						BSP_LCD_DrawString(0, 0, s1, LCD_BLACK);
			
			l_paddle_y = data.x;
			BSP_LCD_DrawFastVLine(PADDLEX, prevx - 12, PADDLEHEIGHT, LCD_BLACK); //erase paddle
			BSP_LCD_DrawFastVLine(PADDLEX, l_paddle_y - 12, PADDLEHEIGHT, LCD_RED); //draw paddle
			
			r_paddle_y = data.y;
			BSP_LCD_DrawFastVLine(PADDLEY, prevy - 12, PADDLEHEIGHT, LCD_BLACK); //erase paddle
			BSP_LCD_DrawFastVLine(PADDLEY, r_paddle_y - 12, PADDLEHEIGHT, LCD_GREEN); //draw paddle
			OS_bSignal(&LCDFree);
			prevx = l_paddle_y; 
			prevy = r_paddle_y;
			ball_xold = ball_x;
			ball_yold = ball_y;
		}
	}
}

//************SW1Push*************
void SW1Push(void){
  if(OS_MsTime() > 20 ){ // debounce
    if (cond == 0) {
			UART_Send(0xF1);
			cond = 1;
		}
		else if (cond == 1) {
			UART_Send(0xF0);
			cond = 0;
		}
    OS_ClearMsTime();  // at least 20ms between touches
  }
	
}

//************SW2Push*************
void SW2Push(void){
  if(OS_MsTime() > 20 ){ // debounce
		UART_Send(0xFA);
		World_Init();
    OS_ClearMsTime();  // at least 20ms between touches
  }
	
}

void World_Init(void){
	cond = 0;
	
	x = 63;
	y = 63;
	ball_x = 63;
  ball_y = 63;
	l_paddle_y = 63;
  r_paddle_y = 63;
	BSP_LCD_FillScreen(LCD_BLACK);
	origin[0] = 0x7FF;
	origin[1] = 0x7FF;
	
	BSP_LCD_DrawFastVLine(PADDLEX, prevx - 12, PADDLEHEIGHT, LCD_BLACK); //draw left paddle
	BSP_LCD_DrawFastVLine(PADDLEY, prevy - 12, PADDLEHEIGHT, LCD_BLACK); //draw right paddle
	
	BSP_LCD_DrawBall(ball_xold, ball_yold, LCD_BLACK);
	
	BSP_LCD_DrawFastVLine(PADDLEX, l_paddle_y - 12, PADDLEHEIGHT, LCD_RED); //draw left paddle
	BSP_LCD_DrawFastVLine(PADDLEY, r_paddle_y - 12, PADDLEHEIGHT, LCD_GREEN); //draw right paddle
	
	BSP_LCD_DrawBall(ball_x, ball_y, LCD_WHITE);
	
	ball_xold = ball_x;
	ball_yold = ball_y;

}

//******************* Main Function**********
int main(void){ 
  OS_Init();
	Device_Init();
  World_Init();

				 
//********initialize communication channels
  JsFifo_Init();

//*******attach background tasks***********
  OS_AddSW1Task(&SW1Push, 1);
	OS_AddSW2Task(&SW2Push, 1);
  OS_AddPeriodicThread(&Producer, PERIOD, 2);
	
 
  OS_AddThread(&Consumer, 128, 1); 

 
  OS_Launch(TIME_1MS);
	return 0;
}
