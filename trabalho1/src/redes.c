#include "redes.h"
#include "utils.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

// ignorar mensagens repetidas (loopback)
int ignore_msg = 0;
int ignore_msg_client = 0;
int ignore_msg_server = 0;

// garante que o tipo utilizado é valido
void assert_type(uint8_t type) {
    switch (type) {
        case MSG_TYPE_ACK:
        case MSG_TYPE_NACK:
        case MSG_TYPE_LIST:
        case MSG_TYPE_DOWNLOAD:
        case MSG_TYPE_DISPLAY:
        case MSG_TYPE_FILEDESC:
        case MSG_TYPE_DATA:
        case MSG_TYPE_END:
        case MSG_TYPE_ERROR:
        case MSG_TYPE_DONE:
            break;
        default:
            error("invalid type", EXIT_FAILURE);
    }
}

// imprime os campos da mensagem
void print_msg_fields(uint8_t start_marker, 
                      uint8_t data_length, 
                      uint8_t sequence, 
                      uint8_t type, 
                      uint8_t *data,
                      uint8_t crc) {
    
    printf("start_marker: %u 0x%x\n", start_marker, start_marker);
    printf("data length: %u 0x%x\n", data_length, data_length);
    printf("sequence: %u 0x%x\n", sequence, sequence);
    printf("type: %s\n", get_msg_type_name(type));
    printf("data: ");
    for (uint8_t i = 0; i < DATA_FIELD_LENGTH; i++) {
        printf("%u ", data[i]);
    }
    printf("\n");
    printf("crc: %u 0x%x\n", crc, crc);
}

// concatena o read_bytes de read_buffer em data_buffer, evitando overflow
void copy_to_buffer(uint8_t *data_buffer, uint64_t *data_buffer_length, 
                    uint8_t *read_buffer, uint8_t read_bytes) {
    
    // buffer cheio, removendo os bytes mais antigos
    if (*data_buffer_length + read_bytes > DATA_BUFFER_LENGTH) {
        // removendo os bytes mais antigos (primeiras posições do buffer)
        memmove(data_buffer, 
                data_buffer + READ_BUFFER_LENGTH, 
                DATA_BUFFER_LENGTH - READ_BUFFER_LENGTH);
        
        // zerando os ultimos bytes do buffer
        memset(data_buffer + DATA_BUFFER_LENGTH - READ_BUFFER_LENGTH, 
               0, 
               READ_BUFFER_LENGTH);
        
        *data_buffer_length -= READ_BUFFER_LENGTH;

        #if DEBUG
        printf("\nrecv_msg: buffer cheio, limpando...\n");
        #endif
    }

    // copiando para o buffer de dados
    memcpy(data_buffer + *data_buffer_length, read_buffer, read_bytes);
    *data_buffer_length += read_bytes;
}

// retorna uma possevel mensagem sem vlan bytes começando em msg_start
msg_t *extract_msg(uint8_t *msg_start, uint64_t msg_length) {
    uint8_t *aux = (uint8_t *) malloc(sizeof(uint8_t) * msg_length);
    assert_ptr(aux, "recv_msg temp buffer malloc", EXIT_FAILURE);

    memcpy(aux, msg_start, msg_length);

    remove_vlan_escape_bytes(&aux, msg_length);

    #if DEBUG
    printf("\nrecv_msg: encontrou uma possivel mensagem\n");
    print_msg_bytes((uint8_t *) aux, sizeof(msg_t));
    print_msg((msg_t *) aux);
    #endif

    return (msg_t *) aux;
}

// encontrar e retorna uma mensagem valida do buffer, ou NULL se não encontrar
msg_t *find_msg(uint8_t *buffer, uint64_t buffer_length) {
    for (uint64_t i = 0; i < buffer_length; i++) {

        if (buffer[i] == MSG_START_MARKER && (buffer_length - i) >= sizeof(msg_t)) {
            // obtem posseivel mensagem começando em &buffer[i]
            msg_t *possible_msg = extract_msg(&(buffer[i]), buffer_length - i);

            if (check_crc(possible_msg, sizeof(msg_t))) {
                #if DEBUG
                printf("\nrevc_msg: a mensagem é valida\n");
                #endif

                msg_t *valid_msg = (msg_t *) malloc(sizeof(msg_t));
                assert_ptr(valid_msg, "find_msg valid_msg malloc", EXIT_FAILURE);

                memcpy(valid_msg, possible_msg, sizeof(msg_t));
                free(possible_msg);
                return valid_msg;
            }
            free(possible_msg);
        }
    }
    return NULL;
}


