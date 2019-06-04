#include "print.h"
#include "command.h"
#include "em_chip.h"
#include "em_usart.h"
#include "em_timer.h"
#include "string.h"

extern volatile bool wait4Text2Write;
extern volatile bool cycleStatus;
extern volatile uint8_t cyclePos;
extern volatile uint8_t text2Cycle[104];
volatile uint8_t temp[7];

// kiiratas a soros portra
void printToSerial(volatile uint8_t *charBuffer) {
	uint8_t i = 0;
	while (charBuffer[i] != '\0') {
		USART_Tx(UART0, charBuffer[i]);
		++i;
	}
}

// kiiratas az LCD kijelzore
void printToLcd(volatile uint8_t *charBuffer) {
	uint8_t len = strlen(charBuffer);
	if (len > 7) {
		// ha nem fér ki egybe:
		// engedélyezzük a szöveg úsztatását
		cycleStatus = 1;
		clearBuffer(text2Cycle, 100);
		// bemásoljuk temp-be és a cycleBufferbe
		strcpy(text2Cycle, charBuffer);
		strcat(text2Cycle, "    ");
		cyclePos = 0;
		for(uint8_t i = 0; i < 7; ++i) {
			temp[i] = text2Cycle[i];
			cyclePos++;
		}
		SegmentLCD_Write(temp);
		TIMER_CounterSet(TIMER0, 0);
	}
	else {
		// ha kifér csak kiírjuk
		cycleStatus = 0;
		SegmentLCD_Write(charBuffer);
	}
}

// hozzadadas a bufferhez
void addToBuffer(volatile uint8_t *charBuffer, uint8_t ch) {
	uint8_t i = 0;
	while (charBuffer[i] != '\0')
		++i;
	// backspace lekezelése
	if (ch == '\177') {
		if (i != 0) {
			// töröljük az utolsó karaktert a bufferben
			charBuffer[i-1] = '\0';
		}
	}
	else {
		// char bufferbe és lezárás
		charBuffer[i] = ch;
		charBuffer[i+1] = '\0';
	}
}

// karakter gettere
void getChar(volatile uint8_t *charBuffer, uint8_t ch) {
	// begepelt karakter echo
	// ha üres a buffer ne törölje ki a sor elejérõl '>>' karaktereket
	if ( !( ch == '\177' && charBuffer[0] == '\0' ) ) {
		USART_Tx(UART0, ch);
	}
	// karakter bufferhez adása
	addToBuffer(charBuffer, ch);
}

// uj karakter erkezesenek kezelo fuggvenye
void newChar(volatile uint8_t *charBuffer, uint8_t ch) {
	if (ch != '\r') {
		// ha nem entert nyomtak
		getChar(charBuffer, ch);
	}
	else if (ch == '\r') {
		enterPressed(charBuffer);
	}
}

// enter erzekelese a soros porton
void enterPressed(volatile uint8_t *charBuffer) {
	if (!wait4Text2Write) {
		// ha nem kiírni való szöveg
		newLine();
		interpretCommand(charBuffer);
	}
	else {
		printToLcd(charBuffer);
		wait4Text2Write = false;
	}
	clearBuffer(charBuffer, 100);
	wait4Command();
}

// buffer nullazasa
void clearBuffer(volatile uint8_t *charBuffer, uint8_t len) {
	uint8_t i = 0;
	for(i = 0; i < len; ++i) {
		charBuffer[i] = '\0';
	}
}

// >> jelek kiiratasa
void wait4Command() {
	printToSerial("\r\n>>");
}

// uj sor kezdese
void newLine() {
	printToSerial("\r\n");
}

// szoveg usztatasa LCD-n
void cycleText() {
	// left shift register
	for(uint8_t i = 1; i < 7; ++i) {
		temp[i-1] = temp[i];
	}
	// text2Cycle elemeivel töltjük
	temp[6] = text2Cycle[cyclePos];
	if (text2Cycle[cyclePos + 1] == '\0')
		cyclePos = 0;
	else
		cyclePos++;
	SegmentLCD_Write(temp);
}
