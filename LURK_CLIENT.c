// Run like this:  simple_client address port
// Results in argv ["./simple_client", "address", "port"]

#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/ip.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<readline/history.h>
//#include<readline/printw.h>
#include<string.h>
#include<pthread.h>
#include<ncurses.h>
#include <signal.h>
#include <vector>
#include <iostream>
//structures for reading and writing

int maxy, maxx, input_cursor, output_cursor, found; //global dimensions
WINDOW *new_window, *input_window, *menu_options, *connections_display, *character_display, *player_stat;
pthread_mutex_t mutex;
char GME_description[65536];
uint16_t savedRoom = 0;
uint16_t currentRoom = 0;
char savedName[32];
int lc = 2;
int fightCalled = 0;


struct Message {        //type 1
        uint8_t type;
        uint16_t message_length;
        char receiver[32];
        char sender[30];
        uint8_t lastbyte, lastbyte2;
        char message[65536];
}__attribute__((packed));

struct version {
        uint8_t type, major, minor;
        uint16_t extensions;
}__attribute__((packed));

struct game {
        uint8_t type;
        uint16_t initial_points, stat_limit, description_length;
}__attribute__((packed));

struct room {
        uint8_t type;
        uint16_t roomNum;
        char roomName[32];
        uint16_t length;
        char room_description[1024];
}__attribute__((packed));

struct character {
        uint8_t type;
        char name[32];
        uint8_t flags;
        uint16_t attack, defense, regan;
        int16_t health;
        uint16_t gold, room, description_length;
        char player_description[1024];
}__attribute__((packed));

struct error {
        uint8_t type, code;
        uint16_t message_length;
        char message[1024];
}__attribute__((packed));



//structure declarations
struct Message message = {1, 0, "", "", 0, 0, ""};
struct version VRS = {0, 0, 0, 0};
struct game GME = {11, 0, 0, 0};
struct character PLAYER = {10, "", 0x00, 0, 0, 0, 0, 0, 0, 0, ""};
struct error ER = {7, 0, 0, ""};
struct room ROOM = {9, 0, "", 0, ""};

std::vector<struct room> connect_vect;
//function declarations
void print_menu();
void handle_resize(int sig);

