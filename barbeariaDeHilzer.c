#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/*
*   Caminho do cliente: 
*       1. Cliente entra na barbearia (Se a barbearia estiver cheia, o cliente vai embora)
*       2. Cliente senta no sofá (Se o sofá estiver cheio, o cliente aguarda)
*       3. Cliente senta em uma cadeira (Se todas as cadeiras estiverem ocupadas, o cliente senta no sofá)
*       4. Cliente corta o cabelo (Cliente corta o cabelo)
*       5. Cliente paga pelo corte (Cliente corta o cabelo e paga pelo corte)
*       6. Cliente sai da barbearia
*
*   Caminho do barbeiro:
*       1. Barbeiro dorme (Se não houver clientes nas cadeiras e ainda houver clientes pendentes, o barbeiro dorme)
*       2. Barbeiro corta o cabelo (Barbeiro corta o cabelo do cliente)
*       3. Barbeiro aguarda pagamento (Barbeiro aguarda o pagamento do cliente)
*       4. Barbeiro recebe pagamento (Barbeiro recebe o pagamento do cliente)
*       5. Barbeiro encerra expediente (Barbeiro encerra o expediente após atender todos os clientes)
*/

#define qtdCadeiras 3 // Numa barbearia existem três cadeiras,
#define totalBarbeiros 3 // três barbeiros e
#define maxSofa 4 // um local de espera que pode acomodar quatro pessoas num sofá.
#define maxClientes 20 // O número máximo de clientes que pode estar na sala é de 20. 

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para controle de acesso às variáveis compartilhadas
pthread_cond_t clienteChegou = PTHREAD_COND_INITIALIZER;
pthread_cond_t barbeiroDisponivel = PTHREAD_COND_INITIALIZER;
pthread_cond_t pagamentoRealizado = PTHREAD_COND_INITIALIZER;
pthread_cond_t clientePagando = PTHREAD_COND_INITIALIZER;

int clientesEsperando = 0; // Clientes esperando para serem atendidos
int clientesSofa = 0; // Clientes sentados no sofá
int clientesCadeiras = 0; // Cientes sentados nas cadeiras
int caixaLivre = 1; // Indica se a caixa está livre para pagamento
int totalClientes = 0; // Número total de Clientes que tentarão entrar na barbearia.
int clientesPendentes = 0; // Clientes que ainda não foram atendidos

int EntrarNaLoja(long id) {
    int totalClientesBarbearia = clientesEsperando + clientesSofa + clientesCadeiras;

    // Se tiver mais cliente do que a barbearia aguenta, o cliente vai embora
    if (totalClientesBarbearia >= maxClientes) {
        printf("Cliente %ld: Barbearia lotada, indo embora.\n", id);
        clientesPendentes--;
        return 0;
    }
    clientesEsperando++;
    printf("Cliente %ld: Entrou na barbearia. Total: %d\n", id, totalClientesBarbearia + 1);
    return 1;
}

void SentarNoSofa(long id) {

    // Enquanto o sofá estiver cheio, o cliente aguarda
    while (clientesSofa >= maxSofa) {
        pthread_cond_wait(&barbeiroDisponivel, &mutex);
    }
    clientesEsperando--;
    clientesSofa++;
    printf("Cliente %ld: Sentou no sofá.\n", id);
}

void SentarNaCadeira(long id) {
    
    // Se todas as cadeiras estiverem ocupadas, o cliente aguarda no sofá
    while (clientesCadeiras >= qtdCadeiras) {
        pthread_cond_wait(&barbeiroDisponivel, &mutex);
    }
    clientesSofa--;
    clientesCadeiras++;
    printf("Cliente %ld: Sentou na cadeira.\n", id);
    pthread_cond_signal(&clienteChegou);
}

void Pagar(long id) {
    printf("Cliente %ld: Pagando pelo corte.\n", id);

    // Se a caixa não estiver livre, o cliente aguarda
    while (!caixaLivre) {
        pthread_cond_wait(&clientePagando, &mutex);
    }
    caixaLivre = 0;
    pthread_cond_signal(&pagamentoRealizado);
}

