/*
 	Ray
    Copyright (C) 2010, 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You have received a copy of the GNU Lesser General Public License
    along with this program (lgpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/

#include <communication/MessagesHandler.h>
#include <memory/allocator.h>
#include <core/OperatingSystem.h>
#include <core/ComputeCore.h>

#include <fstream>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string.h>
using namespace std;

// #define COMMUNICATION_IS_VERBOSE


/* 3 communication models are implemented:
 * latencies are for a system with 36 cores (or 120 cores), QLogic interconnect, 
 *  and Performance scaled messaging
 * - CONFIG_COMM_IPROBE_ANY_SOURCE  
     	latency(36): 16 micro seconds
	latency(120): 19 microseconds
	no fairness, possible starvation
 * - CONFIG_COMM_IPROBE_ROUND_ROBIN 
     	latency(36):, 18 microseconds
     	latency(120): 23 microseconds
     	latency with 1008 cores: 78 (84*12): microseconds (without -route-messages.)
     	with fairness, no starvation
 * - CONFIG_COMM_PERSISTENT (latency(36): , no fairness)
     	latency(36): 28 micro seconds
	latency(120): 75 microseconds
	no fairness, possible starvation
 *
 * only one must be set
 */

/*
 * Persistent communication pumps the message more rapidly
 * on some systems
 */

//#define CONFIG_COMM_PERSISTENT

/**
 *  Use round-robin reception.
 */
#define CONFIG_COMM_IPROBE_ROUND_ROBIN

/* this configuration may be important for low latency */
/* uncomment if you want to try it */
/* this is only useful with CONFIG_COMM_IPROBE_ROUND_ROBIN */
//#define CONFIG_ONE_IPROBE_PER_TICK


//#define CONFIG_COMM_IPROBE_ANY_SOURCE

/**
 * return the first free buffer
 * this is O(MAXIMUM_NUMBER_OF_DIRTY_BUFFERS)
 */
uint8_t MessagesHandler::allocateDirtyBuffer(){

	uint8_t position=0;

	while(position<MAXIMUM_NUMBER_OF_DIRTY_BUFFERS &&
		m_dirtyBuffers[position].m_buffer!=NULL){

		position++;
	}

	#ifdef ASSERT
	if(position==MAXIMUM_NUMBER_OF_DIRTY_BUFFERS){
		cout<<"All buffers are dirty !"<<endl;
	}
	assert(position<MAXIMUM_NUMBER_OF_DIRTY_BUFFERS);
	#endif

	// to remove 
	// attention : array subscript is above array bounds [-Warray-bounds]
	if(position==MAXIMUM_NUMBER_OF_DIRTY_BUFFERS){
		position=0;
	}

	#ifdef ASSERT
	assert(m_dirtyBuffers[position].m_buffer==NULL);
	#endif

	return position;
}

/**
 * TODO Instead of a true/false state, increase and decrease requests
 * using a particular buffer. Otherwise, there may be a problem when 
 * a buffer is re-used several times for many requests.
 */
void MessagesHandler::checkDirtyBuffers(RingAllocator*outboxBufferAllocator){

	// check the buffer and free it if it is finished.
	if(m_dirtyBuffers[m_dirtyBufferPosition].m_buffer!=NULL){
		MPI_Status status;
		MPI_Request*request=&(m_dirtyBuffers[m_dirtyBufferPosition].m_messageRequest);

		int flag=0;

		MPI_Test(request,&flag,&status);

		if(flag){

			void*buffer=m_dirtyBuffers[m_dirtyBufferPosition].m_buffer;
			outboxBufferAllocator->salvageBuffer(buffer);

			#ifdef COMMUNICATION_IS_VERBOSE
			cout<<"From checkDirtyBuffers flag= "<<flag<<endl;
			#endif

			#ifdef ASSERT
			assert(*request == MPI_REQUEST_NULL);
			#endif

			m_dirtyBuffers[m_dirtyBufferPosition].m_buffer=NULL;

		}
	}

	// go to the next buffer.

	m_dirtyBufferPosition++;

	if(m_dirtyBufferPosition==MAXIMUM_NUMBER_OF_DIRTY_BUFFERS){
		m_dirtyBufferPosition=0;
	}
}

