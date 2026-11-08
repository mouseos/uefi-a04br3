#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# v3 supersedes this branch's v2 idle-order model. The original candidate
# and its strict v2 test remain frozen at bb753417/167b050e.
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('thermal-stock-stop-v3.py')),run_name='__main__')
