import os
import subprocess


def generate_cmake():
    res = subprocess.run([
        "cmake",
        ".",
        "-G",
        "Ninja",
        "--preset",
        "conan-debug",
    ])

    if res.returncode != 0:
        raise RuntimeError("Failed to generate cmake")


def build() -> bool:
    print("================ BUILDING ===============", flush=True)
    res = subprocess.run([
        "ninja",
        "-C",
        os.path.join(
            "build",
            "Debug",
        ),
    ])

    if res.returncode != 0:
        raise RuntimeError("Build failed")
