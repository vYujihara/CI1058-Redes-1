#include "interface.h"
#include "redes.h"
#include "utils.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

uint8_t get_file_size(FILE *file) {
    fseek(file, 0, SEEK_END);
    uint8_t file_size = ftell(file);
    rewind(file);
    return file_size;
}

int process_list_responses(msg_t *msg, uint8_t *expected_seq, int sockfd) {
    uint8_t recv_seq = msg->fields.sequence;

    switch (msg->fields.type) {
    case MSG_TYPE_DISPLAY:
        if (*expected_seq == recv_seq) {
            send_ack(recv_seq, sockfd, SENDER_CLIENT);
            printf("%s\n", msg->fields.data);
            *expected_seq = (*expected_seq + 1) % SEQUENCE_LENGTH;
            return DISPLAY;
        } else if (recv_seq < *expected_seq) {
            send_ack(recv_seq, sockfd, SENDER_CLIENT);
            return RESEND;
        }
    case MSG_TYPE_END:
        send_ack(recv_seq, sockfd, SENDER_CLIENT);
        return END_TX;
    default:
        return 0;
    }
    free(msg);
}

int wait_for_ack_nack(msg_t *msg, int sockfd, uint8_t *trys, uint8_t *sent_seq) {
    if (!wait_for_response(msg, sockfd, recv_msg)) {
        if (++(*trys) > MAX_SERVER_TRYS) {
            error("timeout", EXIT_FAILURE);
        }
        printf("timeout wait for ack nack\n");
        return 0;
    }
    
    uint8_t msg_seq_num = msg->fields.data[0];
    uint8_t error_type = 0;

    switch (msg->fields.type) {
    case MSG_TYPE_ACK:
        if (sent_seq == NULL || *sent_seq == msg_seq_num) {
            if (sent_seq) {
                *sent_seq = (*sent_seq + 1) % SEQUENCE_LENGTH;
            }
            *trys = 0;
            return ACK;
        }
        else if (sent_seq) {
            return 0;
        }
    
    case MSG_TYPE_NACK:
        // reenvia
        printf("RECEBEU NACK\n");
        return NACK;

    case MSG_TYPE_ERROR:
        error_type = msg->fields.data[0];
        if (error_type == ERROR_NOT_FOUND) {
            printf("\nErro: arquivo não encontrado\n");
        }
        else if (error_type == ERROR_DENIED_ACCESS) {
            printf("\nErro: acesso negado\n");
        }
        return ERROR;
    
    default: 
        return 0;
    }
}

int greater_than_expected(uint8_t expected_seq, uint8_t recv_seq) {
    return (recv_seq > expected_seq && expected_seq >= 4) || 
           (expected_seq == 31 && (recv_seq >= 0 && recv_seq <= 4));
}

// filtra arquivos com .mp4 e .avi
int filter(const struct dirent *entry) {
    const char *name = entry->d_name;
    char *ext = strrchr(name, '.');

    // Se arquivo não tem extensão retorna 0
    return (ext) ? (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".avi") == 0) : 0;
}

int is_regular_file(const char *path) {
    struct stat statbuf;
    stat(path, &statbuf);
    return S_ISREG(statbuf.st_mode);
}

char *get_file_path(char *dir_path, char *file_name) {
    char *file_path = (char *) malloc(strlen(file_name) + strlen(dir_path) + 1);
    assert_ptr(file_path, "file path malloc", EXIT_FAILURE);

    // concatenando o path
    strcpy(file_path, dir_path);
    strcat(file_path, file_name);

    return file_path;
}

uint8_t get_file_names(char *dir_path, char **file_names) {
    DIR *dir = opendir(dir_path);
    assert_ptr(dir, "dir open failed", EXIT_FAILURE);
    struct dirent *dir_entry;
    uint8_t count = 0;

    while ((dir_entry = readdir(dir)) != NULL && count < MAX_LIST_FILES) {
        char *file_path = get_file_path(dir_path, dir_entry->d_name);

        if (is_regular_file(file_path)) {
            file_names[count] = malloc(strlen(dir_entry->d_name) + 1);
            assert_ptr(file_names[count], "file_names malloc", EXIT_FAILURE);

            // copiando o nome e adicionando '\0' no final
            strcpy(file_names[count], dir_entry->d_name);
            strcat(file_names[count], "\0");
            (count)++;
        }
        free(file_path);
    }
    closedir(dir);
    return count;
}

