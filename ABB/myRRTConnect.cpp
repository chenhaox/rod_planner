/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Avishai Sintov */

//#include "ompl/geometric/planners/rrt/RRTConnect.h"
#include "ompl/base/goals/GoalSampleableRegion.h"
#include "ompl/tools/config/SelfConfig.h"

#include "myRRTConnect.h" // Avishai

// Debugging tool
template <class T>
void o(T a) {
	cout << a << endl;
}

ompl::geometric::RRTConnect::RRTConnect(const base::SpaceInformationPtr &si, string PRMfile, int sNg_nn, int iter_bound_num) : base::Planner(si, "RRTConnect"), StateValidityChecker(si)
{
	specs_.recognizedGoal = base::GOAL_SAMPLEABLE_REGION;
	specs_.directed = true;

	maxDistance_ = 0.0;

	Planner::declareParam<double>("range", this, &RRTConnect::setRange, &RRTConnect::getRange, "0.:1.:10000.");
	connectionPoint_ = std::make_pair<base::State*, base::State*>(nullptr, nullptr);

	defaultSettings(); // Avishai

	Range = 2; // As tested

	prmfile = PRMfile;
	sNg_k = sNg_nn;
	iteration_limit = iter_bound_num;

}

ompl::geometric::RRTConnect::~RRTConnect()
{
	freeMemory();
}

void ompl::geometric::RRTConnect::setup()
{
	Planner::setup();
	tools::SelfConfig sc(si_, getName());
	sc.configurePlannerRange(maxDistance_);

	if (!tStart_)
		tStart_.reset(tools::SelfConfig::getDefaultNearestNeighbors<Motion*>(this));
	if (!tGoal_)
		tGoal_.reset(tools::SelfConfig::getDefaultNearestNeighbors<Motion*>(this));
	tStart_->setDistanceFunction(std::bind(&RRTConnect::distanceFunction, this, std::placeholders::_1, std::placeholders::_2)); //activeDistance
	tGoal_->setDistanceFunction(std::bind(&RRTConnect::distanceFunction, this, std::placeholders::_1, std::placeholders::_2)); //distanceFunction
}

void ompl::geometric::RRTConnect::freeMemory()
{
	std::vector<Motion*> motions;

	if (tStart_)
	{
		tStart_->list(motions);
		for (unsigned int i = 0 ; i < motions.size() ; ++i)
		{
			if (motions[i]->state)
				si_->freeState(motions[i]->state);
			delete motions[i];
		}
	}

	if (tGoal_)
	{
		tGoal_->list(motions);
		for (unsigned int i = 0 ; i < motions.size() ; ++i)
		{
			if (motions[i]->state)
				si_->freeState(motions[i]->state);
			delete motions[i];
		}
	}
}

void ompl::geometric::RRTConnect::clear()
{
	Planner::clear();
	sampler_.reset();
	freeMemory();
	if (tStart_)
		tStart_->clear();
	if (tGoal_)
		tGoal_->clear();
	connectionPoint_ = std::make_pair<base::State*, base::State*>(nullptr, nullptr);
}

double ompl::geometric::RRTConnect::activeDistance(const Motion *a, const Motion *b) {

	Vector aa(6), qa(6), qa_dummy(6);
	Vector ab(6), qb(6), qb_dummy(6);

	if (!active_chain) {
		retrieveStateVector(a->state, aa, qa, qa_dummy);
		retrieveStateVector(b->state, ab, qb, qb_dummy);
	}
	else {
		retrieveStateVector(a->state, aa, qa_dummy, qa);
		retrieveStateVector(b->state, ab, qb_dummy, qb);
	}

	double sum = 0;
	for (int i=0; i < aa.size(); i++)
		sum += pow(aa[i]-ab[i], 2);
	for (int i=0; i < qa.size(); i++)
		sum += pow(qa[i]-qb[i], 2);
	return sqrt(sum);
}

