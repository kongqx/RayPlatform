/*
 	Ray
    Copyright (C) 2010, 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (COPYING).  
	see <http://www.gnu.org/licenses/>

*/

#ifndef _GraphImplementationDeBruijn_h
#define _GraphImplementationDeBruijn_h

#include <routing/GraphImplementation.h>
#include <vector>
#include <stdint.h>
using namespace std;

/**
 * de Bruijn graph
 * n must be a power of something
 */
class GraphImplementationDeBruijn : public GraphImplementation{

	/** the user-provided degree */
	int m_degree;

	vector<Tuple> m_graphToDeBruijn;

	int m_base;

	int m_diameter;

	void configureGraph(int n);

	int getPower(int base,int exponent);

/** convert a number to a de Bruijn vertex */
	void convertToDeBruijn(int i,Tuple*tuple);

/** convert a de Bruijn vertex to base 10 */
	int convertToBase10(Tuple*vertex);

	void printVertex(Tuple*a);
	bool isAPowerOf(int n,int base);

	int getMaximumOverlap(Tuple*a,Tuple*b);

	Rank computeNextRankInRoute(Rank source,Rank destination,Rank rank);
	bool computeConnection(Rank source,Rank destination);

protected:

	void computeRoute(Rank a,Rank b,vector<Rank>*route);
	Rank getNextRankInRoute(Rank source,Rank destination,Rank rank);
	bool isConnected(Rank source,Rank destination);
public:

	void makeConnections(int n);
	void makeRoutes();

	void setDegree(int degree);

	bool isValid(int n);
};

#endif