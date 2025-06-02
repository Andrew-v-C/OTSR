#include <SPI.h>

// Pins for read/write enable and LED
#define R_ENABLE 7
#define W_ENABLE 8
#define LED 9

// Sensor pins
#define minSensor A0
#define maxSensor A3

// Chip select (CS) pins
unsigned char CS;
#define minCS 2
#define maxCS 3

// Address/memory management
unsigned long address;
bool memFull;
#define memMarker 0x00000
#define minAddress 0x00008
#define maxAddress 0xFFFD7

void setup()
{
    pinMode(R_ENABLE, INPUT);
    pinMode(W_ENABLE, INPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    // Initialize SPI
    SPI.begin();
    for (int i = minCS; i <= maxCS; i++)  // Initialize memory CS pins
    {
        pinMode(i, OUTPUT);
        digitalWrite(i, HIGH);
    }
    SPI.beginTransaction(SPISettings(66000000, MSBFIRST, SPI_MODE0));
    
    // Initialize STATUS registers
    for (int i = minCS; i <= maxCS; i++)
    {
        digitalWrite(i, LOW);
        SPI.transfer(0x50);  // Instruction to enable write STATUS register
        digitalWrite(i, HIGH);
        digitalWrite(i, LOW);
        SPI.transfer(0x01);  // Instruction to write STATUS register
        SPI.transfer(0);  // Set all bits in the STATUS register to 0
        digitalWrite(i, HIGH);
    }
    
    //eraseChips();  // Un-comment this and upload to erase memory
      
    // Find starting memory location
    for (CS = minCS; CS <= maxCS; CS++)
    {
        if (read4Bytes(CS, memMarker) == 0) continue;  // Marker indicates chip memory is full
        digitalWrite(CS, LOW);
        SPI.transfer(0x03);  // Instruction to read memory
        send3Bytes(minAddress);  // Send starting address
        for (address = minAddress; address <= maxAddress; address += 2)
        {
            if (SPI.transfer16(0) == -1) break;
        }
        digitalWrite(CS, HIGH);
        if (address > maxAddress)
        {
            // If chip memory is full, set marker
            write4Bytes(CS, memMarker, 0);
        }
        else
        {
            break;
        }
    }
    memFull = (CS > maxCS);  // This means all chips are full
    
    // If read switch is enabled, read data and output to serial port
    if (digitalRead(R_ENABLE))
    {
        // Determine size (in bytes) of one data point
        unsigned char pointSize = 4;  // 4 bytes for timestamp
        for (int i = minSensor; i <= maxSensor; i++) pointSize += 2;  // 2 bytes for each sensor reading
        // Read data and output to serial port
        Serial.begin(9600);
        unsigned char readCS;
        unsigned long readAddress;
        unsigned long output;
        for (readCS = minCS; readCS <= maxCS; readCS++)
        {
            digitalWrite(CS, LOW);
            SPI.transfer(0x0B);  // Instruction to read memory (high-speed)
            send3Bytes(minAddress);  // Send address
            SPI.transfer(0);  // Send dummy byte
            for (readAddress = minAddress; readAddress <= maxAddress; readAddress += pointSize)
            {
                // Read and output timestamp
                output = 0;
                for (int i = 1; i <= 4; i++) output = (output << 8) + SPI.transfer(0);
                Serial.print(output);
                Serial.print(',');
                // Read and output each data point
                for (int i = minSensor; i <= maxSensor; i++)
                {
                    output = SPI.transfer(0);
                    output = (output << 8) + SPI.transfer(0);
                    Serial.print(output);
                    Serial.print(',');
                }
                Serial.print('\n');
            }
            digitalWrite(CS, HIGH);
        }
    }
}

void loop()
{
    if (memFull)
    {
        digitalWrite(LED, HIGH);
    }
    else if (digitalRead(W_ENABLE))
    {
        // Check address
        if (address > maxAddress)
        {
            write4Bytes(CS, memMarker, 0);  // Set marker to indicate chip memory is full
            CS++;  // Go to next chip
            address = minAddress;  // Reset address
        }
        // Check chip
        memFull = (CS > maxCS);
        // Proceed to read/write sensor data
        if (!memFull)
        {
            write4Bytes(CS, address, millis());  // Write timestamp in ms
            address += 4;
            for (int i = minSensor; i <= maxSensor; i++)
            {
                write2Bytes(CS, address, analogRead(i));  // Write sensor reading
                address += 2;
            }
            delay(1);  // Prevent multiple readings having the same timestamp
        }
    }
}

// Read four bytes from device memory
unsigned long read4Bytes(unsigned char chip, unsigned long add)
{
    unsigned long output = 0;
    digitalWrite(chip, LOW);
    SPI.transfer(0x03);  // Instruction to read memory
    send3Bytes(add);  // Send address
    for (int i = 1; i <= 4; i++)
    {
        // Read next byte and append it to output
        output = (output << 8) + SPI.transfer(0);
    }
    digitalWrite(chip, HIGH);
    return output;
}

// Write four bytes to device memory
void write4Bytes(unsigned char chip, unsigned long add, unsigned long data)
{
    writeByte(chip, add, data >> 24);
    add++;
    writeByte(chip, add, data >> 16);
    add++;
    writeByte(chip, add, data >> 8);
    add++;
    writeByte(chip, add, data);
}

// Write two bytes to device memory
void write2Bytes(unsigned char chip, unsigned long add, unsigned int data)
{
    writeByte(chip, add, data >> 8);
    add++;
    writeByte(chip, add, data);
}

// Write one byte to device memory
void writeByte(unsigned char chip, unsigned long add, unsigned char data)
{
    digitalWrite(chip, LOW);
    SPI.transfer(0x06);  // Instruction to write enable
    digitalWrite(chip, HIGH);
    digitalWrite(chip, LOW);
    SPI.transfer(0x02);  // Instruction to program one byte
    send3Bytes(add);  // Send address
    SPI.transfer(data);  // Send data byte
    digitalWrite(chip, HIGH);
}

// Send 3 bytes; useful for address
void send3Bytes(unsigned long send)
{
    SPI.transfer(send >> 16);
    SPI.transfer(send >> 8);
    SPI.transfer(send);
}

// Erase full memory array for all chips
void eraseChips()
{
    for (int i = minCS; i <= maxCS; i++)
    {
        digitalWrite(i, LOW);
        SPI.transfer(0x06);  // Instruction to write enable
        digitalWrite(i, HIGH);
        digitalWrite(i, LOW);
        SPI.transfer(0x60);  // Instruction to erase full memory array
        digitalWrite(i, HIGH);
    }
}