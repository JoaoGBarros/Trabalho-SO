#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/*
*   Caminho do cliente: 
*       1. Cliente entra na barbearia (Se a barbearia estiver cheia, o cliente vai embora)
*       2. Cliente senta no sofá (Se o sofá estiver cheio, o cliente aguarda)
*       3. Cliente senta em uma cadeira (Se todas as cadeiras estiverem ocupadas, o cliente senta no sofá)
*       4. Cliente paga pelo corte (Cliente corta o cabelo e paga pelo corte)
*       5. Cliente sai da barbearia
*
*   Caminho do barbeiro:
*       1. Barbeiro dorme (Se não houver clientes nas cadeiras e ainda houver clientes pendentes, o barbeiro dorme)
*       2. Barbeiro corta o cabelo (Barbeiro corta o cabelo do cliente)
*       3. Barbeiro aguarda pagamento (Barbeiro aguarda o pagamento do cliente)
*       4. Barbeiro recebe pagamento (Barbeiro recebe o pagamento do cliente)
*       5. Barbeiro encerra expediente (Barbeiro encerra o expediente após atender todos os clientes)
*/

#define cadeirasQuantidade 3 // Numa barbearia existem três cadeiras,
#define totalBarbeiros 3 // três barbeiros e
#define capacidadeSofa 4 // um local de espera que pode acomodar quatro pessoas num sofá.
#define maxClientes 20 // O número máximo de clientes que pode estar na sala é de 20. 
#define totalClientes 40 // Número total de Clientes que tentarão entrar na barbearia.

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para controle de acesso às variáveis compartilhadas
pthread_cond_t clienteChegou = PTHREAD_COND_INITIALIZER;
pthread_cond_t barbeiroDisponivel = PTHREAD_COND_INITIALIZER;
pthread_cond_t pagamentoRealizado = PTHREAD_COND_INITIALIZER;
pthread_cond_t clientePagando = PTHREAD_COND_INITIALIZER;

int clientesEsperando = 0; // Clientes esperando para serem atendidos
int clientesSofa = 0; // Clientes sentados no sofá
int clientesCadeiras = 0; // Cientes sentados nas cadeiras
int clientesPendentes = totalClientes; // Clientes que ainda não foram atendidos
int caixaLivre = 1; // Indica se a caixa está livre para pagamento

void entrarLoja(long id) {

    // Se tiver mais cliente do que a barbearia aguenta, o cliente vai embora
    if (clientesEsperando >= maxClientes) {
        printf("Cliente %ld: Barbearia lotada, indo embora.\n", id);
        pthread_exit(NULL);
    }

    // Se não, o cliente entra na barbearia
    clientesEsperando++;
    printf("Cliente %ld: Entrou na barbearia.\n", id);
}

void sentarSofa(long id) {

    // Se o sofá estiver cheio, o cliente aguarda
    while (clientesSofa >= capacidadeSofa) {
        pthread_cond_wait(&barbeiroDisponivel, &mutex);
    }

    // Se não, o cliente senta no sofá
    clientesEsperando--;
    clientesSofa++;
    printf("Cliente %ld: Sentou no sofá.\n", id);
}

void sentarCadeira(long id) {

    // Se todas as cadeiras estiverem ocupadas, o cliente aguarda
    while (clientesCadeiras >= cadeirasQuantidade) {
        pthread_cond_wait(&barbeiroDisponivel, &mutex);
    }

    // Se não, o cliente sai do sofá e senta em uma cadeira
    clientesSofa--;
    clientesCadeiras++;
    printf("Cliente %ld: Sentou na cadeira.\n", id);
}

void pagar(long id) {
    printf("Cliente %ld: Pagando pelo corte.\n", id);

    // Se a caixa não estiver livre, o cliente aguarda
    while (!caixaLivre) {
        pthread_cond_wait(&clientePagando, &mutex);
    }
    // Se o caixa estiver livre, o cliente ocupa o caixa
    caixaLivre = 0;
    pthread_cond_signal(&pagamentoRealizado);
}

