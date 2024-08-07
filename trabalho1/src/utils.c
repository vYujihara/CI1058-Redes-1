#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

// =============================================================================

// termina o programa com mensagem de erro
void error(const char *err_msg, int code) {
    fprintf(stderr, "\nError: %s.\n", err_msg);
    exit(code);
}

// verifica se houve erro em var
void assert_var(long var, char *err_msg, int exit_code) {
    if (var < 0) {
        error(err_msg, exit_code);
    }
}

// verifica se o ponteiro é nulo
void assert_ptr(void *ptr, char *err_msg, int exit_code) {
    if (ptr == NULL) {
        error(err_msg, exit_code);
    }
}

// retorna timestamp em milisegundos
long long timestamp() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec*1000 + tp.tv_usec/1000;
}

// imprima a barra de progresso de transferencia do arquivo
void print_progress_bar(uint64_t sent_msgs, uint64_t total_msgs) {
    if (sent_msgs % 3 == 0) {
        int bar_width = 50;
        float progress = (float)(sent_msgs) / total_msgs;

        printf("[");
        int pos = bar_width * progress;
        for (int i = 0; i < bar_width; ++i) {
            if (i <= pos) printf("=");
            else printf("-");
        }
        printf("] %d%% (%ld Kb/%ld Kb)\r", 
                (int)(progress * 100), 
                sent_msgs * 63 /1000, 
                total_msgs * 63 /1000);
    }
}

// imprime o indicador de transferencia de arquivo
void print_progress_indicatior(uint64_t iteration) {
    const char *indicators[] = { "---", "+++", "***"};
    printf("%s\r", indicators[iteration % 3]);
}

// =============================================================================