// funções gerais ==============================================================

// cria um raw socket modo promíscuo para a interface de rede
int create_raw_socket(char *network_interface_name) {
    int sockfd, ifindex, status;

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    assert_var(sockfd, "socket creation, check if you are root", EXIT_FAILURE);

    // convertendo o nome da interface de rede para um indice
    ifindex = if_nametoindex(network_interface_name);

    // estrutura para o endereçamento do socket
    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = ifindex;

    // vincula o endereço ao socket
    status = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    assert_var(status, "bind", EXIT_FAILURE);

    // modo promíscuo (não joga fora o que identifica com lixo)
    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;

    status = setsockopt(sockfd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
    assert_var(status, "setsockopt", EXIT_FAILURE);

    // configuração de timeout
    struct timeval timeout = { 
        .tv_sec = TIMEOUT_MS / 1000, 
        .tv_usec = (TIMEOUT_MS % 1000) * 1000 
    }; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));

    return sockfd;
}

// extrai o conteúdo dos videos para um buffer, retorna o tamanho do arquivo
int64_t extract_file(FILE *file, uint8_t **buffer) {

    // quantidade de blocos em bytes
    fseek(file, 0 , SEEK_END);
    uint64_t file_size = ftell(file);
    rewind(file);
    printf("tam %ld\n", file_size);

    *buffer = (uint8_t *) malloc(sizeof(uint8_t) * file_size);
    assert_ptr(*buffer, "extract_file buffer malloc", EXIT_FAILURE);

    uint64_t read_bytes = fread(*buffer, sizeof(char), file_size, file);
    if (read_bytes != file_size) {
        free(*buffer);
        error("extract_file file read", EXIT_FAILURE);
    }

    fclose(file);
    
    return file_size;
}

// funções mensagem ============================================================

// cria uma mensagem retornando o seu tamanho
uint64_t create_msg(msg_t **msg, 
                    uint8_t data_length, 
                    uint8_t sequence, 
                    uint8_t type, 
                    uint8_t *data,
                    uint8_t src) {

    assert_ptr(msg, "null msg ptr", EXIT_FAILURE);

    *msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(*msg, "message creation", EXIT_FAILURE);
    assert_type(type);

    (*msg)->fields.start_marker = MSG_START_MARKER;
    (*msg)->fields.data_length = data_length;
    (*msg)->fields.sequence = sequence;
    (*msg)->fields.type = type;
    memset((*msg)->fields.data, 0, DATA_FIELD_LENGTH);
    if (data) {
        memcpy((*msg)->fields.data, data, data_length);
    }

    #ifdef LOOPBACK
    (*msg)->fields.src = src;
    #endif

    uint8_t *input = get_crc_input(*msg, sizeof(msg_t));
    uint8_t crc = calculate_crc(input, sizeof(msg_t) - 2);
    free(input);

    (*msg)->fields.crc = crc;

    uint64_t new_length = add_vlan_escape_bytes((uint8_t **) msg, sizeof(msg_t));

    // adicionando byte de escape se crc for identificador de cabeçalho de vlan
    // if (crc == 0x88 || crc == 0x81) {
    //     printf("crc é 0x88 ou 0x81");
    //     new_length++;
    //     *msg = realloc(*msg, new_length);
    //     (*msg)->bytes[new_length-1] = 0xff;
    //     (*msg)->bytes[new_length-2] = crc;
    // }
    // (*msg)->bytes[new_length-1] = crc;

    #if DEBUG
    printf("create_msg:\n");
    print_msg(*msg);
    print_msg_bytes((uint8_t *) *msg, new_length);
    #endif

    return new_length;
}

// desaloca a memoria alocada da mensagem
void destroy_msg(msg_t *msg) {
    // if (msg) free(msg);
}

// envia uma mensagem pela rede
int64_t send_msg(msg_t *msg, uint8_t msg_length, int sockfd) {
    return write(sockfd, msg, msg_length);
}

