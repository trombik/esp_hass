# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys
# sys.path.insert(0, os.path.abspath('..'))

# on FreeBSD, the name is not `libclang-13.so`, which is used by `clang` python module
import clang.cindex
if os.uname().sysname == 'FreeBSD':
  clang.cindex.Config.set_library_file('/usr/local/llvm13/lib/libclang.so.13')

# -- Project information -----------------------------------------------------

project = 'esp_hass'
copyright = '2022, Tomoyuki Sakurai'
author = 'Tomoyuki Sakurai'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
  "myst_parser",
  "sphinx_c_autodoc",
  "sphinx.ext.autodoc"
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', 'examples', '.tox', '.github']


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# document public herder file only
c_autodoc_roots = ['../include']

# these are necessary to resolve types defined by external headers.
idf_path = os.path.relpath(os.environ['IDF_PATH'])
esp_common = os.path.join(idf_path, 'components', 'esp_common', 'include')
cjson = os.path.join(idf_path, 'components', 'json', 'cJSON')
c_autodoc_compilation_args = [
  f'-I{esp_common}',
  f'-I{cjson}',
]
