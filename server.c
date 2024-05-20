#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/select.h>

#define SERVER_PORT 9000
#define BUF_SIZE 1024
#define MAX_USERS 50
#define MAX_GROUP 10
#define MAX_CONVERSATIONS 10
#define MAX_WORDS 50

typedef int bool;
#define true 1
#define false 0

bool firstConnection = false;

#define PORT_START 9050

typedef struct {
    char username[20];
    char password[20];
    int port; // cada user tem um port associdado
	bool logged_in;	// define se o user está logado ou não
} User;

typedef struct {	// struct que armazena as palavras bloqueadas pelo administrador
	char word[20];
} Word;

typedef struct {	//struct que armazena os users bloqueados (quem bloqueia e quem é bloqueado)
    char blocker_username[20];
	char bloqueado_username[20];
} BlockedUser;

typedef struct {	//struct que define uma conversa
	char username1[20];
	char username2[20];
	int port;
} Conversation;

typedef struct {
    char usernames[MAX_GROUP][50];
    int num_users;
    int port;
} GroupConversation;

typedef struct {
    GroupConversation *conversations;
    int count;
} GroupConversationList;

void erro(char *msg);
// COMMUNICATION
void sendString(int client_fd, char *msg);
char *receiveString(int client_fd);

// MENUS
void mainMenu(int client_fd);
void signupMenu(int client_fd);
void loginMenu(int client_fd);
void setAllUsersLoggedOut();	//ao iniciar uma nova sessão, todos os users ficam logged out (impedir erros)
void logoutUser(User user);	//quando o user faz logout, o seu estado é alterado para logged out
void conversationsMenu(int client_fd, const User user, bool admin);
void privateCommunicationMenu(int client_fd, const User user);


User chooseAvailableUsers(int client_fd, const User user);
void getOnlineUsers(int client_fd, const User user);	//menu que retribui todos os users online

void updateUserStatus(const User user);
void filterWords(int client_fd);
void blockUsers(int client_fd, User user); //func para bloquear users
bool notBlocked(User user, User user_lista); //função que retorna se o user está ou não bloqueado

// FILES
// USERS FILE
void createUsersFile();
void seeUsers();
void seeWords(int client_fd);
void create_user(char *username, char *password);
void addWord (char *word);	//func para adicionar uma palavra bloqueada
void deleteWord(int client_fd); //func para eliminar uma palavra bloqueada
bool checkDuplicateUsername(char *username);
int checkCredentials(char *username, char *password);
void createBlockUsersFile();
void createWordsFile();
// CONVERSATIONS FILE
void createConversationsFileLog();
void addConversation(const char *username1, const char *username2, int port);
void deleteConversation(const char *username1, const char *username2);
int checkExistingConversation(const char *username1, const char *username2);
User activeConversations(int client_fd, const User user);

// GROUP CONVERSATIONS
void groupConversationsMenu(int client_fd, const User user);
void chooseAvailableUsernamesForGroupConversation(int client_fd, const User user, char selected_usernames[][50], int *num_selected);
void createGroupConversationsFileLog();
int addGroupConversation(int num_users, char usernames[][50]);
void deleteGroupConversation(int port);
int checkExistingGroupConversation(int num_users, char usernames[][50]);
GroupConversation activeGroupConversations(int client_fd, const char *username);

