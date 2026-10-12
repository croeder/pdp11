"""Run the tests from the repository root, where config_default.json and
source/ are.  Done at import, because some test classes build a PDP11 in
their class body, while pytest collects them."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.chdir(os.path.dirname(HERE))
