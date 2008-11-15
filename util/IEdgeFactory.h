/*
 *  IEdgeFactory.h
 *  hog
 *
 *  Created by dharabor on 8/11/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef IEDGEFACTORY_H
#define IEDGEFACTORY_H

class edge;

class IEdgeFactory
{
	public:
		virtual edge* newEdge(unsigned int fromId, unsigned int toId, double weight) = 0;
};

#endif