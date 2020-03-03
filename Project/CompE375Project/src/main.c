/*/////////////////////////////////////////////////////////////
//		This project is property of Andres Martinez Paz.
//		To be used in CompE375, San Diego State University
//
//		I/O ports:
//			PORTD0 and PORTD1 will be used for the USART transmission.
//			Alternatively, PORTD0 - PORTD3 would have been used for LCD display data bits
//			
//			PORTD4 through PORTB3 will be used for keypad multiplexer
//			PORTC0 - PORTC2 would have been used for LCD display's Register Select, RW and Enable bits
//
//			PORTC3, PORTC4, and PORTC5 will be used for LEDs
//			PORTC3 - Red
//			PORTC4 - Green
//			PORTC5 - Yellow
//
/////////////////////////////////////////////////////////////*/

#define F_CPU 16000000UL
#define FOSC 16000000UL // Clock Speed
#define BAUD 9600 //define BAUD at 9600 bits per sec
#define MYUBRR FOSC/16/BAUD-1
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "interrupt\interrupt_avr8.h"

//Variables to be used in the program.
volatile unsigned int currentState = 2;
volatile char passwordBuffer[4] = "";
volatile char password[4] = "1234";
volatile unsigned int Counter = 0;

static const PassCodeLength = 4;

//This array contains the values from the keypad multiplexer, mapped to work with my readkey function
static const char* keyArray = "123A456B789C*0#D";

//this array works as an enum. we keep track of the current stte using an integer,
// and we map it to the actual state from this array
static const char *States[] = {
		"idleLocked",
		"lockedAttempt",
		"idleUnlocked",
		"unlockedNewPassword"
	};

int readKey();

//prototype for function transmit
void transmit(char a);

void transmitString(char *a);
//prototype for USART initialization function
void USART_init(unsigned int ubrr);

