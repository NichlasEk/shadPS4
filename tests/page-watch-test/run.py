#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile a supplied real verifier page_manager.cpp, not a copied algorithm."""
import argparse
import pathlib
import subprocess
p=argparse.ArgumentParser()
p.add_argument('source',type=pathlib.Path,help='verifier source checkout')
p.add_argument('output',type=pathlib.Path)
p.add_argument('--expect-eacces',action='store_true')
a=p.parse_args();here=pathlib.Path(__file__).resolve().parent
source=a.source.resolve();output=a.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['c++','-std=c++20','-O1','-pthread','-I'+str(here/'stubs'),'-I'+str(source/'src'),
 str(source/'src/video_core/page_manager.cpp'),str(here/'main.cpp'),'-o',str(output)],check=True)
subprocess.run([str(output)]+(['--expect-eacces'] if a.expect_eacces else []),check=True)
