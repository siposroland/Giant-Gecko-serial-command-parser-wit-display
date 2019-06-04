#include "em_device.h"
#include "em_chip.h"
#include "em_usart.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_timer.h"
#include "em_core.h"
#include "segmentlcd.h"
#include "segmentlcd_spec.h"
#include "segmentlcdconfig.h"
#include "caplesense.h"
#include "command.h"


#define Mode_BUTTON 0
#define Mode_SLIDER 1

extern volatile bool wait4Text2Write;
extern volatile uint8_t charBuffer[100];
extern volatile uint8_t buttonValue;
extern volatile bool getMode;
extern volatile uint8_t  ch;
extern volatile bool timerIT;
extern volatile uint8_t charBuffer[100];
extern volatile bool new_char;


/* !!! INICIALIZALO FUGGVENYEK !!! */

// gombok kezelesehez a GPIO inicializalasa
void gpioSetup(void)
{
  /* Enable GPIO in CMU */
  CMU_ClockEnable(cmuClock_GPIO, true);

  /* PB) es PB10 bemeneti konfiguracioja */
  GPIO_PinModeSet(gpioPortB, 9, gpioModeInput, 0);
  GPIO_PinModeSet(gpioPortB, 10, gpioModeInput, 0);

  /* fel es lefuto elre is interrupt, igy eszlelheto az aktualis ertek */
  GPIO_IntConfig(gpioPortB, 9, true, true, true);
  GPIO_IntConfig(gpioPortB, 10, true, true, true);

  /* interruptok engedelyezese a core-ban */
  NVIC_ClearPendingIRQ(GPIO_EVEN_IRQn);
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
}

// Interruptok, Timerek es  inicializalasa
void initOthers(void){
	  // UART0
	  USART_IntEnable(UART0, USART_IF_RXDATAV);
	  NVIC_EnableIRQ(UART0_RX_IRQn);

	  // GPIO
	  gpioSetup();
	  GPIO_IntEnable(true);


	  // Timer 0, 1s LCD lepteteshez
	  TIMER_TopBufSet(TIMER0, 14000 - 1);
	  TIMER_IntClear(TIMER0, TIMER_IF_OF);
	  NVIC_EnableIRQ(TIMER0_IRQn);
	  TIMER_IntEnable(TIMER0, 1);

	  // Timer 1, slider es button beolvasas utemzese
	  NVIC_EnableIRQ(TIMER1_IRQn);
	  TIMER_TopBufSet(TIMER1, 1000);
	  TIMER_IntClear(TIMER1, TIMER_IF_OF);
	  TIMER_IntDisable(TIMER1, 1);

	  // kapacitiv touch pad
	  CAPLESENSE_Init(false);

	  // LCD kijelzo
	  SegmentLCD_Init(false);
}

/* !!! INTERRUPT FUGGVENYEK !!! */

// UART0 interrupt kezelese
void intUartValue(){
	// uj karakter beolvasasa
	ch = USART_RxDataGet(UART0);

	// jott uj karakter
	new_char = 1;
}

// Timer0 megszakitas a masodperces usztatashoz
void intTimer1Sec(){
	static uint16_t timer0Ticks;

	// szamlalo ujratoltese
	TIMER_IntClear(TIMER0, TIMER_IF_OF);

	// 1000 periodusonkent 1 jelkiadasa
	if (timer0Ticks == 1000) {
		timer0Ticks = 0;
		timerIT = 1;
	}

	// szamlalo novelese
	else
		timer0Ticks++;
}

// GPIO interrupt kezelo fuggvenye, buttonValue ertekadasa
void intButtonPro(uint8_t selBtn, uint8_t selValue) {
	// Btn1 aktiv -> also 2 helyierteken "02" jelenik meg
	// Btn0 aktiv -> felso 2 helyierteken "01" jelenik meg

	// atomikus muveletvegzes
	CORE_DECLARE_IRQ_STATE;
	CORE_ENTER_ATOMIC();

	// megszakitas eszlelese
	 GPIO_IntClear(1 << selBtn);

	// gomb kivalasztasa selBtn alapjan
	if(selBtn == 10){

		// ha az also 2 helyiertek megegyezik a jelzendo ertekkel
		if (buttonValue - ( (buttonValue / 100) * 100 ) == selValue){

			// akkor a gombot felengedtuk, a megfelelo helyierteket kivonjuk
				buttonValue = buttonValue - selValue;
		}
		else {

			// egyebkent most nyomtuk le a gombot es ezt jelezni kell
				buttonValue = buttonValue + selValue;
		}

	} // Btn10 vege

	// gomb kivalasztasa selBtn alapjan
	else if(selBtn == 9) {

		// ha a felso 2 helyiertek megegyezik a jelzendo ertekkel
		if ( ( ( buttonValue / 100) * 100) == selValue){

			// akkor a gombot felengedtuk, a megfelelo helyierteket kivonjuk{
				buttonValue = buttonValue - selValue;
		}

		else {
			// egyebkent most nyomtuk le a gombot es ezt jelezni kell
				buttonValue = buttonValue + selValue;
		}
	} // Btn9 vege

	// atomikus muveletvegzes vege
	CORE_EXIT_ATOMIC();
}

