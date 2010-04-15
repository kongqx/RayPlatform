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
    along with this program (gpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/



#include<TipWatchdog.h>

bool TipWatchdog::getApproval(ExtensionData*ed,DepthFirstSearchData*dfsData,int minimumCoverage,u64 SEEDING_currentVertex,
	int w,BubbleData*bubbleData){
	int readsInFavorOfThis=ed->m_EXTENSION_readPositionsForVertices[dfsData->m_doChoice_tips_newEdges[0]].size();
	int coverageAtTheVertexLocation=ed->m_EXTENSION_coverages[dfsData->m_doChoice_tips_newEdges[0]];

	// reads are not supportive of this.
	if(readsInFavorOfThis*10<coverageAtTheVertexLocation){
		// no luck..., yet.
		return false;
	}


	if(ed->m_enumerateChoices_outgoingEdges.size()==2 and
		(int)ed->m_EXTENSION_readPositionsForVertices[0].size() < minimumCoverage and
		(int)ed->m_EXTENSION_readPositionsForVertices[1].size() < minimumCoverage){
		int winner=dfsData->m_doChoice_tips_newEdges[0];
		int loser=0;
		if(winner==loser)
			loser++;
		int readsForWinner=ed->m_EXTENSION_readPositionsForVertices[winner].size();
		int readsForLoser=ed->m_EXTENSION_readPositionsForVertices[loser].size();
		if(readsForWinner < 2*readsForLoser){
			#ifdef SHOW_TIP_WATCHDOG
			cout<<"Ray Oddity: The genome lacks coverage after "<<idToWord(SEEDING_currentVertex,w)<<"; Ray can't choose wisely if you don't provide enough data, be manly and rerun your sample!"<<endl;
			#endif
			ed->m_doChoice_tips_Detected=true;
			bubbleData->m_doChoice_bubbles_Detected=false;
			bubbleData->m_doChoice_bubbles_Initiated=false;
			return false;
		}
	}
	return true;
}
