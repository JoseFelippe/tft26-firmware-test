// =============================================================================
//  Exercise 01 — Parts Counter
// =============================================================================
//
//  Virtual hardware:
//    SW 0        →  io.digital_read(0)        Inductive sensor input
//    Display     →  io.write_reg(6, …)        LCD debug (see README for format)
//                   io.write_reg(7, …)
//
//  Goal:
//    Count every part that passes the sensor and show the total on the display.
//
//  Read README.md before starting.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdint>
#include <atomic>

// Global I/O object — see trac_fw_io.hpp for details.
trac_fw_io_t io;

// Macros do Sistema
#define DEBOUNCE_RISING_MS 4    // Tempo de debounce para subida do sensor
#define DEBOUNCE_FALLING_MS 4   // Tempo de debounce para descida do sensor
#define sw0 io.digital_read(0)  // Leitura do sensor SW0 (inductive sensor input)

// Variáveis globais
uint32_t count_ms = 0;      // Contador de milissegundos
uint32_t count_pecas = 0;   // Contador de peças
char buf[9] = {};           // Buffer para formatação da contagem (8 dígitos + null terminator)
uint32_t r6, r7;            // Variáveis para armazenar os valores a serem escritos nos registradores 6 e 7

// Estados da máquina de estados
static enum { WAITING_FOR_RISING, WAITING_FOR_FALLING } state = WAITING_FOR_RISING;

// Variáveis atômicas para sinalizar eventos de subida e descida do sensor
std::atomic<bool> sw0_rising{false};
std::atomic<bool> sw0_falling{false};

// Função para lidar com a interrupção de subida do sensor
void sw0_rising_handler(void) {
    sw0_rising.store(true); // Sinaliza que ocorreu uma subida no sensor
}

// Função para lidar com a interrupção de descida do sensor
void sw0_falling_handler(void) {
    sw0_falling.store(true); // Sinaliza que ocorreu uma descida no sensor
}

// Função para atualizar o display com a contagem atual
void display_update(uint32_t count) {

    io.detach_interrupt(0); // desativa a interrupção para evitar múltiplas contagens do mesmo objeto                

    std::snprintf(buf, sizeof(buf), "%8u", count);
    std::memcpy(&r6, buf + 0, 4);
    std::memcpy(&r7, buf + 4, 4);
    io.write_reg(6, r6);
    io.write_reg(7, r7);

    if (state == WAITING_FOR_RISING) {
        io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING); // ativa a interrupção para o próximo objeto
    } else {
        io.attach_interrupt(0, sw0_falling_handler, InterruptMode::FALLING); // ativa a interrupção para o próximo objeto
    }   
}

// Função da máquina de estados para gerenciar as transições entre os estados de espera por subida e descida do sensor
void MaquinaEstados() {
    switch (state) {
        case WAITING_FOR_RISING:
            if (sw0_rising.load()) {

                sw0_rising.store(false); // Reseta o sinalizador de subida do sensor

                if (count_ms > DEBOUNCE_RISING_MS) {
                    count_ms = 0; // Reseta o contador para a nova peça

                    count_pecas++;    // Incrementa o contador de peças

                    io.attach_interrupt(0, sw0_falling_handler, InterruptMode::FALLING); // reativa a interrupção para o próximo objeto
                    state = WAITING_FOR_FALLING;
                } else {
                    io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING); // reativa a interrupção para o próximo objeto
                }
            }
            break;

        case WAITING_FOR_FALLING:
            if (sw0_falling.load()) {

                sw0_falling.store(false); // Reseta o sinalizador de descida do sensor
            
                if (count_ms > DEBOUNCE_FALLING_MS) {
                    count_ms = 0; // Reseta o contador para a nova peça

//                    count_pecas++;    // Incrementa o contador de peças

                    io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING); // reativa a interrupção para o próximo objeto
                    state = WAITING_FOR_RISING;
                } else {
                    io.attach_interrupt(0, sw0_falling_handler, InterruptMode::FALLING); // reativa a interrupção para o próximo objeto
                }
            }
            break;
    }
}

// Função principal
int main() {
    // Configura a interrupção para o sensor SW0 (inductive sensor input) para detectar a borda de subida
    io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING);

    uint32_t count_atual = 0; // Variável para armazenar a contagem atual de peças
    display_update(count_atual); // Inicializa o display com a contagem inicial (0)

    while (true) {
        MaquinaEstados();

        if (count_pecas != count_atual) {
            count_atual = count_pecas; // Atualiza a contagem atual
            display_update(count_atual); // Atualiza o display com a nova contagem
        }

        io.delay(1); // Pequeno delay para evitar uso excessivo da CPU
        count_ms++; // Incrementa o contador de milissegundos (remover esta linha para a contagem real)
    }

    return 0;
}

