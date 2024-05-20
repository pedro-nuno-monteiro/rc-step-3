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
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/tcp.h> // Para não dar problemas na função bind

typedef int bool;
#define true 1
#define false 0

#define BUF_SIZE 1024
#define MAX_WORDS 50
#define MAX_CLIENTS 10

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
char *receiveString(int fd, bool mensagens);
int compareWords(const char *word1, const char *word2);
char * filteredString (char * msgToSend);

// group chat
void createGroupChatServer(const User user);
void connectToGroupChatServer(const User user, int port);
void handle_client_message(int client_fd, fd_set *master_set, int fd_max, int server_fd, const User user);

int main(int argc, char *argv[]) {
	printf("\e[1;1H\e[2J"); 
	int fd = serverConnection(argc, argv);
	char *msgReceived = NULL;
	char msgToSend[BUF_SIZE];

	sleep(2);
	printf("\e[1;1H\e[2J"); 

	while(msgReceived == NULL || strcmp(msgReceived, "\nUntil next time! Thanks for chattingRC with us :)\n") != 0) {
        
        free(msgReceived);
        msgReceived = receiveString(fd, false);
        
        User user;
        const char* compareString = "Creating a new server:";
        size_t compareLength = strlen(compareString);
        if(strncmp(msgReceived, compareString, compareLength) == 0) {
            
            sleep(2);	
            printf("\e[1;1H\e[2J"); 

            if (sscanf(msgReceived, "Creating a new server: %s %d", user.username, &user.port) == 2) { // recebe a string do server que diz para se tornar num server e ficar à espera de uma cliente
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
        
		const char* groupChatString = "New group conversation created by";
        size_t groupChatLength = strlen(groupChatString);
        
		if (strncmp(msgReceived, groupChatString, groupChatLength) == 0) {    
			sleep(2);
            printf("\e[1;1H\e[2J");
            
			if (sscanf(msgReceived, "New group conversation created by %s in port %d", user.username, &user.port) == 2) {
                printf("username: %s\n", user.username);
                createGroupChatServer(user);
                
				sendString(fd, "Created group chat server successfully\n");
                continue;
            }
        }

        const char* connectGroupChatString = "Joining group conversation in port";
        size_t connectGroupChatLength = strlen(connectGroupChatString);
        if (strncmp(msgReceived, connectGroupChatString, connectGroupChatLength) == 0) {
            printf("\n\nConnecting to group chat\n");
            sleep(2);
            printf("\e[1;1H\e[2J");
            User conversa;
            if (sscanf(msgReceived, "Joining group conversation in port %d: %s", &conversa.port, user.username) == 2) {
                printf("username: %s\n", user.username);
                connectToGroupChatServer(user, conversa.port);
                sendString(fd, "Connected to group chat successfully\n");
                continue;
            }
        }

        if(strcmp(msgReceived, "\nUntil next time! Thanks for chattingRC with us :)\n") == 0) // sai do programa
            break;
        
        fgets(msgToSend, sizeof(msgToSend), stdin);
        sendString(fd, msgToSend); 
        fflush(stdout);
	}

	sleep(2);
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

// conecta-se ao outro cliente para conversas
void connectingToClientServer(const User user, const User conversa) {

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
		
		receiveString(fd, false);
		
		char *msgReceived = NULL;
		char msgToSend[BUF_SIZE-22];
		bool disconnectRequested = false; 
		fd_set read_fds;
        int max_fd;

		while (!disconnectRequested) {

            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(fd, &read_fds);
            max_fd = fd > STDIN_FILENO ? fd : STDIN_FILENO;

            if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
                perror("select");
                exit(EXIT_FAILURE);
            }

            // verifica se há entrada do utilizador disponível no teclado
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                
                // guarda mensagem em "msgToSend"
                if (fgets(msgToSend, sizeof(msgToSend), stdin) != NULL) {

                    // Verifica se o utilizador quer sair
                    if (strcmp(msgToSend, "\n") == 0) {
                        sendString(fd, "Disconnected\n");
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
            }

            // verifica se há dados disponíveis no socket do cliente
            if (FD_ISSET(fd, &read_fds)) {
                
                // Handle message from client
                if (msgReceived != NULL) {
                    free(msgReceived);
                    msgReceived = NULL;
                }

                msgReceived = receiveString(fd, true);

                if (msgReceived == NULL) {
                    disconnectRequested = true;
                    break;
                }

                if (strcmp(msgReceived, "Disconnected\n") == 0) {
                    sendString(fd, "Disconnected\n");
                    disconnectRequested = true;
                    free(msgReceived);
                    msgReceived = NULL;
                    break;
                } 
                
                else {
                    printf("%s", msgReceived);
                }
            }
        }

        if (msgReceived != NULL) {
            free(msgReceived);
        }

        close(fd);
        fflush(stdout);
	}
}

// torna o client num server à espera de uma conexão
void createNewServer(const User user) {

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
		} 
        
        else {
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
		strcpy(aux_string, receiveString(client, false));
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
		fd_set read_fds;
        int max_fd;

		// Começa a conversa
		while (!disconnectRequested) {
            
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(client, &read_fds);
            max_fd = client > STDIN_FILENO ? client : STDIN_FILENO;

            if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
                perror("select");
                exit(EXIT_FAILURE);
            }

            // verifica se há entrada do utilizador disponível no teclado
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                
                // guarda mensagem em "msgToSend"
                if (fgets(msgToSend, sizeof(msgToSend), stdin) != NULL) {

                    // Verifica se o utilizador quer sair
                    if (strcmp(msgToSend, "\n") == 0) {
                        sendString(client, "Disconnected\n");
                        free(msgReceived);
                        msgReceived = NULL;
                        msgReceived = receiveString(client, false);

                        if (msgReceived && strcmp(msgReceived, "Disconnected\n") == 0) {
                            disconnectRequested = true;
                            free(msgReceived);
                            msgReceived = NULL;
                            close(client);
                            break;
                        }
                        free(msgReceived);
                        msgReceived = NULL;

                    } 

                    // Se não for para sair, envia a mensagem
                    else {

                        // Filtra a mensagem antes de enviar
                        char *filteredMessage = filteredString(msgToSend);
                        strcpy(msgToSend, filteredMessage);

                        char formattedMsg[BUF_SIZE];
                        sprintf(formattedMsg, "%s: %s", user.username, msgToSend);
                        sendString(client, formattedMsg);
                        fflush(stdout);
                    }
                }
            }

            // verifica se há dados disponíveis no socket do cliente
            if (FD_ISSET(client, &read_fds)) {
                
                // Handle message from client
                char *newMsgReceived = receiveString(client, true);

                if (newMsgReceived && strcmp(newMsgReceived, "Disconnected\n") == 0) {
                    sendString(client, "Disconnected\n");
                    disconnectRequested = true;
                    free(newMsgReceived);
                    close(client);
                    break;

                } else if (newMsgReceived) {
                    printf("%s", newMsgReceived);
                    free(newMsgReceived);
                }
            }
        }

        if (msgReceived != NULL) {
            free(msgReceived);
        }

        close(client);
	}
    
	return;
}