// recebe uma mensagem da rede
int64_t recv_msg(msg_t *msg, int sockfd) {
    assert_ptr(msg, "recv_msg null msg ptr", EXIT_FAILURE);

    uint8_t *read_buffer = (uint8_t *) malloc(sizeof(uint8_t) * READ_BUFFER_LENGTH);
    assert_ptr(read_buffer, "recv_msg read buffer malloc", EXIT_FAILURE);

    uint8_t *data_buffer = (uint8_t *) malloc(sizeof(uint8_t) * DATA_BUFFER_LENGTH);
    assert_ptr(read_buffer, "recv_msg data buffer malloc", EXIT_FAILURE);
    
    uint64_t data_buffer_length = 0;

    while (1) {
        int64_t read_bytes = 0;
        struct timeval timeout = { 
            .tv_sec = TIMEOUT_MS / 1000, 
            .tv_usec = (TIMEOUT_MS % 1000) * 1000 
        };

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
    
        int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0) {
            free(read_buffer);
            free(data_buffer);
            return ERROR_RECV;
        }
        if (ret == 0) {
            free(read_buffer);
            free(data_buffer);
            return ERROR_TIMEOUT;
        }
        if (FD_ISSET(sockfd, &readfds)) {
            read_bytes = read(sockfd, read_buffer, READ_BUFFER_LENGTH);
            if (read_bytes < 0) {
                free(read_buffer);
                free(data_buffer);
                return ERROR_RECV;
            }
        }

        #if DEBUG
        printf("\n----------------\n");
        printf("\nrecv_msg: bytes lidos (%ld bytes)\n", read_bytes);
        print_msg_bytes(read_buffer, read_bytes);
        #endif
            
        // concatena os bytes lidos ao data_buffer
        copy_to_buffer(data_buffer, &data_buffer_length, read_buffer, read_bytes);

        // procura por uma mensagem no buffer
        msg_t *found_msg = find_msg(data_buffer, data_buffer_length);
        if (found_msg) {
            #if USING_LOOPBACK
            if (!ignore_msg) {
                ignore_msg = 1;

                memcpy(msg, found_msg, sizeof(msg_t));
                free(found_msg);
                free(read_buffer);
                free(data_buffer);
                return read_bytes;
            }
            else {
                ignore_msg = 0;
                return LOOPBACK_DUPLICATE;
            }
            #else
            memcpy(msg, found_msg, sizeof(msg_t));
            free(found_msg);
            free(read_buffer);
            free(data_buffer);
            return read_bytes;
            #endif
        }
    }
}

// recebe uma mensagem da rede
int64_t recv_msg_client(msg_t *msg, int sockfd) {
    assert_ptr(msg, "recv_msg null msg ptr", EXIT_FAILURE);

    uint8_t *read_buffer = (uint8_t *) malloc(sizeof(uint8_t) * READ_BUFFER_LENGTH);
    assert_ptr(read_buffer, "recv_msg read buffer malloc", EXIT_FAILURE);

    uint8_t *data_buffer = (uint8_t *) malloc(sizeof(uint8_t) * DATA_BUFFER_LENGTH);
    assert_ptr(read_buffer, "recv_msg data buffer malloc", EXIT_FAILURE);
    
    uint64_t data_buffer_length = 0;

    while (1) {
        int64_t read_bytes = 0;
        struct timeval timeout = { 
            .tv_sec = TIMEOUT_MS / 1000, 
            .tv_usec = (TIMEOUT_MS % 1000) * 1000 
        };

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
    
        int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0) {
            free(read_buffer);
            free(data_buffer);
            return ERROR_RECV;
        }
        if (ret == 0) {
            free(read_buffer);
            free(data_buffer);
            return ERROR_TIMEOUT;
        }
        if (FD_ISSET(sockfd, &readfds)) {
            read_bytes = read(sockfd, read_buffer, READ_BUFFER_LENGTH);
            if (read_bytes < 0) {
                free(read_buffer);
                free(data_buffer);
                return ERROR_RECV;
            }
        }

        #if DEBUG
        printf("\n----------------\n");
        printf("\nrecv_msg: bytes lidos (%ld bytes)\n", read_bytes);
        print_msg_bytes(read_buffer, read_bytes);
        #endif
            
        // concatena os bytes lidos ao data_buffer
        copy_to_buffer(data_buffer, &data_buffer_length, read_buffer, read_bytes);

        // procura por uma mensagem no buffer
        msg_t *found_msg = find_msg(data_buffer, data_buffer_length);
        if (found_msg) {
            #if USING_LOOPBACK
            if (!ignore_msg_client) {
                if (found_msg->fields.src == SENDER_SERVER) {
                    ignore_msg_client = 1;

                    memcpy(msg, found_msg, sizeof(msg_t));
                    free(found_msg);
                    free(read_buffer);
                    free(data_buffer);
                    return read_bytes;
            
                }
            }
            else {
                ignore_msg_client = 0;
                return LOOPBACK_DUPLICATE;
            }
            #else
            memcpy(msg, found_msg, sizeof(msg_t));
            free(found_msg);
            free(read_buffer);
            free(data_buffer);
            return read_bytes;
            #endif
        }
    }
}

