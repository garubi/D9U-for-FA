////////////////////////////////////////////////////////////////////
// Crumar Drawbar Controller D9U 
// A unofficial firmware for Crumar D9U for controlling the drawbars 
// of the Roland FA worktations (FA-06, FA-07, FA-08) tonewheel engine.
// https://github.com/garubi/D9U-for-FA
//
// by Stefano Garuti based on original sketch by Guido Scognamiglio
// and other snippets found on the web 
//
// Sends MIDI SySEx values according to Roland FAxx specifications
//
// Runs on Atmel ATmega32U4 Arduino Leonardo (with MIDI USB Library)
// Reads 9 analog inputs from internal ADCs
//
// Last update: September 2018
//
// The usual disclaimer: 
// I'm not involved in any way with Crumar. 
//
// This sketch is totally untested and is provided "as is", without 
// warranty of any kind under the terms of the 
// MIT license: https://mit-license.org/

////////////////////////////////////////////////////////////////////
// This is where you can define the SysEx values for the FA Drawbars
int SysExMap[9] = 
{ 
  0x2A, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22
};

////////////////////////////////////////////////////////////////////
// You should not modify anything else below this line 
// unless you know what you're doing.
////////////////////////////////////////////////////////////////////

// Define I/O pins
#define LED_RED     15
#define LED_GREEN   16
#define BUTTON      5

// Define global modes
#define DEBOUNCE_TIME 150
#define DEADBAND    8

// Include libraries
#include <EEPROM.h>
#include <MIDIUSB.h>
#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

// Init global variables
int mode = 1; // Should be either 0 or 1
int prev_val[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int debounce_timer = DEBOUNCE_TIME;

// ADC reference map
int ADCmap[9] = { A0, A1, A2, A3, A6, A7, A8, A9, A10 };
int ADCcnt = 0;

// Called then the pushbutton is depressed
void set_mode()
{
    digitalWrite(LED_RED, mode ? LOW : HIGH);
    digitalWrite(LED_GREEN, mode ? HIGH : LOW);
    EEPROM.write(0x01, mode);
}

// Called to Calculate the SysEx string and Roland checksum
void SendMidiSysEx(int channel, int parameter, int value)
{
    int SysexLenght = 14;
    // This is SysEx message with default values that will be customized along the script
    uint8_t data[SysexLenght] = {0xF0, 0x41, 0x10, 0x00, 0x00, 0x77, 0x12, 0x19, 0x02, 0x00, 0x00, 0x00, 0x00, 0xF7 };

    // Here we have the value for data byte #7 and #8 for each of the 16 midi channels
    byte partsA[16] = {0x19, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C, 0x1C };
    byte partsB[16] =  {0x02, 0x22, 0x42, 0x62, 0x02, 0x22, 0x42, 0x62, 0x02, 0x22, 0x42, 0x62, 0x02, 0x22, 0x42, 0x62 };
    data[7] = partsA[channel];
    data[8] = partsB[channel];
    data[10] = parameter;
    data[11] = value;

    // Roland FAs expect the drawbars values to be between 0 and 8, not 0-127
    data[11] = map(value, 0, 127, 0, 8);

    /*
     * calculate Roland CheckSum
     * */
    int address = data[7] + data[8] + data[9] + data[10] + data[11] ;
    int remainder = address % 128;
    data[12] = 128 - remainder;

    MIDI.sendSysEx(SysexLenght, data);
    MidiUSB_sendSysEx(data, SysexLenght);
}

// sending sysex via MidiUSB here is not as easy as I was thinking... because SysEx is variable lenght... 
// the following function is copied from: https://github.com/arduino-libraries/MIDIUSB/issues/19#issuecomment-252320075 
void MidiUSB_sendSysEx(const uint8_t *data, size_t size)
{
    if (data == NULL || size == 0) return;

    size_t midiDataSize = (size+2)/3*4;
    uint8_t midiData[midiDataSize];
    const uint8_t *d = data;
    uint8_t *p = midiData;
    size_t bytesRemaining = size;

    while (bytesRemaining > 0) {
        switch (bytesRemaining) {
        case 1:
            *p++ = 5;   // SysEx ends with following single byte
            *p++ = *d;
            *p++ = 0;
            *p = 0;
            bytesRemaining = 0;
            break;
        case 2:
            *p++ = 6;   // SysEx ends with following two bytes
            *p++ = *d++;
            *p++ = *d;
            *p = 0;
            bytesRemaining = 0;
            break;
        case 3:
            *p++ = 7;   // SysEx ends with following three bytes
            *p++ = *d++;
            *p++ = *d++;
            *p = *d;
            bytesRemaining = 0;
            break;
        default:
            *p++ = 4;   // SysEx starts or continues
            *p++ = *d++;
            *p++ = *d++;
            *p++ = *d++;
            bytesRemaining -= 3;
            break;
        }
    }
    MidiUSB.write(midiData, midiDataSize);
}

// Called to check whether a drawbar has been moved
void DoDrawbar(int d, int value)
{
  // Get difference from current and previous value
  int diff = abs(value - prev_val[d]);
  
  // Exit this function if the new value is not within the deadband
  if (diff <= DEADBAND) return;
  
  // Store new value
  prev_val[d] = value;    

  // Get the 7 bit value
  int val7bit = value >> 3;
  
  // Send Midi 
 // SendMidiCC(mode > 0 ? 1 : 0, CCMap[mode][d], val7bit);
  SendMidiSysEx(mode > 0 ? 1 : 0, SysExMap[d], val7bit);
}

// The setup routine runs once when you press reset:
void setup() 
{
  // Initialize serial MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);  

  // Set up digital I/Os
  pinMode(BUTTON, INPUT_PULLUP); // Button
  pinMode(LED_RED, OUTPUT);      // Led 1
  pinMode(LED_GREEN, OUTPUT);    // Led 2
  
  // Recall mode from memory and set
  // Make sure mode is either 0 or 1
  mode = EEPROM.read(0x01) > 0 ? 1 : 0;
  set_mode();
}

// The loop routine runs over and over again forever:
void loop() 
{
  // Read analog inputs (do the round robin)
  DoDrawbar(ADCcnt, analogRead(ADCmap[ADCcnt]));
  if (++ADCcnt > 8) ADCcnt = 0;

  // Read Button
  if (digitalRead(BUTTON) == LOW)
  {
    if (debounce_timer > 0) --debounce_timer;
  } else {
    debounce_timer = DEBOUNCE_TIME;
  }
  
  if (debounce_timer == 2) 
  {
    mode = !mode; // Reverse
    set_mode(); // and Set!
  }
}