int main() {
	printf("\e[1;1H\e[2J");
	printf("ChatRC Main Server\n");
	int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;

	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		erro("na funcao socket");
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		erro("na funcao bind");
	if (listen(fd, 5) < 0)
		erro("na funcao listen");

	client_addr_size = sizeof(client_addr);

	if (!firstConnection) {
		setAllUsersLoggedOut();
		printf("First log in\n");
		firstConnection = true;
	}

	while (1) {
		
		while (waitpid(-1, NULL, WNOHANG) > 0);		
		client = accept(fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
		if (client > 0) {
			createUsersFile();
			createWordsFile();
			createBlockUsersFile();
			createConversationsFileLog();
			createGroupConversationsFileLog();
			if (fork() == 0) {
				close(fd);
				printf("Connected with a client\n");
				fflush(stdout);
				mainMenu(client);
				exit(0);
			}
			close(client);
		}
	}
	return 0;
}

// COMMUNICATION

void sendString(int client_fd, char *msg) {
	write(client_fd, msg, 1 + strlen(msg));
}

char *receiveString(int client_fd) {
	int nread = 0;
	char buffer[BUF_SIZE];
	char *string;

	nread = read(client_fd, buffer, BUF_SIZE - 1);
	buffer[nread] = '\0';
	string = (char *)malloc(strlen(buffer) + 1);
	strcpy(string, buffer);
	fflush(stdout);

	printf("Received from client %d: %s", getpid(), string);

	return string;
}

void mainMenu(int client_fd) {
	int selection = -1;
	while(selection != 3) {
		sendString(client_fd, "\nWELCOME TO chatRC\n\n1. Sign Up\n2. Log in\n3. Exit\n\nSelection: ");
		selection = atoi(receiveString(client_fd));
		if (selection == 1 || selection == 2 || selection == 3) {
			if (selection == 1) {
				signupMenu(client_fd);
				selection = -1;
				continue;
			}
			if (selection == 2) {
				seeUsers();
				loginMenu(client_fd);
				selection = -1;
				continue;
			}
			if (selection == 3) {
				sendString(client_fd, "\nUntil next time! Thanks for chattingRC with us :)\n");
			}
		}
		else {
			sendString(client_fd, "\nINVALID SELECTION, please press 1, 2 or 3! (Press ENTER to continue...)\n");
			receiveString(client_fd);
		}
	}
}

void signupMenu(int client_fd) {

	bool leave_menu = false;
	while(!leave_menu) {
		char username[50]; 
		sendString(client_fd, "\nSign up for chatRC (Press ENTER to return)\n\nusername: ");
		strcpy(username, receiveString(client_fd));

		if (strcmp(username, "\n") != 0) {
			if (checkDuplicateUsername(username)) {
				sendString(client_fd, "\nusername already exists (Press ENTER to continue...)\n");
				receiveString(client_fd);
			}
			else {
				char password[50]; 
				sendString(client_fd, "password: ");
				strcpy(password, receiveString(client_fd));
				
				if (strcmp(password, "\n") == 0) {
					leave_menu = false;
					break;
				}
				else {
					create_user(username, password);
					sendString(client_fd, "\nUser created successfully!!! (Press ENTER to continue...)\n");
					receiveString(client_fd);
					leave_menu = true;
				}
			}
		}
		else {
			leave_menu = true;
		}
	}
}

void loginMenu(int client_fd) {

	bool leave_menu = false;
	while(!leave_menu) {
		char username[50]; 
		sendString(client_fd, "\nLog in chatRC (Press ENTER to return)\n\nusername: ");
		strcpy(username, receiveString(client_fd));

		if (strcmp(username, "\n") != 0) {
			char password[50]; 
			sendString(client_fd, "password: ");
			strcpy(password, receiveString(client_fd));

			if (strcmp(password, "\n") == 0) {
				leave_menu = true;
			}
			else {
				int user_port = checkCredentials(username, password);
				
				if(user_port != -1) {
					sendString(client_fd, "\nLogged in successfully!!! (Press ENTER to continue...)\n");
					receiveString(client_fd);
					
					User user;
					strcpy(user.username, username);
					strcpy(user.password, password);
					user.port = user_port;
					bool admin_or_not = false;

					if(user.port == 9001) {	//define o user com username admin = admin
						admin_or_not = true;
					}

					user.logged_in = true; //quando se dá login fica-se com status ONLINE
					updateUserStatus(user);
					conversationsMenu(client_fd, user, admin_or_not);
					leave_menu = true;
				}
				else {
					sendString(client_fd, "\nWrong username or password (Press ENTER to continue...)\n");
					receiveString(client_fd);
				}
			}
		}
		else { 
			leave_menu = true;
		}
	}
}

void setAllUsersLoggedOut() {
	FILE *file = fopen("users.bin", "r+b");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    User users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    for (size_t i = 0; i < num_users; i++) {
        users[i].logged_in = false;
    }

    rewind(file);
    fwrite(users, sizeof(User), num_users, file);
    fclose(file);
}

void logoutUser(User user) {
	user.logged_in = false;
    updateUserStatus(user);
}

void updateUserStatus(const User user) {	//função que atualiza o status do user (logged in ou logged out) no ficheiro users.bin

	FILE *file = fopen("users.bin", "r+b");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    User users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    for (size_t i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, user.username) == 0) {
            users[i].logged_in = user.logged_in;
            fseek(file, i * sizeof(User), SEEK_SET);
            fwrite(&users[i], sizeof(User), 1, file);
            break;
        }
    }

    fclose(file);
}

void conversationsMenu(int client_fd, const User user, bool admin) {

	int selection = -1;

	while(selection != 4) {
		char string[200]; 
		if (admin == false) {
			snprintf(string, sizeof(string), "\nWELCOME %.*s TO THE CONVERSATIONS MENU\n\n1. Private\n2. Group\n3. Block users\n4. Log out\n\nSelection: ", (int)(strlen(user.username) - 1), user.username);
		}
		else {
			snprintf(string, sizeof(string), "\nWELCOME %.*s TO THE CONVERSATIONS MENU\n\n1. Private\n2. Group\n3. Block users\n4. Log out\n5. Filter Words\n\nSelection: ", (int)(strlen(user.username) - 1), user.username);
		}
		
		sendString(client_fd, string);
		selection = atoi(receiveString(client_fd));

		if (selection == 1 || selection == 2 || selection == 3 || selection == 4 || (selection == 5 && admin)) {
			if (selection == 1) {
				privateCommunicationMenu(client_fd, user); // Conversas Privadas
				selection = -1;
			}
			if (selection == 2) {
				groupConversationsMenu(client_fd, user);
				selection = -1;
			}
			if (selection == 3) {
				blockUsers(client_fd, user);
				selection = -1;
			}
			if (selection == 4) {
				logoutUser(user);
				sendString(client_fd, "\nReturning to Main Menu! (Press ENTER to continue...)\n");
				receiveString(client_fd);
			}

			if (selection == 5 && admin) {
				filterWords(client_fd);
				selection = -1;
			}
		}
		else {
			sendString(client_fd, "\nINVALID SELECTION, please press 1, 2 or 3! (Press ENTER to continue...)\n");
			receiveString(client_fd);
		}
	}
}

