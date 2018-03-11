#!/usr/bin/env python
from distutils.core import setup, Extension

setup (
    name = 'xntp',
    version = '0.1',
    description = 'Python XNTP SHM interface',
    author = "Jonathan Brady",
    author_email = "<jtjbrady@sourceforge.net>",
    url = "http://pyxntp.sourceforge.net/",
    license = "GPL",
    ext_modules = [
    Extension(
        'xntp',
        sources = ['xntp.c'],
        )],
    )