ompl::geometric::RRTConnect::Motion* ompl::geometric::RRTConnect::walk_on_PRM(TreeData &tree, Motion *nmotion, Motion *tmotion, int mode)
// tmotion - target 
// nmotion - nearest
// target_index - index of the target subms node in the PRM 
{
	Vector q1_target(6), q2_target(6), ik(2), a_dummy(6);
	
	double maxDistance = 0.2;

	if (!full_shortest_path(nmotion->a_data_in_PRM, tmotion->a_data_in_PRM))
		return nmotion;
	
	// Choose active chain
	active_chain = rand() % 2; // 0 - (q1,a) is the active chain, 1 - (q2,a) is the active chain
	
	bool reach = false;
	walkPRM_reached = false;

	int no_collision_count = 0;
	const int collision_buffer = 2;

	base::State *dstate = si_->allocState();
	int i;
	for (i = 1; i < path.size(); i++) {
		
		// Calculate next angle state
		double d = AngleDistance(nmotion->state, tmotion->state); // Angle distance
		//base::State *dstate = tmotion->state;
		if (d > maxDistance)
		{
			si_->getStateSpace()->interpolate(nmotion->state, tmotion->state, maxDistance / d, dstate);
			reach = false;
		}
		else
			reach = true;
		
		// Pick next 'a' state
		Vector a_target = path[i].a;
		Matrix T_end = path[i].index_indicator ? ms.T_end[path[i].index] : subms.T_end[path[i].index];

		// Move to new 'a' and calculate IK to hold it
		retrieveStateVector(dstate, q1_target, q2_target);

		if (!active_chain) { // q1 active chain
			if (!calc_specific_IK_solution_R1(T_end, q1_target, nmotion->ik_q1_active)) {
				if (!calc_specific_IK_solution_R2(T_end, q2_target, nmotion->ik_q2_active))
					break;
				active_chain = !active_chain;
				q1_target = get_IK_solution_q1();
			}
			else
				q2_target = get_IK_solution_q2();
		}
		else { // q2 active chain
			if (!calc_specific_IK_solution_R2(T_end, q2_target, nmotion->ik_q2_active)) {
				if (!calc_specific_IK_solution_R1(T_end, q1_target, nmotion->ik_q1_active))
					break;
				active_chain = !active_chain;
				q2_target = get_IK_solution_q2();
			}
			else
				q1_target = get_IK_solution_q1();
		}

		// Check IK distance
		if (!check_angles_offset(nmotion->state, q1_target, q2_target, active_chain))
			break;

		// Check collision
		if (!(no_collision_count % collision_buffer)) {
			Matrix P = path[i].index_indicator ? ms.P[path[i].index] : subms.P[path[i].index];
			if (collision_state(P, q1_target, q2_target)) {
				collisionFail++;
				break;
			}
		}
		else
			skipped++;
		no_collision_count++;


		clock_t mStart = clock();
		updateStateVector(dstate, a_target, q1_target, q2_target);
		ik = identify_state_ik(dstate, T_end); 
		
		// create a motion
		Motion *motion = new Motion(si_);
		motion->a_data_in_PRM.index = path[i].index; // Index in subms
		motion->a_data_in_PRM.index_indicator = path[i].index_indicator; // Indicates sub-milestone
		motion->ik_q1_active = ik[0];
		motion->ik_q2_active = ik[1];
		si_->copyState(motion->state, dstate);
		motion->parent = nmotion;
		motion->root = nmotion->root;
		motion->a_chain = active_chain;
		tree->add(motion);

		nmotion = motion;
		create_motion_time += double(clock() - mStart) / CLOCKS_PER_SEC;

		if (i==path.size()-1) { // Reached 'a' state on the roadmap
			walkPRM_reached = true; // Because reached 'a'
			break;
		}
		else if (reach) { // Reached random robot state but not 'a' state in the roadmap
			sampler_->sampleUniform(tmotion->state);
			updateStateVector(tmotion->state, path[path.size()-1].a);
		}
	}
	
	return nmotion;
}

