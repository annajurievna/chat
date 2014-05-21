#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

void swap( char* a, char* b ) {
	char c;
	c = *a;
	*a = *b;
	*b = c;
}

// Функция для преобразования из числа в строку для счетчика клиентов
void itoa( unsigned int n, char* s ) {
	int i = 0;
	while( n > 0 ) {
		s[i] = n % 10 + '0';
		n /= 10;
		i++;
	}
	s[i] = '\0';
	for( int j = 0; j < (i-1)/2; j++ ) {
		swap( &(s[j]), &(s[i-j-1]) );
	}
}

// Структура двусвязный список - используем для хранения клиентов
typedef struct List {
	struct List* prev;
	int sock;
	char nickname[30];
	struct List* next;
} List;

typedef struct TList {
	List* begin;
	List* end;
	int size;
} TList;

void ListPushBack( TList* list, List* data ) {
	if( list -> size != 0 ) {
		list -> end -> next = data;
		data -> prev = list -> end;
		list -> end = data;
		data -> next = NULL;
	} else { 
		list -> begin = data;
		list -> end = data;
		data -> next = NULL;
		data -> prev = NULL;
	}
	(list -> size)++;
}

void ListDelete( TList* list, List* data ) {
	close( data -> sock );
	List* left;
	List* right;
	left = data -> prev;
	right = data -> next;
	free( data );
	if( left != NULL && right != NULL ) {
		left -> next = right;
		right -> prev = left;
	} else if( left != NULL ) {
		list -> end = left;
		left -> next = NULL;
	} else if( right != NULL ) {
		right -> prev = NULL;
		list -> begin = right;
	} else {
		list -> begin = NULL;
		list -> end = NULL;
	}
	(list -> size)--;
}

void ListClear( TList* list ) {
	while( list -> size > 0 ) {
		ListDelete( list, list -> begin );
	}
}

// Отправляем в чат общее сообщение
void send_all_msg( TList* list, List* this, char* message ) {
	List* temp = list -> begin;
	char inf[1280];
	strcpy( inf, this -> nickname );
	strcat( inf, ": " );
	strcat( inf, message );
	while( temp != NULL ) {
		if( temp != this ) {
			send( temp -> sock, inf, strlen(inf) + 1, 0 ); 
		}
		temp = temp -> next;
	}
}

// Отправляем всем системное сообщение
void send_all( TList* list, List* this, char* message ) {
	List* temp = list -> begin;
	while( temp != NULL ) {
		if( temp != this ) {
			send( temp -> sock, message, strlen(message) + 1, 0 ); 
		}
		temp = temp -> next;
	}
}

