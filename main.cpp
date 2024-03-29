/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: luisfernandolins
 *
 * Created on June 5, 2019, 11:48 AM
 */

#include <cstdlib>
#include <ctime>    // For time()
#include <iostream>       // std::cout
#include <atomic>         // std::atomic_flag
#include <thread>         // std::thread
#include <vector>         // std::vector
#include <ctime>        // std::clock
#include <stdio.h> 
#include <numeric>   
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <unistd.h> 
#include <signal.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string> 
#include <arpa/inet.h> 
#include <semaphore.h>
#include <string.h>
#include <csignal>
#define PORT 3000 

using namespace std;

/*
 * 
 */

sem_t disabled_mutex, leader_mutex, sent_msgs_count_mutex, recv_msgs_count_mutex, election_timer_mutex;
clock_t election_timer = -1;
bool disabled = false;
int leader_pid = -1, pid, max_id;
int sent_msgs_count[5]={0};
int recv_msgs_count[5]={0};


int getch(void) {
      int c=0;

      struct termios org_opts, new_opts;
      int res=0;
          //-----  store old settings -----------
      res=tcgetattr(STDIN_FILENO, &org_opts);
      assert(res==0);
          //---- set new terminal parms --------
      memcpy(&new_opts, &org_opts, sizeof(new_opts));
      new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
      tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
      c=getchar();
          //------  restore old settings ---------
      res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);
      assert(res==0);
      return(c);
}

int sendingMessages();
int recvMessages();


int sendMessages(int code, int id){
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    string msg; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(id);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0){ 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    }

        //connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){ 
        // printf("\nConnection error\n"); 
        close(sock);
        return -1; 
    } 

    msg = to_string(code) + "|" + to_string(pid);
        
    send(sock , (const char *)msg.c_str(), 1024, 0);
    close(sock);
    
    switch(code){
        sem_wait(&sent_msgs_count_mutex);
        case 1:
           sent_msgs_count[0]++; 
        case 2:
           sent_msgs_count[1]++; 
        case 3:
           sent_msgs_count[2]++; 
        case 4:
           sent_msgs_count[3]++;
        case 5:
           sent_msgs_count[4]++;
        sem_post(&sent_msgs_count_mutex);
    }

    return 0;
}

void signal_handler1(int signal)
{
    if(!disabled){
        printf("Sending FORCED ALIVE \n");
        sendMessages(4, (PORT + leader_pid)); //sends alive msg
        sem_wait(&election_timer_mutex);
            election_timer = clock();
        sem_post(&election_timer_mutex);
    }
}

int arraySum(int a[], int n)  
{ 
    int initial_sum  = 0;  
    return accumulate(a, a+n, initial_sum); 
} 

int keycommands(){ // Reads keypress at any time and prints 'Done!' if key is f, and ends program is key is g.
    char inp;
    
    while(inp!='g'){
        inp=getch();
    
        if (inp=='1'){
            // Check leader; creates an interruption
            raise(SIGUSR1);            
        }
        else if(inp =='2'){
            //disable process
            printf("Shuting down process..\n");
            sem_wait(&disabled_mutex);
                disabled = true;
            sem_post(&disabled_mutex);
        }
        else if(inp =='3'){
            //enable process
            printf("Starting process..\n");
            sem_wait(&disabled_mutex);
                disabled = false;
            sem_post(&disabled_mutex);
        }
        else if(inp =='4'){
            int n1= sizeof(sent_msgs_count)/sizeof(sent_msgs_count[0]); 
            int n2 = sizeof(recv_msgs_count)/sizeof(recv_msgs_count[0]); 
            
            printf("\nSent messages :\nELECTION:%d\nOK:%d\nLEADER:%d\nALIVE:%d\nALIVE-OK:%d\n\n\n", sent_msgs_count[0], sent_msgs_count[1], sent_msgs_count[2], sent_msgs_count[3],sent_msgs_count[4]);
            printf("Received messages :\nELECTION:%d\nOK:%d\nLEADER:%d\nALIVE:%d\nALIVE-OK:%d\n\n\n", recv_msgs_count[0], recv_msgs_count[1], recv_msgs_count[2], recv_msgs_count[3],recv_msgs_count[4]);
            printf("Sent messages total: %d\nReceived messages total: %d\nCurrent leader: %d\n\n", arraySum(sent_msgs_count,n1), arraySum(recv_msgs_count,n2), leader_pid);
        }
        else if(inp =='5'){
            sem_wait(&recv_msgs_count_mutex);
            sem_wait(&sent_msgs_count_mutex);
                fill(begin(recv_msgs_count), end(recv_msgs_count), 0);
                fill(begin(sent_msgs_count), end(sent_msgs_count), 0);
            sem_post(&sent_msgs_count_mutex);
            sem_post(&recv_msgs_count_mutex);
        }
        

    }
    
    return 0;
}
int recvMessages(){
    int server_fd, new_socket, valread, sock; 
    struct sockaddr_in address, cliaddress; 
    int opt = 1; 
    int cliaddrlen = sizeof(cliaddress); 
    int addrlen = sizeof(address); 
    char buffer[15] = {0}; 
    int i = 0;
    int msg[15];
    char *value;
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){ 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    }
       
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){ 
        perror("setsockopt"); 
    // Forcefully attaching socket to the port 8080 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT + pid ); 
       
    // Forcefully attaching socket to the port 8080 
    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0){ 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }

    if (listen(server_fd, 50) < 0){ 
        printf("Listening error\n"); 
        exit(1); 
    } 


    socklen_t len;
    while(1){
        // valread = recvfrom(server_fd, (char *)buffer, 1024, MSG_WAITALL, ( struct sockaddr *) &address, (socklen_t*)&addrlen);
        if ((sock = accept(server_fd, (struct sockaddr *)&cliaddress, (socklen_t*)&cliaddrlen))<0){ 
            printf("Accept error\n");   
            exit(1); 
        }
        recv(sock , buffer, 1024, 0);
        close(sock);
        // printf("Server recieved: %s\n",buffer);
        value = strtok(buffer, "|");
        while(value != NULL){
           msg[i] = atoi(value);
           value = strtok(NULL, "|");
           i ++;
        }
        i = 0;
        
        switch(msg[0]){
            sem_wait(&recv_msgs_count_mutex);
            case 1:
               recv_msgs_count[0]++; 
            case 2:
               recv_msgs_count[1]++; 
            case 3:
               recv_msgs_count[2]++; 
            case 4:
               recv_msgs_count[3]++;
            case 5:
               recv_msgs_count[4]++;
            sem_post(&recv_msgs_count_mutex);
    }

        if(msg[0]==3){ //LIDER
            printf("Received LEADER\n");            
            sem_wait(&leader_mutex);
                leader_pid = msg[1];
            sem_post(&leader_mutex);
            sem_wait(&election_timer_mutex);
                election_timer = -1;
            sem_post(&election_timer_mutex);

        }

        if(!disabled){
            switch(msg[0]){
                case 1: //election 
                    printf("Received ELECTION\n");
                    if(msg[1] < pid){ 
                        printf("Sending OK\n");
                        sendMessages(2, (PORT + msg[1])); //send OK to stop election
                        printf("Starting election\n");
                        for(int i = 1; i <= max_id; i++){
                            if(i != pid)
                                sendMessages(1, (PORT + i)); //start new election
                        }
                        sem_wait(&leader_mutex);
                            leader_pid = -1; //If no response will become leader 
                        sem_post(&leader_mutex);
                    }
                    sem_wait(&election_timer_mutex);
                        election_timer = clock(); 
                    sem_post(&election_timer_mutex);

                    break; 
                case 2: //OK 
                        printf("Received OK\n");
                        //reset election time
                        sem_wait(&election_timer_mutex);
                            election_timer = clock();
                        sem_post(&election_timer_mutex);
                    break;
                case 4: //ALIVE sends ALIVE-OK to leader
                    if(leader_pid == pid){
                        // printf("Sending ALIVE-OK\n");
                        sendMessages(5, (PORT + msg[1]));
                    }
                    break;
                case 5: //ALIVE-OK
                        // printf("Received ALIVE-OK\n");
                        sem_wait(&election_timer_mutex);
                            election_timer = -1;  //setting to -1 will resend ALIVE
                        sem_post(&election_timer_mutex);
                    break; 
            } 
        }
    }
    return 0; 
}