char **alloc_file_names_list(int max_files) {
    char **file_names = malloc(sizeof(char *));
    assert_ptr(file_names, "file_names malloc", EXIT_FAILURE);
    for (uint8_t i = 0; i < max_files; i++) {
        file_names[i] = malloc(DATA_FIELD_LENGTH + 1);
        assert_ptr(file_names[i], "file_names[i] malloc", EXIT_FAILURE);
    }
    return file_names;
}

// funções de interface ========================================================

// exibe os comandos para o cliente
void instructions() {
    printf("----- STREAMING DE VIDEO ------------------------------------\n\n");
    printf("Comandos:\n");
    printf("lsc : listar videos disponiveis no diretorio do cliente\n");
    printf("lss : listar videos disponiveis no diretorio do servidor\n");
    printf("get <arquivo> : fazer download de arquivo do servidor\n");
    printf("cmd : mostrar lista de comandos\n");
    printf("clr : limpar a tela\n");
    printf("end : fecha o programa\n");
}

// lista os videos do servidor (lado do servidor)
void list_server_server_side(int sockfd) {
    msg_t *msg = malloc(sizeof(msg_t));
    assert_ptr(msg, "msg malloc", EXIT_FAILURE);

    uint8_t trys = 0;
    int ack = 0;

    // enviando confirmação de recebimento de LIST
    send_ack(0, sockfd, SENDER_SERVER);

    char **file_names = alloc_file_names_list(MAX_LIST_FILES);
    uint8_t quant_files = get_file_names(SERVER_PATH, file_names);

    // enviando os arquivos
    uint8_t sent_seq = 0;
    for (uint8_t i = 0; i < quant_files; i++) {
        printf("Enviando: %s\n", file_names[i]);
        trys = 0;
        ack = 0;
        while (!ack) {
            send_display(file_names[i], sent_seq, sockfd, SENDER_SERVER);
            ack = wait_for_ack_nack(msg, sockfd, &trys, &sent_seq);
        }
    }

    // enviando final de transmissão
    trys = 0;
    ack = 0;
    while (!ack) {
        send_end_tx(sockfd, SENDER_SERVER);
        ack = wait_for_ack_nack(msg, sockfd, &trys, NULL);
    }
}

// lista os videos do servidor (lado do cliente)
void list_server_client_side(int sockfd) {
    msg_t *msg = malloc(sizeof(msg_t));
    assert_ptr(msg, "msg malloc", EXIT_FAILURE);

    uint8_t trys = 0;
    int ack = 0;

    // envia um pedido para listar os arquivos
    while (!ack) {
        send_list(sockfd, SENDER_CLIENT);
        ack = wait_for_ack_nack(msg, sockfd, &trys, NULL);
    }

    trys = 0;
    uint8_t recv_seq = 0;
    uint8_t expected_seq = 0;

    // recebe os nomes dos arquivos
    printf("Videos do servidor\n");

    while (1) {
        msg_t *msg = malloc(sizeof(msg_t));
        assert_ptr(msg, "msg malloc", EXIT_FAILURE);

        if (!wait_for_response(msg, sockfd, recv_msg)) {
            if (++trys > MAX_CLIENT_TRYS) {
                error("client timeout", EXIT_FAILURE);
            }
            continue;
        }

        int status = process_list_responses(msg, &expected_seq, sockfd);
        if (status == IN_PROGRESS) {
            printf(" %s\n", msg->fields.data);
        }
        else if (status == END_TX) {
            send_ack(recv_seq, sockfd, SENDER_CLIENT);
            break;
        }
        free(msg);
    }
}

