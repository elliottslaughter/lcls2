import os
import sys
import numpy as np
from setuptools import setup, Extension #, find_packages


arg = [arg for arg in sys.argv if arg.startswith('--xtcdata')]
if not arg:
    raise Exception('Parameter --xtcdata is missing')
xtcdata = arg[0].split('=')[1]
sys.argv.remove(arg[0])

dgram_module = Extension('psana.dgram',
                         sources = ['src/dgram.cc'],
                         libraries = ['xtcdata'],
                         include_dirs = [np.get_include(), os.path.join(xtcdata, 'include')],
                         library_dirs = [os.path.join(xtcdata, 'lib')],
                         extra_link_args = ['-Wl,-rpath='+ os.path.abspath(os.path.join(xtcdata, 'lib'))],
                         extra_compile_args=['-std=c++11'])

setup(name = 'psana',
       version = '0.1',
       license = 'LCLS II',
       description = 'LCLS II analysis package',
       install_requires=[
         'numpy',
       ],
       #packages = find_packages(),
       packages = ['psana',
                   'psana.detector',
                   'psana.pscalib.calib',
                   'psana.pscalib.geometry',
                   'psana.pyalgos.generic',
                   'psana.graphqt',
       ],
       #package_dir={
       #             'psana.psana.pscalib.calib'    : 'calib',
       #             'psana.psana.pscalib.geometry' : 'geometry',
       #             'psana.psana.pyalgos.generic'  : 'generic',
       #             'psana.psana.graphqt'          : 'graphqt',
       #},
       include_package_data = True,
       package_data={'graphqt': ['data/icons/*.png','data/icons/*.gif'],
       },

       #cmdclass = {'build': dgram_build, 'build_ext': dgram_build_ext},
       ext_modules = [dgram_module],
       entry_points={
            'console_scripts': [
                'convert_npy_to_txt  = psana.pyalgos.app.convert_npy_to_txt:do_main',
                'convert_txt_to_npy  = psana.pyalgos.app.convert_txt_to_npy:do_main',
                'merge_mask_ndarrays = psana.pyalgos.app.merge_mask_ndarrays:do_main',
                'merge_max_ndarrays  = psana.pyalgos.app.merge_max_ndarrays:do_main',
                'cdb                 = psana.pscalib.app.cdb:cdb_cli',
                'timeconverter       = psana.graphqt.app.timeconverter:timeconverter',
                'calibman            = psana.graphqt.app.calibman:calibman_cli',
             ]
       },
)

from Cython.Build import cythonize
ext = Extension("peakFinder",
                packages=['psana.peakfinder',],
                sources=["psana/peakFinder/peakFinder.pyx", "../psalg/psalg/src/PeakFinderAlgos.cpp", "../psalg/psalg/src/LocalExtrema.cpp"],
                language="c++",
                extra_compile_args=['-std=c++11'],
                include_dirs=[np.get_include(),
                              "../psalg/psalg/include/Array.hh",
                              "../install/include",
                              "../psalg/psalg/include/PeakFinderAlgos.h",
                              "../psalg/psalg/include/LocalExtrema.h"]
)

setup(name="peakFinder",
      ext_modules=cythonize(ext))

'''
from setuptools.command.build_ext import build_ext
class dgram_build_ext(build_ext):
    user_options = build_ext.user_options
    user_options.extend([('xtcdata=', None, 'base folder of xtcdata installation')])

    def initialize_options(self):
        build_ext.initialize_options(self)
        self.xtcdata = None
    
    def finalize_options(self):
        build_ext.finalize_options(self)
        if self.xtcdata is not None:
            for ext in self.extensions:
                ext.library_dirs.append(os.path.join(self.xtcdata, 'lib'))
                ext.include_dirs.append(os.path.join(self.xtcdata, 'include'))
                ext.extra_link_args.append('-Wl,-rpath='+ os.path.join(self.xtcdata, 'lib'))
        else:
            print('missing')
            #raise Exception("Parameter --xtcdata is missing")
        print(self.extensions)
'''   