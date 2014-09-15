/*
 * RoutingServer.cpp
 *
 *  Created on : Apr 24, 2014
 *      Author : sujay
 *  Description: File contains method definitions for class RoutingServer
 */

#include "RoutingServer.h"

using namespace std;

//overladed operators
struct timeval operator-(struct timeval v1, struct timeval v2);
bool operator<(struct RoutingTableEntry v1, struct RoutingTableEntry v2);

/**
 * Default constructor
 */
RoutingServer::RoutingServer() {

}

/**
 * destructor
 */
RoutingServer::~RoutingServer() {

	//deallocate the distance vector memory
	free(vectorTablePtr);

}

/**
 * parameterized constructor
 * @param serverList : compelte server list
 * @param linkList	 : complete link info
 * @param interval	 : routing update interval
 */

RoutingServer::RoutingServer(vector<ServerInfo> serverList,
		vector<LinkInfo> linkList, int interval) {
	ss.str("");
	ss_msg.str("");
	packetCout = 0;
	//set own serverid
	this->serverId = linkList[0].sourceServerId;

	//set own Ip and port number
	for (int i = 0; i < serverList.size(); i++) {
		if (serverList[i].serverId == this->serverId) {
			//initialize port info
			strcpy(this->myPort, serverList[i].serverPort);
			strcpy(this->myIp, serverList[i].serverIp);
			break;
		}
	}

	this->totalNeighborCount = linkList.size();
	this->totalServerCount = serverList.size();

	// initialize the serverList
	this->serverList = serverList;

	//initialize the neighbor link information
	this->linkList = linkList;

	//initialize the routing table
	for (int i = 0; i < this->totalServerCount; i++) {
		RoutingTableEntry e_routingEntry;
		e_routingEntry.destServerId = this->serverList[i].serverId;
		e_routingEntry.nexthopServerId = -1;
		//e_routingEntry.isActiveEntry = 1;
		e_routingEntry.cost = INFINITY;

		if (this->serverId == e_routingEntry.destServerId) {
			// mark the self link with 0 cost and next hop as self.
			e_routingEntry.nexthopServerId = this->serverId;
			e_routingEntry.cost = 0;
		}

		this->routingTable.push_back(e_routingEntry);
	}

	//allocate the distance vector table
	vectorTablePtr = (int *) malloc(
			totalServerCount * totalServerCount * sizeof(int));

	//Initialize all values in the DV's for all servers to INFINITY
	for (int i = 0; i < this->serverList.size(); i++) {
		for (int j = 0; j < this->serverList.size(); j++) {
			if (i == j)
				setDVElement(this->serverList[i].serverId,
						this->serverList[j].serverId, 0);
			else
				setDVElement(this->serverList[i].serverId,
						this->serverList[j].serverId, INFINITY);
		}

	}

	//update the link costs to neighbor in the routing table
	for (int i = 0; i < totalNeighborCount; i++) {

		setDVElement(serverId, this->linkList[i].destinationServerId,
				this->linkList[i].cost);

		UpdateRouteInfo(this->linkList[i].destinationServerId //destinitionServerId
				, this->linkList[i].destinationServerId  //nextHop
				, this->linkList[i].cost);				//cost
	}

	//initialize timeout interval and routing interval
	routingUpdateInterval_sec = interval;
	timeoutInterval.tv_sec = routingUpdateInterval_sec * MAX_INTERVAL;
	timeoutInterval.tv_usec = 0;

	routingUpdateInterval.tv_sec = routingUpdateInterval_sec;
	routingUpdateInterval.tv_usec = 0;

}

/**
 * Function sets the link cost to given value
 * @param neighbourId : other end Server-id of the link
 * @param cost		  : new cost
 * @return			  : 0 on success -1 on failure
 */
int RoutingServer::SetLinkCost(int neighbourId, int cost) {
	struct LinkInfo * linkPtr;
	linkPtr = getLinkPtr(neighbourId);
	if (linkPtr == NULL) {
		ss_msg << "Unable to set the cost." << endl;
		return -1;
	} else {
		linkPtr->cost = cost;
		return 0;
	}

}
/**
 * 	Start the routing server
 */
