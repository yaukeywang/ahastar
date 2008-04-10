/*
 *  AnnotatedClusterAbstraction.cpp
 *  hog
	
	Extends a HPA Cluster in several ways:
		- Is concerned with annotated nodes
		- each cluster must be free of hard obstacles (ie. intraversable nodes). <- implicit if annotations are OK
		- each cluster must have a rectangular or square shape <- but, if these are fscked, we might try to add hard obstacles; need exception
		- each node assigned to the cluster will not be assigned to some other cluster
		- each node in the cluster will not have a larger clearance than the origin node (node at top-left corner of the cluster);
			the clearance value we use for this test is the superset of all single terrain types (currently, kGround|kTrees)
 *
 *  Created by Daniel Harabor on 22/02/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "AnnotatedClusterAbstraction.h"
#include "AnnotatedCluster.h"
#include "clusterAbstraction.h"
#include "AHAConstants.h"
#include "AnnotatedAStar.h"

#include "glUtil.h"
#ifdef OS_MAC
#include "GLUT/glut.h"
#include <OpenGL/gl.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#endif

#include <sstream>


AnnotatedClusterAbstraction::AnnotatedClusterAbstraction(Map* m, AbstractAnnotatedAStar* searchalg, int clustersize)
	: AnnotatedMapAbstraction(m, searchalg)
{
	this->clustersize = clustersize;
	abstractions.push_back(new graph());	
	startid = goalid = -1;
	
	nodesExpanded = nodesTouched = peakMemory = 0;
	searchTime = 0;
}

void AnnotatedClusterAbstraction::addCluster(AnnotatedCluster* ac) 
{ 
	ac->setClusterId(clusters.size()); clusters.push_back(ac); 
} 


AnnotatedCluster* AnnotatedClusterAbstraction::getCluster(int cid)
{
	if(cid < 0 || cid >= clusters.size())
		return 0;
		
	return clusters[cid];
}

void AnnotatedClusterAbstraction::buildClusters(IAnnotatedClusterFactory* acfactory)
{	
	int mapwidth = this->getMap()->getMapWidth();
	int mapheight= this->getMap()->getMapHeight();

	/* need to split the map into fixed-size cluster areas that will form the basis of our abstract graph building later */
	int csize = getClusterSize();
	for(int x=0; x<mapwidth; x+=csize)
		for(int y=0; y<mapheight; y+= csize)
		{	
			int cwidth=csize;
			if(x+cwidth > mapwidth)
				cwidth = mapwidth - x;
			int cheight=csize;
			if(y+cheight > mapheight)
				cheight = mapheight - y;
				
			AnnotatedCluster *ac = /*new AnnotatedCluster(x,y,cwidth,cheight);//*/acfactory->createCluster(x,y,cwidth,cheight);
			addCluster( ac ); // nb: also assigns a new id to cluster
			ac->addNodesToCluster(this);
		}
}


void AnnotatedClusterAbstraction::buildEntrances()
{
	for(int i=0; i<clusters.size(); i++)
	{
		AnnotatedCluster* ac = clusters[i];
		ac->buildEntrances(this);
	}
}

/* NB: relies on path having marked edges. Annotated*AStar and aStarOld all do this; other algorithms may not */
double AnnotatedClusterAbstraction::distance(path* p)
{
	double dist=0;
	
	if(!p)
		return dist;
		
	graph *g = abstractions[p->n->getLabelL(kAbstractionLevel)];
	
	path* next = p->next;
	while(next)
	{
		edge *e = next->n->getMarkedEdge();
		dist+= e->getWeight();
		p = next;
		next = p->next;
	}
	
	return dist;
}

