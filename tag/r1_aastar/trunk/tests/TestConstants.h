/*
 *  TestConstants.h
 *  hog
 *
 *  Created by Daniel Harabor on 11/01/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

/* consts etc */

#ifndef TESTCONSTANTS_H
#define TESTCONSTANTS_H

#include "map.h"

using namespace std;


const string maplocation = "/Users/dharabor/src/ahastar/maps/local/demo.map"; // testmap
const string tslocation = "test.scenario"; // test scenario file
const string badmap = "/Users/dharabor/src/ahastar/maps/local/demo.map";
/* duplicated constants from AHAConstants.h; two copies now. one for testing, one for the app itself */
/*const int NUMTERRAINS = 3;
const int NUMAGENTSIZES = 2;
const int validterrains[NUMTERRAINS] = {kGround, kTrees, (kGround|kTrees)};
const int agentsizes[NUMAGENTSIZES] = {1, 2};
const int MAXAGENTSIZE=2;
const int MINAGENTSIZE=1;
*/
#endif