void RoutingServer::Init() {
	struct timeval currentTime;
	FD_ZERO(&readfds);
	FD_ZERO(&masterfds);
	maxFd = STDIN + 1;

	//add std in
	FD_SET(STDIN, &masterfds);

	// used in while loop. Can be used for event queue error detection
	struct timeval currentEventTime;
	struct timeval lastEventTime;

	//set link timeouts and add the initial events
	gettimeofday(&currentTime, NULL);
	lastEventTime = currentTime;
	for (int k = 0; k < linkList.size(); k++) {
		if (linkList[k].isActiveLink)  // is this condition required ?
		{
			//add last updated timestamp
			linkList[k].lastUpdateTimestamp = currentTime;

			//set next expected event timestamp
			timeradd(&linkList[k].lastUpdateTimestamp, &routingUpdateInterval,
					&linkList[k].nextEventTimeStamp);

			//create an event at nextevent timestamp
			struct EventElement e_event;
			e_event.eventTime = linkList[k].nextEventTimeStamp;
			e_event.neighborId = linkList[k].destinationServerId;
			e_event.type = LINK_TIMEOUT;

			//push the event at the back of the event list
			eventList.push_back(e_event);
		}

	}

	//create an event for update sending
	struct EventElement e_event;
	timeradd(&currentTime, &routingUpdateInterval, &e_event.eventTime);
	e_event.neighborId = -1;
	e_event.type = SEND_UPDATE;
	eventList.push_back(e_event);
	nextDVSendEventTime = e_event.eventTime;

	//bind to given port and update self sockfd.
	Bind();

	//add sockfd to readfd set
	FD_SET(this->sockfd, &masterfds);

	this->maxFd = this->maxFd < sockfd ? sockfd : this->maxFd;

	//display message
	cout<<"Routing node up and running at Ip: "<<this->myIp <<", port: "<<this->myPort<<"(server-id "<<this->serverId<<")"<<endl;

	while (1) {
		readfds = masterfds;
		if (eventList.size() == 0) {
			cout << "No more expected events in future." << endl;
			break;
		}

		//set next timoutvalue
		gettimeofday(&currentTime, NULL);
		tv = eventList.front().eventTime - currentTime; //overloaded '-' operator for timeval

		select(maxFd + 1, &readfds, NULL, NULL, &tv);

		if (FD_ISSET(sockfd,&readfds) || FD_ISSET(STDIN,&readfds)) //handle input on STDIN/socket
		{

			if (FD_ISSET(sockfd,&readfds)) {
				HandleSockInput(sockfd);
			}

			if (FD_ISSET(STDIN,&readfds)) {
				HandleStdIn();
			}
		} else //handle timeout //can check value of tv ,to check if timeout occured.
		{

			currentEventTime = eventList.front().eventTime;

			// handle timeout
			while (eventList.size() > 0
					&& !timercmp(&eventList.front().eventTime,&currentEventTime,!=)) // using  !(a != b) becuase some
			// it seems some implementations
			// do not support == operator
			{

				EventElement current_event = eventList.front();

				switch (current_event.type) {
				case LINK_TIMEOUT: {
					struct LinkInfo *linkPtr = getLinkPtr(
							current_event.neighborId);

					if (linkPtr->isDisabled) {
						break;
					}

					if (timercmp(&linkPtr->nextEventTimeStamp,&currentEventTime,>)) // nextExpectedEventTime > currentEventTime
					{
						//no action
					} else if (!timercmp(&linkPtr->nextEventTimeStamp,&currentEventTime, !=)) // nextExpectedEvent == currentEventTime
					{

						struct timeval timediff;
						timersub(&linkPtr->nextEventTimeStamp,
								&linkPtr->lastUpdateTimestamp, &timediff);

						//check if lastUpdateTimestamp - current >= 3* IntervalTime
						struct timeval diff = linkPtr->nextEventTimeStamp
								- linkPtr->lastUpdateTimestamp;
						if (!timercmp(&diff,&timeoutInterval,<)) // diff not less that timeoutInterval i.e. greater than or equal to
						{

							if (linkPtr->isActiveLink) {
								//mark the link as inactive..
								//effect: no more timeout event generation till any more updates from this link
								//      ,so no more unnecessary distance vector calculations
								linkPtr->isActiveLink = false;

								//update the distance vector matrix
								setDVElement(this->serverId,
										linkPtr->destinationServerId, INFINITY);

								//update the link cost in the linklist
								SetLinkCost(linkPtr->destinationServerId,
										INFINITY);

								//recalculate distance vectors
								recalculateSelfDV();

							} else {
								//cout<<"ERROR: timeout event for non-active link"<<endl;
							}

						} else {
							HandleLinkTimeoutEvent(current_event.neighborId);
						}

					} else // Some issue. NextExpectedEventtime < currentExpectedEventTime
					{
						//cout<<"next expected event < currentevent time. Some issue with the event queue";
					}

				}
					break;
				case SEND_UPDATE: {
					//cout<<"Send_Update event at time "<<currentEventTime.tv_sec<<endl;
					if (!timercmp(&currentEventTime,&nextDVSendEventTime,!=)) //currentEventime and DVupdate event time is same
					{
						sendDVToNeighbors();
						addNextSendUpdateEvent(currentEventTime);
					}
				}
					break;
				default:
					//cout<<"Unknown event"<<endl;
					break;
				}

				//pop event from the list
				eventList.erase(eventList.begin());
			}

		}

	}

}

