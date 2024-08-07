#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#define _GNU_SOURCE

#include <stdio.h>
#include "redes.h"

// funções de interface ========================================================

// exibe os comandos para o cliente
void instructions();

// lista os videos do servidor
void list_server_server_side(int sockfd);
void list_server_client_side(int sockfd);

// lista os videos do cliente
void list_client();

// faz a tranferencia de um video do servidor
void download_video_server_side(int sockfd, msg_t *msg);
void download_video_client_side(int sockfd, char *input);

// envia um video via rede
void send_video(int sockfd, FILE *file, char *file_name);

// recebe um video via rede
void recv_video(int sockfd, FILE *file, char *file_name);

// espera por mensagem
int wait_for_response(msg_t *msg, int sockfd, int64_t (*recv_func) (msg_t *, int));
// =============================================================================

#endif // __INTERFACE_H__