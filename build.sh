#!/usr/bin/env bash
echo "================ BUILDING ==============="

podman run --rm \
	-v .:${PWD}:Z \
	-v ${HOME}/.conan2:${HOME}/.conan2:Z \
	-e CONAN_HOME=${HOME}/.conan2 \
	-w ${PWD} \
	skyhigh-builder \
	bash -c "cd build/Release && ninja"

if [[ $? -ne 0 ]] then
	exit
fi

if [[ $1 == "run" ]] then
	echo "================ RUNNING ================"
	./build/Release/SkyHigh
fi

