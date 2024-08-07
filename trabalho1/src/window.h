#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <stdint.h>

// tamanho da janela deslizante
#define WINDOW_LENGTH 5

#pragma pack(push, 1)  // evitar adição de padding nos bitfields
typedef struct data_parts {
    uint64_t index;
    uint64_t data_length;
    uint8_t seq:6;
} data_parts_t;
#pragma pack(pop)

typedef struct window {
    data_parts_t *data_parts;
    data_parts_t *start;
    data_parts_t *end;
    data_parts_t *curr;
    uint64_t remaining_msgs;
    uint64_t file_size;
} window_t;

// sliding window functions ====================================================

// cria uma janela para quant_msg mensagens
window_t *create_window(uint64_t quant_msg, uint64_t file_size);

// envia as mensagens da janela pela rede
int send_window(window_t *window, uint8_t *buffer, int sockfd);

// move a janela para que o numero da sequencia da mensagem inicial
// seja new_strat_seq 
void move_window(window_t *window, uint8_t new_start_seq);

// desaloca a memoria da janela
void destroy_window(window_t *window);

// imprime a janela
void print_window(window_t *window);

// =============================================================================

#endif  // __WINDOW_H__