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

typedef struct {
    char username[20];
    int port;
} User;

void erro(char *msg);
int serverConnection(int argc, char *argv[]);
void connectingToClientServer(const User user, const User conversa); // conecta-se a um server de um cliente
void createNewServer(const User user); // torna o cliente num server
void sendString(int fd, char *msg);
char *receiveString(int fd);
int compareWords(const char *word1, const char *word2);
char * filteredString (char * msgToSend);

int main(int argc, char *argv[]) {
	printf("\e[1;1H\e[2J"); 
	int fd = serverConnection(argc, argv);
	char *msgReceived = NULL;
	char msgToSend[BUF_SIZE];

	sleep(3);
	printf("\e[1;1H\e[2J"); 

	while(msgReceived == NULL || strcmp(msgReceived, "\nUntil next time! Thanks for chattingRC with us :)\n") != 0){
    free(msgReceived);
    msgReceived = receiveString(fd);
    
    User user;
    const char* compareString = "Creating a new server:";
    size_t compareLength = strlen(compareString);
    if(strncmp(msgReceived, compareString, compareLength) == 0) {
		sleep(2);	
		printf("\e[1;1H\e[2J"); 
		if (sscanf(msgReceived, "Creating a new server: %s %d", user.username, &user.port) == 2){ // recebe a string do server que diz para se tornar num server e ficar à espera de uma cliente
			printf("username: %s\n", user.username);
			createNewServer(user);

			sendString(fd, "Created server successfully\n");
			continue;
		}	
    }

    const char* compareString2 = "Start a conversation:";
    size_t compareLength2 = strlen(compareString2);

    if(strncmp(msgReceived, compareString2, compareLength2) == 0) { // recebe a string do server que diz para se conectar a um outro cliente
		printf("\n\nConnecting with another user\n");
		sleep(2);
		printf("\e[1;1H\e[2J");
		User conversa;
		if (sscanf(msgReceived, "Start a conversation: %s -> %s %d", user.username, conversa.username, &conversa.port) == 3) {
			printf("username: %s\n", user.username);
			// Connect to server
			connectingToClientServer(user, conversa);
			sendString(fd, "Connected successfully\n");
			continue;
		}
    }
    
    if(strcmp(msgReceived, "\nUntil next time! Thanks for chattingRC with us :)\n") == 0) // sai do programa
		break;
    
    fgets(msgToSend, sizeof(msgToSend), stdin);
    sendString(fd, msgToSend); 
    fflush(stdout);
	}
	sleep(3);
	printf("\e[1;1H\e[2J"); 
	close(fd);
	exit(0);
}

int serverConnection(int argc, char *argv[]) {
	int fd;
	struct sockaddr_in addr;
	struct hostent *hostPtr;

	if (argc != 3) {
		printf("client <host> <port>\n");
		fflush(stdout);
		exit(-1);
	}
	if ((hostPtr = gethostbyname(argv[1])) == 0)
		erro("Nao consegui obter endereço");

	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short)atoi(argv[2]));

	bool connection_success = true;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		erro("Socket");
		connection_success = false;
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		erro("Connect");
		connection_success = false;
	}

	if (connection_success) {
		printf("Connected with server\n\n");
		fflush(stdout);
	}

	return fd;
}

void connectingToClientServer(const User user, const User conversa) { // conecta-se ao outro cliente para conversas

	int fd;
	struct sockaddr_in addr;
	struct hostent *hostPtr;

	hostPtr = gethostbyname("127.0.0.1");

	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons(conversa.port); // conversa no porto do outro user

	bool connection_success = true;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		erro("Socket");
		connection_success = false;
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		//erro("Connect");
		printf("connect");
		connection_success = false;
	}

	if (connection_success) {
		
		char aux[100];
		sprintf(aux, "Chatting with %s in port %d\n\n", user.username, conversa.port);
		sendString(fd, aux);
		
		receiveString(fd);
		
		char *msgReceived = NULL;
		char msgToSend[BUF_SIZE-22];
		bool disconnectRequested = false; 


		while(!disconnectRequested) { // envia e recebe mensagens até alguém pressionar ENTER

			if(fork()==0){ //
				while(1){
					//printf("%s: ", user.username);
					fgets(msgToSend, sizeof(msgToSend), stdin);

					
					if(strcmp(msgToSend, "\n") == 0) {
						sendString(fd, "Disconnected\n");
						close(fd);
						disconnectRequested = true;
						break;
					}

					char *filteredMessage = filteredString(msgToSend);
					strcpy(msgToSend, filteredMessage);

					char formattedMsg[BUF_SIZE];
					sprintf(formattedMsg, "%s: %s", user.username, msgToSend);
					sendString(fd, formattedMsg); 
					fflush(stdout);
				}

				exit(0);
				
			}

			else{
				while(1){
					free(msgReceived);
					msgReceived = receiveString(fd);

					if(strcmp(msgReceived, "Disconnected\n") == 0) { 
						sendString(fd, "Disconnected\n");
						disconnectRequested = true;
						close(fd);
						break;
					}

				}

				close(fd);
				wait(NULL);
				//exit(0);
			}

			
			
		}

    fflush(stdout);
	}
}

