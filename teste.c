#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <ctype.h>

#include <netinet/tcp.h> // Para não dar problemas na função bind

typedef int bool;
#define true 1
#define false 0

#define BUF_SIZE 1024
#define MAX_WORDS 50

typedef struct {	// struct que armazena as palavras bloqueadas pelo administrador
	char word[20];
} Word;

char * filteredString (char * msgToSend);
int compareWords(const char *word1, const char *word2);


int main(int argc, char *argv[]) {
	
	char message[] = "Ola merda carolina fodasse banana merda boavida e fixe";

	char *filteredMessage = filteredString(message);

	printf("Mensagem filtrada: %s\n\n", filteredMessage);

	exit(0);
}

int compareWords(const char *palavra1, const char *palavra2) {
    int i = 0;
    while (palavra1[i] != '\0' && palavra2[i] != '\0') {
        // Converte para minúsculo antes da comparação
        char char1 = tolower(palavra1[i]);
        char char2 = tolower(palavra2[i]);

        if (char1 != char2) {
            return 1;
        }
        i++;
    }

  // Verifica se ambas as strings terminaram
    return 0;
}

/*char * filteredString (char * msgToSend){

	FILE *file = fopen("words.bin", "rb");
	if (file == NULL) {
        return NULL;
    }

	Word words[MAX_WORDS];
	size_t num_words = fread(words, sizeof(Word), MAX_WORDS, file);

	fclose(file);

    for (size_t i = 0; i < num_words; i++) {
        size_t len = strlen(words[i].word);
        if (len > 0 && words[i].word[len - 1] == ' ') {
            words[i].word[len - 1] = '\0'; // Substitui o último caractere por '\0'
        }
}   

	if (num_words == 0) {
        return msgToSend; // Se não há palavras para filtrar, retornamos a mensagem original.
    }

    
    char *frase_dividida = strtok(msgToSend, " ");
    char *palavras[MAX_WORDS]; // Array para armazenar as palavras
    int num_palavras = 0;

    while (frase_dividida != NULL && num_palavras < MAX_WORDS) {

        palavras[num_palavras] = frase_dividida;
        num_palavras++;
        frase_dividida = strtok(NULL, " ");
    }

    char *frase_filtrada = malloc(strlen(msgToSend) + 1);

    //char *frase_filtrada[BUF_SIZE] = "";
    int num_palavras_filtradas = 0;

    for(int j=0; j<num_palavras; j++){
        bool existe = false; 
        for(int i=0; i<num_words; i++){
            if(compareWords(words[i].word, palavras[j]) == 0){
                existe = true; 
            }
        }

        if(!existe){
            strcat(frase_filtrada, palavras[j]);
            strcat(frase_filtrada, " ");
        }
    }
    free(frase_filtrada);
    return frase_filtrada;

}*/

char *filteredString(char *msgToSend) {
    FILE *file = fopen("words.bin", "rb");
    if (file == NULL) {
        return NULL;
    }

    Word words[MAX_WORDS];
    size_t num_words = fread(words, sizeof(Word), MAX_WORDS, file);
    fclose(file);

    if (num_words == 0) {
        return msgToSend; // Se não há palavras para filtrar, retornamos a mensagem original.
    }

    char frase_filtrada[BUF_SIZE] = ""; // Inicializa o buffer para a frase filtrada

    char *frase_dividida = strtok(msgToSend, " ");
    char *palavras[MAX_WORDS]; // Array para armazenar as palavras
    int num_palavras = 0;

    while (frase_dividida != NULL && num_palavras < MAX_WORDS) {
        palavras[num_palavras] = frase_dividida;
        num_palavras++;
        frase_dividida = strtok(NULL, " ");
    }

    for (int j = 0; j < num_palavras; j++) {
        bool existe = false;
        for (int i = 0; i < num_words; i++) {
            if (compareWords(words[i].word, palavras[j]) == 0) {
                existe = true;
                break;
            }
        }

        if (!existe) {
            strcat(frase_filtrada, palavras[j]);
            strcat(frase_filtrada, " ");

            // Verifica se o tamanho da frase filtrada excede o buffer máximo
            if (strlen(frase_filtrada) >= BUF_SIZE) {
                return NULL; // Se exceder, retornamos NULL indicando erro
            }
        }
    }

    // Remova o espaço extra no final, se houver
    size_t len = strlen(frase_filtrada);
    if (len > 0 && frase_filtrada[len - 1] == ' ') {
        frase_filtrada[len - 1] = '\0';
    }

    return strdup(frase_filtrada); // Retorna uma cópia alocada dinamicamente da frase filtrada
}




