# coding: utf-8

from __future__ import unicode_literals

from setuptools import setup


setup(name='file-magic',
      version='0.3.0',
      author='Reuben Thomas, Álvaro Justen',
      author_email='rrt@sc3d.org, alvarojusten@gmail.com',
      url='https://github.com/file/file',
      license='BSD',
      description='(official) libmagic Python bindings',
      py_modules=['magic'],
      test_suite='tests',
      classifiers = [
          'Intended Audience :: Developers',
          'License :: OSI Approved :: BSD License',
          'Natural Language :: English',
          'Topic :: Software Development :: Libraries :: Python Modules',
      ])
