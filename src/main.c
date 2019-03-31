//
// STC15 RPN calculator
//

#include "stc15.h"
#include <stdint.h>
#include "lcd.h"
#include "key.h"
#include "decn/decn.h"
#include "calc.h"
#include "utils.h"

#define FOSC 11583000


static const char KEY_MAP[20] = {
	'c', '<', 'r', 'm',
	'/', '9', '8', '7',
	'*', '6', '5', '4',
	'-', '3', '2', '1',
	'+', '=', '.', '0'
};


uint32_t NewKeyBuf[4];
volatile uint8_t new_key_write_i;
volatile uint8_t new_key_read_i;
volatile uint8_t NewKeyEmpty;
#define INCR_NEW_KEY_I(i) i = (i + 1) & 3

volatile uint8_t SecCount;
//#define TRACK_TIME
void timer0_isr() SDCC_ISR(1,1)
{
#ifdef TRACK_TIME
	static uint8_t count = 0;
	static uint8_t min_count = 0, hour_count = 0;
#endif

	uint32_t new_keys;

	//scan keyboard
	KeyScan();
	new_keys = GetNewKeys();
	if (new_keys != 0){
		if (!NewKeyEmpty && (new_key_write_i == new_key_read_i)){
			//do not overwrite keymap currently being processed
			INCR_NEW_KEY_I(new_key_write_i);
		}
		NewKeyBuf[new_key_write_i] = new_keys;
		INCR_NEW_KEY_I(new_key_write_i);
		NewKeyEmpty = 0;
	}

	//track time
#ifdef TRACK_TIME
	count++;
	if (count == 200){
		count = 0;
		SecCount++;
		if (SecCount == 60){
			SecCount = 0;
			min_count++;
			if (min_count == 60){
				min_count = 0;
				hour_count++;
			}
		}
	}
#endif
}


#ifdef DESKTOP
void Timer0Init(void) { }
static void latch_on(void){ }
#else
// Call timer0_isr() 200/sec: 5 ms period
// Initialize the timer count so that it overflows after 0.01 sec
// THTL = 0x10000 - FOSC / 200 = 0x10000 - 115830 = 7621 = 0x1DC5
void Timer0Init(void)
{
	// TMOD = 0;    // default: 16-bit auto-reload
	AUXR |= 0x80;   // use undivided SYSclk for timer0
	// Initial values of TL0 and TH0 are stored in hidden reload registers: RL_TL0 and RL_TH0
	TL0 = 0xC5;		// Initial timer value
	TH0 = 0x1D;		// Initial timer value
	TF0 = 0;		// Clear overflow flag
	TR0 = 1;		// Timer0 start run
	ET0 = 1;		// Enable timer0 interrupt
	EA  = 1;		// Enable global interrupt
}

#endif //!DESKTOP


char Buf[DECN_BUF_SIZE];
__xdata char EntryBuf[MAX_CHARS_PER_LINE + 1];
__xdata uint8_t ExpBuf[2] = {0, 0};

