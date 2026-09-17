from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.files import load
from conan.tools.system.package_manager import Apt

class MothSprite(ConanFile):
    name = "moth_sprite"

    license = "MIT"
    description = "A sprite sheet and animation clip editor for moth"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "CMakeLists.txt", "version.txt", "src/*", "external/nativefiledialog/*", "external/stb/*"

    def set_version(self):
        if not self.version:
            self.version = load(self, "version.txt").strip()

    def validate(self):
        # C++17 is the floor for every moth project.
        check_min_cppstd(self, 17)

    def requirements(self):
        # moth_bridge brings in moth_core, moth_graphics and moth_ui from the toolkit.
        self.requires("moth_bridge/0.1.0")

    def system_requirements(self):
        if self.settings.os == "Linux":
            apt = Apt(self)
            apt.install(["libgtk-3-dev"])

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.27.0]")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