/*
 * send messages,
 */
void MessagesHandler::sendMessages(StaticVector*outbox,RingAllocator*outboxBufferAllocator){

	// update the dirty buffer list.
	for(int i=0;i<MAXIMUM_NUMBER_OF_DIRTY_BUFFERS;i++){
		checkDirtyBuffers(outboxBufferAllocator);
	}

	for(int i=0;i<(int)outbox->size();i++){
		Message*aMessage=((*outbox)[i]);
		Rank destination=aMessage->getDestination();
		void*buffer=aMessage->getBuffer();
		int count=aMessage->getCount();
		int tag=aMessage->getTag();

		#ifdef ASSERT
		assert(destination>=0);
		if(destination>=m_size){
			cout<<"Tag="<<tag<<" Destination="<<destination<<endl;
		}
		assert(destination<m_size);
		assert(!(buffer==NULL && count>0));

		// this assertion is invalid when using checksum calculation.
		//assert(count<=(int)(MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit)));
		#endif

		MPI_Request dummyRequest;

		MPI_Request*request=&dummyRequest;

		bool mustRegister=true;

		for(int i=0;i<MAXIMUM_NUMBER_OF_DIRTY_BUFFERS;i++){

			if(m_dirtyBuffers[i].m_buffer==buffer){

				// the code can not manage
				// this case
				mustRegister=false;

			}
		}
		

		if(buffer!=NULL && mustRegister){
			int dirtyBuffer=allocateDirtyBuffer();

			#ifdef ASSERT
			assert(m_dirtyBuffers[dirtyBuffer].m_buffer==NULL);
			#endif

			m_dirtyBuffers[dirtyBuffer].m_buffer=buffer;

			#ifdef ASSERT
			assert(m_dirtyBuffers[dirtyBuffer].m_buffer!=NULL);
			#endif

			request=&(m_dirtyBuffers[dirtyBuffer].m_messageRequest);

			outboxBufferAllocator->markBufferAsDirty(buffer);
		}

		//  MPI_Isend
		//      Synchronous nonblocking. 
		MPI_Isend(buffer,count,m_datatype,destination,tag,MPI_COMM_WORLD,request);

		// if the buffer is NULL, we free the request right now
		// because we there is no buffer to protect.
		if(buffer==NULL || !mustRegister){

			#ifdef COMMUNICATION_IS_VERBOSE
			cout<<" From sendMessages"<<endl;
			#endif

			MPI_Request_free(request);

			#ifdef ASSERT
			assert(*request==MPI_REQUEST_NULL);
			#endif

		}

		// if the message was re-routed, we don't care
		// we only fetch the tag for statistics
		uint8_t shortTag=tag;

		/** update statistics */
		m_messageStatistics[destination*RAY_MPI_TAG_DUMMY+shortTag]++;

		m_sentMessages++;
	}

	outbox->clear();
}

/*	
 * receiveMessages is implemented as recommanded by Mr. George Bosilca from
the University of Tennessee (via the Open-MPI mailing list)

De: George Bosilca <bosilca@…>
Reply-to: Open MPI Developers <devel@…>
À: Open MPI Developers <devel@…>
Sujet: Re: [OMPI devel] Simple program (103 lines) makes Open-1.4.3 hang
Date: 2010-11-23 18:03:04

If you know the max size of the receives I would take a different approach. 
Post few persistent receives, and manage them in a circular buffer. 
Instead of doing an MPI_Iprobe, use MPI_Test on the current head of your circular buffer. 
Once you use the data related to the receive, just do an MPI_Start on your request.
This approach will minimize the unexpected messages, and drain the connections faster. 
Moreover, at the end it is very easy to MPI_Cancel all the receives not yet matched.

    george. 
 */
