#include "redes.h"
#include "utils.h"
#include "window.h"
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// char *get_file_path___(char *dir_path, char *file_name) {
//     char *file_path = (char *) malloc(strlen(file_name) + strlen(dir_path) + 1);
//     assert_ptr(file_path, "file path malloc", EXIT_FAILURE);

//     // concatenando o path
//     strcpy(file_path, dir_path);
//     strcat(file_path, file_name);

//     return file_path;
// }

// char *get_file_name(char *input, char *dir_path) {
//     char *file_name = input + strlen("get") + 1;
//     file_name[strcspn(file_name, "\n")] = '\0';

//     printf("nome do arquivo: '%s'\n", file_name);
//     file_name = get_file_path___(dir_path, file_name);
//     return file_name;
// }

// int request_download(int sockfd) {
//     msg_t *msg = malloc(sizeof(msg_t));
//     assert_ptr(msg, "msg malloc", EXIT_FAILURE);

//     int trys = 0;

//     while (1) {
//         if (!wait_for_response(msg, sockfd, recv_msg)) {
//             if ((++trys) > MAX_CLIENT_TRYS) {
//                 error("timeout client", EXIT_FAILURE);
//             }
//             continue;
//         }
//         switch (msg->fields.type) {
//         case MSG_TYPE_ACK:
//             // recebeu ack, espera por filedesc
//             break;
//         case MSG_TYPE_FILEDESC:
//             printf("recebeu filedesc\n");
//             send_ack(0, sockfd, SENDER_CLIENT);
//             printf("tamanho do arquivo: %u\n", msg->fields.data[0]);
//             ack = 1;
//             return ACK;
//         case MSG_TYPE_ERROR:
//             return ERROR;
//         default:
//             printf("default\n");
//             printf("msg_type %s\n", get_msg_type_name(msg->fields.type));
//             break;
//         }
//     }
// }

// void download_video_client_side() {

// }



int main(int argc, char **argv) {
    int sockfd = create_raw_socket(NETWORK_INTERFACE);

    char input[128];
    instructions();
    while (1) {
        memset(input, 0, 128);
        fgets(input, 128, stdin);

        if (strstr(input, "lsc")) {
            list_client();
        }
        else if (strstr(input, "lss")) {
            list_server_client_side(sockfd);
        }
        else if (strstr(input, "get")) {
            download_video_client_side(sockfd, input);
        }
        else if (strstr(input, "end")) {
            send_done(sockfd, SENDER_CLIENT);
            break;
        }
        else if (strstr(input, "cmd")) {
            instructions();
        }
        else if (strstr(input, "clr")) {
            system("clear");
            instructions();
        }
        else {
            printf("cormando invalido\n");
        }
    }

    // FILE *client_file = fopen(file_name, "wb");
    // assert_ptr(client_file, "failed to open client file", EXIT_FAILURE);

    // recv_video(sockfd, client_file, file_name);

    close(sockfd);
    return 0;
}