void sairLoja(long id) {
    sleep(1);
    caixaLivre = 1; // Libera o caixa para o próximo cliente
    printf("Cliente %ld: Pagamento concluído e saiu da barbearia.\n", id);
    clientesCadeiras--; // Libera a cadeira
    clientesPendentes--; // Diminui o número de clientes pendentes

    if (clientesPendentes == 0) {
        // Notifica TODOS os barbeiros para encerrarem
        pthread_cond_broadcast(&clienteChegou);
        pthread_cond_broadcast(&pagamentoRealizado);
        pthread_cond_broadcast(&clientePagando);
    }

    pthread_cond_signal(&barbeiroDisponivel);
}

void cortarCabelo(long id) {
    printf("Barbeiro %ld: Cortando cabelo.\n", id);
    sleep(2);
}

void aceitarPagamento(long id) {
    printf("Barbeiro %ld: Aguardando pagamento do cliente.\n", id);
    pthread_cond_signal(&clientePagando);
    
    // Espera pelo pagamento, mas verifica se ainda há clientes pendentes
    while (clientesPendentes > 0) {
        pthread_cond_wait(&pagamentoRealizado, &mutex);
        if (clientesPendentes == 0) {
            break;
        }
    }
}

void* cliente(void* arg) {
    long id = (long)arg; // Identificador do cliente

    pthread_mutex_lock(&mutex);
    entrarLoja(id); // Cliente entra na loja
    sentarSofa(id); // Cliente senta no sofá
    sentarCadeira(id); // Cliente senta na cadeira
    pthread_cond_signal(&clienteChegou); // Notifica os barbeiros
    pthread_mutex_unlock(&mutex);
    sleep(2);

    pthread_mutex_lock(&mutex);
    pagar(id); 
    sairLoja(id);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void* barbeiro(void* arg) {
    long id = (long)arg; // Identificador do barbeiro

    while (1) {
        pthread_mutex_lock(&mutex);

        // Verificar se todos os clientes já foram atendidos antes de iniciar um novo ciclo.
        if (clientesPendentes == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Verifica se há clientes nas cadeiras ou se ainda há clientes pendentes
        while (clientesCadeiras == 0 && clientesPendentes > 0) {
            printf("Barbeiro %ld: Dormindo...\n", id);
            pthread_cond_wait(&clienteChegou, &mutex);

            // Verificar se o barbeiro foi acordado por um sinal falso ou porque o último cliente saiu.
            if (clientesPendentes == 0) {
                pthread_mutex_unlock(&mutex);
                return NULL;
            }
        }
        
        // Garante que, mesmo após sair do loop de espera, não haja clientes pendentes.
        if (clientesPendentes == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);
        cortarCabelo(id);
        pthread_mutex_lock(&mutex);

        aceitarPagamento(id);

        // Verificar se o último cliente foi atendido durante o processo de pagamento.
        if (clientesPendentes == 0) {
            printf("Barbeiro %ld: Encerrando expediente.\n", id);
            pthread_mutex_unlock(&mutex);
            break;
        }

        printf("Barbeiro %ld: Pagamento recebido.\n", id);
        pthread_mutex_unlock(&mutex);
    }
}

int main() {
    pthread_t barbeiros[totalBarbeiros], clientes[totalClientes];

    for (long i = 0; i < totalBarbeiros; i++) {
        pthread_create(&barbeiros[i], NULL, barbeiro, (void*)i); // Cria as threads dos barbeiros
    }

    for (long i = 0; i < totalClientes; i++) {
        sleep(1);
        pthread_create(&clientes[i], NULL, cliente, (void*)i); // Cria as threads dos clientes
    }

    for (int i = 0; i < totalClientes; i++) {
        pthread_join(clientes[i], NULL); // suspende a execução do thread até que o thread de destino termine
    }

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&clienteChegou);
    pthread_cond_broadcast(&pagamentoRealizado);
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < totalBarbeiros; i++) {
        pthread_join(barbeiros[i], NULL);
    }

    printf("Todos os clientes foram atendido!\n");
    return 0;
}