// lista os videos do cliente
void list_client () {
    struct dirent **name_list;
    char *path = CLIENT_PATH;
    int n;

    printf("Videos do cliente\n");
    n = scandir(path, &name_list, filter, alphasort);
    assert_var(n, "list_client scandir", EXIT_FAILURE);

    for(int i = 0; i < n ; i++){
        printf(" %s\n", name_list[i]->d_name);
        free(name_list[i]);
    }
    free(name_list);
}

// send videos =================================================================

int process_client_response(msg_t *status_msg, window_t *window, uint64_t quant_msg, uint64_t *sent_msgs) {
    uint8_t msg_seq_num;

    switch (status_msg->fields.type) {
        case MSG_TYPE_ACK: 
            msg_seq_num = status_msg->fields.data[0];
            
            // calcula o novo começo de janela
            uint8_t new_start_seq = (msg_seq_num + 1) % SEQUENCE_LENGTH;                    
            move_window(window, new_start_seq);
            (*sent_msgs)++;

            #if DEBUG
            printf("RECEBEU ACK %d\n", msg_seq_num);
            printf("REMAINING MSGS %ld\n", window->remaining_msgs);
            #endif

            return 1;

        case MSG_TYPE_NACK:
            msg_seq_num = status_msg->fields.data[0];

            // reevia a janela com as mensagens não recebidas
            move_window(window, msg_seq_num);
            window->curr = window->start;

            #if DEBUG
            printf("RECEBEU NACK %d\n", msg_seq_num);
            #endif

            return 1; 
        
        default: 
            return 0;
    }
}

// envia um video via rede 
void send_video(int sockfd, FILE *file, char *file_name) {
    uint8_t *buffer = NULL;
    uint64_t file_size = extract_file(file, &buffer);
    uint64_t quant_msg = (file_size + DATA_FIELD_LENGTH - 1) / DATA_FIELD_LENGTH;
    
    window_t *window = create_window(quant_msg, file_size);
    uint64_t sent_msgs = 0;
    int trys = 0;

    printf("Enviando video '%s'\n", file_name);

    while (1) {
        send_window(window, buffer, sockfd);
        trys++;

        while (1) {
            msg_t *status_msg = malloc(sizeof(msg_t));
            assert_ptr(status_msg, "status msg malloc", EXIT_FAILURE);

            if (!wait_for_response(status_msg, sockfd, recv_msg)) {
                // reenvia a janela por timeout do servidor
                window->curr = window->start;
                free(status_msg);
                break;
            }

            if (process_client_response(status_msg, window, quant_msg, &sent_msgs)) {
                trys = 0;
                print_progress_bar(sent_msgs, quant_msg);
                free(status_msg);

                if (window->remaining_msgs == 0) {
                    printf("\nEnvio de video completo!\n");
                    send_end_tx(sockfd, SENDER_SERVER);
                    free(buffer);
                    return;
                }
                break;
            }

            if (trys >= MAX_SERVER_TRYS) {
                free(buffer);
                free(status_msg);
                destroy_window(window);
                send_error(ERROR_SERVER_TIMEOUT, sockfd, SENDER_SERVER);
                error("too many trys", EXIT_FAILURE);
            }
        }
    }
}

// recv video ==================================================================

void compute_msg(msg_t *msg, FILE *file, uint8_t *expected_seq, int sockfd) {
    fwrite(&(msg->fields.data), sizeof(uint8_t), msg->fields.data_length, file);
    send_ack(*expected_seq, sockfd, SENDER_CLIENT);
    
    *expected_seq = (*expected_seq + 1) % SEQUENCE_LENGTH;
}

msg_t *nack_response(int sockfd, uint8_t expected_seq) {
    send_nack(expected_seq, sockfd, SENDER_CLIENT);

    uint8_t nack_tries = 0;
    msg_t *msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(msg, "msg malloc", EXIT_FAILURE);

    while (1) {
        int status = recv_msg_client(msg, sockfd);
        if (status == ERROR_TIMEOUT) {
            if (++nack_tries > MAX_NACK_TRYS) {
                error("client nack timeout", EXIT_FAILURE);
            }
            continue;
        }
        #if USING_LOOPBACK
        if (status == LOOPBACK_DUPLICATE) {
            continue;
        }
        #endif
        if (msg->fields.type == MSG_TYPE_DATA && 
            get_msg_field(msg, MSG_FIELD_SEQUENCE) == expected_seq) {
            
            return msg;
        }
    }
}

