#include "redes.h"
#include "utils.h"
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv) {
    int sockfd = create_raw_socket(NETWORK_INTERFACE);

    int end_tx = 0;

    while (!end_tx) {
        msg_t *msg = malloc(sizeof(msg_t));
        assert_ptr(msg, "msg malloc", EXIT_FAILURE);

        // espera por comando do cliene
        if (!wait_for_response(msg, sockfd, recv_msg)) {
            continue;
        }

        switch (msg->fields.type) {
        case MSG_TYPE_LIST:
            list_server_server_side(sockfd);
            break;
            
        case MSG_TYPE_DOWNLOAD:
            download_video_server_side(sockfd, msg);
            break;
        
        case MSG_TYPE_DONE:
            printf("cliente desconectado, encerrando...\n");
            end_tx = 1;
            break;
        
        default:
            break;
        }
    }

    close(sockfd);
    return 0;
}