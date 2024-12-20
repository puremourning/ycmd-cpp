from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout


class YcmdConan(ConanFile):
  name = "ycmd"
  version = "2.0.0"

  # Optional metadata
  license = "Apache 2.0"
  author = "Ben Jackson puremourning+ycmd@gmail.com"
  url = "https://github.com/ycm-core/ycmd"
  description = "A code comprehension server"

  # Binary configuration
  settings = "os", "compiler", "build_type", "arch"

  # Sources are located in the same place as this recipe, copy them to the
  # recipe
  exports_sources = "CMakeLists.txt", "src/*"

  # Requirements
  requires = (
    "boost/1.83.0",
    "nlohmann_json/3.11.2",
    "abseil/20220623.0",
    "gtest/1.15.0",
    "pybind11/2.13.6",
    "function2/4.2.2",
    "xxhash/0.8.2",
    "icu/74.2",
  )

  def layout(self):
    cmake_layout(self)

  def config_options(self):
    # A shame, but we need its stupid regex stuff to actually work.. ?
    self.options['boost/*'].i18n_backend_iconv = "off"
    self.options['boost/*'].i18n_backend_icu = True
    self.options['icu/*'].data_packaging = 'static'

  def generate(self):
    CMakeToolchain(self).generate()
    CMakeDeps(self).generate()

  def build(self):
    cmake = CMake(self)
    cmake.configure()
    cmake.build()

  def package(self):
    cmake = CMake(self)
    cmake.install()
