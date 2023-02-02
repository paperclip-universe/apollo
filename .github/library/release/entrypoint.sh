#!/bin/sh -l

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain nightly

cargo install release-plz
git config --global user.email "release-plz@github.com"
git config --global user.name "release-plz"
release-plz release-pr \
    --git-token "$GITHUB_TOKEN" \
    --repo-url "https://github.com/paperclip-universe/apollo"
release-plz release
