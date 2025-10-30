import os
import pathlib
import platform
import subprocess
import sys

import scripts.utils as utils
import scripts.vars as vars


def prepare_builder_container(container_engine: str):
    res = subprocess.run([
        container_engine,
        "build",
        ".",
        "-t",
        vars.IMAGE_NAME,
    ])
    if res.returncode != 0:
        raise RuntimeError("Failed to create base builder image")

    os.mkdir(".conan2")

    res = subprocess.run([
        container_engine,
        "run",
        "--rm",
        "-v", ".:/app:Z",
        "-v", "./.conan2:/root/.conan2:Z",
        vars.IMAGE_NAME,
        "prepare.py", "--host", "--no-venv",
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to prepare builder")


def prepare_venv(os_name: str, current_folder: str) -> str:
    bin_folder = "bin"
    if os_name == "Windows":
        bin_folder = "Scripts"

    venv_bin_path = os.path.join(
        current_folder,
        vars.VENV_NAME,
        bin_folder,
    )

    if os.path.exists(venv_bin_path):
        print("Venv exists. Skipping venv generation")
        return venv_bin_path

    res = subprocess.run([
        "python",
        "-m",
        "venv",
        vars.VENV_NAME,
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
        vars.CONAN_PROFILE_NAME
    ])
    if res.returncode == 0:
        print("Conan profile exists")
        return

    res = subprocess.run([
        conan_exec,
        "profile",
        "detect",
        "--name=" + vars.CONAN_PROFILE_NAME,
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to create conan profile")


def install_project_deps(venv_bin_path):
    conan_exec = os.path.join(venv_bin_path, "conan")
    build_args = []

    res = subprocess.run([
        conan_exec,
        "list",
        "glfw/" + vars.GLFW_VERSION,
    ], capture_output=True)
    if res.stdout.endswith(b"not found\n"):
        build_args.append("--build=glfw/" + vars.GLFW_VERSION)

    res = subprocess.run([
        conan_exec,
        "install",
        ".",
        "--profile:all=" + vars.CONAN_PROFILE_NAME,
    ] + build_args)

    if res.returncode != 0:
        raise RuntimeError("Failed to install project dependencies")


def main():
    args = sys.argv[1:]

    os_name = platform.system()
    build_on_host = False
    use_venv = True

    for arg in args:
        match arg:
            case "--host":
                build_on_host = True
            case "--no-venv":
                use_venv = False

    if build_on_host:
        utils.check_deps()

        current_folder = pathlib.Path(__file__).parent.resolve()
        venv_bin_path = ""
        if use_venv:
            venv_bin_path = prepare_venv(os_name, current_folder)

        install_conan(venv_bin_path)
        prepare_conan(venv_bin_path)
        install_project_deps(venv_bin_path)
    else:
        container_engine = utils.get_container_engine()
        prepare_builder_container(container_engine)


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print("Assertion:", e)
    except Exception as e:
        print(e)
