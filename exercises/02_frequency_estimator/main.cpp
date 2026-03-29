// =============================================================================
//  Challenge 02 — Frequency Estimator
// =============================================================================
//
//  Virtual hardware:
//    ADC Ch 0  →  io.analog_read(0)      Process sensor signal (0–4095)
//    OUT reg 3 →  io.write_reg(3, …)     Frequency estimate in centiHz
//                                        e.g. write_reg(3, 4733) = 47.33 Hz
//
//  Goal:
//    Measure the frequency of the signal on ADC channel 0 and publish your
//    estimate continuously via register 3.
//
//  Read README.md before starting.
// =============================================================================

#include "trac_fw_io.hpp"
#include <cstdint>

// Global I/O object -- see trac_fw_io.hpp for details.
trac_fw_io_t io;

// Variáveis globais
const uint16_t threshold = 2048;    // nível médio do ADC (12 bits)
uint16_t sample;                    // variável para armazenar a amostra atual do ADC

bool high;                          // estado atual do sinal (alto ou baixo)
bool prev_high = false;             // estado anterior do sinal (alto ou baixo), seta como baixo inicialmente

uint32_t last_edge = 0;             // timestamp da última borda de subida detectada (ms)
float freq = 0.0f;                  // frequência estimada (Hz), inicializada como 0.0

// Buffer para média móvel ponderada
const int N_amostras = 16;              // número de amostras para a média móvel ponderada
float freq_buffer[N_amostras] = {0};    // buffer circular para armazenar as últimas N_amostras de frequência
int idx = 0;                            // índice atual para o buffer circular
bool buffer_full = false;               // indica se o buffer já foi preenchido pelo menos uma vez

// Função para calcular média móvel ponderada
// Responde mais rapidamente a mudanças recentes, dando mais peso às amostras mais recentes
// Manter estabilidade, sem perder a capaicdade de resposta a mudanças rápidas
float MediaPonderada() {
    int count = buffer_full ? N_amostras : idx; // se o buffer ainda não estiver cheio, conta apenas as amostras válidas, count = idx
    float soma = 0.0f;
    float media_soma = 0.0f;

    for (int i = 0; i < count; i++) {
        int media = i + 1;                      // pesos crescentes
        soma += freq_buffer[i] * media;         // soma ponderada
        media_soma += media;                    // soma dos pesos
    }
    return (media_soma > 0) ? (soma / media_soma) : 0.0f; // retorna a média ponderada, evitando divisão por zero
}

// Função principal
int main() {
    while (true) {
        sample = io.analog_read(0);     // Lê a amostra do ADC
        high = (sample > threshold);    // Considera o sinal alto se a amostra for maior que o limiar

        // Detecta borda de subida
        if (high && !prev_high) {
            uint32_t now = io.millis(); // Obtém o timestamp atual em milissegundos
            // Cálculo de frequência: calcula a frequência com base no período entre as bordas de subida
            if (last_edge > 0) {
                uint32_t period_ms = now - last_edge;   // Calcula o período em milissegundos
                // Evita divisão por zero e cálculos inválidos
                if (period_ms > 0) {
                    float new_freq = 1000.0f / period_ms;   // Calcula a frequência em Hz (1000 ms / período em ms)

                    // Armazena no buffer circular
                    freq_buffer[idx] = new_freq;
                    idx = (idx + 1) % N_amostras;       // Incrementa o índice circularmente
                    if (idx == 0) buffer_full = true;   // Marca o buffer como cheio após a primeira volta completa

                    // Média móvel ponderada com filtro adaptativo
                    float media = MediaPonderada();   // Calcula a média móvel ponderada das frequências armazenadas no buffer

                    // Filtro adaptativo
                    float diff = fabs(new_freq - freq); // diferença entre a nova frequência e a frequência estimada atual
                                                        // (fabs para garantir valor positivo em float)
                    // Se a diferença for grande, pode ser um sinal de mudança rápida, então damos mais peso à nova frequência
                    // aplica filtro adaptativo normalmente
                    float alpha = (diff > 1.0f) ? 0.2f : 0.05f; // se a diferença for maior que 1 Hz,
                                                                // aumenta o peso da nova frequência para 20%, caso contrário, mantém em 5%
                    freq = (1 - alpha) * freq + alpha * media;    // Atualiza a frequência estimada usando o filtro adaptativo
                                                                // com a média móvel ponderada

                    io.write_reg(3, static_cast<uint32_t>(freq * 100)); // Escreve a frequência estimada em centiHz no registrador 3
                }
            }
            last_edge = now; // Atualiza o timestamp da última borda de subida detectada
        }

        prev_high = high; // Atualiza o estado anterior do sinal para a próxima iteração
        io.delay(1);      // Garante taxa fixa de 1 kHz, evitando uso excessivo da CPU e garantindo amostragem consistente
    }

    return 0;
}
