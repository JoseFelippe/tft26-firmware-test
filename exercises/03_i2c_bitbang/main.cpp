// =============================================================================
//  Exercise 03 — I2C Sensors (Bit-bang)
// =============================================================================
//
//  Virtual hardware:
//    P8 (SCL)  →  io.digital_write(8, …) / io.digital_read(8)
//    P9 (SDA)  →  io.digital_write(9, …) / io.digital_read(9)
//
//  PART 1 — TMP64 temperature sensor at I2C address 0x48
//    Register 0x0F  WHO_AM_I   — 1 byte  (expected: 0xA5)
//    Register 0x00  TEMP_RAW   — 4 bytes, big-endian int32_t, milli-Celsius
//
//  PART 2 — Unknown humidity sensor (same register layout, address unknown)
//    Register 0x0F  WHO_AM_I   — 1 byte
//    Register 0x00  HUM_RAW    — 4 bytes, big-endian int32_t, milli-percent
//
//  Goal (Part 1):
//    1. Implement an I2C master via bit-bang on P8/P9.
//    2. Read WHO_AM_I from TMP64 and confirm the sensor is present.
//    3. Read TEMP_RAW in a loop and print the temperature in °C every second.
//    4. Update display registers 6–7 with the formatted temperature string.
//
//  Goal (Part 2):
//    5. Scan the I2C bus (addresses 0x08–0x77) and print every responding address.
//    6. For each unknown device found, read its WHO_AM_I and print it.
//    7. Add the humidity sensor to the 1 Hz loop: read HUM_RAW and print %RH.
//
//  Read README.md before starting.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdio>
#include <cstdint>
#include <cstring>

// Global I/O instance -- see trac_fw_io.hpp for API details
trac_fw_io_t io;

// Definição de pinos para SCL e SDA
#define SCL 8
#define SDA 9

// Funções auxiliares para controle dos pinos SCL e SDA
void scl_high() { io.digital_write(SCL, 1); }    // SCL = 1
void scl_low()  { io.digital_write(SCL, 0); }   // SCL = 0
void sda_high() { io.digital_write(SDA, 1); }    // SDA = 1 (liberado)
void sda_low()  { io.digital_write(SDA, 0); }   // SDA = 0
int  sda_read() { return io.digital_read(SDA); }    // lê nível lógico de SDA (0 ou 1)

void i2c_delay() { io.delay(1); } // ajuste conforme datasheet timing

// Funções de alto nível para operações I2C
// 1. Gerar condição de START
void i2c_start() {
    sda_high(); scl_high(); i2c_delay();    // Garantir que SCL e SDA estão altos antes de iniciar START
    sda_low(); i2c_delay();                 // START: SDA de 1 -> 0 enquanto SCL = 1
    scl_low(); i2c_delay();                 // SCL pode ser mantido baixo após START, ou seja, não é necessário setar SCL = 0 aqui
}

// 2. Gerar condição de STOP
void i2c_stop() {
    sda_low(); scl_high(); i2c_delay(); // STOP: SDA de 0 -> 1 enquanto SCL = 1
    sda_high(); i2c_delay();            // Garantir que SDA termina em nível alto (bus liberado)
}

// 3. Escrever um byte e ler ACK/NACK
bool i2c_write_byte(uint8_t data) {
    // Enviar os 8 bits, do MSB para o LSB
    for (int i = 0; i < 8; i++) {
        (data & 0x80) ? sda_high() : sda_low(); // setar SDA de acordo com o bit mais significativo
        scl_high(); i2c_delay();                // Gerar pulso de clock para cada bit
        scl_low(); i2c_delay();
        data <<= 1;                             // Shift para o próximo bit (da esquerda para a direita)
    }
    // ACK
    sda_high();                                 // liberar SDA para o dispositivo responder
    scl_high(); i2c_delay();                    // Gerar pulso de clock para ler ACK/NACK
    bool ack = !sda_read();                     // ACK é 0 (SDA puxado para baixo pelo dispositivo), NACK é 1 (SDA permanece alto)
    scl_low(); i2c_delay();                     // Finalizar pulso de clock
    return ack;                                 // Retornar true se ACK recebido, false se NACK
}