ompl::base::PlannerStatus ompl::geometric::RRTConnect::solve(const base::PlannerTerminationCondition &ptc)
{
	// Load roadmap from .prm file
	startTime = clock();
	load_data(prmfile);
	endTime = clock();
	load_runtime = double(endTime - startTime) / CLOCKS_PER_SEC;
	int ms_size = ms.milestones.size();
	int subms_size = subms.sub_milestones.size();
	OMPL_INFORM("%s: Loaded PRM with %d milestones and %d sub-milestones", getName().c_str(), ms_size, subms_size);

	initiate_log_parameters();
	//setRange(Range);

	Vector a(6), as(6), ag(6), q1(6), q2(6), ik(2);
	ik_start.resize(2);
	ik_goal.resize(2);

	checkValidity(); 
	
	startTime = clock(); // Start measuring planning time

	// ----- Add start and goal to ms list ------
	const base::State *st1 = pis_.nextStart();
	retrieveStateVector(st1, as, q1, q2);
	const base::State *st2 = pis_.nextGoal();
	retrieveStateVector(st2, ag, q1, q2);
	//cout << sNg_k << endl;
	VectorInt new_ms_indices = add_start_n_goal_milestones(as, ag, sNg_k);
	
	clock_t mStart = clock();
	// ----- Add start state ------ 
	//close_chain(st1,0);
	ik = identify_state_ik(st1);
	Motion *motion1 = new Motion(si_);
	si_->copyState(motion1->state, st1);
	motion1->root = motion1->state;
	motion1->a_data_in_PRM.index = new_ms_indices[0]; // Index in subms
	motion1->a_data_in_PRM.index_indicator = 1; // Indicates sub-milestone
	motion1->ik_q1_active = ik[0];
	motion1->ik_q2_active = ik[1];
	motion1->a_chain = 0;
	tStart_->add(motion1);

	o("Real start state: ");
	printStateVector(st1);
	cout << "ik: "; printVector(ik);
	cout << "Start index node: " << new_ms_indices[0] << endl;
	ik_start = ik;
	
	// ----- Add goal state ------ 
	//close_chain(st2,0);
	ik = identify_state_ik(st2);
	Motion *motion2 = new Motion(si_);
	si_->copyState(motion2->state, st2);
	motion2->root = motion2->state;
	motion2->a_data_in_PRM.index = new_ms_indices[1]; // Index in subms
	motion2->a_data_in_PRM.index_indicator = 1; // Indicates sub-milestone
	motion2->ik_q1_active = ik[0];
	motion2->ik_q2_active = ik[1];
	motion2->a_chain = 0;
	tGoal_->add(motion2);

	o("Real goal state: ");
	printStateVector(st2);
	cout << "ik: "; printVector(ik);
	cout << "Goal index node: " << new_ms_indices[1] << endl;
	ik_start = ik;
	
	PlanDistance = distanceFunction(motion1, motion2);

	create_motion_time += double(clock() - mStart) / CLOCKS_PER_SEC;

	// ---------------------------
	
	if (!sampler_)
		sampler_ = si_->allocStateSampler();

	OMPL_INFORM("%s: Starting planning with %d states already in datastructure", getName().c_str(), (int)(tStart_->size() + tGoal_->size()));

	Motion   *rmotion   = new Motion(si_);
	base::State *rstate = rmotion->state;
	bool startTree      = true;
	bool solved         = false;

	int iteration_counter = 1;
	//cout << "Start/goal neighbors will be added every " << iteration_limit << " iterations." <<endl;
	//trees_distance = distanceBetweenTrees(tStart_, tGoal_);
	while (ptc == false)
	{
		clock_t wStart = clock();
		TreeData &tree      = startTree ? tStart_ : tGoal_;
		startTree = !startTree;
		TreeData &otherTree = startTree ? tStart_ : tGoal_;

		/*if (!(iteration_counter % iteration_limit)) {
				if (rng_.uniform01 () < 0.5) {
					cout << "Adding a neighbor to the start node at iteration " << iteration_counter << "..." << endl;
					add_neighbor_to_node(new_ms_indices[0]); // Add neighbor to start node
				}
				else {
					cout << "Adding a neighbor to the goal node at iteration " << iteration_counter << "..." << endl;
					add_neighbor_to_node(new_ms_indices[1]); // Add neighbor to goal node
				}
				//iteration_limit *= 1.5;
		}
		iteration_counter++;*/

		//===================== Plan ==========================

		// Choose random robot angles to move toward
		sampler_->sampleUniform(rstate);
		// Choose random sub-milestone from the roadmap
		int random_index = rng_.uniformInt(0, subms_size);
		updateStateVector(rstate, subms.sub_milestones[random_index]);
		miscTime += double(clock() - wStart) / CLOCKS_PER_SEC;

		clock_t nnStart = clock();
		Motion *nmotion = tree->nearest(rmotion); 
		NN_time += double(clock() - nnStart) / CLOCKS_PER_SEC;

		rmotion->a_data_in_PRM.index = random_index;
		rmotion->a_data_in_PRM.index_indicator = 0;
		Motion* reached_motion = walk_on_PRM(tree, nmotion, rmotion, 1);

		// remember which motion was just added
		Motion *addedMotion = reached_motion;

		nnStart = clock();
		nmotion = otherTree->nearest(reached_motion); 
		NN_time += double(clock() - nnStart) / CLOCKS_PER_SEC;
		reached_motion = walk_on_PRM(otherTree, nmotion, reached_motion, 2);

		//===================== Plan ==========================

		Motion *startMotion = startTree ? reached_motion : addedMotion;
		Motion *goalMotion  = startTree ? addedMotion : reached_motion;

		walkTime += double(clock() - wStart) / CLOCKS_PER_SEC;

		// if we connected the trees in a valid way (start and goal pair is valid)
		if (walkPRM_reached && check_rigid_motion(startMotion->state, goalMotion->state, {startMotion->ik_q1_active, startMotion->ik_q2_active}, {goalMotion->ik_q1_active, goalMotion->ik_q2_active}, startMotion->a_data_in_PRM, goalMotion->a_data_in_PRM)) {
			cout << "Connection point active chain is " << ((!a_chain_connection) ? "q1." : "q2.") << endl;

			// Report computation time
			endTime = clock();
			plan_runtime = double(endTime - startTime) / CLOCKS_PER_SEC;
			cout << "Solved in " << plan_runtime << "s." << endl;

			// it must be the case that either the start tree or the goal tree has made some progress
			// so one of the parents is not nullptr. We go one step 'back' to avoid having a duplicate state
			// on the solution path

			cout << addedMotion << " " << reached_motion << endl;

			if (startMotion->parent)
				startMotion = startMotion->parent;
			else
				goalMotion = goalMotion->parent;

			connectionPoint_ = std::make_pair(startMotion->state, goalMotion->state);

			// construct the solution path
			Motion *solution = startMotion;
			std::vector<Motion*> mpath1;
			while (solution != nullptr)
			{
				mpath1.push_back(solution);
				solution = solution->parent;
			}

			solution = goalMotion;
			std::vector<Motion*> mpath2;
			while (solution != nullptr)
			{
				mpath2.push_back(solution);
				solution = solution->parent;
			}

			cout << "Path from tree 1 size: " << mpath1.size() << ", path from tree 2 size: " << mpath2.size() << endl;
			nodes_in_path = mpath1.size() + mpath2.size();
			nodes_in_trees = tStart_->size() + tGoal_->size();

			PathGeometric *path = new PathGeometric(si_);
			path->getStates().reserve(mpath1.size() + mpath2.size());
			for (int i = mpath1.size() - 1 ; i >= 0 ; --i)
				path->append(mpath1[i]->state);
			for (unsigned int i = 0 ; i < mpath2.size() ; ++i)
				path->append(mpath2[i]->state);
			save2file(mpath1, mpath2);
			pdef_->addSolutionPath(base::PathPtr(path), false, 0.0, getName());
			solved = true;
			break;
		}

		//====================================================
	}
	if (!solved)
	{
		// Report computation time
		endTime = clock();
		plan_runtime = double(endTime - startTime) / CLOCKS_PER_SEC;

		nodes_in_trees = tStart_->size() + tGoal_->size();
	}

	si_->freeState(rstate);
	delete rmotion;

	cout << "  ---------------------------------------\n";
	cout << " |  " << (solved ? "Found solution!" : "Solution not found!") << endl;
	cout << " |  " << "Plan distance: " << PlanDistance << endl; // Distance between nodes 1
	cout << " |  " << "Plan time: " << plan_runtime << "s." << endl; // Overall planning runtime 2
	cout << " |  " << "Load time: " << load_runtime << "s." << endl; //
	cout << " |  " << "Connection time: " << get_add_startNgoal_runtime() << "s." << endl; //
	cout << " |  " << "# of connection to S/G: " << get_add_startNgoal_counter() << "." << endl; //
	cout << " |  " << "Query time: " << get_query_time() << "s." << endl;
	cout << " |  " << "ODE time: " << get_odes_time() << "s." << endl;
	cout << " |  " << "IK time: " << get_IK_time() << "s." << endl;
	cout << " |  " << "Collision time: " << get_collisionCheck_time() << "s." << endl; // How many collision checks? 7
	cout << " |  " << "# collision checks: " << get_collisionCheck_counter() << endl; // Collision check computation time 8
	cout << " |  " << "Skipped collision checks: " << skipped << endl;
	cout << "  ---------------------------------------\n";

	OMPL_INFORM("%s: Created %u states (%u start + %u goal)", getName().c_str(), tStart_->size() + tGoal_->size(), tStart_->size(), tGoal_->size());

	final_solved = solved;
	LogPerf2file();

	return solved ? base::PlannerStatus::EXACT_SOLUTION : base::PlannerStatus::TIMEOUT;
}


