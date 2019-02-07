# NOTE:
# To test on 'real' bigdata: 
# xtc_dir = "/reg/d/psdm/xpp/xpptut15/scratch/mona/test"
# >bsub -n 64 -q psfehq -o log.txt mpirun python user_loops.py

# cpo found this on the web as a way to get mpirun to exit when
# one of the ranks has an exception
import sys
# Global error handler
def global_except_hook(exctype, value, traceback):
    sys.stderr.write("except_hook. Calling MPI_Abort().\n")
    # NOTE: mpi4py must be imported inside exception handler, not globally.
    # In chainermn, mpi4py import is carefully delayed, because
    # mpi4py automatically call MPI_Init() and cause a crash on Infiniband environment.
    import mpi4py.MPI
    mpi4py.MPI.COMM_WORLD.Abort(1)
    sys.__excepthook__(exctype, value, traceback)
sys.excepthook = global_except_hook

import os
from psana import DataSource

def filter_fn(evt):
    return True

xtc_dir = os.path.join(os.getcwd(),'.tmp')

# Usecase 1a : two iterators with filter function
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), filter=filter_fn)
#beginJobCode
for run in ds.runs():
    det = run.Detector('xppcspad')
    #beginRunCode
    for evt in run.events():
        assert det.raw.raw(evt).shape == (18,)
    #endRunCode
#endJobCode

# Usecase 1b : two iterators without filter function
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir))
for run in ds.runs():
    det = run.Detector('xppcspad')
    for evt in run.events():
        assert det.raw.raw(evt).shape == (18,)

# Usecase 2: one iterator 
for evt in ds.events():
    pass

# Todo: MONA add back configUpdates()
# Usecase#3: looping through configs
#for run in ds.runs():
#    for configUpdate in run.configUpdates():
#        for config in configUpdate.events():