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
      "boost/1.78.0",
      "nlohmann_json/3.10.5",
      "abseil/20211102.0",
    )

    def layout(self):
      cmake_layout(self)

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

    def deploy(self):
      self.copy( "*" )
