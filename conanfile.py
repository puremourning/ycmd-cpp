from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout
from conans import tools


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
    "boost/1.80.0", # async_read_some broken in 1.81.0
    "nlohmann_json/3.11.2",
    "abseil/20220623.0",
    "gtest/1.13.0",
    "pybind11/2.10.1",
    "function2/4.2.2"
  )

  def validate(self):
    tools.check_min_cppstd(self, "20")

  def layout(self):
    cmake_layout(self)

  def configure(self):
    # A shame, but we need its stupid regex stuff to actually work.. ?
    self.options['boost'].i18n_backend = "icu"
    self.options['icu'].data_packaging = 'static'

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
