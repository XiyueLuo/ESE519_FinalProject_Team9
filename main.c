/*
 * GccApplication1.c
 *
 * Created: 2021/10/19 18:40:49
 * Author : Xiyue Luo
 */ 

#define F_CPU 16000000UL
#define BAUD_RATE 9600
#define BAUD_PRESCALER (((F_CPU / (BAUD_RATE * 16UL))) - 1)
//need to test
#define water_speed 0.05
#define daily_required 1000
#define warning_hour 20
#define warning_minute 51
#define warning_second 50

#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "print/uart.h"
#include "LCD/ASCII_LUT.h"
#include "LCD/LCD_GFX.h"
#include "LCD/ST7735.h"
#include "DS1307/ds1307.h"
#include "DS1307/i2c.h"
#include "DS1307/global.h"
#include "print/uart.h"

long edge1=0;
long edge2 = 0;
int ovf_echo=0;
int ovf_tri=0;
unsigned long ticks=0;
long dis_cm=0;
char String_time[25];
char String_volume_each_time[25];
char String_sum_volume[25];
char String_goal[25];

int high = 0;

//for water volume
int ovf_pump=0;
long pump_on=0;
long pump_off=0;
long pump_time=0;
int volume_each_time=0;
int state=0;
int display=0;
int sum_volume=0;

//buffer to store time conversion
char buffer[20];

//buzzer for reminding
int freq_reminder[]={30,26,23,22,20};
long beat_reminder[]={1000,10000,500000,200,40000};
	
//buzzer for warning
int freq_warn[]={10,40,50,60,20};
long beat_warn[]={500000,40000,200,200,100000};

struct current_time{
	u08 hour;
	u08 minute;
	u08 second;
};

struct current_time t_now={0,0,0};

struct current_time get_time (struct current_time t){
	/*  get data from the RTC   */
	t.hour = ds1307_hours();
	if( t.hour < 6 ){
		t.hour = t.hour + 24 - 6;
	}
	else{
		t.hour = t.hour - 6;
	}
	t.minute = ds1307_minutes()+33;
	if( t.minute >= 60 ){
		t.minute = t.minute - 60;
		t.hour = t.hour + 1;
	}
	t.second = ds1307_seconds();
	if (t.second < 9){
		t.second = t.second + 60 - 9;
		t.minute = t.minute - 1;
	}
	else{
		t.second = t.second - 9;
	}
	return t;
}

struct current_time display_time (struct current_time t){
	sprintf(String_goal,"Toady's Goal: %d ml", daily_required);
	LCD_drawString(10,10,String_goal,BLACK, WHITE);
	t = get_time(t);
	sprintf(String_time,"%2d : %2d : %2d", t_now.hour,t_now.minute,t_now.second);
	LCD_drawString(10,50,String_time,BLACK, WHITE);
	return t;
}

void display_volume(int volume_each, int volume_sum){
	/*-----------volume each time display-----------*/
	
	LCD_drawString(10,70,"Water volume this time:",BLACK, WHITE);
	sprintf(String_volume_each_time," %3d ml", volume_each);
	UART_putstring(String_volume_each_time);
	LCD_drawString(10,80,String_volume_each_time,BLACK, WHITE);
	
	/*-----------sum volume display-----------*/
	LCD_drawString(10,100,"Sum volume:",BLACK, WHITE);
	sprintf(String_sum_volume," %3d ml", volume_sum);
	UART_putstring(String_sum_volume);
	LCD_drawString(10,110,String_sum_volume,BLACK, WHITE);
}

