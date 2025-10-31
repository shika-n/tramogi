import os
import subprocess

from . import config as conf


def prepare_builder(container_engine: str, project_dir: str):
    res = subprocess.run([
        container_engine,
        "build",
        ".",
        "-t",
        conf.IMAGE_NAME,
    ])
    if res.returncode != 0:
        raise RuntimeError("Failed to create base builder image")

    conan_dir = os.path.join(project_dir, ".conan2")
    res = subprocess.run([
        container_engine,
        "run",
        "--rm",
        "-e", "CONAN_HOME={}".format(conan_dir),
        "-v", "{}:{}:Z".format(project_dir, project_dir),
        "-w", project_dir,
        conf.IMAGE_NAME,
        "prepare.py", "--host", "--no-venv",
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to prepare builder")


def build(
    container_engine: str,
    project_dir: str,
    pass_args: list,
):
    conan_dir = os.path.join(project_dir, ".conan2")
    res = subprocess.run([
        container_engine,
        "run",
        "--rm",
        "-e", "CONAN_HOME={}".format(conan_dir),
        "-v", "{}:{}:Z".format(project_dir, project_dir),
        "-w", project_dir,
        conf.IMAGE_NAME,
        "build.py", "--host",
    ] + pass_args)

    if res.returncode != 0:
        raise RuntimeError("Failed when building in a container")