void SairDaLoja(long id) {
    caixaLivre = 1; // Libera o caixa para o próximo cliente
    printf("Cliente %ld: Pagamento concluído e saiu da barbearia.\n", id);
    clientesCadeiras--; // Libera a cadeira
    clientesPendentes--; // Diminui o número de clientes pendentes

    if (clientesPendentes == 0) {
        pthread_cond_broadcast(&clienteChegou);
        pthread_cond_broadcast(&pagamentoRealizado);
        pthread_cond_broadcast(&clientePagando);
    }
    pthread_cond_signal(&barbeiroDisponivel);
}

void CortarCabelo(long id) {
    printf("Barbeiro %ld: Cortando cabelo.\n", id);
    sleep(2);
}

void AceitarPagamento(long id) {
    while (clientesCadeiras > 0) { // Enquanto houver clientes nas cadeiras
        printf("Barbeiro %ld: Aguardando pagamento do cliente.\n", id);
        pthread_cond_signal(&clientePagando);
        pthread_cond_wait(&pagamentoRealizado, &mutex);
    }
}

void* cliente(void* arg) {
    long id = (long)arg;// Identificador do cliente

    pthread_mutex_lock(&mutex);
    if (!EntrarNaLoja(id)) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    SentarNoSofa(id);
    SentarNaCadeira(id);
    pthread_mutex_unlock(&mutex);

    sleep(2); // Simula tempo de corte

    pthread_mutex_lock(&mutex);
    Pagar(id);
    SairDaLoja(id);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void* barbeiro(void* arg) {
    long id = (long)arg;// Identificador do barbeiro

    while (1) {
        pthread_mutex_lock(&mutex);
        if (clientesPendentes == 0) {// Verificar se todos os clientes já foram atendidos antes de iniciar um novo ciclo.
            pthread_mutex_unlock(&mutex);
            break;
        }
        while (clientesCadeiras == 0 && clientesPendentes > 0) {// Verifica se há clientes nas cadeiras ou se ainda há clientes pendentes
            printf("Barbeiro %ld: Dormindo...\n", id);
            pthread_cond_wait(&clienteChegou, &mutex);
            if (clientesPendentes == 0) break;// Verificar se o barbeiro foi acordado por um sinal falso ou porque o último cliente saiu.
        }
        if (clientesPendentes == 0) { // Garante que, mesmo após sair do loop de espera, não haja clientes pendentes.
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);

        CortarCabelo(id);

        pthread_mutex_lock(&mutex);
        AceitarPagamento(id);
        if (clientesPendentes == 0) {// Verificar se o último cliente foi atendido durante o processo de pagamento.
            printf("Barbeiro %ld: Encerrando expediente.\n", id);
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "É necessário ter o número de clientes, utilize: %s <totalClientes>\n", argv[0]);
        return 1;
    }

    totalClientes = atoi(argv[1]);
    clientesPendentes = totalClientes;

    if (totalClientes <= 0) {
        fprintf(stderr, "O número de clientes deve ser um inteiro positivo.\n");
        return 1;
    }

    pthread_t barbeiros[totalBarbeiros], clientes[totalClientes];

    for (long i = 0; i < totalBarbeiros; i++) {
        pthread_create(&barbeiros[i], NULL, barbeiro, (void*)i); // Cria as threads dos barbeiros
    }

    for (long i = 0; i < totalClientes; i++) {
        usleep(100000); // Tempo para a criação de clientes
        pthread_create(&clientes[i], NULL, cliente, (void*)i); // Cria as threads dos clientes
    }

    for (int i = 0; i < totalClientes; i++) {
        pthread_join(clientes[i], NULL);
    }

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&clienteChegou);
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < totalBarbeiros; i++) {
        pthread_join(barbeiros[i], NULL);
    }

    printf("Todos os clientes foram atendidos!\n");
    return 0;
}