int main(int argc, char* argv[]) {
	// counter - счетчик клиентов
	int counter = 1;
	int listener;
	struct sockaddr_in addr;
	int bytes_read;
	char buf[1024];
	
	listener = socket( AF_INET, SOCK_STREAM, 0 );
	if( listener < 0 ){
		perror( "socket" );
		exit(1);
	}
	
	fcntl(listener, F_SETFL, O_NONBLOCK);
    fcntl(0, F_SETFL, O_NONBLOCK);
    
	addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi (argv[1]) );
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if( bind( listener, ( struct sockaddr* )&addr, sizeof(addr) ) < 0 ) {
        perror( "bind" );
        exit(2);
    }
    printf ("Server is ready\n");
    listen( listener, 3 );
    
    TList clients;
    clients.size = 0;
    clients.begin = NULL;
    clients.end = NULL;
    
	while(1) {
		fd_set readset;
		FD_ZERO( &readset );
		FD_SET( listener, &readset );
		FD_SET( 0, &readset );
		List* temp = clients.begin;
		int max = listener;
		while( temp != NULL ) {
			FD_SET( temp -> sock, &readset );
			if( temp -> sock > max ) {
				max = temp -> sock;
			}
			temp = temp -> next;
		}
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 5000;
		
		if( select( max + 1, &readset, NULL, NULL, &timeout ) < 0 ) {
			perror( "select" );
			exit(3);
		}
		
		if( FD_ISSET( listener, &readset )) {
			int sock = accept( listener, NULL, NULL );
			if( sock < 0 ) {
				perror( "accept" );
				exit(4);
			}
			fcntl( sock, F_SETFL, O_NONBLOCK );
			List* temp = (List*)malloc( sizeof( List ));
			temp -> sock = sock;
			char number[12];
			itoa( counter, number );
			strcpy( temp -> nickname, "Client_" );
			strcat( temp -> nickname, number );
			ListPushBack( &clients, temp );
			counter++;
			printf ("%s joined!\n", temp -> nickname );
			char msg[265];
			strcpy( msg, temp -> nickname );
			strcat( msg, " joined!\n" );
			send_all( &clients, temp, msg );
		}
		
		temp = clients.begin;
		while( temp != NULL ) {
			if( FD_ISSET( temp -> sock, &readset )) {
				bytes_read = recv( temp -> sock, buf, 1024, 0 );
				// Сообщение о количестве полученных байт (серверное)
				printf( "%d bytes were recieved.\n", bytes_read);
				fflush( stdout );
				if( bytes_read <= 0 ) {
					List* temp_temp = temp -> next;
					char msg[40];
					strcpy( msg, temp -> nickname );
					strcat( msg, " left.\n" );
					send_all( &clients, temp, msg );
					close( temp -> sock );
					ListDelete( &clients, temp );
					temp = temp_temp;
					// Сообщение об удалении клиента из чата (серверное)
					printf( "Deleted.\n" );
					fflush( stdout );
					continue;
				}
				if( buf[0] != '/' ) {
					send_all_msg( &clients, temp, buf ); 
				} else if( strncmp( buf, "/name ", 6 ) == 0 ) {
					List* temp_temp = clients.begin;
					int flag = 0;
					while( temp_temp != NULL ) {
						//  Обработка ошибок: проверяем не занято ли желаемое имя
						if( strcmp( buf + 6, temp_temp -> nickname ) == 0 ) {
							send( temp -> sock, "Sorry. This name is taken.\n", 28, 0 );
							flag = 1;
							break;
						}
						temp_temp = temp_temp -> next;
					}
					// Если имя свободно - меняем его для клиента
					if( flag == 0 ) {
						char msg[80];
						strcpy( msg, temp -> nickname );
						strcat( msg, " changed name to " );
						strcat( msg, buf + 6 );
						strcat (msg, ".\n");
						// Сообщаем клиентам о смене имени
						send_all( &clients, temp, msg ); 
						strcpy( temp -> nickname, buf + 6 );
					} 
					// Для /exit - удаляем клиента
				} else if( strcmp( "/exit", buf ) == 0 ) {
					char msg[40];
					strcpy( msg, temp -> nickname );
					strcat( msg, " left.\n" );
					// Сообщаем остальным, что клиент покинул чат
					send_all( &clients, temp, msg );
					close( temp -> sock );			
				} else if( strncmp( buf, "/msg ", 5 ) == 0 ) {
					char* msg_recv = strchr( buf + 5, ' ' );
					*msg_recv = '\0';
					msg_recv++;
					List* temp_temp = clients.begin;
					int flag = 0;
					// Ищем адресата в списке и отправляем ему сообщение
					while( temp_temp != NULL ) {
						if( strcmp( buf + 5, temp_temp -> nickname ) == 0 ) {
							char msg[1060];
							strcpy( msg, temp -> nickname );
							strcat( msg, " (private): " );
							strcat( msg, msg_recv );
							send( temp_temp -> sock, msg, strlen(msg) + 1, 0 );
							flag = 1;
							break;
						}
						temp_temp = temp_temp -> next;
					} 
					// Имя клиента не было найдено. Сообщаем адресанту об ошибке.
					if( flag == 0 ) {
						send( temp -> sock, "This name wasn't found.\n", 25, 0 );
					}
				} 	
			}
			temp = temp -> next;
		}
		if( FD_ISSET( 0, &readset ) ) {
			if( fgets( buf, 1024, stdin ) == NULL ) {
				perror( "fgets" );
				exit(4);
			}
			// Закрытие сервера. Очищаем список клиентов.
			if( strncmp( "/exit", buf, 5 ) == 0 ) {
				printf( "Good bye!\n" );
				ListClear( &clients );
				close( listener );
				return 0;
			}		
		}
		fflush (stdout);
	}
	return 0;
}
