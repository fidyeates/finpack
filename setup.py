from distutils.core import setup, Extension
from setuptools import setup, find_packages

finstruct = Extension('finstruct', sources=['src/finstruct/finstruct-dev.c'])
#finstruct_dev = Extension('finstruct', sources=['src/finstruct/finstruct-dev.c'])

setup(
    name="finpack",
    version="0.2",
    package_dir={'': 'src'},
    packages=['finpack'],
    scripts=[],
    ext_modules=[finstruct],

    install_requires=[],
    package_data={},

    # metadata for upload to PyPI
    author="fin.yeates",
    author_email="",
    description="Lightweight Serialization Library",
    license="PSF",
    keywords="finpack",
    url="",  # project home page, if any
)