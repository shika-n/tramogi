import os
import subprocess

from . import config as conf


def prepare_venv(os_name: str, project_dir: str) -> str:
    bin_folder = "bin"
    if os_name == "Windows":
        bin_folder = "Scripts"

    venv_bin_path = os.path.join(
        project_dir,
        conf.VENV_NAME,
        bin_folder,
    )

    if os.path.exists(venv_bin_path):
        print("Venv exists. Skipping venv generation", flush=True)
        return venv_bin_path

    res = subprocess.run([
        "python",
        "-m",
        "venv",
        conf.VENV_NAME,
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to create python venv")

    return venv_bin_path


def install_conan(venv_bin_path: str):
    pip_exec = os.path.join(
        venv_bin_path,
        "pip",
    )
    res = subprocess.run([
        pip_exec,
        "install",
        "conan",
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to install conan")


def prepare_conan(venv_bin_path):
    conan_exec = os.path.join(venv_bin_path, "conan")

    res = subprocess.run([
        conan_exec,
        "profile",
        "path",
        conf.CONAN_PROFILE_NAME
    ])
    if res.returncode == 0:
        print("Conan profile exists", flush=True)
        return

    res = subprocess.run([
        conan_exec,
        "profile",
        "detect",
        "--name=" + conf.CONAN_PROFILE_NAME,
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to create conan profile")


# TODO: Add clean/rebuild subcommand


def install_project_deps(venv_bin_path):
    conan_exec = os.path.join(venv_bin_path, "conan")
    build_args = []

    build_args.append("--build=missing")

    res = subprocess.run([
        conan_exec,
        "install",
        ".",
        "--profile:all={}".format(conf.CONAN_PROFILE_NAME),
        "-s", "build_type=Debug",
    ] + build_args)

    if res.returncode != 0:
        raise RuntimeError("Failed to install project dependencies")
