#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <zlib.h>

#define MAXLINE 100
#define SERVER_PORT 12345
#define MAXTHREAD 100
#define LISTENNQ 10
#define CHUNKED_SIZE 50000
#define CHUNK_FLAG 1
#define GZIP_FLAG 1
#define windowBits 15
#define GZIP_ENCODING 16
#define CHUNK 0x4000


void* request_func(void *args);
void notfound_404(int client_fd);


int main(int argc, char **argv)
{
		int server_fd, client_fd;
		struct sockaddr_in server_addr, client_addr;
		socklen_t len = sizeof(struct sockaddr_in);
		
		//thread init
		int threads_count = 0;
		pthread_t threads[MAXTHREAD];
		
		//server socket init
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if(server_fd < 0)
		{
			perror("socket");
			exit(1);
		}
		
		//server addr init
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(SERVER_PORT);
		
		//bind server addr and socket
		if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		{
			perror("bind");
			close(server_fd);
			exit(1);
		}
		
		if(listen(server_fd, LISTENNQ) < 0)
		{
			perror("listen");
			close(server_fd);
			exit(1);
		}
		
		while(1)
		{
			printf("thread:%d\n",threads_count);
			//incoming connection
			client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
			if(client_fd < 0)
			{
				perror("Connection failed...\n");
				continue;
			}
			
			printf("Got client connection.....\n");
			//void* temp_client = &client_fd;
			if(pthread_create(&threads[threads_count], NULL,request_func, (void *)client_fd)!=0)
			{
				perror("Error:When creating thread\n");
				exit(1);
			}
		
			if(++threads_count >= MAXTHREAD){
				break;
			}
		}
		
		printf("MAX thread numver reache, wait for all threads to finish and exit...\n");
		int i =0;
		for(i=0;i<MAXTHREAD;++i)
		{
			pthread_join(threads[i], NULL);
		}
		
		return 0;
}