// Timer1 Slider es Button utemzo interrupt kezelo
void intSliderOrButtonTimer() {
	// Slider aktiv
		if (getMode == Mode_SLIDER){

			// LCD szovegsoraban kiirjuk a funckiot
			strcpy(charBuffer, "SLIDER2");
			printToLcd(charBuffer);

			/* elozo es aktualis slider allapot: nem iratja ki folyamatosan,
			 * ha a pozicio nem valtozik */

			int32_t sliderPos;
			int32_t oldPos;
			oldPos = -2;

			// aktualis ertek lekerdezese
			sliderPos = CAPLESENSE_getSliderPosition();

			/* kiiratas, ha valtozas van,
			 * elso  beolvasas utan mindig van valtozas */

			if (oldPos != sliderPos) {
				// aktualis allapot mentese
				oldPos = sliderPos;
				SegmentLCD_Number(sliderPos);
			}
		}	// Slider vege

		// Button aktiv
		else if (getMode == Mode_BUTTON){

				// LCD szovegsoraban kiirjuk a funckiot
				strcpy(charBuffer, "BUTTON2");
				printToLcd(charBuffer);

				// gomb ertekenek kiiratasa, beolvasas mashol
					SegmentLCD_Number(buttonValue);
			} // Button vege

		// timer visszaallitasa alapallapotba
		TIMER_IntClear(TIMER1, TIMER_IF_OF);
}

/* !!! FUNKCIONALIS FUGGVENYEK !!! */

// Kiirja az osszes ekerheto funkciot a sorosportra
void getHelp() {
	// kiirando sztringek osszeallitasa
	uint8_t str_pt1[] = {"Elerheto parancsok:\r\n"
						"- Help: kilistazza az elerheto parancsokat\r\n"
						"- Get Slider: lekerdezi a kapacitiv csuszka aktualis erteket\r\n"
						};
	uint8_t str_pt2[] = {"- Get Slider Pro: folyamtosan lekerdezi a kapacitiv csiszka erteket\r\n"
						};
	uint8_t str_pt3[] = {"- Get Button: lekerdezi a nyomogombok aktualis erteket\r\n"
						};
	uint8_t str_pt4[] = {"- Get Button Pro: folyamatosan lekerdezi a nyomogombok erteket\r\n"
						"- Write Text: az 'ENTER'-ig begepelt karaktereket megjeleniti az LCD kijelzon"
						};

	// kiiratas a soros portra
	printToSerial(str_pt1);
	printToSerial(str_pt2);
	printToSerial(str_pt3);
	printToSerial(str_pt4);
}

// gomb aktualis ertek kiiratasa
void getButton() {
	strcpy(charBuffer, "BUTTON1");
	printToLcd(charBuffer);
	SegmentLCD_Number(buttonValue);
}

// kapacitiv csuszka aktualis ertekenek kiiratasa
void getSlider() {
	int32_t sliderPos;
	strcpy(charBuffer, "SLIDER1");
	printToLcd(charBuffer);
	sliderPos = CAPLESENSE_getSliderPosition();
	SegmentLCD_Number(sliderPos);
}

// sorosporton kapott szoveg kiirasanak beallito fuggvenye
void pushText() {
	SegmentLCD_Number(0);
	printToSerial("Waiting for text to write:");
	wait4Text2Write = true;
}

// hibas parancs kiirasa soros portra
void invalidCommand() {
	SegmentLCD_Number(0);
	printToSerial("Invalid Command");
}

// sorosporton erkezo parancsok feldolgozasa
void interpretCommand(uint8_t *command) {

	//  Get Help kezelese
	if (strcmp(command, commandHelp) == 0) {
		TIMER_IntDisable(TIMER1, 1);
		getHelp();
	}

	//  Get Button kezelese
	else if (strcmp(command, commandButton) == 0) {
		GPIO_IntClear(1 << 9);
		GPIO_IntClear(1 << 10);
		TIMER_IntDisable(TIMER1, 1);
		getButton();
	}

	// Get Slider kezelese
	else if (strcmp(command, commandSlider) == 0) {
		TIMER_IntDisable(TIMER1, 1);
		getSlider();
	}

	// Write Text kezelese
	else if (strcmp(command, commandText) == 0) {
		TIMER_IntDisable(TIMER1, 1);
		pushText();
	}

	// Get Slider Pro kezelese
	else if (strcmp(command, commandSliderPro) == 0) {
		TIMER_IntClear(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, 1);
		getMode = Mode_SLIDER;
	}

	// Get Button Pro kezelese
	else if (strcmp(command, commandButtonPro) == 0) {
		TIMER_IntClear(TIMER1, TIMER_IF_OF);
		TIMER_IntEnable(TIMER1, 1);
		getMode = Mode_BUTTON;
	}

	// hibas parancs kezelese
	else {
		invalidCommand();
	}
}