double ompl::geometric::RRTConnect::distanceBetweenTrees(TreeData &tree1, TreeData &tree2) {

	std::vector<Motion*> motions;
	tree1->list(motions);

	Motion *nmotion;
	double minD = 1e10, curD;
	for (unsigned int i = 0 ; i < motions.size() ; ++i)
	{
		nmotion = tree2->nearest(motions[i]);
		curD = distanceFunction(nmotion, motions[i]);
		if (curD < minD) {
			minD = curD;
		}
	}
	return minD;
}

void ompl::geometric::RRTConnect::getPlannerData(base::PlannerData &data) const
{
	Planner::getPlannerData(data);

	std::vector<Motion*> motions;
	if (tStart_)
		tStart_->list(motions);

	for (unsigned int i = 0 ; i < motions.size() ; ++i)
	{
		if (motions[i]->parent == nullptr)
			data.addStartVertex(base::PlannerDataVertex(motions[i]->state, 1));
		else
		{
			data.addEdge(base::PlannerDataVertex(motions[i]->parent->state, 1),
					base::PlannerDataVertex(motions[i]->state, 1));
		}
	}

	motions.clear();
	if (tGoal_)
		tGoal_->list(motions);

	for (unsigned int i = 0 ; i < motions.size() ; ++i)
	{
		if (motions[i]->parent == nullptr)
			data.addGoalVertex(base::PlannerDataVertex(motions[i]->state, 2));
		else
		{
			// The edges in the goal tree are reversed to be consistent with start tree
			data.addEdge(base::PlannerDataVertex(motions[i]->state, 2),
					base::PlannerDataVertex(motions[i]->parent->state, 2));
		}
	}

	// Add the edge connecting the two trees
	data.addEdge(data.vertexIndex(connectionPoint_.first), data.vertexIndex(connectionPoint_.second));
}

