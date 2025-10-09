#!/usr/bin/env bash
podman build . -t skyhigh-builder
podman run --rm \
	-v .:${PWD}:Z \
	-v ${HOME}/.conan2:${HOME}/.conan2:Z \
	-e CONAN_HOME=${HOME}/.conan2 \
	-w ${PWD} \
	skyhigh-builder \
	bash -c "conan install . --build=missing && \
		cmake . -G Ninja --preset conan-release"
