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

#include <trac_fw_io.hpp>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <cmath>

// Global I/O object -- see trac_fw_io.hpp for details.
trac_fw_io_t io;

// CONFIGURAÇÕES
// Limiar para detectar nível alto/baixo do sinal (meio da faixa ADC de 12 bits)
const uint16_t threshold = 2048;

// Número de amostras armazenadas para cálculo da mediana
const int N_amostras = 8;

// VARIÁVEIS GLOBAIS
uint16_t sample;                // variável para armazenar a amostra atual do ADC
bool high, prev_high = false;   // estado atual e anterior do sinal (alto ou baixo), inicializados como baixo
uint32_t last_edge = 0;         // timestamp da última borda de subida detectada (ms)
float freq = 0.0f;              // frequência estimada (Hz), inicializada como 0.0

float freq_buffer[N_amostras] = {0};    // buffer circular para armazenar as últimas N_amostras de frequência
int idx = 0;                            // índice atual para o buffer circular
bool buffer_full = false;               // indica se o buffer já foi preenchido pelo menos uma vez,
                                        // para controle do número de amostras válidas  

// FILTRO MEDIANA
// A mediana elimina valores espúrios (outliers) causados por ruído.
// Exemplo: se uma borda falsa gerar uma frequência absurda,
// a mediana descarta esse valor e mantém a tendência real.
float FiltroMediana(const float* buffer, int count) {
    std::vector<float> temp(buffer, buffer + count);        // copia os valores válidos para um vetor temporário
    std::sort(temp.begin(), temp.end());                    // ordena o vetor para encontrar a mediana
    if (count % 2 == 0) {
        return (temp[count/2 - 1] + temp[count/2]) / 2.0f;  // se o número de amostras for par, retorna a média dos dois valores centrais
    } else {
        return temp[count/2];                               // se o número de amostras for ímpar, retorna o valor central
    }
}

// FILTRO KALMAN
// O filtro de Kalman suaviza a mediana, equilibrando histórico e medições.
// Parâmetros principais:
//   P -> variância do erro da estimativa (quanto o filtro confia na sua própria previsão)
//      P pequeno: filtro confia mais na previsão, mais estável, mas pode ser lento para responder a mudanças reais
//      P grande: filtro confia mais nas medições, mais responsivo, mas pode ser mais instável se o sinal for ruidoso
//   Q -> variância do processo (quanto a frequência pode mudar naturalmente)
//      Q pequeno: filtro lento, mais estável, mas pode atrasar resposta a mudanças rápidas
//      Q grande: filtro rápido, acompanha mudanças, mas pode ser mais instável
//   R -> variância da medição (quanto ruído esperamos nas medidas)
//      R pequeno: filtro confia mais nas medições, pode ser instável se o sinal for ruidoso
//      R grande: filtro confia mais no histórico, mais estável, mas pode atrasar resposta a mudanças reais
//
// Ajustes típicos:
//   - Sinal limpo: Q = 0.01, R = 0.1
//   - Sinal moderadamente ruidoso: Q = 0.005, R = 0.5
//   - Sinal muito ruidoso: Q = 0.001, R = 1.5 ou maior
//
// Estratégia: quanto mais ruído, maior R. Quanto mais rápida a variação real,
// maior Q.
// Variáveis do filtro de Kalman
float kalman_freq = 0.0f;   // frequência estimada pelo filtro de Kalman, inicializada como 0.0
float P = 0.75f;             // [1.0f] estimativa inicial da variância do erro (pode ser ajustada conforme o comportamento do sinal)
const float Q = 0.05f;      // [0.001f] ajuste para sinal ruidoso: mudanças reais são lentas
const float R = 3.0f;       // [1.5f] ajuste para sinal ruidoso: medições têm bastante ruído

float FiltroKalman(float medida) {
    float P_pred = P + Q;               // previsão da variância do erro após o processo (aumenta devido à incerteza)
    float K = P_pred / (P_pred + R);    // ganho de Kalman: quanto confiar na nova medição vs. no histórico
    kalman_freq = kalman_freq + K * (medida - kalman_freq); // atualização da estimativa: combina a medição com a previsão, 
                                                            // ponderada pelo ganho
    P = (1 - K) * P_pred;               // atualização da variância do erro: reduz à medida que o filtro confia mais na estimativa
    return kalman_freq;                 // retorna a frequência estimada pelo filtro de Kalman
}

// ---------------------- LOOP PRINCIPAL ----------------------
int main() {
    while (true) {
        sample = io.analog_read(0);     // Lê a amostra do ADC
        high = (sample > threshold);    // Considera o sinal alto se a amostra for maior que o limiar

        // Detecta borda de subida
        if (high && !prev_high) {
            uint32_t now = io.millis(); // Obtém o timestamp atual em milissegundos
            if (last_edge > 0) {
                uint32_t period_ms = now - last_edge;                   // Calcula o período em milissegundos entre as bordas de subida
                // Evita divisão por zero e cálculos inválidos    
                if (period_ms > 0) {
                    float new_freq = 1000.0f / period_ms;               // Calcula a frequência em Hz (1000 ms / período em ms)

                    // Armazena no buffer circular
                    freq_buffer[idx] = new_freq;
                    idx = (idx + 1) % N_amostras;
                    if (idx == 0) buffer_full = true;                   // Marca o buffer como cheio após a primeira volta completa

                    // calcula a mediana das frequências armazenadas no buffer, eliminando outliers causados por ruído
                    int count = buffer_full ? N_amostras : idx;         // conta apenas as amostras válidas,
                                                                        // se o buffer ainda não estiver cheio
                    float mediana = FiltroMediana(freq_buffer, count);  // calcula a mediana das frequências armazenadas no buffer, eliminando outliers causados por ruído

                    // aplica o filtro de Kalman para suavizar a mediana, equilibrando histórico e medições
                    freq = FiltroKalman(mediana);

                    // Publica resultado em centiHz
                    io.write_reg(3, static_cast<uint32_t>(freq * 100));
                }
            }
            last_edge = now;    // Atualiza o timestamp da última borda de subida detectada
        }

        prev_high = high;       // Atualiza o estado anterior do sinal para a próxima iteração
        io.delay(1); // taxa fixa de 1 kHz
    }
    return 0;
}
