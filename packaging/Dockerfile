# Ditonira AppImage factory — Ubuntu 20.04 (glibc 2.31).
# Implementer/CI only. End users never run this; they only run the AppImage.
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    APPIMAGE_EXTRACT_AND_RUN=1

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      cmake \
      curl \
      file \
      binutils \
      pkg-config \
      qtbase5-dev \
      qtbase5-dev-tools \
      libqt5serialport5-dev \
      libasound2-dev \
      libfuse2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