void createNewServer(const User user) { // torna o client num server à espera de uma conexão

	int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;
	
	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(user.port); // cria o server no port designado do user

	int opt = 1;
	do { // loop para confirmar se o socket já está pronto a usar outra vez, devido aos erros da função bind
		// cria o socket
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // se der erro vai tentar outra vez
			perror("na funcao socket");
			sleep(2);
			continue;
		}

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) { // se der erro vai tentar outra vez
			perror("setsockopt");
			close(fd);
			sleep(2);
		} else {
			break;
		}

	} while (1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		erro("na funcao bind");
	if (listen(fd, 5) < 0)
		erro("na funcao listen");

	client_addr_size = sizeof(client_addr);

	while (waitpid(-1, NULL, WNOHANG) > 0);

	client = accept(fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size); // fica à espera da conexão do cliente

	if (client > 0) {
		close(fd);
		printf("Connected with another user\n");
		fflush(stdout);

		User conversa;
		
		char aux_string[BUF_SIZE];
		strcpy(aux_string, receiveString(client));
		const char* compareString = "Chatting with";
		size_t compareLength = strlen(compareString);
		if(strncmp(aux_string, compareString, compareLength) == 0) {
			sscanf(aux_string, "Chatting with %s in port %d\n\n!", conversa.username, &conversa.port);
		}

		char aux[100];
		sprintf(aux, "Chatting with %s in port %d\n\n", user.username, user.port);
		sendString(client, aux);
		
		char *msgReceived = NULL;
		char msgToSend[BUF_SIZE-22];
		bool disconnectRequested = false;

		// Começa a conversa
		while(!disconnectRequested) { // envia e recebe mensagens até alguém pressionar ENTER

			if(fork()==0){

				while(1){ // ler 
					free(msgReceived);
					msgReceived = receiveString(client);

					if(strcmp(msgReceived, "Disconnected\n")==0) { // o user desconectou-se então termina a conversa
						disconnectRequested = true; 
						sleep(1); 
						close(client);
						return;
					}
				}

				exit(0);	

			}
			else{
				while(1){
					//printf("%s: ", user.username);
					fgets(msgToSend, sizeof(msgToSend), stdin);


					if(strcmp(msgToSend, "\n") == 0) { // sai do programa mas garante que o user que se conectou saiu primeiro por causa de erros na função bind

						sendString(client, "Disconnected\n");
						free(msgReceived);
						msgReceived = receiveString(client);

						if(strcmp(msgReceived, "Disconnected\n") == 0) { 
							disconnectRequested = true;
							sleep(1);
							close(client);
							return;
							
						}
					}

					char *filteredMessage = filteredString(msgToSend);
					strcpy(msgToSend, filteredMessage);

					char formattedMsg[BUF_SIZE];
					sprintf(formattedMsg, "%s: %s", user.username, msgToSend);
					sendString(client, formattedMsg); 
					fflush(stdout);
				}	
				exit(0);	
			}
		}

    close(client);

	}
	return;
}

void sendString(int fd, char *msg) {
	write(fd, msg, 1 + strlen(msg));
}

char *receiveString(int fd) {
	int nread = 0;
	char buffer[BUF_SIZE];
	char *string;

	nread = read(fd, buffer, BUF_SIZE - 1);
	buffer[nread] = '\0';
	string = (char *)malloc(strlen(buffer) + 1);
	strcpy(string, buffer);
	if(string[0]=='\n')printf("\e[1;1H\e[2J");
	printf("%s", string);
	fflush(stdout);
	return string;
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	fflush(stdout);
	exit(-1);
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

	strcat(frase_filtrada, "\n");

    return strdup(frase_filtrada); // Retorna uma cópia alocada dinamicamente da frase filtrada
}