void Initialize(){
	cli(); /*disable global interrupts*/
	
	DDRB &= ~(1<<DDB0); //set PB0 input pin-echo
	DDRD |= (1<<DDD7); //set PD7 output pin to trigger
	DDRD |= (1<<DDD5); //set PD5 output pin to buzzer
	DDRD |= (1<<DDD4);//set PD4 to relay
	
	//Initialize the I2C bus and the DS1307
	i2cInit();
	ds1307_init(kDS1307Mode24HR);
	
	//Timer1 Initialize
	TCCR1B &= ~(1<<CS10);
	TCCR1B |= (1<<CS11);
	TCCR1B &= ~(1<<CS12);/*Timer1 is 8-times prescale->2MHz*/
	
	TCCR1A &= ~(1<<WGM10);
	TCCR1A &= ~(1<<WGM11);
	TCCR1B &= ~(1<<WGM12);/*set timer1 to normal*/
	
	TIMSK1 |= (1<<ICIE1); //enable the input capture*/
	TCCR1B |= (1<<ICES1);/*looking for rising edge*/
	TIFR1 |= (1<<ICF1); /*clear input capture flag*/
	
	TIMSK1|=(1<<OCIE1A);//set the output compare interrupt request
	TIFR1 |= (1<<OCF1A);//clear output compare interrupt flag
	
	TIMSK1 |= (1 << TOIE1); // enable timer overflow
	
	//timer0 setup for buzzer
	//set timer0 to be divided by 1024, which is 15625 Hz
	TCCR0B |=(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02);
	
	//set timer0 to PWM phase correct mode
	TCCR0A |= (1<<WGM00);
	TCCR0A &= ~(1<<WGM01);
	TCCR0B |= (1<<WGM02);
	
	// clear OC0B on compare match
	TCCR0A &= ~(1<<COM0B0);
	TCCR0A |= (1<<COM0B1);
	
	// clear interrupt flag
	TIFR0 |= (1<<OCF0A);
	
	//Initialize LCD
	lcd_init();
	
	OCR0A=0;
	OCR0B = OCR0A/2;
	
	sei();
}

ISR(TIMER0_COMPA_vect)
{
	
}

ISR(TIMER1_OVF_vect)
{
	ovf_echo++;
	ovf_tri++;
	ovf_pump++;
}

//Output of Arduino is a 10us TTL to trigger sensor
ISR(TIMER1_COMPA_vect)
{
	if (!(PINB & (1<<PINB0)) )//when PD2/Echo is low, need to send a trigger
	{
		PORTD|=(1<<PORTD7);//set PD7 high
		_delay_us(10);
		PORTD ^= (1<<PORTD7);
	}
}

//Input of Arduino is looking for the rising and falling edge of Echo
ISR(TIMER1_CAPT_vect) 
{
	edge2 = ICR1;/*store time*/
	ticks=(edge2+ovf_echo*65535)-edge1;/*calculate ticks*/
	edge1=edge2;
	if (high == 1){
		dis_cm= ticks*0.0085;/*(period*340*100/2000000)/2;*/
		// 		sprintf(String,"Distance is %ld cm\n", dis_cm);
		// 		UART_putstring(String);
		if(dis_cm>=5){
			PORTD |= (1<<PORTD4);
			if (state==1)
			{
				pump_off=ICR1;
				pump_time=((pump_off+ovf_pump*65535)-pump_on)/2000;
				volume_each_time=pump_time*water_speed;
				display=1;
				sum_volume=sum_volume+volume_each_time;
			}
			state=0;
			ovf_pump=0;
		}
		if(dis_cm<5){
			PORTD &= ~(1<<PORTD4);
			pump_on=ICR1;
			state=1;//start calculate the fluid intake
		}
	}
	ovf_echo=0;/*clear the overflow*/
	TCCR1B ^= (1<<ICES1);/*toggle state*/
	high = (1 - high);
}
	

static void delay(long ms)
{
	volatile long i,j;
	for (i=0;i<ms;i++);
	for (j=0;j<10000;j++);
}

void buzzer_remind()
{
	for(int i=0;i<5; i++)
	{
		OCR0A=freq_reminder[i];
		OCR0B = OCR0A/2;
		delay(beat_reminder[i]);
	}
	OCR0A=0;
	OCR0B=OCR0A/2;
}

void buzzer_warn()
{
	for(int i=0;i<5; i++)
	{
		OCR0A=freq_warn[i];
		OCR0B = OCR0A/2;
		delay(beat_warn[i]);
	}
	OCR0A=0;
	OCR0B=OCR0A/2;
}


int main(void)
{
	UART_init(BAUD_PRESCALER);
	Initialize();
	
	LCD_setScreen(WHITE);
	LCD_drawString(10, 40,"Current Time is",BLACK, WHITE);
	
	while (1)
	{
		t_now = display_time(t_now);
		
		display_volume(volume_each_time, sum_volume);
		
		/*---------buzzer for remind------------*/
		if (t_now.second==0)
		{
			buzzer_remind();
		}
		
		/*----------buzzer for warning-----------*/
		if((t_now.hour==warning_hour)&&(t_now.minute==warning_minute)&&(t_now.second==warning_second))
		{
			if(sum_volume<daily_required)
			{
				buzzer_warn();
			}
		}
		
		
		
	}
}
