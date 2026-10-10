#!/usr/bin/env bash
set -euo pipefail
ENV="${1:-dev}"
TAG="${2:-latest}"

echo "kustomize edit set image ... ${ENV} ${TAG}"
