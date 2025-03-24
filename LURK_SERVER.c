#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include<stdlib.h>
#include<thread>
#include<vector>
#include<string.h>
#include <iostream>
#include <mutex>

using namespace std;

#define BUFSIZE 1024

struct client {
	char name[32];
	int fd;
	client(char *n, int f) : fd(f) {
		strcpy(name, n);
	}
};

struct clientsInRoom
{
	char name[32];
};

struct character {
	uint8_t type;
	char c_name[32];
	uint8_t flags;
	uint16_t attack, defense, regan;
	int16_t health;
	uint16_t gold, room, description_length;
	char player_description[1024];
} __attribute__((packed)); // for type 10

vector<struct client> clients;
vector<struct character> monster_characters; //to push all characteres into, including monsters and such. the room number for the monsters or other npcs should be the room that they live in
vector<struct character> server_characters;
vector<struct character> dead_characters;

struct game {
	uint8_t type;
	uint16_t initial_points, stat_limit, description_length;
	char description[371];
} __attribute__((packed)); // for type 11

struct version {
	uint8_t type, major, minor;
	uint16_t extensions;
} __attribute__((packed)); //for type 14

struct room {
	uint8_t type;
	uint16_t roomNumber;
	char roomName[32];
	uint16_t roomDescriptionLength;
	char roomDescription[275];
	//vector<int>connections;
}__attribute__((packed));




struct game our_game = {11, 100, 500, 0, "Imagine, you awake from a slumber to find yourself trapped in a cage, riddled with restraints. There is no mistaking, you are a Pokemon and you have been captured by the notorious Team Plasma. But they were fools. Using the attack of your choice, you effortlessly break free. Your mission now? To escape this facility and to DESTROY Team Plasma for dare messing with you"};
struct version leahs_version = {14, 2, 3, 0};

		
uint8_t not_named_error[] = "    Must send a character before sending messages.  You can't talk unless you have a name";

struct error1 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[36] = "You aren't connected to that room.";
} __attribute__((packed));

struct error2 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[46] = "There is already a player of this type.";
} __attribute__((packed));


struct error3 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[46] = "You cannot loot at this time.";
} __attribute__((packed));

struct error4 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[101] = "The stats you have set are inappropriate. Likely you have overused initial points. Please try again.";
} __attribute__((packed));


struct error5 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[46];
} __attribute__((packed));


struct error6 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[61];
} __attribute__((packed));


struct error7 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[46];
} __attribute__((packed));


struct error8 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[46];
} __attribute__((packed));

struct error0 {
	uint8_t type, errorcode;
	uint16_t desclen;
	char error_message[46];
} __attribute__((packed));

struct error0 badRead = {7, 0, 22, "We read a bad amount."};
struct error1 badRoom = {7, 1, 36, "You aren't connected to that room."};
struct error2 playerExists = {7, 2, 28, "This player already exists!"};
struct error3 badMonster;
struct error4 badStat = {7, 4, 101, "The stats you have set are inappropriate. Likely you have overused initial points. Please try again."};
struct error5 notReady = {7, 5, 41, "You are not ready to perform this action."};
struct error6 noTarget = {7, 6, 59, "The player does not exist or they have nothing else to loot."};
struct error7 noFight = {7, 7, 31, "There are no monsters to fight."};
struct error8 noPvp = {7, 8, 33, "The server does not support PVP"};

struct accept
{
	uint8_t type;
	uint8_t action;
}__attribute__((packed));


//connections of each room on the map
vector<int> roomOne_connections = {2, 3};
vector<int> roomTwo_connections = {1, 3, 4};
vector<int> roomThree_connections = {1, 2, 5};
vector<int> roomFour_connections = {2, 8};
vector<int> roomFive_connections = {3, 6, 7, 8};
vector<int> roomSix_connections = {5};
vector<int> roomSeven_connections = {5};
vector<int> roomEight_connections = {4, 5, 9};
vector<int> roomNine_connections = {8, 10};
vector<int> roomTen_connections = {9}; //room where the final boss fight occurs
vector<vector<int>> arrayOfRooms;

//intializing rooms and their properties based on the previously defined 'room' structure
vector<struct room> leahs_rooms = 
{
	{9, 1, "Confinement center", 111, "All around you there are pokemon locked away. Some seem angry, others seem to be doped up on some sort of drug."},
	{9, 2, "Barrock", 91, "The walls are lined with pokeballs, nets, and other gadgets. You feel angered at the sight."},
	{9, 3, "Laboratory", 185, "Before you lie an array of medical tables, syringes, and monitors. Against the walls there are three cylinder tubes, 10 feet high and filed with liquid. Are these to keep other pokemon?"},
	{9, 4, "Battle zone", 154, "You see four battle courts reminicent of a pokemon gym or pokemon arena. It seems to be where the grunts come to train their pokemon slaves to perfection."},
	{9, 5, "Grunt dorms", 146, "It seems to be a military style doorm room. Columns of bunk beds fill the spaace. You wonder if they even realize the mission they are supporting."},
	{9, 6, "Ghetisis' lair", 263, "A large marble walkway lies before you. At its head, it forms into a large shape. A set of stairs lead to a window, 50 feet in diameter which overlooks what lies below. Thankfully for him, he doesn't seem to be around. Otherwise, you'd surely make today his last."},
	{9, 7, "Server room", 213, "The heat and buzzing in the room is almost unbearable from the servers, which seem to be place in a haphazard fashion. You wonder what all this equipment could possbily be needed for. It couldn't be anything good."},
	{9, 8, "Grand hall", 156, "Several dining tables fill the space. To the side is what can only be described as a prison-style lunch line where the grunts would receive their pity meals"},
	{9, 9, "Passing hall", 110, "A hally which seems to lead somewhere promising. The walls are decorated with torches and Team Plasma banners."},
	{9, 10, "Main coridor", 143, "A castle-esk coridor lies before you. Straight ahead are two giant doors, probably 40 feet in height. Freedom and revenge lie just meters away."}
};