/**
 * FUnction sends the given messagae to the neighbor
 * @param serverId : receiver server-ID
 * @param msgPtr   : message pointer
 * @param msglen   : length of the message
 */
void RoutingServer::SendMessage(int serverId, char * msgPtr, int msglen) {
	struct ServerInfo *nodePtr = GetServerInfo(serverId);

	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct addrinfo hints, *res;
	//int sockfd, new_fd,tempSockFd;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(nodePtr->serverIp, nodePtr->serverPort, &hints, &res)
			!= 0) {
		cout
				<< "Unable to get addrinfo for given server for sending UDP message"
				<< endl;
		return;
	}

	int newSockFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	sendto(newSockFd, msgPtr, msglen, 0, res->ai_addr, res->ai_addrlen);

}

/**
 *  Function updates the rout info for given destination
 * @param destinationServerId: destination Server-ID
 * @param nextHopId			 : next hop server-ID
 * @param cost				 : mincost
 */
void RoutingServer::UpdateRouteInfo(int destinationServerId, int nextHopId,
		int cost) {
	int i = 0;
	for (i = 0; i < routingTable.size(); i++) {
		if (routingTable[i].destServerId == destinationServerId) {
			routingTable[i].nexthopServerId = nextHopId;
			routingTable[i].cost = cost;
			break;
		}
	}

	if (i == routingTable.size()) {
		// ERROR
		//	cout<<"Unable to find the given destination server in the routing table."<<endl;
	}
}

/**
 *	function receives the data on the given sockfd
 * @param sockfd : Sock file descriptor to read data from
 */
void RoutingServer::HandleSockInput(int sockfd) {

	struct sockaddr from;
	uint16_t noOfUpdateFields;
	uint fromlen = sizeof from;
	int recvsize = -1;
	recvsize = (int) recvfrom(sockfd, (void *) &noOfUpdateFields,
			sizeof noOfUpdateFields, MSG_PEEK, (struct sockaddr *) &from,
			&fromlen);

	if (recvsize <= 0) {
		cout << "Unable to read the data on socket." << endl;
		return;
	}
	noOfUpdateFields = ntohs(noOfUpdateFields);
	int packetSize = noOfUpdateFields * sizeof(struct MessageElement)
			+ sizeof(MessageHeader);

	char * buff = new char[packetSize];

	recvsize = -1;
	recvsize = (int) recvfrom(sockfd, (void *) buff, packetSize, 0,
			(struct sockaddr *) &from, &fromlen);
	if (recvsize < packetSize) {
		cout << "Unable to read the entire packet." << endl;
		delete[] buff;
		return;
	}

	struct MessageHeader *headerPtr;

	struct MessageElement *msgElementPtr;

//point the header pointer to the start of received data
	headerPtr = (struct MessageHeader *) buff;

	uint32_t senderbuff;
	memcpy(&senderbuff, &headerPtr->serverIp, sizeof senderbuff);
	senderbuff = ntohl(senderbuff);

//get the senderIp from the message header
	char senderIP[16];
	memset(senderIP, 0, sizeof senderIP);
	socklen_t len = sizeof senderIP;
	inet_ntop(AF_INET, &senderbuff, senderIP, len); // here senderBuff should be in network byte order
	senderIP[15] = '\0';

//get the port from message header
	uint16_t senderPort;
	memcpy(&senderPort, &headerPtr->serverPort, sizeof senderPort);
	senderPort = ntohs(senderPort);

//convert port to char arr
	ss.str("");
	ss << senderPort;
	char senderPort_char[6];
	memcpy(senderPort_char, ss.str().c_str(), ss.str().length());
	senderPort_char[ss.str().length()] = '\0';

//get the sender id based on IP and port
	int senderId = getServerId(senderIP, senderPort_char);

//message from a non neighbor. Just ignore packet and return
	if (senderId == 0) {
		delete[] buff;
		return;
	}

//check if disabled link
	struct LinkInfo *linkPtr = NULL;
	linkPtr = getLinkPtr(senderId);

	if (linkPtr == NULL || linkPtr->isDisabled) {
		//disabled link. Just return ignore packet processing and return.
		delete[] buff;
		return;
	}

//mark the link update
	MarkLinkUpdate(senderId);

	cout << "RECEIVED A MESSAGE FROM SERVER " << senderId << endl;
	packetCout++;

///update distance vector

//point the messagepointer to start of message field
	msgElementPtr = (struct MessageElement *) (buff + sizeof(MessageHeader));

//repeate the message processing for "no of updates" field
	for (int i = 0; i < noOfUpdateFields; i++, msgElementPtr++) {
		//get the destination Id and cost
		uint16_t destinationSeverId, cost;
		memcpy(&destinationSeverId, &msgElementPtr->serverId, sizeof(uint16_t));
		memcpy(&cost, &msgElementPtr->cost, sizeof(uint16_t));

		destinationSeverId = ntohs(destinationSeverId);
		cost = ntohs(cost);

		//set the entry in Distance vector matrix
		setDVElement(senderId, (int) destinationSeverId, (int) cost);
	}

//recalculate the shortest paths
	recalculateSelfDV();

//detete the buffer used for packet
	delete[] buff;
}

