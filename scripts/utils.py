import shutil


def check_deps():
    if shutil.which("g++") is None:
        raise EnvironmentError("g++ required")
    if shutil.which("cmake") is None:
        raise EnvironmentError("CMake required")
    if shutil.which("ninja") is None:
        raise EnvironmentError("Ninja required")


def get_container_engine() -> str:
    podman = shutil.which("podman")
    docker = shutil.which("docker")

    if podman is None and docker is None:
        raise EnvironmentError("Docker or podman required")

    if podman:
        return podman

    return docker
