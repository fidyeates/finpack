from distutils.core import setup, Extension

finstruct = Extension('finstruct', ['finstruct/finstruct.c'])

setup(
    name="finpack",
    version="0.2r2",
    package_dir="",
    packages=['finpack'],
    scripts=[],
    ext_modules=[finstruct],

    install_requires=[],
    package_data={},

    # metadata for upload to PyPI
    author="fin.yeates",
    author_email="",
    description="Lightweight Python Serialization Library",
    license="PSF",
    keywords="finpack, struct, serialization",
    url="",  # project home page, if any
)