// 4. Ler um byte e enviar ACK/NACK
uint8_t i2c_read_byte(bool ack) {
    // Ler os 8 bits, do MSB para o LSB
    uint8_t data = 0;
    sda_high();                                     // liberar SDA para leitura
    for (int i = 0; i < 8; i++) {
        scl_high(); i2c_delay();                    // Gerar pulso de clock para cada bit
        data = (data << 1) | (sda_read() ? 1 : 0);  // Shift para o próximo bit e ler o valor de SDA
        scl_low(); i2c_delay();                     // Finalizar pulso de clock
    }
    // ACK/NACK
    ack ? sda_low() : sda_high();                   // enviar ACK (0) ou NACK (1) para o dispositivo
    scl_high(); i2c_delay();                        // Gerar pulso de clock para enviar ACK/NACK
    scl_low(); i2c_delay();                         // Finalizar pulso de clock
    sda_high();                                     // liberar SDA após enviar ACK/NACK
    return data;                                    // Retornar o byte lido
}

// Leitura de registros específicos dos sensores usando as funções I2C implementadas
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg) {
    i2c_start();                        // Iniciar comunicação

    // Enviar endereço do dispositivo com bit de escrita (0) para indicar que queremos escrever o número do registro
    if (!i2c_write_byte((addr << 1) | 0)) {
        i2c_stop();                     // Se o dispositivo não responder com ACK
        return 0xFF;                    // Finalizar comunicação e retornar 0xFF para indicar erro
    }

    i2c_write_byte(reg);                // Enviar número do registro que queremos ler
    i2c_start();                        // Repetir condição de START para iniciar a leitura do registro
    
    // Enviar endereço do dispositivo com bit de leitura (1) para indicar que queremos ler o valor do registro
    if (!i2c_write_byte((addr << 1) | 1)) {
        i2c_stop();                     // Se o dispositivo não responder com ACK
        return 0xFF;                    // Finalizar comunicação e retornar 0xFF para indicar erro
    }
    
    uint8_t val = i2c_read_byte(false); // Ler o byte do registro e enviar NACK para indicar que não queremos ler mais bytes
    i2c_stop();                         // Finalizar comunicação
    return val;                         // Retornar o valor lido do registro
}

int32_t i2c_read32(uint8_t addr, uint8_t reg) {
    i2c_start();                                    // Iniciar comunicação

    // Enviar endereço do dispositivo com bit de escrita (0) para indicar que queremos escrever o número do registro
    if (!i2c_write_byte((addr << 1) | 0)) {
        i2c_stop();                                 // Se o dispositivo não responder com ACK
        return 0;                                   // Finalizar comunicação e retornar 0 para indicar erro
    }

    i2c_write_byte(reg);                            // Enviar número do registro que queremos ler
    i2c_start();                                    // Repetir condição de START para iniciar a leitura do registro

    // Enviar endereço do dispositivo com bit de leitura (1) para indicar que queremos ler o valor do registro
    if (!i2c_write_byte((addr << 1) | 1)) {
        i2c_stop();                                 // Se o dispositivo não responder com ACK
        return 0;                                   // Finalizar comunicação e retornar 0 para indicar erro
    }

    int32_t val = 0;    // Ler os 4 bytes do registro (big-endian) e montar o valor inteiro
    for (int i = 0; i < 4; i++) {
        val = (val << 8) | i2c_read_byte(i < 3);    // Ler cada byte e enviar ACK para os primeiros 3 bytes
                                                    // NACK para o último byte
    }
    i2c_stop();                                     // Finalizar comunicação
    return val;                                     // Retornar o valor lido do registro
}

// Função para escrever os valores formatados de temperatura e umidade nos registradores de display (4–7)
// O formato é uma string de 8 caracteres, por exemplo " 23.456" para temperatura ou " 45.678%" para umidade,
// onde os espaços são usados para alinhar à direita. A string é dividida em dois registradores de 4 bytes cada (big-endian).
// Display de temperatura: registradores 6 (parte alta) e 7 (parte baixa)
void display_temperature(float t) {
    char buf[9] = {};
    std::snprintf(buf, sizeof(buf), "%8.3f", t);
    uint32_t r6, r7;
    std::memcpy(&r6, buf + 0, 4);
    std::memcpy(&r7, buf + 4, 4);
    io.write_reg(6, r6);
    io.write_reg(7, r7);
}

