# Cross-compilation matrix image: ubuntu base + cross toolchains + qemu-user.
# Build with `podman build -t rvemu-matrix:local -f test/matrix/Dockerfile .`
# and run `python3 test/matrix/matrix.py` inside the container with the repo
# bind-mounted at `/workspace`.
#
# The package list lives in test/matrix/packages.txt (single source of truth,
# shared with .github/workflows/makefile.yml); comments/blank lines are
# filtered out here. Availability is honored the same way: packages the base
# distro has no candidate for are skipped with a note (matrix.py then reports
# that arch as N/A) — on the noble base today the full list installs.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

COPY test/matrix/packages.txt /tmp/packages.txt

RUN apt-get update \
    && PKGS="" \
    && for p in $(grep -vE '^[[:space:]]*(#|$)' /tmp/packages.txt); do \
          if apt-cache policy "$p" 2>/dev/null | awk '/Candidate:/{print $2; exit}' | grep -qE '[0-9]'; then \
            PKGS="$PKGS $p"; \
          else \
            echo "note: $p has no candidate on this base, skipping"; \
          fi; \
        done \
    && [ -n "$PKGS" ] \
    && /usr/bin/apt-get install -y --no-install-recommends $PKGS \
    && rm -rf /var/lib/apt/lists/* /tmp/packages.txt

LABEL purpose="cross-building + qemu-user execution of the rvemu host binary"