//initializing game npcs
struct character sedated_pokemon = {10, "Pickachu- sedated", 0x98, 10, 0, 0, 20, 0, 3, 84, "It's a Pikachu. It is heavily sedated and seemingly lifeless on the medical table."};
struct character lab_tech = {10, "Team Plasma Scientist", 0xB8, 20, 0, 0, 100, 50, 3, 96, "A team plasma tech who is getting ready to experiment on Pikachu. He is causing your kind harm."};
struct character grunt_1 = {10, "Team Plasma Grunt Aaron", 0xB8, 50, 10, 0, 100, 100, 4, 75, "A grunt who is getting ready for a pokemon battle against his fellow grunt."};
struct character grunt_2 = {10, "Team Plasma Grunt Ella", 0xB8, 50, 10, 0, 100, 100, 4, 75, "A grunt who is getting ready for a pokemon battle against her fellow grunt."};
struct character grunt_3 = {10, "Team Palsma Grunt JJ", 0xB8, 50, 40, 10, 100, 300, 8, 74, "A team plasma grunt. He is enjoying his meal, and his Muk is close nearby."};//grand hall- room 8
struct character Muk = {10, "Team Plasma Muk", 0xB8, 50, 50, 0, 100, 500, 8, 86, "A Muk. This poison type pokemon is reminicent of sludge. Even a touch could be brutal."}; 
struct character sleeping_grunt = {10, "Grunt - Sleeping", 0xB8, 0, 0, 20, 100, 100, 5, 93, "A team plasma grunt lies tucked into bed nicely. The life of a villian surely must be tiring."};
struct character joltik = {10, "Joltik", 0xB8, 30, 0, 10, 100, 50, 7, 86, "You notice a joltik darting up and down the server columns. Its many eyes distrub you."};
struct character chandelure = {10, "Chandelure", 0xB8, 60, 20, 20, 100, 250, 9, 204, "As you walk through the coridor, you notice what seems to be a chandeleer hanging from the ceiling. But upon a closer look, you see it is a chandelure, the fire/ghost pokemon! Wouldn't want to anger it..."};
struct character yamask = {10, "Yamask", 0x98, 60, 10, 30, 100, 0, 9, 143, "A yamask appears next to you. It doesn't seem to be violent, instead it seems to be trying to say something, as if to show you the way forward."};
struct character ghetisis {10, "Ghetisis", 0xB8, 70, 20, 10, 100, 10000, 10, 171, "Ghetisis himself. He wears a smirk on his face and in his eyes you can see nothing but evil. It's time for him to be held accountable for his actions. You know what to do."};

//maybe we should put this by the other structure definitions to make the code cleaner?
struct Message {
	uint8_t type; 
	uint16_t message_length; 
	char receiver[32];
	char sender[30];
	uint8_t endSend; // 0 for message
	uint8_t endSendAgain; // 0 for message, 1 for narration
	char message[65536];
}__attribute__((packed));

struct Message message;

void init(){
	our_game.description_length = strlen(our_game.description);

	not_named_error[0] = 7;
	not_named_error[1] = 0;
	// One way (difficult to expand since you have to think about byte order yourself
	not_named_error[2] = strlen((char*)not_named_error + 4);
	not_named_error[3] = 0;
	// Another way, but you have to do some casting
	*((uint16_t*)(not_named_error + 2)) = strlen((char*)not_named_error + 4);
	for(int i = 0; i < leahs_rooms.size(); i++)
	{
		leahs_rooms[i].roomDescriptionLength = strlen(leahs_rooms[i].roomDescription);
	}
	arrayOfRooms.push_back(roomOne_connections);
	arrayOfRooms.push_back(roomTwo_connections);
	arrayOfRooms.push_back(roomThree_connections);
	arrayOfRooms.push_back(roomFour_connections);
	arrayOfRooms.push_back(roomFive_connections);
	arrayOfRooms.push_back(roomSix_connections);
	arrayOfRooms.push_back(roomSeven_connections);
	arrayOfRooms.push_back(roomEight_connections);
	arrayOfRooms.push_back(roomNine_connections);
	arrayOfRooms.push_back(roomTen_connections);


	monster_characters.push_back(sedated_pokemon);
	monster_characters.push_back(lab_tech);
	monster_characters.push_back(grunt_1);
	monster_characters.push_back(grunt_2);
	monster_characters.push_back(grunt_3);
	monster_characters.push_back(Muk);
	monster_characters.push_back(sleeping_grunt);
	monster_characters.push_back(joltik);
	monster_characters.push_back(chandelure);
	monster_characters.push_back(yamask);
	monster_characters.push_back(ghetisis);

	for(int i = 0; i < monster_characters.size(); i++)
		monster_characters[i].description_length = strlen(monster_characters[i].player_description);

}





