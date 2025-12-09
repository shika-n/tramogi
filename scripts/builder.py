import subprocess
from . import utils


class HostBuilder:
    def generate_build_files(self):
        res = subprocess.run([
            "cmake",
            "--preset",
            "default",
            "-D",
            "SLANGC_EXECUTABLE=" + utils.get_slangc()
        ])

        if res.returncode != 0:
            raise RuntimeError("Error generating build files")

    def build(self):
        res = subprocess.run([
            "cmake",
            "--build",
            "--preset",
            "debug"
        ])

        if res.returncode != 0:
            raise RuntimeError("Build failed")


class ContainerBuilder:
    CONTAINER_IMAGE_NAME = "tramogi-builder"

    def __init__(self):
        self.container_engine = utils.get_container_engine()
        self.project_dir = utils.get_project_dir()

    def generate_build_files(self):
        res = subprocess.run([
            self.container_engine,
            "build",
            ".",
            "-t",
            self.CONTAINER_IMAGE_NAME,
        ])

        if res.returncode != 0:
            raise RuntimeError("Failed to create base builder image")

        res = subprocess.run([
            self.container_engine,
            "run",
            "--rm",
            "-v", "{}:{}:Z".format(self.project_dir, self.project_dir),
            "-w", self.project_dir,
            self.CONTAINER_IMAGE_NAME,
            "build.py", "prepare"
        ])

        if res.returncode != 0:
            raise RuntimeError("Failed to generate cmake files via container")

    def build(self):
        res = subprocess.run([
            self.container_engine,
            "run",
            "--rm",
            "-v", "{}:{}:Z".format(self.project_dir, self.project_dir),
            "-w", self.project_dir,
            self.CONTAINER_IMAGE_NAME,
            "build.py", "build"
        ])

        if res.returncode != 0:
            raise RuntimeError("Failed to build via container")
