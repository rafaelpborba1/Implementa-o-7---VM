# Implementa-o-7---VM

# Gerenciador de Memória Virtual (MMU & TLB)

Simulador de gerenciamento de memória virtual com paginação sob demanda, tabela de páginas e TLB utilizando os algoritmos de substituição FIFO e LRU.

## Sistema Operacional
- Desenvolvido e testado em: Linux (Ubuntu)

## Descrição dos Arquivos .c
- `vm.c`: Arquivo principal contendo:
  - Validação dos argumentos da linha de comando e leitura segura do arquivo de entrada.
  - Gerenciamento da TLB com 16 entradas, busca, atualização e invalidação em caso de substituição de página.
  - Tratamento de *page faults* com leitura sob demanda a partir do `BACKING_STORE.bin` para a memória física.

## Como Compilar
Para compilar o programa com o Makefile:
```bash
make

## Como Executar

./vm address.txt fifo fifo
./vm address.txt fifo lru
./vm address.txt lru fifo
./vm address.txt lru lru