/**
 * Handle console inputs
 */
void RoutingServer::HandleStdIn() {
//read input line
	string input;
	getline(cin, input);

// error message if no input
	if (input.length() == 0) {
		cout << "Invalid input" << endl;
		return;
	}

//copy input line to char array for processing
	char * input_char = new char[input.size() + 1];
	strcpy(input_char, input.c_str());
	input_char[input.size()] = '\0';

//point the tmp pointer to command
	char * tmp = strtok(input_char, " ");

	ss_msg.str("");
	switch (getCommand(tmp)) {
	case UPDATE: {
		ss_msg.str("");
		int serverId1 = -1, serverId2 = -1, cost;

		tmp = strtok(NULL, " ");
		if (tmp == NULL) {
			ss_msg << "update: Incomplete parameters for the command" << endl;
			break;
		}
		serverId1 = (int) strtol(tmp, NULL, 10);
		if (serverId1 <= 0) {
			ss_msg << "update: Invalid value for serverId1" << endl;
			break;
		}

		tmp = strtok(NULL, " ");
		if (tmp == NULL) {
			ss_msg << "update: Incomplete parameters for the command" << endl;
			break;
		}
		serverId2 = (int) strtol(tmp, NULL, 10);
		if (serverId2 <= 0) {
			ss_msg << "update: Invalid value for serverId2" << endl;
			break;
		}

		tmp = strtok(NULL, " ");
		if (tmp == NULL) {
			ss_msg << "update : Incomplete parameters for the command" << endl;
			break;
		}
		cost = -1;

		if (strcmp(tmp, "inf") == 0)
			cost = INFINITY;
		else
			cost =  strtol(tmp, NULL, 10) > INFINITY ? INFINITY : strtol(tmp, NULL, 10) ;

		if (cost <= 0) {
			ss_msg << "update : Invalid value for cost parameter" << endl;
			break;
		}

		if (serverId1 != this->serverId) {
			ss_msg << "update : Invalid parameter value for serverId1" << endl;
			break;
		}

		tmp = strtok(NULL, " ");
		if (tmp != NULL) {
			cout << "Ignoring extra input(" << tmp << ")" << endl;
		}

		if (SetLinkCost(serverId2, cost) == 0) {
			struct LinkInfo * linkPtr;
			linkPtr = getLinkPtr(serverId2);
			if (!linkPtr->isActiveLink) {
				//inactive link
				//activate the link again
				MarkLinkUpdate(serverId2);
			}
			recalculateSelfDV();
			ss_msg << "update SUCCESS" << endl;
		} else {
			string str = ss_msg.str();
			ss_msg.str("");
			ss_msg << "update failed" << endl;
			ss_msg << str;
		}

	}
		break;

	case STEP:
		ss_msg.str("");
		//send distance vectors to active links
		sendDVToNeighbors();
		struct timeval currentTime;
		gettimeofday(&currentTime, NULL);

		//add next routing update event
		addNextSendUpdateEvent(currentTime);
		ss_msg << "step SUCCESS" << endl;
		break;

	case PACKETS:
		ss_msg.str("");
		ss_msg << "packets SUCCESS" << endl;
		ss_msg << "Number of packets received since last invocation: "
				<< packetCout << endl;
		//reset the count
		packetCout = 0;
		break;

	case DISABLE: {
		ss_msg.str("");
		int neighborId = -1;
		tmp = strtok(NULL, " ");
		if (tmp == NULL) {
			ss_msg << "disable : Incomplete parameters for the command."
					<< endl;
			break;
		}
		neighborId = (int) strtol(tmp, NULL, 10);

		if (neighborId <= 0) {
			ss_msg << "disable : Invalid value for parameter <server-ID>"
					<< endl;
			break;
		}

		if (DisableLink(neighborId)) {
			ss_msg << "disable SUCCESS" << endl;
		} else {
			cout << "disable : Failed to disable the link." << endl;
		}
		break;
	}

	case CRASH:
		SimulateCrash();
		break;

	case DISPLAY:
		cout << "display SUCCESS" << endl;
		printRoutingTable();
		break;

	case INVALID:
		cout << "Invalid command" << endl;
		break;
	}

// dump the message trace generated during command execution to cout
	cout << ss_msg.str();
	cout.flush();
	ss_msg.str("");

//delete the buffer used for processing
	delete[] input_char;
}

