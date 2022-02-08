#define F_CPU 16500000UL

#define LCD_CLEAR 0b00000001

#include <xc.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

//PINS:
//RS   - PB0
//RW   - PB1
//E    - PB2
//Data - Port A

#define RS 0b1
#define RW 0b10
#define E 0b100

typedef uint8_t byte;

//spam reads the busy flag on the lcd
void lcdbusy() {
	//need to read, setting port a as input
	DDRA = 0;
	PORTB = 0;
	do {
		//setting rw, then pulsing e
		PORTB |= RW;
		PORTB |= E;
		_delay_us(5);
		PORTB &= ~E;
	} while ((PINB & 0b10000000) != 0);
	
	//setting port a back to output
	DDRA = 0xff;
	PORTB = 0;
	_delay_us(10);
}

void lcdcmd(byte cmd) {
	//wait for lcd to stop being busy
	lcdbusy();
	
	PORTB = 0;
	PORTA = cmd;
	PORTB |= E;
	PORTB = 0;
	PORTA = 0;
	
	_delay_ms(10);
}

void lcdchar(char cmd) {
	//wait for lcd to stop being busy
	lcdbusy();
	
	PORTB = 0;
	PORTA = cmd;
	PORTB |= E | RS;
	PORTB = 0;
	PORTA = 0;
	
	_delay_us(10);
}

void lcd_setddram(byte address) {
	lcdcmd(address | 0b10000000);
}

const byte prg1[] = {0b00011110, 0b11100000, 0b01010001, 0b00101110, 0b01001110, 0b01100000, 0,0,0,0,0,0,0,0,1,0};
const byte prg2[] = {0b01010001, 0b01001110, 0b01010000, 0b11100000, 0b00111110, 0b01001111, 0b00011110, 0b01001101, 0b00011111, 0b01001110, 0b00011101, 0b01110110, 0b01100011, 0, 0, 0};

//emulated registers
byte prgm = 0;
byte rega = 0;
byte regb = 0;
byte mem[16];
//byte status = 0;
byte inst = 0;
byte arg = 0;
bool zeroflag = false;
bool carryflag = false;
unsigned short alu_temp = 0;
bool clockactive = true;

void lda(byte a) {
	rega = a;
	//TODO: display updates
	lcd_setddram(0);
	lcdbusy();
	char outputBuf[4];
	itoa(rega, outputBuf, 16);
	
	lcd_setddram(0);
	lcdchar('A');
	lcdchar('=');
	if (rega > 0xf) {
		lcdchar(outputBuf[0]);
		lcdchar(outputBuf[1]);
	} else {
		lcdchar('0');
		lcdchar(outputBuf[0]);
	}
}

void ldb(byte b) {
	regb = b;
	lcdbusy();
	char outputBuf[4];
	itoa(regb, outputBuf, 16);
	lcd_setddram(5);
	lcdchar('B');
	lcdchar('=');
	if (regb > 0xf) {
		lcdchar(outputBuf[0]);
		lcdchar(outputBuf[1]);
	} else {
		lcdchar('0');
		lcdchar(outputBuf[0]);
	}
}

void jmp(byte new_addr) {
	prgm = (new_addr & 0xf);
	//TODO: display
}

int main() {
	//control lines are outputs
	DDRB |= RS | RW | E;
	DDRA = 0xff;
	
	_delay_ms(500);
	
	//lcd is busy when powered on
	lcdbusy();
	
	//sending initialization commands
	lcdcmd(0b00111000); //8 bit, 2 lines, 5x8 font
	_delay_ms(5);
	lcdcmd(0b00001110); //display on, cursor on, blink off
	_delay_ms(5);
	lcdcmd(0b00000110); //increment and shift cursor, don't shift display
	_delay_ms(5);
	lcdcmd(LCD_CLEAR); //clear display
	
	_delay_ms(10);
	
	//copying emulated program into memory
	memcpy(&mem, &prg2, 16);
	
	while (clockactive) {
		inst = (mem[prgm] & 0xf0) >> 4; //opcode is top 4
		arg = mem[prgm] & 0x0f; //arg is bottom 4
	
		//instructions
		switch (inst) {
			case 0:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
				//nop
				break;
			case 1:
				//lda
				lda(mem[arg]);
				zeroflag = (rega == 0);
				break;
			case 2:
				//add
				ldb(mem[arg]);
				alu_temp = rega + regb;
			
				//checking for carry
				if (alu_temp > 0xff) carryflag = true;
				else carryflag = false;
			
				rega = alu_temp & 0xff;
				zeroflag = (rega == 0);
			
				break;
			case 3:
				//sub
				ldb(mem[arg]);
				alu_temp = rega + (regb^0xff);
						
				//checking for carry
				if (alu_temp > 0xff) carryflag = true;
				else carryflag = false;
						
				rega = alu_temp & 0xff;
				zeroflag = (rega == 0);
						
				break;
			case 4:
				//sta
				mem[arg] = rega;
				break;
			case 5:
				//ldi
				lda(arg);
				zeroflag = (rega == 0);
				break;
			case 6:
				//jmp
				jmp(arg);
				break;
			case 7:
				//jc
				if (carryflag) jmp(arg);
				break;
			case 8:
				//jz
				if (zeroflag) jmp(arg);
				break;
			case 14:
				//out
				lcdbusy();
				char outputBuf[4];
				itoa(rega, outputBuf, 10);
				
				lcd_setddram(0x4d);
				if (rega > 99) {
					lcdchar(outputBuf[0]);
					lcdchar(outputBuf[1]);
					lcdchar(outputBuf[2]);
				} else if (rega > 9) {
					lcdchar(' ');
					lcdchar(outputBuf[0]);
					lcdchar(outputBuf[1]);
				} else {
					lcdchar(' ');
					lcdchar(' ');
					lcdchar(outputBuf[0]);
				}
				
				break;
			case 15:
				//hlt
				clockactive = false;
				break;
		}
		
		//updating flags on screen
		lcd_setddram(10);
		lcdchar('P');
		lcdchar('=');
		
		char outputBuf[4];
		itoa(prgm, outputBuf, 16);
		
		lcdchar(outputBuf[0]);
		
		if (zeroflag) lcdchar('Z');
		else lcdchar(' ');
		if (carryflag) lcdchar('C');
		else lcdchar(' ');
		//instruction is now done
		prgm++;
		if (prgm > 0xf) prgm = 0;
		
		_delay_ms(25);
	}
}