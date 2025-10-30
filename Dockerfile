FROM fedora:42
RUN sudo dnf install -y \
	python3 \
	python3-pip \
	cmake \
	g++ \
	ninja \
	libglvnd-devel \
	mesa-libGL-devel \
	libxcb-devel \
	libfontenc-devel \
	libXaw-devel \
	libXcomposite-devel \
	libXcursor-devel \
	libXdmcp-devel \
	libXtst-devel \
	libXinerama-devel \
	libxkbfile-devel \
	libXrandr-devel \
	libXres-devel \
	libXScrnSaver-devel \
	xcb-util-wm-devel \
	xcb-util-image-devel \
	xcb-util-keysyms-devel \
	xcb-util-renderutil-devel \
	libXdamage-devel \
	libXxf86vm-devel \
	libXv-devel \
	xcb-util-devel \
	libuuid-devel \
	xcb-util-cursor-devel && \
	python prepare.py --host --no-venv
