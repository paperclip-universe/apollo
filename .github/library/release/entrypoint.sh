#!/bin/sh -l

apt-get -y update
apt-get -y install \
    curl git \
    nasm make gcc libssl-dev pkg-config

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain nightly -y
. "$HOME/.cargo/env"

cargo install release-plz
git config --global user.email "release-plz@github.com"
git config --global user.name "release-plz"
release-plz release-pr \
    --git-token "$GITHUB_TOKEN" \
    --repo-url "https://github.com/paperclip-universe/apollo"
release-plz release \
    --git-token "$GITHUB_TOKEN" \
    --git-release