mutex STFU;

void client_thread(int client_fd){ //maybe its ok to have all the calls to the types in here?

	ssize_t readlen;
	uint8_t type, connectionFound;
	uint16_t changeRoomCall, current_room, current_room_starting, room, roomLeaving;
	char narrator[32] = "Glowing Orb";
	char savedName[32];
	char buffer[1024];
	int new_index = 0;
	int alive_monster_count, started =0;
	struct accept actionAccepted; //type 8
	char lootTarget[32];
	struct Message NARRATION; //local message structure that will be used exculsively for narration. 
	
	struct character new_character = {10, "", 0, 0, 0, 0, 0, 0, 0, 0, ""};  //type 10 initializer



	printf("Starting thread for client_fd %d\n", client_fd);
	//printf("CURRENT NAME : %s\n", savedName);
	/* Send a game message */
	if(write(client_fd, &our_game, 7) != 7) //we might not want this here. because now its a part of a while loop
		goto exit_and_close;
	if(write(client_fd, our_game.description, our_game.description_length) != our_game.description_length)
		goto exit_and_close;
	while(1){
		printf("CLIENT VECT SIZE:%zu\n", clients.size());
		printf("SERVER CHARACTERS:%zu\n", server_characters.size());
		printf("DEAD CHARACTERS:%zu\n", dead_characters.size());
		int found = 0;
		if(read(client_fd, &type, 1) != 1)
		{
			printf("When reading the type.. we did not read exactly one byte...\n");
			goto exit_and_close; //should we change to continue?
		}
		if(type != 1 && type != 2 && type != 3 && type != 4 && type != 5 && type != 6 && type != 10 && type != 12)
			continue;
		
		if(type == 1){ //indicating that the client wants to send an in game message
			message.type = 1;	

			if(recv(client_fd, &message.message_length, 2, MSG_WAITALL) != 2)
			{
				write(client_fd, &badRead, 26);
				continue;
			}
			if(recv(client_fd, &message.receiver, 32, MSG_WAITALL) != 32)
			{
				write(client_fd, &badRead, 26);
				continue;
			}
			if(recv(client_fd, &message.sender, 30, MSG_WAITALL) != 30)
			{
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &message.endSend, 1, MSG_WAITALL) != 1)
			{
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &message.endSendAgain, 1, MSG_WAITALL) != 1)
			{
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &message.message, message.message_length, MSG_WAITALL) != message.message_length)

			{
				int readlen = strlen(message.message);
				while(readlen < message.message_length) {
					printf("Message not fully received, waiting for more!\n");
					ssize_t new_data = read(client_fd, message.message + 1 + readlen, sizeof(message.message) - 1 - readlen);
					//ssize_t new_data = read(client_fd, message_buffer + 1 + readlen, sizeof(message_buffer) - 1 - readlen);
					if(new_data < 1)
						goto exit_and_close;	
					readlen += new_data;
				}
				if(readlen > message.message_length)
					printf("We read more than we expected - was there a second message we're ignoring?\n");
					//message[*message_length] = 0;
				
			}
			message.message_length = strlen(message.message);
			printf("%lu", message.message_length);

			//printf("MESSAGE : %s\n", message.message);
			STFU.lock();	
			if(!strlen(message.receiver)) { // send to everybody
				for(auto &c : clients) 
					if(c.fd != client_fd)
						write(c.fd, &message, 67 + message.message_length); 
			} 
			else {
				for(auto &c : clients) 
					if(!strcmp(c.name, message.receiver)) // now this probably needs changed
						write(c.fd, &message, 67 + message.message_length); // we probably need to change c.fd (actually never find, that is used for our threads
			}
			STFU.unlock();
		}

		
		else if(type == 2) //sent by client
		{

			if(recv(client_fd, &changeRoomCall, 2, MSG_WAITALL) != 2)
			{
				write(client_fd, &badRead, 26);
				continue;
			}

			if((new_character.flags != 0xD8 && new_character.flags != 0x98) || started == 0)
			{
				write(client_fd, &notReady, 45);
				continue;
			}

			connectionFound = 0;

			for(int i = 0; i < server_characters.size(); i++)
			{
				if(!strcmp(savedName, server_characters[i].c_name)){
					new_index = i;
					printf("The character that is trying to change rooms : %s\n", server_characters[new_index]);
				}
			}
			current_room_starting = server_characters[new_index].room; //whatever room the character is currently in
			if(changeRoomCall > 10)
				printf("That room is altogether invalid!\n");
			else
			{
				for(int i = 0; i < 1; i++) //starting would be 0 if character in room 1, end at 1
				{
					for(int j = 0; j < arrayOfRooms[current_room_starting-1].size(); j++) //index 0, that would be where room 1 connections are
					{
						//printf("%d", arrayOfRooms[i][j]);
						//printf("Looking for connections in room %d. Connection found : %d\n", current_room_starting, arrayOfRooms[current_room_starting-1][j]);
						if(changeRoomCall == arrayOfRooms[current_room_starting-1][j])
						{
							connectionFound = 1;
							printf("we found a connection!\n");
						}
						//printf("Connection found result %u", connectionFound);
					}
					if(connectionFound == 1) // i think that my way of keeping track of the current character is a bit defunct
					{
						roomLeaving = server_characters[new_index].room;
						server_characters[new_index].room = changeRoomCall; 
						room = changeRoomCall; 
						send(client_fd, &leahs_rooms[changeRoomCall -1], 37+strlen(leahs_rooms[changeRoomCall-1].roomDescription), MSG_WAITALL);//new room
						send(client_fd, &server_characters[new_index], 48+strlen(server_characters[new_index].player_description), MSG_WAITALL);
						//we sent our client the new room and their updated character
						//now it appears we are looking through to see if there are any monsters in the room. if so, send to OUR client
						for(int i = 0; i < monster_characters.size(); i++)
						{
							if(monster_characters[i].room == changeRoomCall)
							{
									send(client_fd, &monster_characters[i], 48+strlen(monster_characters[i].player_description), MSG_WAITALL);
									
							}
						}
						//sending OUR CLIENT the other non NPCS
						for(int i = 0; i < server_characters.size(); i++)
						{
							if(server_characters[i].room == changeRoomCall && (strcmp(server_characters[i].c_name, server_characters[new_index].c_name)!=0))
								write(client_fd, &server_characters[i], 48+strlen(server_characters[i].player_description));
						}
						//send any dead characters to OUR client
						if(dead_characters.size() > 0) //if there are any that is
						{
							for(int i = 0; i < dead_characters.size(); i++)
							{
								if(dead_characters[i].room == changeRoomCall)
									write(client_fd, &dead_characters[i], 48+strlen(dead_characters[i].player_description));
							}
						}
						//sending connections to OUR client
						for(int x = 0; x < arrayOfRooms[changeRoomCall-1].size(); x++) //connections
						{
							current_room = arrayOfRooms[changeRoomCall-1][x]; //current connection
							leahs_rooms[current_room-1].type = 13;
							send(client_fd, &leahs_rooms[current_room-1], 37+strlen(leahs_rooms[current_room-1].roomDescription), MSG_WAITALL);
							leahs_rooms[current_room-1].type = 9;

						}
						//send to other characters that we left, and then send to characters in new room that we have arrived. 
						for(int i = 0; i < server_characters.size(); i++) //seeing the names we need to send to in the room we are leaving
						{
							if(server_characters[i].room == roomLeaving) //seeing if the character is in the room we are leaving
								for(int j = 0; j < clients.size(); j++) //they are, now looking for their file descritpor
								{
									if(!strcmp(server_characters[i].c_name, clients[j].name))
										send(clients[j].fd, &server_characters[new_index], 48+strlen(server_characters[new_index].player_description), MSG_WAITALL);
								}
						}
						//send to any existing players in new room, letting them know of our arrival

						for(int i = 0; i < server_characters.size(); i++) //seeing the names we need to send to in the room we entering
						{
							if(server_characters[i].room == room)
								for(int j = 0; j < clients.size(); j++)
								{
									if(!strcmp(server_characters[i].c_name, clients[j].name) && strcmp(server_characters[new_index].c_name, server_characters[i].c_name))
										send(clients[j].fd, &server_characters[new_index], 48+strlen(server_characters[new_index].player_description), MSG_WAITALL);
								}
						}
					}
					else
						send(client_fd, &badRoom, 40, MSG_WAITALL);
				}
			}
		}
		else if(type == 3) //sent by client
		{
			int currentINDEX;
			int won = 0;
			int assist = 0;
			int tooClose = 100;
			if((new_character.flags != 0x98 && new_character.flags != 0xD8) || started == 0)
			{
				write(client_fd, &notReady, 45);
				continue;
			}
			int died = 0;
			alive_monster_count = 0;
			for(int i = 0; i < monster_characters.size(); i++)
			{
				if(monster_characters[i].room == room && monster_characters[i].flags == 0xB8)
					alive_monster_count++;
			}
			if(alive_monster_count == 0)
			{
				write(client_fd, &noFight, 35);
				continue;
			}


			//room = new_character.room;
			STFU.lock();
			vector<struct clientsInRoom> list;
			vector<struct clientsInRoom> clientsJoinBattle;
			struct clientsInRoom currentC, fighters;
			vector<int> otherClientFD;
			vector<int> fighterFD;
			int health_result, assist_index;
			for(int i = 0; i < server_characters.size(); i++) //find out what other actual players are in the room with us (whether they have fight flags or not)
			{
				int result = strcmp(server_characters[i].c_name, new_character.c_name);
				printf("Result:%d", result);
				if(!strcmp(server_characters[i].c_name, new_character.c_name))
					currentINDEX = i; //OUR CHARACTER
				if(server_characters[i].room == room && result !=0)
				{
					printf("Sever Character Room%d", server_characters[i].room);
					printf("Current Room:%d", room);
					strcpy(currentC.name, server_characters[i].c_name); 
					printf("pushing :%s into the vector of current players in room.\n", currentC.name);
					list.push_back(currentC);
				}
				
			}
			//next find their file descriptors from the client vector for sending records to them after the fight has ended.
			for(int i = 0; i < list.size(); i++)
			{
				for(int j = 0; j < clients.size(); j++)
				{
					if(!strcmp(list[i].name, clients[j].name))
							otherClientFD.push_back(clients[j].fd);
				}
			}



			for(int i = 0; i < monster_characters.size(); i++) //this is the fight between our character and the monsters, not the other characters yet
			{
				if(monster_characters[i].flags == 0xB8 && monster_characters[i].room == room)
				{
					printf("time to fight! fighting monster:%s fighter:%s\n", monster_characters[i].c_name, server_characters[currentINDEX].c_name);
					health_result = (server_characters[currentINDEX].health - monster_characters[i].attack) + new_character.defense + new_character.regan; //calc for character
					if(health_result < server_characters[currentINDEX].health)
						server_characters[currentINDEX].health = health_result;
					if(server_characters[currentINDEX].health <=0)
						server_characters[currentINDEX].flags = 0x00;
					health_result= (monster_characters[i].health - new_character.attack) + monster_characters[i].defense + monster_characters[i].regan;//monster calc
					if(health_result < monster_characters[i].health)
						monster_characters[i].health = health_result;
					if(monster_characters[i].health <= 0){
						monster_characters[i].flags = 0x20;
						if(!strcmp(monster_characters[i].c_name, "Ghetisis"))
						{
							won = 1;
							server_characters[currentINDEX].gold = server_characters[currentINDEX].gold + monster_characters[i].gold;
							
						}


					}
					
				}
			}
			for (int i = 0; i < server_characters.size(); i++) //now the other characters in the room that have the flags set to join are fighting too
			{
				if((server_characters[i].flags == 0xD8)&& server_characters[i].room == room && (strcmp(server_characters[i].c_name, new_character.c_name)))
				{
					strcpy(fighters.name, server_characters[i].c_name);
					clientsJoinBattle.push_back(fighters); //saving names of people who joined the battle, and could die
					for(int j = 0; j < monster_characters.size(); j++)
					{
						if(monster_characters[j].flags == 0xB8 && monster_characters[j].room == room)
						{
							printf("Look who's squaring down now:%s\n", server_characters[i].c_name);
							health_result = (server_characters[i].health - monster_characters[j].attack) + server_characters[i].defense + server_characters[i].regan;
							if(health_result < server_characters[i].health)
								server_characters[i].health = health_result;
							if(server_characters[i].health <= 0)
								server_characters[i].flags = 0x00;
								

							health_result = (monster_characters[j].health - server_characters[i].attack) + monster_characters[j].defense + monster_characters[j].regan;
							if(health_result < monster_characters[j].health)
								monster_characters[j].health = health_result;
							if(monster_characters[j].health <= 0)
							{
								monster_characters[j].flags = 0x20;
								if(!strcmp(monster_characters[j].c_name, "Ghetisis"))
								{
									won= 1;
									assist_index = i;
									server_characters[i].gold = server_characters[i].gold + monster_characters[j].gold;
									server_characters[currentINDEX].gold = server_characters[currentINDEX].gold + monster_characters[j].gold;
								}
							}
						}
					} //make consistent with fight above, we will also need to see if any of the other characters died 
				}
			}
			if(monster_characters[10].flags == 0x20)
				monster_characters[10].gold = 0;
			for(int i = 0; i < monster_characters.size(); i++)
			{
				if((monster_characters[i].flags == 0xB8 || monster_characters[i].flags == 0x20) && monster_characters[i].room == room)
				{
					send(client_fd, &monster_characters[i], 48+strlen(monster_characters[i].player_description), MSG_WAITALL); //sends monsters involved to our client
					for(int j = 0; j < otherClientFD.size(); j++)
						send(otherClientFD[j], &monster_characters[i], 48+strlen(monster_characters[i].player_description), MSG_WAITALL); //send to others in room
				}
			}
			//next send other players involved in the fight to our client
			for(int i = 0; i < server_characters.size(); i++)
			{
				if(server_characters[i].room == room && (strcmp(new_character.c_name, server_characters[i].c_name)))
				{
					send(client_fd, &server_characters[i], 48+strlen(server_characters[i].player_description), MSG_WAITALL);
				}
			}
			//now in the vector that has the names of everyone in the room, for each name, send character records of everyone else
			for(int i = 0; i < list.size(); i++) //go through each character we previously determined is in the room. i is the current character in the room
			{	
				for(int j= 0; j< server_characters.size(); j++) //now look through all of our server characters, if they are in the room, we send to each person in list
				{
					if(server_characters[j].room == room) //&& (strcmp(list[i].name, server_characters[j].c_name)))
					{
						send(otherClientFD[i], &server_characters[j], 48+strlen(server_characters[j].player_description), MSG_WAITALL);
					}
				}
			

			}
			for(int i = 0; i < clientsJoinBattle.size(); i++) //finding fighter file descriptors specifically in case they died.
			{
				for(int j = 0; j < clients.size(); j++)
				{
					if(!strcmp(clientsJoinBattle[i].name, clients[j].name))
							fighterFD.push_back(clients[j].fd);
				}
			}
			//we sent all records. now to see if any of the fighters died in battle.
			

			if(server_characters[currentINDEX].flags == 0x00)
			{
				//new_character.flags = 0x00;
				send(client_fd, &server_characters[currentINDEX], 48+strlen(server_characters[currentINDEX].player_description), MSG_WAITALL);
				NARRATION.type = 1; 
				strcpy(NARRATION.receiver, savedName);
				strcpy(NARRATION.sender, narrator);
				NARRATION.endSend = 0;
				NARRATION.endSendAgain = 1;
				strcpy(NARRATION.message,"You fell victim to you injuries. As your eyes fold closed, you can still hear Ghetinsis laughing in the distant. That bastard! One day he will surely pay.");
				NARRATION.message_length = strlen(NARRATION.message);
				send(client_fd, &NARRATION, 67 + NARRATION.message_length, MSG_WAITALL);
				died = 1;
				dead_characters.push_back(server_characters[currentINDEX]);
			}	
			else{
				send(client_fd, &server_characters[currentINDEX], 48+strlen(server_characters[currentINDEX].player_description), MSG_WAITALL);	
				if(won == 1 || assist == 1)
				{
					NARRATION.type = 1; 
					strcpy(NARRATION.receiver, savedName);
					strcpy(NARRATION.sender, narrator);
					NARRATION.endSend = 0;
					NARRATION.endSendAgain = 1;
					strcpy(NARRATION.message,"Ghetisis falls to the ground and lies motionless. Other captive pokemon begin to emerge from the crevices, the doors open and the sun shines down. There is no doubt that this is a great day. Well done. You did it.");
					NARRATION.message_length = strlen(NARRATION.message);
					send(client_fd, &NARRATION, 67 + NARRATION.message_length, MSG_WAITALL);

				}
			}
			for(int i = 0; i < clientsJoinBattle.size(); i++) //look at each fighters name

			{
				for(int j = 0; j < server_characters.size(); j++) //this is for seeing if they have the same name, and then looking at the flags of the corrisponding record
				{
					if(!strcmp(clientsJoinBattle[i].name, server_characters[j].c_name)) //go through and look at each server entry to see if we find the same fighter nam
					{
						if(server_characters[j].flags == 0x00) // we found the same name, are they dead?
						{
							NARRATION.type = 1; 
							strcpy(NARRATION.receiver, clientsJoinBattle[i].name);
							strcpy(NARRATION.sender, narrator);
							NARRATION.endSend = 0;
							NARRATION.endSendAgain = 1;
							strcpy(NARRATION.message,"You fell victim to you injuries. As your eyes fold closed, you can still hear Ghetinsis laughing in the distant. That bastard! One day he will surely pay.");
							NARRATION.message_length = strlen(NARRATION.message);
							send(fighterFD[i], &NARRATION, 67 + NARRATION.message_length, MSG_WAITALL);
							dead_characters.push_back(server_characters[j]);

							server_characters.erase(server_characters.begin() + j);// we are now leaving them in for loot. this may need to be modified
							clients.erase(clients.begin() + j);
							tooClose = i;
							//close(fighterFD[i]);

						}	
						if(server_characters[j].gold > 10000)
						{
							NARRATION.type = 1; 
							strcpy(NARRATION.receiver, server_characters[j].c_name);
							strcpy(NARRATION.sender, narrator);
							NARRATION.endSend = 0;
							NARRATION.endSendAgain = 1;
							strcpy(NARRATION.message,"Ghetisis falls to the ground and lies motionless. Other captive pokemon begin to emerge from the crevices, the doors open and the sun shines down. There is no doubt that this is a great day. Well done. You did it.");
							NARRATION.message_length = strlen(NARRATION.message);
							send(fighterFD[i], &NARRATION, 67 + NARRATION.message_length, MSG_WAITALL);
							//close(fighterFD[i]);
							
							server_characters.erase(server_characters.begin() + j);// we are now leaving them in for loot. this may need to be modified
							clients.erase(clients.begin() + j);
							tooClose = i;

						}
					}
				}
			}
			for(int i = list.size(); i > 0; i--) //free vector for next time
				list.pop_back();
			for(int i = clientsJoinBattle.size(); i > 0; i--)
				clientsJoinBattle.pop_back();
			for(int i = otherClientFD.size(); i > 0; i--)
				otherClientFD.pop_back();
			STFU.unlock();
			if(tooClose != 100)
			{
				close(fighterFD[tooClose]);
			}
			for(int i = fighterFD.size(); i > 0; i--)
				fighterFD.pop_back();
			if(monster_characters[10].flags == 0x20)
			{
				monster_characters[10].flags =0xB8;
				monster_characters[10].health = 100;
				monster_characters[10].gold = 10000;
			}
			if(died == 1 || won == 1)
				goto exit_and_close; //this will remove them from the vector.. we will have to decide a way to keep them in there while also kicking them out of the game
		}
		else if(type == 4) //sent by client, our server will not support
		{
			write(client_fd, &noPvp, 37);
		}
		else if(type == 5) //again sent by client to us
		{
			if(recv(client_fd, &lootTarget, 32, MSG_WAITALL) != 32)
			{
				write(client_fd, &badRead, 26);
				continue;
			}
			int weFoundLoot = 0; //remain 0, throw error
			int yes;
			if((new_character.flags != 0xFF && new_character.flags != 0xD8 && new_character.flags != 0x98) || started == 0)
			{
				write(client_fd, &notReady, 45);
				
			}
		
			for(int i = 0; i < server_characters.size(); i++)
			{
				if(!strcmp(new_character.c_name, server_characters[i].c_name))
				{
					yes = i;
				}

			}
			if(dead_characters.size() > 0)
			{
				for(int i = 0; i < dead_characters.size(); i++)
				{
					if(dead_characters[i].room == room)
						if(dead_characters[i].gold > 0)
						{
							server_characters[yes].gold = server_characters[yes].gold + dead_characters[i].gold;
							dead_characters[i].gold = 0;
							weFoundLoot = 1;
						}

				}
			}
			for(int i = 0; i < monster_characters.size(); i++)
			{
				if(monster_characters[i].room == room && monster_characters[i].flags == 0x20)
				{
					server_characters[yes].gold = server_characters[yes].gold + monster_characters[i].gold;
					monster_characters[i].gold = 0;
					weFoundLoot = 1;

				}
			}
			if(weFoundLoot == 1)
				write(client_fd, &server_characters[yes], 48+strlen(server_characters[yes].player_description));
			else
				write(client_fd, &noTarget, 63);


			//conditions for the loot. 1- the player or monster must exist. 2- the player or monster must be dead. 3- the player or monster must be in that same room with you
		}
		else if(type == 6) //sent by client
		{
			if(new_character.flags != 0xD8 && new_character.flags != 0x98)
			{
				write(client_fd, &notReady, 45); //we may need to fix.. don't know if we should be throwing two errors... (do we still need to see if in vector?) perhap no
				continue;
			}

			started = 1;	
			room = new_character.room; 	//creating a local variable to store room in. we will access this later. 
			for(int i = 0; i < server_characters.size(); i++) //if there happens to be another player in the room when we start
			{
				if(server_characters[i].room == room && strcmp(savedName, server_characters[i].c_name))
				{
					write(client_fd, &server_characters[i], 48+strlen(server_characters[i].player_description)); //send OUR client the other persons records
					for(int j = 0; j < clients.size(); j++) //look through the clients to find which file descriptors to send our new guy to
					{
						if(!strcmp(clients[j].name, server_characters[i].c_name)) //if the client name is equal to the server character (we know it not us)
						{
							write(clients[j].fd, &new_character, 48+strlen(new_character.player_description));
						}
					}

				}
			}
			send(client_fd, &leahs_rooms[0], 37+strlen(leahs_rooms[0].roomDescription),  MSG_WAITALL);
			leahs_rooms[1].type= 13, leahs_rooms[2].type = 13;
			send(client_fd, &leahs_rooms[1], 37+strlen(leahs_rooms[1].roomDescription), MSG_WAITALL);
			send(client_fd, &leahs_rooms[2], 37+strlen(leahs_rooms[2].roomDescription), MSG_WAITALL);
			leahs_rooms[1].type = 9, leahs_rooms[2].type = 9;
			
		}
		//type 7 sent by us if there is an error. we have various error
		//type 8 sent by us. it lets the client know that their action is ok. sent after things like sending a message, creating a character, things like that
		//type 9 sent by us. sent in response to change room (type 2) or start (type 6) 
		if(type == 10){
			if(recv(client_fd, &new_character.c_name, 32, MSG_WAITALL) != 32)
			{
				printf("error in reading character name\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.flags, 1, MSG_WAITALL) != 1) //these seem to be working
			{
				printf("error in reading character flags\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.attack, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character attack\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.defense, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character defense\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.regan, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character regan\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.health, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character health\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.gold, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character gold\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.room, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character room\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.description_length, 2, MSG_WAITALL) != 2)
			{
				printf("error in reading character description length\n");
				write(client_fd, &badRead, 26);
				continue;
			}

			if(recv(client_fd, &new_character.player_description, new_character.description_length, MSG_WAITALL) != new_character.description_length)
			{
				printf("error in reading character player description\n");
				write(client_fd, &badRead, 26);
				continue;
			}
		

			printf("This is what we read through the descriptor: \t name:%s, flags:%u, attack:%u, defenese:%u, regan:%u, health:%d, gold:%u, room:%u, description length:%u, player description:%s -end-\n\n", new_character.c_name, new_character.flags, new_character.attack, new_character.defense, new_character.regan, new_character.health, new_character.gold, new_character.room, new_character.description_length, new_character.player_description);
			new_character.type = 10;
			new_character.health = 100;
			new_character.gold = 100;
			new_character.room = 1;
		
			new_character.description_length = strlen(new_character.player_description);
			if(new_character.description_length == 0)
			{
				strcpy(new_character.player_description, "A brave pokemon who will defeat Team Plasma once and for all!");
				new_character.description_length = strlen(new_character.player_description);
			}

			if((new_character.attack + new_character.defense + new_character.regan > our_game.initial_points))
			{
				write(client_fd, &badStat, 105);
				new_character.flags = 0x00;
				continue;
			}
			printf("Client formerly known as fd %d is named %s\n", client_fd, new_character.c_name);
				//clients.push_back(client(new_character.c_name, client_fd));
			int currentSize = server_characters.size();
			found = 0;
			for(int x = 0; x < currentSize; x++)
			{
				if(!strcmp(new_character.c_name, server_characters[x].c_name) && strcmp(new_character.c_name, "Testscript"))
					found =1;
				else 
					printf("We didn't find it, that is expected and good!\n");

			}
			if(dead_characters.size() > 0)
			{
				for(int i = 0; i < dead_characters.size(); i++)
				{
					if(!strcmp(dead_characters[i].c_name, new_character.c_name))
					{
						dead_characters.erase(dead_characters.begin() + i);
					}
				}
			}

				if(found == 0) {//if the entry was not found (aka its not in there and wouldn't be a duplicate
					STFU.lock();
					actionAccepted.type = 8;
					actionAccepted.action = 10;
					write(client_fd, &actionAccepted, 2);
					//lets add to make FF flags == D8 (join battle but not a monster)
					
					if(new_character.flags != 0xD8 && new_character.flags != 0x98)
						new_character.flags = 0xD8; //default to no join battle
					write(client_fd, &new_character, 48+strlen(new_character.player_description));
					clients.push_back(client(new_character.c_name, client_fd));
					server_characters.push_back(new_character);
					strcpy(savedName, new_character.c_name);
					STFU.unlock();
				}
				else{
					write(client_fd, &playerExists, 32);
					new_character.flags = 0x00;
					continue;
				}
		}
		else if(type == 12)
		{
			//close(client_fd);

			for(int i = 0; i < clients.size(); i++)
			{
				if(clients[i].fd == client_fd)
				{
					printf("%s\n", clients[i].name);
					strcpy(savedName, clients[i].name);
				}
			}

			printf("Removing client named %s\n", savedName);
			for(int i = 0; i < clients.size(); i++){
				if(clients[i].fd == client_fd){ // Then it's our entry
				//clients[i] = clients[clients.size() - 1];
					//clients.pop_back();
					STFU.lock();
					server_characters.erase(server_characters.begin() + i);
					clients.erase(clients.begin() + i);
					STFU.unlock();
				}
			}
			//printf("Closing connection to client %s\n", name);
			close(client_fd);
		}

	}

exit_and_close:
	//printf("Closing connection to client %s\n", name);
	
	for(int i = 0; i < clients.size(); i++){
		if(clients[i].fd == client_fd){ // Then it's our entry
			STFU.lock();
			server_characters.erase(server_characters.begin() + i);// we are now leaving them in for loot. this may need to be modified
			clients.erase(clients.begin() + i);
			STFU.unlock();
			}
		}
			//printf("Closing connection to client %s\n", name);
	close(client_fd);
}


//linking the rooms together notes : perhaps two arrays, one with rooms, the other with the connects of that room at the same index

int main(int argc, char ** argv){
	struct sockaddr_in sad;
	sad.sin_port = htons(5035);
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_family = AF_INET;

	if(argc > 1){
		sad.sin_port = htons(atoi(argv[1]));
	}
	printf("Listening on port %d\n", ntohs(sad.sin_port));

	init();

	int skt = socket(AF_INET, SOCK_STREAM, 0); // Step 1
	if(skt == -1){
		perror("socket");
		return 1;
	}
	if( bind(skt, (struct sockaddr*)(&sad), sizeof(struct sockaddr_in)) ){ // step 2
		perror("bind");
		return 1;
	}
	if( listen(skt, 5) ){ // step 3
		perror("listen");
		return 1;
	}
	vector<thread> threads;

	while(1){
		int client_fd;
		struct sockaddr_in client_address;
		socklen_t address_size = sizeof(struct sockaddr_in);
		client_fd = accept(skt, (struct sockaddr *)(&client_address), &address_size); 
		printf("Connection made from address %s\n", inet_ntoa(client_address.sin_addr));
		write(client_fd, &leahs_version, 5);
		//write(client_fd, &our_game, 7);
		threads.push_back(thread(client_thread, client_fd)); // maybe here we should strictly add it to our collection of clients, then go into another function or all of the interaction
	}
	
	return 0;
}


//what if we make a function for each type
//i think that even before we connect to any clients we should initialize our map for the rooms, our monster characters, anything that needs to be initialized that isn't the client should be 
//can make a map initialization function too -- returns structure / vector
//ok, so after we connect, let's send the version, as well as the game
//after we connect, let's wait for the client to send us a type
//in main function, we will have an if for each type we can receive. each type will have a function
//let's maybe push to client and character if type == 10?
//