void privateCommunicationMenu(int client_fd, const User user) {

	int selection = -1;
	
	while(selection != 4) {
		char string[300];
		snprintf(string, sizeof(string), "\nWELCOME %.*s TO THE PRIVATE CONVERSATIONS MENU\n\n1. See ongoing conversations\n2. Start a new conversation\n3. See who's online\n4. Exit Menu\n\nSelection: ", (int)(strlen(user.username) - 1), user.username);
		sendString(client_fd, string);
		selection = atoi(receiveString(client_fd));

		if (selection == 1 || selection == 2 || selection == 3 || selection == 4) {
			if (selection == 1) { // mostra se algum user já começou uma conversa e está à espera da sua conexão
				User conversa = activeConversations(client_fd, user);
				if(strcmp(conversa.username, "") != 0){ 
					char string[100];
					snprintf(string, sizeof(string), "Start a conversation: %.*s -> %.*s %d\n", (int)(strlen(user.username) - 1), user.username, (int)(strlen(conversa.username) - 1), conversa.username, conversa.port);
					sendString(client_fd, string); // envia ao cliente a informação para começar fazer a conexão com o user que já começou a conversa no porto dele
					receiveString(client_fd);
				}
				selection = -1;
			}
			if (selection == 2) { // começa uma nova conversa ou conecta-se a uma já existente
				User conversa = chooseAvailableUsers(client_fd, user);

				if(strcmp(conversa.username, "") != 0 && strcmp(conversa.password, "") != 0) { // Se não retornar nulo
					if(checkExistingConversation(user.username, conversa.username)){  // se o server para a conversa já tiver sido criada pelo outro user, este apenas se conecta
						char string[100];
						snprintf(string, sizeof(string), "Start a conversation: %.*s -> %.*s %d\n", (int)(strlen(user.username) - 1), user.username, (int)(strlen(conversa.username) - 1), conversa.username, conversa.port);
						sendString(client_fd, string);
						receiveString(client_fd);
					}
					else { // se não existir a conversa ainda
						addConversation(user.username, conversa.username, user.port); // cria a conversa no ficheiro binário para confirmação futura
						char string[100];
						snprintf(string, sizeof(string), "Creating a new server: %.*s %d\n", (int)(strlen(user.username) - 1), user.username, user.port); // envia a string para o cliente cria um server
						sendString(client_fd, string);
						receiveString(client_fd);
						deleteConversation(user.username, conversa.username); // apaga a conversa do ficheiro
					}
				}
				selection = -1;
			}
			if (selection == 3) {
				getOnlineUsers(client_fd, user);
				selection = -1;
			}
			if (selection == 4) {
				sendString(client_fd, "\nReturning to Conversations Menu! (Press ENTER to continue...)\n");
				receiveString(client_fd);
			}
		}
		else {
			sendString(client_fd, "\nINVALID SELECTION, please press 1, 2, 3, 4 or 5 (Press ENTER to continue...)\n");
			receiveString(client_fd);
		}
	}
}

bool notBlocked(User user, User user_lista) {	//função que retorna se o user está ou não bloqueado
	FILE *file_2 = fopen("block_users.bin", "rb");
    if (file_2 == NULL) {
		printf("erro no file Blocked");
    }

	BlockedUser users_bloqueados[MAX_USERS];
	size_t num_users_bloqueados = fread(users_bloqueados, sizeof(BlockedUser), MAX_USERS, file_2);

    fclose(file_2);

	for(size_t i = 0; i < num_users_bloqueados; i++) {
		if(strcmp(users_bloqueados[i].blocker_username, user.username) == 0) { //vê quem o user logado bloqueou
			if(strcmp(users_bloqueados[i].bloqueado_username, user_lista.username) == 0) { //vê quem está bloqueado
				return false;	// user está blocked
			}
		}
	}
	return true;
}

void blockUsers(int client_fd, User user) { //func para bloquear users

	FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        return;
    }

    User users[MAX_USERS];
    User available_users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

	FILE *file_2 = fopen("block_users.bin", "rb");
    if (file_2 == NULL) {
		return;
    }

	BlockedUser users_bloqueados[MAX_USERS];
	BlockedUser users_que_o_user_logado_bloqueou[MAX_USERS];
	size_t num_users_bloqueados = fread(users_bloqueados, sizeof(BlockedUser), MAX_USERS, file_2);

    fclose(file);
    fclose(file_2);

	// ler todos os users que o user logado bloqueou
	int counter_2 = 0;
	for(size_t i = 0; i < num_users_bloqueados; i++) {
		printf("blocker = %s", users_bloqueados[i].blocker_username);
		printf("bloqued = %s", users_bloqueados[i].bloqueado_username);
		if(strcmp(users_bloqueados[i].blocker_username, user.username) == 0) {
			users_que_o_user_logado_bloqueou[counter_2] = users_bloqueados[i]; // todos os users bloqueados pelo user logado
			counter_2++;
		}
	}

	// escolher quem bloquear
    char choices_to_send[1000] = "";
    int counter = 0;
    char counter_str[5];
	strcat(choices_to_send, "\nLIST OF USERS\n\n");
    for(size_t i = 0; i < num_users; i++) {
		if(strcmp(users[i].username, user.username) != 0) {
			available_users[counter] = users[i];
			printf("available_users[counter] = %s", users[i].username);
			counter++;
			sprintf(counter_str, "%d", counter);
			strcat(choices_to_send, counter_str);
			strcat(choices_to_send, ". ");

			size_t username_len = strlen(users[i].username);
			if (users[i].username[username_len - 1] == '\n') {
				users[i].username[username_len - 1] = '\0';
			}	
			strcat(choices_to_send, users[i].username);
			users[i].username[username_len - 1] = '\n';
			for(size_t j = 0; j < counter_2; j++) {
				if (strcmp(users[i].username, users_que_o_user_logado_bloqueou[j].bloqueado_username) == 0) {
					strcat(choices_to_send, " | Blocked");	//info para o user saber quem já bloqueou
					break;
				}
			}
			strcat(choices_to_send, "\n");
		}
    }

    counter++;
    sprintf(counter_str, "%d", counter);
    strcat(choices_to_send, counter_str);
    strcat(choices_to_send, ". ");
    strcat(choices_to_send, "Return to the previous menu\n\nSelection: ");

    int selection = -1;
	User user_a_bloquear;
    while(1) {
		sendString(client_fd, choices_to_send);
		selection = atoi(receiveString(client_fd));

		if(selection == counter) {
			return; 
		}
		else if(selection >= 0 && selection < counter) {
			user_a_bloquear = available_users[selection - 1]; //devolve o user escolhido
			
			bool already_blocked = false;	//para impedir que se bloqueie quem já foi bloqueado
			for(size_t j = 0; j < counter_2; j++) {
				if (strcmp(user_a_bloquear.username, users_que_o_user_logado_bloqueou[j].bloqueado_username) == 0) {
					already_blocked = true;
					break;
				}
			}
			if (already_blocked) {
				sendString(client_fd, "\nThis user is already blocked. Please select another user (press ENTER).\n\n");
				receiveString(client_fd);
				selection = -1;
				already_blocked = false;
			}
			else {
				sendString(client_fd, "\nUser blocked!\nReturning to Main Menu! (Press ENTER to continue...)\n");
				receiveString(client_fd);
				break;
			}
		}
	}

	BlockedUser blockedUser;

	strcpy(blockedUser.blocker_username, user.username);				// define quem bloqueia (o user logado)
	strcpy(blockedUser.bloqueado_username, user_a_bloquear.username);	// define quem é bloqueado (o user escolhido)

	FILE *file_3 = fopen("block_users.bin", "ab+");	//atualiza
    if (file_3 == NULL) {
		printf("erro no file");
		return;
    }

	fwrite(&blockedUser, sizeof(BlockedUser), 1, file_3);
	fclose(file_3);

	return;

}

