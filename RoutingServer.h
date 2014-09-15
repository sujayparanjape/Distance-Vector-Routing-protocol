/*
 * RoutingServer.h
 *
 *  Created on : Apr 24, 2014
 *      Author : sujay
 *  Description: This file contains declaration for the RoutingServer class.
 *  			 This class represents a routing node and provides required functionality
 *  			 For method descriptions please see function definition file RoutingServer.cpp
 *
 */

#ifndef ROUTINGSERVER_H_
#define ROUTINGSERVER_H_

#include "CustomStructs.h"
#include <vector>
#include <sstream>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <queue>
#include <iomanip>

using namespace std;
class RoutingServer {
public:
	RoutingServer();
	RoutingServer(vector<ServerInfo> serverList, vector<LinkInfo> linkList,int interval);
	virtual ~RoutingServer();
	int SetLinkCost(int neighbourId,int cost);
	void Init();
	void SendMessage(int serverId, char * msgPtr, int len);
	void UpdateRouteInfo(int destinationServerId,int nextHopId, int cost);
	void HandleSockInput(int sockfd);
	void HandleStdIn();
	void HandleLinkTimeoutEvent(int neighborId);
	struct LinkInfo * getLinkPtr(int neighborId);
	void setDVElement(int sourceServerId,int destServerId, int cost);
	int getDVElement(int sourceServerId,int destServerId);
	int getServerId(char * ip,char * port);
	void printDVMatrix();
	void recalculateSelfDV();
	struct RoutingTableEntry * getRoutingTableEntry(int serverid);
	void printRoutingTable();
	void sendDVToNeighbors();
	void addNextSendUpdateEvent(struct timeval currentTime);
	struct MessageElement getMessageElement(ServerInfo &nodeInfo);
	struct MessageHeader getMessageheader();
	void getCompleteMessage(char *buff);
	void Bind();
	struct ServerInfo * GetServerInfo(int serverId);
	void MarkLinkUpdate(int destinationId);
	void PrintCurrentLinkStatus();
	COMMAND getCommand(char *);
	bool DisableLink(int neighborId);
	void SimulateCrash();

private:
	//vector to store list of serverInfo objects
	vector<ServerInfo>  serverList;

	//vector to store neighbor link objects
	vector<LinkInfo> linkList;

	//vector to store routing table entries
	vector<RoutingTableEntry> routingTable;

	//vector to store upcoming event list
	vector<EventElement> eventList;

	//pointer to vector table
	int * vectorTablePtr;

	//total server count
	int totalServerCount;

	//total neighbor count
	int totalNeighborCount;

	//self server-Id
	int serverId;

	//receiving port
	char myPort[6];

	//self Ip
	char myIp[16];

	//stringstreams used for number conversions and message trace
	stringstream ss;
	stringstream ss_msg;

	//file descriptors for select
	fd_set readfds,masterfds;

	//max fd for select
	int maxFd;

	//variable to indicate timeout interval in select
	struct timeval tv;

	//indicates threashold for  period of silence
	struct timeval timeoutInterval;

	//routing update interval
	struct timeval routingUpdateInterval;

	//Next routing upadate send event time
	struct timeval nextDVSendEventTime;

	//Accepting sockfd
	int sockfd;

	//routing update interval in int datatype
	int routingUpdateInterval_sec;

	//variable to keep track of received packet count.
	int packetCout;

};

#endif /* ROUTINGSERVER_H_ */