void ompl::geometric::RRTConnect::save2file(vector<Motion*> mpath1, vector<Motion*> mpath2) {

	cout << "Logging path to files..." << endl;
	//return;

	Vector a(6), q1(6), q2(6);
	int active_chain, ik_sol;

	{
		// Open a_path file
		std::ofstream myfile, afile, pfile, ai;
		myfile.open("./path/robot_paths.txt");
		afile.open("./path/afile.txt");
		pfile.open("./path/rod_path.txt");
		ai.open("./path/active_ik.txt");

		myfile << mpath1.size() + mpath2.size() << endl;
		pfile << (mpath1.size() + mpath2.size())*501 << endl;

		Vector temp;
		for (int i = mpath1.size() - 1 ; i >= 0 ; --i) {
			retrieveStateVector(mpath1[i]->state, a, q1, q2);
			//cout << mpath1[i]->a_chain << " " << mpath1[i]->ik_q1_active << " " << mpath1[i]->ik_q2_active << endl;
			for (int j = 0; j<6; j++) {
				myfile << q1[j] << " ";
				afile << a[j] << " ";
			}
			for (int j = 0; j<6; j++) {
				myfile << q2[j] << " ";
			}
			myfile << endl;
			afile << endl;

			rod_solve(a);
			// Log points on rod to file
			for (int k = 0; k < get_Points_on_Rod(); k++) {
				temp = getP(k);
				pfile << temp[0] << " " << temp[1] << " "  << temp[2] << endl;
			}
			pfile << endl;

			ai << mpath1[i]->a_chain << " " << mpath1[i]->ik_q1_active << " " << mpath1[i]->ik_q2_active << endl;
		}
		for (unsigned int i = 0 ; i < mpath2.size() ; ++i) {
			retrieveStateVector(mpath2[i]->state, a, q1, q2);
			//cout << mpath2[i]->a_chain << " " << mpath2[i]->ik_q1_active << " " << mpath2[i]->ik_q2_active << endl;
			for (int j = 0; j<6; j++) {
				myfile << q1[j] << " ";
				afile << a[j] << " ";
			}
			for (int j = 0; j<6; j++) {
				myfile << q2[j] << " ";
			}
			myfile << endl;
			afile << endl;

			rod_solve(a);
			// Log points on rod to file
			for (int k = 0; k < get_Points_on_Rod(); k++) {
				temp = getP(k);
				pfile << temp[0] << " " << temp[1] << " "  << temp[2] << endl;
			}
			pfile << endl;

			ai << mpath2[i]->a_chain << " " << mpath2[i]->ik_q1_active << " " << mpath2[i]->ik_q2_active << endl;
		}
		myfile.close();
		afile.close();
		pfile.close();
		ai.close();
	}
}