void getOnlineUsers(int client_fd, const User user) {	//menu que retribui todos os users online
	FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        return;
    }

	User users[MAX_USERS];
	User users_online[MAX_USERS];
	size_t num_users = fread(users, sizeof(User), MAX_USERS, file);
    fclose(file);

	char choices_to_send[1000] = "";
	strcat(choices_to_send, "\nONLINE USERS\n\n");
	
	int counter = 0;
	char counter_str[5];
	for(size_t i = 0; i < num_users; i++) {
		if(strcmp(users[i].username, user.username) != 0 && users[i].logged_in) { //se o user não for o user logado e estiver online
			users_online[counter] = users[i];
			printf("users_online[counter] = %s", users[i].username);
			counter++;
			sprintf(counter_str, "%d", counter);
			strcat(choices_to_send, counter_str);
			strcat(choices_to_send, ". ");
			strcat(choices_to_send, users[i].username);
		}
    }
	
    counter++;
    sprintf(counter_str, "%d", counter);
    strcat(choices_to_send, counter_str);
    strcat(choices_to_send, ". ");
    strcat(choices_to_send, "Return to the PRIVATE CONVERSATIONS MENU\n\nSelection: ");

	int selection = -1; 
    while(selection != counter) {
		sendString(client_fd, choices_to_send);
		selection = atoi(receiveString(client_fd));
    }
}

User chooseAvailableUsers(int client_fd, const User user) { // função/menu onde o utilizador escolhe com quem quer falar, dependendo de quem está online e se está bloqueado ou não
	
	FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        User nulo = { "", "", 0 };
        return nulo;
    }

    User users[MAX_USERS];
    User available_users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    fclose(file);

    char choices_to_send[1000] = "";
    int counter = 0;
    char counter_str[5];

	strcat(choices_to_send, "\nSTART A NEW CONVERSATION\n\n");
    for(size_t i = 0; i < num_users; i++) { // apenas escolhe quem está online e não está bloqueado
		if(strcmp(users[i].username, user.username) != 0 && users[i].logged_in && notBlocked(user, users[i]) && notBlocked(users[i], user)) {
			available_users[counter] = users[i];
			printf("available_users[counter] = %s", users[i].username);
			counter++;
			sprintf(counter_str, "%d", counter);
			strcat(choices_to_send, counter_str);
			strcat(choices_to_send, ". ");
			strcat(choices_to_send, users[i].username);
		}
    }

    counter++;
    sprintf(counter_str, "%d", counter);
    strcat(choices_to_send, counter_str);
    strcat(choices_to_send, ". ");
    strcat(choices_to_send, "Return to the PRIVATE CONVERSATIONS MENU\n\nSelection: ");

    // Send the string to the user
    // Receive the choice and return the user
    int selection = -1; 
    while(1) {
		sendString(client_fd, choices_to_send);
		selection = atoi(receiveString(client_fd));

		if(selection == counter) { // Exit
			User nulo = { "", "", 0 }; 
			return nulo; // retorna um user nulo
		}
		else if(selection >= 0 && selection < counter) {
			return available_users[selection - 1];
		}
    }

    User nulo = { "", "", 0 };
    return nulo;
}

void filterWords(int client_fd) { //menu para filtrar palavras

	int selection = -1;
	
	while(selection != 4) {
		char string[100]; 
		snprintf(string, sizeof(string), "\nWELCOME TO THE FILTER WORDS MENU\n1. See Words\n2. Add Word\n3. Delete Word\n4. Exit Menu\n\nSelection: ");

		sendString(client_fd, string);
		selection = atoi(receiveString(client_fd));

		if (selection == 1 || selection == 2 || selection == 3 || selection == 4) {
			
			if (selection == 1) {
				seeWords(client_fd);
				selection = -1;
			}

			if (selection == 2) {
				char word_add[50]; 
				sendString(client_fd, "\n(WORD TO ADD)\n\nWord: ");
				strcpy(word_add, receiveString(client_fd));
				addWord(word_add);
				selection = -1;
			}

			if (selection == 3) {
				deleteWord(client_fd);
				selection = -1;
			}

			if (selection == 4) {
				//sendString(client_fd, "\nReturning to Main Menu! (Press ENTER to continue...)\n");
				//receiveString(client_fd);
			}
		}

		else {
			sendString(client_fd, "\nINVALID SELECTION, please press 1, 2 or 3! (Press ENTER to continue...)\n");
			receiveString(client_fd);
		}
	}
}