// TODO: remove code duplication from this method
void AnnotatedClusterAbstraction::insertStartAndGoalNodesIntoAbstractGraph(node* start, node* goal) 
	throw(NodeIsNullException, NodeHasNonZeroAbstractionLevelException)
{
	if(start == NULL || goal == NULL)
		throw NodeIsNullException();

	if(start->getLabelL(kAbstractionLevel) != 0 || goal->getLabelL(kAbstractionLevel) != 0)
		throw NodeHasNonZeroAbstractionLevelException();

	nodesExpanded = nodesTouched = peakMemory = 0;
	searchTime = 0;

	node *absstart, *absgoal;
	if(start->getLabelL(kParent) == -1) // not an entrance endpoint (and hence not in abstract graph)	
	{	
		absstart = dynamic_cast<node*>(start->clone());
		absstart->setLabelL(kAbstractionLevel, start->getLabelL(kAbstractionLevel)+1);
		abstractions[1]->addNode(absstart);
		startid = absstart->getNum();
		start->setLabelL(kParent, startid); // reflect new parent
		AnnotatedCluster* startCluster = clusters[start->getParentCluster()];
		startCluster->addParent(absstart, this);
	}
	if(goal->getLabelL(kParent) == -1)
	{
		absgoal = dynamic_cast<node*>(goal->clone());
		absgoal->setLabelL(kAbstractionLevel, goal->getLabelL(kAbstractionLevel)+1);
		abstractions[1]->addNode(absgoal);
		goalid = absgoal->getNum();
		goal->setLabelL(kParent, goalid);
		AnnotatedCluster* goalCluster = clusters[goal->getParentCluster()];
		goalCluster->addParent(absgoal, this);
	}
}

/* Remove any nodes we added into the abstract graph to facilitate some search query. 
	NB:	startid/goalid are actually index positions of the node in the array stored by the graph class.
		When we remove start, our goalid is no longer an index to the goal node (HOG updates values when removing nodes) so we need to get it 
		again before we remove the goal
		
	TODO: merge common code
*/		
void AnnotatedClusterAbstraction::removeStartAndGoalNodesFromAbstractGraph()
{
	graph* g = abstractions[1];
	node* start = NULL;
	node* goal = NULL;

	assert(startid == -1 || (startid == g->getNumNodes()-1) || (startid == g->getNumNodes()-2)); // should be the last 2 nodes added
	assert(goalid == -1 || (goalid == g->getNumNodes()-1) || (goalid == g->getNumNodes()-2));


	if(startid != -1)
		start = g->getNode(startid);
	if(goalid != -1)
		goal = g->getNode(goalid);
		
//	std::cout << "\n erasing..";
	if(start)
	{
		AnnotatedCluster* startCluster = clusters[start->getParentCluster()];
		startCluster->getParents().pop_back(); // always last one added
		
		edge_iterator ei = start->getEdgeIter();
		edge* e = start->edgeIterNext(ei);
		while(e)
		{
			pathCache.erase(e->getUniqueID());
//			std::cout << pathCache.size()<<",";
			g->removeEdge(e);
			delete e;
			ei = start->getEdgeIter();
			e = start->edgeIterNext(ei);
		}
		
		g->removeNode(startid); 
		startid = -1;
		node* originalStart = getNodeFromMap(start->getLabelL(kFirstData), start->getLabelL(kFirstData+1));
		originalStart->setLabelL(kParent, startid);
		delete start;
	}

//	std::cout << " goal...";
	if(goal)
	{
		AnnotatedCluster* goalCluster = clusters[goal->getParentCluster()];
		goalCluster->getParents().pop_back();
		
		edge_iterator ei = goal->getEdgeIter();
		edge* e = goal->edgeIterNext(ei);
		while(e)
		{
			pathCache.erase(e->getUniqueID());
//			std::cout << pathCache.size()<<",";
			g->removeEdge(e);
			delete e;
			ei = goal->getEdgeIter();
			e = goal->edgeIterNext(ei);
		}
		
		g->removeNode(goal->getNum()); 
		goalid = -1;
		node* originalGoal = getNodeFromMap(goal->getLabelL(kFirstData), goal->getLabelL(kFirstData+1));
		originalGoal->setLabelL(kParent, startid);
		delete goal;
	}
}

void AnnotatedClusterAbstraction::addPathToCache(edge* e, path* p)
{
	if(e == NULL || p == NULL)
		return;
	
	pathCache[e->getUniqueID()] = p;
}

