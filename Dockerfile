FROM fedora:42
WORKDIR /app
RUN sudo dnf install -y \
	cmake \
	g++ \
	ninja \
	python3 \
	python3-pip \
	mesa-libGL-devel \
	libfontenc-devel \
	libglvnd-devel \
	libstdc++-static \
	libuuid-devel \
	libXaw-devel \
	libxcb-devel \
	libXcomposite-devel \
	libXcursor-devel \
	libXdamage-devel \
	libXdmcp-devel \
	libXinerama-devel \
	libxkbfile-devel \
	libXrandr-devel \
	libXres-devel \
	libXScrnSaver-devel \
	libXtst-devel \
	libXv-devel \
	libXxf86vm-devel \
	xcb-util-cursor-devel \
	xcb-util-devel \
	xcb-util-image-devel \
	xcb-util-keysyms-devel \
	xcb-util-renderutil-devel \
	xcb-util-wm-devel
ENTRYPOINT [ "python" ]
