import os
import pathlib
import subprocess
import sys

import scripts.utils as utils
import scripts.vars as vars


def build_in_container(container_engine: str, pass_args: list):
    res = subprocess.run([
        container_engine,
        "run",
        "--rm",
        "-v", ".:/app:Z",
        "-v", "./.conan2:/root/.conan2:Z",
        vars.IMAGE_NAME,
        "build.py", "--host",
    ] + pass_args)

    if res.returncode != 0:
        raise RuntimeError("Failed when building in a container")


def generate_cmake():
    res = subprocess.run([
        "cmake",
        ".",
        "-G",
        "Ninja",
        "--preset",
        "conan-release",
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to generate cmake")


def build() -> bool:
    print("================ BUILDING ===============")
    res = subprocess.run([
        "ninja",
        "-C",
        os.path.join(
            "build",
            "Release",
        ),
    ])

    if res.returncode != 0:
        raise RuntimeError("Build failed")


def run(current_folder: str):
    print("================ RUNNING ================")
    res = subprocess.run([
        os.path.join(
            current_folder,
            "build",
            "Release",
            "SkyHigh",
        ),
    ])

    if res.returncode != 0:
        raise RuntimeError()


def main():
    current_folder = pathlib.Path(__file__).parent.resolve()

    args = sys.argv[1:]

    build_on_host = False
    should_generate_cmake = False
    should_run_after_build = False
    for arg in args:
        match arg:
            case "run":
                should_run_after_build = True
            case "--generate":
                should_generate_cmake = True
            case "--host":
                build_on_host = True

    if build_on_host:
        if should_generate_cmake:
            generate_cmake()

        build()
    else:
        container_args = list(filter(lambda val: val != "run", args))
        container_engine = utils.get_container_engine()
        build_in_container(container_engine, container_args)

    if should_run_after_build:
        run(current_folder)


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print("Assertion:", e)
    except Exception as e:
        print(e)
