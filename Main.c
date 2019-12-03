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
#define PERIOD_1							4000000
#define PSEUDOPERIOD         	8000000
#define LIFETIME             	1000
#define RUNLENGTH            	600 // 30 seconds run length
#define PI 3.14159265

#define PADDLEHEIGHT					25
#define PADDLEX								10
#define PADDLEY								117

#define PADDLEHEIGHT					25
#define L_PADDLEX							10
#define R_PADDLEX							117

#define	BALLRAD								3

extern Sema4Type LCDFree;
uint16_t origin[2]; 	// The original ADC value of x,y if the joystick is not touched, used as reference
int16_t x = 63;  			// horizontal position of the crosshair, initially 63
int16_t y = 63;  			// vertical position of the crosshair, initially 63
int16_t prevx, prevy = 63;	// Previous x and y values of the crosshair
uint8_t select;  			// joystick push
uint8_t area[2];
uint32_t PseudoCount;

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
void UpdateBall(void);


// Ball stuff ;)
int16_t ball_x = 63;
int16_t ball_y = 63;
int16_t ball_xold = 63;
int16_t ball_yold = 63;
float ball_speed = 7;
float ball_angle = 0;
float x_speed = 0;
float y_speed = 0;

// Paddle Stuff ;)
int l_paddle_y = 63;
int r_paddle_y = 63;

// thread to update ball's location
void UpdateBall(void){
//	while(1){
	
	int diff;
	
//detect collisions with left paddle
	if (ball_x - 3 <= L_PADDLEX &&									//bounce if  ball is touching paddle's x coord (works)
			ball_y - 3 >= l_paddle_y - PADDLEHEIGHT/2 &&	// bounce if ball is below or at top of paddle
			ball_y + 3 <= l_paddle_y + PADDLEHEIGHT/2) { // bounce if ball is above or at bottom of paddle
			diff = ball_y - l_paddle_y;
			
			switch (diff) {
				
				case 12: case	11: case	10: case	9:
					ball_angle = (-45 * PI) / 180;
					break;
				
				case 8: case 7: case 6: case 5:
					ball_angle = (-30 * PI) / 180;
					break;
				
				case 4: case 3: case 2: case 1:
					ball_angle = (-15 * PI) / 180;
					break;
				
				case 0:
					ball_angle = 0;
					break;
				
				case -1: case -2: case -3: case -4:
					ball_angle = (15 * PI) / 180;
					break;
				
				case -5: case -6: case -7: case -8:
					ball_angle = (30 * PI) / 180;
					break;
				
				case -9: case -10: case -11: case -12:
					ball_angle = (45 * PI) / 180;
					break;
			}
			
		if (y_speed == 0) y_speed = x_speed;
		x_speed = 5 * cos(ball_angle);
		y_speed = 5 * sin(ball_angle);
					
		}
			
	//detect collisions with right paddle
	if (ball_x + 3 >= R_PADDLEX &&
			ball_y - 3 >= r_paddle_y - PADDLEHEIGHT/2 &&
			ball_y + 3 <= r_paddle_y + PADDLEHEIGHT/2) { 
			diff = ball_y - r_paddle_y;
			switch (diff) {
				
				case 12: case	11: case	10: case	9:
					ball_angle = (-135 * PI) / 180;
					break;
				
				case 8: case 7: case 6: case 5:
					ball_angle = (-150 * PI) / 180;
					break;
				
				case 4: case 3: case 2: case 1:
					ball_angle = (-165 * PI) / 180;
					break;
				
				case 0:
					ball_angle = 180;
					break;
				
				case -1: case -2: case -3: case -4:
					ball_angle = (165 * PI) / 180;
					break;
				
				case -5: case -6: case -7: case -8:
					ball_angle = (150 * PI) / 180;
					break;
				
				case -9: case -10: case -11: case -12:
					ball_angle = (135 * PI) / 180;
					break;
			}
		
		if (y_speed == 0) y_speed = x_speed;
		x_speed = -5 * cos(ball_angle);
		y_speed = 5 * sin(ball_angle);
			
				x_speed *= -1;
		}
	

		//detect collision with top or bottom of screen
		if (ball_y <= 3 || ball_y >= 124) {
			if (ball_y <= 3) {
				ball_y = 4;
			}
			y_speed *= -1;
		}
		
		ball_x += x_speed;
		ball_y -= y_speed;
}

//updates horizontal speed
int xSpeed(int angle){
	if (angle < 0){
		angle += 360;
	}
	
	// next two checks prevent overly-harsh angles, feeling cute, may delete later
	if (angle < 15 && angle > 0){
		angle = 15;
	}
	
	else if (angle < 180 && angle > 165){
		angle = 165;
	}

	ball_angle = angle;
	x_speed = cos((angle * PI) / 180) * ball_speed;
	
	return x_speed;
}

//updates horizontal speed
int ySpeed(int angle){
	if (angle < 0){
		angle += 360;
	}
	
	if (angle < 15 && angle > 0){
		angle = 15;
	}
	
	else if (angle < 180 && angle > 165){
		angle = 165;
	}

	ball_angle = angle;
	y_speed = sin((angle * PI) / 180) * ball_speed;
	
	return y_speed;
}

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
	BSP_Joystick_Input(&rawX,&rawY,&select);
	UpdateWork += UpdatePosition(rawX,rawY,&data); // calculation work
	data.x = r_paddle_y;
	while (1) { // Recv from Slave
		int data1 = UART_Recv();
		if (data1 == 0xFE) {
			break;
		}
		else if (data1 == 0xFA) {
			World_Init();
		}
		else {
			data.x = data1;
		}
	}
	JsFifo_Put(data); // send to consumer 
	UART_Send4(0xFF, data.y, ball_x, ball_y); // Send To Slave
}