void MessagesHandler::pumpMessageFromPersistentRing(StaticVector*inbox,RingAllocator*inboxAllocator){

	/* persistent communication is not enabled by default */
	int flag=0;
	MPI_Status status;
	MPI_Request*request=m_ring+m_head;
	MPI_Test(request,&flag,&status);

	if(flag){
		// get the length of the message
		// it is not necessary the same as the one posted with MPI_Recv_init
		// that one was a lower bound
		MessageTag  tag=status.MPI_TAG;
		Rank source=status.MPI_SOURCE;

		int count;
		MPI_Get_count(&status,m_datatype,&count);
		MessageUnit*filledBuffer=(MessageUnit*)m_buffers+m_head*MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit);

		// copy it in a safe buffer
		// internal buffers are all of length MAXIMUM_MESSAGE_SIZE_IN_BYTES
		MessageUnit*incoming=(MessageUnit*)inboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

		memcpy(incoming,filledBuffer,count*sizeof(MessageUnit));

		// the request can start again
		MPI_Start(m_ring+m_head);
	
		// add the message in the internal inbox
		// this inbox is not the real inbox
		Message aMessage(incoming,count,m_rank,tag,source);

		inbox->push_back(aMessage);
	}

	// advance the ring.
	m_head++;

	if(m_head == m_ringSize)
		m_head = 0;
}

void MessagesHandler::receiveMessages(StaticVector*inbox,RingAllocator*inboxAllocator){
	#if defined CONFIG_COMM_IPROBE_ROUND_ROBIN

	// round-robin reception seems to avoid starvation 
/** round robin with Iprobe will increase the latency because there will be a lot of calls to
 MPI_Iprobe that yield no messages at all */

	roundRobinReception(inbox,inboxAllocator);

	#elif defined CONFIG_COMM_PERSISTENT

	// use persistent communication
	pumpMessageFromPersistentRing(inbox,inboxAllocator);

	#elif defined CONFIG_COMM_IPROBE_ANY_SOURCE

	// receive any message
	// it is assumed that MPI is fair
	// otherwise there may be some starvation

	probeAndRead(MPI_ANY_SOURCE,MPI_ANY_TAG,inbox,inboxAllocator);

	#endif
}


void MessagesHandler::roundRobinReception(StaticVector*inbox,RingAllocator*inboxAllocator){

	/* otherwise, we use MPI_Iprobe + MPI_Recv */
	/* probe and read a message */

	int operations=0;
	while(operations<m_size){

		probeAndRead(m_connections[m_currentRankIndexToTryToReceiveFrom],MPI_ANY_TAG,
			inbox,inboxAllocator);

		// advance the head
		m_currentRankIndexToTryToReceiveFrom++;
		if(m_currentRankIndexToTryToReceiveFrom==m_peers){
			m_currentRankIndexToTryToReceiveFrom=0;
		}

		// increment operations
		operations++;

		// only 1 MPI_Iprobe is called for any source
		#ifdef CONFIG_COMM_IPROBE_ANY_SOURCE
		break;
		#endif /* CONFIG_COMM_IPROBE_ANY_SOURCE */

		#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
		#ifdef CONFIG_ONE_IPROBE_PER_TICK
		break;
		#endif /* CONFIG_ONE_IPROBE_PER_TICK */
		#endif /* CONFIG_COMM_IPROBE_ROUND_ROBIN */


		// we have read a message
		if(inbox->size()>0){
			break;
		}

	}


	#ifdef COMMUNICATION_IS_VERBOSE
	if(inbox->size() > 0){
		cout<<"[RoundRobin] received a message from "<<m_currentRankIndexToTryToReceiveFrom<<endl;
	}
	#endif /* COMMUNICATION_IS_VERBOSE */

}

/* this code is utilised, 
 * the code for persistent communication is not useful because
 * round-robin policy and persistent communication are likely 
 * not compatible because the number of persistent requests can be a limitation.
 */
void MessagesHandler::probeAndRead(Rank source,MessageTag tag,
	StaticVector*inbox,RingAllocator*inboxAllocator){

	// the code here will probe from rank source
	// with MPI_Iprobe

	#ifdef COMMUNICATION_IS_VERBOSE
	cout<<"call to probeAndRead source="<<source<<""<<endl;
	#endif /* COMMUNICATION_IS_VERBOSE */

	int flag;
	MPI_Status status;
	MPI_Iprobe(source,tag,MPI_COMM_WORLD,&flag,&status);

	/* read at most one message */
	if(flag){
		MPI_Datatype datatype=MPI_UNSIGNED_LONG_LONG;
		int tag=status.MPI_TAG;
		Rank source=status.MPI_SOURCE;
		int count=-1;
		MPI_Get_count(&status,datatype,&count);
	
		#ifdef ASSERT
		assert(count >= 0);
		#endif
	
		MessageUnit*incoming=NULL;
		if(count > 0){
			incoming=(MessageUnit*)inboxAllocator->allocate(count*sizeof(MessageUnit));
		}

		MPI_Recv(incoming,count,datatype,source,tag,MPI_COMM_WORLD,&status);

		Message aMessage(incoming,count,m_rank,tag,source);
		inbox->push_back(aMessage);

		#ifdef ASSERT
		assert(aMessage.getDestination() == m_rank);
		#endif

		m_receivedMessages++;
	}

}