// recebe uma mensagem da rede
int64_t recv_msg_server(msg_t *msg, int sockfd) {
    assert_ptr(msg, "recv_msg null msg ptr", EXIT_FAILURE);

    uint8_t *read_buffer = (uint8_t *) malloc(sizeof(uint8_t) * READ_BUFFER_LENGTH);
    assert_ptr(read_buffer, "recv_msg read buffer malloc", EXIT_FAILURE);

    uint8_t *data_buffer = (uint8_t *) malloc(sizeof(uint8_t) * DATA_BUFFER_LENGTH);
    assert_ptr(read_buffer, "recv_msg data buffer malloc", EXIT_FAILURE);
    
    uint64_t data_buffer_length = 0;

    while (1) {
        int64_t read_bytes = 0;
        struct timeval timeout = { 
            .tv_sec = TIMEOUT_MS / 1000, 
            .tv_usec = (TIMEOUT_MS % 1000) * 1000 
        };

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
    
        int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0) {
            free(read_buffer);
            free(data_buffer);
            return ERROR_RECV;
        }
        if (ret == 0) {
            free(read_buffer);
            free(data_buffer);
            return ERROR_TIMEOUT;
        }
        if (FD_ISSET(sockfd, &readfds)) {
            read_bytes = read(sockfd, read_buffer, READ_BUFFER_LENGTH);
            if (read_bytes < 0) {
                free(read_buffer);
                free(data_buffer);
                return ERROR_RECV;
            }
        }

        #if DEBUG
        printf("\n----------------\n");
        printf("\nrecv_msg: bytes lidos (%ld bytes)\n", read_bytes);
        print_msg_bytes(read_buffer, read_bytes);
        #endif
            
        // concatena os bytes lidos ao data_buffer
        copy_to_buffer(data_buffer, &data_buffer_length, read_buffer, read_bytes);

        // procura por uma mensagem no buffer
        msg_t *found_msg = find_msg(data_buffer, data_buffer_length);
        if (found_msg) {
            #if USING_LOOPBACK
            if (!ignore_msg_server) {
                if (found_msg->fields.src == SENDER_CLIENT) {
                    ignore_msg_server = 1;

                    memcpy(msg, found_msg, sizeof(msg_t));
                    free(found_msg);
                    free(read_buffer);
                    free(data_buffer);
                    return read_bytes;
            
                }
            }
            else {
                ignore_msg_server = 0;
                return LOOPBACK_DUPLICATE;
            }
            #else
            memcpy(msg, found_msg, sizeof(msg_t));
            free(found_msg);
            free(read_buffer);
            free(data_buffer);
            return read_bytes;
            #endif
        }
    }
}

// imprime os campos da mensagem
void print_msg(msg_t *msg) {
    print_msg_fields(get_msg_field(msg, MSG_FIELD_START_MARKER), 
                     get_msg_field(msg, MSG_FIELD_DATA_LENGTH), 
                     get_msg_field(msg, MSG_FIELD_SEQUENCE), 
                     get_msg_field(msg, MSG_FIELD_TYPE),
                     msg->fields.data, 
                     get_msg_field(msg, MSG_FIELD_CRC));
}