void ompl::geometric::RRTConnect::LogPerf2file() {

	std::ofstream myfile;
	myfile.open("./path/perf_log.txt"); // <====== Change it before profiling.

	myfile << final_solved << endl;	// Solved? 1
	myfile << PlanDistance << endl; // Distance between nodes 2
	myfile << plan_runtime << endl; // Overall planning runtime 3
	myfile << load_runtime << endl; // 4
	myfile << get_add_startNgoal_runtime() << endl; // 5
	myfile << get_query_time() << endl; // 6
	myfile << get_odes_counter() << endl; // How many ode's checks? 7
	myfile << get_valid_odes_counter() << endl; // How many ode's checks and returned valid? 8
	myfile << get_odes_time() << endl; // ODE computation time 9
	myfile << get_IK_counter() << endl; // How many IK checks? 10
	myfile << get_IK_time() << endl; // IK computation time 11
	myfile << get_collisionCheck_counter() << endl; // How many collision checks? 12
	myfile << get_collisionCheck_time() << endl; // Collision check computation time 13
	myfile << get_isValid_counter() << endl; // How many nodes checked 14
	myfile << nodes_in_path << endl; // Nodes in path 15
	myfile << nodes_in_trees << endl; // 16
	myfile << get_add_startNgoal_counter() << endl; // 17

	myfile << collisionFail << endl;
	myfile << walkTime << endl;
	myfile << create_motion_time << endl;
	myfile << check_rigid_time << endl;
	myfile << NN_time << endl;
	myfile << miscTime << endl;

	myfile.close();
}
