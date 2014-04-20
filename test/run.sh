#!/bin/sh

export PYTHONPATH=/usr/lib/python2.7/dist-packages/yaml:/usr/lib/python2.7/

/usr/bin/env python2 <<__EOB__
import sys
if sys.hexversion < 0x02060000 or sys.hexversion >= 0x03000000:
    sys.stderr.write("ERROR: test harness must use python >= 2.6 but not 3.x\n")
    sys.exit(1)
else:
    sys.exit(0)
__EOB__

[ $? -eq 0 ] && ./test-run.py $*