// imprime os bytes da mensagem
void print_msg_bytes(uint8_t *msg, uint64_t msg_length) {
    for (uint64_t i = 0; i < msg_length; i++) {
        printf("0x%x ", msg[i]);
    }
    printf("\n");
}

// retorna o campo indicado da mensagem
uint8_t get_msg_field(msg_t *msg, int msg_field) {
    switch (msg_field) {
        case MSG_FIELD_START_MARKER: 
            return msg->bytes[0];

        case MSG_FIELD_DATA_LENGTH:
            return msg->bytes[1] & 0b00111111;

        case MSG_FIELD_SEQUENCE: 
            return ( (msg->bytes[1] & 0b11000000) >> 6 ) | 
                   ( (msg->bytes[2] & 0b00000111) << 2 );

        case MSG_FIELD_TYPE:
            return (msg->bytes[2] & 0b11111000) >> 3;
            
        case MSG_FIELD_CRC: 
            return msg->bytes[sizeof(msg->bytes) - 1];

        default: 
            return 0;
    }
}

// retorna o nome correspondente ao tipo de mensagem
char *get_msg_type_name(uint8_t type) {
    switch (type) {
        case MSG_TYPE_ACK:      return "ACK";
        case MSG_TYPE_NACK:     return "NACK";
        case MSG_TYPE_LIST:     return "LIST";
        case MSG_TYPE_DOWNLOAD: return "DOWNLOAD";
        case MSG_TYPE_DISPLAY:  return "DISPLAY";
        case MSG_TYPE_FILEDESC: return "FILE DESC.";
        case MSG_TYPE_DATA:     return "DATA";
        case MSG_TYPE_END:      return "END";
        case MSG_TYPE_ERROR:    return "ERROR";
        default:                return "INVALID";
    }
}

// operações crc ==============================================================

// retorna o input para o crc, que é a mensagem tirando o 
// byte inicial (start_marker) e byte final (crc)
uint8_t *get_crc_input(msg_t *msg, uint8_t msg_length) {
    uint8_t *input = (uint8_t *) malloc(msg_length - 2);
    assert_ptr(input, "crc input calculation", EXIT_FAILURE);
    
    memcpy(input, &(msg->bytes[1]), msg_length - 2);

    #if DEBUG
    printf("crc input\n");
    print_msg_bytes(input, msg_length - 2);
    #endif
    
    return input;
}

// retorna o valor para o campo de crc8
uint8_t calculate_crc(uint8_t *input, uint8_t length) {
    uint8_t poly = 0x07;  // 100000111 (com 1 mais significativo implicito)
    uint8_t crc = 0x00;   // valor inicial do crc

    for (size_t i = 0; i < length; i++) {
        crc ^= input[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            // se o bit mais significativo é 1
            if (crc & 0x80) {
                crc = (crc << 1) ^ poly; 
            } else {
                crc <<= 1;
            }
        }
    }

    #if DEBUG
    printf("\ncalculate_crc\n");
    printf("input\n");
    print_msg_bytes(input, length);
    printf("output\n");
    printf("crc %u 0x%x\n", crc, crc);
    #endif

    return crc;
}

// faz a checagem do crc realizando verificando se a divisão é 0
int check_crc(msg_t *msg, uint8_t msg_length) {
    // input é a mensagem tirando o byte inicial (start_marker)
    uint8_t *input = (uint8_t *) malloc(msg_length - 1);
    assert_ptr(input, "crc check input", EXIT_FAILURE);
    memcpy(input, &(msg->bytes[1]), msg_length - 1);

    #if DEBUG
    printf("\ncrc to check\n");
    print_msg_bytes(input, msg_length -1);
    #endif
    
    uint8_t crc = calculate_crc(input, msg_length - 1);
    free(input);
    return (crc == 0);
}

// operaçoes de vlan ===========================================================