/*
#include <trac_fw_io.hpp>
#include <cstdint>
#include <atomic>

// Global I/O object — see trac_fw_io.hpp for details.
trac_fw_io_t io;

// Macros do Sistema
#define DEBOUNCE_RISING_MS 4    // Tempo de debounce para subida do sensor
#define DEBOUNCE_FALLING_MS 4   // Tempo de debounce para descida do sensor
#define sw0 io.digital_read(0)  // Leitura do sensor SW0 (inductive sensor input)

// Variáveis globais
uint32_t count_ms = 0;      // Contador de milissegundos
uint32_t count_pecas = 0;   // Contador de peças
char buf[9] = {};           // Buffer para formatação da contagem (8 dígitos + null terminator)
uint32_t r6, r7;            // Variáveis para armazenar os valores a serem escritos nos registradores 6 e 7

// Estados da máquina de estados
static enum { WAITING_FOR_RISING, WAITING_FOR_FALLING } state = WAITING_FOR_RISING;

// Variáveis atômicas para sinalizar eventos de subida e descida do sensor
std::atomic<bool> sw0_rising{false};
std::atomic<bool> sw0_falling{false};

// Função para lidar com a interrupção de subida do sensor
void sw0_rising_handler(void) {
    sw0_rising.store(true); // Sinaliza que ocorreu uma subida no sensor
}

// Função para lidar com a interrupção de descida do sensor
void sw0_falling_handler(void) {
    sw0_falling.store(true); // Sinaliza que ocorreu uma descida no sensor
}

// Função para atualizar o display com a contagem atual
void display_update() {
//    count_pecas++;
    std::snprintf(buf, sizeof(buf), "%8u", count_pecas);
    std::memcpy(&r6, buf + 0, 4);
    std::memcpy(&r7, buf + 4, 4);
    io.write_reg(6, r6);
    io.write_reg(7, r7);
}

// Função da máquina de estados para gerenciar as transições entre os estados de espera por subida e descida do sensor
void MaquinaEstados() {
    switch (state) {
        case WAITING_FOR_RISING:
            if (sw0_rising.load()) {

                io.detach_interrupt(0); // desativa a interrupção para evitar múltiplas contagens do mesmo objeto                
                sw0_rising.store(false);

                // teste de contagem
                if (count_ms > DEBOUNCE_RISING_MS) {
                    std::printf("Rising Count: %u\n", count_ms); // Debug: imprime a contagem atual no console
                    count_ms = 0; // Reseta o contador para a nova peça

                    display_update();   // Atualiza o display com a nova contagem

                    sw0_falling.store(false);
                    io.attach_interrupt(0, sw0_falling_handler, InterruptMode::FALLING); // reativa a interrupção para o próximo objeto
                    state = WAITING_FOR_FALLING;
                } else {
                    io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING); // reativa a interrupção para o próximo objeto
                }
                // fim do teste de contagem
            }
            break;

        case WAITING_FOR_FALLING:
            if (sw0_falling.load()) {

                io.detach_interrupt(0); // desativa a interrupção para evitar múltiplas contagens do mesmo objeto
                sw0_falling.store(false);
            
                // teste de contagem
                if (count_ms > DEBOUNCE_FALLING_MS) {
                    std::printf("Falling Count: %u\n", count_ms); // Debug: imprime a contagem atual no console
                    count_ms = 0; // Reseta o contador para a nova peça

                    count_pecas++;    // Incrementa o contador de peças
                    //std::printf("Pecas: %u\n", count_pecas); // Debug: imprime a contagem atual no console
                    display_update();   // Atualiza o display com a nova contagem

                    sw0_rising.store(false);
                    io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING); // reativa a interrupção para o próximo objeto
                    state = WAITING_FOR_RISING;
                } else {
                    io.attach_interrupt(0, sw0_falling_handler, InterruptMode::FALLING); // reativa a interrupção para o próximo objeto
                }
                // fim do teste de contagem
            }
            break;
    }
}

// Função principal
int main() {
    // Configura a interrupção para o sensor SW0 (inductive sensor input) para detectar a borda de subida
    io.attach_interrupt(0, sw0_rising_handler, InterruptMode::RISING);

    display_update(); // Inicializa o display com a contagem inicial (0)

    while (true) {
        MaquinaEstados();

        io.delay(1); // Pequeno delay para evitar uso excessivo da CPU
        count_ms++; // Incrementa o contador de milissegundos (remover esta linha para a contagem real)
    }

    return 0;
}
*/