int process_server_response(msg_t *msg, int sockfd, uint8_t *expected_seq, FILE *file) {
    uint8_t recv_seq = 0;
    uint8_t error_type = 0;

    switch (msg->fields.type) {
        case MSG_TYPE_ERROR:
            error_type = msg->fields.data[0];
            if (error_type == ERROR_SERVER_TIMEOUT) {
                error("server timeout. aborting...", EXIT_FAILURE);
            }
        case MSG_TYPE_END:
            return END_TX;
        
        case MSG_TYPE_DATA:
            recv_seq = get_msg_field(msg, MSG_FIELD_SEQUENCE);

            if (recv_seq == *expected_seq) {
                compute_msg(msg, file, expected_seq, sockfd);
            } 
            else if (greater_than_expected(*expected_seq, recv_seq)) {

                #if DEBUG
                printf("\nexpected %d recieved %d\n", expected_seq, recv_seq);
                #endif

                msg_t *nack_response_msg = malloc(sizeof(msg_t));
                assert_ptr(nack_response_msg, "nack_response malloc", EXIT_FAILURE);

                nack_response_msg = nack_response(sockfd, *expected_seq);
                compute_msg(nack_response_msg, file, expected_seq, sockfd);
                free(nack_response_msg);
            }
            return IN_PROGRESS;
        default: 
            return 0;
    }
}

// recebe um video via rede
void recv_video(int sockfd, FILE *file, char *file_name) {
    uint8_t expected_seq = 0;
    uint64_t recv_msgs = 0;
    uint8_t client_trys = 0;

    printf("Baixando video '%s'\n", file_name);

    while (1) {
        msg_t *msg = (msg_t *) malloc(sizeof(msg_t));
        assert_ptr(msg, "msg malloc", EXIT_FAILURE);

        int status = recv_msg(msg, sockfd);
        if (status == ERROR_TIMEOUT) {
            if (++client_trys > MAX_CLIENT_TRYS) {
                error("client timeout", EXIT_FAILURE);
            }
            continue;
        }
        #if USING_LOOPBACK
        if (status == LOOPBACK_DUPLICATE) {
            continue;
        }
        #endif

        status = process_server_response(msg, sockfd, &expected_seq, file);
        if (status == IN_PROGRESS) {
            print_progress_indicatior(recv_msgs);
            client_trys = 0;
        }
        else if (status == END_TX) {
            printf("\nVídeo baixado\n");
            free(msg);
            break;
        }
        free(msg);
    }
}

// download videos =============================================================

char *get_file_name(char *input, char *dir_path) {
    char *file_name = input + strlen("get") + 1;
    file_name[strcspn(file_name, "\n")] = '\0';

    // printf("nome do arquivo: '%s'\n", file_name);
    file_name = get_file_path(dir_path, file_name);
    return file_name;
}

int request_download(int sockfd, char *server_file_path) {
    msg_t *msg = malloc(sizeof(msg_t));
    assert_ptr(msg, "msg malloc", EXIT_FAILURE);

    int trys = 0;
    uint8_t error_type = 0;

    send_download(server_file_path, sockfd, SENDER_CLIENT);

    while (1) {
        if (!wait_for_response(msg, sockfd, recv_msg)) {
            if ((++trys) > MAX_CLIENT_TRYS) {
                error("timeout client", EXIT_FAILURE);
            }
            continue;
        }
        switch (msg->fields.type) {
        case MSG_TYPE_ACK:
            // recebeu ack, espera por filedesc
            break;
        case MSG_TYPE_FILEDESC:
            printf("recebeu filedesc\n");
            send_ack(0, sockfd, SENDER_CLIENT);
            printf("tamanho do arquivo: %u\n", msg->fields.data[0]);
            return ACK;
        case MSG_TYPE_ERROR:
            error_type = msg->fields.data[0];
            if (error_type == ERROR_NOT_FOUND) {
                printf("\nErro: arquivo não encontrado\n");
            }
            else if (error_type == ERROR_DENIED_ACCESS) {
                printf("\nErro: acesso negado\n");
            }
            return ERROR;
        default:
            break;
        }
    }
}

