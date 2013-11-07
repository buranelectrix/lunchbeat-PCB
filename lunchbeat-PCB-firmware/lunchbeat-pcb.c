/*
 * =============================
 *   B U R A N E L E C T R I X 
 * =============================
 *
 *       L U N C H B E A T
 *
 *             P C B
 *
 *        1-bit groovebox 
 *
 * =============================
 *         version: 1.2
 *   target: atmega328p@16MHz
 *     (arduino compatible)
 * =============================
 *     (c) 2013 Jan Cumpelik
 * =============================
 *
 */

#include <avr/io.h>

#define F_CPU 16000000L
#include <util/delay.h>

#include <avr/interrupt.h>

#include "iolunch.h"


#define NOW 	0
#define PREV 	1
#define PLAY 	4
#define EDIT 	5
uint8_t rawbutton = 0; 
uint8_t button[2][6] = {{0,0,0,0,0,0},{0,0,0,0,0,0}} ; 	
uint8_t seq[8] = {	0b00000001,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000 };
uint8_t playing = 0;
uint8_t extsyncmode = 0;
//uint8_t trigged = 0;
uint8_t editmode = 0;
volatile uint8_t playstep = 0;
volatile uint8_t newstep = 0;
volatile uint8_t editstep = 0;
volatile uint16_t tempo = 4000;
volatile uint16_t counter = 0;
volatile uint8_t setmode = 0;
uint8_t ledbar = 0;
uint8_t ledbarhlf = 0;
uint16_t pot[5];

#define KICKPOT		4
#define SNAREPOT	3
#define HATPOT		2
#define WAVEPOT		1
#define TEMPOPOT	0

#define KICK 	0
#define SNARE 	1
#define HAT 	2
#define WAVE	3
#define TEMPO	4

uint16_t gate[4];  
uint8_t division = 1;
uint8_t subdiv = 1;

#define TEMPOLED_OFF PORTB &= ~(1 << PB1)
#define TEMPOLED_ON  PORTB |= (1 << PB1)
		

/////////////////////////////////////////////////
//
// controls()
//
// cteni tlacitek a potenciometru
// reading buttons and potentiometers
//
/////////////////////////////////////////////////

void controls() {

// cteni hodnot z potenciometru
// reading potentiometers
	static uint8_t p = 0;
	pot[p] = readpot10b((p));
	p ++;
	if (p > 4) p = 0;

	static uint8_t debcycler = 0;
	debcycler ++;
	if (debcycler & 0x01) {
	
		// snimani buttonu do registru "rawbutton"
		// capture buttons to "rawbutton" variable
		uint8_t rawbutton = ~(PIND) & 0b00011111;

		// exchange kick and hat buttons - hardware issue
		if (~(PIND) & (1<<KICK)) {rawbutton |= (1<<HAT);} else {rawbutton &= ~(1<<HAT);}
		if (~(PIND) & (1<<HAT)) {rawbutton |= (1<<KICK);} else {rawbutton &= ~(1<<KICK);}

		if (~(PINB) & (1<<PB0)) rawbutton |= 0b00100000; // pridame edit tlacitko z portu B
								 // add EDIT button from B port

		// debouncing
		uint8_t but;
		for (but = 0; but < 6; but ++) {
				button[NOW][but] <<= 1;
			if (rawbutton & (1 << but)) {
				button[NOW][but] |= 0x01;
			} else {
				button[NOW][but] &= ~(0x01);
			}
		}
	}
	

	// akce pro prave stisknuta tlacitka 
	// button sound just pressed
	uint8_t tlac = 0;
	for (tlac = 0; tlac < 6; tlac++) {
		if (tlac < 4) {
			if ((button[NOW][tlac] == 0xff) && (button[PREV][tlac] != 0xff)) {

				if (setmode) {
					uint8_t divisioncheck = division;
					if (divisioncheck & (1 << (3 -tlac))) {
						divisioncheck &= ~(1 << (3 - tlac)) ;
					} 
					else {
						divisioncheck |= (1 << (3 - tlac));
					}
					if (divisioncheck >0) division = divisioncheck;
				}
				else if (editmode) {
					if (seq[editstep] & (1 << tlac)) {
						seq[editstep] &= ~(1 << tlac);
					}
					else {
						seq[editstep] |= (1 << tlac);
					}
				} 
				else {
					gate[tlac]	= 1;
				}
			}
		}
		
		// tlacitko play (tl. 4)
		// play button
		if (tlac == 4) {
			if ((button[NOW][PLAY] == 0xff) && (button[PREV][PLAY] != 0xff)) { 
				if (setmode) {
					if (extsyncmode) {
						extsyncmode = 0;
						DDRC |= (1 << PC5);
						PORTC &= ~(1 << PC5);
					}
					else {
						extsyncmode = 1;
						DDRC &= ~(1 << PC5);
						PORTC &= ~(1 << PC5);
					}
				}
				else if (playing) {
					playing = 0;
				} 
				else {
					if (extsyncmode) {
						playstep = 7;
						subdiv = 1;
					}
					else {
						playstep = 0;
						newstep = 1;
						PORTC |= (1 << PC5);
						tempo = 5500 - (pot[TEMPOPOT] << 2);
					}	
					playing = 1;
					counter = 0;
				}
			}
		}
		
		// tlacitko edit (tl.5)
		// edit button
		if (tlac == 5) {
			if ((button[NOW][EDIT] == 0xff) && (button[PREV][EDIT] != 0xff)) {
				if (editmode) {
					editstep ++;
					if (editstep > 7) {
						editmode = 0;
						editstep = 0;
						ledbarhlf = 0;
					}
				} 
				else {
					editmode = 1;
				}
			}
		}

		button[PREV][tlac] = button[NOW][tlac];
	}
}



