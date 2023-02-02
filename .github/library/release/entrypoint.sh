#!/bin/sh -l

cargo install release-plz
git config --global user.email "release-plz@github.com"
git config --global user.name "release-plz"
release-plz release-pr \
    --git-token "$GITHUB_TOKEN" \
    --repo-url "https://github.com/paperclip-universe/apollo"
release-plz release