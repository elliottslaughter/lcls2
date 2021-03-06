from setuptools import setup, find_packages
import versioneer

setup(
       name = 'psdaq',
       license = 'LCLS II',
       description = 'LCLS II DAQ package',

       version=versioneer.get_version(),
       cmdclass=versioneer.get_cmdclass(),
       packages = find_packages(),

       scripts = ['psdaq/procmgr/procmgr','psdaq/procmgr/procstat'],

       entry_points={
            'console_scripts': [
                'collection = psdaq.control.collection:main',
                'partca = psdaq.cas.partca:main',
                'partcas = psdaq.cas.partcas:main',
                'modcas = psdaq.cas.modcas:main',
                'xpmca = psdaq.cas.xpmca:main',
                'deadca = psdaq.cas.deadca:main',
                'dtica = psdaq.cas.dtica:main',
                'dticas = psdaq.cas.dticas:main',
                'hsdca = psdaq.cas.hsdca:main',
                'hsdcas = psdaq.cas.hsdcas:main',
             ]
       },
)
