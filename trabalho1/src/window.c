#include "window.h"
#include "defines.h"
#include "utils.h"
#include "redes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// sliding window functions ====================================================

// cria uma janela para quant_msg mensagens
window_t *create_window(uint64_t quant_msg, uint64_t file_size) {
    window_t *window = (window_t *) malloc(sizeof(window_t));
    assert_ptr(window, "window creation malloc", EXIT_FAILURE);

    window->data_parts = (data_parts_t *) malloc(sizeof(data_parts_t) * quant_msg);
    assert_ptr(window->data_parts, "window data parts malloc", EXIT_FAILURE);

    for (uint64_t i = 0; i < quant_msg; i++) {
        window->data_parts[i].index = i;
        window->data_parts[i].seq = i % SEQUENCE_LENGTH;
        window->data_parts[i].data_length = DATA_FIELD_LENGTH;
    }
    uint8_t last_part_length = file_size % DATA_FIELD_LENGTH;
    if (last_part_length != 0) {
        window->data_parts[quant_msg - 1].data_length = last_part_length;
    }

    window->start = &(window->data_parts[0]);
    window->curr = &(window->data_parts[0]);

    if (quant_msg < WINDOW_LENGTH) {
        window->end = &(window->data_parts[quant_msg - 1]);
    } else {
        window->end = window->start + WINDOW_LENGTH;
    }

    window->remaining_msgs = quant_msg;
    window->file_size = file_size;

    return window;
}

// envia as mensagens da janela pela rede
int send_window(window_t *window, uint8_t *buffer, int sockfd) {
    // printf("----------\\\n");
    while (window->curr < window->end) {
        msg_t *msg = (msg_t *) malloc(sizeof(msg_t));
        assert_ptr(msg, "msg malloc", EXIT_FAILURE);

        uint8_t seq = window->curr->seq;
        uint64_t start_idx = window->curr->index * DATA_FIELD_LENGTH;
        uint64_t data_length = window->curr->data_length;
        uint8_t data[DATA_FIELD_LENGTH];
        memcpy(data, &(buffer[start_idx]), data_length);

        uint64_t msg_length = create_msg(&msg, 
                                         data_length, 
                                         seq, 
                                         MSG_TYPE_DATA, 
                                         data, 
                                         SENDER_SERVER);
        
        // printf("MENSAGEM ENVIADA %d\n", seq);
        #if DEBUG
        printf("MENSAGEM ENVIADA\n");
        print_msg(msg);
        printf("\n");
        #endif

        send_msg(msg, msg_length, sockfd);

        free(msg);
        window->curr++;
    }
    
    // printf("----------/\n");

    return 0;
}

// move a janela para que o numero da sequencia da mensagem 
// inicial seja new_strat_seq 
void move_window(window_t *window, uint8_t new_start_seq) {

    while (window->start->seq != new_start_seq && 
        window->start != window->end) {
        
        window->remaining_msgs--;
        window->start++;
    }
    // window->remaining_msgs--;
    // window->start++;

    if (window->start == window->end) {
        // printf("END ----------------------------------------------------\n");
        return;
    }

    if (window->remaining_msgs < WINDOW_LENGTH) {
        window->end = window->start + window->remaining_msgs;
    } else {
        window->end = window->start + WINDOW_LENGTH;
    }

    #if DEBUG
    printf("NEW WINDOW: ");
    print_window(window);
    #endif 
}

// desaloca a memoria da janela
void destroy_window(window_t *window) {
    if (window) {
        if (window->data_parts) {
            free(window->data_parts);
        }
        free(window);
    }
}

// imprime a janela
void print_window(window_t *window) {
    data_parts_t *aux = window->start;
    while (aux != window->end) {
        printf("%u ", aux->seq);
        aux++;
    }
    printf("\n");
}

// =============================================================================
