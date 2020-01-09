#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#define PORT "2121" // OUR SERVER IS LISTENING TO THIS PORT
#define msg_buffer_size 2000 // circular buffer size

#define CIRC_BBUF_DEF(x,y)                 \
    struct message_info messages_bucket[y];\
    circ_bbuf_t x = {                      \
        .buffer = messages_bucket,  	   \
        .head = 0,                         \
        .tail = 0,                         \
        .maxlen = y                        \
    }

int isEmpty = 1; // flag to check if buffer has been used at least once

// ########## FUNCTIONS ##########
// Error handling
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

// Message Struct Declaration
struct message_info
{
	uint32_t AEM_sender;
	uint32_t AEM_receiver;
	unsigned long long int timestamp;
	char message[256];
};

// Buffer Struct Declaration
typedef struct {
    struct message_info * const buffer;
    int head;
    int tail;
    const int maxlen;
} circ_bbuf_t;

CIRC_BBUF_DEF(my_circ_buf, msg_buffer_size);

// Push data in buffer
int circ_bbuf_push(circ_bbuf_t *c, struct message_info data)
{
    int next;

    next = c->head + 1;  // next is where head will point to after this write.
    if (next >= c->maxlen)
        next = 0;

    //if (next == c->tail)  // if the head + 1 == tail, circular buffer is full
    //    return -1;

    c->buffer[c->head] = data;  // Load data and then move
    c->head = next;             // head to next data offset.
    return 0;  // return success to indicate successful push.
}

// Pull data from buffer
int circ_bbuf_pop(circ_bbuf_t *c, struct message_info *data)
{
    int next;

    if (c->head == c->tail)  // if the head == tail, we don't have any data
        return -1;

    next = c->tail + 1;  // next is where tail will point to after this read.
    if(next >= c->maxlen)
        next = 0;

    *data = c->buffer[c->tail];  // Read data and then move
    c->tail = next;              // tail to next offset.
    return 0;  // return success to indicate successful push.
}

