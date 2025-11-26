FROM fedora:43

ARG slang_base_url=https://github.com/shader-slang/slang/releases/download/
ARG slang_url_path=v2025.22.1/slang-2025.22.1-linux-x86_64-glibc-2.27.tar.gz

RUN sudo dnf install -y \
	cmake \
	g++ \
	ninja \
	python3 \
	python3-pip \
	wget \
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
	xcb-util-wm-devel && \
	sudo dnf clean all && \
	rm -rf /var/cache/yum && \
	wget -O slang.tar.gz "${slang_base_url}${slang_url_path}" && \
	tar -xzf slang.tar.gz -C /usr/local && \
	rm slang.tar.gz
ENTRYPOINT [ "python" ]