//#define DEBUG_UPTIME
/*********************************************/
int main()
{
	enum {
		ENTERING_DONE,
		ENTERING_SIGNIF_NOLIFT,
		ENTERING_SIGNIF,
		ENTERING_FRAC,
		ENTERING_EXP,
		ENTERING_EXP_NEG
	};
	uint8_t entry_i = 0;
	uint8_t entering_exp = ENTERING_DONE;
	uint8_t no_lift = 0;
	uint8_t exp_i = 0;
#ifdef DEBUG_KEYS
	uint8_t j = 0;
	const uint8_t* keys;
	uint8_t key_i;
#endif

#ifdef DEBUG_UPTIME
	uint32_t i;
#endif

	latch_on();
	Timer0Init(); // display refresh & switch read
	LCD_Open();
	KeyInit();
	P3_4 = 0; //turn on led backlight

	//set (P3_2) as push pull output
	P3_2 = 1; //latch on
	P3M1 &= ~(0x4);
	P3M0 |= (0x4);
	BACKLIGHT_ON(); //turn on led backlight

#ifdef DEBUG_UPTIME
	i = 0;
#endif
	// LOOP
	while (1)
	{
		//turn off?
		if (Keys[0] == 8 && Keys[4] == 8){
			TURN_OFF();
		}

		LCD_GoTo(0,0);
#ifdef DEBUG_UPTIME
		u32str(i++, Buf, 10);
		LCD_OutString(Buf, MAX_CHARS_PER_LINE);
#else
		//display y register on first line
		if (entering_exp == ENTERING_DONE){
			dec80_to_str(Buf, get_y());
		} else {
			//display x on 1st line, entered number on 2nd line
			dec80_to_str(Buf, get_x());
		}
		LCD_OutString(Buf, MAX_CHARS_PER_LINE);
#endif //DEBUG_UPTIME

#ifdef DEBUG_KEYS
		//keyboard debug
		keys = DebugGetKeys();
		for (key_i = 0; key_i < 5; key_i++){
			LCD_OutNibble(keys[key_i]);
		}
		TERMIO_PutChar(',');
		//counter
		if (SecCount == 0){
			LCD_OutString("  ");
		} else if (SecCount < 10){
			TERMIO_PutChar(' ');
			LCD_OutString(u32str(SecCount, Buf, 10));
		} else {
			LCD_OutString(u32str(SecCount, Buf, 10));
		}
#endif //DEBUG_KEYS


		///get new keys
		if (!NewKeyEmpty){
			uint8_t i_key;
			uint32_t new_keys = NewKeyBuf[new_key_read_i];
			INCR_NEW_KEY_I(new_key_read_i);
			if (new_key_read_i == new_key_write_i){
				NewKeyEmpty = 1;
			}
#ifdef DEBUG_KEYS
			LCD_GoTo(1,j);
#endif
			//find first key pressed in array (if any)
			for (i_key = 0; i_key < 20; i_key++){
				if (new_keys & ((uint32_t) 1 << i_key)){
#ifdef DEBUG_KEYS
					TERMIO_PutChar(KEY_MAP[i_key]);
					j++;
					j &= 0x0f;
#endif
					//process key
					switch(KEY_MAP[i_key]){
						//////////
						case '0': {
							if (entering_exp >= ENTERING_EXP){
								ExpBuf[exp_i] = 0;
								exp_i = (exp_i + 1) & 1;
							} else if (entering_exp == ENTERING_DONE){
								entering_exp = ENTERING_SIGNIF;
								EntryBuf[entry_i] = KEY_MAP[i_key];
								//do not increment entry_i from 0, until first non-0 entry
							} else if (entry_i != 0 && entry_i < MAX_CHARS_PER_LINE - 1 + 1){
								EntryBuf[entry_i] = KEY_MAP[i_key];
								entry_i++;
							}
						} break;
						//////////
						case '1': //fallthrough
						case '2': //fallthrough
						case '3': //fallthrough
						case '4': //fallthrough
						case '5': //fallthrough
						case '6': //fallthrough
						case '7': //fallthrough
						case '8': //fallthrough
						case '9': {
							if (entering_exp >= ENTERING_EXP){
								ExpBuf[exp_i] = KEY_MAP[i_key] - '0';
								exp_i = (exp_i + 1) & 1;
							} else if (entering_exp == ENTERING_DONE){
								entering_exp = ENTERING_SIGNIF;
								EntryBuf[entry_i] = KEY_MAP[i_key];
								entry_i++;
							} else if (entry_i < MAX_CHARS_PER_LINE - 1 + 1){
								EntryBuf[entry_i] = KEY_MAP[i_key];
								entry_i++;
							}
						} break;
						//////////
						case '.': {
							if (entering_exp == ENTERING_DONE){
								EntryBuf[entry_i++] = '0';
								EntryBuf[entry_i++] = '.';
								entering_exp = ENTERING_FRAC;
							} else if (entering_exp == ENTERING_SIGNIF){
								if (entry_i == 0){
									EntryBuf[entry_i++] = '0';
								}
								EntryBuf[entry_i++] = '.';
								entering_exp = ENTERING_FRAC;
							} else if (entering_exp <= ENTERING_EXP) {
								entering_exp++;
							} else { //entering_exp == ENTERING_EXP_NEG
								entering_exp = ENTERING_EXP;
							}
						} break;
						//////////
						case '=': {
							//track stack lift
							if (entering_exp != ENTERING_DONE){
								//finish entry
								int8_t exponent; //exponent is only 2 digits
								exponent = 10*ExpBuf[1] + ExpBuf[0];
								if (entering_exp == ENTERING_EXP_NEG){
									exponent = -exponent;
								}
								EntryBuf[entry_i] = '\0';
								push_decn(EntryBuf, exponent, no_lift);
								process_cmd(KEY_MAP[i_key]);
								//reset state as initial ENTERING_DONE state
								entering_exp = ENTERING_DONE;
								entry_i = 0;
								exp_i = 0;
								ExpBuf[0] = 0;
								ExpBuf[1] = 0;
							} else {
								//dup
								process_cmd(KEY_MAP[i_key]);
							}
							no_lift = 1;
						} break;
						//////////
						case 'c': {
							if (entering_exp == ENTERING_DONE){
								//clear
								clear_x();
								no_lift = 1;
								entering_exp = ENTERING_SIGNIF;
								EntryBuf[entry_i] = '0';
								//do not increment entry_i from 0, until first non-0 entry
							} else if (entering_exp >= ENTERING_EXP){
								//go back to digit entry
								entering_exp--;
								exp_i = 0;
								ExpBuf[0] = 0;
								ExpBuf[1] = 0;
							} else if (entry_i > 0){
								//backspace
								if (EntryBuf[entry_i] == '.'){
									entering_exp--;
								}
								entry_i--;
							}
						} break;
						//////////
						case '+': //fallthrough
						case '*': //fallthrough
						case '-': //fallthrough
						case '/': //fallthrough
						case '<': //fallthrough //use as +/-
						case 'r': { //use as swap
							if (entering_exp != ENTERING_DONE){
								//finish entry
								int8_t exponent; //exponent is only 2 digits
								exponent = 10*ExpBuf[1] + ExpBuf[0];
								if (entering_exp == ENTERING_EXP_NEG){
									exponent = -exponent;
								}
								EntryBuf[entry_i] = '\0';
								push_decn(EntryBuf, exponent, no_lift);
								process_cmd(KEY_MAP[i_key]);
								//reset state as initial ENTERING_DONE state
								entering_exp = ENTERING_DONE;
								entry_i = 0;
								exp_i = 0;
								ExpBuf[0] = 0;
								ExpBuf[1] = 0;
							} else {
								//process key
								process_cmd(KEY_MAP[i_key]);
							}
							no_lift = 0;
						} break;
						//////////
						default: process_cmd(KEY_MAP[i_key]);
						//////////
					} //switch(KEY_MAP[i_key])

					break;
				} //if found new key pressed
			} //for new key array
		} //if (!NewKeyEmpty)

		//print X
		LCD_ClearToEnd(0); //go to 2nd row
		if (entering_exp == ENTERING_DONE){
			dec80_to_str(Buf, get_x());
			LCD_OutString(Buf, MAX_CHARS_PER_LINE);
		} else if (entry_i == 0){
			TERMIO_PutChar('0');
		} else if (entering_exp < ENTERING_EXP){
			uint8_t idx;
			for (idx = 0; idx < entry_i && idx < MAX_CHARS_PER_LINE; idx++){
				TERMIO_PutChar(EntryBuf[idx]);
			}
		} else {
			uint8_t idx;
			//print significand
			for (idx = 0; idx < entry_i && idx < MAX_CHARS_PER_LINE - 3; idx++){
				TERMIO_PutChar(EntryBuf[idx]);
			}
			//go to exponent
			if (idx < MAX_CHARS_PER_LINE - 3){
				//clear until exponent
				for ( ; idx < MAX_CHARS_PER_LINE - 3; idx++){
					TERMIO_PutChar(' ');
				}
			} else {
				LCD_GoTo(1, MAX_CHARS_PER_LINE - 3);
			}
			//print exponent sign
			if (entering_exp == ENTERING_EXP_NEG){
				TERMIO_PutChar('-');
			} else {
				TERMIO_PutChar(' ');
			}
			//print exp
			TERMIO_PutChar(ExpBuf[1] + '0');
			TERMIO_PutChar(ExpBuf[0] + '0');
		}
		LCD_ClearToEnd(1);
		//turn backlight back on
		BACKLIGHT_ON();
	} //while (1)
}
/* ------------------------------------------------------------------------- */

