import os, shutil
import subprocess
import sys, os
sys.path = [os.path.abspath(os.path.dirname(__file__))] + sys.path
from xtc import xtc
from det import det

import hashlib
from psana import DataSource
import dgramCreate as dc


class Test:
    @classmethod
    def setup_class(cls):
        subprocess.call(['xtcwriter'])

    def test_cydgram(self):
        fname = 'data_cydgram.xtc2'
        try:
            os.remove(fname)
        except:
            pass

        # read in an old xtc file
        ds = DataSource('data.xtc2')
        for run in ds.runs():
            pyxtc = dc.parse_xtc(run.configs[0])
            for evt in run.events():
                pyxtc.parse_event(evt)

        # put the dictionaries in a new xtc file
        cydgram = dc.CyDgram()
        pyxtc.write_events(fname, cydgram)

        # test that the values in the new file are correct
        xtc(fname, nsegments=1, cydgram=True)

    def test_xtcdata(self):
        xtc('data.xtc2', nsegments=2)

    def setup_input_files(self):
        subprocess.call(['xtcwriter','-f','data-ts.xtc2', '-t']) # Mona FIXME: writing seq in xtcwriter broke dgramCreate
        subprocess.call(['smdwriter','-f','data-ts.xtc2'])
        subprocess.call(['epicswriter','-f','data-ts.xtc2'])
        tmp_dir = os.path.join('.tmp','smalldata')
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir,ignore_errors=True)
        if not os.path.exists(tmp_dir):
            os.makedirs(tmp_dir)
        
        shutil.copy('data-ts.xtc2',os.path.join('.tmp','data-r0001-s00.xtc2')) # Ex. of run 1 with two detectors s00 and s01
        shutil.copy('data-ts.xtc2',os.path.join('.tmp','data-r0001-s01.xtc2'))
        
        shutil.copy('epics.xtc2',os.path.join('.tmp','data-r0001-e00.xtc2')) # Epics data with streamId 1
        
        subprocess.call(['epicswriter','-f','data-ts.xtc2', '-s', '2']) # Epics data with streamId 2
        shutil.copy('epics.xtc2',os.path.join('.tmp','data-r0001-e01.xtc2'))
        
        shutil.copy('smd.xtc2',os.path.join(tmp_dir,'data-r0001-s00.smd.xtc2'))
        shutil.copy('smd.xtc2',os.path.join(tmp_dir,'data-r0001-s01.smd.xtc2'))
        
        shutil.copy('data-ts.xtc2',os.path.join(tmp_dir,'data-r0002-s00.smd.xtc2'))
        shutil.copy('data-ts.xtc2',os.path.join(tmp_dir,'data-r0002-s01.smd.xtc2'))

        shutil.copy('smd.xtc2', os.path.join(tmp_dir, 'data.smd.xtc2')) # FIXME: chuck's hack to fix nosetests
        shutil.copy('smd.xtc2',os.path.join(tmp_dir,'data_1.smd.xtc2')) # FIXME: chuck's hack to fix nosetests

    def test_serial(self):
        self.setup_input_files()

        loop_based = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'user_loops.py')
        subprocess.check_call(['python',loop_based])

        callback_based = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'user_callbacks.py')
        subprocess.check_call(['python',callback_based])
        
        callback_based = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'parallelreader.py')
        subprocess.check_call(['python',callback_based])
        
        callback_based = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'ds.py')
        subprocess.check_call(['python',callback_based])

    def test_legion(self):
        self.setup_input_files()

        # Legion script mode.
        env = dict(list(os.environ.items()) + [
            ('PS_PARALLEL', 'legion'),
        ])
        callback_based = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'user_callbacks.py')
        subprocess.check_call(['legion_python', callback_based, '-ll:py', '1'], env=env)

        # Legion module mode.
        python_path = os.environ.get('PYTHONPATH', '').split(':')
        python_path.append(os.path.dirname(os.path.realpath(__file__)))
        env.update({
            'PYTHONPATH': ':'.join(python_path),
        })
        subprocess.check_call(['legion_python', 'user_callbacks', '-ll:py', '1'], env=env)

    def test_run_pickle(self):
        # Test that run is pickleable
        self.setup_input_files()

        import run_pickle
        run_pickle.test_run_pickle()

    def test_legion_pickle(self):
        # Again, in Legion
        self.setup_input_files()

        python_path = os.environ.get('PYTHONPATH', '').split(':')
        python_path.append(os.path.dirname(os.path.realpath(__file__)))
        env = dict(list(os.environ.items()) + [
            ('PYTHONPATH', ':'.join(python_path)),
            ('PS_PARALLEL', 'legion'),
        ])
        subprocess.check_call(['legion_python', 'run_pickle', '-ll:py', '1'], env=env)

    def test_det(self):
        det()

