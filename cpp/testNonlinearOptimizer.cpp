/** 
 * @file    testNonlinearOptimizer.cpp
 * @brief   Unit tests for NonlinearOptimizer class
 * @author  Frank Dellaert
 */

#include <iostream>
using namespace std;

#include <boost/assign/std/list.hpp> // for operator +=
using namespace boost::assign;

#include <CppUnitLite/TestHarness.h>

#include <boost/shared_ptr.hpp>
using namespace boost;

#define GTSAM_MAGIC_KEY

#include "Matrix.h"
#include "Ordering.h"
#include "smallExample.h"
#include "pose2SLAM.h"
#include "GaussianFactorGraph.h"
#include "NoiseModel.h"

// template definitions
#include "NonlinearFactorGraph-inl.h"
#include "NonlinearOptimizer-inl.h"
#include "SubgraphPreconditioner-inl.h"

using namespace gtsam;
using namespace example;

typedef NonlinearOptimizer<Graph,Config> Optimizer;

/* ************************************************************************* */
TEST( NonlinearOptimizer, delta )
{
	shared_ptr<Graph> fg(new Graph(
			createNonlinearFactorGraph()));
	Optimizer::shared_config initial = sharedNoisyConfig();

	// Expected configuration is the difference between the noisy config
	// and the ground-truth config. One step only because it's linear !
	VectorConfig expected;
	Vector dl1(2);
	dl1(0) = -0.1;
	dl1(1) = 0.1;
	expected.insert("l1", dl1);
	Vector dx1(2);
	dx1(0) = -0.1;
	dx1(1) = -0.1;
	expected.insert("x1", dx1);
	Vector dx2(2);
	dx2(0) = 0.1;
	dx2(1) = -0.2;
	expected.insert("x2", dx2);

	// Check one ordering
	shared_ptr<Ordering> ord1(new Ordering());
	*ord1 += "x2","l1","x1";
	Optimizer::shared_solver solver;
	solver = Optimizer::shared_solver(new Optimizer::solver(ord1));
	Optimizer optimizer1(fg, initial, solver);
	VectorConfig actual1 = optimizer1.linearizeAndOptimizeForDelta();
	CHECK(assert_equal(actual1,expected));

	// Check another
	shared_ptr<Ordering> ord2(new Ordering());
	*ord2 += "x1","x2","l1";
	solver = Optimizer::shared_solver(new Optimizer::solver(ord2));
	Optimizer optimizer2(fg, initial, solver);
	VectorConfig actual2 = optimizer2.linearizeAndOptimizeForDelta();
	CHECK(assert_equal(actual2,expected));

	// And yet another...
	shared_ptr<Ordering> ord3(new Ordering());
	*ord3 += "l1","x1","x2";
	solver = Optimizer::shared_solver(new Optimizer::solver(ord3));
	Optimizer optimizer3(fg, initial, solver);
	VectorConfig actual3 = optimizer3.linearizeAndOptimizeForDelta();
	CHECK(assert_equal(actual3,expected));
}

/* ************************************************************************* */
TEST( NonlinearOptimizer, iterateLM )
{
	// really non-linear factor graph
  shared_ptr<Graph> fg(new Graph(
			createReallyNonlinearFactorGraph()));

	// config far from minimum
	Point2 x0(3,0);
	boost::shared_ptr<Config> config(new Config);
	config->insert(simulated2D::PoseKey(1), x0);

	// ordering
	shared_ptr<Ordering> ord(new Ordering());
	ord->push_back("x1");

	// create initial optimization state, with lambda=0
	Optimizer::shared_solver solver(new Optimizer::solver(ord));
	Optimizer optimizer(fg, config, solver, 0.);

	// normal iterate
	Optimizer iterated1 = optimizer.iterate();

	// LM iterate with lambda 0 should be the same
	Optimizer iterated2 = optimizer.iterateLM();

	// Try successive iterates. TODO: ugly pointers, better way ?
	Optimizer *pointer = new Optimizer(iterated2);
	for (int i=0;i<10;i++) {
		Optimizer* newOptimizer = new Optimizer(pointer->iterateLM());
		delete pointer;
		pointer = newOptimizer;
	}
	delete(pointer);

	CHECK(assert_equal(*iterated1.config(), *iterated2.config(), 1e-9));
}