//thread from receiving from the server. we are going to need to include types
void *recv_thread(void *p){
        int skt = *(int*)p;
        char recv_buffer[2048];
        uint8_t type_FROM_server;
        int conn_width = getmaxx(connections_display);
        int play_width2 = getmaxx(player_stat);
        while(1){
                if(recv(skt, &type_FROM_server, 1, 0) < 1)
                        break;
                //printf(recv_buffer);
                if(type_FROM_server == 1)
                {
                        pthread_mutex_lock(&mutex);
                        message.type = 1;
                        recv(skt, &message.message_length, 2, MSG_WAITALL);
                        recv(skt, &message.receiver, 32, MSG_WAITALL);
                        recv(skt, &message.sender, 30, MSG_WAITALL);
                        recv(skt, &message.lastbyte, 1, MSG_WAITALL);
                        recv(skt, &message.lastbyte2, 1, MSG_WAITALL);
                        recv(skt, &message.message, message.message_length, MSG_WAITALL); 
                        message.message[message.message_length] = '\0';
                        wprintw(new_window, "Type: %u (MESSAGE) \n From: %s \n To: %s \n Message: %s\n\n", message.type, message.sender, message.receiver, message.message);        
                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);

                }
                else if(type_FROM_server == 7)
                {
                        pthread_mutex_lock(&mutex);
                        ER.type = type_FROM_server;
                        read(skt, &ER.code, 1);
                        recv(skt, &ER.message_length, 2, MSG_WAITALL);
                        recv(skt, &ER.message, ER.message_length, MSG_WAITALL);

                        ER.message[ER.message_length] = '\0';


                        wprintw(new_window, "Type: %u (ERROR)\n", ER.type);

                        wrefresh(new_window);
                        wprintw(new_window, "Error Code: %u\n", ER.code);

                        wrefresh(new_window);
                        wprintw(new_window, "Message Length: %lu\n", ER.message_length);  

                        wrefresh(new_window);
                        wprintw(new_window, "Error Message: %s\n", ER.message); //print statements

                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);
                        //wmove(input_window, 1, 20);
                        //wrefresh(input_window);
                }
                else if(type_FROM_server == 8)
                {
                        pthread_mutex_lock(&mutex);
                        uint8_t actType;
                        read(skt, &actType, 1);
                        if(actType == 10)
                                strcpy(savedName, PLAYER.name);

                        /*printf("Type: %u (ACCEPT)\n", type_FROM_server);
                        printf("Type: %u (ACCEPT)\n", type_FROM_server);*/

                        wprintw(new_window, "Type %u (ACCEPT):\n", type_FROM_server);     

                        wrefresh(new_window);
                        wprintw(new_window, "Type of action accepted: %u\n", actType);    

                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);
                }
                else if(type_FROM_server == 9)
                {
                        pthread_mutex_lock(&mutex);
                        ROOM.type = 9;
                        recv(skt, &ROOM.roomNum, 2, MSG_WAITALL);
                        recv(skt, &ROOM.roomName, 32, MSG_WAITALL);
                        recv(skt, &ROOM.length, 2, MSG_WAITALL);
                        recv(skt, &ROOM.room_description, ROOM.length, MSG_WAITALL);      
                        ROOM.room_description[ROOM.length] = '\0';
                        currentRoom = ROOM.roomNum;
                        if(currentRoom != savedRoom)
                        {
                                savedRoom = currentRoom;
                                wclear(connections_display);
                                wrefresh(connections_display);
                                lc = 2;
                                wattron(connections_display, COLOR_PAIR(3));
                                mvwprintw(connections_display, 1, (conn_width/4), "CURRENT ROOM INFORMATION:");
                                wattroff(connections_display, COLOR_PAIR(3));
                                mvwprintw(connections_display, 2, (conn_width/4), "-------------------------");
                                lc++;

                                mvwprintw(connections_display, lc, 2, "[CURRENT ROOM] %lu: %s\n", ROOM.roomNum, ROOM.roomName);
                                lc++;
                                wrefresh(connections_display);
                                if(!connect_vect.empty())
                                {
                                        while(!connect_vect.empty())
                                                connect_vect.pop_back();
                                }
                        }
                        //printf("Type: %u (ROOM)\n Room Number: %lu \n Room Name: %s \n Room Description: %s\n\n", ROOM.type, ROOM.roomNum, ROOM.roomName, ROOM.room_description); 
                        wprintw(new_window, "Type: %u (ROOM)\n Room Number: %lu \n Room Name: %s \n Room Description: %s\n\n", ROOM.type, ROOM.roomNum, ROOM.roomName, ROOM.room_description);


                        //ROOM.room_description[ROOM.length] = '\0';
                        box(connections_display, 0, 0);
                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);
                }
                else if(type_FROM_server == 10)
                {
                        pthread_mutex_lock(&mutex);
                        PLAYER.type = 10;
                        recv(skt, &PLAYER.name, 32, MSG_WAITALL);
                        recv(skt, &PLAYER.flags , 1, MSG_WAITALL);
                        recv(skt, &PLAYER.attack, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.defense, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.regan, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.health, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.gold, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.room, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.description_length, 2, MSG_WAITALL);
                        recv(skt, &PLAYER.player_description, PLAYER.description_length, MSG_WAITALL);
                        PLAYER.player_description[PLAYER.description_length] = '\0';      
                        if(!strcmp(PLAYER.name, savedName))
                        {
                                werase(player_stat);
                                mvwprintw(player_stat, 1, (play_width2/3.05), "PLAYER STATUS");
                                mvwprintw(player_stat, 2, (play_width2/3.5), "-----------------");
                                mvwprintw(player_stat, 3, 2, "[PLAYER NAME]:  %s", PLAYER.name);
                                mvwprintw(player_stat, 4, 2, "[FLAGS]:  %u", PLAYER.flags);
                                mvwprintw(player_stat, 5, 2, "[ATTACK]:  %lu", PLAYER.attack);
                                mvwprintw(player_stat, 6, 2, "[DEFENSE]:  %lu", PLAYER.defense);
                                mvwprintw(player_stat, 7, 2, "[REGAN]:  %lu", PLAYER.regan);
                                if(PLAYER.health <= 0)
                                        mvwprintw(player_stat, 8, 2, "[HEALTH]:  NONE (you died)");
                                else
                                        mvwprintw(player_stat, 8, 2, "[HEALTH]:  %ld", PLAYER.health);
                                mvwprintw(player_stat, 9, 2, "[GOLD]:  %lu", PLAYER.gold);
                                mvwprintw(player_stat, 10, 2, "[ROOM]:  %lu", PLAYER.room);
                                //mvwprintw(player_stat, 9, 1, "Description: %s", PLAYER.player_description);
                                box(player_stat, 0, 0);
                                wrefresh(player_stat);

                        }

                        wprintw(new_window, "Type: %u (CHARACTER)\n Player Name: %s\n Flags: %u \n Attack: %lu \n Defense %lu \n Regan: %lu \n Health: %d \n Gold %lu \n Room: %lu \n Description: %s \n", PLAYER.type, PLAYER.name, PLAYER.flags, PLAYER.attack, PLAYER.defense, PLAYER.regan, PLAYER.health, PLAYER.gold, PLAYER.room, PLAYER.player_description);    
                        /*wprintw("Player Name:%s\n", PLAYER.name);
                        wprintw("Flags: %s\n", PLAYER.flags);
                        wprintw("Attack: %lu", PLAYER.attack);
                        */
                        if(PLAYER.room == currentRoom && strcmp(savedName, PLAYER.name))  
                        {
                                if(PLAYER.flags == 0xD8 || PLAYER.flags == 0x98 || PLAYER.flags == 0x58 || PLAYER.flags == 0x18 || PLAYER.flags == 0x00)
                                {
                                        if(PLAYER.health <= 0)
                                        {
                                                mvwprintw(connections_display, lc, 2, "[PLAYER]: %s  (HEALTH)-> DEAD", PLAYER.name);
                                                lc++;
                                        }
                                        else
                                        {
                                                mvwprintw(connections_display, lc, 2, "[PLAYER]: %s  (HEALTH)-> %ld", PLAYER.name, PLAYER.health);
                                                lc++;
                                        }
                                }
                                else
                                {
                                        if(PLAYER.health <= 0)
                                        {
                                                wattron(connections_display, COLOR_PAIR(1));
                                                mvwprintw(connections_display, lc, 2, "[PLAYER]: %s  (HEALTH)-> DEAD", PLAYER.name);
                                                wattroff(connections_display, COLOR_PAIR(1));
                                                lc++;
                                        }
                                        else
                                        {
                                                wattron(connections_display, COLOR_PAIR(1));
                                                mvwprintw(connections_display, lc, 2, "[ENEMY]: %s  (HEALTH)-> %ld", PLAYER.name, PLAYER.health);
                                                wattroff(connections_display, COLOR_PAIR(1));
                                                lc++;
                                        }
                                }
                                /*
                                if(PLAYER.flags == 0xB8 || PLAYER.flags == 0xFF){
                                        wattron(connections_display, COLOR_PAIR(1));      
                                        mvwprintw(connections_display, lc, 2, "[ENEMY]: %s\n", PLAYER.name);
                                        wattroff(connections_display, COLOR_PAIR(1));     
                                        lc++;
                                }
                                else if(PLAYER.flags == 0x00){
                                        mvwprintw(connections_display, lc, 2, "[PLAYER - DEAD]: %s\n", PLAYER.name);
                                        lc++;
                                }
                                else{
                                        mvwprintw(connections_display, lc, 2, "[PLAYER]: %s\n", PLAYER.name);
                                        lc++;
                                }*/
                        }
                        box(connections_display, 0, 0);
                        wrefresh(connections_display);
                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);


                }
                else if(type_FROM_server == 11)
                {
                        pthread_mutex_lock(&mutex);
                        GME.type = 11;
                        recv(skt, &GME.initial_points, 2, MSG_WAITALL);
                        recv(skt, &GME.stat_limit, 2, MSG_WAITALL);
                        recv(skt, &GME.description_length, 2, MSG_WAITALL);
                        recv(skt, &GME_description, GME.description_length, MSG_WAITALL); 
                        GME_description[GME.description_length] = '\0';
                        wprintw(new_window, "Type: %u (GAME)\n", GME.type);
                        wprintw(new_window, "Initial Points: %lu\n", GME.initial_points); 
                        wprintw(new_window, "Stat Limit: %lu\n", GME.stat_limit);
                        wprintw(new_window, "Game Description: %s\n", GME_description);   
                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);


                }
                else if(type_FROM_server == 13)
                {
                        pthread_mutex_lock(&mutex);
                        ROOM.type = 13;
                        recv(skt, &ROOM.roomNum, 2, MSG_WAITALL);
                        recv(skt, &ROOM.roomName, 32, MSG_WAITALL);
                        recv(skt, &ROOM.length, 2, MSG_WAITALL);
                        recv(skt, &ROOM.room_description, ROOM.length, MSG_WAITALL);      
                        ROOM.room_description[ROOM.length] = '\0';
                        found = 0;
                        if(!connect_vect.empty())
                        {
                                for(int i = 0; i < sizeof(connect_vect); i++)
                                {
                                        if(connect_vect[i].roomNum == ROOM.roomNum)       
                                                found = 1;
                                }
                        }
                        if(found == 0)
                                connect_vect.push_back(ROOM);

                        /*if(currentRoom != savedRoom)
                        {
                                savedRoom = currentRoom;
                                wclear(connections_display);
                                wprintw(connections_display, "Current Connections: \n");  
                                wrefresh(connections_display);
                        }*/
                        //printf("Type: %u (CONNECTION)\n Room Number: %lu \n Room Name: %s \n Room Description: %s\n\n", ROOM.type, ROOM.roomNum, ROOM.roomName, ROOM.room_description);
                        wprintw(new_window, "Type: %u (CONNECTION)\n", ROOM.type);        

                        wrefresh(new_window);
                        wprintw(new_window, "Room Number: %lu\n", ROOM.roomNum);

                        wrefresh(new_window);
                        wprintw(new_window, "Room Name: %s\n", ROOM.roomName);

                        wrefresh(new_window);
                        wprintw(new_window, "Room Description: %s\n", ROOM.room_description);
                        if(found == 0)
                        {
                                mvwprintw(connections_display, lc, 2, "[CONNECTION]  %lu: %s\n", ROOM.roomNum, ROOM.roomName);
                                lc++;
                                box(connections_display, 0, 0);
                                wrefresh(connections_display);
                        }
                        //should we add another new line?


                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);
                }
                else if(type_FROM_server == 14)
                {
                        pthread_mutex_lock(&mutex);
                        VRS.type = 14;
                        read(skt, &VRS.major, 1);
                        read(skt, &VRS.minor, 1);
                        recv(skt, &VRS.extensions, 2, MSG_WAITALL);
                        wprintw(new_window, "Type: %u (VERSION)\n", VRS.type);
                        wprintw(new_window, "Major: %u\n", VRS.major);
                        wprintw(new_window, "Minor: %u\n", VRS.minor);
                        wprintw(new_window, "Extensions: %lu\n", VRS.extensions);
                        wrefresh(new_window);
                        pthread_mutex_unlock(&mutex);
                }
                else{
                        //printf("Received type: %u. This is not in protocol\n\n", type_FROM_server);
                        /*wprintw(new_window, "Received type: %u. This is not in protocol\n\n", type_FROM_server);
                        wrefresh(new_window);*/
                        continue;
                }

                //first wait on if a type comes through recv
                //types we can receive:
                //1- the server sends us some sort of message (narration or otherwise     
                //3?
                //7 - error
                //8- accept
                //9- room
                //10-character
                //11/14 (game/version: should we include these up here instead of hard coding it down in main (prob)
                //13- connection
        }
        //printf("recv_thread terminating, probable disconnect\n");
        wattron(new_window, COLOR_PAIR(1));
        wprintw(new_window, "CONNECTION TERMINATED");
        wattroff(new_window, COLOR_PAIR(1));
        wrefresh(new_window);
        return 0;
}


