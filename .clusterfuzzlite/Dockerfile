## Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
## See the file COPYING for copying conditions.

## AI-Assisted

## ClusterFuzzLite C/C++ Dockerfile.
## https://google.github.io/clusterfuzzlite/build-integration/c-cpp-lang/
##
## Image: gcr.io/oss-fuzz-base/base-builder.
##
## Trust root - this image path is the canonical C/C++ base for
## OSS-Fuzz / ClusterFuzzLite, published by Google's OSS-Fuzz
## project. The Dockerfile that produces this image lives in the
## OSS-Fuzz repo; the language-specific guide tells integrators to
## use this exact path:
##   https://github.com/google/oss-fuzz/tree/master/infra/base-images/base-builder
##   https://google.github.io/oss-fuzz/getting-started/new-project-guide/
##
## Digest sha256:6dd7113e... resolved from the 'latest' tag via the
## registry manifest API docker-content-digest header (verification
## step, not trust source):
##   https://gcr.io/v2/oss-fuzz-base/base-builder/manifests/latest
FROM gcr.io/oss-fuzz-base/base-builder@sha256:6dd7113e000cf047f96c612ae9d960629578c319e7c78a9f67833bc95b80f0c3

## Install kloak's build deps. The base image is Ubuntu-based, so
## apt works. wayland-protocols + libwayland-bin are needed for the
## wayland-scanner protocol codegen step the Makefile normally
## performs; see ci/cfuzz-build.sh for the codegen invocation.
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      libinput-dev \
      libevdev-dev \
      libwayland-dev \
      libxkbcommon-dev \
      libwayland-bin \
      wayland-protocols \
      pkg-config && \
    rm -rf /var/lib/apt/lists/*

COPY . $SRC/kloak
COPY .clusterfuzzlite/build.sh $SRC/build.sh
WORKDIR $SRC/kloak