// FILES
void seeWords(int client_fd) { //func para ver as palavras bloqueadas

	FILE *file = fopen("words.bin", "rb");
	if (file == NULL) {
        return;
    }

	Word words[MAX_WORDS];
	size_t num_words = fread(words, sizeof(Word), MAX_WORDS, file);

	char menu_words_to_send[1000] = "";
	strcat(menu_words_to_send, "\nWORDS\n\n");

	if (num_words == 0) {
		strcat(menu_words_to_send, "\nNo words in the file\n\nPress ENTER to continue... ");
		sendString(client_fd, menu_words_to_send);
		receiveString(client_fd);
		fclose(file);
		return;
	}

	int counter = 0;
	char counter_str[5];

	for (size_t i = 0; i < num_words; i++) {
        size_t palavra = strlen(words[i].word);

        if (palavra > 0 && words[i].word[palavra - 1] == '\n') {
            words[i].word[palavra - 1] = '\0';
        }
		sprintf(counter_str, "%d", counter);
        strcat(menu_words_to_send, counter_str);
		strcat(menu_words_to_send, ". ");
		strcat(menu_words_to_send, words[i].word);
		strcat(menu_words_to_send, "\n");
		counter++;
    }

	strcat(menu_words_to_send, "Press ENTER to continue...");
	sendString(client_fd, menu_words_to_send);
	receiveString(client_fd);
    fclose(file);
}

void createUsersFile() { // cria o ficheiro de users, com o user inicial admin, com password admin e porto 9001
    FILE *file = fopen("users.bin", "r");
    if (file == NULL) {
        file = fopen("users.bin", "wb");
        if (file == NULL) {
            perror("Error creating file");
            exit(1);
        }

        User initial_user;
        strcpy(initial_user.username, "admin\n");
        strcpy(initial_user.password, "admin\n");
		initial_user.logged_in = false;
        initial_user.port = 9001;
        fwrite(&initial_user, sizeof(User), 1, file);

        fclose(file);
    } else {
        fclose(file);
    }
}

void createBlockUsersFile() { // cria o ficheiro de users bloqueados
	FILE *file = fopen("block_users.bin", "r");
    if (file == NULL) {
        file = fopen("block_users.bin", "wb");
        if (file == NULL) {
            perror("Error creating file");
            exit(1);
        }

		BlockedUser user;
		strcpy(user.blocker_username, "teste\n");
		strcpy(user.bloqueado_username, "teste_2\n");

		fwrite(&user, sizeof(BlockedUser), 1, file);
        fclose(file);
    } else {
        fclose(file);
    }
}


void seeUsers() { // ver os users que existem, do lado do server (apenas para auxílio ao correr o código)
    FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        return;
    }

    User users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    for (size_t i = 0; i < num_users; i++) {
        // Remove trailing newline character if it exists
        size_t username_len = strlen(users[i].username);
        size_t password_len = strlen(users[i].password);
        if (username_len > 0 && users[i].username[username_len - 1] == '\n') {
            users[i].username[username_len - 1] = '\0';
        }
        if (password_len > 0 && users[i].password[password_len - 1] == '\n') {
            users[i].password[password_len - 1] = '\0';
        }
        printf("User %ld -> %s, %s, %d\n", i, users[i].username, users[i].password, users[i].port);
    }

    fclose(file);
}

bool checkDuplicateUsername(char *username) { // confirma se já existe um user com aquele username para não haver iguais
    FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        return false;
    }

    User users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    for (size_t i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            fclose(file);
            return true;
        }
    }

    fclose(file);
    return false;
}

// FICHEIROS

void addWord(char *word) { //func para adicionar uma palavra bloqueada
	
	FILE *file = fopen("words.bin", "ab+");
	if (file == NULL) {
        return;
    }

	Word words[MAX_WORDS];
	size_t num_words = fread(words, sizeof(Word), MAX_WORDS, file);

	Word palavra;
	strcpy(palavra.word, word);
	fwrite(&palavra, sizeof(Word), 1, file);
	fclose(file);
}

void deleteWord(int client_fd) { //func para eliminar uma palavra bloqueada

	FILE *file = fopen("words.bin", "rb+");
	if (file == NULL) {
        return;
    }

	int indice = 0;
	char menu_words_to_send[1000] = "";
	strcat(menu_words_to_send, "\n Word to Delete\n\n");

	Word words[MAX_WORDS];
	size_t num_words = fread(words, sizeof(Word), MAX_WORDS, file);

	int counter = 0;
	char counter_str[5];
	for (size_t i = 0; i < num_words; i++) {
        size_t palavra = strlen(words[i].word);

        if (palavra > 0 && words[i].word[palavra - 1] == '\n') {
            words[i].word[palavra - 1] = '\0';
        }
		sprintf(counter_str, "%d", counter);
        strcat(menu_words_to_send, counter_str);
		strcat(menu_words_to_send, ". ");
		strcat(menu_words_to_send, words[i].word);
		strcat(menu_words_to_send, "\n");
		counter++;
    }
	strcat(menu_words_to_send, "Selecione: ");

	sendString(client_fd, menu_words_to_send);
	indice = atoi(receiveString(client_fd));
	
	if (num_words == 0) {
		sendString(client_fd, "\nNão há palavras no ficheiro\n\nPress ENTER to continue... ");
		receiveString(client_fd);
        fclose(file);
        return;
    }

	if (indice < 0 || indice >= num_words) {
        sendString(client_fd, "\nIndice invalido\n\nPress ENTER to continue... ");
		receiveString(client_fd);
        fclose(file);
        return;
    }

	for (size_t i = indice; i < num_words - 1; ++i) {
        strcpy(words[i].word, words[i + 1].word);
    }
	num_words--;

	sendString(client_fd, "\n Word deleted \n\nPress ENTER to continue... ");
	receiveString(client_fd);

	fclose(file);

	file = fopen("words.bin", "wb");
	fwrite(words, sizeof(Word), num_words, file);
	fclose(file);
}