/* ************************************************************************* */
TEST( NonlinearOptimizer, optimize )
{
  shared_ptr<Graph> fg(new Graph(
			createReallyNonlinearFactorGraph()));

	// test error at minimum
	Point2 xstar(0,0);
	Config cstar;
	cstar.insert(simulated2D::PoseKey(1), xstar);
	DOUBLES_EQUAL(0.0,fg->error(cstar),0.0);

	// test error at initial = [(1-cos(3))^2 + (sin(3))^2]*50 =
	Point2 x0(3,3);
	boost::shared_ptr<Config> c0(new Config);
	c0->insert(simulated2D::PoseKey(1), x0);
	DOUBLES_EQUAL(199.0,fg->error(*c0),1e-3);

	// optimize parameters
	shared_ptr<Ordering> ord(new Ordering());
	ord->push_back("x1");
	double relativeThreshold = 1e-5;
	double absoluteThreshold = 1e-5;

	// initial optimization state is the same in both cases tested
	Optimizer::shared_solver solver(new Optimizer::solver(ord));
	Optimizer optimizer(fg, c0, solver);

	// Gauss-Newton
	Optimizer actual1 = optimizer.gaussNewton(relativeThreshold,
			absoluteThreshold);
	DOUBLES_EQUAL(0,fg->error(*(actual1.config())),1e-3);

	// Levenberg-Marquardt
	Optimizer actual2 = optimizer.levenbergMarquardt(relativeThreshold,
			absoluteThreshold, Optimizer::SILENT);
	DOUBLES_EQUAL(0,fg->error(*(actual2.config())),1e-3);
}

/* ************************************************************************* */
TEST( NonlinearOptimizer, Factorization )
{
	typedef NonlinearOptimizer<Pose2Graph, Pose2Config, GaussianFactorGraph, Factorization<Pose2Graph, Pose2Config> > Optimizer;

	boost::shared_ptr<Pose2Config> config(new Pose2Config);
	config->insert(1, Pose2(0.,0.,0.));
	config->insert(2, Pose2(1.5,0.,0.));

	boost::shared_ptr<Pose2Graph> graph(new Pose2Graph);
	graph->addPrior(1, Pose2(0.,0.,0.), sharedSigma(3, 1e-10));
	graph->addConstraint(1,2, Pose2(1.,0.,0.), sharedSigma(3, 1));

	boost::shared_ptr<Ordering> ordering(new Ordering);
	ordering->push_back(Pose2Config::Key(1));
	ordering->push_back(Pose2Config::Key(2));
	Optimizer::shared_solver solver(new Factorization<Pose2Graph, Pose2Config>(ordering));

	Optimizer optimizer(graph, config, solver);
	Optimizer optimized = optimizer.iterateLM();

	Pose2Config expected;
	expected.insert(1, Pose2(0.,0.,0.));
	expected.insert(2, Pose2(1.,0.,0.));
	CHECK(assert_equal(expected, *optimized.config(), 1e-5));
}

/* ************************************************************************* */
TEST( NonlinearOptimizer, SubgraphPCG )
{
	typedef NonlinearOptimizer<Pose2Graph, Pose2Config, SubgraphPreconditioner, SubgraphPCG<Pose2Graph, Pose2Config> > Optimizer;

	boost::shared_ptr<Pose2Config> config(new Pose2Config);
	config->insert(1, Pose2(0.,0.,0.));
	config->insert(2, Pose2(1.5,0.,0.));

	boost::shared_ptr<Pose2Graph> graph(new Pose2Graph);
	graph->addPrior(1, Pose2(0.,0.,0.), sharedSigma(3, 1e-10));
	graph->addConstraint(1,2, Pose2(1.,0.,0.), sharedSigma(3, 1));

	double relativeThreshold = 1e-5;
	double absoluteThreshold = 1e-5;
	Optimizer::shared_solver solver(new SubgraphPCG<Pose2Graph, Pose2Config>(*graph, *config));
	Optimizer optimizer(graph, config, solver);
	Optimizer optimized = optimizer.gaussNewton(relativeThreshold, absoluteThreshold, Optimizer::SILENT);

	Pose2Config expected;
	expected.insert(1, Pose2(0.,0.,0.));
	expected.insert(2, Pose2(1.,0.,0.));
	CHECK(assert_equal(expected, *optimized.config(), 1e-5));
}

/* ************************************************************************* */
int main() {
	TestResult tr;
	return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
