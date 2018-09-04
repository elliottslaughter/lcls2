#!/bin/bash

set -e

source activate $CONDA_ENV

cd "$(dirname "${BASH_SOURCE[0]}")"
./build_travis.sh -p install
export PYTHONPATH=$PWD/install/lib/python$PYVER/site-packages
if [[ $PYVER == 2.7 ]]; then
    nosetests psana/psana/tests
elif [[ $PYVER == 3.* ]]; then
    pytest psana/psana/tests
fi