// Random Message Generator
static char *rand_string(char *str, size_t size)
{
	srand(time(NULL)); 
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK .";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

// Convert message to String
char* rand_string_alloc(size_t size)
{
     char *s = malloc(size + 1);
     if (s) {
         rand_string(s, size);
     }
     return s;
}

// Check for Redundancies
int checkIfExists (struct message_info k)
{
	int b;
	for(b=0; b<msg_buffer_size; b++){
		if(messages_bucket[b].AEM_sender == k.AEM_sender
			&& messages_bucket[b].AEM_receiver == k.AEM_receiver
			&& strcmp(messages_bucket[b].message, k.message) == 0
			&& messages_bucket[b].timestamp == k.timestamp){
			return 1;
		}
	}
 	return 0;
}

// ######## THREAD FUNCTIONS ########
// Create a random message based on the criteria given
void *generateMsg()
{
	for(;;){
		const int msg_size = 10; // define msg size
   
	  	char to_send[500] = ""; // temp msg variable to send

     		char* msg = rand_string_alloc(msg_size); // create random msg string
     	
     		struct message_info a_message1;
     	
     		// Initialize the a_message value for test purposes
     		strcpy(a_message1.message, msg);
     		a_message1.AEM_sender = 8977;
     		int r = (rand() % 500) + 8501;
     		a_message1.AEM_receiver = r; //make it random 8500-9000
     		a_message1.timestamp = (unsigned long long int)time(NULL);

		// Generate temp message
		char str[1000];
		sprintf(str, "%d", a_message1.AEM_sender);

		strcat(to_send, (char*) str);
		strcat(to_send, "_");
    		  
		sprintf(str, "%d", a_message1.AEM_receiver);
		strcat(to_send, (char*) str);
    		  
		strcat(to_send, "_");
    		  
		sprintf(str, "%llu", a_message1.timestamp);
		strcat(to_send, str);
		
		strcat(to_send, "_");
		strcat(to_send, (char*) a_message1.message);
		printf("%s", to_send);
		
		// Push newly created message in the bucket
    		circ_bbuf_push(&my_circ_buf, a_message1);
    		isEmpty=0;
    		printf("\nNEW MESSAGE CREATED\n");
    	
    		printf("\n--OUR BUCKET--\n");
    		for(int i=0; i<msg_buffer_size; i++){
    		printf("%d message: %s\n", i+1, messages_bucket[i].message);
    	}
    	
    	// Sleep currect thread for random time 1-5 min till next message to be created
    	int r_t = (rand() % 240) + 60; 
    	sleep(r_t);
    	//sleep(5);
    }
	return NULL;
}
// Discover the server IP/port and send the message by using sockets
void *send_messages()
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    for(;;)
    {
    	char HOSTNAME[10] = "10.0.";
       	int hostAvailable = 0;
       	int elem_pos = -1;

   	// CHECK FOR NEW CONNECTIONS WITH NMAP
   	//netstat -an | grep 10.0. | awk -F "[ :]+" '{print $5}'
   	//this returns ip and PORT saved into a file.
   	system("netstat -an | grep 192.168. | awk -F \"[ :]+\" '{print $5}' | tee netstat.log > /dev/null");

    	char c[1000];
    	FILE *fptr;
    	if ((fptr = fopen("netstat.log", "r")) == NULL) {
      		printf("Error! opening file");
        	// Program exits if file pointer returns NULL.
        	exit(1);
    	}

	// IP, PORT AND SEARCH, SEND AND DESTROY
   	int the_receiver;
   	int part1 = 0;
   	int part2 = 0;
   	int part3 = 0; //port
	    
    	// Search for specific Address in the file - if 10.0. format it has encounter a new device
    	while(fscanf(fptr, "%s", c)!=EOF){
    		//printf("Data from the file: %s\n", c);
    		
   		// SPLIT IP AND PORT
   		char *ptr = strtok(c, ".");
   		// Check IP and if it is 10.0. save it, then save port
   		if(atoi(ptr)==10){
   			ptr = strtok(NULL, ".");
			if(atoi(ptr)==0){ // that means IP is 10.0
				ptr = strtok(NULL, ".");
				part1 = atoi(ptr);
				ptr = strtok(NULL, ".");
				part2 = atoi(ptr);
				ptr = strtok(NULL, ".");
				part3 = atoi(ptr);
				the_receiver = part1 * 100;
				the_receiver = the_receiver + part2;
				hostAvailable = 1;
			}
   			// SEARCH IF IP IS ON BUCKET AND RETURN position on bucket
   			for(int i = 0; i<msg_buffer_size; i++){
   				if(messages_bucket[i].AEM_receiver == the_receiver){
   					//found potential message;
   					elem_pos = i;
   					break; // TODO: MAKE IT STORE ALL THE MESSAGES AND SEND THEM ALL AT ONCE
   				}
   			}
			break;
		}
    	}
    	fclose(fptr);
    	sleep(1);
    	
    	// reformat hostname
    	char str[22];
    	sprintf(str, "%d", part1);
    	strcat(HOSTNAME, (char*) str);
    	strcat(HOSTNAME, ".");
    	sprintf(str, "%d", part2);
    	strcat(HOSTNAME, (char*) str);
	printf("%s\n", HOSTNAME);
   
    	// FETCH LAST MESSAGE FROM THE BUCKET
    	//if(isEmpty==0){
    	//	char ip_address[14] = "10.0.";
    	//	char bf1[14];
    	//	char bf2[14];
    	//	sprintf(bf1, "%d", (int) messages_bucket[my_circ_buf.tail].AEM_receiver / 100);
   	 	//	sprintf(bf2, "%02d", (int) messages_bucket[my_circ_buf.tail].AEM_receiver % 100);

    	//	strcat(ip_address, bf1);
    	//	strcat(ip_address, ".");
    	//	strcat(ip_address, bf2);
    	//	printf("%s\n", ip_address);
   	 	//}
    	
    	//portno = atoi(PORT); 
    	portno = part3;
    
    //if (my_circ_buf.head!=my_circ_buf.tail){ // if msg bucket is not empty
	
    	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    	if (sockfd < 0) 
       	error("ERROR opening socket");
       	
    	server = gethostbyname(HOSTNAME);
    	if (server == NULL) {
       		fprintf(stderr,"ERROR, no such host\n");
        	//exit(0);
    	} 
    	else if (isEmpty==1) {
    		printf("ERROR empty buffer!!!\n");
    	}
    	else if (hostAvailable==0) {
    		printf("ERROR host not available!!!\n");
    	}
    	else {
    
    		bzero((char *) &serv_addr, sizeof(serv_addr));
    		serv_addr.sin_family = AF_INET;
    		bcopy((char *)server->h_addr, 
         		(char *)&serv_addr.sin_addr.s_addr,
         		server->h_length);
    		serv_addr.sin_port = htons(portno);
    
    		if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
       		error("ERROR connecting");
    		//printf("Please enter the message to send: ");
    		//bzero(buffer,256);
    		//fgets(buffer,255,stdin);
   
       		// define message to put in buffer
		struct message_info out_data;
	
	  	char to_send_msg[500] = "";
	  	char str[1000];
		sprintf(str, "%d", messages_bucket[elem_pos].AEM_sender);

		strcat(to_send_msg, (char*) str);
		strcat(to_send_msg, "_");
    		  
		sprintf(str, "%d", messages_bucket[elem_pos].AEM_receiver);
		strcat(to_send_msg, (char*) str);
    		  
		strcat(to_send_msg, "_");
    		  
		sprintf(str, "%llu", messages_bucket[elem_pos].timestamp);
		strcat(to_send_msg, str);
    		  
		strcat(to_send_msg, "_");
		strcat(to_send_msg, (char*) messages_bucket[elem_pos].message);

		// Do I need to remove the message from the bucket after 
		// I send it or do I keep it?????
		// Remove lastmessage from the bucket
    		//if (circ_bbuf_pop(&my_circ_buf, &out_data)) {
       		//	printf("CB is empty\n");
        	//	//return -1;
    		//}

    		n = write(sockfd, to_send_msg, strlen(to_send_msg));
    		printf("Message SENT: %s\n", to_send_msg);
    		if (n < 0) 
      	   		error("ERROR writing to socket");
    		bzero(buffer,256);
    		n = read(sockfd,buffer,255);
    		if (n < 0) 
         		error("ERROR reading from socket");
    		printf("%s\n",buffer);
    		
    		sleep(1);    
    		close(sockfd);
    	}
	//}
	}
	return NULL;
}