/**
 * Functino generates new timeout event for the link
 * @param neighborId : neighbor server-id on the other end of link.
 */
void RoutingServer::HandleLinkTimeoutEvent(int neighborId) {

	struct LinkInfo * linkPtr = getLinkPtr(neighborId);

// if link is not active dont generate the timeout event
	if (!linkPtr->isActiveLink)
		return;

//create new event
	struct EventElement e_event;
	e_event.neighborId = neighborId;
	timeradd(&linkPtr->nextEventTimeStamp, &this->routingUpdateInterval,
			&e_event.eventTime);
	e_event.type = LINK_TIMEOUT;

//push at the back of eventqueue
	eventList.push_back(e_event);

//update the next expected event for the link
	linkPtr->nextEventTimeStamp = e_event.eventTime;
}

/**
 * function returns the pointer to the linkinfo object of given neighbor server-id
 * @param neighborId : neighbor server-id
 * @return : pointer to the linkinfo object
 */
struct LinkInfo* RoutingServer::getLinkPtr(int neighborId) {
	struct LinkInfo *linkPtr = NULL;
	for (int i = 0; i < linkList.size(); i++) {
		if (linkList[i].destinationServerId == neighborId) {
			linkPtr = &linkList[i];
			break;
		}
	}

	if (linkPtr == NULL) {
		ss_msg << "Unable to find the link with neighbor " << neighborId << "."
				<< endl;
		cout.flush();
	}
	return linkPtr;
}

/**
 * Overloaded operator used for subtracting 2 timeval element.
 * Can be used to custom handle timeval subtractions
 * @param v1 minuend
 * @param v2 subtrahend
 * @return subtraction result
 */
struct timeval operator-(struct timeval v1, struct timeval v2) {

	struct timeval tv;

	if (timercmp(&v1,&v2,<)) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		timersub(&v1, &v2, &tv);
	}

	return tv;

}

/**
 * function sets particular entry in a distance vector matrix
 * @param sourceServerId : source server-id
 * @param destServerId   : destination server-id
 * @param cost			 : cost to be set
 */
void RoutingServer::setDVElement(int sourceServerId, int destServerId,
		int cost) {

	int sourceIndex = -1;
	int destIndex = -1;
	for (int i = 0; i < serverList.size(); i++) {
		if (serverList[i].serverId == sourceServerId)
			sourceIndex = i;
		if (serverList[i].serverId == destServerId)
			destIndex = i;
	}
	if (sourceIndex == -1 || destIndex == -1) {
		//ERROR
		//cout<<"setDVElement: Unable to find index for given servers."<<"server ids are "<< sourceServerId<<" "<<destServerId<<endl;
	} else {
		*(vectorTablePtr + totalServerCount * sourceIndex + destIndex) = cost;
	}

}

/**
 * function returns particular entry from distance vector matrix
 * @param sourceServerId : source server-id
 * @param destServerId 	 : destination server-id
 * @return				 : cost from source to destination
 */
int RoutingServer::getDVElement(int sourceServerId, int destServerId) {
	int sourceIndex = -1;
	int destIndex = -1;
	for (int i = 0; i < serverList.size(); i++) {
		if (serverList[i].serverId == sourceServerId)
			sourceIndex = i;
		if (serverList[i].serverId == destServerId)
			destIndex = i;
	}
	if (sourceIndex == -1 || destIndex == -1) {
		//ERROR
		//cout<<"getDVElement: Unable to find index for given servers.(source"<<sourceServerId<<" dest:"<<destServerId<<endl;
		return INFINITY;
	}

	return *(vectorTablePtr + totalServerCount * sourceIndex + destIndex);

}