void* request_func(void *args)
{
	int client_fd = (int)args;
	char buffer[2048];
	memset(buffer, 0, 2048);
	read(client_fd, buffer, 2047);
	printf("%s\n",buffer);
	char* file_name = malloc(100*sizeof(char));
	char* file_type = NULL;
	char* header_file_type = NULL;
	FILE* file = NULL;
	int file_size = 0;
	char header[4096];
	//check http get request
	if(strstr(buffer,"GET /")!=NULL){
		//cut file name position
		char* temp_1 = strstr(buffer,"GET /");
		int start_position = temp_1 - buffer +5;
		char* temp_2 = strstr(buffer, "HTTP");
		int end_position = temp_2 - buffer -6;
	
		//check empty file name
		if(end_position >0){
				//printf("get file's buffer:\n%s\n",buffer);
				strncpy(file_name,buffer+start_position, end_position);
				printf("file_name:%s\n",file_name);
				
				//check if contain file type with "."
				if((file_type = strstr(file_name,"."))!=NULL){
						file_type = file_type+1;
						printf("file_type is %s\n",file_type);
							
							//compare and convert file type
							if(file_type!=NULL){
								if(strcmp(file_type,"html")==0){
									header_file_type = "text/html";
								}else if(strcmp(file_type,"jpg")==0){
									header_file_type ="image/jpeg";
								}else if(strcmp(file_type,"pdf")==0){
									header_file_type ="application/pdf";
								}else if(strcmp(file_type,"pptx")==0){
									header_file_type ="application/vnd.openxmlformats-officedocument.presentationml.presentation";
								}else if(strcmp(file_type,"css")==0){
									header_file_type = "text/css";
								}else if(strcmp(file_type,"txt")==0){
									header_file_type = "text/plain";
								}
								printf("header file type is: %s\n",header_file_type);
								
								//check file type because different open file method
								if((strcmp(file_type, "html")==0)||(strcmp(file_type, "css")==0)||(strcmp(file_type, "txt")==0)){
									file = fopen(file_name, "r");
								}else if((strcmp(file_type, "pdf")==0)||(strcmp(file_type, "pptx")==0)||(strcmp(file_type, "jpg")==0)){
									file = fopen(file_name, "rb");
								}else{
									printf("cant deal with this file type\n");
								}
								if(file!= NULL){
									printf("open file with fd:%d\n",file);
									
									//get file size
									fseek(file, 0, SEEK_END);
									file_size = ftell(file);
									rewind(file);
									printf("file size is:%d\n",file_size);
									
									//construct header
									strcat(header,"HTTP/1.1 200 OK\r\nContent-Type:");
									strcat(header,header_file_type);
									strcat(header,"\r\n");
								
									if(GZIP_FLAG &&(strcmp(file_type,"txt")==0)){
									  strcat(header,"Content-Encoding:gzip\r\n");
									}
									strcat(header,"Keep-Alive: timeout=5, max=100\r\n");
									strcat(header,"Connection: Keep-Alive\r\n");
									if(GZIP_FLAG &&(strcmp(file_type,"txt")==0)){
									
									}else{
									strcat(header,"Content-Length:");
									char temp_size[10];
									sprintf(temp_size, "%d", file_size);
									strcat(header,temp_size);
									strcat(header,"\r\n");
									}
									if(CHUNK_FLAG){
										if(GZIP_FLAG && (strcmp(file_type, "txt")==0)){
												char file_temp_buffer[file_size];
												int read_count = 0;
												read_count = fread(file_temp_buffer, 1, file_size, file);
												char temp_size1[10];
												sprintf(temp_size1, "%d", 10);
												//strcat(header,temp_size1);
												//strcat(header,"\r\n");
												strcat(header,"\r\n");
												printf("header:\n %s \n",header);
												write(client_fd, header, strlen(header));
					
												unsigned char out[CHUNK];
												z_stream zip;
												zip.zalloc = Z_NULL;
												zip.zfree = Z_NULL;
												zip.opaque = Z_NULL;
												
												deflateInit2(&zip, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits| GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY);
												
												zip.next_in = (unsigned char *)file_temp_buffer;
												zip.avail_in = strlen(file_temp_buffer);
												do{
												  int zip_size;
												  zip.avail_out = CHUNK;
												  zip.next_out = out;
												  deflate(&zip,Z_FINISH);
												  zip_size = CHUNK - zip.avail_out;
												
												  write(client_fd, out, zip_size);
												}while(zip.avail_out == 0);
												deflateEnd(&zip);
												
												write(client_fd, "\r\n\r\n", 4);
												close(client_fd);
												close(file);
												free(file_name);
										}else{
											
											strcat(header,"Transfer-Encoding: chunked\r\n\r\n");
											printf("header:\n %s \n",header);
											write(client_fd, header, strlen(header));
											char file_temp_buffer[CHUNKED_SIZE];
											int read_count = 0;
											char temp_size[10];
											while((read_count = fread(file_temp_buffer, 1, CHUNKED_SIZE, file))>0){
												sprintf(temp_size, "%x\r\n", read_count);

												write(client_fd, temp_size, strlen(temp_size));
												write(client_fd, file_temp_buffer, read_count);
												write(client_fd, "\r\n",2);
												sleep(1);
											}
											write(client_fd, "0\r\n\r\n", 5);
											close(client_fd);
											close(file);
										}
									}else{
										if(GZIP_FLAG &&(strcmp(file_type,"txt")==0)){
									
										}else{
										strcat(header,"\r\n");
										printf("header:\n %s \n",header);
										write(client_fd, header, strlen(header));
										}
										//read file content
										char file_temp_buffer[file_size];
										int read_count = 0;
										read_count = fread(file_temp_buffer, 1, file_size, file);
										if(read_count > 0){
											printf("inside buffer: \n%s \n",file_temp_buffer);
											if((strcmp(file_type, "html")==0)||(strcmp(file_type, "css")==0)||(strcmp(file_type, "txt")==0)){
												if(GZIP_FLAG && (strcmp(file_type, "txt")==0)){
														char temp_size1[10];
														sprintf(temp_size1, "%d", 10);
														//strcat(header,temp_size1);
														//strcat(header,"\r\n");
														strcat(header,"\r\n");
														printf("header:\n %s \n",header);
														write(client_fd, header, strlen(header));
							
														unsigned char out[CHUNK];
														z_stream zip;
														zip.zalloc = Z_NULL;
												  		zip.zfree = Z_NULL;
														zip.opaque = Z_NULL;
														
														deflateInit2(&zip, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits| GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY);
														
														zip.next_in = (unsigned char *)file_temp_buffer;
														zip.avail_in = strlen(file_temp_buffer);
														do{
														  int zip_size;
														  zip.avail_out = CHUNK;
														  zip.next_out = out;
														  deflate(&zip,Z_FINISH);
														  zip_size = CHUNK - zip.avail_out;
														
														  write(client_fd, out, zip_size);
														}while(zip.avail_out == 0);
														deflateEnd(&zip);
														
														write(client_fd, "\r\n\r\n", 4);
												}else{
														printf("strlen:%d",strlen(file_temp_buffer));
														write(client_fd, file_temp_buffer, strlen(file_temp_buffer));
														write(client_fd, "\r\n\r\n", 4);
												}
											}else if((strcmp(file_type, "pdf")==0)||(strcmp(file_type, "pptx")==0)||(strcmp(file_type, "jpg")==0)){
												write(client_fd, file_temp_buffer, read_count);
												write(client_fd, "\r\n\r\n", 4);
											}
										}else{
											printf("file empty or cant read\n");
										}
										
										close(client_fd);
										close(file);
										free(file_name);
									}
								}else{
									printf("cant open file or file cant find!\n");
									notfound_404(client_fd);
									close(client_fd);
								}
							}else{
								printf("file_type is NULL\n");
								notfound_404(client_fd);
								close(client_fd);
							}
				}else{
						printf("no . inside requested file name\n");
						notfound_404(client_fd);
						close(client_fd);
				}
		}else{
				printf("empty request!\n");
				notfound_404(client_fd);
				close(client_fd);
		}
	}else{
		printf("not a get request\n");
		close(client_fd);
	}
	
}

void notfound_404(int client_fd)
{
		char http[] = "HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Content-Length: 138\r\n"
		"Connection: Keep-Alive\r\n\r\n";
		
		char html[] = "<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested file was nto found on this server.</p></body></html>\r\n\r\n";
		
		char* reply = malloc(4096*sizeof(char));
		strcat(reply, http);
		strcat(reply, html);
		write(client_fd, reply, strlen(reply));
}



