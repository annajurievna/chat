#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char* argv[]) {
	int sock;
	struct sockaddr_in addr;
	int bytes_read;
	char buf[1024];
	sock = socket( AF_INET, SOCK_STREAM, 0 );
	if( sock < 0 ) {
		perror( "socket" );
		exit(1);
	}
	
	addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi (argv[1]) );
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    printf ("connecting...\n");
    
	if( connect( sock, ( struct sockaddr* )&addr, sizeof(addr) ) < 0 ) {
        perror( "connect" );
        exit(2);
    }
	
	printf ("connected\n");
	
	fcntl( sock, F_SETFL, O_NONBLOCK );
	fcntl( 0, F_SETFL, O_NONBLOCK);
	
	while(1) {
		fd_set readset;
		FD_ZERO( &readset );
		FD_SET( sock, &readset );
		// 0 - stdin
		FD_SET( 0, &readset );
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 5000;
	
		if( select( sock + 1, &readset, NULL, NULL, &timeout ) < 0 ) {
			perror( "select" );
			exit(3);
		}
		
		if( FD_ISSET( sock, &readset )) {
			bytes_read = recv( sock, buf, 1024, 0 );
			//printf( "Something has been recieved.\n" );
			//fflush( stdout );
			// Если сервер закрывается, сообщаем об это клиентам.
			if( bytes_read <= 0 ) {
				close( sock );
				printf( "Server has been closed.\n" );
				fflush( stdout );
				return 0;
			}
			printf( "%s", buf );
		} else if( FD_ISSET( 0, &readset )) {
			if( fgets( buf, 1024, stdin ) == NULL ) {
				perror( "fgets" );
				exit(4);
			}
			fflush( stdout );
			
			// Отправка сообщений и обработка ошибок, возможных на этом этапе
			if( buf[0] != '/' ) {
				send( sock, buf, strlen(buf) + 1, 0 );
			// Проверка на содержание пробелов в nickname
			} else if( strncmp( buf, "/name ", 6 ) == 0 ) {
				if( strchr( buf + 6, ' ' ) != NULL ) {
					printf( "Incorrect input format.\n" );
				} else {
					buf[strlen(buf) - 1] = '\0';
					send( sock, buf, strlen(buf) + 1, 0 );
				}
			} else if( strncmp( "/exit", buf, 5 ) == 0 ) {
				send( sock, buf, strlen(buf) + 1, 0 );
				printf( "Good bye!\n" );
				close( sock );
				return 0;			
			} else if( strncmp( buf, "/msg ", 5 ) == 0 ) {
				// Проверка на наличие по крайней мере одного пробела после "/msg "
				if( strchr( buf + 5, ' ' ) != NULL ) {
					send( sock, buf, strlen(buf) + 1, 0 );
				} else {
					printf( "Incorrect input format.\n" );
				}
			} else {
				// На случай, если посылаем неведомую ерунду, т.е. после / стоят неизвестные нам команды
				printf( "Incorrect input format.\n" );
			}
		}
		fflush (stdout);
	}
	close( sock );
	return 0;
}
