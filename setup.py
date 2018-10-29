import os
from distutils.core import setup, Extension
from Cython.Build import cythonize
import numpy

# set the compiler
os.environ["CC"] = "g++-7"

extensions = dict()

def add_extension(name, **kwargs):
    global extensions
    assert isinstance(name, str) and "/" not in name
    assert name not in extensions

    source = name.split(".")[-1] + ".pyx"
    if "language" not in kwargs:
        kwargs["language"] = "c++"
    if "extra_compile_args" in kwargs:
        kwargs["extra_compile_args"] += ["-std=c++11", "-O3"]
    else:
        kwargs["extra_compile_args"] = ["-std=c++11", "-O3"]

    extensions[name] = Extension(
        name,
        sources=[source],
        **kwargs
    )

# configure the extensions
add_extension('pattern_matching.pattern_matcher')
add_extension('pattern_matching.segmenter')

# setup
setup(
    ext_modules=cythonize(extensions.values()),
    packages=extensions.keys(),
)
