import unittest
from gtsam import *
from math import *
import numpy as np
from gtsam_utils.circlePose3 import * 

class TestPose3SLAMExample(unittest.TestCase):

    def test_Pose3SLAMExample(self):
        # Create a hexagon of poses
        hexagon = circlePose3(6, 1.0)
        p0 = hexagon.atPose3(0)
        p1 = hexagon.atPose3(1)

        # create a Pose graph with one equality constraint and one measurement
        fg = NonlinearFactorGraph()
        fg.add(NonlinearEqualityPose3(0, p0))
        delta = p0.between(p1)
        covariance = noiseModel_Diagonal.Sigmas(
            np.array([0.05, 0.05, 0.05, 5. * pi / 180, 5. * pi / 180, 5. * pi / 180]))
        fg.add(BetweenFactorPose3(0, 1, delta, covariance))
        fg.add(BetweenFactorPose3(1, 2, delta, covariance))
        fg.add(BetweenFactorPose3(2, 3, delta, covariance))
        fg.add(BetweenFactorPose3(3, 4, delta, covariance))
        fg.add(BetweenFactorPose3(4, 5, delta, covariance))
        fg.add(BetweenFactorPose3(5, 0, delta, covariance))

        # Create initial config
        initial = Values()
        s = 0.10
        initial.insertPose3(0, p0)
        initial.insertPose3(1, hexagon.atPose3(1).retract(s * np.random.randn(6, 1)))
        initial.insertPose3(2, hexagon.atPose3(2).retract(s * np.random.randn(6, 1)))
        initial.insertPose3(3, hexagon.atPose3(3).retract(s * np.random.randn(6, 1)))
        initial.insertPose3(4, hexagon.atPose3(4).retract(s * np.random.randn(6, 1)))
        initial.insertPose3(5, hexagon.atPose3(5).retract(s * np.random.randn(6, 1)))

        # optimize
        optimizer = LevenbergMarquardtOptimizer(fg, initial)
        result = optimizer.optimizeSafely()

        pose_1 = result.atPose3(1)
        self.assertTrue(pose_1.equals(p1, 1e-4))

if __name__ == "__main__":
    unittest.main()