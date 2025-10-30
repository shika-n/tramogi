import os
import pathlib
import subprocess
import sys


def main():
    current_folder = pathlib.Path(__file__).parent.resolve()

    should_run_after_build = False
    for arg in sys.argv:
        match arg:
            case "run":
                should_run_after_build = True

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
        return

    if should_run_after_build:
        print("================ RUNNING ================")
        subprocess.run([
            os.path.join(
                current_folder,
                "build",
                "Release",
                "SkyHigh",
            ),
        ])


if __name__ == "__main__":
    main()
