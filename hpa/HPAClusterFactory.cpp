/*
 *  ClusterFactory.cpp
 *  hog
 *
 *  Created by dharabor on 11/11/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "HPAClusterFactory.h"
#include "HPACluster.h"
#include "ClusterAStar.h"
#include <stdexcept>

HPACluster* HPAClusterFactory::createCluster(int xpos, int ypos)
{
	return createCluster(xpos, ypos, 10, 10, new ClusterAStar());
}

HPACluster* HPAClusterFactory::createCluster(int xpos, int ypos, int width, int height, AbstractClusterAStar* castar)
{
		if(castar == NULL)
			throw std::invalid_argument("HPAClusterFactory: new cluster requires a non-null AbstractClusterAStar object");
		return new HPACluster(xpos, ypos, width, height, castar);
}