int main(int argc, char ** argv){
        //char GME_description[65536]; //look at protocol to see the maximum that can accepted for game description
        //uint8_t type;
        uint16_t changeRoom;
        char PVP[32];
        char loot[32];

        if(argc < 3){
                printf("Usage:  %s hostname port\n", argv[0]);
                return 1;
        }
        struct sockaddr_in sad;
        sad.sin_port = htons(atoi(argv[2]));
        sad.sin_family = AF_INET;

        int skt = socket(AF_INET, SOCK_STREAM, 0);

        // do a dns lookup
        struct hostent* entry = gethostbyname(argv[1]);
        if(!entry){
                if(h_errno == HOST_NOT_FOUND){
                        printf("This is our own message that says the host wasn't found\n");
                }
                herror("gethostbyname");
                return 1;
        }
        struct in_addr **addr_list = (struct in_addr**)entry->h_addr_list;
        struct in_addr* c_addr = addr_list[0];
        char* ip_string = inet_ntoa(*c_addr);
        sad.sin_addr = *c_addr; // copy the address we found into sad
        // Finally done with DNS!
        printf("Connecting to:  %s\n", ip_string);

        if( connect(skt, (struct sockaddr*)&sad, sizeof(struct sockaddr_in)) ){
                perror("connect");
                return 1;
        }
        /*pthread_t recv_thread_handle;
        pthread_create(&recv_thread_handle, 0, recv_thread, &skt);*/
        //curses setup
        initscr();
        signal(SIGWINCH, handle_resize); // to detect signals
        //WINDOW *new_window;
        getmaxyx(stdscr, maxy, maxx);
        //color
        start_color();
        init_pair(1, COLOR_RED, COLOR_BLACK);
        init_pair(2, COLOR_CYAN, COLOR_BLACK);
        init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(4, COLOR_GREEN, COLOR_BLACK);

        //windows
        new_window= newwin(maxy, (maxx/2)-1, 0, 0);
        input_window = newwin((maxy/2.5), (maxx/3.5), ((maxy/1.75)+1), (maxx/2)+2);       
        menu_options = newwin((maxy/2.5), (maxx/5.5), ((maxy/1.75)+1), (maxx/1.25)+2);    
        connections_display = newwin((maxy/1.75), (maxx/3.5), 0, (maxx/2)+2);
        player_stat = newwin(maxy/1.75, (maxx/5.5), 0, (maxx/1.25)+2);

        int connection_width = getmaxx(connections_display);
        int play_width = getmaxx(player_stat);

        box(input_window, 0, 0);
        box(menu_options, 0, 0);
        box(player_stat, 0, 0);
        box(connections_display, 0, 0);

        wattron(connections_display, COLOR_PAIR(3));
        mvwprintw(connections_display, 1, (connection_width/4), "CURRENT ROOM INFORMATION:");
        wattroff(connections_display, COLOR_PAIR(2));
        mvwprintw(connections_display, 2, (connection_width/4), "-------------------------");
        mvwprintw(player_stat, 1, (play_width/3.05), "PLAYER STATUS");
        mvwprintw(player_stat, 2, (play_width/3.5), "-----------------");

        wrefresh(new_window);
        wrefresh(input_window);
        wrefresh(menu_options);
        wrefresh(connections_display);
        wrefresh(player_stat);

        print_menu();

        scrollok(new_window, true); //need a window for this to work
        cbreak();

        /*recv(skt, &VRS, 5, MSG_WAITALL); //should we add if and then do some if not 5?  
        //printf("Type: %u (VERSION)\n Major: %u\n Minor: %u\n Extensions: %lu\n\n\n", VRS.type, VRS.major, VRS.minor, VRS.extensions);
        wprintw(new_window, "Type: %u (VERSION)\n", VRS.type);
        wprintw(new_window, "Major: %u\n", VRS.major);
        wprintw(new_window, "Minor: %u\n", VRS.minor);
        wprintw(new_window, "Extensions: %lu\n", VRS.extensions);


        wrefresh(new_window);
        recv(skt, &GME, 7, MSG_WAITALL);
        recv(skt, &GME_description, GME.description_length, MSG_WAITALL);
        if(GME.initial_points == 0)
                GME.initial_points = 65535;

        GME_description[GME.description_length] = '\0';

        wprintw(new_window, "Type: %u(GAME)\n", GME.type);
        wprintw(new_window, "Initial Points: %lu\n", GME.initial_points);
        wprintw(new_window, "Stat Limit: %lu\n", GME.stat_limit);
        wprintw(new_window, "Description: %s\n\n", GME_description);

        wrefresh(new_window);
*/
        pthread_t recv_thread_handle;
        pthread_create(&recv_thread_handle, 0, recv_thread, &skt);
        //printf("Welcome! Please enjoy our 'type' menu. Press 123 to view it again at any time!\n\n");
        //print_menu();
        while(1){
                //next step is adding windows. I think that it would be helpful to have one input window (bottom half) one that references menu (part of bottom) and then server output (top half)

                char nbuffer[65536];
                mvwprintw(input_window, 1, 2, "Enter a type:                   ");        
                //wmove(input_window, 2, 20);
                mvwgetnstr(input_window, 1, 20, nbuffer, 3);
                uint8_t type = atoi(nbuffer);
                //printw("%d", type);
                if(type == 1)
                {
                        message.type = 1;
                        message.lastbyte = 0;
                        message.lastbyte2 = 0;
                        //nbuffer = printw("Enter the message length:");
                        /*printw("Enter the message length: ");
                        refresh();
                        getnstr(nbuffer, sizeof(nbuffer-1));
                        if(nbuffer[0] == '\n')
                                message.message_length = 0;
                        else
                                message.message_length = atoi(nbuffer);*/
                        //free(nbuffer);

                        mvwprintw(input_window, 3, 1, "Enter message receipient: ");    //strncpy for limited copy
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 3, 30, nbuffer, 32);
                        if(nbuffer[0] == '\n')
                                memset(message.receiver, '\0', sizeof(message.receiver)); 
                        else
                                strncpy(message.receiver, nbuffer, 32);
                        //free(nbuffer);

                        mvwprintw(input_window, 4, 1, "Enter message sender: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 4, 30, nbuffer, 30);
                        if(nbuffer[0] == '\n')
                                memset(message.sender, '\0', sizeof(message.sender));     
                        else
                                strncpy(message.sender, nbuffer, 30);
                        //free(nbuffer);
                        mvwprintw(input_window, 5, 1, "Enter message: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 5, 25, nbuffer, 65536);//change to long message

                        if(nbuffer[0] == '\n')
                                memset(message.message, '\0', sizeof(message.message));   
                        else
                                strcpy(message.message, nbuffer);
                        //free(nbuffer);
                        message.message_length = strlen(message.message);
                        write(skt, &message, 67+message.message_length);

                }
                else if(type == 2)
                {

                        mvwprintw(input_window, 2, 1, "Room to switch to: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 2, 30, nbuffer, 4);
                        if(nbuffer[0] == '\n')
                                changeRoom = 0;
                        else
                                changeRoom = atoi(nbuffer);
                        //free(nbuffer);
                        write(skt, &type, 1);
                        write(skt, &changeRoom, 2);

                }
                else if(type == 3)
                {
                        fightCalled = 1;
                        write(skt, &type, 1);
                }
                else if(type == 4){
                        mvwprintw(input_window, 2, 1, "Name of player you wish to fight: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 2, 35, nbuffer, 32);
                        if(nbuffer[0] == '\n')
                                memset(PVP, '\0', sizeof(PVP));
                        else
                                strncpy(PVP, nbuffer, 32);
                        //free(nbuffer);
                        write(skt, &type, 1);
                        write(skt, &PVP, 32);
                }
                else if(type == 5)
                {
                        mvwprintw(input_window, 2, 1, "Name of player you wish to loot: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 2, 35, nbuffer, 32);
                        if(nbuffer[0] == '\n')
                                memset(loot, '\0', sizeof(loot));
                        else
                                strncpy(loot, nbuffer, 32);
                        //free(nbuffer);
                        write(skt, &type, 1);
                        write(skt, &loot, 32);
                }
                else if(type == 6)
                        write(skt, &type, 1);
                else if(type == 10)
                {
                        PLAYER.type = 10;
                        mvwprintw(input_window, 2, 1, "Enter your name: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 2, 30, nbuffer, 32);
                        if(nbuffer[0] == '\n')
                                memset(PLAYER.name, '\0', sizeof(PLAYER.name));
                        else
                                strncpy(PLAYER.name, nbuffer, 32);
                        //free(nbuffer);


                        mvwprintw(input_window, 3, 1, "Enter your flags: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 3, 25, nbuffer, 4);
                        if(nbuffer[0] == '\n')
                                PLAYER.flags = 0x00;
                        else
                                PLAYER.flags = atoi(nbuffer);
                        //free(nbuffer);

                        mvwprintw(input_window, 4, 1, "Attack: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 4, 15, nbuffer, 4);
                        if(nbuffer[0] == '\n')
                                PLAYER.attack = 0;
                        else
                                PLAYER.attack = atoi(nbuffer);
                        //free(nbuffer);

                        mvwprintw(input_window, 5, 1, "Defense: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 5, 15, nbuffer, 4);
                        if(nbuffer[0] == '\n')
                                PLAYER.defense = 0;
                        else
                                PLAYER.defense = atoi(nbuffer);
                        //free(nbuffer);


                        mvwprintw(input_window, 6, 1, "Regan: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 6, 15, nbuffer, 4);
                        if(nbuffer[0] == '\n')
                                PLAYER.regan = 0;
                        else
                                PLAYER.regan = atoi(nbuffer);
                        //free(nbuffer);

                        /*nbuffer = printw("Health:");
                        if(nbuffer[0] == '\n')
                                PLAYER.health = 0;
                        else
                                PLAYER.health = atoi(nbuffer);
                        free(nbuffer);

                        nbuffer = printw("Gold:");
                        if(nbuffer[0] == '\n')
                                PLAYER.gold = 0;
                        else
                                PLAYER.gold = atoi(nbuffer);
                        free(nbuffer);

                        nbuffer = printw("Room:");
                        if(nbuffer[0] == '\n')
                                PLAYER.room = 0;
                        else
                                PLAYER.room = atoi(nbuffer);
                        free(nbuffer);*/
                        PLAYER.health = 100;
                        PLAYER.gold = 0;
                        PLAYER.room = 0;

                        /*nbuffer = printw("Description Length:");
                        if(nbuffer[0] == '\n')
                                PLAYER.description_length = 0;
                        else
                                PLAYER.description_length = atoi(nbuffer);
                        free(nbuffer);*/

                        mvwprintw(input_window, 7, 1, "Player description: ");
                        wrefresh(input_window);
                        mvwgetnstr(input_window, 7, 25, nbuffer, 1000); //what should the player description actually be?
                        if(nbuffer[0] == '\n')
                                memset(PLAYER.player_description, '\0', sizeof(PLAYER.player_description));
                        else
                                strcpy(PLAYER.player_description, nbuffer);
                        //free(nbuffer);
                        PLAYER.description_length = strlen(PLAYER.player_description);    
                        write(skt, &PLAYER, 48+strlen(PLAYER.player_description));        
                }
                else if(type == 12)
                {
                        endwin();
                        exit(0);
                }
                else
                        continue;
                werase(input_window);
                box(input_window, 0, 0);
                wrefresh(input_window);

        }
        endwin();
        close(skt);
        return 0;
}

void print_menu(){

        //mvwprintw(menu_options, 1, 0, "~*~*~*~*~*~*~*~*~*~*~*~*~\n");
        int act_width = getmaxx(menu_options);
        mvwprintw(menu_options, 1, (act_width/3), "ACTIONS MENU");
        mvwprintw(menu_options, 2, (act_width/4), "------------------");
        mvwprintw(menu_options, 3, 5, "[1]:  Send Message");
        mvwprintw(menu_options, 4, 5, "[2]:  Change Room");
        mvwprintw(menu_options, 5, 5, "[3]:  Fight");
        mvwprintw(menu_options, 6, 5, "[4]:  PVP Fight");
        mvwprintw(menu_options, 7, 5, "[5]:  Loot");
        mvwprintw(menu_options, 8, 5, "[6]:  Start");
        mvwprintw(menu_options, 9, 5, "[10]: Make Character");
        mvwprintw(menu_options, 10, 5, "[12]: Leave Game");
        wrefresh(menu_options);
        /*
        printf("-----------------------\n");
        printf("33: View Menu Again \n");
        printf("-----------------------\n\n\n");*/

        return;
}
void handle_resize(int sig)
{
        /*endwin();
        refresh();
        clear();
        getmaxyx(stdscr, maxy, maxx);
        WINDOW *new_window;
        getmaxyx(stdscr, maxy, maxx);
        new_window= newwin((maxy/2), maxx, 0, 0);
        box(new_window, 0, 0);
        wrefresh(new_window);
        scrollok(new_window, true); //need a window for this to work
        cbreak();*/
        /*getmaxyx(stdscr, maxy, maxx);
        wresize(new_window, maxy, maxx);
        wrefresh(new_window);*/
        getmaxyx(stdscr, maxy, maxx);
        wresize(new_window, maxy, maxx);
        //resizeterm(maxy, maxx);
        //box(new_window, 0, 0);
        refresh();
        wrefresh(new_window);
        //refresh();

}