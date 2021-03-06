FROM ubuntu:18.04

# Install compilers, dependencies, and tools for CircleCI primary container
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    clang \
    cmake \
    libgtest-dev \
    git \
    ssh \
    tar \
    gzip \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

# Compile and install Google Test
ARG gtestBuildDir=/tmp/gtest/
RUN mkdir -p ${gtestBuildDir} && \
  cd ${gtestBuildDir} && \
  cmake -DCMAKE_CXX_FLAGS="-std=c++17" /usr/src/gtest/ && \
  make -C ${gtestBuildDir} && \
  cp ${gtestBuildDir}lib*.a /usr/lib && \
  rm -rf ${gtestBuildDir}

RUN useradd -m -s /bin/bash user
USER user

VOLUME /code
WORKDIR /code

CMD ["make", "--keep-going"]