// Our server listening to other devices in the same range 10.0.0.1/24
void *Server()
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
        
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(PORT);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);

     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
                
     for(;;){
     	listen(sockfd,5);
     	clilen = sizeof(cli_addr);
     
     	newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
     	if (newsockfd < 0) 
          error("ERROR on accept");
     	bzero(buffer,256);
     	n = read(newsockfd,buffer,255);
     	if (n < 0) error("ERROR reading from socket");
     	//printf("Here is the message: %s\n",buffer);
     	
     	struct message_info a_message2;
     	
     	// split the message
    	char *token = strtok(buffer, "_");
    	a_message2.AEM_sender = atoi(token);
    	token = strtok(NULL, "_");
    	a_message2.AEM_receiver = atoi(token);
    	token = strtok(NULL, "_");
    	a_message2.timestamp = atoi(token);
    	token = strtok(NULL, "_");
  	memset(a_message2.message, 0, sizeof a_message2.message);
     	strcat(a_message2.message, token);
     	
     	// Check if the message allready exists in the bucket
     	if(checkIfExists(a_message2)==0) {
     		// push the message in the bucket
    		circ_bbuf_push(&my_circ_buf, a_message2);
    		printf("MESSAGE RECEIVED");
    		
      		n = write(newsockfd,"Message RECEIVED\n",18); //reply
      		if (n < 0) error("ERROR writing to socket");
    	}
    	else {
    	     	printf("ERROR Message already exists in bucket\n");
      	}
     	close(newsockfd);
     }
     
     close(sockfd);
     return NULL; 
}

// ########## MAIN ##########
int main(int argc, char *argv[])
{   
	int NTHREADS=3;
	pthread_t threads[NTHREADS];
  	int thread_args[NTHREADS];
  	int rc, i;
  	
 	//Run server as a Thread
	pthread_create(&threads[0], NULL, Server, NULL);

	//Run generate messages as a Thread	
	pthread_create(&threads[1], NULL, generateMsg, NULL);
	
	//Run send messages from client to other server as a Thread
	pthread_create(&threads[2], NULL, send_messages, NULL);

	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[2], NULL);
	
	//Free the variables here??
	return 0;
}
