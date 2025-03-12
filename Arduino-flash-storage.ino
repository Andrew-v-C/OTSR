#include <SPI.h>

#define ENABLE 7
#define LED 9

unsigned char sensor;
#define minSensor 0
#define maxSensor 3

unsigned char CS;
#define minCS 2
#define maxCS 3

unsigned long address;
bool memFull;
#define memMarker 0x00000
#define minAddress 0x00004
#define maxAddress 0xFFFFE

void setup()
{
    Serial.begin(9600);
    pinMode(ENABLE, INPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    // Initialize SPI
    SPI.begin();
    for (int i = minCS; i <= maxCS; i++)
    {
        pinMode(i, OUTPUT);
        digitalWrite(i, HIGH);
    }
    SPI.beginTransaction(SPISettings(25000000, MSBFIRST, SPI_MODE0));
    
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
    
    Serial.println("Searching for starting memory location...");
    
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
    memFull = (CS > maxCS);

    if (memFull)
    {
        Serial.println("No memory available.");
    }
    else
    {
        Serial.print("CS: ");
        Serial.print(CS);
        Serial.print("; address: ");
        Serial.println(address, HEX);
    }
    
    // Initialize sensor
    sensor = minSensor;
}

void loop()
{
    if (memFull)
    {
        digitalWrite(LED, HIGH);
    }
    else if (digitalRead(ENABLE))
    {
        // Check address
        if (address > maxAddress)
        {
            write4Bytes(CS, memMarker, 0);  // Set marker to indicate chip memory is full
            CS++;  // Go to next chip
            address = minAddress;  // Reset address
        }
        // Check CS
        memFull = (CS > maxCS);
        // Proceed to read/write sensor data
        if (!memFull)
        {
            if (sensor > maxSensor) sensor = minSensor;  // Check/reset sensor number
            write2Bytes(CS, address, (analogRead(sensor + 14) << 4) + sensor);  // Combine reading and sensor into one 16-bit value
            address += 2;  // Increment address
            sensor++;  // Increment sensor number
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


// Testing functions --------------------------------------------------

void printBytes(unsigned char chip, unsigned long add, int num)
{
    unsigned char output;
    digitalWrite(chip, LOW);
    SPI.transfer(0x03);
    send3Bytes(add);
    for (int i = 1; i <= num; i++)
    {
        output = SPI.transfer(0);
        if (output >> 4 == 0) Serial.print('0');
        Serial.print(output, HEX);
        Serial.print(' ');
    }
    digitalWrite(chip, HIGH);
    Serial.println();
}
