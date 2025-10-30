import os
import pathlib
import platform
import shutil
import subprocess
import sys


VENV_NAME = ".venv"
CONAN_PROFILE_NAME = "project-sky-high"
GLFW_VERSION = "3.4"


def check_deps():
    assert shutil.which("g++") is not None, "g++ required"
    assert shutil.which("cmake") is not None, "CMake required"
    assert shutil.which("ninja") is not None, "Ninja required"


def get_container_engine() -> str:
    podman = shutil.which("podman")
    docker = shutil.which("docker")

    assert podman is not None and \
        docker is not None, \
        "Docker or podman required"

    if podman:
        return podman

    return docker


def prepare_builder_container(container_engine: str) -> bool:
    res = subprocess.run([
        container_engine,
        "build",
        ".",
        "-t",
        "skyhigh-builder",
    ])
    if res.returncode != 0:
        return False

    return subprocess.run([
        "podman",
        "run",
        "--rm",
        "-v", ".:${PWD}:Z",
        "-w ${PWD}",
        "skyhigh-builder",
        "bash -c 'conan install . --build=missing && \
            cmake . -G Ninja --preset conan-release'"
    ]).returncode == 0


def prepare_venv(os_name: str, current_folder: str) -> str | None:
    bin_folder = "bin"
    if os_name == "Windows":
        bin_folder = "Scripts"

    venv_bin_path = os.path.join(
        current_folder,
        VENV_NAME,
        bin_folder,
    )

    if os.path.exists(venv_bin_path):
        print("Venv exists. Skipping venv generation")
        return venv_bin_path

    res = subprocess.run([
        "python",
        "-m",
        "venv",
        VENV_NAME,
    ])

    if res.returncode != 0:
        print("Failed to create python venv")
        return None

    return venv_bin_path


def install_conan(venv_bin_path: str) -> bool:
    pip_exec = os.path.join(
        venv_bin_path,
        "pip",
    )
    return subprocess.run([
        pip_exec,
        "install",
        "conan",
    ]).returncode == 0


def prepare_conan(venv_bin_path) -> bool:
    conan_exec = os.path.join(venv_bin_path, "conan")

    res = subprocess.run([
        conan_exec,
        "profile",
        "path",
        CONAN_PROFILE_NAME
    ])
    if res.returncode == 0:
        return True

    return subprocess.run([
        conan_exec,
        "profile",
        "detect",
        "--name=" + CONAN_PROFILE_NAME,
    ]).returncode == 0


def build_glfw(venv_bin_path) -> bool:
    conan_exec = os.path.join(venv_bin_path, "conan")
    build_args = []

    res = subprocess.run([
        conan_exec,
        "list",
        "glfw/" + GLFW_VERSION,
    ])
    if res.returncode != 0:
        build_args.append("--build=glfw/" + GLFW_VERSION)

    return subprocess.run([
        conan_exec,
        "install",
        ".",
        "--profile:all=" + CONAN_PROFILE_NAME,
    ] + build_args).returncode == 0


def generate_cmake() -> bool:
    return subprocess.run([
        "cmake",
        ".",
        "-G",
        "Ninja",
        "--preset",
        "conan-release",
    ]).returncode == 0


def main():
    os_name = platform.system()
    build_on_host = False
    use_venv = True

    for arg in sys.argv:
        match arg:
            case "--host":
                build_on_host = True
            case "--no-venv":
                use_venv = False

    if build_on_host:
        check_deps()

        current_folder = pathlib.Path(__file__).parent.resolve()
        venv_bin_path = ""
        if use_venv:
            venv_bin_path = prepare_venv(os_name, current_folder)
            if venv_bin_path is None:
                return

        if not install_conan(venv_bin_path):
            return
        if not prepare_conan(venv_bin_path):
            return
        if not build_glfw(venv_bin_path):
            return
        if not generate_cmake():
            return
    else:
        container_engine = get_container_engine()
        prepare_builder_container(container_engine)


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print("Assertion:", e)
    except Exception as e:
        print(e)
