#include <errno.h>

/**************************************************************************************
 * CONSTANTS - I/O PIN MAPPING
 **************************************************************************************
 *
 * A total of 35 digital pins are required to use all of the features of this tool.
 *
 * You can use any digital pin for any input/output, EXCEPT, the clock pin (PHI0).
 * PHI0 must be on an interrupt pin to use the monitor or measure clock commands,
 * and PHI0 must be on timer pin to generate a clock.
 *
 * Only pins 2 and 3 on the Arduino Mega meet both criteria
 *
 * The default mapping puts the pins (including GND) on the edge connector in the
 * same order they appear on the chip (with the exception of PHI0)
 *
 * The control inputs (READY, BE; IRQ, NMI, VPB if used), should be pulled high with a 1K+
 * resistor (not tied high with a jumper) so that the arduino can pull them low when
 * needed and they still have a signal when the arduino is disconnected.
 *
 **************************************************************************************/

#define PHI0                2
#define PHI0OUT             3     // use different pin for output if using a clock divider

#define UNUSED              36
#define VPB                 UNUSED // 22    /* unused */
#define READY               22 // 23
#define IRQ                 UNUSED // 24    /* unused */
#define NMI                 UNUSED // 25    /* unused */
#define SYNC                26 // 26

#define RW_READ             30 // 41
#define BUS_ENABLE          28 // 40
#define RESET               24 // 39

/*********************************************
 * ERROR Codes returned by various functions *
 *********************************************/

#define E_OK                0
#define E_MISSING           1
#define E_INVALID           2
#define E_NOT_SUPPORTED     3
#define E_CLOCK_DETECTED    4
#define E_OUT_OF_RANGE      5

/*****************************************************
 * Addressing modes used by the 65c02 microprocessor *
 *****************************************************/

#define AM_ABS              0     // Absolute                             a
#define AM_ABS_X_ID         1     // Absolute Indexed Indirect            (a,x)
#define AM_ABS_X            2     // Absolute Indexed with X              a,x
#define AM_ABS_Y            3     // Absolute Indexed with Y              a,y
#define AM_ABS_IDR          4     // Absolute Indirect                    (a)
#define AM_ACC              5     // Accumulator                          A
#define AM_IMM              6     // Immediate Addressing                 #
#define AM_IMP              7     // Implied or Stack                     i
#define AM_REL              8     // Program Counter Relative             r
#define AM_ZP               9     // Zero Page                            zp
#define AM_ZP_X_IDR         10    // Zero Page Indexed Indirect           (zp,x)
#define AM_ZP_X             11    // Zero Page Indexed with X             zp,x
#define AM_ZP_Y             12    // Zero Page Indexed with Y             zp,y
#define AM_ZP_IDR           13    // Zero Page Indirect                   (zp)
#define AM_ZP_IDR_Y         14    // Zero Page Indirect Indexed with Y    (zp),y

/***************************
 * Custom Type Definitions *
 ***************************/

typedef struct  {
  const char *name;
  void (*execute)(void);
} COMMAND;

struct _symbol;
typedef struct _symbol {
  const char *label;
  unsigned int address;
  _symbol *next;
} SYMBOL;

typedef struct
{
  char    mnemonic[5];
  byte    addressMode;
} OPCODE;

typedef struct
{
  char    form[14];
  byte    operandSize;
} ADDRESSMODE;

/**********************
 * Run-time constants *
 **********************/

const char DELIMS[] = " \t";

// const byte ADDR[] = { 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, /**/ 53, 52, 51, 50 };  // 0-15
const byte ADDR[] =    { 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53 };
// const byte DATA[] = { 42, 43, 44, 45, 46, 47, 48, 49 };  // 0-7
const byte DATA[] =    { 38, 40, 42, 44, 46, 48, 50, 52 }; 