// ***********ButtonWork*************
void ButtonWork(void){
	uint32_t StartTime,CurrentTime,ElapsedTime, ButtonStartTime;
	StartTime = OS_MsTime();
	ButtonStartTime = OS_Time();
	ElapsedTime = 0;
	OS_bWait(&LCDFree);
	BSP_LCD_FillScreen(BGCOLOR);
	Button1RespTime = (Button1RespTime + (OS_Time() - ButtonStartTime)) >> 1;
	while (ElapsedTime < LIFETIME){
		CurrentTime = OS_MsTime();
		ElapsedTime = CurrentTime - StartTime;
		BSP_LCD_Message(0,5,0,"Life Time:",LIFETIME);
		BSP_LCD_Message(1,0,0,"Horizontal Area:",area[0]);
		BSP_LCD_Message(1,1,0,"Vertical Area:",area[1]);
		BSP_LCD_Message(1,2,0,"Elapsed Time:",ElapsedTime);
		OS_Sleep(50);
		
	}
	BSP_LCD_FillScreen(BGCOLOR);
	OS_bSignal(&LCDFree);
  OS_Kill();  // done, OS does not return from a Kill
} 

//************SW1Push*************
void SW1Push(void){
  if(OS_MsTime() > 20 ){ // debounce
    if(OS_AddThread(&ButtonWork,128,4)){
			OS_ClearMsTime();
      NumCreated++; 
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
	
}

//******** Consumer *************** 
void Consumer(void){
	while(1){
		jsDataType data;
		JsFifo_Get(&data);
		OS_bWait(&LCDFree);

		l_paddle_y = data.y;
		BSP_LCD_DrawFastVLine(PADDLEX, prevy - 12, PADDLEHEIGHT, LCD_BLACK); //erase left paddle
		BSP_LCD_DrawFastVLine(PADDLEX, l_paddle_y - 12, PADDLEHEIGHT, LCD_GREEN); //draw left paddle
		
		r_paddle_y = data.x;
		BSP_LCD_DrawFastVLine(PADDLEY, prevx - 12, PADDLEHEIGHT, LCD_BLACK); //erase paddle
		BSP_LCD_DrawFastVLine(PADDLEY, r_paddle_y - 12, PADDLEHEIGHT, LCD_RED); //draw paddle
		
		BSP_LCD_DrawBall(ball_xold, ball_yold, LCD_BLACK);
		BSP_LCD_DrawBall(ball_x, ball_y, LCD_WHITE);
		
		OS_bSignal(&LCDFree);
		prevy = l_paddle_y; 
		prevx = r_paddle_y;
		ball_xold = ball_x;
		ball_yold = ball_y;
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
	x = 63;
	y = 63;
	
	// Ball stuff ;)
	ball_x = 63;
	ball_y = 63;
	ball_speed = 7;
	ball_angle = 0;
	x_speed = 0;
	y_speed = 0;

	// Paddle Stuff ;)
	l_paddle_y = 63;
	r_paddle_y = 63;
	
	BSP_LCD_FillScreen(LCD_BLACK);
	origin[0] = 0x7FF;
	origin[1] = 0x7FF;
	
	BSP_LCD_DrawFastVLine(PADDLEX, prevx - 12, PADDLEHEIGHT, LCD_BLACK); //draw left paddle
	BSP_LCD_DrawFastVLine(PADDLEY, prevy - 12, PADDLEHEIGHT, LCD_BLACK); //draw right paddle
	BSP_LCD_DrawBall(ball_xold, ball_yold, LCD_BLACK);
	
	BSP_LCD_DrawFastVLine(PADDLEX, l_paddle_y - 12, PADDLEHEIGHT, LCD_GREEN); //draw left paddle
	BSP_LCD_DrawFastVLine(PADDLEY, r_paddle_y - 12, PADDLEHEIGHT, LCD_RED); //draw right paddle
	
	
	ball_angle = PI;
	x_speed = 5 * cos(ball_angle);
	y_speed = 0;
	
	BSP_LCD_DrawBall(ball_x, ball_y, LCD_WHITE);
	ball_xold = ball_x;
	ball_yold = ball_y;
}

//******************* Main Function**********
int main(void){ 
  OS_Init();           // initialize, disable interrupts
	Device_Init();
  World_Init();

				 
//********initialize communication channels
  JsFifo_Init();

//*******attach background tasks***********
  //OS_AddSW1Task(&SW1Push, 4);
	OS_AddSW2Task(&SW2Push, 1);
  OS_AddPeriodicThread(&Producer, PERIOD, 1); // 2 kHz real time sampling of PD3
	OS_AddPeriodicThread(&UpdateBall, PERIOD_1, 3); // 2 kHz real time sampling of PD3
 
  OS_AddThread(&Consumer, 128, 2); 

 
  OS_Launch(TIME_1MS);
	return 0;
}