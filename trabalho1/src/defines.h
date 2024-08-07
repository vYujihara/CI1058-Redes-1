#ifndef __REDES_DEFS_H__
#define __REDES_DEFS_H__

#include <stdint.h>
#include <dirent.h>

#define MAX_SERVER_TRYS 10  // numero maximo de tentativa de reenvio da janela
#define MAX_CLIENT_TRYS 12  // maximo de tentativas de recebimento de janela
#define MAX_NACK_TRYS 3
#define MAX_LIST_FILES 30  // quantidade maxima de arquivos no diretorio server

#define IN_PROGRESS 1
#define END_TX 2
#define DISPLAY 3
#define RESEND 4
#define ACK 1
#define NACK 0
#define ERROR -1

#define READ_BUFFER_LENGTH 140
#define DATA_BUFFER_LENGTH 300

#define CLIENT_PATH "videos-client/"
#define SERVER_PATH "videos-server/"

#define TIMEOUT_MS 2000
#define TIMEOUT_S  2

// codigo para os campos da mensagem
#define MSG_FIELD_START_MARKER 1
#define MSG_FIELD_DATA_LENGTH  2
#define MSG_FIELD_SEQUENCE     3
#define MSG_FIELD_TYPE         4
#define MSG_FIELD_DATA         5
#define MSG_FIELD_CRC          6

#define MSG_START_MARKER  0b01111110  // marcador de inicio da mensagem
#define DATA_FIELD_LENGTH 63          // tamanho máximo da mensagem
#define SEQUENCE_LENGTH   32          // numero de sequencia

// tipos de mensagem para o campo "type"
#define MSG_TYPE_ACK      0b00000   // ack
#define MSG_TYPE_NACK     0b00001   // nack
#define MSG_TYPE_LIST     0b01010   // listar opções
#define MSG_TYPE_DOWNLOAD 0b01011   // baixar video
#define MSG_TYPE_DISPLAY  0b10000   // mostrar na tela
#define MSG_TYPE_FILEDESC 0b10001   // descritor de arquivo
#define MSG_TYPE_DATA     0b10010   // dados
#define MSG_TYPE_END      0b11110   // fim tx
#define MSG_TYPE_ERROR    0b11111   // erro
#define MSG_TYPE_DONE     0b01111   // encerrar programa

// tipos de erros reportados
#define ERROR_DENIED_ACCESS  1   // acesso negado
#define ERROR_NOT_FOUND      2   // não encontrado
#define ERROR_FULL_DISK      3   // armazenamento cheio
#define ERROR_SERVER_TIMEOUT 4   // server timeout

// erros internos
#define ERROR_TIMEOUT -1
#define ERROR_RECV    -2

// indicadores de quem esta enviando a mensagem
#define SENDER_SERVER       1
#define SENDER_CLIENT       2

// defines loopback
// #define LOOPBACK

#ifdef LOOPBACK
    #define USING_LOOPBACK      1
    #define LOOPBACK_DUPLICATE -4
    #define NETWORK_INTERFACE "lo"
#else
    #define USING_LOOPBACK      0
    #define NETWORK_INTERFACE "enp3s0"
#endif

#endif  // __REDES_DEFS_H__