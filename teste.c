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


int main(int argc, char *argv[]) {
	
	char message[] = "Esta é uma mensagem de teste que contém algumas palavras para filtrar.";

	char *filteredMessage = filteredString(message);

	printf("Mensagem filtrada: %s\n", filteredMessage);

	exit(0);
}


char * filteredString (char * msgToSend){

	FILE *file = fopen("words.bin", "rb");
	if (file == NULL) {
        return;
    }

	Word words[MAX_WORDS];
	size_t num_words = fread(words, sizeof(Word), MAX_WORDS, file);

	fclose(file);

	if (num_words == 0) {
        return msgToSend; // Se não há palavras para filtrar, retornamos a mensagem original.
    }

    // Para cada palavra lida do arquivo, substituímos todas as ocorrências na mensagem.
    for (size_t i = 0; i < num_words; i++) {
        char *pos = msgToSend;
        while ((pos = strstr(pos, words[i].word)) != NULL) {
            // Substituímos a palavra encontrada por espaços.
            memset(pos, ' ', strlen(words[i].word));
            pos += strlen(words[i].word); // Avançamos a posição.
        }
    }

    return msgToSend;

}