// adiciona um byte 0xff (escape byte) apos bytes de reconhecimento vlan
// retorna o novo tamanho da mensagem
uint64_t add_vlan_escape_bytes(uint8_t **msg, uint64_t msg_length) {
    assert_ptr(msg, "add vlan escape bytes null msg", EXIT_FAILURE);
    assert_ptr(*msg, "add vlan espace bytes null *msg", EXIT_FAILURE);

    // calculando o novo tamanho da mensagem
    uint8_t new_length = msg_length;
    for (uint8_t i = 0; i < msg_length; i++) {
        if ((*msg)[i] == 0x88 || (*msg)[i] == 0x81) {
            new_length++;
        }
    }

    // alocando espaco para a nova mensagem
    uint8_t *aux = (uint8_t *) malloc(new_length);
    assert_ptr(aux, "add vlan escape bytes malloc", EXIT_FAILURE);

    // copiando os dados
    for (uint64_t i = 0, j = 0; i < msg_length; i++, j++) {        
        aux[j] = (*msg)[i];

        if ((*msg)[i] == 0x88 || (*msg)[i] == 0x81) {
            aux[j+1] = 0xff;
            j++;
        }
    }
    
    #if DEBUG
    printf("add_vlan_escape_bytes:\n");
    printf("input\n");
    print_msg_bytes((*msg), sizeof(msg_t));
    
    printf("output\n");
    print_msg_bytes(aux, new_length);
    #endif

    free((*msg));
    (*msg) = aux;

    return new_length;
}

// remove os bytes 0xff (escape byte) apos bytes de reconhecimento vlan
// retorna o novo tamanho da mensagem
uint64_t remove_vlan_escape_bytes(uint8_t **msg, uint64_t msg_length) {
    assert_ptr(msg, "remove vlan escape bytes null msg", EXIT_FAILURE);
    assert_ptr(*msg, "remove vlan espace bytes null *msg", EXIT_FAILURE);
    
    if (msg_length <= 0) {
        return 0;
    }

    uint8_t *aux = (uint8_t *) malloc(msg_length);
    assert_ptr(aux, "remove vlan escape bytes malloc", EXIT_FAILURE);

    uint64_t new_length = msg_length;
    for (uint64_t i = 0, j = 0; i < msg_length; i++, j++) {        
        aux[j] = (*msg)[i];

        if ((*msg)[i] == 0x88 || (*msg)[i] == 0x81) {
            // pula o byte de escape 0xff apos byte de vlan
            new_length--;
            i++;
        }
    }

    aux = realloc(aux, new_length);
    assert_ptr(aux, "aux realloc remove vlan bytes", EXIT_FAILURE);

    #if DEBUG
    printf("\nremove_vlan_escape_bytes:\n");
    printf("input\n");
    print_msg_bytes((*msg), msg_length);
    
    printf("output\n");
    print_msg_bytes(aux, new_length);
    #endif

    free((*msg));
    (*msg) = aux;

    return new_length;
}

// send messages ===============================================================

// manda uma mensagem de ack
void send_ack(uint8_t msg_seq, int sockfd, uint8_t src) {
    msg_t *ack_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(ack_msg, "ack msg malloc", EXIT_FAILURE);

    uint8_t data[DATA_FIELD_LENGTH];
    memset(data, 0, DATA_FIELD_LENGTH);
    memcpy(data, &(msg_seq), DATA_FIELD_LENGTH);

    uint64_t ack_msg_length = create_msg(&ack_msg, 1, 0, MSG_TYPE_ACK, data, src);
    send_msg(ack_msg, ack_msg_length, sockfd);
    free(ack_msg);

    #if DEBUG
    printf("%d ", msg_seq);
    #endif
}

// mandar uma mensagem de nack
void send_nack(uint8_t msg_seq, int sockfd, uint8_t src) {
    msg_t *nack_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(nack_msg, "nack msg malloc", EXIT_FAILURE);

    uint8_t data[DATA_FIELD_LENGTH];
    memset(data, 0, DATA_FIELD_LENGTH);
    memcpy(data, &(msg_seq), DATA_FIELD_LENGTH);

    uint64_t nack_msg_length = create_msg(&nack_msg, 1, 0, MSG_TYPE_NACK, data, src);
    send_msg(nack_msg, nack_msg_length, sockfd);
    free(nack_msg);

    #if DEBUG
    printf("\nNACK ENVIADO %d\n\n", msg_seq);
    #endif
}

// mandar uma mensagem de end tx
void send_end_tx(int sockfd, uint8_t src) {
    msg_t *endtx_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(endtx_msg, "end tx msg malloc", EXIT_FAILURE);

    uint64_t msg_length = create_msg(&endtx_msg, 0, 0, MSG_TYPE_END, NULL, src);
    send_msg(endtx_msg, msg_length, sockfd);
    free(endtx_msg);

    #if DEBUG
    printf("\nEND_TX ENVIADO\n");
    #endif
}