/**
 * function returns the server-id by matching given IP and port
 * @param ip   : Ip address in  the form a.b.c.d
 * @param port : numeric port
 * @return
 */
int RoutingServer::getServerId(char* ip, char* port) {

	for (int i = 0; i < serverList.size(); i++) {
		if (strcmp(ip, serverList[i].serverIp) == 0
				&& strcmp(port, serverList[i].serverPort) == 0)
			return serverList[i].serverId;
	}
	//ERROR
	//cout<<"Unable to match given Ip/port with the server list"<<endl;
	return 0;
}

/**
 * function prints the distance vector matrix
 *  Used for debugging. Not called from anywhere in final code
 */
void RoutingServer::printDVMatrix() {

	for (int i = 0; i < serverList.size(); i++) {
		cout << "DV of Serverid " << serverList[i].serverId << endl;
		for (int j = 0; j < serverList.size(); j++) {
			cout << serverList[i].serverId << "->" << serverList[j].serverId
					<< ": " << *(vectorTablePtr + totalServerCount * i + j)
					<< " ";
		}
		cout << endl;
	}

}

/**
 *  function recaluclates the distance vector based on distance vector matrix
 */
void RoutingServer::recalculateSelfDV() {

//calculate distance and next hop for each node in network
	for (int i = 0; i < serverList.size(); i++) {
		//do not perform calculations if destination is self
		if (serverList[i].serverId == this->serverId)
			continue;

		int nextHopServerId = -1;
		int minCost = INFINITY;

		// calculate the cost for destination via each neighbor
		for (int j = 0; j < linkList.size(); j++) {
			if (!linkList[j].isDisabled) // consider link for calculations only if not disabled
			{

				if (minCost
						> getDVElement(linkList[j].destinationServerId,
								serverList[i].serverId) + linkList[j].cost) {
					//minCost > (link[j] weight + neighbor[j]->dest distance)
					//update min cost to (link[j] weight + neighbor[j]->dest distance)
					//update next hop
					minCost = getDVElement(linkList[j].destinationServerId,
							serverList[i].serverId) + linkList[j].cost;
					nextHopServerId = linkList[j].destinationServerId;
				}
			}

		}

		//set the minimum cost in self DV
		setDVElement(this->serverId, serverList[i].serverId, minCost);

		//update routing table
		struct RoutingTableEntry * routingEntryPtr = getRoutingTableEntry(
				serverList[i].serverId);

		routingEntryPtr->nexthopServerId = nextHopServerId;
		routingEntryPtr->cost = minCost;
	}

}

/**
 * Returns the routing table entry for given destination
 * @param destServerid : Server-Id of destination
 * @return returns the Routing table entry which contains next hop and cost
 */
struct RoutingTableEntry * RoutingServer::getRoutingTableEntry(
		int destServerid) {
	struct RoutingTableEntry * routingEntryPtr = NULL;

	for (int i = 0; i < routingTable.size(); i++) {
		if (routingTable[i].destServerId == destServerid) {
			routingEntryPtr = &routingTable[i];
			break;
		}

	}
	if (routingEntryPtr == NULL) {
		//ERROR
		//cout<<"Unable to find routing entry for given destination server("<<destServerid<<")"<<endl;

	}

	return routingEntryPtr;
}

/**
 * function prints the routing table. Entry to self is not printed
 */
void RoutingServer::printRoutingTable() {

	priority_queue<RoutingTableEntry> queue; // overloaded < operator for struct RoutingTableEntry to support queue operations

	cout << setw(25) << "Destination Server-ID" << setw(20)
			<< "Next hop Server-ID" << setw(15) << "Cost" << endl;

	//push the routing table entries in priority queue
	for (int i = 0; i < routingTable.size(); i++) {
		queue.push(routingTable[i]);

	}

	//print entries in ascending server-Id order
	for (int i = 0; i < routingTable.size(); i++) {
		RoutingTableEntry e_entry;
		e_entry = queue.top();
		queue.pop();
		if (e_entry.destServerId != this->serverId) {

			ss.str("");
			ss << e_entry.nexthopServerId;
			string nextHop_str = ss.str();

			ss.str("");
			ss << e_entry.cost;
			string cost_str = ss.str();

			//inf cost infinity the print nexthop: N/A and cost: inf else print actuals
			cout << setw(25) << e_entry.destServerId << setw(20)
					<< (e_entry.nexthopServerId == -1 ? "N/A" : nextHop_str)
					<< setw(15) << (e_entry.cost >= INFINITY ? "inf" : cost_str)
					<< endl;
		}
	}
}