int main(void){
	//set the output/input Data directio registers and ports
	DDRD &= ~(0xF0);
	PORTD |= (0xF0);

	DDRB &= ~(0xF0);
	PORTB &= ~(0xF0);

	DDRB |= 1<<DDRB5;

	sei();  //global interrupt enable
	TIMSK1 |= (1 << OCIE1A);  //enable timer compare interrupt
	
	OCR1A = 0xBB80;   //set the compare A register to trigger every 6ms
	TCCR1B |= (1 << WGM12 | 1 << CS10);  //set the timer
	TCCR1B &= ~(1 << CS11);
	TCCR1B &= ~(1 << CS12);

	
	
	//Set LED ports as output
	DDRC |= (1 << DDRC3 | 1 << DDRC4 | 1 << DDRC5);

	//initialize USART
	USART_init(MYUBRR);
	char key;

	while(1){ //infinite while loop
		//check to see what the current state is
		if(States[currentState]=="idleLocked"){
			//clear our buffers
			key = '\0';
			memset(&passwordBuffer[0], 0, sizeof(passwordBuffer));

			//turn off all three LEDs
			PORTC &= ~(1 << PORTC3);
			PORTC &= ~(1 << PORTC4);
			PORTC &= ~(1 << PORTC5);
			
			//scan keypad
			key = keyArray[readKey()];

			//if we detect a key being pressed, w change states to get out of idle state
			if(key){
				currentState = 1;
			}
		}else if(States[currentState]=="lockedAttempt"){
			//clear the buffers again
			key = '\0';
			memset(&passwordBuffer[0], 0, sizeof(passwordBuffer));

			//Transmit a string prompting user to input the password
			transmitString("Enter Passcode:\r\n");

			//turn green and red LEDs off, and yellow LED on
			PORTC &= ~(1 << PORTC3);
			PORTC &= ~(1 << PORTC4);
			PORTC |= (1 << PORTC5);

			//keep reading keys and putting them in the buffer
			int i = 0;
			while(strlen(passwordBuffer) < PassCodeLength){
				//while(i < 4){
				key = keyArray[readKey()];
				if(key){
					passwordBuffer[i]= key;
					transmit(key);
					i = i + 1;
				}
			}

			//Compare buffer to actual Password
			int j;
			int correct = 1;
			for(j = 0; j < PassCodeLength; j++){

				//if any of the characters is different, set incorrect flag
				if(password[j]!=passwordBuffer[j]){
					correct = 0;
				}
			}

			//check if buffer was correct
			if(correct){
				//transmit correct message and change state to idle unlocked
				transmitString("\r\nCorrect!\r\n");
				currentState = 2;
			}else{
				//else transmit incorrect passcode message and flash the red LED
				transmitString("\r\nIncorrect passcode\r\n");
				int c;
				for(c = 0; c < 5; c++){
					PORTC |= 1<<PORTC3;
					_delay_ms(400);
					PORTC &= ~(1<<PORTC3);
					_delay_ms(400);
				}
			}
			//clear the buffers
			key = '\0';
			memset(&passwordBuffer[0], 0, sizeof(passwordBuffer)); // Clear buffer
		}else if(States[currentState]=="idleUnlocked"){
			//clear the buffers
			key = '\0';
			memset(&passwordBuffer[0], 0, sizeof(passwordBuffer));

			//turn green LED on while mechanism is locked
			PORTC &= ~(1 << PORTC3);
			PORTC &= ~(1 << PORTC5);
			PORTC |= (1 << PORTC4);

			//keep scanning key while we are in this state
			key = keyArray[readKey()];
			if(key){
				//once we get a key, change state to stop being idle
				currentState = 3;
			}
		}else{	//"unlockedNewPassword"
			//clear buffers
			key = '\0';
			memset(&passwordBuffer[0], 0, sizeof(passwordBuffer));
			
			//prompt user to enter a new passcode and turn on yellow LED while we set new password
			transmitString("\r\nEnter New Passcode:\r\n");
			PORTC &= ~(1 << PORTC3);
			PORTC &= ~(1 << PORTC4);
			PORTC |= (1 << PORTC5);

			//keep scanning and storing into buffer
			int i = 0;
			while(strlen(passwordBuffer) < PassCodeLength){
				//while(i < 4){
				key = keyArray[readKey()];
				if(key){
					passwordBuffer[i]= key;
					transmit(key);
					i = i + 1;
				}
			}

			//when buffer is full, ask user if they want to set the password in the buffer
			transmitString("\r\n");
			transmitString("New passcode selected: ");
			transmitString(passwordBuffer);
			transmitString("\r\nPress # to set new passcode.\r\n");

			//keep scanning keypad until user chooses an option
			do{
				key = keyArray[readKey()];
			}while(!key);

			//if they press pound, password is set, change state and flash all three LEDs several times
			if(key == '#'){
				transmitString("New passcode set!\r\n");
				currentState = 0;
				int c;
				for(c = 0; c < 5; c++){
					PORTC |= 1<<PORTC3;
					PORTC |= 1<<PORTC4;
					PORTC |= 1<<PORTC5;
					_delay_ms(400);
					PORTC &= ~(1<<PORTC3);
					PORTC &= ~(1<<PORTC4);
					PORTC &= ~(1<<PORTC5);
					_delay_ms(400);
				}
				strncpy(password, passwordBuffer, PassCodeLength);
			}else{
				//else discard the buffer and go to idle state
				transmitString("Passcode has been discarded.\r\n");
				currentState = 2;
			}
			//clear buffer
			key = '\0';
			memset(&passwordBuffer[0], 0, sizeof(passwordBuffer));
		}
	}

}

int readKey(){
	int i;
	int j;

	for(i = 0; i < 4; i++){
		//set all pins to input, high-z state
		DDRB |= (1 << i); //set the pin for the particular, individual column as output
		for(j = -1; j < 4; j++){
			if (j != -1){
				if(!(PIND & (0x10 << j))){ //check if row/col combination is pressed
					
					while(!(PIND & (0x10 << j)));
					Counter = 0;
					return ((j*4)+i); //return value of row/col combo (0 - 15)
				}
			}
		}
		DDRB &= ~(1 << i);
	}
	return 16; //return 16 if no switch was pressed

}

//function transmit
void transmitString(char *a){
	//set indexing variable i to zero
	int i = 0;

	//iterate over array
	while(a[i]){
		transmit(a[i]);
		i = i+1;
	}
}

//function transmit
void transmit(char a){
	//wait for incoming bit
	while(!(UCSR0A & 0x20));

	//transmit char
	UDR0 = a;
}

//This function initializes the USART in the uController
void USART_init(unsigned int ubrr){
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (3<<UCSZ00);
}

//ISR for timer compare interrupt
ISR(TIMER1_COMPA_vect){

	//Check if our global counter has arrived to 4000
	if(Counter>4000){
		//this interrupt only affects the program if it is in state 1 o state 2
		if(currentState == 1){

			//This would have cleared the LCD display
			PORTB ^= 1 << PORTB5; //toggle AVR LED instead, to make sure it works

		}else if(currentState == 3){

			//This would have cleared the LCD display
			PORTB ^= 1 << PORTB5; //toggle AVR LED instead, to make sure it works
		}
		Counter = 0; //set the counter variable back to zero
	}

	Counter++;
}