void MessagesHandler::initialiseMembers(){
	#ifdef CONFIG_COMM_PERSISTENT
	// the ring itself  contain requests ready to receive messages
	m_ringSize=m_size;

	m_ring=(MPI_Request*)__Malloc(sizeof(MPI_Request)*m_ringSize,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_RING",false);
	m_buffers=(uint8_t*)__Malloc(MAXIMUM_MESSAGE_SIZE_IN_BYTES*m_ringSize,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_BUFFERS",false);
	m_head=0;

	// post a few receives.
	for(int i=0;i<m_ringSize;i++){
		uint8_t*buffer=m_buffers+i*MAXIMUM_MESSAGE_SIZE_IN_BYTES;
		MPI_Recv_init(buffer,MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit),m_datatype,
			MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,m_ring+i);
		MPI_Start(m_ring+i);
	}

	#endif
}

void MessagesHandler::freeLeftovers(){
	#ifdef CONFIG_COMM_PERSISTENT

	for(int i=0;i<m_ringSize;i++){
		MPI_Cancel(m_ring+i);
		MPI_Request_free(m_ring+i);
	}

	__Free(m_ring,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_RING",false);
	m_ring=NULL;
	__Free(m_buffers,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_BUFFERS",false);
	m_buffers=NULL;

	__Free(m_messageStatistics,"RAY_MALLOC_TYPE_MESSAGE_STATISTICS",false);
	m_messageStatistics=NULL;

	#endif
}

void MessagesHandler::constructor(int*argc,char***argv){

	for(int i=0;i<MAXIMUM_NUMBER_OF_DIRTY_BUFFERS;i++){
		m_dirtyBuffers[i].m_buffer=NULL;
		//m_dirtyBuffers[i].m_messageRequest
	}

	m_dirtyBufferPosition=0;

	m_destroyed=false;

	m_sentMessages=0;
	m_receivedMessages=0;
	m_datatype=MPI_UNSIGNED_LONG_LONG;

	MPI_Init(argc,argv);

	char serverName[1000];
	int len;

	/** initialize the message passing interface stack */
	MPI_Get_processor_name(serverName,&len);
	MPI_Comm_rank(MPI_COMM_WORLD,&m_rank);
	MPI_Comm_size(MPI_COMM_WORLD,&m_size);
	initialiseMembers();
	m_processorName=serverName;


}

void MessagesHandler::createBuffers(){

	/** initialize message statistics to 0 */
	m_messageStatistics=(uint64_t*)__Malloc(RAY_MPI_TAG_DUMMY*m_size*sizeof(uint64_t),"RAY_MALLOC_TYPE_MESSAGE_STATISTICS",false);
	for(int rank=0;rank<m_size;rank++){
		for(int tag=0;tag<RAY_MPI_TAG_DUMMY;tag++){
			m_messageStatistics[rank*RAY_MPI_TAG_DUMMY+tag]=0;
		}
	}

	// start with the first connection
	m_currentRankIndexToTryToReceiveFrom=0;


	// initialize connections
	for(int i=0;i<m_size;i++)
		m_connections.push_back(i);

	#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
	cout<<"[MessagesHandler] Will use "<<m_connections.size()<<" connections for round-robin reception."<<endl;
	#endif

	m_peers=m_connections.size();
}


void MessagesHandler::destructor(){
	if(!m_destroyed){
		MPI_Finalize();
		m_destroyed=true;
	}
}

string*MessagesHandler::getName(){
	return &m_processorName;
}

int MessagesHandler::getRank(){
	return m_rank;
}

int MessagesHandler::getSize(){
	return m_size;
}

void MessagesHandler::barrier(){
	MPI_Barrier(MPI_COMM_WORLD);
}

void MessagesHandler::version(int*a,int*b){
	MPI_Get_version(a,b);
}

void MessagesHandler::appendStatistics(const char*file){
	/** add an entry for RAY_MPI_TAG_GOOD_JOB_SEE_YOU_SOON_REPLY
 * 	because this message is sent after writting the current file
 *
 * 	actually, this RAY_MPI_TAG_GOOD_JOB_SEE_YOU_SOON_REPLY is not accounted for because
 * 	it is sent after printing the statistics.
 */

	cout<<"MessagesHandler::appendStatistics file= "<<file<<endl;

	ofstream fp;
	fp.open(file,ios_base::out|ios_base::app);
	vector<int> activePeers;
	for(int destination=0;destination<m_size;destination++){
		bool active=false;
		for(int tag=0;tag<RAY_MPI_TAG_DUMMY;tag++){
			uint64_t count=m_messageStatistics[destination*RAY_MPI_TAG_DUMMY+tag];

			if(count==0)
				continue;

			active=true;

			fp<<m_rank<<"\t"<<destination<<"\t"<<MESSAGE_TAGS[tag]<<"\t"<<count<<"\n";
		}
		if(active)
			activePeers.push_back(destination);
	}
	fp.close();

	cout<<"Rank "<<m_rank<<": sent "<<m_sentMessages<<" messages, received "<<m_receivedMessages<<" messages."<<endl;
	cout<<"Rank "<<m_rank<<": Active peers (including self): "<<activePeers.size()<<" list:";
	for(int i=0;i<(int)activePeers.size();i++){
		cout<<" "<<activePeers[i];
	}
	cout<<endl;


	cout<<endl;
}

string MessagesHandler::getMessagePassingInterfaceImplementation(){
	ostringstream implementation;

	#ifdef MPICH2
        implementation<<"MPICH2 (MPICH2) "<<MPICH2_VERSION;
	#endif

	#ifdef OMPI_MPI_H
        implementation<<"Open-MPI (OMPI_MPI_H) "<<OMPI_MAJOR_VERSION<<"."<<OMPI_MINOR_VERSION<<"."<<OMPI_RELEASE_VERSION;
	#endif

	if(implementation.str().length()==0)
		implementation<<"Unknown";
	return implementation.str();
}

// set connections to probe from
// we assume that connections are sorted.
void MessagesHandler::setConnections(vector<int>*connections){

	bool hasSelf=false;
	for(int i=0;i<(int)connections->size();i++){
		if(connections->at(i)==m_rank){
			hasSelf=true;
			break;
		}
	}

	m_connections.clear();

	// we have to make sure to insert the self link
	// at the good place.

	for(int i=0;i<(int)connections->size();i++){

		Rank otherRank=connections->at(i);

		// add the self rank
		if(!hasSelf){
			if(m_rank < otherRank){
				m_connections.push_back(m_rank);
				hasSelf=true;
			}
		}

		m_connections.push_back(otherRank);
	}

	// if we have not added the self link yet,
	// it is because it is greater.
	if(!hasSelf){
		m_connections.push_back(m_rank);
		hasSelf=true;
	}

	cout<<"[MessagesHandler] after runtime re-configuration, will use "<<m_connections.size()<<" connections for round-robin reception."<<endl;

	m_peers=m_connections.size();
}

void MessagesHandler::registerPlugin(ComputeCore*core){
	m_plugin=core->allocatePluginHandle();

	core->setPluginName(m_plugin,"MessagesHandler");
	core->setPluginDescription(m_plugin,"MPI wrapper with round-robin policy (bundled with RayPlatform)");
	core->setPluginAuthors(m_plugin,"Sébastien Boisvert");
	core->setPluginLicense(m_plugin,"GNU Lesser General License version 3");

	RAY_MPI_TAG_DUMMY=core->allocateMessageTagHandle(m_plugin);
	core->setMessageTagSymbol(m_plugin,RAY_MPI_TAG_DUMMY,"RAY_MPI_TAG_DUMMY");

	createBuffers();
}

void MessagesHandler::resolveSymbols(ComputeCore*core){
	RAY_MPI_TAG_DUMMY=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_DUMMY");
}
