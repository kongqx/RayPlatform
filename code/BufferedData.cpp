/*
 	Ray
    Copyright (C) 2010  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (LICENSE).  
	see <http://www.gnu.org/licenses/>

*/

#include<assert.h>
#include<BufferedData.h>
#include<RingAllocator.h>
#include<StaticVector.h>

void BufferedData::constructor(int numberOfRanks,int capacity){
	m_sizes=(int*)__Malloc(sizeof(int)*numberOfRanks);
	m_data=(u64*)__Malloc(sizeof(u64)*capacity*numberOfRanks);
	for(int i=0;i<(int)numberOfRanks;i++){
		m_sizes[i]=0;
	}
	m_capacity=capacity;
	m_ranks=numberOfRanks;
}


int BufferedData::size(int i){
	#ifdef DEBUG
	assert(i<m_ranks);
	#endif
	return m_sizes[i];
}

u64 BufferedData::getAt(int i,int j){
	return m_data[i*m_capacity+j];
}

void BufferedData::addAt(int i,u64 k){
	int j=size(i);
	m_data[i*m_capacity+j]=k;
	m_sizes[i]++;
}

void BufferedData::reset(int i){
	m_sizes[i]=0;
	#ifdef DEBUG
	assert(m_sizes[i]==0);
	#endif
}


bool BufferedData::flushAll(int period,int tag,RingAllocator*outboxAllocator,StaticVector*outbox,int rank){
	bool flushed=false;
	for(int i=0;i<m_ranks;i++){
		if(flush(i,period,tag,outboxAllocator,outbox,rank,true)){
			flushed=true;
		}
	}
	return flushed;
}

bool BufferedData::flush(int destination,int period,int tag,RingAllocator*outboxAllocator,StaticVector*outbox,int rank,bool force){
	int threshold=MPI_BTL_SM_EAGER_LIMIT/sizeof(VERTEX_TYPE)/period*period;
	int amount=size(destination);
	if(!force && amount<threshold){
		return false;
	}
	if(amount==0){
		return false;
	}
	VERTEX_TYPE*message=(VERTEX_TYPE*)outboxAllocator->allocate(amount*sizeof(VERTEX_TYPE));
	for(int i=0;i<amount;i++){
		message[i]=getAt(destination,i);
	}
	Message aMessage(message,amount,MPI_UNSIGNED_LONG_LONG,destination,tag,rank);
	outbox->push_back(aMessage);
	reset(destination);
	return true;
}

#ifdef DEBUG
void BufferedData::inspect(){
	for(int i=0;i<m_ranks;i++){
		assert(m_sizes[i]==0);
	}
}

#endif