// Display de umidade: registradores 4 (parte alta) e 5 (parte baixa)
void display_humidity(float h) {
    char buf[9] = {};
    std::snprintf(buf, sizeof(buf), "%7.3f%%", h);
    uint32_t r4, r5;
    std::memcpy(&r4, buf + 0, 4);
    std::memcpy(&r5, buf + 4, 4);
    io.write_reg(4, r4);
    io.write_reg(5, r5);
}

// Função para varrer o barramento I2C e descobrir o endereço do HMD10
uint8_t discover_hmd10() {
    printf("Varrendo I2C bus para encontrar HMD10...\n");
    // O endereço do HMD10 é desconhecido, mas sabemos que ele responde a um WHO_AM_I diferente de 0xA5.
    // Vamos varrer os endereços possíveis (0x08 a 0x77) e ler o WHO_AM_I de cada dispositivo encontrado.    
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        i2c_start();                                // Iniciar comunicação para verificar se há um dispositivo respondendo
                                                    // no endereço atual
        bool ack = i2c_write_byte((addr << 1) | 0); // Enviar endereço com bit de escrita (0) para verificar se o
                                                    // dispositivo responde (ACK)
        i2c_stop();                                 // Finalizar comunicação após verificar o endereço
        if (ack) {  // Se o dispositivo respondeu com ACK, ler o WHO_AM_I para identificar se é o HMD10
            uint8_t who = i2c_read_reg(addr, 0x0F); // Ler o WHO_AM_I do dispositivo encontrado para identificar se é o HMD10
            printf("Addr 0x%02X WHO_AM_I 0x%02X\n", addr, who); // Imprimir o endereço e o valor do WHO_AM_I 
                                                                // para cada dispositivo encontrado
            return addr;                                        // Retornar o endereço do primeiro dispositivo encontrado
                                                                // (assumindo que é o HMD10, já que o TMP64 tem WHO_AM_I 0xA5)
        }
    }
    return 0;
}

// Função principal
int main() {
    io.set_pullup(SCL, 1); // Ativar pull-up interno para SCL
    io.set_pullup(SDA, 1); // Ativar pull-up interno para SDA

    printf("Iniciando firmware...\n");

    // TMP64 check
    uint8_t who_tmp = i2c_read_reg(0x48, 0x0F);
    bool tmp64_ok = (who_tmp == 0xA5);
    if (!tmp64_ok) {
        printf("Erro: TMP64 nao encontrado ou WHO_AM_I inesperado (0x%02X)\n", who_tmp);
        return 1; // Encerrar o programa se o TMP64 não estiver presente, já que é o sensor principal para a leitura de temperatura
    } else {
        printf("TMP64 WHO_AM_I 0x%02X\n", who_tmp);
    }

    // Discover HMD10
    uint8_t hum_addr = discover_hmd10();
    if (hum_addr) {
        printf("HMD10 Addr 0x%02X\n", hum_addr);
    } else {
        printf("HMD10 nao encontrado na varredura de enderecos I2C\n");
    }

    uint32_t last = io.millis();
    while (true) {
        if (io.millis() - last >= 1000) {
            last = io.millis();

            // Temperatura
            if (tmp64_ok) {
                int32_t raw_t = i2c_read32(0x48, 0x00);
                float temp_c = raw_t / 1000.0f;
                printf("Temp: %.3f C\n", temp_c);
                display_temperature(temp_c);
            } else {
                display_temperature(0.0f); // Limpar display de temperatura se o sensor não estiver presente
                printf("TMP64 nao disponivel, pulando leitura de temperatura\n");
            }

            // Umidade
            if (hum_addr) {
                int32_t raw_h = i2c_read32(hum_addr, 0x00);
                float hum_pct = raw_h / 1000.0f;
                printf("Hum: %.3f %%\n", hum_pct);
                display_humidity(hum_pct);
            } else {
                display_humidity(0.0f);
                printf("Umidade nao disponivel.\n");
            }
        }
    }
}