void create_user(char *username, char *password) {
	FILE *file = fopen("users.bin", "ab+");

    if (file == NULL) {
		erro("Opening file");
		exit(0);
    }

    User existing_users[MAX_USERS];
    size_t num_users = fread(existing_users, sizeof(User), MAX_USERS, file);

    int highest_port = 0;
    for (size_t i = 0; i < num_users; i++) {
        if (existing_users[i].port > highest_port) {
            highest_port = existing_users[i].port;
        }
    }

    int new_port = highest_port + 1;

    User user;
    strcpy(user.username, username);
    strcpy(user.password, password);
    user.port = new_port;

    fwrite(&user, sizeof(User), 1, file);

    fclose(file);
}

int checkCredentials(char *username, char *password) { // confirma se as credencias que o user digitou são válidas no login
    FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        return false;
    }

    User users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    for (size_t i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
			
            if(strcmp(users[i].password, password) == 0) {
				return users[i].port;
            }
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return -1;
}

// CONVERSATIONS FILE

void createConversationsFileLog() { // cria o ficheira das conversas
    FILE *file = fopen("conversationsLog.bin", "r");
    if (file == NULL) {
        file = fopen("conversationsLog.bin", "wb");
        if (file == NULL) {
            perror("Error creating file");
            exit(1);
        }

        fclose(file);
    } else {
        fclose(file);
    }
}

void createWordsFile() { // cria o ficheiro das palavras bloqueadas
	FILE *file = fopen("words.bin", "r");
	if (file == NULL) {
		file = fopen("words.bin", "wb");
		if (file == NULL) {
			perror("Error creating file");
			exit(1);
		}

		fclose(file);
	} else {
		fclose(file);
	}
}

void addConversation(const char *username1, const char *username2, int port) { // adiciona uma conversa -> dois users e o porto em qual está designada a conversa
    FILE *file = fopen("conversationsLog.bin", "ab");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    Conversation conversation;
    strcpy(conversation.username1, username1);
    strcpy(conversation.username2, username2);
    conversation.port = port;

    fwrite(&conversation, sizeof(Conversation), 1, file);

    fclose(file);

    printf("Conversation added successfully.\n");
}

