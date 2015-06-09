from setuptools import setup, find_packages
setup(
    name="finpack",
    version="0.1r1",
    package_dir={'': 'src'},
    packages=['finpack'],
    scripts=[],

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