void AnnotatedClusterAbstraction::openGLDraw()
{
	GLdouble x, y, z, r, xx, yy, zz, rr;
	Map* map = this->getMap();
	
	graph* g1 = abstractions[1];
	char cl1id[2], cl2id[2];
	node_iterator nodeIter;
	nodeIter = g1->getNodeIter();
	node* n = g1->nodeIterNext(nodeIter);
		
	/* draw lines to represent entrance info. Thick lines for inter-edges, thin lines for intra-edges */
	edge_iterator edgeIter; 
	edgeIter = g1->getEdgeIter();
	edge* e = g1->edgeIterNext(edgeIter);
	while(e)
	{
		node *n1, *n2; 		
		float x1, x2, y1, y2;

		path* thepath = pathCache[e->getUniqueID()];
		if(thepath)
		{
			node* efrom = abstractions[1]->getNode(e->getFrom());
			node* eto = abstractions[1]->getNode(e->getTo());
			
			int halflen = thepath->length()*0.5;
			int cnt=0;
			glLineWidth(1.0f);

			/* draw intra-edges */
			while(thepath->next)
			{
				n1 = thepath->n;
				n2 = thepath->next->n;

				x1 = n1->getLabelF(kXCoordinate);
				x2 = n2->getLabelF(kXCoordinate);
				y1 = n1->getLabelF(kYCoordinate);
				y2 = n2->getLabelF(kYCoordinate);
				
				glColor3f (0.7F, 0.5F, 0.5F);
				glBegin(GL_LINES);
				glVertex3f(x1, y1, -0.010);
				glVertex3f(x2, y2, -0.010);
				glEnd();
				
				if(cnt == halflen)
				{
					int clearance = /*e->getWidth()<*/e->getClearance(kTrees|kGround)/*?e->getWidth():e->getLabelL(kEdgeCapacity)*/;
					glColor3f (0.51F, 1.0F, 0.0F);
					glRasterPos3f(x1, y1+0.01, -0.012);
					glutBitmapCharacter (GLUT_BITMAP_HELVETICA_12, cl1id[0]);	
				}

				cnt++;
				thepath = thepath->next;
			}
		}
		
		if(e->getMarked())
		{
			glLineWidth(3.0f);

			n1 = g1->getNode(e->getFrom());
			n2 = g1->getNode(e->getTo());
			x1 = n1->getLabelF(kXCoordinate);
			x2 = n2->getLabelF(kXCoordinate);
			y1 = n1->getLabelF(kYCoordinate);
			y2 = n2->getLabelF(kYCoordinate);
			
			glColor3f (0.6F, 0.4F, 0.4F);
			glBegin(GL_QUADS);
			glVertex3f(x1-0.01, y1-0.01, -0.010);
			glVertex3f(x1+0.01, y1-0.01, -0.010);
			glVertex3f(x1+0.01, y1+0.01, -0.010);
			glVertex3f(x1-0.01, y1+0.01, -0.010);		
			glEnd();

			glBegin(GL_QUADS);
			glVertex3f(x2-0.01, y2-0.01, -0.010);
			glVertex3f(x2+0.01, y2-0.01, -0.010);
			glVertex3f(x2+0.01, y2+0.01, -0.010);
			glVertex3f(x2-0.01, y2+0.01, -0.010);		
			glEnd();
			
			glColor3f (0.7F, 0.5F, 0.5F);
			glBegin(GL_LINES);
			glVertex3f(x1, y1, -0.010);
			glVertex3f(x2, y2, -0.010);
			glEnd();
			
			int clearance = e->getClearance(kTrees|kGround);
			glColor3f (0.51F, 1.0F, 0.0F);
			glRasterPos3f(x1+((x2-x1)*0.5)-0.01, y1+((y2-y1)*0.5)+0.01, -0.012);
			glutBitmapCharacter (GLUT_BITMAP_HELVETICA_12, clearance);	
		}

		e = g1->edgeIterNext(edgeIter);							
	}
	glLineWidth(1.0f);
}