/**
 * sends the own distance vector on all active links
 */
void RoutingServer::sendDVToNeighbors() {

	//Allocate the memory for the message
	int messageLen = sizeof(struct MessageHeader)
			+ sizeof(struct MessageElement) * serverList.size();
	char * buff = new char[messageLen];

	//generate the message and copy it to buffer
	getCompleteMessage(buff);

	//send updates on all non disabled links
	for (int i = 0; i < linkList.size(); i++) {
		if (!linkList[i].isDisabled) //  if not disabled then send. Link may be active/inactive
		{
			//send the UDP message
			SendMessage(linkList[i].destinationServerId, buff, messageLen);
		}
	}

	// free the buffer memory
	delete[] buff;

}

/**
 * Function adds the next routing update event with time = currentTime + routingUpdate interval
 * @param currentTime : current time value
 */
void RoutingServer::addNextSendUpdateEvent(struct timeval currentTime) {
	//gettimeofday(&currentTime,NULL);
	struct EventElement e_event;

	//set event time  = current time + routing update interval
	timeradd(&currentTime, &routingUpdateInterval, &e_event.eventTime);
	e_event.neighborId = -1;
	e_event.type = SEND_UPDATE;
	eventList.push_back(e_event);

	//update the next expected routingUpdate time
	//used to simulate timer reset on step command
	nextDVSendEventTime = e_event.eventTime;
}

/**
 * Function generates entry for the routing update message from given serverInfo object
 * @param nodeInfo : serverInfo object
 * @return returns struct MessageElement that can be appended to the final message to be sent over the network
 */
struct MessageElement RoutingServer::getMessageElement(ServerInfo &nodeInfo) {
	struct MessageElement msgElement;

	//assign ip field
	inet_pton(AF_INET, nodeInfo.serverIp, (void *) &msgElement.serverIp);
	msgElement.serverIp = htonl(msgElement.serverIp);

	//assign port info
	msgElement.serverPort = htons(
			(uint16_t) strtol(nodeInfo.serverPort, NULL, 10));

	//copy zero to the allzero field
	memset(&msgElement.allZeroField, 0, sizeof(msgElement.allZeroField));

	//copy server Id
	msgElement.serverId = htons((uint16_t) nodeInfo.serverId);

	//copy cost
	msgElement.cost = htons(
			(uint16_t) getDVElement(this->serverId, nodeInfo.serverId));

	return msgElement;
}

/**
 * Function generates the message header element
 * @return : MessageHeader object that can be added to the final message
 */
struct MessageHeader RoutingServer::getMessageheader() {

	struct MessageHeader msgHeader;

	// no of MessageElements = no of servers
	msgHeader.noOfUpdateFields = htons((uint16_t) this->serverList.size());

	// port
	msgHeader.serverPort = htons((uint16_t) strtol(this->myPort, NULL, 10));

	//IP
	inet_pton(AF_INET, this->myIp, (void *) &msgHeader.serverIp);
	msgHeader.serverIp = htonl(msgHeader.serverIp);

	return msgHeader;
}

/**
 * Function generates finalmessage that contains (header) + (message element for each node on network)
 * @param buff : final message buffer
 */
void RoutingServer::getCompleteMessage(char *buff) {
	//generate message header
	struct MessageHeader msgHeader = getMessageheader();

	//copy header to final message buffer start
	memcpy(buff, &msgHeader, sizeof msgHeader);

	//fill the remaining buffer with messageelement for each distance vector entry i.e. No of nodes in nw
	//buffer should be exact size.
	for (int i = 0; i < serverList.size(); i++) {
		struct MessageElement msgElement = getMessageElement(serverList[i]);
		memcpy(
				buff + sizeof(struct MessageHeader)
						+ i * sizeof(struct MessageElement), &msgElement,
				sizeof msgElement);
	}
}

/**
 * binds to the self sock
 */
void RoutingServer::Bind() {

	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof hints);

	int status;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;


	if ((status = getaddrinfo(this->myIp, this->myPort, &hints, &res)) != 0) {
		cout
				<< "getaddrinfo returned failure status for given IP-port combination. Terminating the execution."
				<< endl;
		exit(1);
		//return ;
	}

	this->sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	status = 0;

	//set sock opt to reuse the sockets
	int yes = 1;
	if (setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
			== -1) {
		freeaddrinfo(res);
		cout << "Failed at setsockopt() call. Terminatinf the execution"
				<< endl;
		exit(1);
	}

	//bind to given socket
	if ((status = bind(this->sockfd, res->ai_addr, res->ai_addrlen)) < 0) {
		cout << "Bind failed for given socket. Terminating the execution."
				<< endl;
		freeaddrinfo(res);
		exit(1);
	}

	freeaddrinfo(res);
}
/**
 * returns the serverinfo object for given server-id
 * @param serverId: server-id
 * @return
 */