/////////////////////////////////////////////////
//
// lights()
//
/////////////////////////////////////////////////

void lights() {

	if (setmode) {
		ledbar = ((division) << 4);
		if (extsyncmode) ledbar |= 0b00000001;
		ledbarhlf = 0;
	}
	else if (editmode) {
		if (seq[editstep] & (1 << KICK))  {ledbar |= 0b11000000;} else {ledbar &= ~(0b11000000);}
		if (seq[editstep] & (1 << SNARE)) {ledbar |= 0b00110000;} else {ledbar &= ~(0b00110000);}
		if (seq[editstep] & (1 << HAT))   {ledbar |= 0b00001100;} else {ledbar &= ~(0b00001100);}
		if (seq[editstep] & (1 << WAVE))  {ledbar |= 0b00000011;} else {ledbar &= ~(0b00000011);}

		ledbar 		|=  (1<<(7-editstep));
		ledbarhlf  	 = ~(1<<(7-editstep));
	}
	else {
		if (gate[KICK] ) {ledbar |= 0b11000000;} else {ledbar &= ~(0b11000000);}
		if (gate[SNARE]) {ledbar |= 0b00110000;} else {ledbar &= ~(0b00110000);}
		if (gate[HAT]  ) {ledbar |= 0b00001100;} else {ledbar &= ~(0b00001100);}
		if (gate[WAVE] ) {ledbar |= 0b00000011;} else {ledbar &= ~(0b00000011);}
	}
}



/////////////////////////////////////////////////
/////////////////////////////////////////////////
//
//   M A I N 
//
/////////////////////////////////////////////////
/////////////////////////////////////////////////


int main() {

	// do setup 
	setup();
	
	DDRC |= (1 << PC5);
	PORTC &= ~(1 << PC5);

/*
	// wait until all buttons released
	uint8_t waitbutton = ~(PIND) & 0b00011111;
	if (~(PINB) & (1<<PB0)) waitbutton |= 0b00100000; // pridame edit tlacitko z portu B
	while (waitbutton) {
		waitbutton = ~(PIND) & 0b00011111;
		if (~(PINB) & (1<<PB0)) waitbutton |= 0b00100000; // pridame edit tlacitko z portu B
	}
*/

	// and go for infinite loop
	for (;;) {
		
		controls();		// read buttons and pots
		lights();		// compute which LED to light up
		
		if (newstep) {
			newstep = 0;
			uint8_t i;
			for (i = 0; i < 4; i++) {
				if (seq[playstep] & (1 << i)) gate[i] = 1;			
			}
		}
	}
}