// used to convert digital pins to port bitmasks
byte ADDRMASK[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
byte ADDRDATAMASK[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// 65C02 opcode table for decoding instructions while monitoring the bus

const OPCODE OPCODES[] =
{
  { "BRK ", AM_IMP }, { "ORA ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "TSB ", AM_ZP },      { "ORA ", AM_ZP },    { "ASL ", AM_ZP },    { "RMB0", AM_ZP },    // $00 - $07
  { "PHP ", AM_IMP }, { "ORA ", AM_IMM },       { "ASL ", AM_ACC },     { "??? ", AM_IMP },     { "TSB ", AM_ABS },     { "ORA ", AM_ABS },   { "ASL ", AM_ABS },   { "BBR0", AM_REL },   // $08 - $0F
  { "BPL ", AM_REL }, { "ORA ", AM_ZP_IDR_Y },  { "ORA ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "TRB ", AM_ZP },      { "ORA ", AM_ZP_X },  { "ASL ", AM_ZP_X },  { "RMB1", AM_ZP },    // $10 - $17
  { "CLC ", AM_IMP }, { "ORA ", AM_ABS_Y },     { "INC ", AM_ACC },     { "??? ", AM_IMP },     { "TRB ", AM_ABS },     { "ORA ", AM_ABS_X }, { "ASL ", AM_ABS_X }, { "BBR1", AM_REL },   // $18 - $1F
  { "JSR ", AM_ABS }, { "AND ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "BIT ", AM_ZP },      { "AND ", AM_ZP },    { "ROL ", AM_ZP },    { "RMB2", AM_ZP },    // $20 - $27
  { "PLP ", AM_IMP }, { "AND ", AM_IMM },       { "ROL ", AM_ACC },     { "??? ", AM_IMP },     { "BIT ", AM_ABS },     { "AND ", AM_ABS },   { "ROL ", AM_ABS },   { "BBR2", AM_REL },   // $28 - $2F
  { "BMI ", AM_REL }, { "AND ", AM_ZP_IDR_Y },  { "AND ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "BIT ", AM_ZP_X },    { "AND ", AM_ZP_X },  { "ROL ", AM_ZP_X },  { "RMB3", AM_ZP },    // $30 - $37
  { "SEC ", AM_IMP }, { "AND ", AM_ABS_Y },     { "DEC ", AM_ACC },     { "??? ", AM_IMP },     { "BIT ", AM_ABS_X },   { "AND ", AM_ABS_X }, { "ROL ", AM_ABS_X }, { "BBR3", AM_REL },   // $38 - $3F
  { "RTI ", AM_IMP }, { "EOR ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "EOR ", AM_ZP },    { "LSR ", AM_ZP },    { "RMB4", AM_ZP },    // $40 - $47
  { "PHA ", AM_IMP }, { "EOR ", AM_IMM },       { "LSR ", AM_ACC },     { "??? ", AM_IMP },     { "JMP ", AM_ABS },     { "EOR ", AM_ABS },   { "LSR ", AM_ABS },   { "BBR4", AM_REL },   // $48 - $4F
  { "BVC ", AM_REL }, { "EOR ", AM_ZP_IDR_Y },  { "EOR ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "EOR ", AM_ZP_X },  { "LSR ", AM_ZP_X },  { "RMB5", AM_ZP },    // $50 - $57
  { "CLI ", AM_IMP }, { "EOR ", AM_ABS_Y },     { "PHY ", AM_IMP },     { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "EOR ", AM_ABS_X }, { "LSR ", AM_ABS_X }, { "BBR5", AM_REL },   // $58 - $5F
  { "RTS ", AM_IMP }, { "ADC ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "STZ ", AM_ZP },      { "ADC ", AM_ZP },    { "ROR ", AM_ZP },    { "RMB6", AM_ZP },    // $60 - $67
  { "PLA ", AM_IMP }, { "ADC ", AM_IMM },       { "ROR ", AM_ACC },     { "??? ", AM_IMP },     { "JMP ", AM_ABS_IDR }, { "ADC ", AM_ABS },   { "ROR ", AM_ABS },   { "BBR6", AM_REL },   // $68 - $6F
  { "BVS ", AM_REL }, { "ADC ", AM_ZP_IDR_Y },  { "ADC ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "STZ ", AM_ZP_X },    { "ADC ", AM_ZP_X },  { "ROR ", AM_ZP_X },  { "RMB7", AM_ZP },    // $70 - $77
  { "SEI ", AM_IMP }, { "ADC ", AM_ABS_Y },     { "PLY ", AM_IMP },     { "??? ", AM_IMP },     { "JMP ", AM_ABS_X_ID },{ "ADC ", AM_ABS_X }, { "ROR ", AM_ABS_X }, { "BBR7", AM_REL },   // $78 - $7F
  { "BRA ", AM_REL }, { "STA ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "STY ", AM_ZP },      { "STA ", AM_ZP },    { "STX ", AM_ZP },    { "SMB0", AM_ZP },    // $80 - $87
  { "DEY ", AM_IMP }, { "BIT ", AM_IMM },       { "TXA ", AM_IMP },     { "??? ", AM_IMP },     { "STY ", AM_ABS },     { "STA ", AM_ABS },   { "STX ", AM_ABS },   { "BBS0", AM_REL },   // $88 - $8F
  { "BCC ", AM_REL }, { "STA ", AM_ZP_IDR_Y },  { "STA ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "STY ", AM_ZP_X },    { "STA ", AM_ZP_X },  { "STX ", AM_ZP_Y },  { "SMB1", AM_ZP },    // $90 - $97
  { "TYA ", AM_IMP }, { "STA ", AM_ABS_Y },     { "TXS ", AM_IMP },     { "??? ", AM_IMP },     { "STZ ", AM_ABS },     { "STA ", AM_ABS_X }, { "STZ ", AM_ABS_X }, { "BBS1", AM_REL },   // $98 - $9F
  { "LDY ", AM_IMM }, { "LDA ", AM_ZP_X_IDR },  { "LDX ", AM_IMM },     { "??? ", AM_IMP },     { "LDY ", AM_ZP },      { "LDA ", AM_ZP },    { "LDX ", AM_ZP },    { "SMB2", AM_ZP },    // $A0 - $A7
  { "TAY ", AM_IMP }, { "LDA ", AM_IMM },       { "TAX ", AM_IMP },     { "??? ", AM_IMP },     { "LDY ", AM_ABS },     { "LDA ", AM_ABS },   { "LDX ", AM_ABS },   { "BBS2", AM_REL },   // $A8 - $AF
  { "BCS ", AM_REL }, { "LDA ", AM_ZP_IDR_Y },  { "LDA ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "LDY ", AM_ZP_Y },    { "LDA ", AM_ZP_X },  { "LDX ", AM_ZP_Y },  { "SMB3", AM_ZP },    // $B0 - $B7
  { "CLV ", AM_IMP }, { "LDA ", AM_ABS_Y },     { "TSX ", AM_IMP },     { "??? ", AM_IMP },     { "LDY ", AM_ABS_Y },   { "LDA ", AM_ABS_X }, { "LDX ", AM_ABS_Y }, { "BBS3", AM_REL },   // $B8 - $BF
  { "CPY ", AM_IMM }, { "CMP ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "CPY ", AM_ZP },      { "CMP ", AM_ZP },    { "DEC ", AM_ZP },    { "SMB4", AM_ZP },    // $C0 - $C7
  { "INY ", AM_IMP }, { "CMP ", AM_IMM },       { "DEX ", AM_IMP },     { "WAI ", AM_IMP },     { "CPY ", AM_ABS },     { "CMP ", AM_ABS },   { "DEC ", AM_ABS },   { "BBS4", AM_REL },   // $C8 - $CF
  { "BNE ", AM_REL }, { "CMP ", AM_ZP_IDR_Y },  { "CMP ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "CMP ", AM_ZP_X },  { "DEC ", AM_ZP_X },  { "SMB5", AM_ZP },    // $D0 - $D7
  { "CLD ", AM_IMP }, { "CMP ", AM_ABS_Y },     { "PHX ", AM_IMP },     { "STP ", AM_IMP },     { "??? ", AM_IMP },     { "CMP ", AM_ABS_X }, { "DEC ", AM_ABS_X }, { "BBS5", AM_REL },   // $D8 - $DF
  { "CPX ", AM_IMM }, { "SBC ", AM_ZP_X_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "CPX ", AM_ZP },      { "SBC ", AM_ZP },    { "INC ", AM_ZP },    { "SMB6", AM_ZP },    // $E0 - $E7
  { "INX ", AM_IMP }, { "SBC ", AM_IMM },       { "NOP ", AM_IMP },     { "??? ", AM_IMP },     { "CPX ", AM_ABS },     { "SBC ", AM_ABS },   { "INC ", AM_ABS },   { "BBS6", AM_REL },   // $E8 - $EF
  { "BEQ ", AM_REL }, { "SBC ", AM_ZP_IDR_Y },  { "SBC ", AM_ZP_IDR },  { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "SBC ", AM_ZP_X },  { "INC ", AM_ZP_X },  { "SMB7", AM_ZP },    // $F0 - $F7 
  { "SED ", AM_IMP }, { "SBC ", AM_ABS_Y },     { "PLX ", AM_IMP },     { "??? ", AM_IMP },     { "??? ", AM_IMP },     { "SBC ", AM_ABS_X }, { "INC ", AM_ABS_X }, { "BBS7", AM_REL }    // $F8 - $FF
};

// Address mode operand formatting

const ADDRESSMODE ADDRESSMODES[] =
{
  { " $%04x", 2 },         // AM_ABS         Absolute                    a
  { " ($%04x,X)", 2 },     // AM_ABS_X_ID    Absolute Indexed Indirect   (a,x)
  { " $%04x,X", 2 },       // AM_ABS_X       Absolute Indexed with X     a,x
  { " $%04x,Y", 2 },       // AM_ABS_Y       Absolute Indexed with Y     a,y
  { " ($%04x)", 2 },       // AM_ABS_IDR     Absolute Indirect           (a)
  { " A", 0 },             // AM_ACC         Accumulator                 A
  { " #$%02hx", 1 },       // AM_IMM         Immediate Addressing        #
  { "", 0 },               // AM_IMP         Implied                     i
  { " $%02hx", 1 },        // AM_REL         Program Counter Relative    r
  { " $00%02hx", 1 },      // AM_ZP          Zero Page                   zp
  { " ($00%02hx,X)", 1 },  // AM_ZP_X_IDR    Zero Page Indexed Indirect  (zp,x)
  { " $00%02hx,X", 1 },    // AM_ZP_X        Zero Page Indexed with X    zp,x
  { " $00%02hx,Y", 1 },    // AM_ZP_Y        Zero Page Indexed with Y    zp,y
  { " ($00%02hx)", 1 },    // AM_ZP_IDR      Zero Page Indirect          (zp)
  { " ($00%02hx),Y", 1 }   // AM_ZP_IDR_Y    Zero Page Indirect Indexed  (zp),y
};

const int OPERAND_BITMASK[] =
{
  0x00,
  0x01,
  0x03
};

SYMBOL *symbols = NULL;

/********************
 * Global Variables *
 ********************/

bool internalClock = false;
int processorDisables = 0;

/******************
 * Setup Function *
 ******************/

void setup()
{
  pinMode(PHI0, INPUT);
  pinMode(READY, INPUT);
  pinMode(RW_READ, INPUT);
  pinMode(BUS_ENABLE, INPUT);
  pinMode(SYNC, INPUT);
  pinMode(RESET, INPUT);
  pinMode(IRQ, INPUT);
  pinMode(NMI, INPUT);
  pinMode(VPB, INPUT);

  for (int i = 0; i < 16; i++)
  {
    // for faster IO, read/write collections of pins as ports
    byte port = digitalPinToPort(ADDR[i]);
    ADDRMASK[port] |= digitalPinToBitMask(ADDR[i]);
    ADDRDATAMASK[port] |= digitalPinToBitMask(ADDR[i]);
  }

  for (int i = 0; i < 8; i++)
  {
    byte port = digitalPinToPort(DATA[i]);
    ADDRDATAMASK[port] |= digitalPinToBitMask(DATA[i]);
  }

  clearAddressData();

  Serial.begin(115200);

  while (!Serial)
  {
  }

  Serial.println("\x0c\x1b[2J");
  Serial.println("65C02 Tool Ready");
  Serial.println();
}

/***********************
   SERIAL I/O ROUTINES
 ***********************/

char getChar()
{
  int c = -1;

  while (c < 0)
  {
    c = Serial.read();
  }

  return c;
}

int getLine(char *pcBuffer, size_t maxLength)
{
  int i = 0;

  while (true)
  {
    char c = getChar();

    switch (c)
    {
      case 3:       // Control+C
        Serial.println("^C");
        return -1;

      case 8:       // Backspace
      case 127:     // Delete
        if (i > 0)
        {
          Serial.print("\x08 \x08");
          i--;
        }
        else
        {
          Serial.print((char) 7); // Beep
        }
        break;

      case 27:      // Escape - Start of special codes on VT family of protocols, this just reads the code and discards it.
        {
          int x = 0, y = -1;

          while (x++ < 4096 && y < 0)
          {
            y = Serial.read();
          }

          if (y == '[')
          {
            while (x++ < 4096)
            {
              y = Serial.read();

              if (0x30 <= y && y <= 0x7E)
                break;
            }
          }
          break;
        }

      case '\0':  // NUL
      case '\r':  // CR
      case '\n':  // LF
        pcBuffer[i] = '\0';

        Serial.println();

        return i;

      default:    // All other chars just add them to our keybaord buffer
        if (i + 1 == maxLength)
        {
          Serial.print("\\");
          Serial.print((char) 7);
          Serial.println();
          return -1;
        }

        Serial.print((char) c);

        pcBuffer[i++] = c;

        break;
    }
  }
}

void writef(const char *format, ...)
{
  char buffer[256];

  va_list args;

  va_start(args, format);

  int bytes = vsnprintf(buffer, sizeof(buffer), format, args);

  va_end(args);

  Serial.write(buffer, bytes);
}

void writelnf(const char *format, ...)
{
  char buffer[256];

  va_list args;

  va_start(args, format);

  vsnprintf(buffer, sizeof(buffer), format, args);

  va_end(args);

  Serial.println(buffer);
}

/********************
   String Functions
 ********************/

bool isNullOrEmpty(char const *x)
{
  return x == NULL || x[0] == '\0';
}

int stricmp(char const *a, char const *b)
{
  for (;; a++, b++)
  {
    int d = tolower((unsigned char) * a) - tolower((unsigned char) * b);

    if (d != 0 || !*a)
      return d;
  }
}

/************************************************
   Get or Set Address, Data, or Control bus pins
 ************************************************/

byte getData()
{
  byte data = 0;

  for (int i = 0; i < 8; i++)
  {
    uint8_t bit = digitalPinToBitMask(DATA[i]);

    uint8_t port = digitalPinToPort(DATA[i]);

    if (*portInputRegister(port) & bit)
      data |= 1 << i;
  }

  return data;
}

unsigned int getAddress()
{
  unsigned int address = 0;

  for (int i = 0; i < 16; i++)
  {
    uint8_t bit = digitalPinToBitMask(ADDR[i]);

    uint8_t port = digitalPinToPort(ADDR[i]);

    if (*portInputRegister(port) & bit)
      address |= 1 << i;
  }

  return address;
}

void setAddress(unsigned int address)
{
  byte oldSREG = SREG;

  cli();

  DDRA |= ADDRMASK[1];
  DDRB |= ADDRMASK[2];
  DDRC |= ADDRMASK[3];
  DDRD |= ADDRMASK[4];
  DDRE |= ADDRMASK[5];
  DDRF |= ADDRMASK[6];
  DDRG |= ADDRMASK[7];
  DDRH |= ADDRMASK[8];
  DDRJ |= ADDRMASK[10];
  DDRK |= ADDRMASK[11];
  DDRL |= ADDRMASK[12];

  for (unsigned int i = 0, j = 1; i < 16; i++, j = j << 1)
  {
    byte pin = ADDR[i];

    byte bit = digitalPinToBitMask(pin);

    volatile byte *pOut = portOutputRegister(digitalPinToPort(pin));

    if (address & j)
      *pOut |= bit;
    else
      *pOut &= ~bit;
  }

  SREG = oldSREG;
}

void setAddressData(unsigned int address, byte data)
{
  byte oldSREG = SREG;

  cli();

  DDRA |= ADDRDATAMASK[1];
  DDRB |= ADDRDATAMASK[2];
  DDRC |= ADDRDATAMASK[3];
  DDRD |= ADDRDATAMASK[4];
  DDRE |= ADDRDATAMASK[5];
  DDRF |= ADDRDATAMASK[6];
  DDRG |= ADDRDATAMASK[7];
  DDRH |= ADDRDATAMASK[8];
  DDRJ |= ADDRDATAMASK[10];
  DDRK |= ADDRDATAMASK[11];
  DDRL |= ADDRDATAMASK[12];

  for (unsigned int i = 0, j = 1; i < 16; i++, j = j << 1)
  {
    byte pin = ADDR[i];

    byte bit = digitalPinToBitMask(pin);

    volatile byte *pOut = portOutputRegister(digitalPinToPort(pin));

    if (address & j)
      *pOut |= bit;
    else
      *pOut &= ~bit;
  }

  for (unsigned int i = 0, j = 1; i < 8; i++, j = j << 1)
  {
    byte pin = DATA[i];

    byte bit = digitalPinToBitMask(pin);

    volatile byte *pOut = portOutputRegister(digitalPinToPort(pin));

    if (data & j)
      *pOut |= bit;
    else
      *pOut &= ~bit;
  }

  SREG = oldSREG;
}

void clearAddressData()
{
  byte oldSREG = SREG;

  cli();

  DDRA &= ~ADDRDATAMASK[1];
  DDRB &= ~ADDRDATAMASK[2];
  DDRC &= ~ADDRDATAMASK[3];
  DDRD &= ~ADDRDATAMASK[4];
  DDRE &= ~ADDRDATAMASK[5];
  DDRF &= ~ADDRDATAMASK[6];
  DDRG &= ~ADDRDATAMASK[7];
  DDRH &= ~ADDRDATAMASK[8];
  DDRJ &= ~ADDRDATAMASK[10];
  DDRK &= ~ADDRDATAMASK[11];
  DDRL &= ~ADDRDATAMASK[12];

  PORTA &= ~ADDRDATAMASK[1];
  PORTB &= ~ADDRDATAMASK[2];
  PORTC &= ~ADDRDATAMASK[3];
  PORTD &= ~ADDRDATAMASK[4];
  PORTE &= ~ADDRDATAMASK[5];
  PORTF &= ~ADDRDATAMASK[6];
  PORTG &= ~ADDRDATAMASK[7];
  PORTH &= ~ADDRDATAMASK[8];
  PORTJ &= ~ADDRDATAMASK[10];
  PORTK &= ~ADDRDATAMASK[11];
  PORTL &= ~ADDRDATAMASK[12];

  SREG = oldSREG;
}

bool isReading()
{
  return digitalRead(RW_READ);
}

void setRead()
{
  pinMode(RW_READ, OUTPUT);

  digitalWrite(RW_READ, HIGH);
}

void setWrite()
{
  pinMode(RW_READ, OUTPUT);

  digitalWrite(RW_READ, LOW);

}

void clearReadWrite()
{
  pinMode(RW_READ, INPUT);
}

bool isOpCodeFetch()
{
  return digitalRead(SYNC);
}

/*********
   Reset
 *********/

bool tryReset()
{
  if (!waitForClock(HIGH, 1000) || !waitForClock(LOW, 1000))
    return false;

  pinMode(RESET, OUTPUT);

  digitalWrite(RESET, LOW);

  // Reset needs to be held low for at least 2 clock cycles, we'll do 4 just to be safe
  for (int i = 0; i < 4; i++)
  {
    if (!waitForClock(HIGH, 1000) || !waitForClock(LOW, 1000))
      return false;
  }

  pinMode(RESET, INPUT);

  return true;
}

/**************
   Clock Sync
 **************/

const long clockSelectPrescaler[] = { 0, 1, 8, 64, 256, 1024 };

int startInternalClock(long frequencyInHz)
{
  uint8_t timer = digital_pin_to_timer_PGM[PHI0OUT];

  if (timer != TIMER3B && timer != TIMER3C)
     return E_NOT_SUPPORTED;

  if (frequencyInHz == 0)
  {
    stopInternalClock();
    return E_OK;
  }

  if (frequencyInHz > F_CPU / 4)
    return E_OUT_OF_RANGE;

  if (!internalClock && waitForClock(HIGH, 2000) && waitForClock(LOW, 2000))
    return E_CLOCK_DETECTED;

  byte clockSelect;
  long period;

  for (clockSelect = 1; clockSelect < 6; clockSelect++)
  {
    period = F_CPU / 2 / clockSelectPrescaler[clockSelect] / frequencyInHz - 1;

    if (period < 65536)
      break;
  }

  switch (timer)
  {
    case TIMER3B:
      TCCR3A = 0x10;
      break;

    case TIMER3C:
      TCCR3A = 0x04;
      break;
  }

  TCCR3B = 0x08 | clockSelect;

  OCR3A = period;
  OCR3B = 0;
  OCR3C = 0;

  internalClock = true;

  pinMode(PHI0OUT, OUTPUT);

  return E_OK;
}

void stopInternalClock()
{
  pinMode(PHI0, INPUT);

  TCCR3A = 0x01;
  TCCR3B = 0x03;

  OCR3A = 0xFFFF;
  OCR3B = 0;
  OCR3C = 0;

  internalClock = false;
}

bool waitForClock(byte value, unsigned long timeout)
{
  uint8_t bit = digitalPinToBitMask(PHI0);
  uint8_t port = digitalPinToPort(PHI0);
  timeout += millis();

  if (value)
  {
    while ((*portInputRegister(port) & bit) == 0)
    {
      if (millis() > timeout)
        return false;
    }
  }
  else
  {
    while (*portInputRegister(port) & bit)
    {
      if (millis() > timeout)
        return false;
    }
  }

  return true;
}

/*************
   Processor
 *************/

void disableProcessor()
{
  if (processorDisables++ > 0)
    return;

  digitalWrite(READY, LOW);

  digitalWrite(BUS_ENABLE, LOW);

  pinMode(READY, OUTPUT);

  pinMode(BUS_ENABLE, OUTPUT);
}

void enableProcessor()
{
  if (--processorDisables > 0)
    return;

  digitalWrite(BUS_ENABLE, HIGH);

  digitalWrite(READY, HIGH);

  pinMode(BUS_ENABLE, INPUT);

  pinMode(READY, INPUT);
}

/***************************************
   Peek & Poke a single memory address
 ***************************************/

bool peek(unsigned int address, byte &data)
{
  bool result = false;

  disableProcessor();

  if (waitForClock(LOW, 1000))
  {
    setRead();

    setAddress(address);

    if (waitForClock(HIGH, 1000))
    {
      data = getData();

      result = true;
    }

    clearAddressData();

    clearReadWrite();
  }

  enableProcessor();

  return result;
}

bool poke(unsigned int address, byte data)
{
  bool result = false;

  disableProcessor();

  if (waitForClock(LOW, 1000))
  {
    setWrite();

    setAddressData(address, data);

    if (waitForClock(HIGH, 1000) && waitForClock(LOW, 1000))
      result = true;

    clearReadWrite();

    clearAddressData();
  }

  enableProcessor();

  return result;
}

/****************************************************************************************************************

   ISR for watching the bus on each clock tick, decoding instructions along the way.

   The limiting factor for this is how fast it can output to the serial port. Although there can be up to 25
   bytes to write in a single clock cycle of the 6502, there shouldn't be more that 38 bytes written for any 2
   consecutive clock cycles. The RS232 serial protocal requires 10 bits per byte (1 start bit, 8 data bits, 1
   stop bit) so that equates to 380 bits per two clock cycles or roughly 190 bits/cycle in the worst case.

   At the default 115200 baud we'll start to miss clock cycles around 600 Hz

   At the maximum 2000000 baud we hit that limit with a clock around 10 kHz

 ****************************************************************************************************************/

unsigned int currentOpAddress = 0;
byte currentOpCode = 0;
byte currentAddressMode = 0;

unsigned int operand = 0;
int operandSize = 0;
int operandBytesNeeded = -1;

int consecutiveActionCount = 0;
unsigned int lastAddress;
byte lastAction = -1;

void onClock()
{
  delayMicroseconds(100);

  unsigned int address = getAddress();
  bool reading = isReading();
  bool opCodeFetch = isOpCodeFetch();
  byte data = getData();
  char *label;

  // First output what happened (byte read or write)

  byte currentAction = reading ? 0 : 1;

  if (opCodeFetch || lastAction != currentAction || address != lastAddress + 1)
  {
    Serial.println();
    if (opCodeFetch) {
      label = maybeLabel(address);
      if (label) {
        writef("%s:", label);
        Serial.println();
      }
    }
    consecutiveActionCount = 1;
    writef("%c %04x %c %02hx", opCodeFetch ? '*' : ' ', address, reading ? 'r' : 'W', data);
  }
  else
  {
    consecutiveActionCount++;
    writef(" %02hx", data);
  }

  lastAction = currentAction;
  lastAddress = address;

  // If this was the fetching of a new opcode, make a note of the opcode

  if (opCodeFetch)
  {
    currentOpAddress = address;
    currentAddressMode = OPCODES[data].addressMode;
    currentOpCode = data;

    operand = 0;
    operandSize = ADDRESSMODES[currentAddressMode].operandSize;
    operandBytesNeeded = OPERAND_BITMASK[operandSize];
  }

  // If we're reading a byte that's part of the operand record that.

  if (reading && operandSize > 0 && currentOpAddress < address && address <= (currentOpAddress + operandSize))
  {
    switch (address - currentOpAddress)
    {
      case 1:
        operandBytesNeeded &= ~1;
        operand |= data;
        break;

      case 2:
        operandBytesNeeded &= ~2;
        operand |= data << 8;
        break;
    }
  }

  // If we have the full instruction, decode it and print it

  ADDRESSMODE am = ADDRESSMODES[currentAddressMode];

  if (operandBytesNeeded == 0)
  {
    Serial.write("           ", 11 - consecutiveActionCount * 3);
    Serial.write(OPCODES[currentOpCode].mnemonic);

    if (currentAddressMode == AM_REL)
      operand = currentOpAddress + ((char) operand) + 2;

    writef(am.form, operand);

    if(currentAddressMode != AM_IMM && am.operandSize) {
      label = maybeLabel(operand);
      if (label) writef("    ; %s", label);
    }

    operandBytesNeeded = -1;
    lastAction = -1;
  }
}

/*******************************
   Measure the clock frequency

   This is limited by the clock on the Arduino's ability to handle the interrupt on each clock tick.

   On an Arduino with a clock of 16 MHz, this will accurately measure the speed up to  100 kHz

 *******************************/

volatile unsigned long tickCount = 0;

void countTick()
{
  tickCount++;
}

void measureClock()
{
  attachInterrupt(digitalPinToInterrupt(PHI0), countTick, RISING);

  tickCount = 0;
  unsigned long startTime = micros();

  delay(2000);

  unsigned long elapsedCount = tickCount;
  unsigned long elapsedTime = micros() - startTime;

  detachInterrupt(digitalPinToInterrupt(PHI0));

  if (elapsedTime < 1980000 || elapsedCount > 202000)
  {
    Serial.println("500 INTERNAL ERROR - The clock is too fast to accurately measure");
  }
  else
  {
    double frequency = 1000000.0 * elapsedCount / elapsedTime;

    char str_frequency[16];

    dtostrf(frequency, 1, 0, str_frequency);

    writelnf("200 OK - Counted %ld clock pulses in %ld ms for a clock frequency of %s Hz",
             elapsedCount, elapsedTime / 1000, str_frequency);
  }
}

/****************************
   Command argument parsing
 ****************************/

int TryParseUInt8(byte &result)
{
  char *pcArg = strtok(NULL, DELIMS), *s=NULL;

  if (pcArg == NULL || strlen(pcArg) == 0)
    return E_MISSING;

  long value = strtol(pcArg, &s, 16);

  if (s == pcArg || value < 0x00 || value > 0xFF)
    return E_INVALID;

  result = value;

  return E_OK;
}

int TryParseUInt16(unsigned int &result)
{
  char *pcArg = strtok(NULL, DELIMS), *s=NULL;

  if (pcArg == NULL || strlen(pcArg) == 0)
    return E_MISSING;

  long value = strtol(pcArg, &s, 16);

  if (s == pcArg || value < 0x0000 || value > 0xFFFF)
    return E_INVALID;

  result = value;

  return E_OK;
}

int TryParseInt32(long &result)
{
  char *pcArg = strtok(NULL, DELIMS), *s=NULL;

  if (pcArg == NULL || strlen(pcArg) == 0)
    return E_MISSING;


  long value = strtol(pcArg, &s, 0);

  if (s == pcArg)
    return E_INVALID;

  result = value;

  return E_OK;
}

int TryParseByteArray(byte *pBuffer, int &length)
{
  if (pBuffer == NULL || length == 0)
    return E_INVALID;

  char *pcArg = strtok(NULL, DELIMS);

  if (pcArg == NULL || strlen(pcArg) == 0)
    return E_MISSING;

  int stringLength = strlen(pcArg);

  if (stringLength % 2 > 0 || stringLength > length * 2)
    return E_INVALID;

  length = stringLength >> 1;

  for (int i = 0; i < length; i++)
  {
    byte nibble = ntob(pcArg[i * 2]);

    if (nibble == 0xFF)
      return E_INVALID;

    pBuffer[i] = nibble << 4;

    nibble = ntob(pcArg[i * 2 + 1]);

    if (nibble == 0xFF)
      return E_INVALID;

    pBuffer[i] |= nibble;
  }

  return E_OK;
}

byte ntob(char c)
{
  if ('0' <= c && c <= '9')
    return c - '0';

  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;

  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;

  return 0xff;
}

/************
   Commands
 ************/

const COMMAND commands[] = {
  { "help", help },

  { "reset", reset },

  { "disableProcessor", disableProcessor },
  { "enableProcessor", enableProcessor },

  { "setAddress", setAddress },

  { "measureClock", measureClock },
  { "startClock", startClock },
  { "stopClock", stopClock },

  { "peek", peek },
  { "poke", poke },

  { "monitor", monitor },
  { "al", addLabel },

  { NULL, NULL }
};

void help()
{
  Serial.println("200 OK - Available Commands:");

  for (int i = 0; commands[i].name != NULL; i++)
  {
    Serial.print('\t');
    Serial.println(commands[i].name);
  }
}

void reset()
{
  if (tryReset())
  {
    Serial.println("200 OK");
  }
  else
  {
    Serial.println("500 INTERNAL ERROR - Timeout while waiting for clock");
  }
}

void startClock()
{
  long frequency = 0;

  if (TryParseInt32(frequency) != E_OK || frequency <= 0)
  {
    Serial.println("400 BAD REQUEST - Frequency was missing or invalid");
    return;
  }

  switch (startInternalClock(frequency))
  {
    case E_OK:
      writelnf("200 OK - Generating internal clock at %ld Hz", frequency);
    return;

    case E_NOT_SUPPORTED:
      Serial.println("500 INTERNAL ERROR - The clock pin has been changed to an unsupported pin. Generating an internal clock is only supported on Timer 3 on channel A or B.");
    return;

    case E_CLOCK_DETECTED:
      Serial.println("409 CONFLICT - An external clock has been detected.");
      return;

    case E_OUT_OF_RANGE:
      Serial.println("400 BAD REQUEST - The frequency requested exceeds the maximum frequency of the clock generator.");
      return;
  }

  Serial.println("500 INTERNAL ERROR - An unexpected error occurred");
}

void stopClock()
{
  bool wasClockRunning = internalClock;

  stopInternalClock();

  if (wasClockRunning)
  {
    Serial.println("200 OK - Clock has been stopped");
  }
  else
  {
    Serial.println("200 OK - Internal clock was not running");
  }
}

void setAddress()
{
  unsigned int address = 0;

  if (TryParseUInt16(address) != E_OK)
  {
    Serial.println("400 BAD REQUEST - Address was missing or invalid");
    return;
  }

  byte data = 0;

  switch (TryParseUInt8(data))
  {
    case E_OK:
      setAddressData(address, data);
      writelnf("200 OK - Address set to 0x%04x (%u), Data set to 0x%02hx (%hu)", address, address, data, data);
      break;

    case E_MISSING:
      setAddress(address);
      writelnf("200 OK - Address set to 0x%04x (%u)", address, address);
      break;

    case E_INVALID:
      Serial.println("400 BAD REQUEST - Data value was invalid");
      break;
  }
}

void peek()
{
  unsigned int address = 0;

  if (TryParseUInt16(address) != E_OK)
  {
    Serial.println("400 BAD REQUEST - Address was missing or invalid");
    return;
  }

  byte data;

  if (peek(address, data))
  {
    if (' ' <= data && data <= '~')
    {
      writelnf("200 OK - 0x%04x: 0x%02hx (%hu) '%c'", address, data, data, data);
    }
    else
    {
      writelnf("200 OK - 0x%04x: 0x%02hx (%hu)", address, data, data);
    }
  }
  else
  {
    Serial.println("500 INTERNAL ERROR - Timeout while waiting for clock");
  }
}

void poke()
{
  unsigned int address = 0;

  if (TryParseUInt16(address) != E_OK)
  {
    Serial.println("400 BAD REQUEST - Address was missing or invalid");
    return;
  }

  byte data = 0;

  if (TryParseUInt8(data) != E_OK)
  {
    Serial.println("400 BAD REQUEST - Data was missing or invalid");
    return;
  }

  if (poke(address, data))
  {
    Serial.println("200 OK");
  }
  else
  {
    Serial.println("500 INTERNAL ERROR - Timeout while waiting for clock");
  }
}

void monitor()
{
  Serial.println("200 OK - Monitoring bus, send any key to stop...");

  attachInterrupt(digitalPinToInterrupt(PHI0), onClock, RISING);

  getChar();

  detachInterrupt(digitalPinToInterrupt(PHI0));
}

char* maybeLabel(unsigned int address) {
  SYMBOL *sym = symbols;
  while (sym && sym->address != address) sym = sym->next;
  return sym ? sym->label : NULL;
}

void addLabel()
{
  /* al 1fa7 .fubar */
  unsigned int address = 0;

  if (TryParseUInt16(address) != E_OK) {
    Serial.println("400 BAD REQUEST - Address was missing or invalid");
    return;
  }

  char* label = strtok(NULL, DELIMS);

  SYMBOL *sym = (SYMBOL*)malloc(sizeof(SYMBOL));
  sym->label = strdup(label);
  sym->address = address;
  sym->next = symbols;
  symbols = sym;
}

void loop()
{
  Serial.print("> ");

  char buffer[256];

  int length = getLine(buffer, 256);

  if (length <= 0) return;

  char* cmd = strtok(buffer, DELIMS);

  if (isNullOrEmpty(cmd)) return;

  bool commandFound = false;

  for (int i = 0; commands[i].name != NULL; i++)
  {
    if (stricmp(cmd, commands[i].name) != 0) continue;

    commandFound = true;

    commands[i].execute();
  }

  if (!commandFound)
  {
    Serial.print("Unknown Command: ");
    Serial.println(cmd);
    Serial.println("Type 'help' for a list of commands.");
  }

  Serial.println();
}