struct ServerInfo* RoutingServer::GetServerInfo(int serverId) {
	struct ServerInfo* nodePtr = NULL;

	for (int i = 0; i < serverList.size(); i++) {
		if (serverList[i].serverId == serverId) {
			nodePtr = &serverList[i];
			break;
		}
	}

	if (nodePtr == NULL) {
		ss_msg << "Unable to find given serverId in server list." << endl;
	}
	return nodePtr;
}

/**
 * Function marks that update has benn received on link to given neighbor
 * and generates next timeout event for the link
 * @param destinationId : server id of the node on the other end of link
 */
void RoutingServer::MarkLinkUpdate(int destinationId) {
	struct LinkInfo * linkPtr;

	linkPtr = getLinkPtr(destinationId);
	//set the link as active
	if (!linkPtr->isActiveLink) {
		//set the link active again
		linkPtr->isActiveLink = true;

		//update the old cost ?:no. Keep it inf
	}

	//update the lastUpadteTimestamp
	gettimeofday(&linkPtr->lastUpdateTimestamp, NULL);

	//generate the next event timestamp
	timeradd(&linkPtr->lastUpdateTimestamp, &routingUpdateInterval,
			&linkPtr-> nextEventTimeStamp);

	//create an event for next timeout
	struct EventElement e_event;
	e_event.neighborId = destinationId;
	e_event.eventTime = getLinkPtr(destinationId)->nextEventTimeStamp;
	e_event.type = LINK_TIMEOUT;

	//add the next event to the eventlist
	this->eventList.push_back(e_event);
}

/**
 * Prints current link status
 * used for debugging
 */
void RoutingServer::PrintCurrentLinkStatus() {
	for (int i = 0; i < linkList.size(); i++) {
		cout << "Link " << serverId << "->" << linkList[i].destinationServerId
				<< " ,cost:" << linkList[i].cost << ", communication status:"
				<< (linkList[i].isActiveLink ? "Active" : "Inactive")
				<< ", manually disabled: "
				<< (linkList[i].isDisabled ? "True" : "False") << endl;
	}
}

/**
 * function validates the input command
 * @param cmd : command string
 * @return COMMAND enum indicating command type
 */
COMMAND RoutingServer::getCommand(char* cmd) {

	//check if input command is a valid command
	//return corresponding enum
	if (strcmp(cmd, "update") == 0) {
		return UPDATE;
	} else if (strcmp(cmd, "step") == 0) {
		return STEP;
	} else if (strcmp(cmd, "packets") == 0) {
		return PACKETS;
	} else if (strcmp(cmd, "display") == 0) {
		return DISPLAY;
	} else if (strcmp(cmd, "disable") == 0) {
		return DISABLE;
	} else if (strcmp(cmd, "crash") == 0) {
		return CRASH;
	} else {
		return INVALID;
	}

}

/**
 * Overloaded < operator required for min priority queue
 * @param v1,v2 entries to be compared
  * @return false if v1 < v2, else true
 */
bool operator<(struct RoutingTableEntry v1, struct RoutingTableEntry v2) {
	if (v1.destServerId < v2.destServerId)
		return false;
	else
		return true;
}

/**
 * Function marks the link to givne neighbr as disabled
 * @param neighborId - neighbor server id
 * @return
 */
bool RoutingServer::DisableLink(int neighborId) {

	struct LinkInfo *linkPtr = NULL;

	linkPtr = getLinkPtr(neighborId);
	if (linkPtr == NULL) {
		return false;
	}

	linkPtr->isDisabled = true;
	linkPtr->cost = INFINITY;

	//recalculate distance vectors after disabling the link
	recalculateSelfDV();

	return true;
}

/**
 * function simulates node crash. Execution never returns from here.
 */
void RoutingServer::SimulateCrash() {

	cout
			<< "Simulating crash. Process will be unresponsive to any console / socket inputs."
			<< endl;

	//simulating crash using select
	//just to save some processing load.
	//infinite while will take care of non responsiveness.
	struct timeval dummyTimer;
	dummyTimer.tv_sec = 100;
	dummyTimer.tv_usec = 0;
	while (true) {
		dummyTimer.tv_sec = 100;
		dummyTimer.tv_usec = 0;
		select(0, NULL, NULL, NULL, &dummyTimer);
	}
}
