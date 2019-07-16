/*
* Test.c
* Cds의 밝기에 따라 4단계로 LED 밝기를 조절하고,
* PC에서 입력에 따라 P.456 가 동작되도록 cording 하시오
* Created: 2019-07-05 오후 4:11:12
*  Author: KYM
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>	// strlen 함수 호출 위해..
#include "Lcd.h"

#define Frequency 7372800

#define  DIR_L	0	// 모터 회전 방향 제어(Left)
#define  DIR_R	1	// 모터 회전 방향 제어(Right)

volatile unsigned int ADC_result = 0;	// Cds 출력 값(아날로그)을 디지털 값으로 변환한 결과 값을 저장할 변수

unsigned char timer0Cnt = 0, mot_cnt = 0;
volatile unsigned char dir = DIR_R;	// 초기 방향 우측..
volatile unsigned char Step_flag = 0, buzzer_flag = 0, LCD_flag = 0x01;	// 각 기능을 제어할 flag

volatile unsigned char flag = 1;	// 키보드 입력을 제어하기 위한 flag...(한번 입력 된 후 수정이 안됨..)

// 1-2 상 여자 값을 사용
unsigned char Step[] = {0x90, 0x80, 0xc0, 0x40, 0x60, 0x20, 0x30, 0x10};
// 피아노 음계 주파수 PWM
unsigned int DoReMi[8] = {523, 587, 659, 698, 783, 880, 987, 1046};
volatile unsigned char piano = 0;

volatile char RX_data = 0;	// 키보드 입력 값을 저장 할 변수

// Uart 통신 문자 출력 함수
void putch(unsigned char data){
	while((UCSR0A & (1 << UDRE0)) == 0);
	UDR0 = data;
	UCSR0A |= (1 << UDRE0);
}

// Uart 통신 문자열 출력 함수
void putch_Str(char *str){
	unsigned char i = 0;
	while(str[i] != '\0')
	putch(str[i++]);
}

// Uart 통신 키보드로 입력 받은 값 반환 하는 함수
unsigned char getch(void){
	unsigned char data;
	//while((UCSR0A & 0x80) == 0);	// 키보드 입력 전 까지 대기...
	data = UDR0;
	putch(data);
	UCSR0A |= 0x80;
	return data;
}

void AllStop(void);			// Piezzo, Step Motor 멈춤...
void BuzzerOn(void);		// Piezzo를 On 시켜 도레미 출력
void Step_RightSpin(void);	// Step Motor를 우측으로 회전
void Step_LeftSpin(void);	// Step Motor를 좌측으로 회전

int main(){
	
	// ============= LCD ============= //
	
	DDRA = 0xff;	// LCD 제어 포트
	LcdInit_4bit();	// LCD 초기화...
	
	// ============= LED ============= //
	
	DDRB = 0xf0;	// LED 제어 포트 => PB5 사용(OCR0)
	PORTB = 0x00;
	
	// ============= Step Motor ============= //
	
	DDRD = 0xf0;	// Step Motor 제어 포트
	
	//TCCR1A |= 0x0a;
	/* TIMER/COUNTER1 Fast PWM Mode  No Prescaling*/
	TCCR1A |= 0x8a;		// OCR1A(LED) ,OCR1C(Piezzo)...
	TCCR1B |= 0x19;		// TCCR1B |= (1 << WGM13) | (1 << WGM12) | (1 << CS10);
	TCNT1 = 0;
	
	/* TIMER/COUNTER0 Normal Mode 1024 From prescaler */
	TIMSK = 0x01;	// TIMSK |= (1 << TOIE0); => T/C0 오버플로우 인터럽트 허용
	TCCR0 = 0x07;	// 1024 분주
	TCNT0 = 184;
	// 외부 인터럽트...
	EICRB = 0xff;	
	EIMSK = 0xf0;
	// ============= Uart ============= //
	
	DDRE = 0x0e;	// Uart0 => RX : 0, TX : 1
	
	// ============= Cds ============= //
	DDRF = 0x02;	
	PORTF = 0x02;
	
	unsigned int AdData = 0;	// 10bit ADC 값 저장 변수
	
	ADMUX = 0x40;	// ADC0 선택
	ADCSRA = 0xaf;
	ADCSRA |= 0x40;
	
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);	// 0x18;
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);	// 0x06;
	UBRR0L = 3;
	
	sei();
	
	Lcd_Pos(0,0);
	Byte step_text[] = "Step Motor :";
	Lcd_STR(step_text);
	Lcd_Pos(0,strlen(step_text) + 1);
	Lcd_STR("OFF");
	
	Lcd_Pos(1,0);
	Byte buzzer_text[] = "Buzzer :";
	Lcd_STR(buzzer_text);
	Lcd_Pos(1,strlen(buzzer_text) + 1);
	Lcd_STR("OFF");
	
	while(1){
		
		//if(flag != 0)
			//RX_data = getch();
			
		RX_data = getch();		// getch() 함수에서 문자 입력 전까지 대기 상태가 되어
								// while문이 진행 되다 멈춤...
								// 버그는 알지만 고치질 못 하겠어요...
		
		if(RX_data == '1')
			AllStop();
		else if(RX_data == '2')
			BuzzerOn();
		else if(RX_data == '3')
			Step_RightSpin();
		else if(RX_data == '4')
			Step_LeftSpin();
		
		if(LCD_flag != 0x00){
	
			if(LCD_flag & 0x01)	// 스탭 모터 및 부저 상태 OFF
			{
				Lcd_Pos(0, strlen(step_text) + 1);
				Lcd_STR("OFF");
				Lcd_Pos(1,strlen(buzzer_text) + 1);
				Lcd_STR("OFF");
				buzzer_flag = 0;
				//LCD_flag &= 0x03;
				LCD_flag &= 0x00;
				flag = 0;
				
				_delay_ms(100);
			}
	
			if(LCD_flag & 0x02)	// 부저 상태 ON
			{
				Lcd_Pos(1,strlen(buzzer_text) + 1);
				Lcd_STR("ON ");
	
				buzzer_flag = 1;
				//LCD_flag &= 0x0d;
				LCD_flag &= 0x00;
				flag = 0;
				
				_delay_ms(100);
			}
	
			if(LCD_flag & 0x04)	// 스탭 모터 우측 회전 ON
			{
				Lcd_Pos(0,strlen(step_text) + 1);
				Lcd_STR("CW ");
	
				//LCD_flag &= 0x0b;
				LCD_flag &= 0x00;
				flag = 0;
				_delay_ms(100);
			}
	
			if(LCD_flag & 0x08)	// 스탭 모터 좌측 회전 ON
			{
				Lcd_Pos(0, strlen(step_text) + 1);
				Lcd_STR("CCW");
	
				//LCD_flag &= 0x07;
				LCD_flag &= 0x00;
				flag = 0;
				_delay_ms(100);
			}
	
		}
	
		if(buzzer_flag == 1){
			
			//// for문 완료 까지 다음 동장이 안됨...
			//for(piano = 0; piano < 8; piano++){
				//
				//ICR1 = Frequency/DoReMi[piano];
				//OCR1C = ICR1/25;	//듀티비 4%
				//_delay_ms(300);
				//
			//}
			//
			//buzzer_flag = 0;
			//OCR1C = 0;
			
			ICR1 = Frequency/DoReMi[piano];
			OCR1C = ICR1/25;	//듀티비 4%
			
			piano++;
			
			if(piano > 8)
			piano = 0;
			
			_delay_ms(300);
		}
		
		ADCSRA |= 0x40;	// ADSC AD 개시(Start)
		while((ADCSRA & 0x10) == 0);	// ADIF AD다 될 때까지 기다림
		AdData = ADC_result;	// 전압이 디지털로 변환된 값 읽어오기
		
		if(AdData < 400){
			OCR1A = 255;
			//putch_Str("\n\r 255");
		}
		
		else if(AdData >= 400 && AdData < 500){
			OCR1A = 170;
			//putch_Str("\n\r 170");
		}
		
		else if(AdData >= 500 && AdData < 600){
			OCR1A = 85;
			//putch_Str("\n\r 85");
		}
		else if(AdData >= 600){
			OCR1A = 0;
			//putch_Str("\n\r 0");
		}
		_delay_ms(100);
	}

	return 0;

}

ISR(TIMER0_OVF_vect){
	cli();
	TCNT0 = 184;

	// 스탭 모터
	if(Step_flag)
		timer0Cnt++;
	if(timer0Cnt == 2){
		timer0Cnt = 0;
		
		if(dir == DIR_L){
			PORTD = Step[mot_cnt--];
			if(mot_cnt == 0)
			mot_cnt = 7;
		}
		
		else if(dir == DIR_R){
			PORTD = Step[mot_cnt++];
			if(mot_cnt > 7)
			mot_cnt = 0;
		}
	}
	sei();
}

// 모든 기능 정지
void AllStop(){
	Step_flag = 0;
	PORTD = 0;
	//buzzer_flag = 0;
	OCR1C = 0;
	LCD_flag = 0x01;
}


ISR(ADC_vect){
	cli();
	ADC_result = ADC;
	sei();
}

// 부저 작동
void BuzzerOn(){
	LCD_flag = 0x02;
}

void Step_RightSpin(void){
	Step_flag = 1;
	dir = DIR_R;
	LCD_flag = 0x04;
}

void Step_LeftSpin(void){
	Step_flag = 1;
	dir = DIR_L;
	LCD_flag = 0x08;
}