void deleteConversation(const char *username1, const char *username2) { // apaga a conversa assim que esta acaba
    FILE *file = fopen("conversationsLog.bin", "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    FILE *tempFile = fopen("tempFile.bin", "wb"); // ficheiro auxiliar para guardar a informação exceto a da conversa que vai ser apagada
    if (tempFile == NULL) {
        perror("Error creating temporary file");
        fclose(file);
        exit(1);
    }

    Conversation conversation;
    bool deleted = false;

    while (fread(&conversation, sizeof(Conversation), 1, file) == 1) {
        if ((strcmp(conversation.username1, username1) == 0 && strcmp(conversation.username2, username2) == 0) ||
            (strcmp(conversation.username1, username2) == 0 && strcmp(conversation.username2, username1) == 0)) {
            deleted = true;
        } else {
            fwrite(&conversation, sizeof(Conversation), 1, tempFile);
        }
    }

    fclose(file);
    fclose(tempFile);

    if (!deleted) {
        printf("Conversation not found.\n");
    } else {
        printf("Conversation deleted successfully.\n");
    }

    remove("conversationsLog.bin"); // apaga-se o ficheiro inicial
    rename("tempFile.bin", "conversationsLog.bin"); // muda-se o nome do ficheiro auxiliar para o nome antigo
}

bool checkExistingConversation(const char *username1, const char *username2) { // confirma se existe uma conversa que já começou
    FILE *file = fopen("conversationsLog.bin", "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    Conversation conversation;

    while (fread(&conversation, sizeof(Conversation), 1, file) == 1) {
        if ((strcmp(conversation.username1, username1) == 0 && strcmp(conversation.username2, username2) == 0) ||
            (strcmp(conversation.username1, username2) == 0 && strcmp(conversation.username2, username1) == 0)) {
            fclose(file);
            return true;
        }
    }

    fclose(file);
    return false;
}

User activeConversations(int client_fd, const User user) { // retorna as pessoas que estão à espera de uma conexão deste user
    FILE *file = fopen("conversationsLog.bin", "rb");
    if (file == NULL) {
        User nulo = { "", "", 0 };
        return nulo;
    }

    User available_users[MAX_USERS];
    int counter = 0;
    char choices_to_send[1000] = "\nACTIVE CONVERSATIONS\n\n";
    char counter_str[5];

    Conversation conversations[MAX_USERS - 1];
    size_t num_conversations = fread(conversations, sizeof(Conversation), MAX_USERS - 1, file);

    fclose(file);

    for (size_t i = 0; i < num_conversations; i++) {
        if (strcmp(conversations[i].username1, user.username) == 0) {
            strcpy(available_users[counter].username, conversations[i].username2);
            available_users[counter].port = conversations[i].port;
            counter++;
            sprintf(counter_str, "%d", counter);
            strcat(choices_to_send, counter_str);
            strcat(choices_to_send, ". ");
            strcat(choices_to_send, conversations[i].username2);
        } else if (strcmp(conversations[i].username2, user.username) == 0) {
            strcpy(available_users[counter].username, conversations[i].username1);
            available_users[counter].port = conversations[i].port;
            counter++;
            sprintf(counter_str, "%d", counter);
            strcat(choices_to_send, counter_str);
            strcat(choices_to_send, ". ");
            strcat(choices_to_send, conversations[i].username1);
        }
    }

    counter++;
    sprintf(counter_str, "%d", counter);
    strcat(choices_to_send, counter_str);
    strcat(choices_to_send, ". ");
    strcat(choices_to_send, "Return to the PRIVATE CONVERSATIONS MENU\n\nSelection: ");

    int selection = -1;
    while (1) {
        sendString(client_fd, choices_to_send);
        selection = atoi(receiveString(client_fd));

        if (selection == counter) { // Sair
            User nulo = { "", "", 0 };
            return nulo;
        } else if (selection >= 0 && selection < counter) {
			return available_users[selection - 1];
        }
    }

    User nulo = { "", "", 0 };
    return nulo;
}

// GROUP CONVERSATIONS
void chooseAvailableUsersForGroupConversation(int client_fd, const User user, char selected_usernames[][50], int *num_selected) { 
	// Escolhe os users disponíveis para criar um grupo
	FILE *file = fopen("users.bin", "rb");
    if (file == NULL) {
        *num_selected = 0;
        return;
    }

    User users[MAX_USERS];
    User available_users[MAX_USERS];
    size_t num_users = fread(users, sizeof(User), MAX_USERS, file);

    fclose(file);

    char choices_to_send[1000] = "\nSTART A NEW GROUP CONVERSATION\n\n";
    int counter = 0;
    char counter_str[5];

    for (size_t i = 0; i < num_users; i++) { // apenas escolhe quem está online e não está bloqueado
        if (strcmp(users[i].username, user.username) != 0 && users[i].logged_in && notBlocked(user, users[i]) && notBlocked(users[i], user)) {
            available_users[counter] = users[i];
            counter++;
            sprintf(counter_str, "%d", counter);
            strcat(choices_to_send, counter_str);
            strcat(choices_to_send, ". ");
            strcat(choices_to_send, users[i].username);
            strcat(choices_to_send, "\n");
        }
    }

    counter++;
    sprintf(counter_str, "%d", counter);
    strcat(choices_to_send, counter_str);
    strcat(choices_to_send, ". ");
    strcat(choices_to_send, "Finish selection and start group conversation\n\nSelection (comma separated): ");

    int selection[MAX_USERS];
    int selection_count = 0;
    char *selection_str;

    while (1) {
        sendString(client_fd, choices_to_send);
        selection_str = receiveString(client_fd);

        char *token = strtok(selection_str, ",");
        selection_count = 0;
        while (token != NULL && selection_count < MAX_USERS) {
            selection[selection_count++] = atoi(token);
            token = strtok(NULL, ",");
        }
        free(selection_str);

        if (selection_count > 0 && selection[selection_count - 1] == counter) {
            break;
        }
    }

    *num_selected = 0;
    strcpy(selected_usernames[(*num_selected)++], user.username);

    for (int i = 0; i < selection_count - 1; i++) {
        if (selection[i] > 0 && selection[i] < counter) {
            strcpy(selected_usernames[(*num_selected)++], available_users[selection[i] - 1].username);
        }
    }
}

void groupConversationsMenu(int client_fd, const User user) { // Menu das conversas de Grupo
    int selection = -1;

    while (selection != 4) {
        char string[300];
        snprintf(string, sizeof(string), "\nWELCOME %.*s TO THE GROUP CONVERSATIONS MENU\n\n1. See ongoing group conversations\n2. Start a new group conversation\n3. See who's online\n4. Exit Menu\n\nSelection: ", (int)(strlen(user.username) - 1), user.username);
        sendString(client_fd, string);
        selection = atoi(receiveString(client_fd));

        if (selection == 1 || selection == 2 || selection == 3 || selection == 4) {
            if (selection == 1) { // Show ongoing group conversations
                GroupConversation conversation = activeGroupConversations(client_fd, user.username);
                if (strcmp(conversation.usernames[0], "") != 0) {
                    char string[100];
                    snprintf(string, sizeof(string), "Joining group conversation in port %d: %s\n", conversation.port, user.username);
                    sendString(client_fd, string);
                    receiveString(client_fd);
                }
                selection = -1;
            }
            if (selection == 2) { // Start a new group conversation
                char selected_usernames[MAX_GROUP][50];;
                int num_selected;
                chooseAvailableUsersForGroupConversation(client_fd, user, selected_usernames, &num_selected);

                if (num_selected > 1) {
                    int port = addGroupConversation(num_selected, selected_usernames);
                    char string[100];
                    snprintf(string, sizeof(string), "New group conversation created by %s in port %d\n", user.username, port);
                    sendString(client_fd, string);
                    receiveString(client_fd);
					deleteGroupConversation(port);
                }
                selection = -1;
            }
            if (selection == 3) { // See who's online
                getOnlineUsers(client_fd, user);
                selection = -1;
            }
            if (selection == 4) { // Exit menu
                sendString(client_fd, "\nReturning to Main Menu! (Press ENTER to continue...)\n");
                receiveString(client_fd);
            }
        } else {
            sendString(client_fd, "\nINVALID SELECTION, please press 1, 2, 3 or 4 (Press ENTER to continue...)\n");
            receiveString(client_fd);
        }
    }
}

void createGroupConversationsFileLog() { // cria o ficheiro das conversas de grupo
    FILE *file = fopen("groupConversationsLog.bin", "r");
    if (file == NULL) {
        file = fopen("groupConversationsLog.bin", "wb");
        if (file == NULL) {
            perror("Error creating file");
            exit(1);
        }

        fclose(file);
    } else {
        fclose(file);
    }
}

int addGroupConversation(int num_users, char usernames[][50]) {
    FILE *file = fopen("groupConversationsLog.bin", "ab+");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    fseek(file, 0, SEEK_SET);

    int max_port = PORT_START - 1; // Initialize to one less than PORT_START
    GroupConversation conversation;

    while (fread(&conversation, sizeof(GroupConversation), 1, file) == 1) {
        if (conversation.port > max_port) {
            max_port = conversation.port;
        }
    }

    int port = max_port + 1; // Increment to get the next available port

    conversation.num_users = num_users;
    for (int i = 0; i < num_users; i++) {
        strcpy(conversation.usernames[i], usernames[i]);
    }
    conversation.port = port;

    fseek(file, 0, SEEK_END);
    fwrite(&conversation, sizeof(GroupConversation), 1, file);

    fclose(file);

    printf("Group Conversation added successfully with port %d.\n", port);

    return port;
}

void deleteGroupConversation(int port) { // Apaga uma conversa de grupo
    FILE *file = fopen("groupConversationsLog.bin", "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    FILE *tempFile = fopen("temporary.bin", "wb");
    if (tempFile == NULL) {
        perror("Error opening temporary file");
        fclose(file);
        exit(1);
    }

    GroupConversation conversation;
    int found = 0;
    while (fread(&conversation, sizeof(GroupConversation), 1, file) == 1) {
        if (conversation.port != port) {
            fwrite(&conversation, sizeof(GroupConversation), 1, tempFile);
        } else {
            found = 1;
        }
    }

    fclose(file);
    fclose(tempFile);

    if (found) {
        remove("groupConversationsLog.bin");
        rename("temporary.bin", "groupConversationsLog.bin");
        printf("Group Conversation with port %d deleted successfully.\n", port);
    } else {
        remove("temporary.bin");
        printf("No Group Conversation found with port %d.\n", port);
    }
}

int checkExistingGroupConversation(int num_users, char usernames[][50]) { // Verifica se a conversa de grupo já existe
    FILE *file = fopen("groupConversationsLog.bin", "rb");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    GroupConversation conversation;
    while (fread(&conversation, sizeof(GroupConversation), 1, file) == 1) {
        if (conversation.num_users == num_users) {
            bool match = true;
            for (int i = 0; i < num_users; i++) {
                bool user_found = false;
                for (int j = 0; j < conversation.num_users; j++) {
                    if (strcmp(usernames[i], conversation.usernames[j]) == 0) {
                        user_found = true;
                        break;
                    }
                }
                if (!user_found) {
                    match = false;
                    break;
                }
            }
            if (match) {
                fclose(file);
                return conversation.port;
            }
        }
    }

    fclose(file);
    return -1;
}

GroupConversation activeGroupConversations(int client_fd, const char *username) { // Devolve o nome dos users e o porto das conversas de grupo que estão ativas
    FILE *file = fopen("groupConversationsLog.bin", "rb");
    if (file == NULL) {
        GroupConversation nulo = { .num_users = 0, .port = 0 };
        return nulo;
    }

    GroupConversation available_conversations[MAX_CONVERSATIONS];
    int counter = 0;
    char choices_to_send[1000] = "\nACTIVE GROUP CONVERSATIONS\n\n";
    char counter_str[5];

    GroupConversation conversation;
    while (fread(&conversation, sizeof(GroupConversation), 1, file) == 1) {
        for (int i = 0; i < conversation.num_users; i++) {
            if (strcmp(conversation.usernames[i], username) == 0) {
                available_conversations[counter] = conversation;
                counter++;
                sprintf(counter_str, "%d", counter);
                strcat(choices_to_send, counter_str);
                strcat(choices_to_send, ". Port: ");
                char port_str[10];
                sprintf(port_str, "%d", conversation.port);
                strcat(choices_to_send, port_str);
                strcat(choices_to_send, " Users: ");
                for (int j = 0; j < conversation.num_users; j++) {

					size_t len = strlen(conversation.usernames[j]);
                    if (conversation.usernames[j][len - 1] == '\n') {
                        conversation.usernames[j][len - 1] = '\0';
                    }

                    strcat(choices_to_send, conversation.usernames[j]);
                    if (j < conversation.num_users - 1) {
                        strcat(choices_to_send, ", ");
                    }
                }
                strcat(choices_to_send, "\n");
                break;
            }
        }
    }

    fclose(file);

    counter++;
    sprintf(counter_str, "%d", counter);
    strcat(choices_to_send, counter_str);
    strcat(choices_to_send, ". Return to the MENU\n\nSelection: ");

    int selection = -1;
    while (1) {
        sendString(client_fd, choices_to_send);
        char *selection_str = receiveString(client_fd);
        selection = atoi(selection_str);
        free(selection_str);

        if (selection == counter) { // Sair
            GroupConversation nulo = { .num_users = 0, .port = 0 };
            return nulo;
        } else if (selection > 0 && selection < counter) {
            return available_conversations[selection - 1];
        }
    }

    GroupConversation nulo = { .num_users = 0, .port = 0 };
    return nulo;
}

void erro(char *msg) { 
	printf("Error: %s\n", msg);
	exit(-1);
}