// manda uma mensagem de erro
void send_error(uint8_t error_type, int sockfd, uint8_t src) {
    msg_t *error_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(error_msg, "error msg malloc", EXIT_FAILURE);

    uint8_t data[DATA_FIELD_LENGTH];
    memset(data, 0, DATA_FIELD_LENGTH);
    memcpy(data, &(error_type), DATA_FIELD_LENGTH);

    uint64_t msg_length = create_msg(&error_msg, 1, 0, MSG_TYPE_ERROR, data, src);
    send_msg(error_msg, msg_length, sockfd);
    free(error_msg);

    #if DEBUG
    printf("\nERROR %u ENVIADO\n", error_type);
    #endif
}

// manda uma mensagem de mostrar na tela como o nome do arquivo a ser mostrado
void send_display(char *file_name, uint8_t msg_seq, int sockfd, uint8_t src) {
    msg_t *display_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(display_msg, "filedesc msg malloc", EXIT_FAILURE);

    uint64_t msg_length = create_msg(&display_msg, 
                                     strlen(file_name), 
                                     msg_seq, 
                                     MSG_TYPE_DISPLAY, 
                                     (uint8_t *) file_name, 
                                     SENDER_SERVER);
    send_msg(display_msg, msg_length, sockfd);
    free(display_msg);

    #if DEBUG
    printf("\nDISPLAY ENVIADO %s\n", file_name);
    #endif
}

// manda uma mensagem de descritor de arquivo
void send_filedesc(uint8_t file_size, uint8_t msg_seq, int sockfd, uint8_t src) {
    msg_t *filedesc_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(filedesc_msg, "filedesc msg malloc", EXIT_FAILURE);

    uint8_t data[DATA_FIELD_LENGTH];
    memset(data, 0, DATA_FIELD_LENGTH);
    memcpy(data, &(file_size), DATA_FIELD_LENGTH);

    uint64_t msg_length = create_msg(&filedesc_msg, 
                                     1, 
                                     msg_seq, 
                                     MSG_TYPE_FILEDESC, 
                                     data, 
                                     src);
    send_msg(filedesc_msg, msg_length, sockfd);
    free(filedesc_msg);

    #if DEBUG
    printf("\nFILEDESC ENVIADO %u\n", file_size);
    #endif
}

// manda uma mensagem para listar arquivos do servidor
void send_list(int sockfd, uint8_t src) {
    msg_t *list_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(list_msg, "list msg malloc", EXIT_FAILURE);

    uint64_t msg_length = create_msg(&list_msg, 0, 0, MSG_TYPE_LIST, NULL, src);
    send_msg(list_msg, msg_length, sockfd);
    free(list_msg);

    #if DEBUG
    printf("\nLIST ENVIADO\n");
    #endif
}

// manda uma mensagem para baixar um arquivo do servidor
void send_download(char *file_name, int sockfd, uint8_t src) {
    msg_t *download_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(download_msg, "download msg malloc", EXIT_FAILURE);

    uint64_t msg_length = create_msg(&download_msg, 
                                     strlen(file_name), 
                                     0, 
                                     MSG_TYPE_DOWNLOAD, 
                                     (uint8_t *) file_name, 
                                     src);
    send_msg(download_msg, msg_length, sockfd);
    free(download_msg);

    #if DEBUG
    printf("\nDOWNLOAD ENVIADO %s\n", file_name);
    #endif
}

// manda uma mensagem de finalização de programa
void send_done(int sockfd, uint8_t src) {
    msg_t *done_msg = (msg_t *) malloc(sizeof(msg_t));
    assert_ptr(done_msg, "done msg malloc", EXIT_FAILURE);

    uint64_t msg_length = create_msg(&done_msg, 
                                     0, 
                                     0, 
                                     MSG_TYPE_DONE, 
                                     NULL, 
                                     SENDER_SERVER);
    send_msg(done_msg, msg_length, sockfd);
    free(done_msg);

    #if DEBUG
    printf("\nDONE ENVIADO\n");
    #endif
}

// =============================================================================