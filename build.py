#!/usr/bin/env python

import os
import subprocess
import sys

from scripts import (
    builder as b,
    utils
)


def prepare(args: list):
    print("=============== GENERATING ===============", flush=True)
    builder = b.HostBuilder()
    for arg in args:
        match arg:
            case "--container":
                builder = b.ContainerBuilder()
            case _:
                raise RuntimeError(
                    "Unknown option '{}' for build subcommand".format(arg)
                )

    builder.generate_build_files()


def build(args: list):
    print("================ BUILDING ===============", flush=True)
    builder = b.HostBuilder()
    for arg in args:
        match arg:
            case "--container":
                builder = b.ContainerBuilder()
            case _:
                raise RuntimeError(
                    "Unknown option '{}' for build subcommand".format(arg)
                )

    builder.build()


def run():
    print("================ RUNNING ================", flush=True)
    project_dir = utils.get_project_dir()
    res = subprocess.run([
        os.path.join(
            project_dir,
            "build",
            "src",
            "Debug",
            "Tramogi",
        ),
    ])
    if res.returncode != 0:
        print(res.stderr)
        raise RuntimeError("Application runtime error")


def main():
    args = sys.argv[1:]
    if len(args) < 1:
        raise RuntimeError(
            "One or more parameters are required."
            " `build.py help` to see available commands"
        )

    subcommand = args.pop(0)
    match subcommand:
        case "help":
            utils.print_help()
        case "prepare":
            utils.check_tool_dependencies()
            prepare(args)
        case "build":
            utils.check_tool_dependencies()
            build(args)
        case "run":
            run()
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