void sendString(int fd, char *msg) {
	write(fd, msg, 1 + strlen(msg));
}

char *receiveString(int fd, bool mensagens) {
	int nread = 0;
	char buffer[BUF_SIZE];
	char *string;

	nread = read(fd, buffer, BUF_SIZE - 1);
	buffer[nread] = '\0';
	string = (char *)malloc(strlen(buffer) + 1);
	strcpy(string, buffer);
	
    if(string[0] == '\n') 
        printf("\e[1;1H\e[2J");
	
    if (!mensagens)
        printf("%s", string);

	fflush(stdout);
	return string;
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	fflush(stdout);
	exit(-1);
}

int compareWords(const char *palavra1, const char *palavra2) { //função que devolve 0 se as palavras forem iguais e 1 se forem diferentes
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
            if (compareWords(words[i].word, palavras[j]) == 0) { //função retorna 0 quando 2 palavras são iguais
                existe = true;
                break;
            }
        }

        if (!existe) { // se a palavra for uma palavra bloqueada, simplesmente não a adicionamos à frase filtrada
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

void createGroupChatServer(const User user) {
    int server_fd, new_client_fd, fd_max;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen;
    fd_set master_set, read_fds; // master contém todos os file descriptors que são monitorizados leitura, escrita ou exceções

	// Cria um socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

	// Configura a opção de reutilização de endereço de socket
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

	// Configura o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(user.port);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

	// Configura o socket para ouvir conexões
    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

	// Inicializa os conjuntos master e read_fds
    FD_ZERO(&master_set);
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &master_set);
    FD_SET(STDIN_FILENO, &master_set); // Adiciona a entrada padrão ao conjunto master
    fd_max = server_fd;

    printf("Server listening on port %d\n", user.port);

    while (1) {
        read_fds = master_set; // Copia o conjunto master para o conjunto temporário

        if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

		// Itera através dos descritores de arquivo
        for (int i = 0; i <= fd_max; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
					// Nova conexão
                    addrlen = sizeof(client_addr);
                    new_client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
                    if (new_client_fd == -1) {
                        perror("Accept failed");
                    } else {
                        FD_SET(new_client_fd, &master_set); // Adiciona o novo cliente ao conjunto master
                        if (new_client_fd > fd_max) {
                            fd_max = new_client_fd;
                        }
                        printf("New connection from %s on socket %d\n", inet_ntoa(client_addr.sin_addr), new_client_fd);
                    }
                } else if (i == STDIN_FILENO) { // Mensagens do criador do server
                    char buffer[BUF_SIZE - 22];
                    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                        if (strcmp(buffer, "\n") == 0) {
                            printf("Shutting down server...\n");
                            for (int j = 0; j <= fd_max; j++) { // Termina a ligação com todos os clientes
                                if (FD_ISSET(j, &master_set) && j != server_fd && j != STDIN_FILENO) {
                                    send(j, "Server is shutting down...\n", 27, 0);
                                    close(j);
                                    FD_CLR(j, &master_set);
                                }
                            }
                            close(server_fd);
                            return;
                        } else {
                            char message[BUF_SIZE];
                            snprintf(message, sizeof(message), "%s: %s", user.username, buffer);
                            for (int j = 0; j <= fd_max; j++) { // Envia a  mensagem para todos
                                if (FD_ISSET(j, &master_set)) {
                                    if (j != server_fd && j != STDIN_FILENO) {
                                        send(j, message, strlen(message), 0);
                                    }
                                }
                            }
                        }
                    }
                } else { // Lida com os dados de cada cliente
                    char buffer[BUF_SIZE];
                    int nbytes = recv(i, buffer, sizeof(buffer), 0);
                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            printf("Socket %d hung up\n", i);
                        } else {
                            perror("Recv failed");
                        }
                        close(i);
                        FD_CLR(i, &master_set);
                    } else {
                        buffer[nbytes] = '\0';
                        printf("%s", buffer);
                        for (int j = 0; j <= fd_max; j++) { // Envia a  mensagem para todos
                            if (FD_ISSET(j, &master_set)) {
                                if (j != server_fd && j != i) {
                                    send(j, buffer, nbytes, 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void connectToGroupChatServer(const User user, int port) {
    int sock_fd;
    struct sockaddr_in server_addr;
    fd_set master_set, read_fds;
    char buffer[BUF_SIZE - 22];

	// Cria um socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

	// Configura o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

	// Conexão com o server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

	// Inicializa os conjuntos de descritores de arquivo
    FD_ZERO(&master_set);
    FD_SET(sock_fd, &master_set);
    FD_SET(STDIN_FILENO, &master_set);
    int fd_max = sock_fd;

    printf("Connected to the server. Type messages to send.\n");

    while (1) {
        read_fds = master_set;

        if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= fd_max; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sock_fd) {
                    int nbytes = recv(sock_fd, buffer, sizeof(buffer), 0);
                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            printf("Server closed the connection\n");
                        } else {
                            perror("Recv failed");
                        }
                        close(sock_fd);
                        return;
                    } else {
                        buffer[nbytes] = '\0';
                        printf("%s", buffer);
                    }
                } else if (i == STDIN_FILENO) {
                    char message[BUF_SIZE];
                    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                        if (strcmp(buffer, "\n") == 0) {
                            printf("Disconnecting from server...\n");
                            close(sock_fd);
							return;
                        } else {
                            snprintf(message, sizeof(message), "%s: %s", user.username, buffer);
                            send(sock_fd, message, strlen(message), 0);
                        }
                    }
                }
            }
        }
    }

    close(sock_fd);
}