int checkLeader(){
    
    void (*hand_pt1)(int);
    hand_pt1 = &signal_handler1;
    
    while(1){
        if(leader_pid < 0 && election_timer < 0){
            printf("Starting new election \n");
            for(int i = 1; i <= max_id; i++){
                if(i != pid)
                    sendMessages(1, (PORT + i));
            }
            sem_wait(&election_timer_mutex);
                election_timer = clock();
            sem_post(&election_timer_mutex);

        }    
        else if(leader_pid < 0 && election_timer > 0 && (clock() - election_timer) > (max_id - pid)*10){
            printf("Becoming new leader \n");
            //declares itself as leader if no new messages
            for(int i = 1; i <= max_id; i++){
                if(i != pid)
                    sendMessages(3, (PORT + i));
            }
            sem_wait(&leader_mutex);
            sem_wait(&election_timer_mutex);
                leader_pid = pid;
                election_timer = -1;
            sem_post(&leader_mutex);
            sem_post(&election_timer_mutex);
        }
        else if(leader_pid > 0 && !disabled){
            if(election_timer < 0 && leader_pid != pid){
                // printf("Sending ALIVE \n");
                sendMessages(4, (PORT + leader_pid)); //sends alive msg
                sem_wait(&election_timer_mutex);
                    election_timer = clock();
                sem_post(&election_timer_mutex);
                
            }else if(election_timer > 0 && (clock() - election_timer) > (max_id - pid)*10){
                printf("Starting new election \n");
                for(int i = 1; i <= max_id; i++){
                    if(i != pid)
                        sendMessages(1, (PORT + i));
                }
                sem_wait(&leader_mutex);
                sem_wait(&election_timer_mutex);
                    leader_pid = -1;
                    election_timer = clock();
                sem_post(&leader_mutex);
                sem_post(&election_timer_mutex);
            }
        }
        
        signal(SIGUSR1,signal_handler1);
        
        sleep(1);        
    }
}

void kill(thread arg){
    
}

int main(int argc, char** argv) {
    if(argc != 3){
        printf("Usage: %s $id $max_id\n", argv[0]);
        exit(1);
    }
    thread myThreads[3];
    
    sem_init(&disabled_mutex, 0, 1); 
    sem_init(&leader_mutex, 0, 1); 
    sem_init(&election_timer_mutex, 0, 1); 
    sem_init(&sent_msgs_count_mutex, 0, 1); 
    sem_init(&recv_msgs_count_mutex, 0, 1); 

    pid = atoi(argv[1]);
    printf("pid %d\n", pid);
    max_id = atoi(argv[2]); 

    myThreads[0] = thread(keycommands);
    myThreads[1] = thread(recvMessages);
    myThreads[2] = thread(checkLeader);
    

    for(int i = 0; i < sizeof(myThreads); i++){
        myThreads[i].join();
    }

    sem_destroy(&disabled_mutex); 
    sem_destroy(&sent_msgs_count_mutex); 
    sem_destroy(&recv_msgs_count_mutex); 
    sem_destroy(&leader_mutex); 
    
    return 0;
}