void download_video_client_side(int sockfd, char *input) {
    char *server_file_path = get_file_name(input, SERVER_PATH);

    char player_command[200];
    char *client_file_path;
    int display = 0;

    msg_t *msg = malloc(sizeof(msg_t));
    assert_ptr(msg, "msg malloc", EXIT_FAILURE);

    uint8_t trys = 0;
    int ack = 0;
    int end_tx = 0;

    // envia um pedidos para tranferir um arquivo do servidor
    while (1) {
        send_download(server_file_path, sockfd, SENDER_CLIENT);

        while (!end_tx) {
            if (!wait_for_response(msg, sockfd, recv_msg)) {
                if ((++trys) > MAX_CLIENT_TRYS) {
                    error("timeout client", EXIT_FAILURE);
                }
                continue;
            }
            switch (msg->fields.type) {
            case MSG_TYPE_ACK:
                // recebeu ack, espera por filedesc
                break;
            case MSG_TYPE_FILEDESC:
                printf("recebeu filedesc\n");
                send_ack(0, sockfd, SENDER_CLIENT);
                printf("tamanho do arquivo: %u\n", msg->fields.data[0]);
                ack = 1;
                end_tx = 1;
                break;
            case MSG_TYPE_ERROR:
                end_tx = 1;
                break;
            default:
                printf("default\n");
                printf("msg_type %s\n", get_msg_type_name(msg->fields.type));
                break;
            }
        }

        if (ack) {
            client_file_path = get_file_name(input, CLIENT_PATH);
            FILE *client_file = fopen(client_file_path, "wb");
            assert_ptr(client_file, "failed to open client file", EXIT_FAILURE);

            recv_video(sockfd, client_file, client_file_path);
            display = 1;
            break;
        }
        else {
            uint8_t error_type = msg->fields.data[0];
            if (error_type == ERROR_NOT_FOUND) {
                printf("\nErro: arquivo não encontrado\n");
            }
            else if (error_type == ERROR_DENIED_ACCESS) {
                printf("\nErro: acesso negado\n");
            }
            break;
        }
    }
    if (display) {
        snprintf(player_command, sizeof(player_command), "vlc %s", client_file_path);
        int status = system(player_command);
        assert_var(status, "failed to open vlc", EXIT_FAILURE);
    }
}

void download_video_server_side(int sockfd, msg_t *msg) {
    uint8_t trys = 0;
    char file_path[DATA_FIELD_LENGTH];
    memset(file_path, 0, DATA_FIELD_LENGTH);
    memcpy(file_path, msg->fields.data, msg->fields.data_length);
    printf("get file: '%s'\n", file_path);

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        if (errno == ENOENT) {
            printf("Erro: arquivo não encontrado\n");
            send_error(ERROR_NOT_FOUND, sockfd, SENDER_SERVER);
            return;
        } else if (errno == EACCES) {
            send_error(ERROR_DENIED_ACCESS, sockfd, SENDER_SERVER);
            printf("Erro: permissão negada\n");
            return;
        }
    }
    else {
        send_ack(0, sockfd, SENDER_SERVER);

        // enviando o file descriptor
        uint8_t file_size = get_file_size(file);
        trys = 0;
        int ack = 0;
        while (!ack) {
            send_filedesc(file_size, 0, sockfd, SENDER_SERVER);
            ack = wait_for_ack_nack(msg, sockfd, &trys, NULL);
        }
    }
    send_video(sockfd, file, file_path);
}

// espera por mensagem
int wait_for_response(msg_t *msg, int sockfd, int64_t (*recv_func) (msg_t *, int)) {
    while (1) {
        int status = recv_func(msg, sockfd);
        if (status == ERROR_TIMEOUT) {
            return 0;
        }
        #if USING_LOOPBACK 
        if (status == LOOPBACK_DUPLICATE) {
            continue;
        } 
        #endif
        else {
            return 1;
        }
    }
}