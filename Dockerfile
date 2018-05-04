FROM ubuntu:17.04
MAINTAINER Christian Godenschwager <christian.godenschwager@fau.de>

# OpenSSH is required to run MPI applications
RUN apt-get update &&  apt-get install  -y \
    automake \
    build-essential \
    curl \
    doxygen \
    git \
    libgtk2.0-dev \
    pkg-config \
    build-essential \
    git \
    cmake \
    libssh-dev \
    libwxbase3.0-dev \
    libsqlite3-dev \
    libwxsqlite3-3.0-dev \
    libwxgtk3.0-dev \
    wx3.0-headers \
    wx-common \
    wx3.0-i18n \
    wx3.0-examples \
    libclang-dev \
    libhunspell-dev \
    lldb-3.5-dev
    
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* 

#ADD install*.sh /root/ 

