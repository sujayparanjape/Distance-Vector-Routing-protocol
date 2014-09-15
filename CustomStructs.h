/*
 * CustomStructs.h
 * Created on	: Apr 20, 2014
 *     Author	: sujay
 *  Description : This file contains all the configurable variables and
 *  			  custom declared structures required for the application.
 */

#ifndef CUSTOMSTRUCTS_H_
#define CUSTOMSTRUCTS_H_

#include <vector>
#include <time.h>
#include <sys/types.h>
#include <stdint.h>

using namespace std;

// defines file descriptor for console input
#define STDIN 0

// defines threashold for message backlog number on accepting socket
#define BACKLOG 10

// value that indicates INFINITY link cost
// can be set to any value <= 65535 for proper functioning of routing system
// greater values will cause wrap around while sending routing updates and will result in
// unexpected behaviour
int const INFINITY =32000;

// max intervals to wait for before making link cost to INFINITY
int const MAX_INTERVAL = 3;

//Enum type to indicate event type
enum EVENT_TYPE{LINK_TIMEOUT,SEND_UPDATE};

// Enum type to indicate command type
enum COMMAND {UPDATE,STEP,PACKETS,DISABLE,CRASH,DISPLAY,INVALID};

/**
 * Structure represents a node in the network
 */
struct ServerInfo
{
// server-id
int serverId;

//ip address of the server in x.x.x.x format
char serverIp[16];

//Message accepting port number
char serverPort[6];
};

/**
 * Structure represents information about the link with neighbor.
 */
struct LinkInfo
{
// source server Id of the link i.e. self-id
int sourceServerId;

// destination server-id
int destinationServerId;

// link cost
int cost;

// time when last update on link was received
struct timeval lastUpdateTimestamp;

// next expected event on the link
struct timeval nextEventTimeStamp;

//variable is used to indicate that link is active and updates are expected from neighbor
bool isActiveLink;

//used to indicate that link is disabled by command ,may be to indicate the broken link
bool isDisabled;
};

/*
 * Structure represents entry in a routing table
 */
struct RoutingTableEntry
{
//destination server-id
int destServerId;

//next hop server-id
int nexthopServerId;

// path cost to destination
int cost;

};


/*
 * Structure represents an event in time along with the links that are
 * associated with those links.
 */
struct EventElement
{
// time when event should occure
struct timeval eventTime;

// neighbour -id associated with the link
int neighborId;

//type of the event
EVENT_TYPE type;
};

/*
 * Below structure represents a message element in routing update message
 */
struct MessageElement
{
// 32 bit representation of Ip-address of the node
//(to be filled in network byte order)
uint32_t serverIp;

// 16 bit representation of accepting port of the node
//(to be filled in network byte order)
uint16_t serverPort;

// 16 bits , all zero field
uint16_t allZeroField;

// 16 bit representation of server-id of the node
//(to be filled in network byte order)
uint16_t serverId;

// 16 bit representation of path cost to reach the node from self
//(to be filled in network byte order)
uint16_t cost;
};

/*
 * Structure represents the message header
 */
struct MessageHeader
{
// 16 bit representation of number of updates field
//(to be filled in network byte order)
uint16_t noOfUpdateFields;

// 16 bit representation of senders accepting port.
//(to be filled in network byte order)
uint16_t serverPort;

// 32 bit representation of sender-ip i.e. self ip  when generating the message
//(to be filled in network byte order)
uint32_t serverIp;
};

#endif /* CUSTOMSTRUCTS_H_ */
