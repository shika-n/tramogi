import os
import platform
import subprocess
import sys

from scripts import (
    build as bld,
    container_flow as cf,
    prepare as prep,
    utils,
)


def prepare(args: list, project_dir: str):
    os_name = platform.system()
    build_on_host = False
    use_venv = True

    for arg in args:
        match arg:
            case "--host":
                build_on_host = True
            case "--no-venv":
                use_venv = False
            case _:
                raise RuntimeError(
                    "Unknown option '{}' for prepare subcommand".format(arg)
                )

    if build_on_host:
        utils.check_deps()

        venv_bin_path = ""
        if use_venv:
            venv_bin_path = prep.prepare_venv(os_name, project_dir)

        prep.install_conan(venv_bin_path)
        prep.prepare_conan(venv_bin_path)
        prep.install_project_deps(venv_bin_path)
    else:
        container_engine = utils.get_container_engine()
        cf.prepare_builder(container_engine, project_dir)


def sub_build(args: list, project_dir: str):
    build_on_host = False
    should_generate_cmake = False
    for arg in args:
        match arg:
            case "--generate":
                should_generate_cmake = True
            case "--host":
                build_on_host = True
            case _:
                raise RuntimeError(
                    "Unknown option '{}' for build subcommand".format(arg)
                )

    if build_on_host:
        if should_generate_cmake:
            bld.generate_cmake()

        bld.build()
    else:
        container_engine = utils.get_container_engine()
        cf.build(container_engine, project_dir, args)


def run(project_dir: str):
    print("================ RUNNING ================", flush=True)
    res = subprocess.run([
        os.path.join(
            project_dir,
            "build",
            "Debug",
            "src",
            "SkyHigh",
        ),
    ])

    if res.returncode != 0:
        raise RuntimeError("Application runtime error")


def main():
    args = sys.argv[1:]
    if len(args) < 1:
        raise RuntimeError(
            "One or more parameters are required."
            " `build.py help` to see available commands"
        )

    project_dir = utils.get_project_dir()

    subcommand = args.pop(0)
    match subcommand:
        case "help":
            utils.print_help()
        case "prepare":
            prepare(args, project_dir)
        case "build":
            sub_build(args, project_dir)
        case "run":
            run(project_dir)
        case _:
            raise RuntimeError("Unknown subcommand '{}'".format(subcommand))


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print("Assertion:", e)
        sys.exit(1)
    except Exception as e:
        print(e)
        sys.exit(1)
