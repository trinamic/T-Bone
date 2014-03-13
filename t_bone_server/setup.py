#!/usr/bin/env python
# coding=utf-8

from setuptools import setup, find_packages

VERSION = "1.0.0-a1"


def params():
    name = "T Bone Server"
    version = VERSION
    description = "3D Printer for T Bone CNC cape"
    long_description = open("../README.md").read()
    classifiers = [
        "Development Status :: 3 - Alpha",
        "Environment :: Web Environment",
        "Framework :: Flask",
        "Intended Audience :: End Users/Desktop",
        "Intended Audience :: Manufacturing",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: GNU Affero General Public License v3",
        "Natural Language :: English",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: JavaScript",
        "Topic :: Internet :: WWW/HTTP",
        "Topic :: Internet :: WWW/HTTP :: Dynamic Content",
        "Topic :: Internet :: WWW/HTTP :: WSGI",
        "Topic :: Printing",
        "Topic :: System :: Networking :: Monitoring"
    ]
    author = "Marcus Nowotny"
    author_email = "dev@t-bone.cc"
    url = "http://tbone.cc"
    license = "AGPLv3"

    packages = find_packages(where="src")
    package_dir = {"t_bone": "src/t_bone"}
    test_suite="tests"

    include_package_data = True
    zip_safe = False

    tests_require = open("test/requirements.txt").read().split("\n")
    install_requires = open("requirements.txt").read().split("\n") + test_requirements

    #entry_points = {
    #"console_scripts": [
    #    "octoprint = octoprint:main"
    #]
    #}

    #scripts = {
    #	"scripts/octoprint.init": "/etc/init.d/octoprint"
    #}

    return locals()


setup(**params())