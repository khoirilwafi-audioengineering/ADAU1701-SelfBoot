/*
 * koneksi arduino ke board dsp
 * 
   DSP  | Arduino
   -----+--------
   SDA  |   9
   SCL  |   10
   WP   |   11
   RST  |   12
   GND  |   GND

*/

#include <EEPROM24.h>
#include <I2CMaster.h>
#include <SoftI2C.h>

#define SOFT_SDA  9
#define SOFT_SCL  10
const int write_protect_pin = 11;
const int reset_pin = 12;

SoftI2C i2c(SOFT_SDA,SOFT_SCL); // SDA=10, SCL=11
EEPROM24 eeprom(i2c,EEPROM_24LC256);

uint8_t in_buffer[128]; // Largest possible page size.
size_t in_buffer_index = 0;
uint32_t dst_address = 0;
int rx_state = 0;
uint32_t timeout = 0;
const uint32_t baudrate = 9600;
uint32_t bytes_received = 0;
boolean busy = false;

// States for main loop
enum states{
  STATE_RESET_DSP,
  STATE_RUNNING
} availableStates;

uint8_t state = STATE_RESET_DSP;

void print_hex_char(uint8_t ch)
{
  Serial.print((ch>>4)&0x0f,HEX);
  Serial.print(ch&0x0f,HEX);
}

void print_buffer(uint8_t *p_data, size_t data_size)
{
  for (uint8_t i=0; i<data_size; i++)
  {
    print_hex_char(p_data[i]);
  }
  Serial.println();
}

uint8_t hex_to_bin(uint8_t ch)
{
  if (ch>='0' && ch<='9') return ch - '0'; // 0-9
  else if (ch>='A' && ch<='F') return ch - 'A' + 10; // 10-15
  else if (ch>='a' && ch<='f') return ch - 'a' + 10; // 10-15
  return 0;
}

void resetDSP(void)
{ 
  // Disconnect all signals
  pinMode(write_protect_pin, INPUT);
  pinMode(SOFT_SDA, INPUT);
  pinMode(SOFT_SCL, INPUT);
  
  // Perform Reset
  pinMode(reset_pin, OUTPUT);
  digitalWrite(reset_pin,0);
  delay(250);
  pinMode(reset_pin, INPUT);
}

void initVariables(void)
{
 // Reinitialize variables
  busy = false;
  rx_state = 0;
  in_buffer_index = 0;
  dst_address = 0;
  bytes_received = 0;
  timeout = millis();  
}

void setup(void)
{
  Serial.begin(baudrate);
  while (!Serial) {
    ; // wait for serial port to connect.
  }
  Serial.println("DSP EEPROM Programmer");
  Serial.print("eeprom ");
  if (eeprom.available()==false) Serial.print("tidak ");
  Serial.println("ditemukan");
  
  state = STATE_RESET_DSP;
}

void loop(void)
{
  switch(state)
  {
    case STATE_RESET_DSP:
      initVariables();
      resetDSP();
      Serial.println("sedang menunggu file E2Prom.Hex ...");
      state = STATE_RUNNING;
      break;
      
    case STATE_RUNNING:
        if (busy==true && millis()-timeout>500)
        {
          // Serial port idle too long, consider file transfer done.
          // Write any remaining data to eeprom.
          eeprom.write(dst_address,in_buffer,in_buffer_index);
          Serial.print("Received ");
          Serial.print(bytes_received);
          Serial.println(" characters");
          Serial.print("Programmed ");
          Serial.print(dst_address+in_buffer_index);
          Serial.println(" bytes");
          Serial.println();
          
          // Read programmed data back to serial port.
          Serial.println("EEPROM contents:");
          uint16_t i = 0;
          while (i<dst_address+in_buffer_index)
          {
            eeprom.read(i,in_buffer,8);
            print_buffer(in_buffer,8);
            i += 8;
            if (i%eeprom.pageSize()==0) Serial.println();
          }
          
          // Get ready for a new file.
          state = STATE_RESET_DSP;
        }
        while (Serial.available()!=0)
        {
          if(!busy)
          {
            busy = true;
            initVariables();
            pinMode(SOFT_SDA, OUTPUT);
            pinMode(SOFT_SCL, OUTPUT);
            pinMode(reset_pin, OUTPUT);
            digitalWrite(reset_pin,LOW);
            pinMode(write_protect_pin, OUTPUT);
            digitalWrite(write_protect_pin,LOW);
          }
          busy = true;
          timeout = millis();
          uint8_t ch = Serial.read();
          bytes_received += 1;
          switch (rx_state)
          {
            case 0:
              if (ch=='x') rx_state = 1;
              else if (ch==0x0d || ch==0x0a) Serial.write(ch);
              break;
              
            case 1:
              in_buffer[in_buffer_index] = hex_to_bin(ch) << 4;
              rx_state = 2;
              break;
              
            case 2:
              in_buffer[in_buffer_index] += hex_to_bin(ch);
              print_hex_char(in_buffer[in_buffer_index]);
              in_buffer_index += 1;
              if (in_buffer_index==eeprom.pageSize())
              {
                eeprom.write(dst_address,in_buffer,in_buffer_index);
                dst_address += in_buffer_index;
                in_buffer_index = 0;
                Serial.println();
              }
              rx_state = 0;
              break;
          }
        }
      break;
    default:
      state = STATE_RESET_DSP;
      break;
  }
  
  
}
