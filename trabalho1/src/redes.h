#ifndef __REDES_H__
#define __REDES_H__

#include "defines.h"  // defines
#include <stdint.h>
#include <stdio.h>

#pragma pack(push, 1)  // evitar adição de padding nos bitfields
typedef struct fields {
    uint8_t start_marker:8;
    uint8_t data_length:6;
    uint8_t sequence:5;
    uint8_t type:5;
    uint8_t data[DATA_FIELD_LENGTH];
#if USING_LOOPBACK
    uint8_t src;  // indica quem enviou a mensagem para o loopback
#endif 
    uint8_t crc:8;
} fields_t;
#pragma pack(pop)

typedef union msg {
    fields_t fields;
    uint8_t bytes[sizeof(fields_t)];
} msg_t;

// funções gerais ==============================================================

// cria um raw socket modo promíscuo para a interface de rede
int create_raw_socket(char *network_interface_name);

// extrai o conteúdo dos videos para um buffer, retorna o tamanho do arquivo
int64_t extract_file(FILE *file, uint8_t **buffer);

// funções mensagem ============================================================

// cria uma mensagem
uint64_t create_msg(msg_t **msg, 
                    uint8_t data_length, 
                    uint8_t sequence, 
                    uint8_t type, 
                    uint8_t *data,
                    uint8_t src);

// desaloca a memoria alocada da mensagem
void destroy_msg(msg_t *msg);

// envia uma mensagem pela rede
int64_t send_msg(msg_t *msg, uint8_t msg_length, int sockfd);

// recebe uma mensagem da rede
int64_t recv_msg(msg_t *msg, int sockfd);
int64_t recv_msg_client(msg_t *msg, int sockfd);
int64_t recv_msg_server(msg_t *msg, int sockfd);

// imprime os campos da mensagem
void print_msg(msg_t *msg);

// imprime os bytes da mensagem
void print_msg_bytes(uint8_t *msg, uint64_t msg_size);

// retorna o campo indicado da mensagem
uint8_t get_msg_field(msg_t *msg, int msg_field);

// retorna o nome correspondente ao tipo de mensagem
char *get_msg_type_name(uint8_t type);

// operações crc ==============================================================

// retorna o input para o crc, que é a mensagem tirando o 
// byte inicial (start_marker) e byte final (crc)
uint8_t *get_crc_input(msg_t *msg, uint8_t msg_length);

// retorna o valor para o campo de crc8
uint8_t calculate_crc(uint8_t *input, uint8_t length);

// faz a checagem do crc realizando verificando se a divisão é 0
int check_crc(msg_t *msg, uint8_t msg_length);

// operaçoes de vlan ===========================================================

// adiciona um byte 0xff (escape byte) apos bytes de reconhecimento vlan
// retorna o novo tamanho da mensagem
uint64_t add_vlan_escape_bytes(uint8_t **msg, uint64_t msg_length);

// remove os bytes 0xff (escape byte) apos bytes de reconhecimento vlan
// retorna o novo tamanho da mensagem
uint64_t remove_vlan_escape_bytes(uint8_t **msg, uint64_t msg_length);

// send messages ===============================================================

// mandar uma mensagem de ack
void send_ack(uint8_t msg_seq, int sockfd, uint8_t src);

// mandar uma mensagem de nack
void send_nack(uint8_t msg_seq, int sockfd, uint8_t src);

// mandar uma mensagem de end tx
void send_end_tx(int sockfd, uint8_t src);

// manda uma mensagem de erro
void send_error(uint8_t error_type, int sockfd, uint8_t src);

// manda uma mensagem de mostrar na tela como o nome do arquivo a ser mostrado
void send_display(char *file_name, uint8_t msg_seq, int sockfd, uint8_t src);

// manda uma mensagem de descritor de arquivo
void send_filedesc(uint8_t file_size, uint8_t msg_seq, int sockfd, uint8_t src);

// manda uma mensagem para listar arquivos do servidor
void send_list(int sockfd, uint8_t src);

// manda uma mensagem para baixar um arquivo do servidor
void send_download(char *file_name, int sockfd, uint8_t src);

// manda uma mensagem de finalização de programa
void send_done(int sockfd, uint8_t src);

// =============================================================================

#endif  // __REDES_H__