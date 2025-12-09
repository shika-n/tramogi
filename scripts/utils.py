import os
import pathlib
import shutil
import subprocess
import textwrap


def check_tool_dependencies():
    if shutil.which("g++") is None and shutil.which("clang++") is None:
        raise EnvironmentError("g++ or clang++ required")
    if shutil.which("cmake") is None:
        raise EnvironmentError("CMake required")
    if shutil.which("ninja") is None:
        raise EnvironmentError("Ninja required")
    get_slangc()  # Will throw if not found


def get_container_engine() -> str:
    podman = shutil.which("podman")
    docker = shutil.which("docker")

    if podman is None and docker is None:
        raise EnvironmentError("Docker or podman required")

    if podman:
        return podman

    return docker


def get_project_dir() -> str:
    for dir in pathlib.Path(__file__).parents:
        if os.path.exists(os.path.join(dir, ".git")):
            return dir.resolve()

    raise EnvironmentError("Can't detect project directory (.git missing)")


def get_slangc():
    slangc_exec = shutil.which("slangc")
    if slangc_exec is None:
        print("No slangc in PATH. Checking Neovim's Mason", flush=True)
        neovim_exec = shutil.which("nvim")
        if neovim_exec is None:
            print(
                "No nvim in PATH to check for slangc installed via Mason",
                flush=True,
            )
            raise EnvironmentError(
                "slangc required to compile shaders"
            )

        current_file_dir = os.path.dirname(__file__)

        result = subprocess.run([
            neovim_exec,
            "-l", os.path.join(current_file_dir, "datapath.lua"),
        ], capture_output=True)
        if result.returncode != 0:
            raise RuntimeError("Failed to run datapath.lua")

        mason_bin_dir = result.stderr.decode("utf-8").strip()
        slangc_exec = os.path.join(mason_bin_dir, "mason", "bin", "slangc")

        if os.name == "nt":
            slangc_exec += ".cmd"

        if not os.path.isfile(slangc_exec):
            raise RuntimeError(
                "No slangc found in PATH" +
                " nor does it seem to be installed via Mason"
            )

        print("slangc found: {}".format(slangc_exec), flush=True)
    else:
        print("slangc in PATH: {}".format(slangc_exec), flush=True)

    return slangc_exec


def print_help():
    print(textwrap.dedent("""
        python build.py <subcommand> <options>
        subcommands:
            help
            prepare
                --container
            build
                --container
            run
    """))
