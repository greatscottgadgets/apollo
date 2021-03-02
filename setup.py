import os
import sys

from setuptools import setup, find_packages


# Provide our default install requirements.
install_requirements = [
    'pyusb',
    'pyvcd~=0.1.7',
]

# On ReadTheDocs don't enforce requirements; we'll use requirements.txt
# to provision the documentation builder.
if os.environ.get('READTHEDOCS') == 'True':
    install_requirements = []


setup(

    # Vitals
    name='apollo-fpga',
    license='BSD',
    url='https://github.com/greatscottgadgets/apollo',
    author='Katherine J. Temkin',
    author_email='ktemkin@greatscottgadgets.com',
    description='host tools for Apollo FPGA debug controllers',
    use_scm_version= {
        "root": '..',
        "relative_to": __file__,
        "version_scheme": "guess-next-dev",
        "local_scheme": lambda version : version.format_choice("+{node}", "+{node}.dirty"),
        "fallback_version": "r0.0"
    },

    # Imports / exports / requirements.
    platforms='any',
    packages=find_packages(),
    include_package_data=True,
    python_requires="~=3.7",
    install_requires=install_requirements,
    setup_requires=['setuptools', 'setuptools_scm'],
    entry_points= {
        'console_scripts': [
            'apollo = apollo_fpga.commands.cli:main',
        ],
    },

    # Metadata
    classifiers = [
        'Programming Language :: Python',
        'Development Status :: 1 - Planning',
        'Natural Language :: English',
        'Environment :: Console',
        'Environment :: Plugins',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: BSD License',
        'Operating System :: OS Independent',
        'Topic :: Scientific/Engineering',
        'Topic :: Security',
        ],
)