/////////////////////////////////////////////////
/////////////////////////////////////////////////
//
//   I N T E R R U P T 
//
/////////////////////////////////////////////////
/////////////////////////////////////////////////


ISR(TIMER1_COMPA_vect) {


	// counter pocita tempo 
	// counter variable counts tempo
	if (playing) {
		counter ++;
		
		uint8_t trigpin = 0;
		static uint8_t lasttrigpin = 0;

		if (extsyncmode) { // externi clock
	
			if (PINC & (1 << PC5)) {trigpin = 1;} else {trigpin = 0;}

			// novy krok 
			// new step
			if ((trigpin == 1) && (lasttrigpin == 0)) {
				subdiv --;
				if (subdiv == 0) {
					subdiv = division ;
					counter = 0;
					newstep = 1;
					playstep ++;
					if (playstep > 7) playstep = 0;
				}
			}
			
			lasttrigpin = trigpin;
		}  
		else {  // interni clock
			
			// novy krok
			// new step
			if (counter > tempo)   {
				tempo = 5500 - (pot[TEMPOPOT] << 2);
				counter = 0;
				newstep = 1;
				playstep ++;
				if (playstep > 7) playstep = 0;

				subdiv --;
				if (subdiv == 0) {
					subdiv = division ;
					PORTC |= (1 << PC5);
				}
			}
			
			// trig out off
			if (counter > 0x8f) {  // cca 10ms
				PORTC &= ~(1 << PC5);
			}
				
		}
	}
	

	///////////////////////
	// vypocet zvuku
	// sound modelling :)

	// kick
	static uint8_t kick = 0;
	static uint16_t kickfreq = 2;
	if (gate[KICK]) {
		if (gate[KICK] == 1) { // zvuk zrovna zacal - sound starts
			kickfreq = 2;
		}
		gate[KICK] ++;
		if (gate[KICK] > ((pot[KICKPOT] << 3) + 0x01ff)) { // cas pro konec zvuku - sound ends
			gate[KICK] = 0;  // ticho
			kick = 0;
		} 
		else { // zvuk hraje - sound plays
			if (gate[KICK] == kickfreq) {
				if (kick) {
					kick = 0;
				} else {
					kick = 1;
				}
				kickfreq += (kickfreq>>4) + (64-(pot[KICKPOT]>>4));
			}
		}
	}

	//snare 
	static uint8_t snare = 0;
	static uint16_t snarefreq = 2;
	static uint16_t snarelfsr = 0b0011010010100100; // seed pro linear feedback shift register
	if (gate[SNARE]) {
		if (gate[SNARE] == 1) {
			snarefreq = 2;
		}
		gate[SNARE] ++;
		if (gate[SNARE] > ((pot[SNAREPOT]<<2) + 0x01ff)) { // cas pro konec zvuku - end of sound
			gate[SNARE] = 0;  // ticho - silence
			snare = 0;
		} else { 			// zvuk hraje - sound is playing
		// prvni cast zvuku - first part of sound
			if (gate[SNARE] < (pot[SNAREPOT] + 0x0090)) {

				if ((gate[SNARE] & 128) ||	 (!(gate[SNARE] % ((pot[SNAREPOT] >> 5) + 3)))) {
					if ( ((snarelfsr & 3) == 0) || ((snarelfsr & 3) == 3) ) {
						if (snare) {snare = 0;} else {snare = 1;}
						snarelfsr >>= 1;
						snarelfsr |= 0b1000000000000000;
					} else {
						snarelfsr >>= 1;
					}
				}
			}
			else 
			// druha cast zvuku - second part of sound
			{
				if ((gate[SNARE] & 32) ||	 (!(gate[SNARE] % ((pot[SNAREPOT] >> 4) + 5)))) {
					if ( ((snarelfsr & 3) == 0) || ((snarelfsr & 3) == 3) ) {
						if (snare) {snare = 0;} else {snare = 1;}
						snarelfsr >>= 1;
						snarelfsr |= 0b1000000000000000;
					} else {
						snarelfsr >>= 1;
					}
				}
			}
		}
	}

	//hat 
	uint8_t hat = 0;
	static uint16_t hatlfsr = 0b0100101010001010; // seed for linear feedback shift register
	if (gate[HAT]) {
		gate[HAT] ++;
		if (gate[HAT] > (pot[HATPOT] + (pot[HATPOT] >> 2) + 0x01ff)) { // cas pro konec zvuku - sound ends here
			gate[HAT] = 0;  // ticho - silence
			hat = 0;
		} else { // zvuk hraje - sound is playing
			if (!(gate[HAT] % (((pot[HATPOT] + 190) >> 6) ))) {
				if ( ((hatlfsr & 3) == 0) || ((hatlfsr & 3) == 3) ) {
					hat = 1;
					hatlfsr >>= 1;
					hatlfsr |= 0b1000000000000000;
				} else {
					hatlfsr >>= 1;
				}
			}
		}
	}
	
	//wave 
	static uint8_t wave = 0;
	static uint16_t wavefreq = 1;

	if (button[NOW][WAVE] == 0x00) {
		if (!(playing   &&    ((seq[playstep] & (1 << WAVE))))) {
			gate[WAVE] = 0;
		}
	}
	if (gate[WAVE]) {
		gate[WAVE] ++;
		if (!(gate[WAVE] % wavefreq )) {
			wavefreq = 50 + (255 - (pot[WAVEPOT] >> 2));
			gate[WAVE] = 1;
			if (wave) {wave = 0;} else {wave = 1;}
		}
	} 
	else {
		if ((!playing) || (!(seq[playstep] & (1 << WAVE)))) {
			wave = 0;
		}
	}



	// smichani vsech zvuku dohromady
	// add all sounds together
	uint8_t mix = kick + snare + hat + wave;	

	// zde uz primo vystup na prevodnik
	// prevodnik je na 5, 6 a 7 pinu portu D
	//
	// output to DAC
	// DAC is on PD5, PD6 and PD7 pins
	uint8_t portmp = (mix << 5);
	portmp |= 0b00011111;	// pullups on PD0 - PD4
	PORTD = portmp;




	//////////////////////////////////////////////////////
	// out lights
	/////////////////////////////////////////////////////
	uint8_t leds = ledbar;
	static uint8_t darkcycle = 0;
	darkcycle ++;
	if (darkcycle > 25) darkcycle = 0;
	if (darkcycle) leds &= ~(ledbarhlf);
	ledbarout(leds);

	//  tempo LED
	if (setmode) {
		static uint16_t setled = 0;
		setled ++;
		if (setled > 500) {
			setled = 0;
			if (PORTB & (1 << PB1)) {
				TEMPOLED_OFF;
			} 
			else {
				TEMPOLED_ON;
			}
		}
	}

	else {
		if (playing) {
			if (extsyncmode) {
				if (PINC & (1 << PC5)) {
					TEMPOLED_ON;
				} 
				else {
					if (darkcycle) {
						TEMPOLED_OFF;
					}
					else {
						TEMPOLED_ON;
					}
				}
			}
			else
			{
				if (counter < (tempo >> 3)) {TEMPOLED_ON;} else {TEMPOLED_OFF;}
			}
		}
		else {
			if (extsyncmode) {
				if (PINC & (1 << PC5)) {
					if (darkcycle) {
						TEMPOLED_OFF;
					}
					else {
						TEMPOLED_ON;
					}
				}
				else {
					TEMPOLED_OFF;
				}
			}
			else {
				TEMPOLED_OFF;
			}
		}
	}

	/////////////////////////////////////////////////////////
	// setup mode
	//
	static uint16_t setupcounter = 0;
	
	if (PINB & (1 << PB0)) {
		setupcounter = 0;
	} 
	else {
		setupcounter ++;
	}

	if (setupcounter == 0x7fff) {
		if (setmode) {
			setmode = 0;
			editstep = 0;
			editmode = 0;
		}
		else {
			setmode = 1;
		}
	}
}

/*
 * konec programu
 * end of sranda
 */

