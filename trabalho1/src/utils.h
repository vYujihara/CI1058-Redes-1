#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>

// =============================================================================

// termina o programa com mensagem de erro
void error(const char *err_msg, int code);

// verifica se houve erro em var
void assert_var(long var, char *err_msg, int exit_code);

// verifica se o ponteiro é nulo
void assert_ptr(void *ptr, char *err_msg, int exit_code);

// retorna timestamp em milisegundos
long long timestamp();

// imprima a barra de progresso de transferencia do arquivo
void print_progress_bar(uint64_t sent_msgs, uint64_t total_msgs);

// imprime o indicador de transferencia de arquivo
void print_progress_indicatior(uint64_t iteration);

// =============================================================================

#endif  // __UTILS_H__